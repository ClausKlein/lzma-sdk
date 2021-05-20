cmake_minimum_required(VERSION 3.11...3.20)

# use ccache if found
find_program(CCACHE_EXECUTABLE "ccache" HINTS /usr/local/bin /opt/local/bin)
if(CCACHE_EXECUTABLE AND NOT CMAKE_TOOLCHAIN_FILE)
  message(STATUS "use ccache")
  set(CMAKE_CXX_COMPILER_LAUNCHER
      "${CCACHE_EXECUTABLE}"
      CACHE PATH "ccache" FORCE)
endif()

# ==============================
project(lzma LANGUAGES C CXX)
# ==============================

#
# usage: $ mkdir build && cd build $ cmake .. $ make DESTDIR=/tmp/lzma install
#

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
      # -Wno-gnu-zero-variadic-macro-arguments
      # -Wno-gnu-conditional-omitted-operand
      -Wno-sign-compare # TODO!
      -Wno-unused-parameter # TODO!
      # -Wno-ignored-attributes
    )
  endif()
endif()

#
# Where to put all the RUNTIME targets when built.  This variable is used to
# initialize the RUNTIME_OUTPUT_DIRECTORY property on all the targets.
#
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

add_executable(
  lzma
  main.cpp
  Alloc.c
  LzFind.c
  LzFindMt.c
  LzmaDec.c
  LzmaEnc.c
  LzmaLib.c
  # sha384.c
  Threads.c
  Alloc.h
  LzFind.h
  LzFindMt.h
  LzHash.h
  LzmaDec.h
  LzmaEnc.h
  LzmaLib.h
  # mysha384.h
  Threads.h
  Types.h)

# install the bin
install(TARGETS lzma DESTINATION bin)

option(USE_CPPCHECK "Looking for cppcheck program ..." ON)
if(USE_CPPCHECK)
  find_program(CPPCHECK cppcheck)
  if(CPPCHECK)
    find_program(HTMLREPORT cppcheck-htmlreport)
    if(HTMLREPORT)
      message(
        STATUS
          "cppchek found at '${CPPCHECK}'; you may use target 'cppcheck' to run it!"
      )
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
        VERBATIM)
    endif()
  endif()
endif()

include(CMakePrintSystemInformation)
message("CMAKE_CXX_LIBRARY_ARCHITECTURE ${CMAKE_CXX_LIBRARY_ARCHITECTURE}")
