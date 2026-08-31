FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    debhelper \
    devscripts \
    dpkg-dev \
    dput \
    fakeroot \
    gnupg \
    curl \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

USER ubuntu
WORKDIR /work
CMD ["bash"]
