ARG BASE_CONTAINER=jupyter/datascience-notebook
FROM $BASE_CONTAINER

USER root

RUN mkdir -p /me/Infomap && \
    chown -R $NB_UID /me

USER $NB_UID

COPY . /me/Infomap/

WORKDIR /me/Infomap

RUN make all

WORKDIR /me

RUN pip -vvv --no-cache-dir install --upgrade -I --index-url https://test.pypi.org/simple/ infomap
