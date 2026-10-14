# Detector-assisted NanoTrack recapture design

## Goal

Make detector-assisted long-term recovery safe for both whole-object targets
(person/car) and arbitrary subtargets, while preserving the invariant that a
detector result alone can never produce `control_valid=true`.

## Scope

This phase changes the RK3566 long-term controller and its audit log. It does
not change the YOLOv8 detector implementation or model, add a ReID model, or
claim an occlusion-recovery rate without labeled data.

## Target modes

`full_object` is the backward-compatible default. A detector candidate is a
provisional target-sized hypothesis and may be passed to NanoTrack as the
predicted target box.

`subtarget` treats the detector candidate only as a search region. NanoTrack
keeps the last accepted subtarget width and height and probes bounded points
inside the region. A detector region must never replace the subtarget box.

Mode selection is explicit in the controller configuration for this phase.
Automatic initialization-time classification is deferred until labeled
initialization examples exist.

## Candidate lifecycle

Every accepted detector candidate receives a monotonically increasing local
candidate id. Only one candidate may be active at a time.

1. The controller probes one active candidate. In `subtarget` mode it may use
   several bounded probe locations over successive frames.
2. A detector-origin probe becomes locked only when both anchor and recent
   template observations are reliable and agree spatially.
3. Once locked, later frames continue from that candidate's NanoTrack bbox;
   pending detector candidates are not consumed.
4. Recovery requires two spatially and geometrically consistent reliable
   NanoTrack confirmations within three locked frames.
5. A failed confirmation window discards the lock before another candidate
   can be tried.
6. New detector batches may refresh pending candidates but cannot replace an
   active lock.

## Geometry and deformation

Detector-origin confirmations use dedicated, conservative IoU, center,
area-change, and aspect-change gates. These bring-up values remain marked
uncalibrated. After successful recovery, the existing recent-template staging
and probation policy handles gradual deformation; the anchor remains fixed.

## Safety invariants

- A detector batch, detector confidence, or detector bbox never directly sets
  `accepted`, `reacquired`, or `control_valid`.
- Confirmations from different candidate ids never combine.
- A detector-origin recovery requires anchor and recent evidence.
- Ambiguous or failed candidates leave control invalid.
- The existing detector implementation and ABI remain unchanged.

## Observability

The CSV log records target mode, candidate id, detector candidate region,
lock state, probe index, confirmation attempts, and confirmation hits. The
schema version is incremented.

## Validation boundary

Unit tests and synthetic replay prove state-machine safety. `girl_dance.mp4`
may be used for an unlabeled diagnostic run, including occlusion and same-class
interference inspection. Recovery accuracy, false-recovery rate, and final
thresholds require frame annotations and are TODO items, not acceptance claims.

