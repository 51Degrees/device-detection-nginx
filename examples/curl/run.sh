docker build -t 51d-nginx-curl .
docker run -d -p 8080:8080 51d-nginx-curl
sleep 1
./curl.sh
