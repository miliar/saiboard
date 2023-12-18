FROM ubuntu:20.04
RUN apt-get update && apt-get install -y software-properties-common gcc && \
    add-apt-repository -y ppa:deadsnakes/ppa
RUN apt-get update && apt-get install -y python3.9 python3-distutils python3-pip python3-apt
ENV DEBCONF_NOWARNINGS yes
ENV DEBIAN_FRONTEND noninteractive
ENV APT_KEY_DONT_WARN_ON_DANGEROUS_USAGE DontWarn
RUN apt-get -y update \
    && apt-get -y upgrade
RUN apt-get install -y wget
RUN wget https://launchpad.net/ubuntu/+archive/primary/+files/libzip-dev_1.5.1-0ubuntu1_amd64.deb
RUN wget https://launchpad.net/ubuntu/+archive/primary/+files/libzip5_1.5.1-0ubuntu1_amd64.deb
# Install libzip5
# https://askubuntu.com/questions/1071789/how-to-update-libzip-to-1-2-on-ubuntu-18-04-and-php-7-2
RUN apt-get install -y unzip zip zlib1g-dev
RUN dpkg -i libzip-dev_1.5.1-0ubuntu1_amd64.deb libzip5_1.5.1-0ubuntu1_amd64.deb
RUN rm *deb
WORKDIR /workspace
RUN mkdir /workspace/katago
RUN cd /workspace/katago \
    && wget https://github.com/lightvector/KataGo/releases/download/v1.13.0/katago-v1.13.0-eigen-linux-x64.zip \
    && wget https://media.katagotraining.org/uploaded/networks/models/kata1/kata1-b18c384nbt-s6582191360-d3422816034.bin.gz
RUN cd /workspace/katago \
    && unzip katago-v1.13.0-eigen-linux-x64.zip \
    && chmod +x katago \
    && rm *zip
RUN rm -rf /var/cache/*
WORKDIR /app
COPY requirements.txt .
RUN pip3 install --no-cache-dir -r requirements.txt
COPY katago.py .
COPY main.py .
COPY analysis.cfg .
CMD ["python3", "main.py"]
