FROM ubuntu:18.04
# FROM ubuntu:latest

RUN apt-get update && apt-get install -y \
  build-essential \
  python3-pip \
  swig \
  && apt-get clean \
  && rm -rf /var/lib/apt/lists/*

COPY . /infomap/

WORKDIR /infomap

RUN make -j

RUN pip3 --no-cache-dir install Cython --install-option="--no-cython-compile"

RUN python3 -m pip install --upgrade pip setuptools wheel

RUN pip3 --no-cache-dir install -r requirements_dev.txt

RUN make python

RUN pip3 --no-cache-dir install --index-url https://test.pypi.org/simple/ infomap

ENTRYPOINT ["/infomap/Infomap"]
CMD ["--help"]