#!/bin/bash
# Runs a command; when it fails, its last lines become a GitHub error
# annotation, readable through the API without downloading the job log.
log=$(mktemp)
"$@" 2>&1 | tee "$log"
code=${PIPESTATUS[0]}
if [ "$code" -ne 0 ]; then
  msg=$(tail -n 120 "$log" | sed -e 's/%/%25/g' -e 's/\r/%0D/g' | awk '{printf "%s%%0A", $0}')
  echo "::error title=$1 $2 $3 failed (exit $code)::$msg"
fi
rm -f "$log"
exit "$code"
