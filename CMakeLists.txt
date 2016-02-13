cmake_minimum_required(VERSION 3.1)
set(ENTITYX_PYTHON_MAJOR_VERSION 1)
set(ENTITYX_PYTHON_MINOR_VERSION 1)
set(ENTITYX_PYTHON_PATCH_VERSION 0)
set(ENTITYX_PYTHON_VERSION
    ${ENTITYX_PYTHON_MAJOR_VERSION}.${ENTITYX_PYTHON_MINOR_VERSION}.${ENTITYX_PYTHON_PATCH_VERSION})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

project(EntityX_Python)

message("EntityX_Python version ${ENTITYX_PYTHON_VERSION}")

if(NOT DEFINED CMAKE_MACOSX_RPATH)
  set(CMAKE_MACOSX_RPATH 0)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(ENTITYX_PYTHON_BUILD_TESTING true CACHE BOOL "Enable building of tests.")
set(ENTITYX_PYTHON_BUILD_SHARED false CACHE BOOL "Build shared libraries?")

# Library installation directory
if(NOT DEFINED CMAKE_INSTALL_LIBDIR)
    set(CMAKE_INSTALL_LIBDIR lib)
endif(NOT DEFINED CMAKE_INSTALL_LIBDIR)
set(libdir ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})

# Default compiler args
if (CMAKE_CXX_COMPILER_ID MATCHES "(GNU|.*Clang)")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic
        -Werror -Wall -Wextra -Wno-unused-parameter
        -Wno-error=unused-variable -Wno-error=sign-compare -std=c++11")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
    set(CMAKE_CXX_FLAGS_MINSIZEREL "-g -Os -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "-g -O2 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL 'MSVC')
    # /Zi - Produces a program database (PDB) that contains type information and symbolic debugging information for use with the debugger.
    # /FS - Allows multiple cl.exe processes to write to the same .pdb file
    # /DEBUG - Enable debug during linking
    # /Od - Disables optimization
    set(CMAKE_CXX_FLAGS_DEBUG "/Zi /FS /DEBUG /Od /MDd")
    # /Ox - Full optimization
    set(CMAKE_CXX_FLAGS_RELEASE "/Ox -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/Ox /Zi /FS /DEBUG")
endif()

include(${CMAKE_ROOT}/Modules/CheckIncludeFile.cmake)
include(CheckCXXSourceCompiles)

check_cxx_source_compiles(
"
#include <memory>

int main() {
    std::shared_ptr<int>();
}
"
ENTITYX_HAVE_CXX11_STDLIB
)

if (NOT ENTITYX_HAVE_CXX11_STDLIB)
    message("-- Not using -stdlib=libc++ (test failed to build)")
else ()
    message("-- Using -stdlib=libc++")
    if (CMAKE_CXX_COMPILER_ID MATCHES "(GNU|.*Clang)")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    endif()
endif ()

# Misc features
check_include_file("stdint.h" HAVE_STDINT_H)

macro(create_test TARGET_NAME SOURCE)
    add_executable(${TARGET_NAME} ${SOURCE})
    target_link_libraries(
        ${TARGET_NAME}
        entityx_python
        ${ARGN}
        )
    set_target_properties(${TARGET_NAME} PROPERTIES
        FOLDER "entityx/python/tests/")
    add_test(${TARGET_NAME} ${TARGET_NAME})
endmacro()

if (NOT CMAKE_BUILD_TYPE)
    message("-- Defaulting to release build (use -DCMAKE_BUILD_TYPE:STRING=Debug for debug build)")
    set(CMAKE_BUILD_TYPE "Release")
endif()

# Add Python
find_package(PythonLibs 2.7 REQUIRED)
message(status "** Python Include: ${PYTHON_INCLUDE_DIRS}")
message(status "** Python Libraries: ${PYTHON_LIBRARIES}")
if(NOT PYTHONLIBS_FOUND)
    set(PYTHON_ROOT "" CACHE PATH "PYTHON top-level directory")
    message("---> Python 2.7 directory not found. Set PYTHON_ROOT to Pythons's top-level path (containing \"include\" and \"lib\" directories).\n")
endif()

# HACK(SMA) : Always statically link Boost_Python.
SET(Boost_USE_STATIC_LIBS     ON)
SET(Boost_USE_MULTITHREADED    ON)
SET(Boost_USE_STATIC_RUNTIME     OFF)
find_package(Boost 1.59.0 COMPONENTS python filesystem system REQUIRED)
message(status "** Boost Include: ${Boost_INCLUDE_DIR}")
message(status "** Boost Libraries: ${Boost_LIBRARIES}")
if(NOT Boost_FOUND)
    set(BOOST_ROOT "" CACHE PATH "BOOST top-level directory")
    message("---> Boost 1.59 directory not found. Set BOOST_ROOT to Boost's top-level path (containing \"include\" and \"lib\" directories).\n")
endif()

# Add entityx
# TODO(SMA) : Update FindEntityX.cmake to find the version tag, we've tested this
# with version 1.1.2
# find_package(entityx 1.1.2 REQUIRED)
find_package(EntityX REQUIRED)
message(status "** EntityX Include: ${ENTITYX_INCLUDE_DIRS}")
message(status "** EntityX Libraries: ${ENTITYX_LIBRARIES}")
if(NOT entityx_FOUND)
    set(entityx_ROOT "" CACHE PATH "entityx top-level directory")
    message("---> entityx 1.1.2 directory not found. Set entityx_ROOT to Boost's top-level path (containing \"include\" and \"lib\" directories).\n")
endif()

set(ENTITYX_INSTALLED_PYTHON_PACKAGE_DIR ${PYTHON_ROOT}/Lib CACHE STRING "Python package directory")

# Define HAVE_ROUND for pymath.h
add_definitions(/DHAVE_ROUND)
# HACK(SMA): Always statically link boost & python
add_definitions(-DBOOST_STATIC -DBOOST_PYTHON_STATIC_LIB )

include_directories(${CMAKE_CURRENT_LIST_DIR})
include_directories(${PYTHON_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${ENTITYX_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIR})

# Things to install
set(install_libs entityx_python)
# Inclue headers here so they appear in visual studio.
set(sources entityx/python/PythonSystem.cc
            entityx/python/PythonSystem.h
            entityx/python/config.h)
add_library(entityx_python STATIC ${sources})
set_target_properties(entityx_python PROPERTIES DEBUG_POSTFIX -d FOLDER entityx)
target_link_libraries(entityx_python ${ENTITYX_LIBRARIES} ${Boost_LIBRARIES} ${PYTHON_LIBRARIES})

# Enable python shared builds (untested)
if (ENTITYX_PYTHON_BUILD_SHARED)
    message("-- Building shared libraries (-DENTITYX_PYTHON_BUILD_SHARED=0 to only build static librarires)")
    add_library(entityx_python_shared SHARED ${sources})
    target_link_libraries(entityx_python_shared)
    set_target_properties(entityx_python_shared PROPERTIES
        OUTPUT_NAME entityx_python
        DEBUG_POSTFIX -d
        VERSION ${ENTITYX_VERSION}
        SOVERSION ${ENTITYX_MAJOR_VERSION}
        FOLDER entityx/python/)
    list(APPEND install_libs entityx_python_shared)
endif (ENTITYX_PYTHON_BUILD_SHARED)

if (ENTITYX_PYTHON_BUILD_TESTING)
    enable_testing()
    add_definitions(-DENTITYX_PYTHON_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/entityx/python/")
    create_test(PythonSystem_test entityx/python/PythonSystem_test.cc)
endif (ENTITYX_PYTHON_BUILD_TESTING)


configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/entityx/python/config.h.in
    ${CMAKE_CURRENT_SOURCE_DIR}/entityx/python/config.h
)

install(
    DIRECTORY "entityx/python/"
    DESTINATION "include"
    FILES_MATCHING PATTERN "*.h"
    )

install(
    TARGETS ${install_libs}
    LIBRARY DESTINATION "${libdir}"
    ARCHIVE DESTINATION "${libdir}"
    )
