import math
import os
import struct
import sys
from pathlib import Path

import pytest


TOOLS_DIR = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS_DIR))

from visdrone_dataset import (  # noqa: E402
    ConvertedBox,
    SkipReason,
    audit_dataset,
    convert_box,
    convert_dataset,
    parse_annotation,
)


def write_fake_jpeg(path: Path, width: int = 100, height: int = 50) -> None:
    # The converter only needs a valid JPEG SOF segment to read dimensions.
    sof = b"\xff\xc0" + struct.pack(">H", 17) + bytes([8])
    sof += struct.pack(">HH", height, width) + bytes([3])
    sof += bytes([1, 0x11, 0, 2, 0x11, 0, 3, 0x11, 0])
    path.write_bytes(b"\xff\xd8" + sof + b"\xff\xd9")


@pytest.mark.parametrize(
    ("category", "expected_class"),
    [(1, 0), (2, 0), (3, 1), (4, 2), (5, 3), (6, 4),
     (7, 5), (8, 6), (9, 7), (10, 8)],
)
def test_visdrone_categories_map_to_nine_classes(category, expected_class):
    annotation = parse_annotation(f"10,5,20,10,1,{category},1,2")

    result = convert_box(annotation, image_width=100, image_height=50)

    assert isinstance(result, ConvertedBox)
    assert result.class_id == expected_class
    assert result.truncation == 1
    assert result.occlusion == 2


@pytest.mark.parametrize(
    ("line", "reason"),
    [
        ("10,5,20,10,1,0,0,0", "ignored_category"),
        ("10,5,20,10,1,11,0,0", "ignored_category"),
        ("10,5,20,10,0,4,0,0", "non_positive_score"),
        ("10,5,0,10,1,4,0,0", "non_positive_extent"),
        ("10,5,-2,10,1,4,0,0", "non_positive_extent"),
        ("200,5,20,10,1,4,0,0", "outside_image"),
    ],
)
def test_invalid_or_excluded_annotations_are_skipped_with_exact_reason(line, reason):
    result = convert_box(parse_annotation(line), image_width=100, image_height=50)

    assert result == SkipReason(reason)


def test_out_of_bounds_box_is_clipped_and_normalized():
    result = convert_box(
        parse_annotation("-10,-5,30,20,1,4,0,0"),
        image_width=100,
        image_height=50,
    )

    assert isinstance(result, ConvertedBox)
    assert result.class_id == 2
    assert result.clipped is True
    assert result.center_x == pytest.approx(0.1)
    assert result.center_y == pytest.approx(0.15)
    assert result.width == pytest.approx(0.2)
    assert result.height == pytest.approx(0.3)


@pytest.mark.parametrize("value", ["nan", "inf", "-inf"])
def test_non_finite_annotation_values_are_rejected(value):
    with pytest.raises(ValueError, match="finite"):
        parse_annotation(f"{value},5,20,10,1,4,0,0")


def create_source_dataset(root: Path) -> None:
    for split in ("train", "val"):
        split_root = root / f"VisDrone2019-DET-{split}"
        images = split_root / "images"
        annotations = split_root / "annotations"
        images.mkdir(parents=True)
        annotations.mkdir()
        write_fake_jpeg(images / f"{split}-sample.jpg")
        (annotations / f"{split}-sample.txt").write_text(
            "10,5,20,10,1,1,0,2\n"
            "20,5,20,10,1,2,1,1\n"
            "10,5,20,10,1,0,0,0\n"
            "10,5,20,10,0,4,0,0\n",
            encoding="utf-8",
        )


def test_conversion_creates_relative_image_links_labels_and_auditable_counts(tmp_path):
    source = tmp_path / "source"
    output = tmp_path / "output"
    create_source_dataset(source)

    report = convert_dataset(source, output, image_mode="symlink")

    assert report["status"] == "pass"
    assert report["splits"]["train"]["images"] == 1
    assert report["splits"]["val"]["images"] == 1
    assert report["totals"]["converted"] == 4
    assert report["totals"]["skipped"] == {
        "ignored_category": 2,
        "non_positive_score": 2,
    }
    train_image = output / "images" / "train" / "train-sample.jpg"
    assert train_image.is_symlink()
    assert not os.path.isabs(os.readlink(train_image))
    assert (output / "labels" / "train" / "train-sample.txt").read_text(
        encoding="utf-8"
    ).splitlines() == [
        "0 0.20000000 0.20000000 0.20000000 0.20000000",
        "0 0.30000000 0.20000000 0.20000000 0.20000000",
    ]

    audit = audit_dataset(
        output,
        expected_names=(
            "person",
            "bicycle",
            "car",
            "van",
            "truck",
            "tricycle",
            "awning-tricycle",
            "bus",
            "motor",
        ),
    )
    assert audit["status"] == "pass"
    assert audit["totals"] == {"images": 2, "labels": 2, "instances": 4}
    assert audit["class_counts"] == {"0": 4}


@pytest.mark.parametrize(
    ("damage", "expected_error"),
    [
        ("broken_image", "broken_image_link"),
        ("missing_label", "missing_label"),
        ("class_out_of_range", "class_out_of_range"),
        ("coordinate_out_of_range", "coordinate_out_of_range"),
        ("non_finite_coordinate", "non_finite_coordinate"),
    ],
)
def test_independent_audit_rejects_broken_or_invalid_generated_dataset(
    tmp_path, damage, expected_error
):
    dataset = tmp_path / "dataset"
    image_dir = dataset / "images" / "train"
    label_dir = dataset / "labels" / "train"
    image_dir.mkdir(parents=True)
    label_dir.mkdir(parents=True)
    source_image = tmp_path / "source.jpg"
    write_fake_jpeg(source_image)
    image = image_dir / "sample.jpg"
    image.symlink_to(os.path.relpath(source_image, image.parent))
    label = label_dir / "sample.txt"
    label.write_text("0 0.5 0.5 0.2 0.2\n", encoding="utf-8")

    if damage == "broken_image":
        source_image.unlink()
    elif damage == "missing_label":
        label.unlink()
    elif damage == "class_out_of_range":
        label.write_text("9 0.5 0.5 0.2 0.2\n", encoding="utf-8")
    elif damage == "coordinate_out_of_range":
        label.write_text("0 1.1 0.5 0.2 0.2\n", encoding="utf-8")
    else:
        label.write_text(f"0 {math.nan} 0.5 0.2 0.2\n", encoding="utf-8")

    report = audit_dataset(
        dataset,
        expected_names=("person", "bicycle", "car", "van", "truck",
                        "tricycle", "awning-tricycle", "bus", "motor"),
    )

    assert report["status"] == "fail"
    assert report["errors"][0]["reason"] == expected_error
    assert report["errors"][0]["path"].endswith("sample.jpg" if damage == "broken_image" else "sample.txt")
