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
        exit 1
    fi
    java_version=$(java -version 2>&1 | head -n 1 | cut -d'"' -f2 | cut -d'.' -f1)
    if [ "$java_version" -lt "8" ]; then
        print_error "Java 8 or higher is required. Found version $java_version"
        exit 1
    fi
    print_status "Found Java version: $(java -version 2>&1 | head -n 1)"
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

    # Configure with CMake
    cd build
    cmake .. \
        -DCMAKE_BUILD_TYPE=$build_type \
        -DMILVUS_USE_JVECTOR=ON \
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
