import hashlib
import json
import tempfile
import unittest
from copy import deepcopy
from pathlib import Path

try:
    from scripts import check_occ_tracking_robustness as robustness
except ImportError as error:
    robustness = None
    IMPORT_ERROR = error
else:
    IMPORT_ERROR = None


IDENTITY = {
    "source_commit": "a" * 40,
    "model_sha256": "b" * 64,
}


def report(mean_iou=0.80, config_hash="c" * 64):
    return {
        "schema_version": 1,
        "scope": "development",
        "frame_range": [0, 189],
        "verdict": "PASS",
        "gates": {
            name: {"passed": True, "detail": "ok", "frames": []}
            for name in (
                "tracking_schema", "visible_boxes", "full_occlusion_semantics",
                "prediction_continuity", "reacquisition", "continuous_control",
                "identity_proxy",
            )
        },
        "strata": {"visible": {"mean_iou": mean_iou}},
        "reacquisition": {"first_frame": 185, "deadline_exclusive": 190},
        "provenance": {
            **IDENTITY,
            "config_sha256": config_hash,
        },
    }


def complete_manifest(root):
    root = Path(root)
    reports = []

    def add(label, category, variant, parameter=None, mean_iou=0.80):
        config_hash = (format(len(reports) + 1, "x") * 64)[:64]
        path = root / (label + ".json")
        path.write_text(json.dumps(report(mean_iou, config_hash)), encoding="utf-8")
        item = {
            "label": label,
            "category": category,
            "variant": variant,
            "path": str(path),
            "config_sha256": config_hash,
        }
        if parameter:
            item["parameter"] = parameter
        reports.append(item)

    add("nominal", "nominal", "nominal")
    for parameter in ("motion_search_confidence_ms", "motion_max_speed_diagonals_per_s"):
        add(parameter + "-minus10", "real", "-10%", parameter)
        add(parameter + "-plus10", "real", "+10%", parameter)
    for parameter in ("confirmation_window", "recent_probation_frames"):
        add(parameter + "-minus1", "count", "-1", parameter)
        add(parameter + "-plus1", "count", "+1", parameter)
    for variant in ("center_x_minus2", "center_x_plus2", "center_y_minus2",
                    "center_y_plus2", "scale_minus5", "scale_plus5"):
        add("init-" + variant, "initialization", variant)
    fault_evidence = []
    for variant in ("variable_timestamps", "detector_delay", "detector_drop",
                    "generation_gap"):
        path = root / ("fault-" + variant + ".json")
        path.write_text(json.dumps({
            "schema_version": 1,
            "variant": variant,
            "passed": True,
            "evidence": "synthetic state matrix",
        }), encoding="utf-8")
        fault_evidence.append({
            "variant": variant,
            "path": str(path),
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        })
    return {
        "schema_version": 1,
        "candidate": {
            "id": "C001",
            **IDENTITY,
            "nominal_config_sha256": reports[0]["config_sha256"],
        },
        "real_parameters": [
            "motion_search_confidence_ms", "motion_max_speed_diagonals_per_s"
        ],
        "count_parameters": ["confirmation_window", "recent_probation_frames"],
        "initialization_variants": [
            "center_x_minus2", "center_x_plus2", "center_y_minus2",
            "center_y_plus2", "scale_minus5", "scale_plus5",
        ],
        "fault_variants": [
            "variable_timestamps", "detector_delay", "detector_drop", "generation_gap"
        ],
        "fault_evidence": fault_evidence,
        "safety_audit": {
            "confirmation_required": 2,
            "detector_direct_control": False,
            "generation_stale_isolation": True,
            "fail_closed_faults": True,
            "evidence": "synthetic C++ state matrix",
        },
        "reports": reports,
    }


class RobustnessManifestTest(unittest.TestCase):
    def setUp(self):
        if IMPORT_ERROR:
            self.fail("robustness checker missing: {}".format(IMPORT_ERROR))

    def check(self, manifest):
        return robustness.evaluate_manifest(manifest)

    def test_complete_plateau_passes(self):
        with tempfile.TemporaryDirectory() as directory:
            result = self.check(complete_manifest(directory))
        self.assertEqual(result["verdict"], "PASS")
        self.assertLessEqual(result["max_visible_mean_iou_drop"], 0.03)

    def test_missing_neighbor_and_nominal_only_pass_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            manifest = complete_manifest(directory)
            manifest["reports"] = [
                item for item in manifest["reports"]
                if item["label"] != "motion_search_confidence_ms-plus10"
            ]
            with self.assertRaisesRegex(ValueError, "missing.*\+10%"):
                self.check(manifest)
            manifest = complete_manifest(directory)
            manifest["reports"] = manifest["reports"][:1]
            with self.assertRaisesRegex(ValueError, "missing"):
                self.check(manifest)

    def test_fault_evidence_requires_complete_hashed_passing_variants(self):
        with tempfile.TemporaryDirectory() as directory:
            manifest = complete_manifest(directory)
            manifest["fault_evidence"].pop()
            with self.assertRaisesRegex(ValueError, "missing fault"):
                self.check(manifest)
        with tempfile.TemporaryDirectory() as directory:
            manifest = complete_manifest(directory)
            manifest["fault_evidence"][0]["sha256"] = "0" * 64
            with self.assertRaisesRegex(ValueError, "SHA-256"):
                self.check(manifest)
        with tempfile.TemporaryDirectory() as directory:
            manifest = complete_manifest(directory)
            item = manifest["fault_evidence"][0]
            payload = json.loads(Path(item["path"]).read_text())
            payload["passed"] = False
            Path(item["path"]).write_text(json.dumps(payload))
            item["sha256"] = hashlib.sha256(Path(item["path"]).read_bytes()).hexdigest()
            with self.assertRaisesRegex(ValueError, "not passing"):
                self.check(manifest)

    def test_scope_holdout_duplicate_and_identity_mismatches_are_rejected(self):
        mutations = []
        with tempfile.TemporaryDirectory() as directory:
            base = complete_manifest(directory)
            scoped = json.loads(Path(base["reports"][1]["path"]).read_text())
            scoped["scope"] = "full"
            Path(base["reports"][1]["path"]).write_text(json.dumps(scoped))
            with self.assertRaisesRegex(ValueError, "development"):
                self.check(base)

        with tempfile.TemporaryDirectory() as directory:
            base = complete_manifest(directory)
            base["reports"][1]["path"] = base["reports"][0]["path"]
            with self.assertRaisesRegex(ValueError, "duplicate"):
                self.check(base)

        for field, bad in (("source_commit", "d" * 40),
                           ("config_sha256", "e" * 64),
                           ("model_sha256", "f" * 64)):
            with tempfile.TemporaryDirectory() as directory:
                base = complete_manifest(directory)
                item = base["reports"][1]
                payload = json.loads(Path(item["path"]).read_text())
                if field == "config_sha256":
                    payload["provenance"][field] = bad
                else:
                    payload["provenance"][field] = bad
                Path(item["path"]).write_text(json.dumps(payload))
                with self.assertRaisesRegex(ValueError, field):
                    self.check(base)

    def test_safety_invariant_and_knife_edge_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            manifest = complete_manifest(directory)
            manifest["safety_audit"]["confirmation_required"] = 1
            with self.assertRaisesRegex(ValueError, "confirmation_required"):
                self.check(manifest)

        with tempfile.TemporaryDirectory() as directory:
            manifest = complete_manifest(directory)
            item = manifest["reports"][1]
            payload = json.loads(Path(item["path"]).read_text())
            payload["verdict"] = "FAIL"
            Path(item["path"]).write_text(json.dumps(payload))
            with self.assertRaisesRegex(ValueError, "plateau"):
                self.check(manifest)

        with tempfile.TemporaryDirectory() as directory:
            manifest = complete_manifest(directory)
            item = manifest["reports"][1]
            payload = json.loads(Path(item["path"]).read_text())
            payload["strata"]["visible"]["mean_iou"] = 0.76
            Path(item["path"]).write_text(json.dumps(payload))
            with self.assertRaisesRegex(ValueError, "IoU drop"):
                self.check(manifest)


if __name__ == "__main__":
    unittest.main()
