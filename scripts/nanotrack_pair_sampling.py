"""Controlled GOT-only adapter for NanoTrack positive-pair sampling."""

import hashlib
import json
from numbers import Integral
from pathlib import Path

import numpy as np


PAIR_POLICY_VERSION = "nanotrack-got-original-frame-id-v1"


def validate_pair_policy(policy):
    """Reject ambiguous modes and any attempt to change aerial subpools."""
    if not isinstance(policy, dict) or not policy:
        raise ValueError("pair-policy must be a nonempty prefix-to-mode JSON mapping")
    for prefix, mode in policy.items():
        if not isinstance(prefix, str) or not prefix.startswith("GOT_"):
            raise ValueError("pair-policy prefixes must start with GOT_: {!r}".format(prefix))
        if mode != "original_frame_id":
            raise ValueError("pair-policy has unknown mode {!r}".format(mode))
    return dict(policy)


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("pair-policy contains duplicate key {!r}".format(key))
        result[key] = value
    return result


def load_pair_policy(path):
    """Bind the parsed mapping to the exact bytes supplied on the command line."""
    path = Path(path).resolve()
    raw = path.read_bytes()
    try:
        policy = validate_pair_policy(json.loads(raw.decode("utf-8"), object_pairs_hook=_unique_object))
    except (ValueError, UnicodeError) as error:
        raise ValueError("invalid pair-policy {}: {}".format(path, error)) from error
    return {"version": PAIR_POLICY_VERSION, "path": str(path),
            "sha256": hashlib.sha256(raw).hexdigest(), "policy": policy}


def eligible_search_frames(frames, template_id, frame_range, mode):
    """Return ascending eligible IDs under original-ID or historical index distance."""
    frames = list(frames)
    if (not frames or any(not isinstance(frame, Integral) for frame in frames)
            or any(left >= right for left, right in zip(frames, frames[1:]))):
        raise ValueError("frames must be a nonempty ascending list of unique integer IDs")
    if not isinstance(frame_range, Integral) or frame_range < 0:
        raise ValueError("frame_range must be a non-negative integer")
    if template_id not in frames:
        raise ValueError("template_id must belong to frames")
    if mode == "original_frame_id":
        return [frame for frame in frames if abs(frame - template_id) <= frame_range]
    if mode == "list_position":
        position = frames.index(template_id)
        return frames[max(0, position - frame_range):position + frame_range + 1]
    raise ValueError("unknown pair sampling mode: {!r}".format(mode))


def install_pair_policy(subdataset_class, policy):
    """Patch only the class in this process, keeping unmatched calls exactly upstream."""
    policy = validate_pair_policy(policy)
    original = subdataset_class.__dict__.get("_nanotrack_pair_original",
                                             subdataset_class.get_positive_pair)

    def get_positive_pair(self, index):
        if not self.name.startswith("GOT_") or not any(self.name.startswith(prefix) for prefix in policy):
            return original(self, index)
        video_name = self.videos[index]
        video = self.labels[video_name]
        track = np.random.choice(list(video.keys()))
        frames = video[track]["frames"]
        template_id = frames[np.random.randint(0, len(frames))]
        search_ids = eligible_search_frames(frames, template_id, self.frame_range, "original_frame_id")
        search_id = np.random.choice(search_ids)
        return (self.get_image_anno(video_name, track, template_id),
                self.get_image_anno(video_name, track, search_id))

    subdataset_class._nanotrack_pair_original = original
    subdataset_class.get_positive_pair = get_positive_pair


def restore_pair_policy(subdataset_class):
    """Idempotently remove this adapter and restore the upstream class method."""
    if "_nanotrack_pair_original" in subdataset_class.__dict__:
        subdataset_class.get_positive_pair = subdataset_class.__dict__["_nanotrack_pair_original"]
        del subdataset_class._nanotrack_pair_original


def validate_resume_pair_policy(checkpoint, pair_policy_sha256):
    """Fail before loading mutable state when either side changes sampling semantics."""
    recorded = checkpoint.get("pair_policy_sha256")
    if recorded != pair_policy_sha256:
        raise ValueError("resume pair-policy SHA-256 mismatch: checkpoint={!r}, requested={!r}".format(
            recorded, pair_policy_sha256))
