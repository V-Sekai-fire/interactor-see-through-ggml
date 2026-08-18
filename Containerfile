# The C++ half of the A/B, as one RunPod Serverless worker.
#
# Same two processes as the Python half and the same bus between them, so the two images differ
# in the implementation under test and in nothing else. That is what makes a timing difference
# between them attributable.
#
# iceoryx2 is built from source because there is no Linux release of the C ABI to install --
# the Python binding ships a wheel, the C one does not. Nothing links it even so: the harness
# dlopens it, so this stage produces a shared object the runtime image loads at start.
#
# Declared before the first FROM because that is the only scope a FROM can read an ARG from.
ARG TRANSPORT_IMAGE=ghcr.io/v-sekai-multiplayer-fabric/transport-runpod:main

FROM rust:1-slim-bookworm AS iceoryx

ARG ICEORYX2_VERSION=v0.9.3
RUN apt-get update && apt-get install -y --no-install-recommends \
      git cmake build-essential libclang-dev ca-certificates \
    && rm -rf /var/lib/apt/lists/*
RUN git clone --depth 1 --branch ${ICEORYX2_VERSION} \
      https://github.com/eclipse-iceoryx/iceoryx2.git /iceoryx2
WORKDIR /iceoryx2
RUN cargo build --release -p iceoryx2-ffi-c

FROM debian:bookworm-slim AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential cmake python3 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY CMakeLists.txt ./
COPY include ./include
COPY src ./src
COPY proof ./proof
COPY third_party ./third_party
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
# The gate, in the build. An image whose interactor would serve a 512px run is not one this
# endpoint should be able to produce, and that is cheaper to find here than in a job result.
RUN ./build/seethrough-decompose

# The transport layer's worker, taken from its own image rather than rebuilt, so there is one
# copy of the job loop.
FROM ${TRANSPORT_IMAGE} AS transport

FROM debian:bookworm-slim
# libcurl is what the worker speaks the RunPod webhooks with, and ca-certificates because every
# webhook is https -- a worker with no roots fails on the first job-take with nothing in the
# log that says why.
RUN apt-get update && apt-get install -y --no-install-recommends \
      libcurl4 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY --from=iceoryx /iceoryx2/target/release/libiceoryx2_ffi_c.so /usr/local/lib/
COPY --from=build /src/build/seethrough-interactor /usr/local/bin/
COPY --from=transport /usr/local/bin/rp-worker-bus /usr/local/bin/
COPY entrypoint.sh /usr/local/bin/entrypoint.sh

RUN ldconfig && chmod +x /usr/local/bin/entrypoint.sh && mkdir -p /tmp/iceoryx2

# No weights in the image. They are cached on the network volume RunPod mounts at
# /runpod-volume, written once and read by every worker in the data center.
ENV ST_WEIGHTS_DIR=/runpod-volume/see-through/weights \
    ST_OUT_DIR=/runpod-volume/see-through/out \
    WEFT_ICEORYX2_PATH=/usr/local/lib/libiceoryx2_ffi_c.so

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
