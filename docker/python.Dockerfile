FROM python:3.13-slim

RUN apt-get update && apt-get install -y \
  build-essential \
  make \
  && apt-get clean \
  && rm -rf /var/lib/apt/lists/*

COPY . /infomap/

WORKDIR /infomap

# Upgrade pip and install Python build tooling
RUN python -m pip install --upgrade pip build wheel

RUN make build-python \
  && make dev-python-install \
  && make test-python-unit

ENTRYPOINT ["python", "-m", "pytest"]
CMD ["test/python"]
