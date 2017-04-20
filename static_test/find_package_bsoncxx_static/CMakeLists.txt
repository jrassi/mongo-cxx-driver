# Copyright 2016 MongoDB Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Demonstrates how to use the CMake 'find_package' mechanism to locate
# and build against libmongocxx and libbsoncxx.

cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

if(POLICY CMP0025)
  cmake_policy(SET CMP0025 NEW)
endif()

project(MONGO_CXX_DRIVER LANGUAGES CXX)

# Enforce the C++ standard, and disable extensions
if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 11)
endif()

set(CMAKE_CXX_EXTENSIONS OFF)

# NOTE: For this to work, the CMAKE_PREFIX_PATH variable must be set to point to the directory that
# was used as the argument to CMAKE_INSTALL_PREFIX when building libmongocxx and libbsoncxx.
find_package(libbsoncxx-static)

#message(STATUS "LIBBSONCXX_LIBRARY_DIRS: ${LIBBSONCXX_LIBRARY_DIRS}")
#link_directories(
#  ${LIBBSONCXX_LIBRARY_DIRS}
#)

add_executable(hello_bsoncxx hello_bsoncxx.cpp)

target_include_directories(hello_bsoncxx
  PRIVATE ${LIBBSONCXX_INCLUDE_DIRS}
)

message(STATUS "LIBBSONCXX_LIBRARIES: ${LIBBSONCXX_LIBRARIES}")
target_link_libraries(hello_bsoncxx
  PRIVATE ${LIBBSONCXX_LIBRARIES}
)
