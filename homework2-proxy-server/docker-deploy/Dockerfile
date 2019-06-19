FROM gcc
RUN mkdir /proxy-app
RUN apt-get update && apt-get -y install g++ make
WORKDIR /proxy-app
ADD ./proxy-app /proxy-app
