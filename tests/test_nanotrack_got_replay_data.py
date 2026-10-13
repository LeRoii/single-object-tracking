import configparser
import json
import random
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

import cv2
import numpy as np


try:
    from scripts.nanotrack_got_replay_data import (
        DEFAULT_HIDDEN_QUOTAS,
        GotFrame,
        GotSequence,
        build_split_inventory,
        discover_train_sequences,
        load_got_sequence,
        prepare_got_training_data,
        select_training_frames,
        stratified_train_hidden,
        training_pool,
    )
except ImportError as error:
    _IMPORT_ERROR = error
else:
    _IMPORT_ERROR = None


class GotReplayDataTest(unittest.TestCase):
    def setUp(self):
        if _IMPORT_ERROR is not None:
            self.fail("GOT replay data API is missing: {}".format(_IMPORT_ERROR))

    @staticmethod
    def _sequence(name, root_class, frame_count=3):
        frames = tuple(
            GotFrame(i, "/images/{}/{:08d}.jpg".format(name, i),
                     (float(i), 10.0, 20.0, 10.0), 0, 8)
            for i in range(1, frame_count + 1)
        )
        return GotSequence(name, Path("/dataset") / name, root_class, frames)

    def test_default_hidden_quotas_are_the_approved_467_sequences(self):
        self.assertEqual(DEFAULT_HIDDEN_QUOTAS, {
            "animal": 169,
            "person": 114,
            "vehicle": 112,
            "object part": 49,
            "passive motion object": 23,
        })
        self.assertEqual(sum(DEFAULT_HIDDEN_QUOTAS.values()), 467)

    def test_direct_cli_help_bootstraps_workspace_imports(self):
        script = Path(__file__).resolve().parents[1] / "scripts/nanotrack_got_replay_data.py"
        completed = subprocess.run(
            [sys.executable, str(script), "--help"], cwd="/tmp",
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertIn("--prepare-training", completed.stdout)

    def test_nested_train_discovery_is_sorted_unique_and_ignores_archives(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for split, name in (("02", "GOT-10k_Train_000003"),
                                ("01", "GOT-10k_Train_000001"),
                                ("01", "not-a-sequence")):
                path = root / "train_data" / ("GOT-10k_Train_split_" + split) / name
                path.mkdir(parents=True)
                if name.startswith("GOT-10k_Train_"):
                    (path / "meta_info.ini").write_text("[METAINFO]\nroot_class: vehicle\n")
            (root / "train_data" / "GOT-10k_Train_split_01.zip").write_bytes(b"zip")
            found = discover_train_sequences(root)
            self.assertEqual([path.name for path in found], [
                "GOT-10k_Train_000001", "GOT-10k_Train_000003"])

    def test_stratified_hidden_split_is_deterministic_disjoint_and_order_independent(self):
        sequences = []
        quotas = {"animal": 3, "person": 2, "vehicle": 2,
                  "object part": 1, "passive motion object": 1}
        for root_class, count in (("animal", 8), ("person", 6), ("vehicle", 7),
                                  ("object part", 4), ("passive motion object", 3)):
            for index in range(count):
                sequences.append(self._sequence("{}-{:02d}".format(root_class, index), root_class))
        first = stratified_train_hidden(sequences, quotas, seed=123456)
        shuffled = list(sequences)
        random.Random(99).shuffle(shuffled)
        second = stratified_train_hidden(shuffled, quotas, seed=123456)
        self.assertEqual(first, second)
        self.assertFalse(set(first["train"]) & set(first["hidden"]))
        self.assertEqual(len(first["hidden"]), sum(quotas.values()))
        by_name = {sequence.name: sequence.root_class for sequence in sequences}
        self.assertEqual(
            {key: sum(by_name[name] == key for name in first["hidden"]) for key in quotas},
            quotas,
        )
        self.assertEqual(set(first["train"]) | set(first["hidden"]), set(by_name))

    def test_load_sequence_filters_absent_uncovered_and_invalid_frames(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "GOT-10k_Train_000001"
            path.mkdir()
            (path / "meta_info.ini").write_text(
                "[METAINFO]\nroot_class: person\nobject_class: pedestrian\n")
            boxes = ["1,2,10,20", "2,3,10,20", "3,4,0,20", "4,5,10,20", "5,6,10,20"]
            (path / "groundtruth.txt").write_text("\n".join(boxes) + "\n")
            (path / "absence.label").write_text("0\n1\n0\n0\n0\n")
            (path / "cover.label").write_text("8\n8\n8\n0\n4\n")
            for index in range(1, 6):
                (path / "{:08d}.jpg".format(index)).write_bytes(b"image")
            sequence = load_got_sequence(path)
            self.assertEqual(sequence.root_class, "person")
            self.assertEqual([frame.number for frame in sequence.frames], [1, 5])
            self.assertEqual(sequence.frames[0].bbox_xywh, (1.0, 2.0, 10.0, 20.0))
            self.assertEqual(sequence.frames[-1].cover, 4)

    def test_sampling_is_deterministic_capped_and_retains_hard_changes(self):
        frames = []
        for index in range(1, 101):
            x = float(index)
            width = 20.0
            cover = 8
            if index == 57:
                x = 500.0
                width = 80.0
                cover = 1
            frames.append(GotFrame(index, "/{:08d}.jpg".format(index),
                                   (x, 10.0, width, 20.0), 0, cover))
        sequence = GotSequence("seq", Path("/seq"), "vehicle", tuple(frames))
        first = select_training_frames(sequence, maximum=48, uniform_count=36, hard_count=12)
        second = select_training_frames(sequence, maximum=48, uniform_count=36, hard_count=12)
        numbers = [frame.number for frame in first]
        self.assertEqual(first, second)
        self.assertLessEqual(len(first), 48)
        self.assertEqual(numbers, sorted(numbers))
        self.assertIn(1, numbers)
        self.assertIn(100, numbers)
        self.assertIn(57, numbers)

    def test_pool_mapping_is_exact(self):
        self.assertEqual(training_pool("person"), "person")
        self.assertEqual(training_pool("vehicle"), "vehicle")
        for root_class in ("animal", "object part", "passive motion object"):
            self.assertEqual(training_pool(root_class), "other")
        with self.assertRaises(ValueError):
            training_pool("unknown-root")

    def test_inventory_writes_auditable_split_without_loading_images(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "got"
            output = Path(directory) / "split.json"
            counts = {"animal": 3, "person": 3, "vehicle": 3,
                      "object part": 2, "passive motion object": 2}
            number = 1
            for root_class, count in counts.items():
                for _ in range(count):
                    path = (root / "train_data" / "GOT-10k_Train_split_01" /
                            "GOT-10k_Train_{:06d}".format(number))
                    path.mkdir(parents=True)
                    (path / "meta_info.ini").write_text(
                        "[METAINFO]\nroot_class: {}\n".format(root_class))
                    number += 1
            quotas = {key: 1 for key in counts}
            result = build_split_inventory(root, output, quotas, seed=123456)
            self.assertEqual(result["sequence_count"], sum(counts.values()))
            self.assertEqual(result["root_class_counts"], counts)
            self.assertEqual(len(result["splits"]["hidden"]), 5)
            self.assertEqual(len(result["splits"]["train"]), 8)
            self.assertFalse(set(result["splits"]["hidden"]) &
                             set(result["splits"]["train"]))
            self.assertEqual(json.loads(output.read_text()), result)

    def test_validated_inventory_excludes_sequences_without_a_trainable_pair(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "got"
            output = Path(directory) / "split.json"
            classes = ("animal", "person", "vehicle", "object part",
                       "passive motion object")
            number = 1
            excluded_name = None
            for root_class in classes:
                for member in range(2):
                    name = "GOT-10k_Train_{:06d}".format(number)
                    path = root / "train_data/GOT-10k_Train_split_01" / name
                    path.mkdir(parents=True)
                    (path / "meta_info.ini").write_text(
                        "[METAINFO]\nroot_class: {}\n".format(root_class))
                    (path / "groundtruth.txt").write_text("1,2,10,20\n2,3,10,20\n")
                    (path / "absence.label").write_text("0\n0\n")
                    invalid = root_class == "vehicle" and member == 0
                    (path / "cover.label").write_text("0\n0\n" if invalid else "8\n8\n")
                    if invalid:
                        excluded_name = name
                    number += 1
            result = build_split_inventory(
                root, output, {key: 0 for key in classes}, seed=123456,
                validate_eligibility=True)
            self.assertEqual(result["raw_sequence_count"], 10)
            self.assertEqual(result["sequence_count"], 9)
            self.assertEqual(result["excluded_sequences"], {
                excluded_name: "fewer_than_two_usable_frames"})
            self.assertNotIn(excluded_name, result["splits"]["train"])
            self.assertNotIn(excluded_name, result["splits"]["hidden"])

    def test_synthetic_preparation_writes_partitioned_decodable_crops_and_resumes(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            root = base / "got"
            output = base / "prepared"
            counts = {"animal": 2, "person": 2, "vehicle": 2,
                      "object part": 2, "passive motion object": 2}
            number = 1
            for root_class, count in counts.items():
                for _ in range(count):
                    path = (root / "train_data" / "GOT-10k_Train_split_01" /
                            "GOT-10k_Train_{:06d}".format(number))
                    path.mkdir(parents=True)
                    (path / "meta_info.ini").write_text(
                        "[METAINFO]\nroot_class: {}\n".format(root_class))
                    (path / "groundtruth.txt").write_text("20,20,30,20\n21,20,30,20\n22,20,30,20\n")
                    (path / "absence.label").write_text("0\n0\n0\n")
                    (path / "cover.label").write_text("8\n8\n8\n")
                    for frame in range(1, 4):
                        image = np.full((96, 128, 3), 40 + number, dtype=np.uint8)
                        self.assertTrue(cv2.imwrite(str(path / "{:08d}.jpg".format(frame)), image))
                    number += 1
            inventory = base / "split.json"
            quotas = {key: 0 for key in counts}
            build_split_inventory(root, inventory, quotas, seed=123456)
            first = prepare_got_training_data(
                inventory, output, workers=1, maximum=2, uniform_count=2, hard_count=0)
            self.assertEqual(first["sequence_count"], 10)
            self.assertEqual(first["pool_sequence_counts"], {
                "other": 6, "person": 2, "vehicle": 2})
            names = set()
            for pool in ("person", "vehicle", "other"):
                manifest = json.loads((output / "got/normal" / (pool + ".json")).read_text())
                self.assertFalse(names & set(manifest))
                names.update(manifest)
                for sequence, tracks in manifest.items():
                    for frame in tracks["00"]:
                        crop = cv2.imread(str(output / "got/crop511" / sequence /
                                               (frame + ".00.x.jpg")))
                        self.assertIsNotNone(crop)
                        self.assertEqual(crop.shape, (511, 511, 3))
            self.assertEqual(len(names), 10)
            self.assertEqual(first["crop_counts"]["written"], 20)
            second = prepare_got_training_data(
                inventory, output, workers=1, maximum=2, uniform_count=2, hard_count=0)
            self.assertEqual(second["crop_counts"]["skipped"], 20)


if __name__ == "__main__":
    unittest.main()
