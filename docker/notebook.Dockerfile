ARG BASE_CONTAINER=jupyter/datascience-notebook
FROM $BASE_CONTAINER

RUN pip install infomap

VOLUME /me

WORKDIR /me

USER $NB_UID

ENTRYPOINT ["jupyter"]
CMD ["lab", "--LabApp.token=''"]