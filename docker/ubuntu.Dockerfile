FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
  build-essential \
  make \
  python3 \
  python3-pip \
  swig \
  && apt-get clean \
  && rm -rf /var/lib/apt/lists/*

COPY . /infomap/

WORKDIR /infomap

RUN make -j build-native \
  && make build-python-package-files \
  && python3 -m pip install --break-system-packages --no-build-isolation build/py

RUN python3 -c "import infomap; print(infomap.__version__)"

ENTRYPOINT ["/infomap/Infomap"]
CMD ["--help"]
