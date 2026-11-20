#!/bin/sh
set -eu
app=/app/nanotrack_rk3588_display_stability_20261120/bin/viewlink_app
[ "$(hostname)" = ok3588 ]
pids=
for proc in /proc/[0-9]*; do
  pid=${proc##*/}
  if [ "$(readlink "$proc/exe" 2>/dev/null || true)" = "$app" ]; then
    pids="$pids $pid"
  fi
done
[ -n "$pids" ] || { echo 'No candidate running'; exit 0; }
for pid in $pids; do kill -TERM "$pid" 2>/dev/null || true; done
sleep 5
for pid in $pids; do
  if [ "$(readlink "/proc/$pid/exe" 2>/dev/null || true)" = "$app" ]; then
    kill -KILL "$pid" 2>/dev/null || true
  fi
done
echo "STOPPED$pids"
