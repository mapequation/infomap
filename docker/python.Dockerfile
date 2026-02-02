FROM python:3.11-slim

RUN apt-get update && apt-get install -y \
  build-essential \
  swig \
  git \
  && apt-get clean \
  && rm -rf /var/lib/apt/lists/*

COPY . /infomap/

WORKDIR /infomap

# Upgrade pip and install build/test dependencies
RUN python -m pip install --upgrade pip setuptools wheel
RUN python -m pip install -r requirements_dev.txt

# Build python artifacts
RUN make python

ENTRYPOINT ["/infomap/Infomap"]
CMD ["--help"]