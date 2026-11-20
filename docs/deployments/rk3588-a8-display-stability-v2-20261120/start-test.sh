#!/bin/sh
set -eu
stage=preflight
trap 'rc=$?; if [ "$rc" -ne 0 ]; then echo "ERROR: start failed at $stage (exit $rc)" >&2; fi' 0
fail() { echo "ERROR: $*" >&2; exit 1; }
check_sha() {
  actual=$(sha256sum "$1" | cut -d' ' -f1)
  [ "$actual" = "$2" ] || fail "SHA256 mismatch: $1 expected=$2 actual=$actual"
}
case "${1:-}" in ''|--check) ;; *) fail 'Usage: start-test.sh [--check]' ;; esac
echo 'Checking display-stability candidate files and configuration...'
root=/app/nanotrack_rk3588_display_stability_v2_20261120
base=$root
config=$root/config
video=$root
app=$root/bin/viewlink_app
limit=${AI_CONTROL_LIMIT:-0}
case "$limit" in 0|1) ;; *) echo 'AI_CONTROL_LIMIT must be 0 or 1' >&2; exit 2 ;; esac
images=${AI_DEBUG_IMAGES:-0}
case "$images" in 0|1) ;; *) fail 'AI_DEBUG_IMAGES must be 0 or 1' ;; esac
[ "$(hostname)" = ok3588 ] || fail "Unexpected host: $(hostname)"
wait_ready() {
  waited=0
  echo "Waiting for initialization (up to 30 seconds); pid=$pid log=$log"
  while [ "$waited" -lt 30 ]; do
    kill -0 "$pid" 2>/dev/null || fail "Program exited; see $log/stderr.log"
    if grep -q '^MODEL_INIT .*result=0$' "$log/debug/events.log" 2>/dev/null; then
      fail "Model initialization failed; see $log/stderr.log (pid=$pid)"
    fi
    if [ "$(readlink "/proc/$pid/exe" 2>/dev/null || true)" = "$app" ] &&
       grep -q '^DEBUG_READY ' "$log/debug/events.log" 2>/dev/null &&
       grep -q '^MODEL_INIT .*result=1$' "$log/debug/events.log" 2>/dev/null; then
      return 0
    fi
    sleep 1
    waited=$((waited + 1))
  done
  fail "Initialization not confirmed after 30 seconds; pid=$pid may still be running; inspect $log/stderr.log before restarting"
}
# Re-running this script must not create a second process or report a healthy
# instance as a failed launch. --check still enforces the original preflight gate.
if [ "${1:-}" != --check ]; then
  for proc in /proc/[0-9]*; do
    if [ "$(readlink "$proc/exe" 2>/dev/null || true)" = "$app" ]; then
      pid=${proc##*/}
      debug_dir=$(tr '\000' '\n' < "$proc/environ" | sed -n 's/^AI_DEBUG_DIR=//p')
      case "$debug_dir" in "$root"/log/*/debug) ;; *) fail "Candidate pid=$pid is running with an unknown log directory" ;; esac
      current_limit=$(tr '\000' '\n' < "$proc/environ" | sed -n 's/^AI_CONTROL_LIMIT=//p')
      current_images=$(tr '\000' '\n' < "$proc/environ" | sed -n 's/^AI_DEBUG_IMAGES=//p')
      [ "$current_limit" = "$limit" ] && [ "$current_images" = "$images" ] ||
        fail "Candidate pid=$pid uses AI_CONTROL_LIMIT=$current_limit AI_DEBUG_IMAGES=$current_images; stop it before changing settings"
      log=${debug_dir%/debug}
      stage="existing process readiness; log=$log"
      wait_ready
      echo "ALREADY RUNNING pid=$pid log=$log; no second process started"
      exit 0
    fi
  done
fi
if pgrep -f '[/]bin/viewlink_app' >/dev/null || pgrep -f '[/]app/script/app_watchdog.sh' >/dev/null; then
  echo 'Another viewlink_app or watchdog is running; stop it first' >&2
  exit 1
fi
check_sha "$app" a7cdf62095fdcdeeafb107988c509d85055b7ed816ca48b85541d9f772d54c43
[ -x "$app" ] || fail "Program lacks execute permission: $app (expected mode 755)"
check_sha "$config/production-deployment.conf" 0f6ace3665bb443e88dddceb85446202930100c771c60616d68c45f8b5904e3a
check_sha "$config/controller.conf" 4dafcabf08a7ae68c79ca9895f4b382583bd214397829dfeb60ba9f511f966f7
# Match the final A/B run, using a private writable copy for each launch.
check_sha "$root/systemconf.seed.ini" 48f90901238f46ceaa5dd7f60790ff41e37e33ded4996f25350fe75054b63803
check_sha "$video/with-video-config" e060e6aa0f774ca50a2574a856c38cd1f400f904befe0e551ff02533cd612ec4
[ -x "$video/with-video-config" ] || fail "Video wrapper lacks execute permission: $video/with-video-config"
kind=manual
[ "${1:-}" != --check ] || kind=preflight
log=$root/log/$kind-limit-$limit-$(date +%s)-$$
available_kb=$(df -Pk /app | awk 'END {print $4}')
[ "$images" = 0 ] || [ "$available_kb" -ge 2097152 ] ||
  fail 'Need at least 2 GiB free on /app for diagnostic images'
[ "$images" = 1 ] || [ "$available_kb" -ge 786432 ] ||
  fail 'Need at least 768 MiB free on /app for logs'
mkdir -p "$log/debug"
stage="input validation; log=$log"
echo "Checking models; AI_CONTROL_LIMIT=$limit; AI_DEBUG_IMAGES=$images; UNKNOWN_MIN_FAILED_METRICS=3; UNKNOWN_GRACE_MS=1500; UNKNOWN_RECOVERY_MS=60; UNKNOWN_STABLE_RECOVERY_MS=120; UNKNOWN_SEARCH_SEED_MS=120; DISPLAY_STABILITY=1; log=$log"
if ! (cd "$base" && sha256sum -c payload.sha256 && sha256sum -c models.sha256) > "$log/input-check.log" 2>&1; then
  cat "$log/input-check.log" >&2
  fail "Input validation failed; see $log/input-check.log"
fi
cp "$root/systemconf.seed.ini" "$log/systemconf.before.ini"
cp "$root/systemconf.seed.ini" "$log/systemconf.ini"
printf 'AI_CONTROL_LIMIT=%s\nAI_DEBUG_IMAGES=%s\n' "$limit" "$images" > "$log/parameters.txt"
sha256sum "$app" "$config/production-deployment.conf" "$config/controller.conf" \
  "$root/systemconf.seed.ini" > "$log/inputs.sha256"
# The existing launcher bind-mounts logs onto this hardcoded SDK path.
# Its empty mount point may have been removed with historical candidates.
stage="video namespace validation; log=$log"
sdk_log_target=/app/nanotrack_rk3588_4eea404_20261119/log
[ -f /param_data/systemconf.ini ] || fail 'Missing SDK config mount target: /param_data/systemconf.ini'
mkdir -p "$sdk_log_target"
if ! "$video/with-video-config" "$log/systemconf.ini" "$log" -- /bin/true \
    > "$log/video-preflight.log" 2>&1; then
  cat "$log/video-preflight.log" >&2
  fail "Video namespace validation failed; see $log/video-preflight.log"
fi
if [ "${1:-}" = --check ]; then
  echo "CHECK OK; program not started; log=$log"
  exit 0
fi
stage="launch; log=$log"
echo 'Starting viewlink_app...'
cd /app
nohup env XDG_RUNTIME_DIR=/var/run LD_LIBRARY_PATH=/app/lib:/usr/lib AI_CONTROL_LIMIT="$limit" AI_DEBUG_DIR="$log/debug" AI_DEBUG_IMAGES="$images" \
  "$video/with-video-config" "$log/systemconf.ini" "$log" -- "$app" \
  > "$log/stdout.log" 2> "$log/stderr.log" < /dev/null &
pid=$!
echo "$pid" > "$log/pid"
stage="initialization; log=$log"
wait_ready
ls -l "/proc/$pid/fd" > "$log/fd-map.txt"
uname -a > "$log/runtime.txt"
cat /proc/version >> "$log/runtime.txt"
echo "STARTED pid=$pid log=$log"
echo "DEBUG: images=$images; serial log=$log/debug/events.log"
echo 'Wait at least 5 seconds after the final click before stopping.'
