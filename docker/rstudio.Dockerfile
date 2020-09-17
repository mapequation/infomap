ARG BASE_CONTAINER=rocker/tidyverse:3.6.3-ubuntu18.04
FROM $BASE_CONTAINER

RUN apt-get update && apt-get install -y -q \
    build-essential \
    libpcre3-dev \
    autoconf \
    automake \
    libtool \
    bison \
    git \
    libboost-dev \
    python3-dev \
  && apt-get clean \
  && rm -rf /var/lib/apt/lists/*

RUN wget https://github.com/swig/swig/archive/rel-3.0.12.tar.gz \
  && tar -zxf rel-3.0.12.tar.gz \
  && rm -f ../rel-3.0.12.tar.gz \
  && cd swig-rel-3.0.12 \
  && ./autogen.sh \
  && ./configure \
  && make \
  && make install

RUN mkdir -p /infomap

COPY . /infomap/

WORKDIR /infomap

RUN make R