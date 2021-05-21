# https://www.7-zip.org/sdk.html
# usage:
#$ cmake -B build -S .
#$ DESTDIR=/tmp/lzmc make -C build install
#

option(CMAKE_EXPORT_COMPILE_COMMANDS "for clang-tidy" ON)

cmake_minimum_required(VERSION 3.11...3.20)

# use ccache if found
find_program(CCACHE_EXECUTABLE "ccache" HINTS /usr/local/bin /opt/local/bin)
if(CCACHE_EXECUTABLE AND NOT CMAKE_TOOLCHAIN_FILE)
  message(STATUS "use ccache")
  set(CMAKE_CXX_COMPILER_LAUNCHER
      "${CCACHE_EXECUTABLE}"
      CACHE PATH "ccache" FORCE
  )
endif()

# ==============================
project(lzmc LANGUAGES C CXX)
# ==============================

# Where to put all the RUNTIME targets when built.  This variable is used to
# initialize the RUNTIME_OUTPUT_DIRECTORY property on all the targets.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

# ---------------------------------------------------------------------------------------
# Compiler config
# ---------------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(COMPILER_WARNINGS_ARE_ERRORS "To be pedantic! ;-)" ON)
if(COMPILER_WARNINGS_ARE_ERRORS)
  if(MSVC)
    # warning level 4 and all warnings as errors /WX
    add_compile_options(/W4)
  else()
    # lots of warnings and all warnings as errors
    add_compile_options(
      -Wall
      -Wextra
      -Werror
      -Wno-sign-compare # TODO!
      -Wno-unused-parameter # TODO!
    )
  endif()
endif()

# ---------------------------------------------------------------------------------------
add_executable(
  lzmc
  main.cpp
  Alloc.c
  Alloc.h
  LzFind.c
  LzFind.h
  LzHash.h
  LzmaDec.c
  LzmaDec.h
  LzmaEnc.c
  LzmaEnc.h
  LzmaLib.c
  LzmaLib.h
  Types.h
)

if(NOT MSVC)
  option(USE_7ZIP_ST "use 7Z code to compile lzmc" ON)
endif()

if(USE_7ZIP_ST)
  target_compile_definitions(lzmc PUBLIC _7ZIP_ST)
else()
  target_sources(lzmc PRIVATE LzFindMt.c LzFindMt.h Threads.c Threads.h)
endif()

add_executable(
  lzma
  Util/Lzma/LzmaUtil.c
  Alloc.c
  Alloc.h
  7zFile.c
  7zFile.h
  7zStream.c
  LzFind.c
  LzFind.h
  LzHash.h
  LzmaDec.c
  LzmaDec.h
  LzmaEnc.c
  LzmaEnc.h
  Types.h
)
target_compile_definitions(lzma PUBLIC _7ZIP_ST)

# ---------------------------------------------------------------------------------------
# test support
enable_testing()
add_test(NAME test_compress COMMAND lzmc c ${PROJECT_SOURCE_DIR}/README.rst README.lzmc)
add_test(NAME test_decompress COMMAND lzmc d README.lzmc README.rst)
add_test(NAME test_check COMMAND diff ${PROJECT_SOURCE_DIR}/README.rst README.rst)
add_test(NAME test_stdout COMMAND lzmc d README.lzmc)
add_test(NAME test_help COMMAND lzmc)

# ---------------------------------------------------------------------------------------
# install the bin
install(TARGETS lzmc DESTINATION bin)
install(TARGETS lzma DESTINATION bin)

# ---------------------------------------------------------------------------------------
option(USE_CPPCHECK "Looking for cppcheck program ..." ON)
if(USE_CPPCHECK)
  find_program(CPPCHECK cppcheck)
  if(CPPCHECK)
    find_program(HTMLREPORT cppcheck-htmlreport)
    if(HTMLREPORT)
      message(STATUS "cppchek found at '${CPPCHECK}'; you may use target 'cppcheck' to run it!")
      add_custom_target(
        cppcheck
        ${CPPCHECK}
        --platform=unix32
        --std=posix
        --verbose
        --force
        --enable=all
        --inconclusive
        --template=gcc
        --xml
        --xml-version=2
        .
        2>
        cppcheck.xml
        COMMAND ${HTMLREPORT} --file=cppcheck.xml --report-dir=cppcheck
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        VERBATIM
      )
    endif()
  endif()
endif()

# ---------------------------------------------------------------------------------------
include(CMakePrintSystemInformation)
# ---------------------------------------------------------------------------------------
