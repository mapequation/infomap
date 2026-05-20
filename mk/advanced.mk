##################################################
# Advanced / secondary targets
##################################################

TAG_NAME := mapequation/infomap
DOCKER ?= docker
DOCKER_COMPOSE ?= $(DOCKER) compose
DOCKER_SUPPORTED_CLI_TAG ?= infomap:cli-local
DOCKER_SUPPORTED_NOTEBOOK_TAG ?= infomap:notebook-local

.PHONY: \
	docker-build \
	docker-run \
	docker-build-notebook \
	docker-run-notebook \
	docker-build-ubuntu-test-python \
	docker-run-ubuntu-test-python \
	docker-build-python \
	test-docker-supported

docker-build: docker/infomap.Dockerfile
	$(DOCKER_COMPOSE) build infomap

docker-run:
	$(DOCKER_COMPOSE) run --rm infomap

docker-build-notebook: docker/notebook.Dockerfile
	$(DOCKER_COMPOSE) build notebook

docker-run-notebook:
	$(DOCKER_COMPOSE) up notebook

docker-build-ubuntu-test-python:
	$(DOCKER) build -f docker/ubuntu.Dockerfile -t infomap:python-test .

docker-run-ubuntu-test-python:
	$(DOCKER) run --rm infomap:python-test

docker-build-python: docker/python.Dockerfile
	$(DOCKER) build -f docker/python.Dockerfile -t infomap-python .

test-docker-supported:
	$(DOCKER) build -f docker/infomap.Dockerfile -t $(DOCKER_SUPPORTED_CLI_TAG) .
	$(DOCKER) run --rm $(DOCKER_SUPPORTED_CLI_TAG) --help > /dev/null
	$(DOCKER) build -f docker/notebook.Dockerfile -t $(DOCKER_SUPPORTED_NOTEBOOK_TAG) .
	$(DOCKER) run --rm $(DOCKER_SUPPORTED_NOTEBOOK_TAG) python -c "from infomap import Infomap; print(Infomap(silent=True).num_top_modules)" > /dev/null
