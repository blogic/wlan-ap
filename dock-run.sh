#!/bin/bash -ex

tag=$(echo ${PWD} | tr / - | cut -b2- | tr A-Z a-z)
groups=$(id -G | xargs -n1 echo -n " --group-add ")
params="-v ${PWD}:${PWD} --rm -w ${PWD} -u"$(id -u):$(id -g)" $groups -v/etc/passwd:/etc/passwd -v/etc/group:/etc/group -v ${HOME}/.gitconfig:/etc/gitconfig ${tag}"

docker build --tag=${tag} .
echo $params
docker run -t $params $@
