# Étape de compilation pour le programme Go
FROM golang:1.23 AS go-builder
WORKDIR /app
COPY app/ .
RUN CGO_ENABLED=0 go build -o /api/app ./cmd/main.go

# Étape de compilation pour le programme C++ avec DPP
FROM ubuntu:24.04 AS cpp-builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libssl-dev \
    zlib1g-dev \
    libopus-dev \
    clang \
    pkg-config \
    libsodium-dev

# Clone DPP
RUN git clone https://github.com/brainboxdotcc/DPP.git /dpp && \
    cd /dpp && \
    git checkout c6bcab5b4632fe35e32e63e3bc813e9e2cd2893e && \
    git submodule update --init --recursive

# Build DPP (shared)
RUN mkdir -p /dpp/build && \
    cd /dpp/build && \
    cmake .. \
    -DDPP_BUILD_TEST=OFF && \
    make -j$(nproc) && \
    make install

# Build user bot
WORKDIR /src
COPY bot/ .

RUN mkdir build && cd build && \
    cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=/usr/local && \
    make -j$(nproc)

# Étape finale d'exécution
FROM ubuntu:24.04
WORKDIR /app

# Install runtime deps
RUN apt-get update && apt-get install --reinstall -y \
    libssl3 \
    zlib1g \
    libopus0 \
    libsodium23 \
    ca-certificates \
    && apt-get clean
# Copie des binaires
COPY --from=go-builder /api/app ./app
COPY --from=cpp-builder /src/build/discord-bot ./discord-bot
COPY --from=cpp-builder /usr/local/lib/ /usr/local/lib/

RUN chmod +x ./app ./discord-bot

ENV LD_LIBRARY_PATH=/usr/local/lib

EXPOSE 2030

ENTRYPOINT ["./app"]
