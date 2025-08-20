# Copyright (c) 2025 acrion innovations GmbH
# Authors: Stefan Zipproth, s.zipproth@acrion.ch
#
# This file is part of acrion image, see https://github.com/acrion/image
#
# acrion image is offered under a commercial and under the AGPL license.
# For commercial licensing, contact us at https://acrion.ch/sales. For AGPL licensing, see below.
#
# AGPL licensing:
#
# acrion image is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# acrion image is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with acrion image. If not, see <https://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.25.2)
cmake_policy(VERSION 3.25.2)

include(FetchContent)

FetchContent_Declare(
    acrion_cmake
    GIT_REPOSITORY https://github.com/acrion/cmake.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(acrion_cmake)
if (NOT acrion_cmake_POPULATED)
    FetchContent_MakeAvailable(acrion_cmake)
endif ()

# cbeam
if (NOT TARGET cbeam)
    FetchContent_Declare(
        cbeam
        GIT_REPOSITORY https://github.com/acrion/Cbeam.git
        GIT_TAG main
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(cbeam)
endif ()

include(${acrion_cmake_SOURCE_DIR}/set-cpp-version.cmake)

include(${acrion_cmake_SOURCE_DIR}/get-project-version-from-git-tag.cmake)
get_project_version_from_git_tag("origin/main")

project(acrion_image VERSION ${version_major}.${version_minor}.${version_patch} DESCRIPTION "nexuslua plugin providing basic image processing functions including I/O")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version_acrion_image_generated.hpp.cmake ${CMAKE_CURRENT_BINARY_DIR}/version_acrion_image_generated.hpp)

include(${acrion_cmake_SOURCE_DIR}/find-openmp.cmake)

add_library(${PROJECT_NAME} INTERFACE
    include/acrion/image/bitmap.hpp
    include/acrion/image/bitmap_data.hpp
    include/acrion/image/color.hpp
    include/acrion/image/interpolation.hpp
    include/acrion/image/mixable_scalar.hpp
    include/acrion/image/utility.hpp
    include/acrion/image/vector.hpp
    include/acrion/image/version_acrion_image.hpp
)

target_include_directories(${PROJECT_NAME}
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(${PROJECT_NAME} INTERFACE cbeam OpenMP::OpenMP_CXX)

include(${acrion_cmake_SOURCE_DIR}/do-logging.cmake)

project(acrion_image_test VERSION ${version_major}.${version_minor}.${version_patch} DESCRIPTION "unit test executable for acrion image framework")

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
    GIT_SHALLOW TRUE
)

if (WIN32)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif ()

FetchContent_MakeAvailable(googletest)
enable_testing()
add_executable(
    ${PROJECT_NAME}
    test/test.cpp
)

target_link_libraries(
    ${PROJECT_NAME}
    acrion_image

    GTest::gtest_main
)
include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME})

include(${acrion_cmake_SOURCE_DIR}/run-tests.cmake)
