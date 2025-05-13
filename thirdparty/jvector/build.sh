#!/bin/bash
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Print with color
print_status() {
    echo -e "${GREEN}[✓] $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}[!] $1${NC}"
}

print_error() {
    echo -e "${RED}[✗] $1${NC}"
}

# Check if a command exists
check_command() {
    if ! command -v $1 &> /dev/null; then
        print_error "$1 is required but not installed."
        exit 1
    fi
    print_status "Found $1: $(command -v $1)"
}

# Check Java version
check_java() {
    if ! java -version 2>&1 | grep -q "version"; then
        print_error "Java is required but not installed."
        print_error "Please install OpenJDK 21:"
        print_error "sudo apt-get install -y openjdk-21-jdk-headless default-jdk-headless"
        exit 1
    fi
    java_version=$(java -version 2>&1 | head -n 1 | cut -d'"' -f2 | cut -d'.' -f1)
    if [ "$java_version" -lt "21" ]; then
        print_error "JDK 21 or higher is required. Found version $java_version"
        print_error "Please install OpenJDK 21:"
        print_error "sudo apt-get install -y openjdk-21-jdk-headless"
        exit 1
    fi
    print_status "Found Java version: $(java -version 2>&1 | head -n 1)"
    
    # Set JAVA_HOME if not already set
    if [ -z "$JAVA_HOME" ]; then
        if [ -d "/usr/lib/jvm/java-21-openjdk-amd64" ]; then
            export JAVA_HOME="/usr/lib/jvm/java-21-openjdk-amd64"
        else
            # Try to find Java home from java command
            java_path=$(readlink -f $(which java))
            java_home=$(dirname $(dirname $java_path))
            export JAVA_HOME="$java_home"
        fi
        print_status "Set JAVA_HOME to: $JAVA_HOME"
    else
        print_status "Using existing JAVA_HOME: $JAVA_HOME"
    fi
}

# Check CMake version
check_cmake() {
    local required_version="3.18"
    local cmake_version=$(cmake --version | head -n1 | cut -d" " -f3)
    if [ "$(printf '%s\n' "$required_version" "$cmake_version" | sort -V | head -n1)" != "$required_version" ]; then
        print_error "CMake version $required_version or higher is required. Found version $cmake_version"
        exit 1
    fi
    print_status "Found CMake version: $cmake_version"
}

# Verify Folly installation
build_folly_from_source() {
    print_status "Building Folly from source..."
    local folly_version="2024.01.15.00"
    local build_dir="/tmp/folly-build"
    
    # Create build directory
    rm -rf "$build_dir"
    mkdir -p "$build_dir"
    cd "$build_dir"
    
    # Download and extract Folly
    wget "https://github.com/facebook/folly/archive/v${folly_version}.tar.gz"
    tar xzf "v${folly_version}.tar.gz"
    cd "folly-${folly_version}"
    
    # Build and install
    mkdir -p _build && cd _build
    cmake -DCMAKE_BUILD_TYPE=Release ..
    make -j$(nproc)
    make install
    ldconfig
    
    # Clean up
    cd /
    rm -rf "$build_dir"
}

# Build prometheus-cpp from source
build_prometheus() {
    print_status "Building prometheus-cpp from source..."
    local build_dir="$(mktemp -d)"
    trap 'rm -rf "$build_dir"' EXIT

    # Clone prometheus-cpp
    cd "$build_dir"
    git clone --recursive https://github.com/jupp0r/prometheus-cpp.git
    cd prometheus-cpp

    # Create build directory
    mkdir -p _build
    cd _build

    # Configure and build
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=ON \
        -DENABLE_PUSH=OFF \
        -DENABLE_COMPRESSION=OFF

    # Build and install
    make -j$(nproc)
    make install
    ldconfig

    print_status "prometheus-cpp installed successfully"
}

# Build GTest from source
build_gtest() {
    print_status "Building GTest from source..."
    local orig_dir="$PWD"
    local build_dir="$(mktemp -d)"
    trap 'rm -rf "$build_dir"; cd "$orig_dir"' EXIT

    # Clone GTest
    cd "$build_dir" || {
        print_error "Failed to change to build directory"
        return 1
    }

    print_status "Cloning GTest..."
    git clone --depth 1 https://github.com/google/googletest.git || {
        print_error "Failed to clone GTest"
        return 1
    }

    cd googletest || {
        print_error "Failed to change to googletest directory"
        return 1
    }

    # Create build directory
    mkdir -p _build
    cd _build || {
        print_error "Failed to change to build directory"
        return 1
    }

    # Configure and build
    print_status "Configuring GTest..."
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DBUILD_SHARED_LIBS=ON || {
        print_error "Failed to configure GTest"
        return 1
    }

    # Build and install
    print_status "Building GTest..."
    make -j$(nproc) VERBOSE=1 || {
        print_error "Failed to build GTest"
        return 1
    }

    print_status "Installing GTest..."
    make install || {
        print_error "Failed to install GTest"
        return 1
    }

    # Update library cache
    ldconfig

    # Debug: List installed files
    print_status "Installed GTest files:"
    find /usr/local -name "*gtest*" -type f

    # Return to original directory
    cd "$orig_dir"
    print_status "GTest installed successfully"
}

# Verify GTest installation
verify_gtest() {
    local gtest_paths=(
        "/usr/local/lib/cmake/GTest/GTestConfig.cmake"
        "/usr/local/include/gtest/gtest.h"
        "/usr/local/lib/libgtest.so"
    )

    local found=false
    for path in "${gtest_paths[@]}"; do
        if [ -f "$path" ]; then
            print_status "Found GTest component: $path"
            found=true
        fi
    done

    if [ "$found" = false ]; then
        print_warning "GTest not found or incomplete installation, building from source..."
        build_gtest || {
            print_error "Failed to build GTest"
            exit 1
        }

        # Verify installation was successful
        local missing_files=false
        for path in "${gtest_paths[@]}"; do
            if [ ! -f "$path" ]; then
                print_error "Missing GTest component after installation: $path"
                missing_files=true
            fi
        done

        if [ "$missing_files" = true ]; then
            print_error "GTest installation verification failed"
            exit 1
        fi

        print_status "GTest installation verified successfully"
    fi
}

# Build opentelemetry-cpp from source
build_opentelemetry() {
    print_status "Building opentelemetry-cpp from source..."
    local build_dir="$(mktemp -d)"
    trap 'rm -rf "$build_dir"' EXIT

    # Clone opentelemetry-cpp
    cd "$build_dir"
    git clone --recursive https://github.com/open-telemetry/opentelemetry-cpp.git
    cd opentelemetry-cpp

    # Create build directory
    mkdir -p _build
    cd _build

    # Configure and build
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=ON \
        -DWITH_OTLP=OFF \
        -DWITH_EXAMPLES=OFF \
        -DWITH_METRICS_PREVIEW=ON

    # Build and install
    make -j$(nproc)
    make install
    ldconfig

    print_status "opentelemetry-cpp installed successfully"
}

# Verify prometheus-cpp installation
verify_prometheus() {
    if [ -f "/usr/local/lib/cmake/prometheus-cpp/prometheus-cpp-config.cmake" ]; then
        print_status "Found prometheus-cpp"
    else
        print_warning "prometheus-cpp not found, building from source..."
        build_prometheus
    fi
}

# Verify opentelemetry-cpp installation
verify_opentelemetry() {
    if [ -f "/usr/local/lib/cmake/opentelemetry-cpp/opentelemetry-cpp-config.cmake" ]; then
        print_status "Found opentelemetry-cpp"
    else
        print_warning "opentelemetry-cpp not found, building from source..."
        build_opentelemetry
    fi
}

# Verify Folly installation
verify_folly() {
    if pkg-config --exists libfolly; then
        local folly_version=$(pkg-config --modversion libfolly)
        print_status "Found Folly: $folly_version"
    else
        print_warning "Folly not found, building from source..."
        build_folly_from_source
    fi
    print_status "Found Folly: $(pkg-config --modversion libfolly)"
}

# Check system dependencies
# Install build dependencies
install_build_deps() {
    if [ "$EUID" -ne 0 ]; then
        print_error "Please run with sudo to install build dependencies"
        exit 1
    fi

    print_status "Installing build dependencies..."
    apt-get update
    apt-get install -y \
        build-essential \
        cmake \
        libboost-all-dev \
        libblas-dev \
        liblapack-dev \
        libopenblas-dev \
        libssl-dev \
        libgflags-dev \
        libgoogle-glog-dev \
        libdouble-conversion-dev \
        libevent-dev \
        libsnappy-dev \
        zlib1g-dev \
        liblz4-dev \
        liblzma-dev \
        libzstd-dev \
        libbz2-dev \
        libsodium-dev \
        libfmt-dev \
        pkg-config \
        git \
        ca-certificates \
        curl \
        wget \
        nlohmann-json3-dev \
        libgtest-dev
}

check_system_deps() {
    local os_id=$(cat /etc/os-release | grep ^ID= | cut -d= -f2)
    
    case $os_id in
        "ubuntu")
            # Install build dependencies
            install_build_deps
            
            # Build Folly from source
            build_folly_from_source
            ;;
        *)
            print_error "Unsupported OS: $os_id. This script currently only supports Ubuntu."
            exit 1
            ;;
    esac
}

# Check C++ compiler version
check_cpp() {
    if ! command -v g++ &> /dev/null; then
        print_error "g++ compiler is required but not installed."
        exit 1
    fi
    cpp_version=$(g++ --version | head -n1 | cut -d" " -f4 | cut -d"." -f1)
    if [ "$cpp_version" -lt "7" ]; then
        print_error "g++ version 7 or higher is required for C++17 support. Found version $cpp_version"
        exit 1
    fi
    print_status "Found g++ version: $(g++ --version | head -n1)"
}

# Check environment variables
check_env() {
    if [ -z "$JAVA_HOME" ]; then
        print_error "JAVA_HOME environment variable is not set"
        exit 1
    fi
    print_status "Found JAVA_HOME: $JAVA_HOME"
}

# Setup build environment
setup_env() {
    # Add Java library path
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${JAVA_HOME}/lib/server
    print_status "Updated LD_LIBRARY_PATH"
}

# Create build directory
setup_build_dir() {
    local build_dir="build"
    if [ -d "$build_dir" ]; then
        print_warning "Build directory already exists. Cleaning..."
        rm -rf "$build_dir"
    fi
    mkdir -p "$build_dir"
    print_status "Created build directory"
}

# Main build process
build() {
    local build_type=${1:-Release}
    local jobs=${2:-$(nproc)}

    print_status "Starting build process..."
    print_status "Build type: $build_type"
    print_status "Using $jobs parallel jobs"

    # Get absolute paths
    local script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    local knowhere_root="$(cd "$script_dir/../.." && pwd)"
    local current_dir="$(pwd)"

    # Store original directory
    local orig_dir="$PWD"
    trap 'cd "$orig_dir"' EXIT

    # Debug: Print paths
    print_status "Script directory: $script_dir"
    print_status "Knowhere root: $knowhere_root"
    print_status "Current directory: $current_dir"

    # Build Knowhere first
    cd "$knowhere_root"
    mkdir -p build
    cd build
    print_status "Building Knowhere in $(pwd)..."

    # Configure Knowhere
    cmake .. \
        -DCMAKE_BUILD_TYPE=$build_type \
        -DMILVUS_USE_JVECTOR=ON \
        -DKNOWHERE_BUILD_TESTS=ON

    # Build Knowhere
    make -j$jobs

    # Now build JVector
    cd "$script_dir"
    mkdir -p build
    cd build
    print_status "Building JVector in $(pwd)..."

    # Configure JVector
    cmake .. \
        -DCMAKE_BUILD_TYPE=$build_type \
        -DKNOWHERE_ROOT="$knowhere_root" \
        -DKNOWHERE_BUILD_TESTS=ON

    # Build JVector
    make -j$jobs

    # Run tests if enabled
    if [ "$KNOWHERE_BUILD_TESTS" = "ON" ]; then
        print_status "Running tests..."
        ctest --output-on-failure
    fi

    print_status "Build completed successfully!"
}

# Main script
main() {
    echo "=== JVector Build Script ==="
    echo "Checking prerequisites..."

    # Store original directory and environment
    local script_dir="$(cd "$(dirname "$0")" && pwd)"
    local orig_dir="$PWD"
    local orig_path="$PATH"
    local orig_java_home="$JAVA_HOME"
    local orig_ld_library_path="$LD_LIBRARY_PATH"

    # Always return to original directory on exit
    trap 'cd "$orig_dir"' EXIT

    # Change to script directory
    cd "$script_dir"

    # Check required tools
    check_command "java"
    check_command "cmake"
    check_command "g++"
    
    # Check versions
    check_java
    check_cmake
    check_cpp
    
    # Check system dependencies
    check_system_deps
    
    # Verify Folly installation
    verify_folly
    
    # Verify prometheus-cpp installation
    verify_prometheus
    
    # Verify opentelemetry-cpp installation
    verify_opentelemetry
    
    # Verify GTest installation
    verify_gtest
    
    # Check environment
    check_env
    
    # Setup environment
    setup_env
    
    # Debug: Print environment
    print_status "Script directory: $script_dir"
    print_status "Current directory: $PWD"
    print_status "PATH: $PATH"
    print_status "JAVA_HOME: $JAVA_HOME"
    print_status "LD_LIBRARY_PATH: $LD_LIBRARY_PATH"
    
    # Prepare build directory
    setup_build_dir
    
    # Start build process with preserved environment
    sudo -E PATH="$orig_path" \
         JAVA_HOME="$orig_java_home" \
         LD_LIBRARY_PATH="$orig_ld_library_path" \
         "$script_dir/build.sh" --build -t "$BUILD_TYPE" -j "$JOBS"
}

# Print help message
print_help() {
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  -t, --type TYPE    Build type (Debug|Release) [default: Release]"
    echo "  -j, --jobs N       Number of parallel jobs [default: number of CPUs]"
    echo "  -h, --help        Show this help message"
    echo ""
    echo "Environment variables:"
    echo "  JAVA_HOME         Java home directory"
    echo "  LD_LIBRARY_PATH   Library search path"
}

# Default values
BUILD_TYPE="Release"
JOBS=$(nproc)
DIRECT_BUILD=0

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -t|--type)
            if [ -z "$2" ] || [[ "$2" == -* ]]; then
                print_error "Missing build type after $1"
                exit 1
            fi
            BUILD_TYPE="$2"
            shift 2
            ;;
        -j|--jobs)
            if [ -z "$2" ] || [[ "$2" == -* ]]; then
                print_error "Missing job count after $1"
                exit 1
            fi
            JOBS="$2"
            shift 2
            ;;
        --build)
            DIRECT_BUILD=1
            shift
            ;;
        -h|--help)
            print_help
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            print_help
            exit 1
            ;;
    esac
done

# Validate build type
case $BUILD_TYPE in
    Debug|Release)
        ;;
    *)
        print_error "Invalid build type: $BUILD_TYPE"
        print_help
        exit 1
        ;;
esac

# Validate jobs
if ! [[ "$JOBS" =~ ^[0-9]+$ ]] || [ "$JOBS" -lt 1 ]; then
    print_error "Invalid job count: $JOBS"
    print_help
    exit 1
fi

# Either run build directly or go through main
if [ $DIRECT_BUILD -eq 1 ]; then
    build "$BUILD_TYPE" "$JOBS"
else
    main "$BUILD_TYPE" "$JOBS"
fi
