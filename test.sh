#!/bin/bash

## Set test constants. ##
if [ "$REQUESTS" == "" ]; then
REQUESTS=100000
fi
LIMIT=0.150
RELOADLIMIT=1000
if [ "$RELOADSTODO" == "" ]; then
RELOADSTODO=5
fi
FAILED=0

## Set text macros. ##
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'
PASS="${GREEN}[Pass]${NC}\t"
FAIL="${RED}[Fail]${NC}\t"

HASH=`2>&1 ./nginx -V | grep -c "FIFTYONEDEGREES_HASH"`

if [ ! -e "./nginx" ]; then
	printf "nginx is not installed in this directory. Install with \"make install\"\n"
	exit
fi

## Write the config file to use for testing. ##
printf "\n---------------------\n| Writing config file.\n--------------------\n"

mv build/nginx.conf build/nginx.conf.bkp
echo "worker_processes 4;" >> build/nginx.conf
echo "worker_rlimit_core  500M;" >> build/nginx.conf
echo "working_directory   coredumps/;" >> build/nginx.conf
echo "" >> build/nginx.conf
echo "load_module modules/ngx_http_51D_module.so;" >> build/nginx.conf
echo "" >> build/nginx.conf
echo "events {" >> build/nginx.conf
echo "	worker_connections 1024;" >> build/nginx.conf
echo "}" >> build/nginx.conf
echo "" >> build/nginx.conf
echo "http {" >> build/nginx.conf
echo "	## set the datafile ##" >> build/nginx.conf
echo "	51D_file_path ./device-detection-cxx/device-detection-data/51Degrees-LiteV4.1.hash;" >> build/nginx.conf
echo "  51D_use_performance_graph on;" >> build/nginx.conf
echo "  51D_use_predictive_graph off;" >> build/nginx.conf
echo "" >> build/nginx.conf
echo "	server {" >> build/nginx.conf
echo "		listen 8888;" >> build/nginx.conf
echo "" >> build/nginx.conf
echo "		location / {" >> build/nginx.conf
echo "		51D_match_all x-mobile IsMobile;" >> build/nginx.conf
echo "		add_header test \$http_x_mobile;" >> build/nginx.conf
echo "		}" >> build/nginx.conf
echo "	}" >> build/nginx.conf
echo "}" >> build/nginx.conf

printf "Written test config to \"build/nginx.conf\" and stored current one in \"build/nginx.conf.bkp\".\n"

## Make a directory for core dumps if one does not already exist. ##
if [ ! -e "coredumps" ]; then
	mkdir coredumps
fi

## Kill any current Nginx processes and start a new one. ##
printf "\n---------------------\n| Starting new nginx process.\n---------------------\n"

SLEEPCOUNT='0'
while [ "$(pidof nginx)" ]; do
	killall nginx
	SLEEPCOUNT=$(($SLEEPCOUNT + 1))	
	if [ $SLEEPCOUNT -gt 5 ]; then
		printf "\n--------------------\n| Restoring previous config file.\n--------------------\n"
		rm build/nginx.conf
		mv build/nginx.conf.bkp build/nginx.conf
		printf "Removed test config \"build/nginx.conf\" and restored \"build/nginx.conf.bkp\".\n"
		printf "Failed to stop currently running nginx process\n"
		exit
	fi
	sleep 2
done
./nginx
printf "Nginx process started.\n"

## Test the server gives a valid response. ##
printf "\n---------------------\n| Testing Server response.\n--------------------\n"

RESPONSE=`curl -I 127.0.0.1:8888 | grep -c "200 OK"`
if [ $RESPONSE -gt '0' ]; then
	printf "${PASS}Server Response: Server response 200 recieved.\n"
else
	printf "${FAIL}Server Response: Server response 200 not recieved.\n"
	FAILED=$(($FAILED + 1))
fi

## Test the server matches desktop and mobile User-Agents correctly. ##
printf "\n---------------------\n| Testing mobile/non-mobile User-Agents.\n--------------------\n"

DESKTOPUA="Mozilla/5.0 (Windows NT 6.3; WOW64; rv:41.0) Gecko/20100101 Firefox/41.0"
MOBILEUA="Mozilla/5.0 (iPhone; CPU iPhone OS 7_1 like Mac OS X) AppleWebKit/537.51.2 (KHTML, like Gecko) 'Version/7.0 Mobile/11D167 Safari/9537.53"
TRUE='test: True'
FALSE='test: False'

CORRECTHEADER=`curl -A "$DESKTOPUA" -I 127.0.0.1:8888 | grep -c "$FALSE"`
if [ $CORRECTHEADER -gt '0' ]; then
	printf "${PASS}Desktop User-Agent: matched correctly\n"
else
	printf "${FAIL}Desktop User-Agent: matched incorrectly\n"
	FAILED=$(($FAILED + 1))
fi

CORRECTHEADER=`curl -A "$MOBILEUA" -I 127.0.0.1:8888/ | grep -c "$TRUE"`
if [ $CORRECTHEADER -gt '0' ]; then
	printf "${PASS}Mobile User-Agent: matched correctly\n"
else
	printf "${FAIL}Mobile User-Agent: matched incorrectly\n"
	FAILED=$(($FAILED + 1))
fi

## Test the time per response when none of the User-Agents are stored in the cache. ##
printf "\n---------------------\n| Testing ApacheBench stress with no cache.\n--------------------\n"

TIME=`./tests/performance/build/ApacheBench-prefix/src/ApacheBench-build/bin/ab -c 10 -i -n $REQUESTS -U ./tests/performance/build/uas.csv 127.0.0.1:8888/ | grep "(mean, across all concurrent requests)" | egrep -o '\<[0-9]{1,2}\.[0-9]{2,5}\>'`
if [ $TIME ]; then
	if [ $(echo "$TIME < $LIMIT" | bc -l) -eq 1 ]; then
		printf "${PASS}Stress test: $TIME ms per detection is within limit of $LIMIT.\n"
	else
		printf "${FAIL}Stress test: $TIME ms per detection exceedes limit of $LIMIT.\n"
		FAILED=$(($FAILED + 1))
	fi
	BASETIME=$TIME
else
	printf "${FAIL}Stress test reload: ApacheBench could not complete tests.\n"
	FAILED=$(($FAILED + 1))
fi

## Test the time per response when periodically reloading Nginx, and the time ##
## penalty involved with reloading (this should be around 0 unless there is a ##
## problem). ##
printf "\n---------------------\n| Testing ApacheBench stress with reload.\n--------------------\n"

RELOADCOUNT=0
RELOADPASS=1
TOTALTIME=0
while [ $RELOADCOUNT -lt $RELOADSTODO ]; do
	printf "Doing nginx reload test $((RELOADCOUNT + 1)) of ${RELOADSTODO}\n"
	./nginx -s reload
	RELOADCOUNT=$(($RELOADCOUNT + 1))
	TIME=`./tests/performance/build/ApacheBench-prefix/src/ApacheBench-build/bin/ab -c 10 -i -n $(echo "$REQUESTS / $RELOADSTODO" | bc -l) -q -U ./tests/performance/build/uas.csv 127.0.0.1:8888/ | grep "mean, across all concurrent requests" | egrep -o '\<[0-9]{1,2}\.[0-9]{2,5}\>'`
	if [ ! "$TIME" ]; then
		break
	fi
	TOTALTIME=$(echo "$TOTALTIME + $TIME" | bc -l)
done

if [ "$TIME" ]; then
	TOTALTIME=0$(echo "scale=3; $TOTALTIME / $RELOADSTODO" | bc -l)
	RELOADTIME=$(echo "scale=0; ( $TOTALTIME - $BASETIME ) * $REQUESTS" | bc -l)
	if [ $(echo "$RELOADTIME < $RELOADLIMIT" | bc -l) -eq 1 ]; then
		printf "${PASS}Reload penalty: $RELOADTIME ms penalty on reloading is within limit of $RELOADLIMIT.\n"
	else
		printf "${FAIL}Reload penalty: $RELOADTIME ms penalty on reloading exceedes limit of $RELOADLIMIT.\n"
		FAILED=$(($FAILED + 1))
	fi
	if [ $(echo "$TOTALTIME < $LIMIT" | bc -l) -eq 1 ]; then
		printf "${PASS}Stress test reload: $TOTALTIME ms per detection is within limit of $LIMIT.\n"
	else
		printf "${FAIL}Stress test reload: $TOTALTIME ms per detection exceedes limit of $LIMIT.\n"
		FAILED=$(($FAILED + 1))
	fi
else
	printf "${FAIL}Stress test reload: ApacheBench could not complete tests.\n"
	FAILED=$(($FAILED + 1))
fi

## Revert to the config file which was backed up earlier. ##
printf "\n--------------------\n| Restoring previous config file.\n--------------------\n"

rm build/nginx.conf
mv build/nginx.conf.bkp build/nginx.conf
printf "Removed test config \"build/nginx.conf\" and restored \"build/nginx.conf.bkp\".\n"
./nginx -s quit
printf "Killed running nginx processes.\n"
if [ $FAILED -gt '0' ]; then
	TESTCOLOUR=${RED}
else
	TESTCOLOUR=${GREEN}
fi

## Print the outcome of the tests. ##
printf "\n${TESTCOLOUR}Finished tests with ${FAILED} failure(s).${NC}\n"

## If there were no core dumps, remove the directory. ##
if [ ! "$(find coredumps -type f)" ]; then
	rmdir coredumps
fi
