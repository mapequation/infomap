ARG BASE_CONTAINER=jupyter/scipy-notebook:python-3.11
FROM $BASE_CONTAINER

USER root

RUN apt-get update && apt-get install -y -q \
        build-essential \
        && rm -rf /var/lib/apt/lists/*

COPY . /tmp/infomap

WORKDIR /tmp/infomap

RUN python -m pip install --no-cache-dir --upgrade pip build wheel backports.tarfile importlib_metadata \
        && make build-python \
        && wheel="$(ls dist/python/*.whl)" \
        && python -m pip install --no-cache-dir "$wheel[notebooks]" \
        && mkdir -p /home/$NB_USER/work/infomap-notebooks \
        && cp -a examples/notebooks/. /home/$NB_USER/work/infomap-notebooks/ \
        && rm -rf /tmp/infomap \
        && fix-permissions $CONDA_DIR \
        && fix-permissions /home/$NB_USER

USER $NB_UID

VOLUME /home/jovyan/work

WORKDIR /home/jovyan/work/infomap-notebooks
