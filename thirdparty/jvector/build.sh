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

verify_folly() {
    if ! pkg-config --exists libfolly; then
        print_warning "Folly package not found, attempting to build from source..."
        if [ "$EUID" -ne 0 ]; then
            print_error "Please run with sudo to build Folly from source"
            exit 1
        fi
        build_folly_from_source
        
        # Verify again
        if ! pkg-config --exists libfolly; then
            print_error "Failed to build and install Folly"
            exit 1
        fi
    fi
    print_status "Found Folly: $(pkg-config --modversion libfolly)"
}

# Check system dependencies
# Setup repositories
setup_repos() {
    if [ "$EUID" -ne 0 ]; then
        print_error "Please run with sudo to setup repositories"
        exit 1
    fi

    # Add Facebook repository
    if [ ! -f /etc/apt/sources.list.d/facebook.list ]; then
        print_status "Adding Facebook repository..."
        wget -q https://download.opensuse.org/repositories/home:facebook/xUbuntu_22.04/Release.key -O- | apt-key add -
        echo "deb https://download.opensuse.org/repositories/home:facebook/xUbuntu_22.04/ ./" > /etc/apt/sources.list.d/facebook.list
        apt-get update
    fi
}

check_system_deps() {
    local os_id=$(cat /etc/os-release | grep ^ID= | cut -d= -f2)
    
    case $os_id in
        "ubuntu")
            # Setup repositories first
            setup_repos

            # Basic build tools and math libraries
            local deps=("build-essential" "libblas-dev" "liblapack-dev" "libopenblas-dev" "libboost-all-dev"
                      # Folly dependencies
                      "libssl-dev" "libgflags-dev" "libgoogle-glog-dev" "libdouble-conversion-dev"
                      "libfmt-dev" "libfolly-dev" "libevent-dev" "libsnappy-dev" "zlib1g-dev"
                      "liblz4-dev" "liblzma-dev" "libzstd-dev" "libbz2-dev" "libsodium-dev"
                      # Additional Facebook packages
                      "libfolly-dev" "libfizz-dev" "libwangle-dev" "libfmt-dev")
            local missing_deps=()
            
            for pkg in "${deps[@]}"; do
                if ! dpkg -l "$pkg" &> /dev/null; then
                    missing_deps+=("$pkg")
                fi
            done
            
            if [ ${#missing_deps[@]} -gt 0 ]; then
                print_warning "Missing required system dependencies: ${missing_deps[*]}"
                print_warning "Installing missing dependencies..."
                if [ "$EUID" -ne 0 ]; then
                    print_error "Please run with sudo to install dependencies:"
                    print_error "sudo apt-get update && sudo apt-get install -y ${missing_deps[*]}"
                    exit 1
                else
                    apt-get update
                    apt-get install -y "${missing_deps[@]}"
                fi
            fi
            ;;
        *)
            print_warning "Unsupported OS: $os_id. Please install BLAS, LAPACK, and OpenBLAS manually."
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

    # Get Knowhere root directory
    local script_dir=$(dirname "$(readlink -f "$0")")
    local knowhere_root=$(dirname "$(dirname "$script_dir")")
    print_status "Knowhere root: $knowhere_root"

    # First build Knowhere
    print_status "Building Knowhere..."
    cd "$knowhere_root"
    mkdir -p build && cd build
    cmake .. \
        -DCMAKE_BUILD_TYPE=$build_type \
        -DMILVUS_USE_JVECTOR=ON \
        -DKNOWHERE_BUILD_TESTS=ON
    make -j$jobs

    # Now build JVector
    print_status "Building JVector..."
    cd "$script_dir"
    mkdir -p build && cd build
    cmake .. \
        -DCMAKE_BUILD_TYPE=$build_type \
        -DKNOWHERE_ROOT="$knowhere_root" \
        -DKNOWHERE_BUILD_TESTS=ON

    # Build
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
    
    # Check environment
    check_env
    
    # Setup environment
    setup_env
    
    # Prepare build directory
    setup_build_dir
    
    # Start build process
    build "$@"
}

# Parse command line arguments
BUILD_TYPE="Release"
JOBS=$(nproc)

while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --jobs=*)
            JOBS="${1#*=}"
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --debug        Build in debug mode"
            echo "  --jobs=N       Use N parallel jobs (default: number of CPU cores)"
            echo "  --help         Show this help message"
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Run main function with arguments
main "$BUILD_TYPE" "$JOBS"
