#!/bin/bash

LOCAL_FILE="log/pid.txt"
REMOTE_USER="gye"
REMOTE_HOST="10.42.0.1"
REMOTE_PATH="/home/gye/drone/log"

while inotifywait -e modify "$LOCAL_FILE"; do
    rsync -avz --partial --progress "$LOCAL_FILE" "$REMOTE_USER@$REMOTE_HOST:$REMOTE_PATH"
done
