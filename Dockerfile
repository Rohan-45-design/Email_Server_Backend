FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
    build-essential cmake git pkg-config ca-certificates \
    libssl-dev libyaml-cpp-dev \
    wget curl \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy project
COPY . /app

# Build
RUN mkdir -p build && cd build \
 && cmake -DCMAKE_BUILD_TYPE=Release .. \
 && cmake --build . -- -j$(nproc)

# Expose ports matching docker-compose.yml expectations
# Note: Server config should use ports 25, 587, 143, 993, 8080, 9090
EXPOSE 25 587 143 993 8080 9090

VOLUME ["/app/data", "/app/config"]

WORKDIR /app

ENTRYPOINT ["/app/build/mailserver"]
