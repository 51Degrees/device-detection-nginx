# cURL Example

This example demonstrates how to feed arbitrary evidence in the form of headers to an nginx module using cURL.

## Build

There is a docker image with nginx and 51d module - convenient for testing on a mac. 

You can just run `./run.sh` from this directory or `docker` commands similar to:

```sh
docker build -t 51d-nginx-curl .
docker run -d -p 8080:8080 51d-nginx-curl
```

## Run

Use the `curl.sh` script or just plain curl command (the evidence provided may be arbitrary):
```
$ curl  --head \
        -H 'User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/114.0.0.0 Safari/537.36 Edg/114.0.1823.41' \
        -H 'Sec-CH-UA: "Not.A/Brand";v="8","Chromium";v="114","Microsoft Edge";v="114"' \
        -H 'Sec-CH-UA-Arch: x86' \
        -H 'Sec-CH-UA-Bitness: 64' \
        -H 'Sec-CH-UA-Full-Version: 114.0.5735.110' \
        -H 'Sec-CH-UA-Full-Version-List: "Not.A/Brand";v="8.0.0.0","Chromium";v="114.0.5735.110","Microsoft Edge";v="114.0.1823.41"' \
        -H 'Sec-CH-UA-Mobile: ?0' \
        -H 'Sec-CH-UA-Model: ' \
        -H 'Sec-CH-UA-Platform: Windows' \
        -H 'Sec-CH-UA-Platform-Version: 15.0.0' \
        http://127.0.0.1:8080/hash

HTTP/1.1 200 OK
Server: nginx/1.20.0
Date: Wed, 16 Aug 2023 16:03:00 GMT
Content-Type: text/plain
Content-Length: 1
Last-Modified: Wed, 16 Aug 2023 13:41:12 GMT
Connection: keep-alive
ETag: "64dcd1f8-1"
x-device-type: NoMatch
x-platform-name: Windows
x-platform-version: 11.0
x-browser-name: Edge (Chromium) for Windows
x-browser-version: 114.0
x-is-mobile: False
Accept-Ranges: bytes
```

To test feeding the evidence via the PHP libcurl - use `curl.php` script:

```
$ php examples/curl/curl.php 
HTTP/1.1 200 OK
Server: nginx/1.20.0
Date: Wed, 16 Aug 2023 16:01:19 GMT
Content-Type: text/plain
Content-Length: 1
Last-Modified: Wed, 16 Aug 2023 13:41:12 GMT
Connection: keep-alive
ETag: "64dcd1f8-1"
x-device-type: NoMatch
x-platform-name: Windows
x-platform-version: 11.0
x-browser-name: Edge (Chromium) for Windows
x-browser-version: 114.0
x-is-mobile: False
Accept-Ranges: bytes
```