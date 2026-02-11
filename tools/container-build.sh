#!/bin/bash
#
# Local container build script - mirrors the CI container builds
#
# Usage:
#   ./tools/container-build.sh              # Build all 64-bit containers
#   ./tools/container-build.sh alpine       # Build only Alpine containers (64-bit)
#   ./tools/container-build.sh debian       # Build only Debian containers (64-bit)
#   ./tools/container-build.sh fedora       # Build only Fedora containers (64-bit)
#   ./tools/container-build.sh i386         # Build all 32-bit containers
#   ./tools/container-build.sh alpine:3.21  # Build specific container
#   ./tools/container-build.sh --all        # Build everything (64-bit + 32-bit)
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Container definitions matching CI (64-bit)
ALPINE_CONTAINERS="alpine:3.21 alpine:3.20"
DEBIAN_CONTAINERS="debian:bookworm debian:bullseye"
FEDORA_CONTAINERS="fedora:41 fedora:40"

# 32-bit containers (i386)
I386_ALPINE_CONTAINERS="i386/alpine:3.21 i386/alpine:3.20"
I386_DEBIAN_CONTAINERS="i386/debian:bookworm i386/debian:bullseye"
I386_CONTAINERS="$I386_ALPINE_CONTAINERS $I386_DEBIAN_CONTAINERS"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_status() {
    echo -e "${GREEN}[BUILD]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

get_install_cmd() {
    local container="$1"
    # Strip i386/ prefix for matching
    local base_container="${container#i386/}"

    case "$base_container" in
        alpine:*)
            echo "apk add --no-cache build-base python3-dev libevent-dev openssl-dev pkgconf linux-headers"
            ;;
        debian:*)
            echo "apt-get update && apt-get install -y build-essential python3-dev libevent-dev libssl-dev pkg-config"
            ;;
        fedora:*)
            echo "dnf install -y gcc gcc-c++ make python3-devel libevent-devel openssl-devel pkg-config"
            ;;
        *)
            print_error "Unknown container: $container"
            exit 1
            ;;
    esac
}

build_container() {
    local container="$1"
    local install_cmd
    local platform_flag=""

    install_cmd=$(get_install_cmd "$container")

    # Check if this is a 32-bit build
    if [[ "$container" == i386/* ]]; then
        platform_flag="--platform linux/386"
    fi

    print_status "Building in $container..."

    docker run --rm $platform_flag \
        -v "$PROJECT_DIR:/src:ro" \
        -w /src \
        "$container" \
        sh -c "
            $install_cmd && \
            cp -r /src /build && \
            cd /build/Samples/Linux/SingleMain && \
            make clean && \
            make
        "

    if [ $? -eq 0 ]; then
        print_status "$container: SUCCESS"
        return 0
    else
        print_error "$container: FAILED"
        return 1
    fi
}

show_usage() {
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  all          Build all 64-bit containers (default)"
    echo "  alpine       Build Alpine containers (64-bit)"
    echo "  debian       Build Debian containers (64-bit)"
    echo "  fedora       Build Fedora containers (64-bit)"
    echo "  i386         Build all 32-bit containers"
    echo "  i386-alpine  Build Alpine 32-bit containers"
    echo "  i386-debian  Build Debian 32-bit containers"
    echo "  --all        Build everything (64-bit + 32-bit)"
    echo "  <image:tag>  Build specific container (e.g., alpine:3.21, i386/debian:bookworm)"
    echo ""
    echo "Examples:"
    echo "  $0                    # Build all 64-bit"
    echo "  $0 alpine             # Build Alpine 64-bit only"
    echo "  $0 i386               # Build all 32-bit"
    echo "  $0 --all              # Build everything"
    echo "  $0 i386/alpine:3.21   # Build specific 32-bit container"
}

# Determine which containers to build
CONTAINERS=""
case "${1:-all}" in
    all)
        CONTAINERS="$ALPINE_CONTAINERS $DEBIAN_CONTAINERS $FEDORA_CONTAINERS"
        ;;
    --all)
        CONTAINERS="$ALPINE_CONTAINERS $DEBIAN_CONTAINERS $FEDORA_CONTAINERS $I386_CONTAINERS"
        ;;
    alpine)
        CONTAINERS="$ALPINE_CONTAINERS"
        ;;
    debian)
        CONTAINERS="$DEBIAN_CONTAINERS"
        ;;
    fedora)
        CONTAINERS="$FEDORA_CONTAINERS"
        ;;
    i386)
        CONTAINERS="$I386_CONTAINERS"
        ;;
    i386-alpine)
        CONTAINERS="$I386_ALPINE_CONTAINERS"
        ;;
    i386-debian)
        CONTAINERS="$I386_DEBIAN_CONTAINERS"
        ;;
    alpine:*|debian:*|fedora:*|i386/*)
        CONTAINERS="$1"
        ;;
    -h|--help)
        show_usage
        exit 0
        ;;
    *)
        echo "Unknown option: $1"
        echo ""
        show_usage
        exit 1
        ;;
esac

# Check if docker is available
if ! command -v docker &> /dev/null; then
    print_error "docker not found. Please install docker first."
    exit 1
fi

# Run builds
FAILED=""
PASSED=""

for container in $CONTAINERS; do
    echo ""
    if build_container "$container"; then
        PASSED="$PASSED $container"
    else
        FAILED="$FAILED $container"
    fi
done

# Summary
echo ""
echo "=========================================="
echo "Build Summary"
echo "=========================================="

if [ -n "$PASSED" ]; then
    print_status "Passed:$PASSED"
fi

if [ -n "$FAILED" ]; then
    print_error "Failed:$FAILED"
    exit 1
fi

print_status "All builds passed!"
