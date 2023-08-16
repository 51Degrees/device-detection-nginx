# cURL Example

This example demonstrate how to feed arbitrary evidence in the form of headers to an nginx module using cURL.

## Build

There is a docker image with nginx and 51d module - convenient for testing on a mac. 

You can just run `./run.sh` from this directory or:

```sh
docker build -t 51d-nginx-curl .
docker run -d -p 8080:8080 51d-nginx-curl
```

To test curl from the command line use the `curl.sh` script (the evidence may be arbitrary).

To test feeding the evidence via the PHP libcurl - use `curl.php` script.  