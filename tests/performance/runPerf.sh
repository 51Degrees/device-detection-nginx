#!/bin/bash

# Constants
FULLPATH="$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
HOST=127.0.0.1:3000
CAL=calibrate
PRO=process
PERF=./ApacheBench-prefix/src/ApacheBench-build/bin/runPerf.sh

# Run the benchmark with as many concurrent requests as there are
# processors, matching the worker_processes auto setting in the config
# templates, so that all of the Nginx workers are exercised at once. This
# mirrors the multi threaded benchmarks of the other 51Degrees APIs rather
# than measuring a single request at a time. Override with CONCURRENCY.
CONCURRENCY=${CONCURRENCY:-$(nproc)}

# Number of requests per worker. The total passed to ApacheBench scales
# with the concurrency so that each phase runs for the same wall-clock
# time however many processors the machine has. The overhead is measured
# as the difference between the calibration and processing wall times, so
# holding that time constant keeps the measurement resolution constant
# and stops the difference from disappearing into timing noise on a
# machine with many cores.
PASSES_PER_WORKER=${PASSES_PER_WORKER:-20000}
PASSES=$((PASSES_PER_WORKER * CONCURRENCY))

# The engine to test. Either 'hash' (device detection, the default) or
# 'ipi' (IP intelligence).
ENGINE=${ENGINE:-hash}

# Get the repo directory as an absolute path
ORIGINPATH="$(pwd)"
cd $FULLPATH/../../../
REPO_DIR="$(pwd)"
cd $ORIGINPATH

echo $REPO_DIR

# Create the nginx.conf
MODULES_DIR=$REPO_DIR/build/modules
if [ "$ENGINE" == "ipi" ]; then
	DATA_FILE_DIR_IPI=$REPO_DIR/ip-intelligence-cxx/ip-intelligence-data
	sed "s/\${MODULES_DIR}/${MODULES_DIR//\//\\/}/g" ./nginx.conf.ipi.template > ./nginx.conf
	sed -i "s/\${DATA_FILE_DIR_IPI}/${DATA_FILE_DIR_IPI//\//\\/}/g" ./nginx.conf
	if [ "$DATA_FILE_NAME_IPI" ]; then
		sed -i "s/51Degrees-IPIV4AsnIpiV41\.ipi/${DATA_FILE_NAME_IPI}/g" nginx.conf
	fi
	# The benchmark harness rotates through the lines of uas.csv using the
	# User-Agent header. Replace it with the IP addresses evidence file so
	# that each request carries an IP address, and keep a backup to restore
	# after the run.
	if [ ! -f uas.csv.bkp ]; then
		cp uas.csv uas.csv.bkp
	fi
	cp $REPO_DIR/ip-intelligence-cxx/ip-intelligence-data/evidence.csv uas.csv
else
	DATA_FILE_DIR=$REPO_DIR/device-detection-cxx/device-detection-data
	sed "s/\${MODULES_DIR}/${MODULES_DIR//\//\\/}/g" ./nginx.conf.template > ./nginx.conf
	sed -i "s/\${DATA_FILE_DIR}/${DATA_FILE_DIR//\//\\/}/g" ./nginx.conf
	if [ "$DATA_FILE_NAME" ]; then
		sed -i "s/51Degrees-LiteV4\.1\.hash/${DATA_FILE_NAME}/g" nginx.conf
	fi
	# Restore the User-Agents file if a previous IP intelligence run
	# replaced it.
	if [ -f uas.csv.bkp ]; then
		mv uas.csv.bkp uas.csv
	fi
fi

# Create required directories and target files for service
echo > ../../../build/html/calibrate
echo > ../../../build/html/process

# Create coredumps folder
mkdir -p coredumps

# Backup the current config and replace with the test config
mv $REPO_DIR/build/nginx.conf $REPO_DIR/build/nginx.conf.bkp
cp $FULLPATH/nginx.conf $REPO_DIR/build/nginx.conf

# Run the performrance
$PERF -n $PASSES -s "$REPO_DIR/nginx" -t "$REPO_DIR/nginx -s stop" -c $CAL -p $PRO -h $HOST -k $CONCURRENCY

# Replace the original config
mv $REPO_DIR/build/nginx.conf.bkp $REPO_DIR/build/nginx.conf

# Restore the User-Agents file if this was an IP intelligence run.
if [ -f uas.csv.bkp ]; then
	mv uas.csv.bkp uas.csv
fi

# Remove coredumps folder
if [ ! "$(find coredumps -type f)" ]; then
	rmdir coredumps
fi
