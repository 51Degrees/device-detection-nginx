#!/bin/bash

# Constants
FULLPATH="$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
PASSES=20000
HOST=127.0.0.1:3000
CAL=calibrate
PRO=process
PERF=./ApacheBench-prefix/src/ApacheBench-build/bin/runPerf.sh

# Create the nginx.conf
MODULES_DIR=$FULLPATH/../../../build/modules
DATA_FILE_DIR=$FULLPATH/../../../device-detection-cxx/device-detection-data
sed "s/\${MODULES_DIR}/${MODULES_DIR//\//\\/}/g" ./nginx.conf.template > ./nginx.conf
sed -i "s/\${DATA_FILE_DIR}/${DATA_FILE_DIR//\//\\/}/g" ./nginx.conf

# Create required directories and target files for service
echo > ../../../build/html/calibrate
echo > ../../../build/html/process

# Create coredumps folder
mkdir -p coredumps

# Run the nginx server here so we have more control over how to closing it.
$FULLPATH/../../../nginx -c $FULLPATH/nginx.conf

RESPONSE=`curl -I 127.0.0.1:3000 | grep -c "200 OK"`
if [ $RESPONSE -gt '0' ]; then
	echo "Server is up and alive."
else
	echo "Failed: server failed to start."
	exit 1
fi

# Run the performrance
$PERF -n $PASSES -s "echo \"Service is runned externally from parent script.\"" -c $CAL -p $PRO -h $HOST

# Close the nginx server here
echo "Close the running nginx."
$FULLPATH/../../../nginx -s quit 

# Remove coredumps folder
if [ ! "$(find coredumps -type f)" ]; then
	rmdir coredumps
fi
