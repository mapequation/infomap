FROM python:3.13-slim

RUN apt-get update && apt-get install -y \
  build-essential \
  make \
  swig \
  && apt-get clean \
  && rm -rf /var/lib/apt/lists/*

COPY . /infomap/

WORKDIR /infomap

# Upgrade pip and install build/test dependencies
RUN python -m pip install --upgrade pip setuptools wheel
RUN python -m pip install -r requirements_dev.txt

RUN make build-python \
  && make dev-python-install \
  && make test-python-unit

ENTRYPOINT ["python", "-m", "pytest"]
CMD ["test/python"]
