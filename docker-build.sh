#!/usr/bin/env bash
set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

IMAGE_NAME="btop-nvidia"
BINARY_OUTPUT_DIR="./bin"

show_help() {
    echo -e "${BLUE}btop NVIDIA Docker Build & Run Helper${NC}"
    echo "Usage: $0 [action]"
    echo ""
    echo "Actions:"
    echo "  build         Build the Docker image (for running in-container)"
    echo "  export        Build and export the compiled 'btop' binary to host (${BINARY_OUTPUT_DIR}/btop)"
    echo "  run           Run the built btop image inside a Docker container (with NVIDIA support)"
    echo "  help          Show this help message"
    echo ""
}

build_image() {
    echo -e "${BLUE}[*] Building Docker image '${IMAGE_NAME}'...${NC}"
    docker build -t "${IMAGE_NAME}" .
    echo -e "${GREEN}[+] Docker image '${IMAGE_NAME}' built successfully.${NC}"
}

export_binary() {
    echo -e "${BLUE}[*] Compiling btop and exporting binary to host...${NC}"
    mkdir -p "${BINARY_OUTPUT_DIR}"
    
    # Compile the builder stage
    docker build --target builder -t btop-temp-builder .
    
    # Extract the binary using a temporary container
    echo -e "${BLUE}[*] Extracting binary to ${BINARY_OUTPUT_DIR}/btop...${NC}"
    CONTAINER_ID=$(docker create btop-temp-builder)
    docker cp "${CONTAINER_ID}:/build/bin/btop" "${BINARY_OUTPUT_DIR}/btop"
    docker rm "${CONTAINER_ID}"
    docker rmi btop-temp-builder
    
    if [ -f "${BINARY_OUTPUT_DIR}/btop" ]; then
        echo -e "${GREEN}[+] Binary compiled and exported to: ${BINARY_OUTPUT_DIR}/btop${NC}"
        echo -e "${YELLOW}[!] You can run this binary natively on your host with: ${BINARY_OUTPUT_DIR}/btop${NC}"
    else
        echo -e "${RED}[-][Error] Failed to export btop binary.${NC}"
        exit 1
    fi
}

run_container() {
    # Check if image exists
    if ! docker image inspect "${IMAGE_NAME}" >/dev/null 2>&1; then
        echo -e "${YELLOW}[!] Docker image '${IMAGE_NAME}' not found. Building it first...${NC}"
        build_image
    fi

    echo -e "${BLUE}[*] Starting btop in Docker container with NVIDIA GPU support...${NC}"
    echo -e "${YELLOW}[!] Note: Running with --pid=host, --net=host, and --privileged for full host system stats.${NC}"
    
    # Run btop with GPU capabilities, host PID namespace, host network, and host IPC
    docker run -it --rm \
        --gpus all \
        --pid host \
        --net host \
        --privileged \
        -e NVIDIA_VISIBLE_DEVICES=all \
        -e NVIDIA_DRIVER_CAPABILITIES=all \
        "${IMAGE_NAME}"
}

# Main command dispatcher
if [ $# -lt 1 ]; then
    show_help
    exit 0
fi

case "$1" in
    build)
        build_image
        ;;
    export)
        export_binary
        ;;
    run)
        run_container
        ;;
    help|--help|-h)
        show_help
        ;;
    *)
        echo -e "${RED}[-][Error] Unknown action: $1${NC}"
        show_help
        exit 1
        ;;
esac
