#!/bin/sh

rm -rf dist

docker build --pull -t dqcsim-openql-mapper-wheel . && \
docker run -it --name dqcsim-openql-mapper-wheel dqcsim-openql-mapper-wheel && \
docker cp dqcsim-openql-mapper-wheel:/io/dist/ .

docker rm dqcsim-openql-mapper-wheel
