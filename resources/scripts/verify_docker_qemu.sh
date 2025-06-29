#!/bin/bash

# --- Configuration ---
# Preferred order of architectures if the image is multi-arch.
# The script will pick the first matching architecture from this list.
PREFERRED_ARCHITECTURES=("arm64" "amd64" "armhf" "arm32v7" "armel" "arm32v6")
# --- End Configuration ---

# --- Input Validation ---
if [ -z "$1" ]; then
    echo "ERROR: Please provide the Docker image name as the first parameter."
    echo "Usage: $0 <docker_image_name>"
    echo "Example: $0 ghcr.io/awawa-dev/arm64/debian:bookworm"
    exit 1
fi

DOCKER_IMAGE="$1"

# --- Helper Functions ---

# Function to get the host's architecture
get_host_architecture() {
    local arch=$(uname -m)
    case "$arch" in
        x86_64) echo "amd64" ;;
        aarch64) echo "arm64" ;;
        armv7l) echo "armhf" ;; # Often corresponds to arm32v7 in Docker
        armv6l) echo "armel" ;; # Often corresponds to arm32v6 in Docker
        *) echo "$arch" ;; # Default fallback
    esac
}

# Function to extract the target Docker image architecture
get_target_docker_architecture() {
    local image="$1"
    local target_arch=""

    # Check if Docker is running
    if ! docker info &>/dev/null; then
        echo "ERROR: Docker is not running or you don't have access to it."
        echo "Please ensure the Docker daemon is running and your user has appropriate permissions."
        return 1 # Indicate an error
    fi

    # If it's a single-architecture image, try to get its architecture from "docker inspect"
    # This works if the image is already pulled locally.
    if docker inspect "${image}" &>/dev/null; then
        target_arch=$(docker inspect "${image}" --format '{{ .Architecture }}')
        # Normalize arch name (e.g., aarch64 -> arm64)
        case "$target_arch" in
            x86_64) target_arch="amd64" ;;
            aarch64) target_arch="arm64" ;;
            armv7l) target_arch="armhf" ;;
            armv6l) target_arch="armel" ;;
            *) ;; # Keep as is for others
        esac
    else
        echo "ERROR: Image ${image} does not exist locally."
        return 1 # Indicate an error
    fi

    if [ -z "${target_arch}" ]; then
        echo "ERROR: Failed to determine target Docker image architecture."
        return 1
    fi

    echo "$target_arch" # Return the found architecture
    return 0
}

# Function to check if qemu-user-static is registered
check_qemu_binfmt() {
    echo "--- Checking qemu-user-static registration ---"

    # Check if binfmt_misc is mounted
    if ! mountpoint -q /proc/sys/fs/binfmt_misc; then
        echo "  /proc/sys/fs/binfmt_misc IS NOT mounted. This is critical for QEMU emulation."
        return 1
    fi
    echo "  /proc/sys/fs/binfmt_misc is mounted."

    # Check for qemu-* entries
    if ! ls /proc/sys/fs/binfmt_misc/qemu-* &>/dev/null; then
        echo "  No qemu-* entries found in binfmt_misc."
        return 1
    fi
    echo "  qemu-* entries found in binfmt_misc."

    # Check for the qemu-user-static package (Debian/Ubuntu/RHEL/Fedora/CentOS)
    if command -v dpkg &>/dev/null; then
        echo "  Checking for 'qemu-user-static' package (Debian/Ubuntu)..."
        if ! dpkg -s qemu-user-static &>/dev/null; then
            echo "  'qemu-user-static' package IS NOT installed."
            return 1
        fi
        echo "  'qemu-user-static' package is installed."
    elif command -v rpm &>/dev/null; then
        echo "  Checking for 'qemu-user-static' package (RHEL/Fedora/CentOS)..."
        if ! rpm -q qemu-user-static &>/dev/null; then
            echo "  'qemu-user-static' package IS NOT installed."
            return 1
        fi
        echo "  'qemu-user-static' package is installed."
    else
        echo "  Neither dpkg nor rpm found. Cannot check for package installation directly."
        echo "  Relying solely on binfmt_misc entries."
    fi

    echo "--- Conclusion: multiarch/qemu-user-static appears to be correctly set up. ---"
    return 0 # Success
}

# --- Main Script Logic ---

echo "--- Starting QEMU Verification for Docker ---"

# 1. Get host architecture
HOST_ARCH=$(get_host_architecture)
echo "Host Architecture: ${HOST_ARCH}"

# 2. Extract target Docker image architecture
TARGET_DOCKER_ARCH=$(get_target_docker_architecture "${DOCKER_IMAGE}")
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to determine target Docker image architecture for '${DOCKER_IMAGE}'. Please check the image name or Docker connection."
    exit 1
fi
echo "Target Docker Image Architecture: ${TARGET_DOCKER_ARCH}"

# 3. Conditional QEMU check
if [ "$HOST_ARCH" = "$TARGET_DOCKER_ARCH" ]; then
    echo "Host architecture (${HOST_ARCH}) matches target Docker image architecture. QEMU emulation is not required."
    echo "Ready to build/run the native image."
    exit 0
else
    echo "Host architecture (${HOST_ARCH}) differs from target Docker image architecture (${TARGET_DOCKER_ARCH}). QEMU emulation is required."
    if check_qemu_binfmt; then
        echo "QEMU user static emulation is ready for cross-architecture HyperHDR compiling."
        echo "You can proceed with your Docker operations."
        exit 0
    else
        echo "---------------------------------------------------------"
        echo "WARNING: QEMU user static emulation is not detected or not fully configured."
        echo "This is required for cross-architecture HyperHDR compiling for different architectures (e.g., ${TARGET_DOCKER_ARCH} on ${HOST_ARCH})."
        echo ""
        echo "Please ensure you have 'qemu-user-static' installed and registered."
        echo "You can typically fix this by running the following commands on your host system:"
        echo ""
        echo "  sudo apt update && sudo apt install -y qemu-user-static"
        echo "  sudo docker run --rm --privileged multiarch/qemu-user-static --reset -p yes"
        echo ""
        echo "For Fedora/CentOS/RHEL, use 'sudo dnf install -y qemu-user-static' or 'sudo yum install -y qemu-user-static' instead of apt."
        echo "Then run the 'sudo docker run --rm --privileged multiarch/qemu-user-static --reset -p yes' command."
        echo "---------------------------------------------------------"
        exit 1 # Exit with an error code to indicate failure
    fi
fi