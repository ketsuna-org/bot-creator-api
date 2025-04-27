# Étape de compilation pour le programme Go
FROM golang:1.23-alpine AS go-builder
WORKDIR /app
COPY app/ .
RUN CGO_ENABLED=0 go build -o /api/app ./cmd/main.go

# Étape de compilation pour le programme C++ avec DPP
FROM alpine:3.21 AS cpp-builder
RUN apk add --no-cache build-base cmake git openssl-dev zlib-dev opus-dev clang

# Clonage de DPP
RUN git clone https://github.com/brainboxdotcc/DPP.git /dpp && \
    cd /dpp && \
    git checkout c6bcab5b4632fe35e32e63e3bc813e9e2cd2893e && \
    git submodule update --init --recursive

# Construction de DPP
RUN mkdir -p /dpp/build && \
    cd /dpp/build && \
    cmake .. -DDPP_BUILD_TEST=OFF && \
    make -j$(nproc) && \
    make install && \
    cp -r /dpp/include/dpp /usr/include/ && \
    cp /usr/local/lib/libdpp* /usr/lib/

# Construction du bot utilisateur
WORKDIR /src
COPY bot/ .

# Construction en pointant vers DPP installé localement
RUN mkdir build && cd build && \
    cmake .. && \
    make -j$(nproc)

# Étape finale d'exécution
FROM alpine:3.21
WORKDIR /app

# Copie des binaires
COPY --from=go-builder /api ./api
COPY --from=cpp-builder /usr/local/lib/libdpp* /usr/lib/
COPY --from=cpp-builder /src/build/discord-bot ./bot/build/discord-bot

# Installation des dépendances runtime
RUN apk add --no-cache \
    libstdc++ \
    libssl3 \
    zlib \
    opus

RUN chmod +x ./api/app ./bot/build/discord-bot

ENTRYPOINT ["./api/app"]
