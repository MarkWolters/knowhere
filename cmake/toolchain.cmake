# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -fPIC -fopenmp -Wall -Wextra -Wpedantic -Werror")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")

# Remove any Java-related flags that might have been set
foreach(flag_var
    CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
    CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
    string(REGEX REPLACE "-fenable-preview" "" ${flag_var} "${${flag_var}}")
    string(REGEX REPLACE "--enable-preview" "" ${flag_var} "${${flag_var}}")
    string(STRIP "${${flag_var}}" ${flag_var})
    set(${flag_var} "${${flag_var}}" CACHE STRING "" FORCE)
    message(STATUS "${flag_var}: ${${flag_var}}")
endforeach()
