#!/usr/bin/env python3
"""Exercise request identity and the actual privileged full-result publisher."""

import argparse
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
MERGE = "d" * 40
PULL = {
    "state": "open",
    "head": {"sha": HEAD},
    "base": {"sha": BASE, "ref": "rust-migration"},
    "labels": [],
    "mergeable": True,
    "merge_commit_sha": MERGE,
}


def script_for(filename, job):
    text = (ROOT / ".github/workflows" / filename).read_text()
    section = text.split(f"  {job}:\n", 1)[1]
    section = re.split(r"\n  [a-z_]+:\n", section, maxsplit=1)[0]
    match = re.search(r"        script: \|\n((?:          .*\n|\n)+)", section)
    if match is None:
        raise AssertionError(f"No publisher script in {filename}:{job}")
    return "\n".join(line[10:] for line in match[1].splitlines())


def node(harness):
    return json.loads(subprocess.check_output(["node", "-e", harness], text=True))


def evaluate(expression, **variables):
    declarations = "\n".join(
        f"const {key} = {json.dumps(value)};" for key, value in variables.items()
    )
    return node(declarations + f"console.log(JSON.stringify({expression}));")


def good_needs():
    needs = {
        name: {"result": "success", "outputs": {}}
        for name in ("plan", "quick", "native", "platform")
    }
    needs["plan"]["outputs"] = {
        "head": HEAD,
        "base": BASE,
        "merge": MERGE,
        "pr": "170",
    }
    for name in ("native", "platform"):
        needs[name]["outputs"]["validated"] = "true"
    return needs


def execute_script(filename, job, pull, *, env=None, event=None, latest=None):
    script = script_for(filename, job)
    return node(f"""
const statuses = [], failures = [], outputs = {{}};
const context = {{repo: {{owner: 'example', repo: 'fork'}}, actor: 'root',
  serverUrl: 'https://github.com', runId: 123, payload: {{pull_request: {json.dumps(event)}}}}};
const core = {{setFailed: value => failures.push(value), setOutput: (k,v) => outputs[k] = v}};
const github = {{rest: {{pulls: {{get: async () => ({{data: {json.dumps(pull)}}})}}, repos: {{
  getCollaboratorPermissionLevel: async () => ({{data: {{permission: 'write'}}}}),
  getCombinedStatusForRef: async () => ({{data: {{statuses: {json.dumps([latest] if latest else [])}}}}}),
  createCommitStatus: async value => statuses.push(value)
}}}}}};
Object.assign(process.env, {json.dumps(env or {})});
(async () => {{{script}}})().then(() => console.log(JSON.stringify({{statuses, failures, outputs}})))
  .catch(error => console.log(JSON.stringify({{error: error.message, statuses}})));
""")


def execute_publisher(needs, pull, superseded=False):
    latest = {
        "context": ci.CONTEXT,
        "target_url": "https://github.com/example/fork/actions/runs/"
        + ("999" if superseded else "123"),
    }
    return execute_script(
        "ci-request.yml",
        "publish",
        pull,
        env={"RESULTS": json.dumps(needs)},
        latest=latest,
    )


def execute_invalidator(event, pull, latest=None):
    return execute_script(
        "ci-invalidate.yml", "invalidate", pull, event=event, latest=latest
    )["statuses"]


class GateTests(unittest.TestCase):
    def test_plan_checks_mergeability_and_checks_out_merge_while_status_targets_head(
        self,
    ):
        for mergeable in (True, False, None):
            pull = dict(PULL, mergeable=mergeable)
            result = execute_script(
                "ci-request.yml",
                "plan",
                pull,
                env={"PR": "170", "HEAD": HEAD, "PROFILE": "full"},
            )
            if mergeable is True:
                self.assertEqual(result["outputs"]["merge"], MERGE)
                self.assertEqual(result["statuses"][0]["sha"], HEAD)
            else:
                self.assertIn("unmergeable", result["error"])
                self.assertEqual(result["statuses"], [])
        workflow = (ROOT / ".github/workflows/ci-request.yml").read_text()
        self.assertEqual(workflow.count("ref: ${{ needs.plan.outputs.merge }}"), 4)
        self.assertNotIn("ref: ${{ needs.plan.outputs.head }}", workflow)

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

    def test_status_writers_share_a_non_cancelling_per_pr_concurrency_group(self):
        groups = []
        for filename, job in (
            ("ci-request.yml", "plan"),
            ("ci-request.yml", "publish"),
            ("ci-invalidate.yml", "invalidate"),
        ):
            workflow = (ROOT / ".github/workflows" / filename).read_text()
            section = workflow.split(f"  {job}:\n", 1)[1]
            section = re.split(r"\n  [a-z_]+:\n", section, maxsplit=1)[0]
            concurrency = re.search(
                r"    concurrency:\n      group: (.+)\n      cancel-in-progress: (.+)",
                section,
            )
            self.assertIsNotNone(concurrency)
            self.assertEqual(concurrency[2], "false")
            expression = re.fullmatch(r"validation-status-\${{ (.+) }}", concurrency[1])
            self.assertIsNotNone(expression)
            # Dispatch inputs are strings; pull_request_target provides a number.
            for inputs in ({"pr": "170"}, {}):
                groups.append(
                    evaluate(
                        "'validation-status-' + (" + expression[1] + ")",
                        inputs=inputs,
                        github={"event": {"pull_request": {"number": 170}}},
                    )
                )
        self.assertEqual(groups, ["validation-status-170"] * 6)

    def test_delayed_invalidation_preserves_current_request(self):
        event = dict(PULL, number=170, updated_at="2026-10-07T12:00:00Z")
        for timestamp in (event["updated_at"], "2026-10-07T12:00:01Z"):
            with self.subTest(timestamp=timestamp):
                latest = {
                    "context": ci.CONTEXT,
                    "updated_at": timestamp,
                    "target_url": "https://github.com/example/fork/actions/runs/123",
                }
                self.assertEqual(execute_invalidator(event, PULL, latest), [])

    def test_invalidation_only_marks_current_unvalidated_head_pending(self):
        event = dict(PULL, number=170, updated_at="2026-10-07T12:00:00Z")
        older = {"context": ci.CONTEXT, "updated_at": "2026-10-07T11:59:59Z"}
        for latest in (None, older):
            with self.subTest(latest=latest):
                statuses = execute_invalidator(event, PULL, latest)
                self.assertEqual(len(statuses), 1)
                self.assertEqual(statuses[0]["state"], "pending")
                self.assertEqual(statuses[0]["sha"], HEAD)
                self.assertEqual(statuses[0]["context"], ci.CONTEXT)
        pull = dict(PULL, head={"sha": "c" * 40})
        self.assertEqual(execute_invalidator(event, pull), [])

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

    def test_missing_job_or_validation_proof_is_rejected(self):
        for job in ("quick", "native", "platform"):
            needs = good_needs()
            del needs[job]
            self.assertEqual(
                execute_publisher(needs, PULL)["statuses"][0]["state"], "failure"
            )
            if job in ("native", "platform"):
                needs = good_needs()
                needs[job]["outputs"] = {}
                self.assertEqual(
                    execute_publisher(needs, PULL)["statuses"][0]["state"], "failure"
                )

    def test_changed_pull_cannot_publish_success(self):
        for change in (
            {"head": {"sha": "c" * 40}},
            {"state": "closed"},
            {"base": {"ref": "master", "sha": BASE}},
            {"base": {"ref": "rust-migration", "sha": "e" * 40}},
            {"merge_commit_sha": "e" * 40},
            {"mergeable": False},
        ):
            self.assertEqual(
                execute_publisher(good_needs(), dict(PULL, **change))["statuses"][0][
                    "state"
                ],
                "failure",
            )

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
        self.assertIn(
            "needs.plan.outputs.profile == 'full'", workflow.split("  publish:\n", 1)[1]
        )
        self.assertIn("if (profile === 'full')", script_for("ci-request.yml", "plan"))

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
                    with self.subTest(filename=filename, job=job, cancelled=cancelled):
                        result = evaluate(
                            expression.replace(
                                "inputs.checkout-ref", 'inputs["checkout-ref"]'
                            ).replace("cancelled()", str(cancelled).lower()),
                            inputs=inputs,
                            needs=needs,
                        )
                        self.assertEqual(result, expected)

    def test_doc_after_code_push_has_separate_non_cancelling_groups(self):
        for filename in ("ci-build.yml", "rust-migration.yml", "ci-quick.yml"):
            workflow = (ROOT / ".github/workflows" / filename).read_text()
            expression = re.search(r"^  group: (.+)$", workflow, re.MULTILINE)[1]
            expression = (
                expression.rsplit("${{ ", 1)[1]
                .removesuffix(" }}")
                .replace("inputs.checkout-ref", 'inputs["checkout-ref"]')
            )
            groups = []
            for sha in ("code-merge", "later-docs"):
                groups.append(
                    evaluate(
                        expression,
                        inputs={},
                        github={
                            "ref": "refs/heads/rust-migration",
                            "sha": sha,
                            "run_id": 1,
                        },
                    )
                )
            self.assertNotEqual(*groups)
            self.assertIn("github.ref != 'refs/heads/rust-migration'", workflow)

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
                result = node(harness)
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
