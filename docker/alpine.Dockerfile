ARG ALPINE_VERSION=3.12

FROM alpine:$ALPINE_VERSION AS builder

RUN apk add --no-cache build-base

COPY . /infomap

WORKDIR /infomap

RUN make

FROM alpine:${ALPINE_VERSION}

ARG INFOMAP_VERSION
ARG VCS_REF
ARG VCS_URL
LABEL org.mapequation.infomap.version=$INFOMAP_VERSION
LABEL org.mapequation.infomap.vcs-ref=$VCS_REF
LABEL org.mapequation.infomap.vcs-url=$VCS_URL

RUN apk add --no-cache \
        libgcc \
        libstdc++ \
        libgomp

WORKDIR /infomap

COPY --from=builder /infomap/Infomap .

VOLUME /data

ENTRYPOINT ["/infomap/Infomap"]
CMD ["--help"]
