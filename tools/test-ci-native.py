#!/usr/bin/env python3
"""Exercise complete shard coverage, build identity, and frozen adapter inputs."""

import copy
import json
import shutil
import subprocess
import tarfile
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import ci_native
from simulation import runtime


class ShardTests(unittest.TestCase):
    def fixture(self, folder):
        manifest = folder / runtime.MANIFEST
        manifest.write_text(
            json.dumps(
                {
                    "roles": {r: {"binary": r} for r in ("reference", "candidate")},
                    "files": {"reference": "r", "candidate": "c"},
                }
            )
        )
        value = {
            "schema_version": 1,
            "candidate_commit": "head",
            "inventory": ["a", "b"],
            "runtime_manifest_sha256": runtime.digest(manifest),
            "shards": [{"id": 0, "names": ["a"]}, {"id": 1, "names": ["b"]}],
        }
        reports = [
            {
                "shard": index,
                "passed": True,
                "results": [{"scenario": name, "passed": True}],
                "candidate_commit": "head",
                "runtime_manifest_sha256": value["runtime_manifest_sha256"],
                "exported_binaries": {"reference": "r", "candidate": "c"},
                "binaries": {
                    "reference": {"sha256": "r"},
                    "candidate": {"sha256": "c"},
                },
                "mode": "reference-vs-candidate",
                "benchmark_repetitions": 0,
            }
            for index, name in enumerate(("a", "b"))
        ]
        return value, reports, manifest

    def check(self, mutate=None, mutate_plan=None):
        with (
            tempfile.TemporaryDirectory() as directory,
            patch.object(ci_native.migration, "git", return_value="head"),
            patch.object(
                ci_native.core,
                "scenario_list",
                return_value=[{"name": "a"}, {"name": "b"}],
            ),
        ):
            value, reports, manifest = self.fixture(Path(directory))
            if mutate:
                mutate(reports)
            if mutate_plan:
                mutate_plan(value)
            return ci_native.aggregate(value, reports, manifest)

    def test_complete_inventory_passes(self):
        self.assertEqual(self.check(), {"passed": True, "scenarios": 2, "shards": 2})

    def test_missing_duplicate_empty_failed_and_wrong_build_shards_fail(self):
        changes = [
            lambda r: r.pop(),
            lambda r: r.append(copy.deepcopy(r[0])),
            lambda r: r[1].update(shard=0),
            lambda r: r[1].update(results=[]),
            lambda r: r[1].update(results=[{"scenario": "a", "passed": True}]),
            lambda r: r[1].update(passed=False),
            lambda r: r[1]["results"][0].update(passed=False),
            lambda r: r[1].update(candidate_commit="other"),
            lambda r: r[1].update(runtime_manifest_sha256="other"),
            lambda r: r[1].update(exported_binaries={}),
            lambda r: r[1]["binaries"]["candidate"].update(sha256="other"),
            lambda r: r[1].update(mode="reference-vs-reference"),
            lambda r: r[1].update(benchmark_repetitions=3),
        ]
        for index, change in enumerate(changes):
            with self.subTest(change=index), self.assertRaises(ValueError):
                self.check(mutate=change)

    def test_plan_cannot_omit_scenarios_or_shards(self):
        for change in (
            lambda v: v.update(inventory=["a"]),
            lambda v: v["shards"].pop(),
            lambda v: v["shards"][1].update(names=[]),
            lambda v: v["shards"][1].update(id=7),
            lambda v: v.update(candidate_commit="other"),
        ):
            with self.assertRaises(ValueError):
                self.check(mutate_plan=change)

    def test_real_partition_is_complete_and_keeps_fixture_families_whole(self):
        scenarios = ci_native.core.scenario_list(False)
        weights = json.loads(ci_native.TIMINGS.read_text())["seconds"]
        shards = ci_native.partition(scenarios, weights, 6)
        self.assertEqual(
            sorted(n for s in shards for n in s["names"]),
            sorted(s["name"] for s in scenarios),
        )
        for family in ("effects", "disasters", "aircraft-control"):
            names = {
                s["name"] for s in scenarios if ci_native.fixture_family(s) == family
            }
            self.assertEqual(
                sum(bool(names.intersection(s["names"])) for s in shards), 1
            )
        self.assertTrue(all(s["names"] for s in shards))
        self.assertLess(
            max(s["seconds"] for s in shards) / min(s["seconds"] for s in shards), 1.02
        )
        for count in (0, -1, len(scenarios) + 1):
            with self.assertRaises(ValueError):
                ci_native.partition(scenarios, weights, count)


class FrozenRuntimeTests(unittest.TestCase):
    def test_canonical_export_identity_rejects_stale_binary_config_and_source(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary = root / "game"
            binary.write_bytes(b"binary")
            cache = root / "CMakeCache.txt"
            cache.write_text("configuration")
            checkout = {
                "source_commit": "head",
                "source_status": "",
                "source_digest": "source",
            }
            identity = {
                "schema_version": 1,
                **checkout,
                "source_stable": True,
                "configuration_digest": "config",
                "binary_sha256": runtime.digest(binary),
                "configuration_cache_sha256": runtime.digest(cache),
            }
            (root / runtime.IDENTITY_NAME).write_text(json.dumps(identity))
            self.assertEqual(
                runtime.candidate_identity(root, binary, checkout), identity
            )
            for altered in (
                dict(checkout, source_status="M source"),
                dict(checkout, source_digest="changed"),
                dict(checkout, source_commit="other"),
            ):
                with self.assertRaises(RuntimeError):
                    runtime.candidate_identity(root, binary, altered)
            binary.write_bytes(b"changed")
            with self.assertRaises(RuntimeError):
                runtime.candidate_identity(root, binary, checkout)
            binary.write_bytes(b"binary")
            cache.write_text("changed")
            with self.assertRaises(RuntimeError):
                runtime.candidate_identity(root, binary, checkout)

    @unittest.skipUnless(
        shutil.which("cc") and shutil.which("objcopy"), "native ELF compiler/objcopy"
    )
    def test_object_export_preserves_symbols_relocations_and_comdat(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "probe.cpp"
            source.write_text(
                "extern int other; int local = 7; template<typename T> T add(T x) { return x + other; } int run(void) { return add(local); }\n"
            )
            binary, derived = root / "probe.o", root / "derived.o"
            subprocess.run(
                ["cc", "-g", "-c", str(source), "-o", str(binary)], check=True
            )
            receipt = runtime.strip_debug(binary, derived)
            proof = receipt["content_proof"]["relocatable_link_inputs"]
            self.assertGreater(proof["symbols"], 0)
            self.assertGreater(proof["relocations"], 0)
            self.assertGreater(proof["groups"], 0)
            self.assertEqual(runtime.elf_content(binary), runtime.elf_content(derived))
            # A symbol rename alters relocation resolution while allocated bytes stay exact.
            altered = root / "altered.o"
            subprocess.run(
                [
                    "objcopy",
                    "--redefine-sym",
                    "other=different",
                    str(derived),
                    str(altered),
                ],
                check=True,
            )
            before, after = runtime.elf_content(derived), runtime.elf_content(altered)
            self.assertEqual(before["SHF_ALLOC"], after["SHF_ALLOC"])
            self.assertNotEqual(
                before["relocatable_link_inputs"], after["relocatable_link_inputs"]
            )
            with (
                patch.object(
                    runtime.subprocess,
                    "run",
                    side_effect=lambda *a, **k: shutil.copy2(altered, derived),
                ),
                self.assertRaises(RuntimeError),
            ):
                runtime.strip_debug(binary, derived)

    def test_debug_export_preserves_mapped_content_and_original_output(self):
        self.assertIsNotNone(shutil.which("cc"), "ELF regression requires cc")
        self.assertIsNotNone(shutil.which("objcopy"), "ELF regression requires objcopy")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "probe.c"
            source.write_text("int main(void) { return 3; }\n")
            binary, derived = root / "probe", root / "derived"
            subprocess.run(["cc", "-g", str(source), "-o", str(binary)], check=True)
            original = binary.read_bytes()
            receipt = runtime.strip_debug(binary, derived)
            self.assertEqual(binary.read_bytes(), original)
            self.assertNotEqual(receipt["source_sha256"], receipt["sha256"])
            self.assertEqual(runtime.elf_content(binary), runtime.elf_content(derived))
            self.assertTrue(
                receipt["content_proof"][
                    "PT_LOAD_without_section_table_header_metadata"
                ]
            )
            self.assertEqual(
                receipt["command"],
                ["objcopy", "--strip-debug", str(binary), str(derived)],
            )
            # A changed program is rejected, even if a transform reports success.
            source.write_text("int main(void) { return 8; }\n")
            altered = root / "altered"
            subprocess.run(["cc", "-g", str(source), "-o", str(altered)], check=True)
            with (
                patch.object(
                    runtime.subprocess,
                    "run",
                    side_effect=lambda *a, **k: shutil.copy2(altered, derived),
                ),
                self.assertRaises(RuntimeError),
            ):
                runtime.strip_debug(binary, derived)
            obj, exported = root / "probe.o", root / "exported.o"
            subprocess.run(["cc", "-g", "-c", str(source), "-o", str(obj)], check=True)
            proof = runtime.strip_debug(obj, exported)["content_proof"]
            self.assertTrue(proof["SHF_ALLOC"])
            self.assertEqual(proof["PT_LOAD_without_section_table_header_metadata"], [])

    def test_native_metadata_relocates_without_touching_binary_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            staging, worker = root / "source", root / "worker"
            (staging / ".local").mkdir(parents=True)
            (staging / "build-rust").mkdir()
            (staging / ".local/native-export-root.json").write_text(
                json.dumps({"root": str(staging), "common_root": str(staging.parent)})
            )
            (staging / "build-rust/CMakeCache.txt").write_text(
                f"RUST_TARGET_DIR={staging}/build-rust/cargo\n"
            )
            (staging / "build-rust/input.o").write_bytes(b"binary bytes stay exact")
            archive = root / "support.tar"
            with tarfile.open(archive, "w") as stream:
                stream.add(staging / ".local", arcname=".local")
                stream.add(staging / "build-rust", arcname="build-rust")
            worker.mkdir()
            with patch.object(ci_native, "ROOT", worker):
                ci_native.unpack(archive)
            self.assertEqual(
                (worker / "build-rust/CMakeCache.txt").read_text(),
                f"RUST_TARGET_DIR={worker}/build-rust/cargo\n",
            )
            self.assertEqual(
                (worker / "build-rust/input.o").read_bytes(), b"binary bytes stay exact"
            )
            receipt = json.loads((worker / ".local/native-relocation.json").read_text())
            self.assertNotEqual(receipt[0]["before"], receipt[0]["after"])

    def test_prepared_adapter_checks_both_binaries_without_relinking(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            game = root / "openttd"
            game.write_bytes(b"ordinary")
            (root / "fleet").mkdir()
            binary = root / "fleet/openttd-fleet"
            binary.write_bytes(b"adapter")
            receipt = {
                "binary_sha256": runtime.digest(binary),
                "ordinary_binary_sha256": runtime.digest(game),
                "compile": ["unchanged compile"],
                "link": ["unchanged link"],
            }
            (root / "adapters.json").write_text(json.dumps({"fleet": receipt}))

            def prepare():
                return runtime.prepared_adapter(
                    "fleet",
                    "reference",
                    {"reference": game},
                    {"reference": root},
                    root / "output",
                )

            copied, evidence = prepare()
            self.assertEqual(copied.read_bytes(), b"adapter")
            self.assertEqual(evidence, receipt)
            binary.write_bytes(b"changed")
            with self.assertRaises(RuntimeError):
                prepare()
            binary.write_bytes(b"adapter")
            game.write_bytes(b"changed")
            with self.assertRaises(RuntimeError):
                prepare()

    def test_manifest_rejects_missing_mutated_and_extra_runtime_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            candidate = root / "candidate-runtime"
            candidate.mkdir()
            (candidate / "game").write_bytes(b"bytes")
            checkout = {
                "source_commit": "head",
                "source_status": "",
                "source_digest": "source",
            }
            identity = {**checkout, "source_stable": True, "binary_sha256": "original"}
            (candidate / runtime.IDENTITY_NAME).write_text(json.dumps(identity))
            (candidate / "adapters.json").write_text("{}")
            manifest = {
                "schema_version": 1,
                "baseline": ci_native.migration.BASELINE,
                "candidate_commit": "head",
                "candidate_build_identity": identity,
                "roles": {
                    "candidate": {
                        "binary": "candidate-runtime/game",
                        "ordinary_derivation": {
                            "source_sha256": "original",
                            "sha256": runtime.digest(candidate / "game"),
                            "content_proof": "proof",
                        },
                        "adapter_derivations": {},
                    }
                },
                "files": {
                    str(p.relative_to(root)): runtime.digest(p)
                    for p in candidate.iterdir()
                },
            }
            (root / runtime.MANIFEST).write_text(json.dumps(manifest))
            with (
                patch.object(ci_native.migration, "git", return_value="head"),
                patch.object(runtime, "source_identity", return_value=checkout),
                patch.object(runtime, "elf_content", return_value="proof"),
            ):
                self.assertEqual(runtime.verify(root, ci_native.migration), manifest)
                (root / "shard-plan.json").write_text("{}")
                runtime.verify(root, ci_native.migration)
                (root / "extra").write_text("unexpected")
                with self.assertRaises(RuntimeError):
                    runtime.verify(root, ci_native.migration)
                (root / "extra").unlink()
                manifest["roles"]["candidate"]["ordinary_derivation"][
                    "source_sha256"
                ] = "unrelated"
                (root / runtime.MANIFEST).write_text(json.dumps(manifest))
                with self.assertRaises(RuntimeError):
                    runtime.verify(root, ci_native.migration)
                manifest["roles"]["candidate"]["ordinary_derivation"][
                    "source_sha256"
                ] = "original"
                (root / runtime.MANIFEST).write_text(json.dumps(manifest))
                (candidate / "game").write_text("changed")
                with self.assertRaises(RuntimeError):
                    runtime.verify(root, ci_native.migration)
                (candidate / "game").unlink()
                with self.assertRaises(RuntimeError):
                    runtime.verify(root, ci_native.migration)
            with (
                patch.object(ci_native.migration, "git", return_value="other"),
                self.assertRaises(RuntimeError),
            ):
                runtime.verify(root, ci_native.migration)


if __name__ == "__main__":
    unittest.main()
