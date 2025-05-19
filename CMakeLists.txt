# Copyright (C) 2019-2023 Zilliz. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions# Set minimum required CMake version and project name
cmake_minimum_required(VERSION 3.20)
project(knowhere VERSION 0.1.0 LANGUAGES CXX C)

# Include toolchain file to set compiler flags and standard
include(cmake/toolchain.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/")
include(GNUInstallDirs)
include(ExternalProject)
include(cmake/utils/utils.cmake)
include(cmake/utils/compile_flags.cmake)
include(cmake/utils/platform_check.cmake)

knowhere_option(WITH_CUVS "Build with CUVS indexes" OFF)
if(WITH_CUVS)
  set(CMAKE_CUDA_ARCHITECTURES RAPIDS)
  include(cmake/libs/librapids.cmake)
  project(knowhere CXX C CUDA)
  include(cmake/libs/libcuvs.cmake)
endif()

knowhere_option(WITH_UT "Build with UT test" OFF)
knowhere_option(WITH_ASAN "Build with ASAN" OFF)
knowhere_option(WITH_DISKANN "Build with diskann index" OFF)
knowhere_option(WITH_BENCHMARK "Build with benchmark" OFF)
knowhere_option(WITH_COVERAGE "Build with coverage" OFF)
knowhere_option(WITH_CCACHE "Build with ccache" ON)
knowhere_option(WITH_PROFILER "Build with profiler" OFF)
knowhere_option(WITH_FAISS_TESTS "Build with Faiss unit tests" OFF)
knowhere_option(WITH_LIGHT "Build with light weight version" OFF)

# cardinal is an enterprise vector search engine and can only be enabled for
# cloud environment
knowhere_option(WITH_CARDINAL "Build with cardinal" OFF)
knowhere_option(CARDINAL_VERSION_FORCE_CHECKOUT
                "Force checkout cardinal version" OFF)
# this is needed for clang on ubuntu:20.04, otherwise the linked fails with
# 'undefined reference' error. fmt v9 was used by the time the error was
# encountered. clang on ubuntu:22.04 seems to be unaffected. gcc seems to be
# unaffected.
add_definitions(-DFMT_HEADER_ONLY)

# this is needed for clang on ubuntu:20.04, otherwise the linked fails with
# 'undefined reference' error. fmt v9 was used by the time the error was
# encountered. clang on ubuntu:22.04 seems to be unaffected. gcc seems to be
# unaffected.
add_definitions(-DFMT_HEADER_ONLY)

if(KNOWHERE_VERSION)
  message(STATUS "Building KNOWHERE version: ${KNOWHERE_VERSION}")
  add_definitions(-DKNOWHERE_VERSION=${KNOWHERE_VERSION})
endif()

if(WITH_CCACHE)
  find_program(CCACHE_FOUND ccache)
  if(CCACHE_FOUND)
    message(STATUS "Using ccache: ${CCACHE_FOUND}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_FOUND})
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE_FOUND})
    # let ccache preserve C++ comments, because some of them may be meaningful
    # to the compiler
    set(ENV{CCACHE_COMMENTS} "1")
  endif()
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR}/)

add_definitions(-DNOT_COMPILE_FOR_SWIG)

if(WITH_LIGHT)
  add_definitions(-DKNOWHERE_WITH_LIGHT)
endif()

include(cmake/utils/platform_check.cmake)
include(cmake/utils/compile_flags.cmake)
include(cmake/libs/libfaiss.cmake)
include(cmake/libs/libhnsw.cmake)

# Add JVector subdirectory with Panama support
option(WITH_JVECTOR "Build with JVector support" ON)
if(WITH_JVECTOR)
    # Find Java 23 with Panama support
    find_package(Java 23 REQUIRED COMPONENTS Development)
    include(UseJava)
    
    # Set Java options for Panama preview features
    set(JAVA_COMPILE_OPTIONS "--enable-preview" CACHE STRING "Java compile options for preview features")
    set(JAVA_RUNTIME_OPTIONS "--enable-preview --enable-native-access=ALL-UNNAMED" CACHE STRING "Java runtime options for preview features")
    set(ENV{JAVA_OPTS} ${JAVA_RUNTIME_OPTIONS})
    
    # Remove any Java flags from C++ compiler flags and ensure proper C++ flags
    string(REPLACE "--enable-preview" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(STRIP "${CMAKE_CXX_FLAGS}" CMAKE_CXX_FLAGS)
    
    # Set C++ standard and other necessary flags
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    
    # Set compile options for all targets
    add_compile_options(
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
        $<$<CONFIG:Debug>:-g -O0 -DDEBUG>
        -Wall -Wextra -Wpedantic -Werror
        -fPIC
        -fopenmp
    )
    
    # Override any remaining flags that might contain Java options
    string(REGEX REPLACE "-fenable-preview" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REGEX REPLACE "--enable-preview" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(STRIP "${CMAKE_CXX_FLAGS}" CMAKE_CXX_FLAGS)
    
    # Set clean C++ flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -fPIC -fopenmp")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
    
    # Force remove any remaining Java preview flags
    foreach(flag_var
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        if(${flag_var} MATCHES "-fenable-preview|--enable-preview")
            string(REGEX REPLACE "-fenable-preview" "" ${flag_var} "${${flag_var}}")
            string(REGEX REPLACE "--enable-preview" "" ${flag_var} "${${flag_var}}")
            string(STRIP "${${flag_var}}" ${flag_var})
        endif()
    endforeach()
    
    # Find JNI with Panama support
    find_package(JNI REQUIRED)
    
    if(JAVA_FOUND AND JNI_FOUND)
        message(STATUS "Java found: ${Java_JAVA_EXECUTABLE}")
        
        # Configure JNI paths for JDK 23
        if(DEFINED ENV{JAVA_HOME})
            # Add standard JNI and Panama includes
            include_directories(SYSTEM 
                $ENV{JAVA_HOME}/include
                $ENV{JAVA_HOME}/include/linux
                $ENV{JAVA_HOME}/include/jdk.incubator.foreign
            )
            
            # Find JVM library
            find_library(JAVA_JVM_LIBRARY
                NAMES jvm
                PATHS $ENV{JAVA_HOME}/lib/server
                      $ENV{JAVA_HOME}/lib/amd64/server
                      $ENV{JAVA_HOME}/jre/lib/amd64/server
                NO_DEFAULT_PATH
            )
            
            if(JAVA_JVM_LIBRARY)
                message(STATUS "Found JVM library: ${JAVA_JVM_LIBRARY}")
                set(JNI_LIBRARIES ${JAVA_JVM_LIBRARY} CACHE STRING "JNI libraries")
            else()
                message(WARNING "JVM library not found, JNI might not work properly")
            endif()
        endif()
        
        message(STATUS "Enabling JVector support with Panama")
        
        # Add compiler definitions for Panama
        add_definitions(-DPANAMA)
        add_compile_options(--enable-preview)
        add_link_options(-Wl,--enable-new-dtags,-rpath,'$ORIGIN/../lib')
        
        # Add JVector subdirectory
        add_subdirectory(thirdparty/jvector)
        
        # Add JVector library to the linker
        list(APPEND KNOWHERE_LINKER_LIBS knowhere_jvector)
        
        # Add JNI libraries to the linker
        list(APPEND KNOWHERE_LINKER_LIBS ${JNI_LIBRARIES})
        
        # Add JVM library path to runtime library path
        if(JAVA_JVM_LIBRARY)
            get_filename_component(JVM_LIBRARY_DIR ${JAVA_JVM_LIBRARY} DIRECTORY)
            set(CMAKE_INSTALL_RPATH "${JVM_LIBRARY_DIR};${CMAKE_INSTALL_RPATH}")
        endif()
        
        # Add JVector to the build
        add_definitions(-DKNOWHERE_WITH_JVECTOR)
    else()
        message(WARNING "JNI not found, JVector support will be disabled")
        set(WITH_JVECTOR OFF)
    endif()
else()
    message(STATUS "JVector support disabled by user")
endif()

include_directories(thirdparty/faiss)

# Find Boost
find_package(Boost REQUIRED CONFIG)
if(TARGET Boost::headers)
    # Modern CMake style
    set(Boost_INCLUDE_DIRS $<TARGET_PROPERTY:Boost::headers,INTERFACE_INCLUDE_DIRECTORIES>)
    set(Boost_LIBRARIES Boost::headers Boost::system Boost::filesystem)
else()
    # Fallback to old-style variables
    find_package(Boost REQUIRED COMPONENTS system filesystem)
    include_directories(${Boost_INCLUDE_DIRS})
endif()

find_package(OpenMP REQUIRED)

# Find Folly from Conan
find_package(folly CONFIG QUIET)
if(TARGET folly::folly)
    # Folly found via config file
    set(folly_LIBRARIES folly::folly)
    get_target_property(FOLLY_INCLUDE_DIRS folly::folly INTERFACE_INCLUDE_DIRECTORIES)
    include_directories(${FOLLY_INCLUDE_DIRS})
else()
    # Fallback: Create imported target for Folly
    message(STATUS "Folly not found via CONFIG, creating imported target")
    
    # Set Folly include directory
    set(FOLLY_INCLUDE_DIR "/home/mwolters138/.conan/data/folly/2023.10.30.09/milvus/dev/package/229e6d3ef2d1ca92ed80b186cbb86b4e80c370bd/include")
    set(FOLLY_LIBRARY "/home/mwolters138/.conan/data/folly/2023.10.30.09/milvus/dev/package/229e6d3ef2d1ca92ed80b186cbb86b4e80c370bd/lib/libfolly.a")
    
    # Create imported target
    add_library(folly::folly STATIC IMPORTED)
    set_target_properties(folly::folly PROPERTIES
        IMPORTED_LOCATION ${FOLLY_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${FOLLY_INCLUDE_DIR}
    )
    
    # Add dependencies
    find_package(Threads REQUIRED)
    find_package(ZLIB REQUIRED)
    find_package(OpenSSL REQUIRED)
    find_package(Boost REQUIRED COMPONENTS context filesystem program_options regex system thread)
    
    target_link_libraries(folly::folly INTERFACE
        Threads::Threads
        ZLIB::ZLIB
        OpenSSL::SSL
        OpenSSL::Crypto
        Boost::context
        Boost::filesystem
        Boost::program_options
        Boost::regex
        Boost::system
        Boost::thread
    )
    
    set(folly_LIBRARIES folly::folly)
    include_directories(${FOLLY_INCLUDE_DIR})
    message(STATUS "Created imported target folly::folly")
endif()

# Find other dependencies from Conan
# Custom nlohmann_json configuration
if(NOT TARGET nlohmann_json::nlohmann_json)
    add_library(nlohmann_json INTERFACE IMPORTED)
    set_target_properties(nlohmann_json PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "/home/mwolters138/.conan/data/nlohmann_json/3.11.2/_/_/package/5ab84d6acfe1f23c4fae0ab88f26e3a396351ac9/include/"
    )
    add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
endif()
find_package(glog CONFIG REQUIRED)
find_package(prometheus-cpp CONFIG REQUIRED)
if(NOT WITH_CUVS)
    find_package(fmt 9.1.0 CONFIG REQUIRED)
endif()
if(NOT WITH_LIGHT)
    find_package(opentelemetry-cpp CONFIG REQUIRED)
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OSX_DEPLOYMENT_TARGET
    "10.15"
    CACHE STRING "Minimum OS X deployment version" FORCE)

if(OPENMP_FOUND)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS
      "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()

if(WITH_COVERAGE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
endif()

if(NOT WITH_LIGHT)
  knowhere_file_glob(
    GLOB_RECURSE
    KNOWHERE_SRCS
    src/common/*.cc
    src/index/*.cc
    src/cluster/*.cc
    src/io/*.cc
    src/common/*.cu
    src/index/*.cu
    src/io/*.cu)
endif()

if(WITH_LIGHT)
  knowhere_file_glob(
    GLOB_RECURSE
    KNOWHERE_SRCS
    src/common/*.cc
    src/index/ivf/ivf.cc
    src/index/index_node_data_mock_wrapper.cc
    src/index/index_static.cc
    src/index/index.cc
    src/index/interrupt.cc
    src/index/sparse/*.cc
    src/io/*.cc
    src/index/index_factory.cc)
  knowhere_file_glob(GLOB_RECURSE KNOWHERE_TRACER_SRCS src/common/tracer.cc)
  list(REMOVE_ITEM KNOWHERE_SRCS ${KNOWHERE_TRACER_SRCS})
endif()

set(KNOWHERE_LINKER_LIBS "")

if(WITH_CARDINAL)
  add_definitions(-DKNOWHERE_WITH_CARDINAL)
  include(cmake/libs/libcardinal.cmake)
endif()

if(WITH_DISKANN)
  add_definitions(-DKNOWHERE_WITH_DISKANN)
  include(cmake/libs/libdiskann.cmake)
else()
  knowhere_file_glob(GLOB_RECURSE KNOWHERE_DISKANN_SRCS src/index/diskann/*.cc)
  list(REMOVE_ITEM KNOWHERE_SRCS ${KNOWHERE_DISKANN_SRCS})
endif()

knowhere_file_glob(GLOB_RECURSE KNOWHERE_GPU_SRCS src/index/gpu/flat_gpu/*.cc
                   src/index/gpu/ivf_gpu/*.cc)
list(REMOVE_ITEM KNOWHERE_SRCS ${KNOWHERE_GPU_SRCS})

if(NOT WITH_CUVS)
  knowhere_file_glob(GLOB_RECURSE KNOWHERE_CUVS_SRCS src/common/cuvs/*.cu
                     src/common/cuvs/*.cc src/index/gpu_cuvs/*.cc)
  list(REMOVE_ITEM KNOWHERE_SRCS ${KNOWHERE_CUVS_SRCS})
endif()

include_directories(src)
include_directories(include)

list(APPEND KNOWHERE_LINKER_LIBS Boost::boost)
list(APPEND KNOWHERE_LINKER_LIBS faiss)
list(APPEND KNOWHERE_LINKER_LIBS glog::glog)
list(APPEND KNOWHERE_LINKER_LIBS nlohmann_json::nlohmann_json)
list(APPEND KNOWHERE_LINKER_LIBS prometheus-cpp::core prometheus-cpp::push)
list(APPEND KNOWHERE_LINKER_LIBS fmt::fmt-header-only)
list(APPEND KNOWHERE_LINKER_LIBS Folly::folly)
if(NOT WITH_LIGHT)
  list(APPEND KNOWHERE_LINKER_LIBS opentelemetry-cpp::opentelemetry_trace)
  list(APPEND KNOWHERE_LINKER_LIBS
       opentelemetry-cpp::opentelemetry_exporter_ostream_span)
  list(APPEND KNOWHERE_LINKER_LIBS
       opentelemetry-cpp::opentelemetry_exporter_jaeger_trace)
  list(APPEND KNOWHERE_LINKER_LIBS
       opentelemetry-cpp::opentelemetry_exporter_otlp_grpc)
  list(APPEND KNOWHERE_LINKER_LIBS
       opentelemetry-cpp::opentelemetry_exporter_otlp_http)
endif()

add_library(knowhere SHARED ${KNOWHERE_SRCS})
add_dependencies(knowhere ${KNOWHERE_LINKER_LIBS})
if(WITH_CUVS)
  list(
    APPEND
    KNOWHERE_LINKER_LIBS
    raft::raft
    raft::raft_logger
    raft::raft_logger_impl
    rmm::rmm_logger
    rmm::rmm_logger_impl
    cuvs::cuvs
    CUDA::cublas
    CUDA::cusparse
    CUDA::cusolver)
endif()
target_link_libraries(knowhere PUBLIC ${KNOWHERE_LINKER_LIBS})
target_include_directories(knowhere PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(WITH_UT)
  add_subdirectory(tests/ut)
endif()

if(WITH_BENCHMARK)
  add_subdirectory(benchmark)
endif()

if(WITH_FAISS_TESTS)
  add_subdirectory(tests/faiss)
endif()

install(TARGETS knowhere
        DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/knowhere"
        DESTINATION "${CMAKE_INSTALL_PREFIX}/include")
