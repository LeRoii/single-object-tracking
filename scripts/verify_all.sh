#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$project_root"

require_file() {
    local path="$1"
    [[ -s "$path" ]] || { echo "FAIL missing or empty: $path" >&2; exit 1; }
}

check_video() {
    local name="$1"
    local path="$2"
    local expected="$3"
    require_file "$path"
    local frames
    frames="$(ffprobe -v error -select_streams v:0 -count_frames -show_entries stream=nb_read_frames -of csv=p=0 "$path")"
    [[ "$frames" == "$expected" ]] || { echo "FAIL $name frames=$frames expected=$expected" >&2; exit 1; }
    echo "PASS $name video_frames=$frames"
}

check_lines() {
    local name="$1"
    local path="$2"
    local expected="$3"
    require_file "$path"
    local lines
    lines="$(wc -l < "$path")"
    [[ "$lines" == "$expected" ]] || { echo "FAIL $name lines=$lines expected=$expected" >&2; exit 1; }
    echo "PASS $name result_lines=$lines"
}

loaded_version="$(sed -n 's/^NVRM version:.*Kernel Module  \([^ ]*\).*/\1/p' /proc/driver/nvidia/version)"
disk_version="$(modinfo -F version nvidia)"
nvml_version="$(nvidia-smi --query-gpu=driver_version --format=csv,noheader | head -n 1)"
[[ "$loaded_version" == "$disk_version" && "$disk_version" == "$nvml_version" ]] || {
    echo "FAIL NVIDIA versions: loaded=$loaded_version disk=$disk_version nvml=$nvml_version" >&2
    exit 1
}
echo "PASS NVIDIA driver/NVML=$nvml_version"

for env_name in fear lighttrack nanotrack siamabc siammask stmtrack; do
    ".venvs/$env_name/bin/pip" check >/dev/null
    ".venvs/$env_name/bin/python" -c 'import torch; assert torch.cuda.is_available(); torch.ones(1, device="cuda")' >/dev/null
    echo "PASS $env_name environment+CUDA"
done

require_file repos/fear/evaluate/checkpoints/FEAR-XS-NoEmbs.ckpt
require_file repos/lighttrack/snapshot/LightTrackM/LightTrackM.pth
require_file repos/siamtrackers/NanoTrack/models/pretrained/nanotrackv3.pth
require_file repos/siamabc/assets/S_Tiny/model_S_Tiny_v1.pt
require_file repos/siammask/experiments/siammask_sharp/SiamMask_DAVIS.pth
require_file repos/stmtrack/snapshots/stmtrack-googlenet-fulldata-train/epoch-19.pkl

check_video FEAR artifacts/fear/output.mp4 661
check_lines LightTrack repos/lighttrack/result/VOT2019/LightTrackM/baseline/fear-demo/fear-demo_001.txt 120
check_video NanoTrack artifacts/nanotrack/output.mp4 987
check_lines NanoTrack artifacts/nanotrack/bboxes.txt 987
check_video SiamABC artifacts/siamabc/output.mp4 256
check_lines SiamABC artifacts/siamabc/bboxes/output.txt 256
check_video SiamMask artifacts/siammask/output.mp4 120
check_lines SiamMask artifacts/siammask/bboxes.txt 120
check_video STMTrack artifacts/stmtrack/output.mp4 120
check_lines STMTrack artifacts/stmtrack/bboxes.txt 120

echo "PASS all six trackers"
