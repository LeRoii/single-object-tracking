import json
import math
import tempfile
import unittest
from pathlib import Path

import cv2
import numpy as np

from scripts.uav123_nanotrack import (
    audit_dataset,
    build_dataset,
    category_for,
    grouped_split,
    make_search_crop,
    parse_config_seqs,
    read_boxes,
    write_split_annotations,
)


class UAV123MetadataTest(unittest.TestCase):
    def test_parses_10fps_config_and_preserves_physical_folder(self):
        config = """function seqs=configSeqs
seqUAV123_10fps = {struct('name','car1_1','path','D:\\data_seq\\UAV123_10fps\\car1\\','startFrame',1,'endFrame',2,'nz',6,'ext','jpg','init_rect',[0,0,0,0]),...
    struct('name','car1_2','path','D:\\data_seq\\UAV123_10fps\\car1\\','startFrame',2,'endFrame',3,'nz',6,'ext','jpg','init_rect',[0,0,0,0])};
"""
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "configSeqs.m"
            path.write_text(config, encoding="utf-8")
            specs = parse_config_seqs(path)

        self.assertEqual([spec.name for spec in specs], ["car1_1", "car1_2"])
        self.assertEqual([spec.folder for spec in specs], ["car1", "car1"])
        self.assertEqual((specs[1].start_frame, specs[1].end_frame), (2, 3))

    def test_reads_nan_as_absent_and_rejects_nonpositive_boxes(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "boxes.txt"
            path.write_text(
                "10,20,30,40\nNaN,NaN,NaN,NaN\n1,2,0,4\n",
                encoding="utf-8",
            )
            boxes = read_boxes(path)

        self.assertEqual(boxes[0], (10.0, 20.0, 30.0, 40.0))
        self.assertIsNone(boxes[1])
        self.assertIsNone(boxes[2])

    def test_grouped_split_never_leaks_source_folder(self):
        config_lines = ["function seqs=configSeqs", "seqUAV123_10fps = {"]
        for index, folder in enumerate(
            ["car1", "car1", "car2", "car3", "person1", "person2", "bird1", "boat1"]
        ):
            name = folder if index not in (0, 1) else "car1_{}".format(index + 1)
            config_lines.append(
                "struct('name','{}','path','D:\\data_seq\\UAV123_10fps\\{}\\','startFrame',1,'endFrame',2,'nz',6,'ext','jpg','init_rect',[0,0,0,0]),...".format(
                    name, folder
                )
            )
        config_lines.append("};")
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "configSeqs.m"
            path.write_text("\n".join(config_lines), encoding="utf-8")
            specs = parse_config_seqs(path)
            split = grouped_split(specs, seed=123456)

        seen = {}
        for split_name, names in split["logical_sequences"].items():
            for name in names:
                folder = next(spec.folder for spec in specs if spec.name == name)
                if folder in seen:
                    self.assertEqual(seen[folder], split_name, msg="{} leaked".format(folder))
                else:
                    seen[folder] = split_name
        self.assertEqual(set(seen), {spec.folder for spec in specs})
        self.assertEqual(category_for("car16_2"), "vehicle")
        self.assertEqual(category_for("person14_3"), "person")


class UAV123CropAndManifestTest(unittest.TestCase):
    def test_search_crop_centers_target_and_has_requested_size(self):
        image = np.zeros((160, 240, 3), dtype=np.uint8)
        image[50:90, 90:130] = 255
        crop = make_search_crop(image, (90.0, 50.0, 40.0, 40.0), out_size=511)

        self.assertEqual(crop.shape, (511, 511, 3))
        gray = cv2.cvtColor(crop, cv2.COLOR_BGR2GRAY)
        ys, xs = np.where(gray > 200)
        self.assertLess(abs(float(xs.mean()) - 255.0), 2.0)
        self.assertLess(abs(float(ys.mean()) - 255.0), 2.0)

    def test_audit_and_annotation_writer_account_for_absent_frames(self):
        config = """function seqs=configSeqs
seqUAV123_10fps = {struct('name','car1','path','D:\\data_seq\\UAV123_10fps\\car1\\','startFrame',1,'endFrame',3,'nz',6,'ext','jpg','init_rect',[0,0,0,0])};
"""
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            (root / "anno/UAV123_10fps").mkdir(parents=True)
            images = root / "data_seq/UAV123_10fps/car1"
            images.mkdir(parents=True)
            (root / "configSeqs.m").write_text(config, encoding="utf-8")
            (root / "anno/UAV123_10fps/car1.txt").write_text(
                "10,10,20,20\nNaN,NaN,NaN,NaN\n12,11,20,20\n",
                encoding="utf-8",
            )
            for frame in range(1, 4):
                cv2.imwrite(str(images / "{:06d}.jpg".format(frame)), np.zeros((64, 64, 3), np.uint8))

            specs = parse_config_seqs(root / "configSeqs.m")
            report = audit_dataset(root, specs)
            out = root / "train.json"
            write_split_annotations(root, specs, ["car1"], out)
            annotations = json.loads(out.read_text(encoding="utf-8"))

        self.assertEqual(report["logical_sequences"], 1)
        self.assertEqual(report["annotations_total"], 3)
        self.assertEqual(report["valid_boxes"], 2)
        self.assertEqual(report["absent_or_invalid_boxes"], 1)
        self.assertEqual(sorted(annotations["car1"]["00"]), ["000001", "000003"])
        self.assertEqual(annotations["car1"]["00"]["000001"], [10.0, 10.0, 30.0, 30.0])

    def test_build_dataset_writes_only_valid_crops_and_reproducibility_files(self):
        config = """function seqs=configSeqs
seqUAV123_10fps = {struct('name','person1','path','D:\\data_seq\\UAV123_10fps\\person1\\','startFrame',1,'endFrame',3,'nz',6,'ext','jpg','init_rect',[0,0,0,0])};
"""
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir) / "source"
            output = Path(temp_dir) / "prepared"
            (root / "anno/UAV123_10fps").mkdir(parents=True)
            images = root / "data_seq/UAV123_10fps/person1"
            images.mkdir(parents=True)
            (root / "configSeqs.m").write_text(config, encoding="utf-8")
            (root / "anno/UAV123_10fps/person1.txt").write_text(
                "10,10,20,20\nNaN,NaN,NaN,NaN\n12,11,20,20\n",
                encoding="utf-8",
            )
            for frame in range(1, 4):
                image = np.full((64, 64, 3), 20 * frame, np.uint8)
                cv2.imwrite(str(images / "{:06d}.jpg".format(frame)), image)

            summary = build_dataset(root, output, workers=1, seed=123456)

            crops = sorted((output / "crop511/person1").glob("*.jpg"))
            self.assertEqual([path.name for path in crops], ["000001.00.x.jpg", "000003.00.x.jpg"])
            self.assertEqual(cv2.imread(str(crops[0])).shape, (511, 511, 3))
            self.assertEqual(summary["crop_summary"]["written"], 2)
            self.assertEqual(summary["crop_summary"]["failed"], 0)
            for relative in (
                "audit_report.json",
                "splits.json",
                "annotations/train.json",
                "annotations/val.json",
                "annotations/test.json",
                "prepare_summary.json",
            ):
                self.assertTrue((output / relative).is_file(), relative)


if __name__ == "__main__":
    unittest.main()
