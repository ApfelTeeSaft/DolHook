# DolHook Build Environment
FROM debian:bullseye-slim

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    wget \
    git \
    bzip2 \
    libarchive-tools \
    && rm -rf /var/lib/apt/lists/*

# Install devkitPPC
RUN wget https://github.com/devkitPro/pacman/releases/download/v1.0.2/devkitpro-pacman.amd64.deb && \
    dpkg -i devkitpro-pacman.amd64.deb || true && \
    apt-get install -f -y && \
    rm devkitpro-pacman.amd64.deb

# Install GameCube development packages
RUN dkp-pacman -Sy --noconfirm gamecube-dev

# Set environment variables
ENV DEVKITPRO=/opt/devkitpro
ENV DEVKITPPC=/opt/devkitpro/devkitPPC
ENV PATH=$PATH:$DEVKITPPC/bin

# Set working directory
WORKDIR /work

# Default command
CMD ["make", "all"]