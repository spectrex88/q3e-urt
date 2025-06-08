ARG BASE_IMAGE=ubuntu:24.04
FROM $BASE_IMAGE

USER root
ENV TZ=UTC \
    DEBIAN_FRONTEND=noninteractive \
    LANG=en_US.UTF-8 \
    LANGUAGE=en_US:en \
    LC_ALL=en_US.UTF-8 \
    MALLOC_CHECK_=0 \
    GLIBC_TUNABLES="glibc.malloc.check=0"

RUN apt-get update && apt-get install -y \
    locales \
    tzdata \
    sudo \
    wget curl \
    unzip \
    nano vim \
    htop \
    net-tools iputils-ping \
    apt-transport-https ca-certificates gnupg \
    software-properties-common \
    lsb-release gpg tree \
    aria2 \
    strace \
    && locale-gen en_US.UTF-8 \
    && update-locale LANG=en_US.UTF-8 LANGUAGE=en_US:en LC_ALL=en_US.UTF-8 \
    && ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && apt-get install -y \
    git \
    cmake \
    build-essential \
    gcc-11 \
    g++-11 \
    make \
    gdb \
    lcov \
    pkg-config \
    libc6-dev \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 60 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 60 \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && apt-get install -y \
    mesa-common-dev \
    libxxf86dga-dev \
    libxrandr-dev \
    libxxf86vm-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    libxext6 \
    libxrender1 \
    libxtst6 \
    libxi6 \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && apt-get install -y \
    libasound2-dev \
    libsdl2-dev \
    libsdl2-image-dev \
    libsdl2-mixer-dev \
    libsdl2-ttf-dev \
    pulseaudio-utils \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && apt-get install -y \
    libcurl4-openssl-dev \
    libssl-dev \
    libhiredis-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && apt-get install -y \
    libbz2-dev \
    libffi-dev \
    libgdbm-dev \
    liblzma-dev \
    libncurses-dev \
    libreadline-dev \
    libsqlite3-dev \
    xz-utils \
    tk-dev \
    uuid-dev \
    zlib1g-dev \
    lib32z1 \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /usr/src/urt
