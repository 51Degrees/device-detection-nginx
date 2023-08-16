FROM debian:12-slim
WORKDIR /app

#build dependencies
RUN apt-get update && apt-get install --no-install-recommends -y git curl ca-certificates build-essential libpcre3-dev libz-dev nano && rm -rf /var/lib/apt/lists/*

#cloning the current repo
RUN git clone https://github.com/51Degrees/device-detection-nginx . && git submodule update --init --recursive

#dev dependencies
RUN curl -Lo device-detection-cxx/device-detection-data/51Degrees-LiteV4.1.hash https://github.com/51Degrees/device-detection-data/raw/main/51Degrees-LiteV4.1.hash

RUN make install

COPY nginx.conf /app/build/nginx.conf

EXPOSE 8080

CMD ["/app/nginx", "-c", "/app/build/nginx.conf"]
