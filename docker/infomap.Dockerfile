ARG ALPINE_VERSION=3.22

FROM alpine:$ALPINE_VERSION AS builder

RUN apk add --no-cache build-base

COPY . /infomap

WORKDIR /infomap

RUN make -j build-native

FROM alpine:${ALPINE_VERSION}

RUN apk add --no-cache \
        libgcc \
        libstdc++ \
        libgomp

RUN mkdir /infomap

COPY --from=builder /infomap/Infomap /infomap

VOLUME /data

WORKDIR /data

ENTRYPOINT ["/infomap/Infomap"]
CMD ["--help"]
