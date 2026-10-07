#!/usr/bin/env python3
"""Exercise request identity and the actual privileged full-result publisher."""

import argparse
import copy
import json
import re
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import ci

ROOT = Path(__file__).resolve().parents[1]
HEAD = "a" * 40
BASE = "b" * 40
PULL = {
    "state": "open",
    "head": {"sha": HEAD},
    "base": {"sha": BASE, "ref": "rust-migration"},
    "labels": [],
}


def script_for(filename, job):
    text = (ROOT / ".github/workflows" / filename).read_text()
    section = text.split(f"  {job}:\n", 1)[1]
    section = re.split(r"\n  [a-z_]+:\n", section, maxsplit=1)[0]
    match = re.search(r"        script: \|\n((?:          .*\n|\n)+)", section)
    if match is None:
        raise AssertionError(f"No publisher script in {filename}:{job}")
    return "\n".join(line[10:] for line in match[1].splitlines())


def good_needs():
    needs = {
        name: {"result": "success", "outputs": {}}
        for name in ("plan", "quick", "native", "platform", "annotations")
    }
    needs["plan"]["outputs"] = {"head": HEAD, "pr": "170", "profile": "full"}
    for name in ("native", "platform"):
        needs[name]["outputs"]["validated"] = "true"
    return needs


def execute_publisher(needs, pull, superseded=False):
    # Execute the YAML's publisher, not a Python imitation of its decisions.
    script = script_for("ci-request.yml", "publish")
    harness = """
const statuses = [];
const failures = [];
const context = {repo: {owner: 'example', repo: 'fork'}, serverUrl: 'https://github.com', runId: 123};
const core = {setFailed: message => failures.push(message)};
const github = {rest: {
  pulls: {get: async () => ({data: PULL})},
  repos: {
    getCombinedStatusForRef: async () => ({data: {statuses: [{context: 'Full validation', target_url: RUN_URL}]}}),
    createCommitStatus: async value => statuses.push(value)
  }
}};
process.env.RESULTS = JSON.stringify(NEEDS);
(async () => {SCRIPT;})().then(() => console.log(JSON.stringify({statuses, failures}))).catch(error => {console.error(error); process.exit(1);});
"""
    harness = (
        harness.replace("PULL", json.dumps(pull))
        .replace("NEEDS", json.dumps(needs))
        .replace("SCRIPT", script)
        .replace(
            "RUN_URL",
            json.dumps(
                "https://github.com/example/fork/actions/runs/"
                + ("999" if superseded else "123")
            ),
        )
    )
    return json.loads(
        subprocess.run(
            ["node", "-e", harness], check=True, capture_output=True, text=True
        ).stdout
    )


class GateTests(unittest.TestCase):
    def test_full_success(self):
        result = execute_publisher(good_needs(), PULL)
        self.assertEqual(result["statuses"][0]["state"], "success")
        self.assertEqual(result["statuses"][0]["sha"], HEAD)
        self.assertEqual(result["statuses"][0]["context"], ci.CONTEXT)
        self.assertFalse(result["failures"])

    def test_superseded_request_cannot_overwrite_newer_status(self):
        result = execute_publisher(good_needs(), PULL, superseded=True)
        self.assertEqual(result["statuses"], [])
        self.assertTrue(result["failures"])

    def test_each_non_success_result_is_rejected(self):
        for job in good_needs():
            for outcome in ("failure", "skipped", "cancelled", "timed_out", ""):
                with self.subTest(job=job, outcome=outcome):
                    needs = good_needs()
                    needs[job]["result"] = outcome
                    self.assertEqual(
                        execute_publisher(needs, PULL)["statuses"][0]["state"],
                        "failure",
                    )

    def test_missing_native_or_platform_proof_is_rejected(self):
        for job in ("native", "platform"):
            needs = good_needs()
            needs[job]["outputs"] = {}
            self.assertEqual(
                execute_publisher(needs, PULL)["statuses"][0]["state"], "failure"
            )

    def test_missing_jobs_are_rejected(self):
        for job in ("quick", "native", "platform", "annotations"):
            needs = good_needs()
            del needs[job]
            self.assertEqual(
                execute_publisher(needs, PULL)["statuses"][0]["state"], "failure"
            )

    def test_stale_head_and_closed_pr_are_rejected(self):
        for field in ("head", "state", "base"):
            pull = copy.deepcopy(PULL)
            if field == "head":
                pull["head"]["sha"] = "c" * 40
            elif field == "state":
                pull["state"] = "closed"
            else:
                pull["base"]["ref"] = "master"
            self.assertEqual(
                execute_publisher(good_needs(), pull)["statuses"][0]["state"], "failure"
            )

    def test_partial_never_publishes_full_status(self):
        workflow = (ROOT / ".github/workflows/ci-request.yml").read_text()
        publish = workflow.split("  publish:\n", 1)[1]
        self.assertIn("needs.plan.outputs.profile == 'full'", publish)
        plan = script_for("ci-request.yml", "plan")
        self.assertIn("if (profile === 'full')", plan)

    def test_no_candidate_execution_in_privileged_jobs(self):
        workflow = (ROOT / ".github/workflows/ci-request.yml").read_text()
        for job in ("plan", "publish"):
            section = workflow.split(f"  {job}:\n", 1)[1]
            section = re.split(r"\n  [a-z_]+:\n", section, maxsplit=1)[0]
            self.assertIn("statuses: write", section)
            self.assertNotIn("actions/checkout", section)
            self.assertNotIn("run:", section)
        self.assertIn("github.ref == 'refs/heads/rust-migration'", workflow)
        self.assertIn("vars.CI_ON_DEMAND != 'true'", workflow)

    def test_push_invalidation_has_no_candidate_checkout(self):
        workflow = (ROOT / ".github/workflows/ci-invalidate.yml").read_text()
        self.assertIn("pull_request_target:", workflow)
        self.assertNotIn("actions/checkout", workflow)
        self.assertIn("state: 'pending'", workflow)
        self.assertNotIn("state: 'success'", workflow)


class PolicyTests(unittest.TestCase):
    def test_heavy_predicates_bypass_skipped_policy_but_respect_cancellation(self):
        for filename, jobs in (
            ("ci-build.yml", ("emscripten", "linux", "macos", "windows")),
            ("rust-migration.yml", ("compare",)),
        ):
            workflow = (ROOT / ".github/workflows" / filename).read_text()
            for job in jobs:
                section = workflow.split(f"  {job}:\n", 1)[1]
                predicate = re.search(r"^    if: (.+)$", section, re.MULTILINE)[1]
                self.assertNotIn("always()", predicate)
                self.assertIn("!cancelled()", predicate)
                expression = predicate.removeprefix("${{ ").removesuffix(" }}")
                for cancelled, checkout, heavy, profile, expected in (
                    (False, "head", "", "full", True),
                    (False, "", "true", "full", True),
                    (False, "", "false", "full", False),
                    (True, "head", "true", "full", False),
                    (True, "", "true", "full", False),
                ):
                    inputs = {"checkout-ref": checkout, "profile": profile}
                    needs = {"policy": {"outputs": {"heavy": heavy}}}
                    harness = f"""
                    const cancelled = () => {json.dumps(cancelled)};
                    const inputs = {json.dumps(inputs)};
                    const needs = {json.dumps(needs)};
                    console.log(JSON.stringify({expression.replace("inputs.checkout-ref", 'inputs["checkout-ref"]')}));
                    """
                    with self.subTest(filename=filename, job=job, cancelled=cancelled):
                        result = json.loads(
                            subprocess.run(
                                ["node", "-e", harness],
                                check=True,
                                capture_output=True,
                                text=True,
                            ).stdout
                        )
                        self.assertEqual(result, expected)

    def test_bootstrap_on_demand_and_docs_classification(self):
        script = script_for("ci-policy.yml", "policy")
        cases = [
            ("", "pull_request", [], "true"),
            ("false", "pull_request", [], "true"),
            ("true", "pull_request", [], "false"),
            ("true", "push", [{"filename": "docs/roadmap.md"}], "false"),
            ("true", "push", [{"filename": "tools/ci.py"}], "true"),
            (
                "true",
                "push",
                [{"filename": "README.md", "previous_filename": "tools/tool.py"}],
                "true",
            ),
            ("true", "push", [{"filename": "docs/new.md"}] * 300, "true"),
            ("true", "push", [], "true"),
        ]
        for variable, event, files, expected in cases:
            with self.subTest(variable=variable, event=event, files=len(files)):
                harness = f"""
                process.env.CI_ON_DEMAND = {json.dumps(variable)};
                const outputs = {{}};
                const context = {{eventName: {json.dumps(event)}, repo: {{}}, sha: 'head',
                  payload: {{before: 'base', pull_request: {{base: {{ref: 'rust-migration'}}}}}}}};
                const core = {{setOutput: (key, value) => outputs[key] = value, info: () => {{}}}};
                const github = {{rest: {{repos: {{compareCommitsWithBasehead: async () => ({{data: {{files: {json.dumps(files)}}}}})}}}}}};
                (async () => {{{script}}})().then(() => console.log(JSON.stringify(outputs)));
                """
                result = json.loads(
                    subprocess.run(
                        ["node", "-e", harness],
                        check=True,
                        capture_output=True,
                        text=True,
                    ).stdout
                )
                self.assertEqual(result["heavy"], expected)


class RequestTests(unittest.TestCase):
    def test_each_profile_dispatches_captured_head_on_protected_ref(self):
        for profile in ci.PROFILES:
            with tempfile.TemporaryDirectory() as directory:
                args = argparse.Namespace(
                    repo="example/fork",
                    pr=170,
                    profile=profile,
                    bootstrap=False,
                    receipt=Path(directory) / "request.json",
                )
                with (
                    patch.object(ci, "api", return_value=PULL),
                    patch.object(ci, "gh") as gh,
                ):
                    receipt_path = ci.request(args)
                invocation = gh.call_args.args
                self.assertIn(f"head={HEAD}", invocation)
                self.assertIn(f"profile={profile}", invocation)
                self.assertEqual(
                    invocation[invocation.index("--ref") + 1], "rust-migration"
                )
                receipt = json.loads(receipt_path.read_text())
                self.assertEqual(receipt["head"], HEAD)
                self.assertEqual(receipt["base"], BASE)
                self.assertEqual(len(receipt["request"]), 32)

    def test_default_request_is_rust(self):
        with (
            patch("sys.argv", ["ci.py", "request", "170"]),
            patch.object(ci, "request", return_value=Path("unused")) as request,
        ):
            self.assertEqual(ci.main(), 0)
            self.assertEqual(request.call_args.args[0].profile, "rust")

    def test_run_discovery_uses_unique_request_identity(self):
        receipt = {
            "profile": "rust",
            "pr": 170,
            "request": "unique",
            "bootstrap": False,
        }
        run = {
            "id": 1,
            "display_title": "CI rust PR 170 unique",
            "event": "workflow_dispatch",
            "head_branch": "rust-migration",
        }
        other = dict(run, id=2, display_title="CI rust PR 170 other")
        self.assertEqual(ci.find_run(receipt, [other, run]), run)
        self.assertIsNone(ci.find_run(receipt, [dict(run, head_branch="untrusted")]))
        with self.assertRaises(ValueError):
            ci.find_run(receipt, [run, dict(run, id=3)])

    def test_push_invalidates_receipt_head(self):
        with self.assertRaisesRegex(ValueError, "head changed"):
            ci.validate_pull(PULL, "c" * 40)

    def test_successful_run_without_full_status_is_rejected(self):
        receipt = {
            "repository": "example/fork",
            "pr": 170,
            "head": HEAD,
            "profile": "full",
        }
        run = {
            "conclusion": "success",
            "html_url": "https://github.com/example/fork/actions/runs/123",
        }
        with patch.object(ci, "api", side_effect=[PULL, {"statuses": []}]):
            with self.assertRaisesRegex(ValueError, "did not publish"):
                ci.verify_completion(receipt, run)
        for state in ("pending", "failure"):
            statuses = [
                {"context": ci.CONTEXT, "target_url": run["html_url"], "state": state}
            ]
            with patch.object(ci, "api", side_effect=[PULL, {"statuses": statuses}]):
                with self.assertRaises(ValueError):
                    ci.verify_completion(receipt, run)


if __name__ == "__main__":
    unittest.main()
