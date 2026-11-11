#!/usr/bin/env python3
"""Build and validate strictly layered occ_video_1 car_002 draft annotations."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import shutil
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence


SCHEMA_VERSION = "1.0-draft"
TARGET_STATES = {"visible", "partial_occlusion", "full_occlusion", "ambiguous", "out_of_view"}
REVIEW_STATUSES = {
    "manual_confirmed_visible",
    "assistant_visual_candidate",
    "needs_human_review",
    "estimated_non_observable",
}
IDENTITY_STATUSES = {"certain", "ambiguous"}
ANNOTATION_QUALITIES = {"reliable", "usable_with_caution", "unusable"}
OBSERVABLE_STATES = {"visible", "partial_occlusion"}
UNRESOLVED_REVIEW_STATUSES = {"assistant_visual_candidate", "needs_human_review"}
ESTIMATE_METHODS = {
    "forward_motion",
    "bidirectional_interpolation",
    "scene_constrained",
    "manual_judgement",
    "not_estimated",
}


@dataclass(frozen=True)
class BuildConfig:
    video: Path
    video_sha256: str
    width: int
    height: int
    fps_num: int
    fps_den: int
    frame_count: int
    candidate_csv: Path
    review_policy_csv: Path
    events_json: Path
    distractors_json: Path
    output_root: Path
    episode_id: str
    target_id: str
    review_dir: Optional[Path] = None
    replace_draft: bool = False


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _load_csv(path: Path, name: str) -> List[Dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        if reader.fieldnames is None:
            raise ValueError(f"{name} has no header")
        return list(reader)


def _float(text: str, name: str) -> float:
    try:
        value = float(text)
    except (TypeError, ValueError) as error:
        raise ValueError(f"invalid {name}: {text!r}") from error
    if not math.isfinite(value):
        raise ValueError(f"invalid {name}: {text!r}")
    return value


def _bbox_from_row(row: Dict[str, str], prefix: str) -> Optional[List[float]]:
    names = [f"{prefix}_x", f"{prefix}_y", f"{prefix}_width", f"{prefix}_height"]
    values = [row.get(name, "").strip() for name in names]
    if not any(values):
        return None
    if not all(values):
        raise ValueError(f"partial {prefix} bbox at frame {row.get('frame_id')}")
    return [_float(value, name) for value, name in zip(values, names)]


def _validate_bbox(box: Sequence[float], width: int, height: int, name: str) -> List[float]:
    if len(box) != 4:
        raise ValueError(f"{name} bbox needs four values")
    x, y, box_width, box_height = [float(value) for value in box]
    if not all(math.isfinite(value) for value in (x, y, box_width, box_height)):
        raise ValueError(f"{name} bbox is not finite")
    if x < 0 or y < 0 or box_width <= 0 or box_height <= 0:
        raise ValueError(f"{name} bbox is not positive and in-frame")
    if x + box_width > width + 1e-6 or y + box_height > height + 1e-6:
        raise ValueError(f"{name} bbox exceeds frame geometry")
    return [x, y, box_width, box_height]


def _indexed_rows(rows: Iterable[Dict[str, str]], frame_count: int, name: str) -> Dict[int, Dict[str, str]]:
    indexed: Dict[int, Dict[str, str]] = {}
    for row in rows:
        try:
            frame_id = int(row["frame_id"])
        except (KeyError, TypeError, ValueError) as error:
            raise ValueError(f"invalid {name} frame_id") from error
        if frame_id < 0 or frame_id >= frame_count or frame_id in indexed:
            raise ValueError(f"invalid or duplicate {name} frame_id {frame_id}")
        indexed[frame_id] = row
    expected = set(range(frame_count))
    if set(indexed) != expected:
        missing = sorted(expected - set(indexed))
        raise ValueError(f"{name} does not cover every frame; missing={missing[:10]}")
    return indexed


def _load_json(path: Path, name: str) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"invalid {name} JSON") from error
    if not isinstance(value, dict) or value.get("ground_truth_schema_version") != SCHEMA_VERSION:
        raise ValueError(f"invalid {name} schema")
    return value


def _validate_distractors(payload: dict, frame_count: int, width: int, height: int) -> Dict[int, list]:
    by_frame: Dict[int, list] = {frame_id: [] for frame_id in range(frame_count)}
    seen = set()
    for distractor in payload.get("distractors", []):
        distractor_id = distractor.get("distractor_id")
        if not isinstance(distractor_id, str) or not distractor_id:
            raise ValueError("invalid distractor_id")
        if distractor_id in seen:
            raise ValueError(f"duplicate distractor_id: {distractor_id}")
        seen.add(distractor_id)
        category = distractor.get("category")
        if not isinstance(category, str) or not category:
            raise ValueError(f"invalid category for {distractor_id}")
        frame_ids = set()
        for frame in distractor.get("frames", []):
            frame_id = frame.get("frame_id")
            if not isinstance(frame_id, int) or not 0 <= frame_id < frame_count or frame_id in frame_ids:
                raise ValueError(f"invalid distractor frame for {distractor_id}")
            frame_ids.add(frame_id)
            record = dict(frame)
            record["distractor_id"] = distractor_id
            record["category"] = category
            record["bbox_xywh"] = _validate_bbox(
                record.get("bbox_xywh", []), width, height, distractor_id
            )
            for required in (
                "same_class",
                "appearance_similarity",
                "overlaps_target",
                "identity_ambiguous_with_target",
                "review_status",
            ):
                if required not in record:
                    raise ValueError(f"missing distractor field {required}")
            if record["review_status"] not in REVIEW_STATUSES:
                raise ValueError("invalid distractor review_status")
            by_frame[frame_id].append(record)
    return by_frame


def _yolo_line(box: Optional[Sequence[float]], width: int, height: int) -> str:
    if box is None:
        return ""
    x, y, box_width, box_height = box
    return (
        f"0 {(x + box_width / 2.0) / width:.6f} {(y + box_height / 2.0) / height:.6f} "
        f"{box_width / width:.6f} {box_height / height:.6f}\n"
    )


def _write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def _write_manifest(root: Path, layer: str, manifest_name: str) -> None:
    lines = []
    for path in sorted((root / layer).glob("frame_*.txt")):
        lines.append(f"{_sha256(path)}  {path.relative_to(root).as_posix()}\n")
    (root / manifest_name).write_text("".join(lines), encoding="utf-8")


def _write_checksums(root: Path) -> None:
    ignored = {"checksums.sha256"}
    paths = sorted(
        path for path in root.rglob("*")
        if path.is_file() and path.name not in ignored
    )
    lines = [f"{_sha256(path)}  {path.relative_to(root).as_posix()}\n" for path in paths]
    (root / "checksums.sha256").write_text("".join(lines), encoding="utf-8")


def _prepare_output(config: BuildConfig) -> None:
    root = config.output_root
    if root.exists() and any(root.iterdir()):
        if not config.replace_draft:
            raise ValueError("output draft is nonempty; pass --replace-draft")
        metadata_path = root / "annotation_metadata.json"
        try:
            metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            raise ValueError("cannot replace unidentified draft") from error
        if (metadata.get("episode", {}).get("episode_id") != config.episode_id or
                metadata.get("video", {}).get("sha256") != config.video_sha256):
            raise ValueError("cannot replace a different draft")
        for child in root.iterdir():
            if child.is_dir():
                shutil.rmtree(child)
            else:
                child.unlink()
    root.mkdir(parents=True, exist_ok=True)
    for layer in ("labels", "candidate_labels", "estimated_labels"):
        (root / layer).mkdir()


def build_draft(config: BuildConfig) -> dict:
    if config.width <= 0 or config.height <= 0 or config.frame_count <= 0:
        raise ValueError("invalid video geometry or frame_count")
    if config.fps_num <= 0 or config.fps_den <= 0:
        raise ValueError("invalid fps")
    if _sha256(config.video) != config.video_sha256:
        raise ValueError("video sha256 mismatch")
    candidates = _indexed_rows(
        _load_csv(config.candidate_csv, "candidate"), config.frame_count, "candidate"
    )
    policies = _indexed_rows(
        _load_csv(config.review_policy_csv, "review policy"), config.frame_count,
        "review policy",
    )
    events = _load_json(config.events_json, "events")
    distractors = _load_json(config.distractors_json, "distractors")
    distractors_by_frame = _validate_distractors(
        distractors, config.frame_count, config.width, config.height
    )
    _prepare_output(config)
    review_sheet_names: List[str] = []
    if config.review_dir is not None:
        review_sheets = sorted(config.review_dir.glob("contact-sheet-*.jpg"))
        if not review_sheets:
            raise ValueError("review directory has no contact sheets")
        destination = config.output_root / "review_contact_sheets"
        destination.mkdir()
        for sheet in review_sheets:
            if not sheet.is_file():
                raise ValueError(f"invalid review contact sheet: {sheet}")
            shutil.copy2(sheet, destination / sheet.name)
            review_sheet_names.append(sheet.name)

    frame_records = []
    status_rows = []
    review_rows = []
    status_counts = {status: 0 for status in REVIEW_STATUSES}
    for frame_id in range(config.frame_count):
        candidate_row = candidates[frame_id]
        policy = policies[frame_id]
        target_state = policy.get("target_state", "")
        review_status = policy.get("review_status", "")
        identity_status = policy.get("identity_status", "")
        annotation_quality = policy.get("annotation_quality", "")
        if target_state not in TARGET_STATES:
            raise ValueError(f"invalid target_state at frame {frame_id}: {target_state}")
        if review_status not in REVIEW_STATUSES:
            raise ValueError(f"invalid review_status at frame {frame_id}: {review_status}")
        if identity_status not in IDENTITY_STATUSES:
            raise ValueError(f"invalid identity_status at frame {frame_id}")
        if annotation_quality not in ANNOTATION_QUALITIES:
            raise ValueError(f"invalid annotation_quality at frame {frame_id}")
        visibility_ratio = _float(policy.get("visibility_ratio", ""), "visibility_ratio")
        if not 0.0 <= visibility_ratio <= 1.0:
            raise ValueError(f"invalid visibility_ratio at frame {frame_id}")

        candidate_box = _bbox_from_row(candidate_row, "candidate")
        if candidate_box is not None:
            candidate_box = _validate_bbox(
                candidate_box, config.width, config.height, f"candidate frame {frame_id}"
            )
        estimate_box = _bbox_from_row(policy, "estimate")
        if estimate_box is not None:
            estimate_box = _validate_bbox(
                estimate_box, config.width, config.height, f"estimate frame {frame_id}"
            )

        observable = target_state in OBSERVABLE_STATES
        target_box = None
        visible_candidate = None
        if review_status == "manual_confirmed_visible":
            if not observable or candidate_box is None or identity_status != "certain":
                raise ValueError(f"manual confirmation is not observable at frame {frame_id}")
            target_box = candidate_box
        elif review_status in UNRESOLVED_REVIEW_STATUSES and observable:
            visible_candidate = candidate_box
        elif review_status == "estimated_non_observable" and observable:
            raise ValueError(f"observable frame marked estimated at frame {frame_id}")
        if not observable:
            target_box = None
            visible_candidate = None
        if estimate_box is not None:
            method = policy.get("estimate_method", "")
            if method not in ESTIMATE_METHODS - {"not_estimated"}:
                raise ValueError(f"invalid estimate_method at frame {frame_id}")
            if not policy.get("estimate_confidence") or not policy.get("estimate_basis_frames"):
                raise ValueError(f"incomplete estimate provenance at frame {frame_id}")
            _float(policy.get("estimate_uncertainty_px", ""), "estimate_uncertainty_px")
            _float(
                policy.get("estimate_scale_uncertainty_ratio", ""),
                "estimate_scale_uncertainty_ratio",
            )
        elif review_status == "estimated_non_observable":
            raise ValueError(f"estimated_non_observable frame lacks estimate at frame {frame_id}")

        status_counts[review_status] += 1
        record = {
            "ground_truth_schema_version": SCHEMA_VERSION,
            "frame_id": frame_id,
            "timestamp_us": round(frame_id * 1_000_000 * config.fps_den / config.fps_num),
            "target_state": target_state,
            "review_status": review_status,
            "target_bbox_xywh": target_box,
            "candidate_bbox_xywh": visible_candidate,
            "candidate_source_status": candidate_row.get("status"),
            "estimated_bbox_xywh": estimate_box,
            "bbox_valid": target_box is not None,
            "bbox_source": "manual" if target_box is not None else "not_available",
            "visibility_ratio": visibility_ratio,
            "truncation_ratio": 1.0 if target_state == "out_of_view" else 0.0,
            "identity_status": identity_status,
            "annotation_quality": annotation_quality,
            "estimate_method": policy.get("estimate_method") or None,
            "estimate_confidence": policy.get("estimate_confidence") or None,
            "estimate_uncertainty_px": (
                _float(policy["estimate_uncertainty_px"], "estimate_uncertainty_px")
                if policy.get("estimate_uncertainty_px") else None
            ),
            "estimate_scale_uncertainty_ratio": (
                _float(policy["estimate_scale_uncertainty_ratio"],
                       "estimate_scale_uncertainty_ratio")
                if policy.get("estimate_scale_uncertainty_ratio") else None
            ),
            "estimate_basis_frames": (
                [int(value) for value in policy.get("estimate_basis_frames", "").split(";") if value]
            ),
            "estimate_reviewer_note": policy.get("reviewer_note") or None,
            "distractors": distractors_by_frame[frame_id],
        }
        frame_records.append(record)
        status_rows.append(
            {
                "frame_id": frame_id,
                "target_state": target_state,
                "review_status": review_status,
                "identity_status": identity_status,
                "annotation_quality": annotation_quality,
            }
        )
        if review_status in UNRESOLVED_REVIEW_STATUSES:
            review_rows.append(
                {
                    "frame_id": frame_id,
                    "target_state": target_state,
                    "review_status": review_status,
                    "candidate_available": int(visible_candidate is not None),
                    "note": policy.get("reviewer_note", ""),
                }
            )
        filename = f"frame_{frame_id:06d}.txt"
        (config.output_root / "labels" / filename).write_text(
            _yolo_line(target_box, config.width, config.height), encoding="utf-8"
        )
        (config.output_root / "candidate_labels" / filename).write_text(
            _yolo_line(visible_candidate, config.width, config.height), encoding="utf-8"
        )
        (config.output_root / "estimated_labels" / filename).write_text(
            _yolo_line(estimate_box, config.width, config.height), encoding="utf-8"
        )

    metadata = {
        "ground_truth_schema_version": SCHEMA_VERSION,
        "draft": True,
        "formal_evaluation_authorized": False,
        "video": {
            "path": str(config.video),
            "sha256": config.video_sha256,
            "width": config.width,
            "height": config.height,
            "fps_num": config.fps_num,
            "fps_den": config.fps_den,
            "frame_count": config.frame_count,
            "frame_id_origin": 0,
        },
        "episode": {
            "episode_id": config.episode_id,
            "target_id": config.target_id,
            "tracking_target_id": config.target_id,
            "category": "car",
            "class_id": 0,
            "target_scope": "full_object",
            "start_frame": 0,
            "end_frame": config.frame_count - 1,
            "init_frame": 0,
        },
        "review_status_counts": status_counts,
        "unresolved_review_count": len(review_rows),
        "review_contact_sheets": {
            "source": str(config.review_dir) if config.review_dir is not None else None,
            "count": len(review_sheet_names),
            "files": review_sheet_names,
        },
    }
    _write_json(config.output_root / "annotation_metadata.json", metadata)
    _write_json(config.output_root / "events.json", events)
    _write_json(config.output_root / "distractors.json", distractors)
    with (config.output_root / "frames.jsonl").open("w", encoding="utf-8") as handle:
        for record in frame_records:
            handle.write(json.dumps(record, ensure_ascii=False, sort_keys=True) + "\n")
    with (config.output_root / "annotation_status.csv").open(
        "w", newline="", encoding="utf-8"
    ) as handle:
        writer = csv.DictWriter(handle, fieldnames=list(status_rows[0]))
        writer.writeheader()
        writer.writerows(status_rows)
    with (config.output_root / "review_manifest.csv").open(
        "w", newline="", encoding="utf-8"
    ) as handle:
        fields = ["frame_id", "target_state", "review_status", "candidate_available", "note"]
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(review_rows)
    (config.output_root / "README.md").write_text(
        "# occ_video_1 car_002 draft\n\n"
        "This package is a preannotation draft. Only `labels/` contains user-confirmed "
        "observable truth; candidate and estimated layers are not formal ground truth.\n",
        encoding="utf-8",
    )
    _write_manifest(config.output_root, "labels", "labels.sha256")
    _write_manifest(config.output_root, "candidate_labels", "candidate_labels.sha256")
    _write_manifest(config.output_root, "estimated_labels", "estimated_labels.sha256")
    _write_checksums(config.output_root)
    return {
        "frame_count": config.frame_count,
        "review_status_counts": status_counts,
        "unresolved_review_count": len(review_rows),
    }


def _verify_manifest(root: Path, manifest_name: str, expected_layer: str, frame_count: int) -> None:
    manifest = root / manifest_name
    lines = manifest.read_text(encoding="utf-8").splitlines()
    if len(lines) != frame_count:
        raise ValueError(f"manifest mismatch: {manifest_name} entry count")
    expected_names = [f"{expected_layer}/frame_{frame_id:06d}.txt" for frame_id in range(frame_count)]
    actual_names = []
    for line in lines:
        parts = line.split("  ", 1)
        if len(parts) != 2:
            raise ValueError(f"manifest mismatch: {manifest_name} syntax")
        digest, relative = parts
        actual_names.append(relative)
        path = root / relative
        if not path.is_file() or _sha256(path) != digest:
            raise ValueError(f"manifest mismatch: {relative}")
    if actual_names != expected_names:
        raise ValueError(f"manifest mismatch: {manifest_name} frame mapping")


def validate_draft(root: Path, require_confirmed: bool) -> dict:
    metadata = json.loads((root / "annotation_metadata.json").read_text(encoding="utf-8"))
    if metadata.get("ground_truth_schema_version") != SCHEMA_VERSION or not metadata.get("draft"):
        raise ValueError("not a car_002 draft package")
    frame_count = int(metadata["video"]["frame_count"])
    _verify_manifest(root, "labels.sha256", "labels", frame_count)
    _verify_manifest(root, "candidate_labels.sha256", "candidate_labels", frame_count)
    _verify_manifest(root, "estimated_labels.sha256", "estimated_labels", frame_count)
    checksum_lines = (root / "checksums.sha256").read_text(encoding="utf-8").splitlines()
    for line in checksum_lines:
        parts = line.split("  ", 1)
        if len(parts) != 2 or not (root / parts[1]).is_file() or _sha256(root / parts[1]) != parts[0]:
            raise ValueError(f"manifest mismatch: checksums {line}")
    frames = [json.loads(line) for line in (root / "frames.jsonl").read_text(encoding="utf-8").splitlines()]
    if len(frames) != frame_count or [record.get("frame_id") for record in frames] != list(range(frame_count)):
        raise ValueError("frames.jsonl is not continuous")
    unresolved = [record for record in frames if record.get("review_status") in UNRESOLVED_REVIEW_STATUSES]
    review_media = metadata.get("review_contact_sheets", {})
    expected_sheets = review_media.get("files", [])
    if review_media.get("count") != len(expected_sheets) or any(
        not (root / "review_contact_sheets" / name).is_file() for name in expected_sheets
    ):
        raise ValueError("review contact sheet manifest mismatch")
    if require_confirmed and unresolved:
        raise ValueError(f"unresolved review rows: {len(unresolved)}")
    for record in frames:
        frame_id = record["frame_id"]
        if record["target_state"] not in OBSERVABLE_STATES and record["target_bbox_xywh"] is not None:
            raise ValueError(f"non-observable formal bbox at frame {frame_id}")
        filename = f"frame_{frame_id:06d}.txt"
        if record["target_bbox_xywh"] is None and (root / "labels" / filename).read_text():
            raise ValueError(f"formal layer leak at frame {frame_id}")
        if record["candidate_bbox_xywh"] is None and (root / "candidate_labels" / filename).read_text():
            raise ValueError(f"candidate layer leak at frame {frame_id}")
        if record["estimated_bbox_xywh"] is None and (root / "estimated_labels" / filename).read_text():
            raise ValueError(f"estimated layer leak at frame {frame_id}")
    return {
        "frame_count": frame_count,
        "unresolved_review_count": len(unresolved),
        "review_status_counts": metadata.get("review_status_counts", {}),
    }


def _bool(text: str) -> bool:
    lowered = text.lower()
    if lowered == "true":
        return True
    if lowered == "false":
        return False
    raise argparse.ArgumentTypeError("expected true or false")


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    build = subparsers.add_parser("build")
    build.add_argument("--video", type=Path, required=True)
    build.add_argument("--video-sha256", required=True)
    build.add_argument("--width", type=int, required=True)
    build.add_argument("--height", type=int, required=True)
    build.add_argument("--fps-num", type=int, required=True)
    build.add_argument("--fps-den", type=int, required=True)
    build.add_argument("--frame-count", type=int, required=True)
    build.add_argument("--candidate-csv", type=Path, required=True)
    build.add_argument("--review-policy", type=Path, required=True)
    build.add_argument("--events", type=Path, required=True)
    build.add_argument("--distractors", type=Path, required=True)
    build.add_argument("--output", type=Path, required=True)
    build.add_argument("--review-dir", type=Path)
    build.add_argument("--episode-id", default="occ_video_1_car_002_full_object")
    build.add_argument("--target-id", default="car_002")
    build.add_argument("--replace-draft", action="store_true")
    validate = subparsers.add_parser("validate")
    validate.add_argument("--root", type=Path, required=True)
    validate.add_argument("--require-confirmed", type=_bool, required=True)
    return parser


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = _parser().parse_args(argv)
    if args.command == "build":
        result = build_draft(
            BuildConfig(
                video=args.video,
                video_sha256=args.video_sha256,
                width=args.width,
                height=args.height,
                fps_num=args.fps_num,
                fps_den=args.fps_den,
                frame_count=args.frame_count,
                candidate_csv=args.candidate_csv,
                review_policy_csv=args.review_policy,
                events_json=args.events,
                distractors_json=args.distractors,
                output_root=args.output,
                episode_id=args.episode_id,
                target_id=args.target_id,
                review_dir=args.review_dir,
                replace_draft=args.replace_draft,
            )
        )
    else:
        result = validate_draft(args.root, args.require_confirmed)
    print(json.dumps(result, ensure_ascii=False, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
