import csv
import hashlib
import json
import tempfile
import unittest
from dataclasses import replace
from pathlib import Path

from rk3566.nanotrack_v3.tools import build_occ_car002_draft as draft


class DraftFixture:
    def __init__(self, root: Path, frame_count: int = 5):
        self.root = root
        self.frame_count = frame_count
        self.video = root / "video.mp4"
        self.video.write_bytes(b"fixture-video")
        self.video_sha256 = hashlib.sha256(self.video.read_bytes()).hexdigest()
        self.candidates = root / "candidates.csv"
        self.policy = root / "review-policy.csv"
        self.events = root / "events.json"
        self.distractors = root / "distractors.json"
        self.review_dir = root / "review"
        self.review_dir.mkdir()
        (self.review_dir / "contact-sheet-000.jpg").write_bytes(b"review-sheet")
        self.output = root / "draft"
        self._write_candidates()
        self._write_policy()
        self.events.write_text(
            json.dumps({"ground_truth_schema_version": "1.0-draft", "events": []}),
            encoding="utf-8",
        )
        self.distractors.write_text(
            json.dumps(
                {
                    "ground_truth_schema_version": "1.0-draft",
                    "distractors": [
                        {
                            "distractor_id": "car_002_distractor_001",
                            "category": "car",
                            "frames": [
                                {
                                    "frame_id": min(4, frame_count - 1),
                                    "bbox_xywh": [60, 70, 15, 10],
                                    "same_class": True,
                                    "appearance_similarity": "high",
                                    "overlaps_target": False,
                                    "identity_ambiguous_with_target": True,
                                    "review_status": "needs_human_review",
                                }
                            ],
                        }
                    ],
                }
            ),
            encoding="utf-8",
        )

    def _write_candidates(self):
        fields = [
            "frame_id",
            "candidate_x",
            "candidate_y",
            "candidate_width",
            "candidate_height",
            "status",
        ]
        with self.candidates.open("w", newline="", encoding="utf-8") as handle:
            writer = csv.DictWriter(handle, fieldnames=fields)
            writer.writeheader()
            for frame_id in range(self.frame_count):
                writer.writerow(
                    {
                        "frame_id": frame_id,
                        "candidate_x": 10 + (frame_id % 50),
                        "candidate_y": 20,
                        "candidate_width": 30,
                        "candidate_height": 40,
                        "status": "anchor" if frame_id in {0, self.frame_count - 1}
                        else "bidirectional_candidate",
                    }
                )

    def _write_policy(self):
        fields = [
            "frame_id",
            "target_state",
            "review_status",
            "visibility_ratio",
            "identity_status",
            "annotation_quality",
            "confirmed_x",
            "confirmed_y",
            "confirmed_width",
            "confirmed_height",
            "estimate_x",
            "estimate_y",
            "estimate_width",
            "estimate_height",
            "estimate_method",
            "estimate_confidence",
            "estimate_uncertainty_px",
            "estimate_scale_uncertainty_ratio",
            "estimate_basis_frames",
            "reviewer_note",
        ]
        with self.policy.open("w", newline="", encoding="utf-8") as handle:
            writer = csv.DictWriter(handle, fieldnames=fields)
            writer.writeheader()
            for frame_id in range(self.frame_count):
                row = {
                    "frame_id": frame_id,
                    "target_state": "visible",
                    "review_status": "assistant_visual_candidate",
                    "visibility_ratio": "1.0",
                    "identity_status": "certain",
                    "annotation_quality": "usable_with_caution",
                }
                if frame_id == 0:
                    row.update(review_status="manual_confirmed_visible",
                               annotation_quality="reliable",
                               confirmed_x="10", confirmed_y="20",
                               confirmed_width="30", confirmed_height="40")
                elif frame_id == 2 and self.frame_count >= 5:
                    row.update(
                        target_state="full_occlusion",
                        review_status="estimated_non_observable",
                        visibility_ratio="0.0",
                        annotation_quality="unusable",
                        estimate_x="12",
                        estimate_y="20",
                        estimate_width="30",
                        estimate_height="40",
                        estimate_method="bidirectional_interpolation",
                        estimate_confidence="medium",
                        estimate_uncertainty_px="12",
                        estimate_scale_uncertainty_ratio="0.15",
                        estimate_basis_frames="1;3",
                        reviewer_note="not observable",
                    )
                elif frame_id == 3 and self.frame_count >= 5:
                    row.update(
                        target_state="ambiguous",
                        review_status="needs_human_review",
                        visibility_ratio="0.3",
                        identity_status="ambiguous",
                        annotation_quality="unusable",
                    )
                elif frame_id == 4 and self.frame_count >= 5:
                    row.update(review_status="needs_human_review")
                writer.writerow(row)

    def config(self):
        return draft.BuildConfig(
            video=self.video,
            video_sha256=self.video_sha256,
            width=100,
            height=200,
            fps_num=30,
            fps_den=1,
            frame_count=self.frame_count,
            candidate_csv=self.candidates,
            review_policy_csv=self.policy,
            events_json=self.events,
            distractors_json=self.distractors,
            output_root=self.output,
            episode_id="occ_video_1_car_002_full_object",
            target_id="car_002",
            review_dir=self.review_dir,
        )


class BuildOccCar002DraftTests(unittest.TestCase):
    def test_build_keeps_confirmed_candidate_and_estimate_layers_separate(self):
        with tempfile.TemporaryDirectory() as directory:
            fixture = DraftFixture(Path(directory))
            summary = draft.build_draft(fixture.config())

            self.assertEqual(summary["frame_count"], 5)
            self.assertEqual(
                (fixture.output / "labels/frame_000000.txt").read_text(),
                "0 0.250000 0.200000 0.300000 0.200000\n",
            )
            self.assertEqual(
                (fixture.output / "candidate_labels/frame_000000.txt").read_text(),
                "0 0.250000 0.200000 0.300000 0.200000\n",
            )
            self.assertEqual(
                (fixture.output / "candidate_labels/frame_000001.txt").read_text(),
                "0 0.260000 0.200000 0.300000 0.200000\n",
            )
            self.assertEqual((fixture.output / "labels/frame_000001.txt").read_text(), "")
            self.assertEqual((fixture.output / "labels/frame_000002.txt").read_text(), "")
            self.assertEqual(
                (fixture.output / "candidate_labels/frame_000002.txt").read_text(), ""
            )
            self.assertNotEqual(
                (fixture.output / "estimated_labels/frame_000002.txt").read_text(), ""
            )
            self.assertEqual(
                (fixture.output / "candidate_labels/frame_000003.txt").read_text(), ""
            )
            frames = [json.loads(line) for line in
                      (fixture.output / "frames.jsonl").read_text().splitlines()]
            self.assertIsNone(frames[1]["target_bbox_xywh"])
            self.assertEqual(frames[1]["candidate_bbox_xywh"], [11.0, 20.0, 30.0, 40.0])
            self.assertIsNone(frames[2]["target_bbox_xywh"])
            self.assertIsNone(frames[2]["candidate_bbox_xywh"])
            self.assertEqual(frames[2]["estimated_bbox_xywh"], [12.0, 20.0, 30.0, 40.0])
            self.assertEqual(frames[3]["target_state"], "ambiguous")
            self.assertIsNone(frames[3]["candidate_bbox_xywh"])
            self.assertEqual(
                (fixture.output / "review_contact_sheets/contact-sheet-000.jpg").read_bytes(),
                b"review-sheet",
            )

    def test_build_creates_exactly_271_contiguous_files_per_layer(self):
        with tempfile.TemporaryDirectory() as directory:
            fixture = DraftFixture(Path(directory), frame_count=271)
            draft.build_draft(fixture.config())
            expected = [f"frame_{frame_id:06d}.txt" for frame_id in range(271)]
            for layer in ("labels", "candidate_labels", "estimated_labels"):
                self.assertEqual(sorted(path.name for path in (fixture.output / layer).iterdir()),
                                 expected)

    def test_validate_detects_tampering_and_unconfirmed_draft(self):
        with tempfile.TemporaryDirectory() as directory:
            fixture = DraftFixture(Path(directory))
            draft.build_draft(fixture.config())
            self.assertEqual(
                draft.validate_draft(fixture.output, require_confirmed=False)["frame_count"],
                5,
            )
            with self.assertRaisesRegex(ValueError, "unresolved review rows"):
                draft.validate_draft(fixture.output, require_confirmed=True)
            label = fixture.output / "candidate_labels/frame_000001.txt"
            label.write_text("0 0.1 0.1 0.1 0.1\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "manifest mismatch"):
                draft.validate_draft(fixture.output, require_confirmed=False)

        with tempfile.TemporaryDirectory() as directory:
            fixture = DraftFixture(Path(directory))
            draft.build_draft(fixture.config())
            frames_path = fixture.output / "frames.jsonl"
            frames = [json.loads(line) for line in frames_path.read_text().splitlines()]
            frames[0]["candidate_bbox_xywh"] = [999999, -50, 0, float("nan")]
            frames_path.write_text(
                "".join(json.dumps(row) + "\n" for row in frames), encoding="utf-8"
            )
            draft._write_checksums(fixture.output)
            with self.assertRaisesRegex(ValueError, "candidate bbox"):
                draft.validate_draft(fixture.output, require_confirmed=False)

    def test_failed_replacement_preserves_previous_draft(self):
        with tempfile.TemporaryDirectory() as directory:
            fixture = DraftFixture(Path(directory))
            draft.build_draft(fixture.config())
            before = (fixture.output / "checksums.sha256").read_bytes()
            with fixture.policy.open(encoding="utf-8") as handle:
                rows = list(csv.DictReader(handle))
            rows[1]["target_state"] = "invented_state"
            with fixture.policy.open("w", newline="", encoding="utf-8") as handle:
                writer = csv.DictWriter(handle, fieldnames=rows[0].keys())
                writer.writeheader()
                writer.writerows(rows)
            with self.assertRaisesRegex(ValueError, "target_state"):
                draft.build_draft(replace(fixture.config(), replace_draft=True))
            self.assertEqual((fixture.output / "checksums.sha256").read_bytes(), before)
            draft.validate_draft(fixture.output, require_confirmed=False)

    def test_build_rejects_invalid_enums_and_duplicate_distractor_ids(self):
        with tempfile.TemporaryDirectory() as directory:
            fixture = DraftFixture(Path(directory))
            with fixture.policy.open(encoding="utf-8") as handle:
                rows = list(csv.DictReader(handle))
            rows[1]["target_state"] = "invented_state"
            with fixture.policy.open("w", newline="", encoding="utf-8") as handle:
                writer = csv.DictWriter(handle, fieldnames=rows[0].keys())
                writer.writeheader()
                writer.writerows(rows)
            with self.assertRaisesRegex(ValueError, "target_state"):
                draft.build_draft(fixture.config())

        with tempfile.TemporaryDirectory() as directory:
            fixture = DraftFixture(Path(directory))
            payload = json.loads(fixture.distractors.read_text())
            payload["distractors"].append(dict(payload["distractors"][0]))
            fixture.distractors.write_text(json.dumps(payload), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "duplicate distractor_id"):
                draft.build_draft(fixture.config())


if __name__ == "__main__":
    unittest.main()
