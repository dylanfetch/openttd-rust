#!/usr/bin/env python3
"""Exercise request identity and the actual privileged full-result publisher."""

import argparse
import gzip
import io
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest
from contextlib import chdir, nullcontext
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import cache_payload
import cache_zstd
import ci

ROOT = Path(__file__).resolve().parents[1]
HEAD = "a" * 40
BASE = "b" * 40
MERGE = "d" * 40
TREE = "f" * 40
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
        "tree": TREE,
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
  getCommit: async () => ({{data: {{commit: {{tree: {{sha: {json.dumps(TREE)}}}}}}}}}),
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
        quick = (ROOT / ".github/workflows/ci-quick.yml").read_text()
        self.assertIn("HEAD: ${{ inputs.checkout-ref }}^2", quick)

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
        ):
            self.assertEqual(
                execute_publisher(good_needs(), dict(PULL, **change))["statuses"][0][
                    "state"
                ],
                "failure",
            )

    def test_base_advances_and_mergeability_pending_preserve_exact_head_validation(
        self,
    ):
        for change in (
            {"base": {"ref": "rust-migration", "sha": "e" * 40}},
            {"merge_commit_sha": "e" * 40},
            {"mergeable": None},
            {"mergeable": False},
        ):
            result = execute_publisher(good_needs(), dict(PULL, **change))
            self.assertEqual(result["statuses"][0]["state"], "success")
            marker = result["statuses"][1]
            self.assertEqual(marker["context"], "Validated tree")
            self.assertEqual(marker["description"], TREE)
            self.assertEqual(marker["target_url"], result["statuses"][0]["target_url"])

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


class CacheTests(unittest.TestCase):
    def test_real_ccache_fit_evicts_with_environment_limits_and_preserves_fallback(
        self,
    ):
        executable = shutil.which("ccache")
        compiler = shutil.which("cc")
        self.assertIsNotNone(executable, "Default cache checks require ccache")
        self.assertIsNotNone(compiler, "Default cache checks require a C compiler")
        config = ROOT / "migration/ccache.conf"
        config_bytes = config.read_bytes()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            sources = [root / "reference", root / "candidate"]
            env = {
                key: value
                for key, value in os.environ.items()
                if not key.startswith("CCACHE_")
            }
            env.update(
                CCACHE_CONFIGPATH=str(config),
                CCACHE_MAXSIZE="0",
                CCACHE_TEMPDIR=str(root / "tmp"),
            )
            # Four small, real compilations create valid incompressible results.
            # No game build or synthetic files masquerading as ccache entries.
            data = "".join(f"\\x{byte:02x}" for byte in os.urandom(768 * 1024))
            for index in range(4):
                source = root / f"{index}.c"
                source.write_text(f'const unsigned char value{index}[] = "{data}";\n')
                subprocess.run(
                    [
                        executable,
                        compiler,
                        "-c",
                        str(source),
                        "-o",
                        str(root / "object.o"),
                    ],
                    env=dict(env, CCACHE_DIR=str(sources[index // 2])),
                    check=True,
                    capture_output=True,
                )
            for source in sources:
                self.assertEqual(len(list(source.rglob("*R"))), 2)
                (source / "ccache.conf").write_text("max_size = 7GiB\n")
            fallback = root / "fallback.tgz"
            cache_payload.pack(fallback, sources, "native", "image", BASE)
            fallback_bytes = fallback.read_bytes()
            allocations = [cache_payload.allocated_size(source) for source in sources]
            with (
                patch.dict(os.environ, dict(env, CCACHE_EXE=executable), clear=True),
                patch.dict(cache_payload.POLICY["families"], {"native": 4}),
                patch.object(cache_payload, "pack", wraps=cache_payload.pack) as pack,
                patch.object(
                    cache_payload.subprocess, "run", wraps=subprocess.run
                ) as cleanup,
            ):
                item = cache_payload.pack_to_fit(
                    root / "upload.tgz", sources, "native", "image", HEAD
                )
            self.assertGreater(pack.call_count, 1)
            self.assertEqual(cleanup.call_count, 2)
            for source, before, call in zip(
                sources, allocations, cleanup.call_args_list, strict=True
            ):
                self.assertEqual(call.kwargs["env"]["CCACHE_DIR"], str(source))
                self.assertRegex(call.kwargs["env"]["CCACHE_MAXSIZE"], r"^[1-9]\d*KiB$")
                self.assertLess(cache_payload.allocated_size(source), before)
                self.assertLess(len(list(source.rglob("*R"))), 2)
                self.assertEqual(
                    (source / "ccache.conf").read_text(), "max_size = 7GiB\n"
                )
            self.assertLessEqual(item["envelope"], 4 * 1024**2)
            self.assertEqual(fallback.read_bytes(), fallback_bytes)
            self.assertEqual(config.read_bytes(), config_bytes)

    def test_measured_oversize_trims_current_roots_and_repacks_before_admission(self):
        path_stat = Path.stat

        def windows_stat(path, *args, **kwargs):
            info = path_stat(path, *args, **kwargs)
            return SimpleNamespace(st_mode=info.st_mode, st_size=info.st_size)

        for family, executable in (
            ("native", None),
            ("macos-debug", "/runner/ccache-bin/ccache"),
            ("msvc-x64", "C:/runner/ccache-bin/ccache.exe"),
        ):
            with (
                self.subTest(family=family),
                tempfile.TemporaryDirectory() as temporary,
            ):
                root = Path(temporary)
                sources = [root / "cache0"]
                if family == "native":
                    sources.append(root / "cache1")
                for source in sources:
                    source.mkdir()
                    for index in range(16 // len(sources)):
                        (source / f"{index:02d}").write_bytes(os.urandom(192 * 1024))
                before = sum(len(list(source.iterdir())) for source in sources)
                cleanups = []

                def cleanup(
                    command,
                    *,
                    env,
                    check,
                    executable=executable,
                    sources=sources,
                    cleanups=cleanups,
                ):
                    self.assertEqual(command, [executable or "ccache", "--cleanup"])
                    self.assertTrue(check)
                    self.assertEqual(
                        env["CCACHE_CONFIGPATH"],
                        str(ROOT / "migration/ccache.conf"),
                    )
                    source = Path(env["CCACHE_DIR"])
                    self.assertIn(source, sources)
                    target = int(env["CCACHE_MAXSIZE"].removesuffix("KiB")) * 1024
                    self.assertLess(target, cache_payload.allocated_size(source))
                    cleanups.append(source)
                    # Deterministic cleanup service; packing and measurement are
                    # real. Evict only enough old entries to reach the request.
                    for child in sorted(source.iterdir()):
                        if cache_payload.allocated_size(source) <= target:
                            break
                        child.unlink()

                state = root / f".local/cache-state/{family}"
                state.mkdir(parents=True)
                (state / "publication.json").write_text(json.dumps({"legacy": []}))
                env = {
                    "CACHE_FAMILY": family,
                    "CACHE_PREFIX": "compiler/image",
                    "CACHE_PATHS": "\n".join(map(str, sources)),
                    "GITHUB_SHA": HEAD,
                    "GITHUB_OUTPUT": str(root / "output"),
                }
                if executable:
                    env["CCACHE_EXE"] = executable
                with (
                    chdir(root),
                    patch.object(Path, "stat", windows_stat)
                    if family.startswith("msvc-")
                    else nullcontext(),
                    patch.dict(os.environ, env, clear=True),
                    patch.dict(cache_payload.POLICY["families"], {family: 4}),
                    patch.object(sys, "argv", ["cache_payload.py", "pack"]),
                    patch.object(cache_payload.subprocess, "run", side_effect=cleanup),
                    patch.object(cache_zstd, "install") as install,
                    patch.object(
                        cache_payload, "pack", wraps=cache_payload.pack
                    ) as pack,
                ):
                    cache_payload.main()
                    self.assertGreater(pack.call_count, 1)
                    install.assert_called_once_with(
                        4 * 1024**2, state.relative_to(root) / "outer.json"
                    )
                self.assertEqual(cleanups, sources)
                after = sum(len(list(source.iterdir())) for source in sources)
                self.assertGreaterEqual(after, before * 0.8)
                self.assertLess(after, before)
                item = json.loads((state / "upload.json").read_text())
                self.assertLessEqual(item["envelope"], 4 * 1024**2)
                self.assertIn(f"hash={item['hash']}", (root / "output").read_text())

    def test_local_fit_exhaustion_never_admits_save_and_preserves_fallback(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "cache"
            source.mkdir()
            (source / "object").write_bytes(os.urandom(64 * 1024))
            family = "macos-debug"
            state = root / f".local/cache-state/{family}"
            state.mkdir(parents=True)
            fallback = state / "0.tgz"
            cache_payload.pack(fallback, [source], family, "image", BASE)
            original = fallback.read_bytes()
            (state / "publication.json").write_text(json.dumps({"legacy": []}))
            env = {
                "CACHE_FAMILY": family,
                "CACHE_PREFIX": "image",
                "CACHE_PATHS": str(source),
                "GITHUB_SHA": HEAD,
                "GITHUB_OUTPUT": str(root / "output"),
            }
            with (
                chdir(root),
                patch.dict(os.environ, env, clear=True),
                patch.dict(cache_payload.POLICY["families"], {family: 1}),
                patch.object(sys, "argv", ["cache_payload.py", "pack"]),
                patch.object(cache_payload.subprocess, "run") as cleanup,
                patch.object(cache_zstd, "install") as install,
                patch.object(cache_payload, "pack", wraps=cache_payload.pack) as pack,
            ):
                with self.assertRaises(cache_payload.PayloadTooLarge):
                    cache_payload.main()
            self.assertEqual(cleanup.call_count, cache_payload.FIT_RETRIES)
            self.assertEqual(pack.call_count, cache_payload.FIT_RETRIES + 1)
            install.assert_not_called()
            self.assertFalse((state / "upload.json").exists())
            self.assertFalse((root / "output").exists())
            self.assertFalse((root / cache_payload.payload_path(family)).exists())
            self.assertEqual(fallback.read_bytes(), original)
            action = (
                ROOT / ".github/actions/bounded-compiler-cache/action.yml"
            ).read_text()
            save = action.split("  - name: Save the admitted immutable slot\n", 1)[1]
            condition = re.search(r"    if: (.+)", save)[1]
            self.assertEqual(
                [
                    evaluate(
                        condition,
                        inputs={"operation": "publish"},
                        steps={
                            "prepare": {"outputs": {"publish": "true"}},
                            "pack": {"outcome": outcome},
                        },
                    )
                    for outcome in ("success", "failure")
                ],
                [True, False],
            )

    def test_non_size_pack_and_cleanup_errors_stop_without_size_retries(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "cache"
            source.mkdir()
            (source / "object").write_bytes(os.urandom(64 * 1024))
            with (
                patch.dict(cache_payload.POLICY["families"], {"macos-debug": 1}),
                patch.object(cache_payload.subprocess, "run") as cleanup,
            ):
                # A bad manifest on an oversized archive is metadata failure,
                # not permission to trim and silently retry it.
                with self.assertRaisesRegex(ValueError, "manifest"):
                    cache_payload.pack_to_fit(
                        root / "bad.tgz", [source], "macos-debug", "image", "bad-sha"
                    )
                cleanup.assert_not_called()
            failure = subprocess.CalledProcessError(1, ["ccache", "--cleanup"])
            with (
                patch.dict(cache_payload.POLICY["families"], {"macos-debug": 1}),
                patch.object(
                    cache_payload.subprocess, "run", side_effect=failure
                ) as cleanup,
                patch.object(cache_payload, "pack", wraps=cache_payload.pack) as pack,
            ):
                with self.assertRaises(subprocess.CalledProcessError):
                    cache_payload.pack_to_fit(
                        root / "failed.tgz", [source], "macos-debug", "image", HEAD
                    )
                cleanup.assert_called_once()
                pack.assert_called_once()

    def test_installed_compressor_launchers_preserve_shell_line_endings(self):
        path_open = Path.open

        def windows_text_open(path, mode="r", *args, **kwargs):
            if mode == "w" and kwargs.get("newline") is None:
                kwargs["newline"] = "\r\n"
            return path_open(path, mode, *args, **kwargs)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            receipt = root / "outer.json"
            env = dict(
                os.environ,
                RUNNER_TEMP=str(root),
                GITHUB_PATH=str(root / "github-path"),
            )
            # Exercise Windows newline translation even on Linux default CI.
            with (
                patch.dict(os.environ, env),
                patch.object(Path, "open", windows_text_open),
            ):
                cache_zstd.install(1024, receipt)
            guard = root / "cache-archive-guard"
            for name in ("zstd", "zstdmt"):
                data = (guard / name).read_bytes()
                self.assertTrue(data.startswith(b"#!/usr/bin/env bash\n"))
                self.assertNotIn(b"\r", data)
            windows = (guard / "zstd.cmd").read_bytes()
            self.assertTrue(windows.endswith(b"\r\n"))
            self.assertEqual(windows.count(b"\r"), 1)
            env["PATH"] = f"{guard}{os.pathsep}{os.environ['PATH']}"
            with (
                patch.dict(os.environ, env),
                patch.object(Path, "open", windows_text_open),
            ):
                cache_zstd.probe(1024, receipt)

    def test_actual_sdk_compressor_stream_and_file_outputs_are_capped_before_upload(
        self,
    ):
        binary = shutil.which("zstd")
        self.assertIsNotNone(binary, "Default cache policy checks require zstd")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "payload").write_bytes(os.urandom(256 * 1024))
            (root / "manifest.txt").write_text("payload\n", newline="")
            guard = root / "guard"
            guard.mkdir()
            receipt = root / "outer.json"
            for name, text in cache_zstd.wrappers(
                Path(sys.executable).as_posix(),
                (ROOT / "tools/cache_zstd.py").as_posix(),
                Path(binary).as_posix(),
                1024 * 1024,
                str(receipt),
            ).items():
                path = guard / name
                path.write_text(text, newline="")
                path.chmod(0o755)
            env = dict(os.environ, PATH=f"{guard}{os.pathsep}{os.environ['PATH']}")
            # Exact pinned SDK GNU-tar flags and compressor invocation; the
            # real outer archive (not its inner payload) reaches the guard.
            command = [
                cache_zstd.sdk_tar(),
                "--posix",
                "-cf",
                "cache.tzst",
                "--exclude",
                "cache.tzst",
                "-P",
                "-C",
                str(root),
                "--files-from",
                "manifest.txt",
                "--use-compress-program",
                "zstd -T0" if sys.platform == "win32" else "zstdmt",
            ]
            if sys.platform == "win32":
                command.append("--force-local")
            elif sys.platform == "darwin" and Path(command[0]).name == "gtar":
                command.append("--delay-directory-restore")
            subprocess.run(command, cwd=root, env=env, check=True, capture_output=True)
            measured = json.loads(receipt.read_text())["size"]
            self.assertEqual(measured, (root / "cache.tzst").stat().st_size)
            self.assertGreater(measured, (root / "payload").stat().st_size)
            with patch.dict(os.environ, env):
                cache_zstd.probe(1024 * 1024, receipt)
            # Missing PATH injection stops before the SDK can upload.
            with self.assertRaises(subprocess.CalledProcessError):
                cache_zstd.probe(1024 * 1024, receipt)
            # Exceeding the cap fails tar. SDK reserve/upload follows only a
            # successful tar call, so no oversized replacement can be uploaded.
            receipt.unlink()
            for name, text in cache_zstd.wrappers(
                Path(sys.executable).as_posix(),
                (ROOT / "tools/cache_zstd.py").as_posix(),
                Path(binary).as_posix(),
                measured - 1,
                str(receipt),
            ).items():
                (guard / name).write_text(text, newline="")
            rejected = subprocess.run(command, cwd=root, env=env, capture_output=True)
            self.assertNotEqual(rejected.returncode, 0)
            self.assertFalse(receipt.exists())
            # The SDK's Windows BSD-tar fallback uses zstd -o instead of stdout.
            for limit, success in ((1024 * 1024, True), (1, False)):
                result = subprocess.run(
                    [
                        sys.executable,
                        str(ROOT / "tools/cache_zstd.py"),
                        "--binary",
                        binary,
                        "--limit",
                        str(limit),
                        "--",
                        "-T0",
                        "--force",
                        "-o",
                        str(root / "file.tzst"),
                        str(root / "payload"),
                    ],
                    capture_output=True,
                )
                self.assertEqual(result.returncode == 0, success)
            version_command = [str(guard / "zstd"), "--quiet", "--version"]
            if sys.platform == "win32":
                version_command = [
                    "cmd",
                    "/d",
                    "/c",
                    str(guard / "zstd.cmd"),
                    "--quiet",
                    "--version",
                ]
            version = subprocess.check_output(version_command)
            self.assertEqual(
                version, subprocess.check_output([binary, "--quiet", "--version"])
            )
            launcher = cache_zstd.wrappers(
                "C:/Python/python.exe",
                "C:/repo/cache_zstd.py",
                "C:/Program Files/zstd.exe",
                123,
                "C:/repo/outer.json",
            )
            self.assertIn('"C:/Program Files/zstd.exe"', launcher["zstd.cmd"])
            self.assertIn(" -- %*", launcher["zstd.cmd"])

    def test_restored_payload_body_must_match_claimed_manifest_before_prune(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            sources = [root / "reference", root / "candidate"]
            for source in sources:
                source.mkdir()
                (source / "object").write_bytes(b"compiler object")
            archive = root / "original.tgz"
            cache_payload.pack(archive, sources, "native", "image", HEAD)
            changed = root / "tampered.tgz"
            with (
                tarfile.open(archive, "r:gz") as original,
                tarfile.open(changed, "w:gz") as target,
            ):
                for member in original.getmembers():
                    data = (
                        original.extractfile(member).read() if member.isfile() else None
                    )
                    if member.name == "root0/object":
                        data = b"altered! object"
                        member.size = len(data)
                    target.addfile(
                        member, io.BytesIO(data) if data is not None else None
                    )
            with self.assertRaisesRegex(ValueError, "content hash"):
                cache_payload.metadata(changed)

    def test_real_packing_preserves_roots_and_rejects_oversize_or_wrong_compatibility(
        self,
    ):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            sources = [root / "reference", root / "candidate"]
            for index, source in enumerate(sources):
                (source / "ab").mkdir(parents=True)
                (source / "ab" / "object").write_bytes(os.urandom(32768 + index))
            archive = root / "native.tgz"
            receipt = cache_payload.pack(
                archive, sources, "native", "compiler/image", HEAD
            )
            # Exercise the real tar + compressor envelope with incompressible
            # input, alongside the conservative pre-upload bound.
            outer = io.BytesIO()
            with tarfile.open(
                fileobj=outer, mode="w", format=tarfile.PAX_FORMAT
            ) as packed:
                packed.add(archive, arcname=".local/cache-payload/native.tgz")
            self.assertLessEqual(
                len(gzip.compress(outer.getvalue())), receipt["envelope"]
            )
            destinations = [root / "restored-reference", root / "restored-candidate"]
            self.assertFalse(
                cache_payload.unpack(archive, destinations, "native", "different-image")
            )
            self.assertTrue(
                cache_payload.unpack(archive, destinations, "native", "compiler/image")
            )
            for source, restored in zip(sources, destinations, strict=True):
                self.assertEqual(
                    (source / "ab/object").read_bytes(),
                    (restored / "ab/object").read_bytes(),
                )
            with patch.dict(cache_payload.POLICY["families"], {"native": 1}):
                with self.assertRaisesRegex(ValueError, "envelope"):
                    cache_payload.pack(
                        archive, sources, "native", "compiler/image", HEAD
                    )

    def test_candidate_callers_limit_cache_access_and_dependencies_do_not_save(self):
        for filename, jobs in (
            ("ci-request.yml", ("quick", "rust", "native", "platform")),
            ("ci-build.yml", ("emscripten", "linux", "macos", "windows")),
        ):
            text = (ROOT / ".github/workflows" / filename).read_text()
            for job in jobs:
                section = text.split(f"  {job}:\n", 1)[1].split("\n  ", 1)[0]
                self.assertIn("    cache-mode: read", section)
        for filename in ("ci-emscripten.yml", "preview-build.yml"):
            text = (ROOT / ".github/workflows" / filename).read_text()
            self.assertNotIn("uses: actions/cache@", text)
            self.assertIn("uses: actions/cache/restore@", text)
        self.assertNotIn(
            "actions/cache/save@",
            (ROOT / ".github/actions/install-opengfx/action.yml").read_text(),
        )
        self.assertIn(
            "enable-cache: false",
            (ROOT / ".github/workflows/ci-quick.yml").read_text(),
        )
        self.assertIn(
            "cache: false", (ROOT / ".github/workflows/ci-mingw.yml").read_text()
        )

    def cache_model(self, scenario):
        """Execute the same controller module that the protected action invokes."""
        return node(f"""
const policy = require({json.dumps(str(ROOT / "tools/cache_policy.cjs"))});
const scenario = {json.dumps(scenario)};
let caches = scenario.caches || [], files = scenario.files || {{}}, deleted = [], outputs = {{}};
const versions = Object.fromEntries(Object.keys(policy.policy.families).map(f => [f, policy.versionFor(f)]));
for (const cache of caches) {{
  cache.ref ??= 'refs/heads/rust-migration';
  const match = /^bounded-v1-(.+)-[01]$/.exec(cache.key);
  cache.version ??= match ? versions[match[1]] : 'dependency-version';
}}
const context = {{repo: {{}}, eventName: scenario.event || 'push',
  ref: scenario.ref || 'refs/heads/rust-migration', sha: scenario.sha || {json.dumps(HEAD)}}};
const core = {{info: () => {{}}, setOutput: (key, value) => outputs[key] = value}};
const github = {{paginate: async () => structuredClone(caches), rest: {{
  actions: {{getActionsCacheList: () => {{}}, deleteActionsCacheById: async ({{cache_id}}) => {{
    if (scenario.deleteFailure) throw Error('delete failed');
    deleted.push(cache_id);
    if (!scenario.staleDeletion) caches = caches.filter(cache => cache.id !== cache_id);
    return {{status: 204}};
  }}}},
  repos: {{compareCommitsWithBasehead: async ({{basehead}}) => {{
    const [base, head] = basehead.split('...');
    return {{data: {{status: scenario.diverged ? 'diverged' :
      head === base ? 'identical' : head > base ? 'ahead' : 'behind'}}}};
  }}}}
}}}};
(async () => {{
  try {{
    if (scenario.inventoryOnly) policy.inventory(caches);
    else await policy.control({{github, context, core, family: scenario.family || 'native',
      operation: scenario.operation || 'prepare',
      read: name => files[name] || null, write: (name, value) => files[name] = value}});
    console.log(JSON.stringify({{outputs, deleted, caches, files}}));
  }} catch (error) {{console.log(JSON.stringify({{error: error.message, outputs, deleted, caches, files}}));}}
}})();
""")

    @staticmethod
    def slot(family, slot, sha=HEAD, size=1024):
        key = f"bounded-v1-{family}-{slot}"
        cache = {"id": slot + 10, "key": key, "size_in_bytes": size}
        payload = {
            "schema": 1,
            "family": family,
            "sha": sha,
            "key": key,
            "hash": "e" * 64,
            "content_hash": "d" * 64,
            "bytes": size,
            "compatibility": "image/compiler/config",
        }
        return cache, payload

    def test_fixed_slot_protocol_confirms_before_deleting_and_preserves_newer_ancestry(
        self,
    ):
        older, old_payload = self.slot("native", 0, "a" * 40)
        newer, new_payload = self.slot("native", 1, "b" * 40)
        for creation in ("earlier", "later"):
            newer["created_at"] = creation  # Wall-clock ordering is irrelevant.
            result = self.cache_model(
                {
                    "caches": [older, newer],
                    "files": {"0.json": old_payload, "1.json": new_payload},
                    "sha": "c" * 40,
                }
            )
            self.assertNotIn("error", result)
            self.assertEqual(result["deleted"], [older["id"]])
            self.assertEqual(result["outputs"]["key"], older["key"])
        result = self.cache_model({"caches": [newer], "files": {"1.json": new_payload}})
        self.assertEqual(result["outputs"]["publish"], "false")
        self.assertEqual(result["deleted"], [])

    def test_failure_cancellation_stale_metadata_and_versions_stop_publication(self):
        older, payload = self.slot("native", 0)
        replacement, replacement_payload = self.slot("native", 1, "b" * 40)
        cases = [
            {"caches": [older]},  # Listed slot could not actually be restored.
            {"files": {"0.json": payload}},  # Restore hit missing from stale list.
            {"caches": [dict(older, version="different-compression")]},
            {"caches": [dict(older, ref="refs/pull/123/merge")]},
            {"caches": [older, dict(older, id=99)]},
            {"caches": [dict(older, size_in_bytes=1100 * 1024**2 + 1)]},
            {
                "caches": [
                    {"id": 99, "key": "setup-uv-old", "size_in_bytes": 1024**3 + 1}
                ]
            },
            {
                "caches": [older, replacement],
                "files": {"0.json": payload, "1.json": replacement_payload},
                "deleteFailure": True,
            },
            {
                "caches": [older, replacement],
                "files": {"0.json": payload, "1.json": replacement_payload},
                "staleDeletion": True,
            },
            {
                "caches": [older, replacement],
                "files": {"0.json": payload, "1.json": replacement_payload},
                "diverged": True,
            },
            {"event": "pull_request"},
            {"ref": "refs/heads/feature"},
        ]
        for scenario in cases:
            with self.subTest(scenario=scenario):
                result = self.cache_model(scenario)
                self.assertIn("error", result)
                self.assertNotIn("key", result["outputs"])
        # Failed/cancelled/unfinished uploads cannot authorize predecessor deletion.
        state = {"family": "native", "sha": HEAD, "target": 1, "predecessor": older}
        files = {
            "0.json": payload,
            "publication.json": state,
            "upload.json": dict(payload, envelope=2048),
        }
        result = self.cache_model(
            {"operation": "finish", "caches": [older], "files": files}
        )
        self.assertIn("error", result)
        self.assertEqual(result["deleted"], [])
        # Save@v4 warns instead of failing on reservation collision: a restored
        # old replacement hash is still rejected by the actual final controller.
        files["1.json"] = dict(replacement_payload, hash="f" * 64)
        result = self.cache_model(
            {"operation": "finish", "caches": [older, replacement], "files": files}
        )
        self.assertIn("error", result)
        self.assertEqual(result["deleted"], [])

    def test_every_image_key_generation_counts_in_family_transition(self):
        legacy = {
            "id": 1,
            "key": "compiler-v19-other-image-" + HEAD,
            "size_in_bytes": 1024,
        }
        slot, payload = self.slot("native", 0, "b" * 40)
        result = self.cache_model({"caches": [legacy], "sha": "b" * 40})
        self.assertEqual(result["outputs"]["key"], slot["key"])
        result = self.cache_model(
            {"caches": [legacy, slot], "files": {"0.json": payload}, "sha": "c" * 40}
        )
        self.assertEqual(result["deleted"], [1])
        self.assertEqual(result["outputs"]["key"], "bounded-v1-native-1")
        for key in (
            "ccache-linux-v19-gcc-e3b0c44298fc-new-image-config-" + HEAD,
            "ccache-macos-v19-arm64-new-image-a763a3567032-config-" + HEAD,
            "ccache-msvc-v19-x64-new-image-config-" + HEAD,
        ):
            result = self.cache_model(
                {
                    "caches": [{"id": 1, "key": key, "size_in_bytes": 1024}],
                    "inventoryOnly": True,
                }
            )
            self.assertNotIn("error", result)
        self.assertIn(
            "error",
            self.cache_model(
                {
                    "caches": [
                        {
                            "id": 1,
                            "key": "ccache-macos-v1-unclassified",
                            "size_in_bytes": 1024,
                        }
                    ],
                    "inventoryOnly": True,
                }
            ),
        )
        self.assertIn(
            "error",
            self.cache_model(
                {
                    "caches": [legacy, dict(slot, key="bounded-v1-native-1")],
                    "inventoryOnly": True,
                }
            ),
        )

    def test_hidden_legacy_predecessor_is_reconciled_before_slot_one_admission(self):
        legacy = {
            "id": 1,
            "key": "compiler-v1-old-image-" + "a" * 40,
            "ref": "refs/heads/rust-migration",
            "version": "legacy-version",
            "size_in_bytes": 1024,
        }
        slot, payload = self.slot("native", 0, "b" * 40)
        payload["legacy"] = [legacy]
        scenario = {"caches": [slot], "files": {"0.json": payload}, "sha": "c" * 40}
        result = self.cache_model(scenario)
        self.assertNotIn("error", result)
        self.assertEqual(result["deleted"], [legacy["id"]])
        self.assertEqual(result["outputs"]["key"], "bounded-v1-native-1")
        self.assertEqual(result["files"]["publication.json"]["legacy"], [])
        result = self.cache_model(dict(scenario, deleteFailure=True))
        self.assertIn("error", result)
        self.assertNotIn("key", result["outputs"])
        payload["legacy"] = [dict(legacy, version=None)]
        result = self.cache_model(scenario)
        self.assertIn("error", result)
        self.assertEqual(result["deleted"], [])
        self.assertNotIn("key", result["outputs"])

    def test_raw_cache_api_identities_must_be_complete_before_admission(self):
        # Do not use cache_model's convenient metadata defaults: these are the
        # unmodified REST records that reach the production inventory checker.
        result = node(f"""
const policy = require({json.dumps(str(ROOT / "tools/cache_policy.cjs"))});
const original = {{id: 1, key: 'restore-only', ref: 'refs/heads/rust-migration',
  version: 'legacy-version', size_in_bytes: 1}};
const rejected = [];
for (const field of ['id', 'key', 'ref', 'version']) {{
  for (const value of [undefined, null, '', 0]) {{
    try {{ policy.inventory([{{...original, [field]: value}}]); rejected.push(false); }}
    catch {{ rejected.push(true); }}
  }}
}}
console.log(JSON.stringify(rejected));
""")
        self.assertEqual(result, [True] * 16)

    def test_three_overlapping_merges_have_two_fixed_slots_per_family_in_both_orders(
        self,
    ):
        # Execute the publication controller with all families' save waves in
        # flight together. The workflow lock serializes owners within a family;
        # cache reservations include unfinished uploads and immutable identity.
        harness = f"""
const p = require({json.dumps(str(ROOT / "tools/cache_policy.cjs"))});
let peak = 0, completed = 0;
const MiB = 1024 ** 2, families = Object.keys(p.policy.families);
(async () => {{
  for (const order of [['a','b','c'], ['c','b','a']]) {{
    let caches = [], next = 100;
    const record = () => {{
      p.inventory(caches);
      peak = Math.max(peak, caches.reduce((sum, cache) => sum + cache.size_in_bytes, 0) + p.policy.other_mib * MiB);
    }};
    const github = {{paginate: async () => {{record(); return structuredClone(caches);}}, rest: {{
      actions: {{getActionsCacheList: () => {{}}, deleteActionsCacheById: async ({{cache_id}}) => {{
        caches = caches.filter(cache => cache.id !== cache_id); record(); return {{status: 204}};
      }}}},
      repos: {{compareCommitsWithBasehead: async ({{basehead}}) => {{
        const [base, head] = basehead.split('...');
        return {{data: {{status: head === base ? 'identical' : head > base ? 'ahead' : 'behind'}}}};
      }}}}
    }}}};
    for (const letter of order) {{
      const actors = [];
      for (const family of families) {{
        const sha = letter.repeat(40), files = {{}}, outputs = {{}};
        for (let slot = 0; slot < 2; slot++) {{
          const cache = caches.find(cache => cache.key === p.slotKey(family, slot));
          if (cache) files[`${{slot}}.json`] = cache.payload;
        }}
        const core = {{info: () => {{}}, setOutput: (k,v) => outputs[k] = v}};
        const context = {{repo: {{}}, eventName: 'push', ref: 'refs/heads/rust-migration', sha}};
        const actor = {{github, context, core, family, read: name => files[name] || null,
          write: (name, value) => files[name] = value}};
        await p.control({{...actor, operation: 'prepare'}});
        if (outputs.publish === 'false') continue; // Late older protected run.
        const slot = Number(outputs.slot), bytes = p.policy.families[family] * MiB;
        const payload = {{schema: 1, family, sha, key: outputs.key, compatibility: `image-${{letter}}`,
          hash: letter.repeat(64), content_hash: letter.repeat(64), bytes: Math.floor(bytes / 2)}};
        // Backend reserve rejects an occupied or unfinished key/version/ref.
        if (caches.some(cache => cache.key === outputs.key)) throw Error('Slot reservation collision');
        caches.push({{id: next++, key: outputs.key, version: p.versionFor(family),
          ref: context.ref, size_in_bytes: bytes, payload}});
        record();
        files[`${{slot}}.json`] = payload; // Actual successful backend restore/hash check.
        files['upload.json'] = {{...payload, envelope: bytes}};
        files['outer.json'] = {{size: bytes, cap: p.policy.families[family] * MiB, sha}};
        actors.push(actor);
      }}
      // Save-before-delete: all families have replacements at the same time.
      for (const actor of actors.reverse()) {{
        await p.control({{...actor, operation: 'finish'}}); completed++;
      }}
    }}
    if (caches.length !== families.length || caches.some(cache => cache.payload.sha !== 'c'.repeat(40))) {{
      throw Error('An older merge replaced newer protected cache state');
    }}
  }}
  console.log(JSON.stringify({{peak, completed,
    settled: (Object.values(p.policy.families).reduce((a,b) => a+b,0) + p.policy.other_mib) * MiB}}));
}})().catch(error => {{ console.error(error); process.exit(1); }});
"""
        result = node(harness)
        self.assertEqual(result["peak"], 10184 * 1024**2)
        self.assertEqual(result["settled"], 5316 * 1024**2)
        self.assertEqual(result["completed"], 28)
        self.assertLess(result["peak"], 10 * 1024**3)
        self.assertLessEqual(result["settled"], 8 * 1024**3)

    def test_protected_routes_lock_stable_families_and_never_accept_candidate_artifacts(
        self,
    ):
        for filename in (
            "ci-native-build.yml",
            "ci-linux.yml",
            "ci-macos.yml",
            "ci-windows.yml",
        ):
            text = (ROOT / ".github/workflows" / filename).read_text()
            self.assertIn("queue: max", text)
            self.assertIn("cancel-in-progress: false", text)
            self.assertIn("protected-cache-", text)
            self.assertIn('test -z "$CHECKOUT_REF"', text)
            self.assertIn('test "$REF" = refs/heads/rust-migration', text)
            self.assertIn(
                "ref: ${{ inputs.protected-writer && github.sha || inputs.checkout-ref }}",
                text,
            )
            self.assertNotIn("download-artifact", text)
        action = (
            ROOT / ".github/actions/bounded-compiler-cache/action.yml"
        ).read_text()
        self.assertIn("0057852bfaa89a56745cba8c7296529d2fc39830", action)
        self.assertIn("fail-on-cache-miss: true", action)
        self.assertLess(
            action.index("cache-guard-probe"),
            action.index("Save the admitted immutable slot"),
        )
        self.assertIn("cache_payload.py verify", action)
        self.assertLess(
            action.index("cache_payload.py verify"), action.index("'finish'")
        )
        audit = (ROOT / ".github/workflows/ci-cache.yml").read_text()
        self.assertIn("actions: read", audit)
        self.assertNotIn("deleteActionsCache", audit)

    def test_candidate_and_protected_graphs_share_checks_without_permission_overrequests(
        self,
    ):
        for filename in ("ci-build.yml", "rust-migration.yml"):
            text = (ROOT / ".github/workflows" / filename).read_text()
            self.assertNotIn("actions: write", text)
        for filename in (
            "ci-native-build.yml",
            "ci-linux.yml",
            "ci-macos.yml",
            "ci-windows.yml",
        ):
            text = (ROOT / ".github/workflows" / filename).read_text()
            # GitHub rejects an explicit callee write request beyond a caller's
            # read ceiling. These shared workers inherit their explicit caller.
            self.assertNotRegex(text, r"(?m)^\s*cache-mode:")
            self.assertNotIn("actions: write", text)
        for filename in ("ci-protected-native.yml", "ci-protected-platform.yml"):
            text = (ROOT / ".github/workflows" / filename).read_text()
            self.assertIn("branches: [rust-migration]", text)
            self.assertNotIn("workflow_call:", text)
            self.assertNotIn("workflow_dispatch:", text)
            self.assertNotIn("checkout-ref: ${{ inputs.", text)
            self.assertIn("actions: write", text)
            self.assertIn("cache-mode: write", text)
            self.assertNotIn("download-artifact", text)
        worker = (ROOT / ".github/workflows/ci-native-validation.yml").read_text()
        self.assertIn("python3 tools/run-comparisons.py", worker)
        self.assertIn("python3 tools/ci_native.py shard", worker)
        self.assertIn("python3 tools/ci_native.py report", worker)
        self.assertIn("shard: ${{ fromJSON(inputs.shards) }}", worker)
        self.assertNotIn("concurrency:", worker)
        native = (ROOT / ".github/workflows/ci-native-build.yml").read_text()
        for command in (
            "migration.py tools",
            "migration.py verify",
            "migration.py build",
            "ci_native.py pack --shards 6",
        ):
            self.assertIn(command, native)
        for filename in ("ci-linux.yml", "ci-macos.yml", "ci-windows.yml"):
            text = (ROOT / ".github/workflows" / filename).read_text()
            self.assertIn("continue-on-error: true", text)
            self.assertIn("Require successful bounded publication", text)
            self.assertIn("ctest", text)
        protected = (ROOT / ".github/workflows/ci-protected-native.yml").read_text()
        self.assertIn("needs.build.outputs.shards != ''", protected)
        self.assertIn('test "$BUILD" = success', protected)
        self.assertIn('test "$CACHE_VALIDATED" = true', protected)
        self.assertIn("cache-validated: ${{ !inputs.protected-writer", native)

    def test_lock_identity_uses_stable_family_not_compatibility_or_runner_image(self):
        for filename in ("ci-linux.yml", "ci-macos.yml"):
            text = (ROOT / ".github/workflows" / filename).read_text()
            group = re.search(r"      group: (.+)", text)[1].split(" || ", 1)[0]
            self.assertIn("inputs.cache-family", group)
            self.assertNotIn("inputs.extra-cmake-parameters", group)
            self.assertNotIn("runner-image", group)
        for filename in ("ci-build.yml", "ci-protected-platform.yml"):
            text = (ROOT / ".github/workflows" / filename).read_text()
            for family in (
                "linux-sdl2",
                "linux-dedicated",
                "macos-debug",
                "macos-release",
            ):
                self.assertIn(f"cache-family: {family}", text)


class PolicyTests(unittest.TestCase):
    def test_platform_cache_refresh_is_only_a_covered_protected_push(self):
        workflow = (ROOT / ".github/workflows/ci-build.yml").read_text()
        for job in ("linux", "macos", "windows"):
            section = workflow.split(f"  {job}:\n", 1)[1]
            section = re.split(r"\n  [a-z_]+:\n", section, maxsplit=1)[0]
            expression = re.search(r"      cache-only: \${{ (.+) }}", section)[1]
            expression = expression.replace(
                "inputs.checkout-ref", 'inputs["checkout-ref"]'
            )
            for checkout, event, ref, covered, expected in (
                ("", "push", "refs/heads/rust-migration", "true", True),
                ("merge", "push", "refs/heads/rust-migration", "true", False),
                ("", "pull_request", "refs/heads/rust-migration", "true", False),
                ("", "push", "refs/heads/master", "true", False),
                ("", "push", "refs/heads/rust-migration", "false", False),
            ):
                self.assertEqual(
                    evaluate(
                        expression,
                        inputs={"checkout-ref": checkout},
                        github={"event_name": event, "ref": ref},
                        needs={"policy": {"outputs": {"covered": covered}}},
                    ),
                    expected,
                )
        excluded = {
            "ci-linux.yml": ["Test"],
            "ci-macos.yml": [
                "Rust checks",
                "Test",
                "Compare accepted wide math templates with pinned C++",
                "Reject unsupported Rust configurations",
                "Fresh Rust native generators",
                "Verify native archive configuration freshness",
                "Retain native Rust evidence",
            ],
            "ci-windows.yml": [
                "Rust checks for actual selected target",
                "Test",
                "Fresh Rust native generators",
                "Validate refusals and actual archive freshness",
                "Record native CRT ABI linkage and fresh outputs",
                "Retain native Windows Rust and test evidence",
            ],
        }
        for filename, labels in excluded.items():
            child = (ROOT / ".github/workflows" / filename).read_text()
            for label in labels:
                step = child.split(f"    - name: {label}\n", 1)[1].split(
                    "    - name:", 1
                )[0]
                self.assertRegex(step, r"      if: .* !inputs.cache-only")
        self.assertIn(
            "!inputs.cache-only || inputs.compiler != 'clang'",
            (ROOT / ".github/workflows/ci-linux.yml").read_text(),
        )

    def test_heavy_predicates_bypass_skipped_policy_but_respect_cancellation(self):
        for filename, jobs in (
            ("ci-build.yml", ("emscripten", "linux", "macos", "windows")),
            ("rust-migration.yml", ("build", "validation")),
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
                    needs = {
                        "policy": {"outputs": {"heavy": heavy}},
                        "build": {"result": "success"},
                        "protected-build": {
                            "result": "skipped",
                            "outputs": {"shards": ""},
                        },
                    }
                    with self.subTest(filename=filename, job=job, cancelled=cancelled):
                        result = evaluate(
                            expression.replace(
                                "inputs.checkout-ref", 'inputs["checkout-ref"]'
                            ).replace("cancelled()", str(cancelled).lower()),
                            inputs=inputs,
                            needs=needs,
                            github={
                                "event_name": "pull_request",
                                "ref": "refs/pull/170/merge",
                            },
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
                const github = {{paginate: async () => [], rest: {{repos: {{
                  getCommit: async () => ({{data: {{commit: {{tree: {{sha: "tree"}}}}}}}}),
                  compareCommitsWithBasehead: async () => ({{data: {{files: {json.dumps(files)}}}}})
                }}}}}};
                (async () => {{{script}}})().then(() => console.log(JSON.stringify(outputs)));
                """
                result = node(harness)
                self.assertEqual(result["heavy"], expected)

    def test_only_matching_successful_merged_tree_skips_validation(self):
        script = script_for("ci-policy.yml", "policy")
        for full, marker, same_run, tree, merged, expected in (
            ("success", "success", True, TREE, True, "false"),
            ("success", "success", True, "other", True, "true"),
            ("pending", "success", True, TREE, True, "true"),
            ("failure", "success", True, TREE, True, "true"),
            ("success", "failure", True, TREE, True, "true"),
            ("success", "success", False, TREE, True, "true"),
            ("success", "success", True, TREE, False, "true"),
        ):
            statuses = [
                {"context": "Full validation", "state": full, "target_url": "run"},
                {
                    "context": "Validated tree",
                    "state": marker,
                    "target_url": "run" if same_run else "old",
                    "description": tree,
                },
            ]
            pull = dict(PULL, merged_at="date" if merged else None)
            harness = f"""
            process.env.CI_ON_DEMAND = 'true';
            const outputs = {{}};
            const context = {{eventName: 'push', repo: {{}}, sha: 'head', payload: {{before: 'base'}}}};
            const core = {{setOutput: (k, v) => outputs[k] = v, info: () => {{}}}};
            const github = {{paginate: async () => [{json.dumps(pull)}], rest: {{repos: {{
              getCommit: async () => ({{data: {{commit: {{tree: {{sha: {json.dumps(TREE)}}}}}}}}}),
              getCombinedStatusForRef: async () => ({{data: {{statuses: {json.dumps(statuses)}}}}}),
              compareCommitsWithBasehead: async () => ({{data: {{files: [{{filename: 'src/code.cpp'}}]}}}})
            }}}}}};
            (async () => {{{script}}})().then(() => console.log(JSON.stringify(outputs)));
            """
            with self.subTest(
                full=full, marker=marker, same_run=same_run, tree=tree, merged=merged
            ):
                self.assertEqual(node(harness)["heavy"], expected)


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
