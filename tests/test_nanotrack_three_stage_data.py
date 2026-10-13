import importlib.util
import json
import subprocess
import sys
import unittest
from unittest import mock
import tempfile
from pathlib import Path

import cv2
import numpy as np

from scripts import nanotrack_three_stage_data as data
from scripts.aerial_sot_nanotrack import FrameRecord, Tracklet


BOX = (1.0, 2.0, 3.0, 4.0)


def make_tracklet(source_video, name, category="vehicle", frame_count=3, first_frame=1):
    return Tracklet(
        name=name,
        source_video=source_video,
        track_id=name,
        category=category,
        frames=tuple(
            FrameRecord(number, "{}/{:04d}.jpg".format(source_video, number), BOX)
            for number in range(first_frame, first_frame + frame_count)
        ),
    )


def dataset_with_videos(source, count):
    return {
        source: [
            make_tracklet("v{:03d}".format(index), "{}-{:03d}".format(source, index))
            for index in range(count)
        ]
    }


class DataInterfaceTest(unittest.TestCase):
    def test_data_interface_module_exists(self):
        self.assertIsNotNone(importlib.util.find_spec("scripts.nanotrack_three_stage_data"))

    def test_required_interfaces_exist(self):
        self.assertTrue(callable(getattr(data, "split_source_videos", None)))
        self.assertTrue(callable(getattr(data, "build_evaluation_panel", None)))
        self.assertTrue(callable(getattr(data, "AerialSequenceSpec", None)))
        self.assertTrue(callable(getattr(data, "serialize_evaluation_panel", None)))

    def test_direct_script_help_bootstraps_workspace_imports(self):
        script = Path(__file__).resolve().parents[1] / "scripts/nanotrack_three_stage_data.py"
        with tempfile.TemporaryDirectory() as temp:
            result = subprocess.run(
                [sys.executable, str(script), "--help"],
                cwd=temp,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                universal_newlines=True,
            )
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("usage:", result.stdout)
        self.assertIn("--workers", result.stdout)


class PreparationTest(unittest.TestCase):
    def api(self, name):
        self.assertTrue(callable(getattr(data, name, None)), "missing preparation behavior: " + name)
        return getattr(data, name)

    def test_sample_keeps_temporal_anchors_and_caps_24(self):
        sample = self.api("sample_training_frames")
        numbers = list(range(1, 182, 2)) + [182]
        frames = tuple(FrameRecord(n, str(n), BOX) for n in numbers)
        selected = sample(frames)
        self.assertEqual(len(selected), 24)
        self.assertTrue({1, 61, 121, 181, 182}.issubset({f.frame_number for f in selected}))
        self.assertEqual(selected, sample(tuple(reversed(frames))))
        self.assertEqual(len(frames), 92)

    def test_uniform_fill_covers_late_temporal_region(self):
        sample = self.api("sample_training_frames")
        selected = sample(tuple(FrameRecord(n, str(n), BOX) for n in range(1, 183)))
        self.assertLessEqual(max(b.frame_number - a.frame_number
                                 for a, b in zip(selected, selected[1:])), 15)

    def test_unsampled_evaluation_stream_fills_absent_gt(self):
        complete = self.api("consecutive_evaluation_tracklet")
        source = Tracklet("a", "v", "1", "vehicle", tuple(
            FrameRecord(n, "v/{:04d}.jpg".format(n), BOX) for n in range(1, 306) if n != 150))
        stream = complete(source, {n: "v/{:04d}.jpg".format(n) for n in range(1, 306)})
        self.api("sample_training_frames")(source.frames)
        panel = data.build_evaluation_panel({"UAVDT": [stream]},
            {"UAVDT": {"hidden-test": ["v"]}}, "hidden-test", {})
        self.assertEqual((panel[0].window_start, panel[0].window_end), (3, 302))
        self.assertEqual(panel[0].boxes_xywh[147], None)
        self.assertEqual(len(stream.frames), 305)

    def test_only_mot_and_uavdt_apply_the_default_centered_300_frame_cap(self):
        frames = tuple(FrameRecord(n, "frame-{}".format(n), BOX) for n in range(1, 406))
        sot = Tracklet("sot-long", "sot-video", "1", "unknown", frames)
        mot = Tracklet("mot-long", "mot-video", "1", "person", frames)
        uavdt = Tracklet("uavdt-long", "uavdt-video", "1", "vehicle", frames)
        panel = data.build_evaluation_panel(
            {"VisDrone-SOT": [sot], "VisDrone-MOT": [mot], "UAVDT": [uavdt]},
            {source: {"hidden-test": [video]} for source, video in (
                ("VisDrone-SOT", "sot-video"), ("VisDrone-MOT", "mot-video"), ("UAVDT", "uavdt-video")
            )},
            "hidden-test", {},
        )
        by_name = {sequence.name: sequence for sequence in panel}
        self.assertEqual((len(by_name["sot-long"].image_paths), by_name["sot-long"].window_start,
                          by_name["sot-long"].window_end), (405, 1, 405))
        for name in ("mot-long", "uavdt-long"):
            with self.subTest(name=name):
                self.assertEqual((len(by_name[name].image_paths), by_name[name].window_start,
                                  by_name[name].window_end), (300, 53, 352))

    def test_duplicate_panel_names_are_rejected_before_order_dependent_selection(self):
        duplicates = [
            make_tracklet("video", "same", frame_count=2),
            make_tracklet("video", "same", frame_count=3),
        ]
        with self.assertRaisesRegex(ValueError, "duplicate.*same"):
            data.build_evaluation_panel(
                {"VisDrone-MOT": duplicates},
                {"VisDrone-MOT": {"validation": ["video"]}}, "validation", {},
            )

    def test_category_partition_is_exclusive_and_complete_and_rejects_nontrain(self):
        partition = self.api("category_tracklets")
        values = [make_tracklet("a", "one", "person"), make_tracklet("b", "two", "bike")]
        groups = partition(values, {"a", "b"})
        self.assertEqual({key: [t.name for t in ts] for key, ts in groups.items()},
                         {"person": ["one"], "bike": ["two"]})
        self.assertEqual(sum(len(ts) for ts in groups.values()), len(values))
        with self.assertRaisesRegex(ValueError, "non-train"):
            partition(values, {"a"})

    def test_hard_reasons_and_neighbor_retention(self):
        hard_pool = self.api("hard_tracklet")
        frames = tuple(FrameRecord(n, str(n), (100, 100, 20, 20)) for n in range(1, 6))
        track = Tracklet("a", "v", "1", "vehicle", frames)
        hard, reasons = hard_pool(track, (100, 100), {3: ["occlusion"]})
        self.assertEqual([f.frame_number for f in hard.frames], [2, 3, 4])
        self.assertEqual(reasons, {2: ["neighbor"], 3: ["occlusion"], 4: ["neighbor"]})
        self.assertGreaterEqual(len(hard.frames), 2)
        clean, flags = hard_pool(track, (100, 100))
        self.assertIsNone(clean)
        self.assertEqual(flags, {})

    def test_hard_small_scale_motion_and_distractor_rules(self):
        hard_pool = self.api("hard_tracklet")
        for boxes, dimensions, expected in (
            ([(0, 0, 2, 2), (0, 0, 2, 2)], (100, 100), "small"),
            ([(0, 0, 10, 10), (0, 0, 20, 10)], (100, 100), "scale"),
            ([(0, 0, 10, 10), (20, 0, 10, 10)], (100, 100), "motion")):
            track = Tracklet("a", "v", "1", "vehicle", tuple(
                FrameRecord(i + 1, str(i), box) for i, box in enumerate(boxes)))
            hard, flags = hard_pool(track, dimensions)
            self.assertEqual(len(hard.frames), 2)
            self.assertTrue(all(expected in value for value in flags.values()))
        nearby = self.api("has_nearby_distractor")
        self.assertTrue(nearby((0, 0, 10, 10), "1", 4, [("2", 4, (20, 0, 10, 10))]))
        self.assertFalse(nearby((0, 0, 10, 10), "1", 4, [("2", 5, (0, 0, 10, 10))]))
        self.assertFalse(nearby((0, 0, 10, 10), "1", 4, [("1", 4, (0, 0, 10, 10))]))

    def test_synthetic_preparation_emits_decodable_manifests_and_audit(self):
        prepare = self.api("prepare_source_pools")
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            image = root / "source.jpg"
            self.assertTrue(cv2.imwrite(str(image), np.zeros((100, 100, 3), dtype=np.uint8)))
            tracks = [Tracklet("a", "v", "1", "person", tuple(
                FrameRecord(n, str(image), (0, 0, 5, 5)) for n in (1, 2, 3)))]
            summary = prepare("synthetic", tracks, {"v"}, root / "out", workers=1)
            self.assertEqual(summary["normal"]["person"]["tracklets"], 1)
            self.assertEqual(summary["hard"]["person"]["frames"], 3)
            manifest = json.loads((root / "out/normal/person.json").read_text())
            self.assertEqual(set(manifest), {"a"})
            for key in manifest["a"]["00"]:
                decoded = cv2.imread(str(root / "out/crop511/a" / (key + ".00.x.jpg")))
                self.assertEqual(decoded.shape, (511, 511, 3))
            audit = json.loads((root / "out/audit.json").read_text())
            self.assertEqual(audit["hard_reasons"]["a"]["1"], ["small"])

    def test_raw_mot_metadata_keeps_marked_frames_and_short_track_distractors(self):
        load = self.api("mot_difficulty")
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            annotation = root / "v.txt"
            annotation.write_text("1,1,0,0,10,10,1,4,1,2\n"
                                  "1,2,20,0,10,10,1,4,0,0\n"
                                  "2,1,0,0,10,10,1,4,0,0\n"
                                  "2,2,20,0,10,10,0,4,0,0\n")
            track = Tracklet("a", "v", "1", "vehicle", (
                FrameRecord(1, "one", (0, 0, 10, 10)), FrameRecord(2, "two", (0, 0, 10, 10))))
            flags = load([track], root, "VisDrone-MOT")
            self.assertEqual(flags["a"][1], ["distractor", "occlusion", "truncation"])
            self.assertEqual(flags["a"].get(2, []), [])

    def test_preparation_refuses_empty_required_hard_pool(self):
        prepare = self.api("prepare_source_pools")
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            image = root / "source.jpg"
            cv2.imwrite(str(image), np.zeros((100, 100, 3), dtype=np.uint8))
            track = Tracklet("a", "v", "1", "person", tuple(
                FrameRecord(n, str(image), (0, 0, 20, 20)) for n in (1, 2)))
            with self.assertRaisesRegex(ValueError, "hard subpool is empty"):
                prepare("s", [track], {"v"}, root / "out", workers=1)

    def test_grouped_crop_job_decodes_source_once_and_preserves_crops_and_resume_counts(self):
        grouped = self.api("prepare_crop_group")
        from scripts.uav123_nanotrack import make_search_crop
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "image.jpg"
            cv2.imwrite(str(source), np.arange(100*100*3, dtype=np.uint8).reshape(100, 100, 3))
            boxes = [(1, 2, 20, 20), (20, 30, 15, 25)]
            jobs = [(str(source), box, str(root / (str(i)+'.jpg')), False) for i, box in enumerate(boxes)]
            with mock.patch.object(cv2, 'imread', wraps=cv2.imread) as reader:
                counts = grouped(jobs)
                self.assertEqual(sum(call[0][0] == str(source) for call in reader.call_args_list), 1)
            self.assertEqual(counts, {'written': 2})
            image = cv2.imread(str(source))
            for i, box in enumerate(boxes):
                ok, encoded = cv2.imencode('.jpg', make_search_crop(image, box, out_size=511),
                                           [int(cv2.IMWRITE_JPEG_QUALITY), 95])
                self.assertTrue(ok)
                self.assertEqual((root / (str(i)+'.jpg')).read_bytes(), encoded.tobytes())
            self.assertEqual(grouped(jobs), {'skipped': 2})

    def test_grouped_crop_job_fails_without_publishing_missing_source_outputs(self):
        grouped = self.api("prepare_crop_group")
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            with self.assertRaisesRegex(RuntimeError, 'cannot decode'):
                grouped([(str(root/'missing.jpg'), BOX, str(root/'crop.jpg'), False)])
            self.assertFalse((root/'crop.jpg').exists())

    def test_grouped_crop_job_formats_pathlike_decode_failure(self):
        grouped = self.api("prepare_crop_group")
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            with self.assertRaisesRegex(RuntimeError, "cannot decode .*missing.jpg"):
                grouped([(root / "missing.jpg", BOX, str(root / "crop.jpg"), False)])


class SourceSplitTest(unittest.TestCase):
    def test_current_dataset_video_counts(self):
        tracklets = {}
        for source, count in (("VisDrone-SOT", 86), ("VisDrone-MOT", 56), ("UAVDT", 30)):
            tracklets.update(dataset_with_videos(source, count))

        result = data.split_source_videos(tracklets)

        self.assertEqual(
            {source: tuple(len(result[source][split]) for split in ("train", "validation", "hidden-test"))
             for source in tracklets},
            {"VisDrone-SOT": (70, 8, 8), "VisDrone-MOT": (46, 5, 5), "UAVDT": (24, 3, 3)},
        )

    def test_duplicate_targets_and_segments_stay_in_one_source_split(self):
        values = [
            make_tracklet("v{:02d}".format(index), "v{:02d}-target-1".format(index))
            for index in range(10)
        ]
        values += [
            make_tracklet("v{:02d}".format(index), "v{:02d}-target-2-segment".format(index))
            for index in range(10)
        ]

        result = data.split_source_videos({"VisDrone-MOT": values})["VisDrone-MOT"]

        self.assertEqual(sum(len(group) for group in result.values()), 10)
        self.assertEqual(len(set(result["train"]) & set(result["validation"])), 0)
        self.assertEqual(len(set(result["train"]) & set(result["hidden-test"])), 0)
        self.assertEqual(len(set(result["validation"]) & set(result["hidden-test"])), 0)
        self.assertEqual(set().union(*(set(group) for group in result.values())),
                         {"v{:02d}".format(index) for index in range(10)})

    def test_sorted_ids_then_seeded_shuffle_and_input_order_independence(self):
        forward = dataset_with_videos("VisDrone-SOT", 10)
        reverse = {"VisDrone-SOT": list(reversed(forward["VisDrone-SOT"]))}

        actual = data.split_source_videos(forward)

        self.assertEqual(actual, data.split_source_videos(reverse))
        self.assertEqual(actual["VisDrone-SOT"]["train"],
                         ("v007", "v003", "v001", "v009", "v006", "v005", "v008", "v002"))
        self.assertEqual(actual["VisDrone-SOT"]["validation"], ("v000",))
        self.assertEqual(actual["VisDrone-SOT"]["hidden-test"], ("v004",))

    def test_each_dataset_has_an_independent_seeded_shuffle(self):
        tracklets = {}
        tracklets.update(dataset_with_videos("VisDrone-SOT", 10))
        tracklets.update(dataset_with_videos("VisDrone-MOT", 10))

        result = data.split_source_videos(tracklets)

        self.assertEqual(result["VisDrone-SOT"], result["VisDrone-MOT"])


class EvaluationPanelTest(unittest.TestCase):
    def test_validation_panel_applies_each_category_limit_and_keeps_all_sot_sequences(self):
        tracklets = {
            "VisDrone-SOT": [make_tracklet("sot{:02d}".format(i), "sot{:02d}".format(i), "unknown")
                              for i in range(8)],
            "VisDrone-MOT": [
                make_tracklet("mot", "mot-{}-{:02d}".format(category, i), category)
                for category, count in (("person", 35), ("vehicle", 34), ("bike", 14))
                for i in range(count)
            ],
            "UAVDT": [make_tracklet("uav", "uav-{:02d}".format(i)) for i in range(27)],
        }
        split_map = {
            "VisDrone-SOT": {"validation": tuple("sot{:02d}".format(i) for i in range(8))},
            "VisDrone-MOT": {"validation": ("mot",)},
            "UAVDT": {"validation": ("uav",)},
        }
        limits = {"VisDrone-MOT": {"person": 30, "vehicle": 30, "bike": 10},
                  "UAVDT": {"vehicle": 20}}

        panel = data.build_evaluation_panel(tracklets, split_map, "validation", limits)

        self.assertEqual(len(panel), 98)
        self.assertEqual(sum(spec.source == "VisDrone-SOT" for spec in panel), 8)
        self.assertEqual(
            {(source, category): sum(spec.source == source and spec.category == category for spec in panel)
             for source, category in (("VisDrone-MOT", "person"), ("VisDrone-MOT", "vehicle"),
                                      ("VisDrone-MOT", "bike"), ("UAVDT", "vehicle"))},
            {("VisDrone-MOT", "person"): 30, ("VisDrone-MOT", "vehicle"): 30,
             ("VisDrone-MOT", "bike"): 10, ("UAVDT", "vehicle"): 20},
        )
        self.assertEqual([spec.name for spec in panel if spec.source == "VisDrone-MOT" and spec.category == "bike"],
                         ["mot-bike-{:02d}".format(i) for i in range(10)])
        reversed_tracklets = {source: list(reversed(values)) for source, values in tracklets.items()}
        self.assertEqual(panel, data.build_evaluation_panel(reversed_tracklets, split_map, "validation", limits))

    def test_panel_uses_only_requested_split(self):
        tracklets = {"VisDrone-SOT": [make_tracklet("train", "train"),
                                      make_tracklet("val", "val"),
                                      make_tracklet("hidden", "hidden")]}
        split_map = {"VisDrone-SOT": {"train": ("train",), "validation": ("val",),
                                       "hidden-test": ("hidden",)}}

        validation = data.build_evaluation_panel(tracklets, split_map, "validation", {})
        hidden = data.build_evaluation_panel(tracklets, split_map, "hidden-test", {})

        self.assertEqual([spec.name for spec in validation], ["val"])
        self.assertEqual([spec.name for spec in hidden], ["hidden"])

    def test_visdrone_sot_long_sequence_keeps_full_paths_and_boxes(self):
        tracklet = make_tracklet("video", "long", frame_count=305, first_frame=11)
        panel = data.build_evaluation_panel(
            {"VisDrone-SOT": [tracklet]},
            {"VisDrone-SOT": {"validation": ("video",)}}, "validation", {},
        )

        self.assertEqual(len(panel), 1)
        self.assertEqual(panel[0].window_start, 11)
        self.assertEqual(panel[0].window_end, 315)
        self.assertEqual(len(panel[0].image_paths), 305)
        self.assertEqual(panel[0].image_paths[0], "video/0011.jpg")
        self.assertEqual(panel[0].image_paths[-1], "video/0315.jpg")
        self.assertEqual(panel[0].boxes_xywh[0], BOX)
        self.assertEqual(panel[0].boxes_xywh[-1], BOX)

    def test_short_sequence_keeps_full_annotation_stream_and_optional_boxes(self):
        frames = (FrameRecord(4, "v/0004.jpg", BOX), FrameRecord(5, "v/0005.jpg", None),
                  FrameRecord(6, "v/0006.jpg", BOX))
        tracklet = Tracklet("three", "v", "1", "vehicle", frames)
        panel = data.build_evaluation_panel(
            {"UAVDT": [tracklet]}, {"UAVDT": {"validation": ("v",)}},
            "validation", {"UAVDT": {"vehicle": 20}},
        )

        self.assertEqual(panel[0].image_paths, ("v/0004.jpg", "v/0005.jpg", "v/0006.jpg"))
        self.assertEqual(panel[0].boxes_xywh, (BOX, None, BOX))
        self.assertEqual((panel[0].window_start, panel[0].window_end), (4, 6))

    def test_panel_json_records_members_and_frame_window_stably(self):
        tracklet = make_tracklet("v", "a", frame_count=3, first_frame=20)
        panel = data.build_evaluation_panel(
            {"UAVDT": [tracklet]}, {"UAVDT": {"validation": ("v",)}},
            "validation", {"UAVDT": {"vehicle": 20}},
        )

        text = data.serialize_evaluation_panel(panel, "validation")
        document = json.loads(text)

        self.assertEqual(text, data.serialize_evaluation_panel(panel, "validation"))
        self.assertEqual(document["split"], "validation")
        self.assertEqual(document["members"], ["a"])
        self.assertEqual(document["sequences"][0]["source_video"], "v")
        self.assertEqual(document["sequences"][0]["window_start"], 20)
        self.assertEqual(document["sequences"][0]["window_end"], 22)
        self.assertEqual(document["sequences"][0]["image_paths"],
                         ["v/0020.jpg", "v/0021.jpg", "v/0022.jpg"])


if __name__ == "__main__":
    unittest.main()
