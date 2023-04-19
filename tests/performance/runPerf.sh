#!/bin/bash

# Constants
FULLPATH="$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
PASSES=20000
HOST=127.0.0.1:3000
CAL=calibrate
PRO=process
PERF=./ApacheBench-prefix/src/ApacheBench-build/bin/runPerf.sh

# Get the repo directory as an absolute path
ORIGINPATH="$(pwd)"
cd $FULLPATH/../../../
REPO_DIR="$(pwd)"
cd $ORIGINPATH

echo $REPO_DIR

# Create the nginx.conf
MODULES_DIR=$REPO_DIR/build/modules
DATA_FILE_DIR=$REPO_DIR/device-detection-cxx/device-detection-data
sed "s/\${MODULES_DIR}/${MODULES_DIR//\//\\/}/g" ./nginx.conf.template > ./nginx.conf
sed -i "s/\${DATA_FILE_DIR}/${DATA_FILE_DIR//\//\\/}/g" ./nginx.conf

# Create required directories and target files for service
echo > ../../../build/html/calibrate
echo > ../../../build/html/process

# Create coredumps folder
mkdir -p coredumps

# Backup the current config and replace with the test config
mv $REPO_DIR/build/nginx.conf $REPO_DIR/build/nginx.conf.bkp
cp $FULLPATH/nginx.conf $REPO_DIR/build/nginx.conf

# Run the performrance
$PERF -n $PASSES -s "$REPO_DIR/nginx" -t "$REPO_DIR/nginx -s stop" -c $CAL -p $PRO -h $HOST

# Replace the original config
mv $REPO_DIR/build/nginx.conf.bkp $REPO_DIR/build/nginx.conf

# Remove coredumps folder
if [ ! "$(find coredumps -type f)" ]; then
	rmdir coredumps
fi
