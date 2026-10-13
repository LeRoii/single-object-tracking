import tempfile
import unittest
from pathlib import Path
import subprocess
import sys

import cv2
import numpy as np

from scripts.aerial_sot_nanotrack import (
    FrameRecord,
    Tracklet,
    prepare_tracklets,
    parse_uavdt_train,
    parse_visdrone_mot_train,
    parse_visdrone_sot_train,
    split_and_subsample,
)


class TrackletNormalizationTest(unittest.TestCase):
    def test_gap_split_rejects_short_segments_and_keeps_endpoints_when_subsampling(self):
        frames = [(number, "image-{}".format(number), (1.0, 2.0, 3.0, 4.0)) for number in range(1, 8)]
        frames += [(number, "image-{}".format(number), (1.0, 2.0, 3.0, 4.0)) for number in range(30, 35)]

        segments = split_and_subsample(frames, min_frames=5, max_gap=3, max_frames=4)

        self.assertEqual([[frame.frame_number for frame in segment] for segment in segments], [[1, 3, 5, 7], [30, 31, 33, 34]])

    def test_visdrone_sot_keeps_both_train_parts_without_name_collision(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            for part in ("part1", "part2"):
                base = root / ("VisDrone2019-SOT-train_" + part) / "VisDrone2019-SOT-train"
                (base / "annotations").mkdir(parents=True)
                sequence = base / "sequences" / "same_sequence"
                sequence.mkdir(parents=True)
                for frame in (1, 2):
                    (sequence / "{:07d}.jpg".format(frame)).write_bytes(b"jpeg")
                (base / "annotations/same_sequence.txt").write_text(
                    "10,20,30,40\n11,21,30,40\n", encoding="utf-8"
                )

            tracklets = parse_visdrone_sot_train(root)

        self.assertEqual(len(tracklets), 2)
        self.assertEqual(len({tracklet.name for tracklet in tracklets}), 2)
        self.assertEqual([len(tracklet.frames) for tracklet in tracklets], [2, 2])
        self.assertEqual(tracklets[0].frames[0].bbox_xywh, (10.0, 20.0, 30.0, 40.0))

    def test_visdrone_mot_filters_ignore_rows_and_splits_long_gaps(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            base = root / "VisDrone2019-MOT-train"
            (base / "annotations").mkdir(parents=True)
            sequence = base / "sequences/video_a"
            sequence.mkdir(parents=True)
            rows = []
            for frame in list(range(1, 5)) + list(range(20, 24)):
                (sequence / "{:07d}.jpg".format(frame)).write_bytes(b"jpeg")
                rows.append("{},7,10,20,30,40,1,4,0,0".format(frame))
            rows.extend(("1,0,1,1,10,10,0,0,0,0", "2,8,1,1,0,10,1,1,0,0"))
            (base / "annotations/video_a.txt").write_text("\n".join(rows) + "\n", encoding="utf-8")

            tracklets = parse_visdrone_mot_train(root, min_frames=4, max_gap=3, max_frames=60)

        self.assertEqual(len(tracklets), 2)
        self.assertEqual([frame.frame_number for frame in tracklets[0].frames], [1, 2, 3, 4])
        self.assertEqual([frame.frame_number for frame in tracklets[1].frames], [20, 21, 22, 23])
        self.assertTrue(all(tracklet.category == "vehicle" for tracklet in tracklets))
        self.assertTrue(all("video_a" in tracklet.name and "id000007" in tracklet.name for tracklet in tracklets))

    def test_uavdt_uses_only_official_train_list_and_preserves_vehicle_class(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            (root / "M_attr/train").mkdir(parents=True)
            (root / "M_attr/test").mkdir(parents=True)
            (root / "M_attr/train/M0101_attr.txt").write_text("1,0,0,0,1,0,1,1,0,0\n", encoding="utf-8")
            (root / "M_attr/test/M9999_attr.txt").write_text("1,0,0,0,1,0,1,1,0,0\n", encoding="utf-8")
            gt = root / "UAV-benchmark-MOTD_v1.0/GT"
            gt.mkdir(parents=True)
            train_images = root / "UAV-benchmark-M/M0101"
            test_images = root / "UAV-benchmark-M/M9999"
            train_images.mkdir(parents=True)
            test_images.mkdir(parents=True)
            train_rows = []
            test_rows = []
            for frame in range(1, 5):
                (train_images / "img{:06d}.jpg".format(frame)).write_bytes(b"jpeg")
                (test_images / "img{:06d}.jpg".format(frame)).write_bytes(b"jpeg")
                train_rows.append("{},3,10,20,30,40,1,1,2".format(frame))
                test_rows.append("{},9,10,20,30,40,1,1,1".format(frame))
            (gt / "M0101_gt_whole.txt").write_text("\n".join(train_rows) + "\n", encoding="utf-8")
            (gt / "M9999_gt_whole.txt").write_text("\n".join(test_rows) + "\n", encoding="utf-8")

            tracklets = parse_uavdt_train(root, min_frames=4, max_gap=3, max_frames=60)

        self.assertEqual(len(tracklets), 1)
        self.assertEqual(tracklets[0].source_video, "M0101")
        self.assertEqual(tracklets[0].category, "vehicle")
        self.assertNotIn("M9999", tracklets[0].name)


class PreparedDatasetTest(unittest.TestCase):
    def test_cli_help_runs_when_invoked_as_script(self):
        workspace = Path(__file__).resolve().parents[1]
        completed = subprocess.run(
            [sys.executable, str(workspace / "scripts/aerial_sot_nanotrack.py"), "--help"],
            cwd=str(workspace),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True,
        )

        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertIn("--visdrone-sot-root", completed.stdout)
        self.assertIn("--mot-max-frames", completed.stdout)

    def test_import_limits_opencv_threads_for_process_parallelism(self):
        workspace = Path(__file__).resolve().parents[1]
        completed = subprocess.run(
            [
                sys.executable,
                "-c",
                "import cv2; import scripts.aerial_sot_nanotrack; print(cv2.getNumThreads())",
            ],
            cwd=str(workspace),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True,
        )

        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(completed.stdout.strip(), "1")

    def test_prepare_tracklets_writes_centered_crop_and_pysot_manifest(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            image_path = root / "source.jpg"
            image = np.zeros((120, 160, 3), dtype=np.uint8)
            image[40:60, 70:90] = 255
            cv2.imwrite(str(image_path), image)
            tracklet = Tracklet(
                name="source__track",
                source_video="source",
                track_id="7",
                category="vehicle",
                frames=(FrameRecord(12, str(image_path), (70.0, 40.0, 20.0, 20.0)),),
            )

            summary = prepare_tracklets("unit", [tracklet], root / "prepared", workers=1)

            crop_path = root / "prepared/crop511/source__track/000012.00.x.jpg"
            annotation_path = root / "prepared/train.json"
            self.assertTrue(crop_path.is_file())
            self.assertEqual(cv2.imread(str(crop_path)).shape, (511, 511, 3))
            annotations = __import__("json").loads(annotation_path.read_text(encoding="utf-8"))
            self.assertEqual(annotations["source__track"]["00"]["000012"], [70.0, 40.0, 90.0, 60.0])
            self.assertEqual(summary["tracklets"], 1)
            self.assertEqual(summary["frames"], 1)
            self.assertEqual(summary["crop_summary"]["failed"], 0)

    def test_prepare_tracklets_rejects_missing_source_before_writing_manifest(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            tracklet = Tracklet(
                name="broken",
                source_video="broken_video",
                track_id="1",
                category="person",
                frames=(FrameRecord(1, str(root / "missing.jpg"), (1.0, 2.0, 3.0, 4.0)),),
            )

            with self.assertRaisesRegex(FileNotFoundError, "missing source images"):
                prepare_tracklets("unit", [tracklet], root / "prepared", workers=1)

            self.assertFalse((root / "prepared/train.json").exists())

    def test_prepare_tracklets_rejects_duplicate_names(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            image_path = root / "source.jpg"
            cv2.imwrite(str(image_path), np.zeros((32, 32, 3), dtype=np.uint8))
            first = Tracklet("duplicate", "a", "1", "vehicle", (FrameRecord(1, str(image_path), (1.0, 1.0, 4.0, 4.0)),))
            second = Tracklet("duplicate", "b", "2", "person", (FrameRecord(2, str(image_path), (2.0, 2.0, 4.0, 4.0)),))

            with self.assertRaisesRegex(ValueError, "duplicate tracklet names"):
                prepare_tracklets("unit", [first, second], root / "prepared", workers=1)


if __name__ == "__main__":
    unittest.main()
