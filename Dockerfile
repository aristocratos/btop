# ==============================================================================
# STAGE 1: Build stage (using a compiler image with GCC 14+)
# ==============================================================================
FROM gcc:14-bookworm AS builder

# Install build dependencies
# lowdown is used for generating the man page, but it's optional. 
# Installing it ensures we get a complete build with man pages.
RUN apt-get update && apt-get install -y --no-install-recommends \
    coreutils \
    sed \
    git \
    make \
    lowdown \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# Copy the entire workspace into the build directory
COPY . .

# Clean any existing artifacts and build btop
# GPU_SUPPORT=true is the default on Linux x86_64 but explicitly setting it guarantees it.
# We do not use STATIC=true because dynamic loading of NVML library (libnvidia-ml.so)
# is required for Nvidia GPU monitoring.
RUN make clean && make GPU_SUPPORT=true

# Create an install structure inside /install
RUN make install DESTDIR=/install PREFIX=/usr

# ==============================================================================
# STAGE 2: Export stage (scratch image to output only the compiled binary to host)
# ==============================================================================
FROM scratch AS binary
COPY --from=builder /build/bin/btop /btop

# ==============================================================================
# STAGE 3: Runtime stage (slim image for running btop inside docker)
# ==============================================================================
FROM ubuntu:24.04 AS runtime

# Install runtime dependencies
# btop runs directly in the terminal, it just needs a UTF-8 locale and basic packages
RUN apt-get update && apt-get install -y --no-install-recommends \
    locales \
    lm-sensors \
    && rm -rf /var/lib/apt/lists/*

# Set up UTF-8 locale (required for btop's beautiful graphs)
RUN locale-gen en_US.UTF-8
ENV LANG=en_US.UTF-8 \
    LANGUAGE=en_US:en \
    LC_ALL=en_US.UTF-8

# Copy btop installation from builder
COPY --from=builder /install/ /

# Define environment variables to enable NVIDIA GPUs inside the container at runtime
ENV NVIDIA_VISIBLE_DEVICES=all \
    NVIDIA_DRIVER_CAPABILITIES=all

# Set entrypoint to run btop
ENTRYPOINT ["/usr/bin/btop"]
