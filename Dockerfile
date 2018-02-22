FROM rocker/rstudio

RUN apt-get update && apt-get install -y \
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
&& cd swig-rel-3.0.12 \
&& rm -f ../rel-3.0.12.tar.gz \
&& ./autogen.sh \
&& ./configure \
&& make \
&& make install

COPY . /home/rstudio/

WORKDIR /home/rstudio

RUN make all

ENTRYPOINT ["/home/rstudio/Infomap"]
CMD ["--help"]