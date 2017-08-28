FROM ubuntu:14.04
MAINTAINER ilufei "m.lufei@qq.com"

ENV DEBIAN_FRONTEND noninteractive
#RUN echo "deb http://archive.ubuntu.com/ubuntu precise main universe" > /etc/apt/sources.list
RUN apt-get update -q
RUN apt-get upgrade --yes --force-yes

RUN apt -y install build-essential git subversion quilt gawk unzip python wget zlib1g-dev libncurses5-dev fakeroot ca-certificates openssh-server vim

RUN mkdir /home/builder

RUN useradd --home-dir /home/builder --shell /bin/bash --no-create-home -p builder builder
USER builder

ENV HOME /home/builder
WORKDIR /home/builder
# 每个Dockerfile中只能有一个 ENTRYPOINT ，当指定多个时，只有最后一个起效。
#ENTRYPOINT command param1 param2
ADD ./git-shell.sh /home/buidler/
