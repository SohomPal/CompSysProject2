#!/usr/bin/env bash
# Usage: ./signals_test.sh
#
# 1) start the part3 program in background
# 2) send each of the 8 relevant signals three times to the parent
# 3) wait a bit, then send same to each child
# 4) collect output (redirect this script's stdout to output.txt)

./part3 &
PARENT=$!
echo "Started part3 (PID=$PARENT)"
sleep 1

for sig in INT QUIT TSTP ABRT ILL CHLD SEGV FPE HUP; do
  for i in 1 2 3; do
    kill -$sig $PARENT
  done
done

sleep 2

CHILDREN=$(pgrep -P $PARENT)
for c in $CHILDREN; do
  echo "Sending to child $c"
  for sig in INT QUIT TSTP ABRT ILL CHLD SEGV FPE HUP; do
    for i in 1 2 3; do
      kill -$sig $c
    done
  done
done

wait $PARENT
