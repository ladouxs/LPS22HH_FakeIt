# Host integration tests (DocTest + FakeIt). CMake project independent of the firmware:
# it does not use the ARM toolchain.
cmake_minimum_required(VERSION 3.20)
project(lps22hh_host_tests LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ASan + UBSan for the driver and tests, only if the toolchain can COMPILE AND LINK them
# (runtimes—libasan/libubsan packages—are sometimes missing). Otherwise: warning, without sanitizers.
#   DISABLED BY DEFAULT. Enable with: -DLPS_SANITIZE=ON
#   -DLPS_SANITIZERS=undefined   selects the list (default: address,undefined)
option(LPS_SANITIZE "Compile avec AddressSanitizer + UBSan si disponibles" OFF)
set(LPS_SANITIZERS "address,undefined" CACHE STRING "Liste passée à -fsanitize=")
if(LPS_SANITIZE)
  include(CheckCXXSourceCompiles)
  set(CMAKE_REQUIRED_FLAGS        "-fsanitize=${LPS_SANITIZERS}")
  set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=${LPS_SANITIZERS}")
  check_cxx_source_compiles("int main() { return 0; }" LPS_SANITIZERS_WORK)
  unset(CMAKE_REQUIRED_FLAGS)
  unset(CMAKE_REQUIRED_LINK_OPTIONS)
  if(LPS_SANITIZERS_WORK)
    add_compile_options(-fsanitize=${LPS_SANITIZERS} -fno-omit-frame-pointer)
    add_link_options(-fsanitize=${LPS_SANITIZERS})
  else()
    message(WARNING "-fsanitize=${LPS_SANITIZERS} non utilisable avec cette toolchain "
                    "(runtime manquant ? ex. paquets libasan/libubsan) : tests construits SANS sanitizers.")
  endif()
endif()

include(FetchContent)

# Shared cache outside the build directory (CI, clean rebuild):
#   -DFETCHCONTENT_BASE_DIR=$HOME/.cache/fetchcontent   or   -DFETCHCONTENT_FULLY_DISCONNECTED=ON

# doctest and FakeIt are header-only: SOURCE_SUBDIR is missing => their CMakeLists.txt is not
# added. Essential with CMake >= 4, which rejects the cmake_minimum_required < 3.5 found in doctest 2.4.11.
FetchContent_Declare(doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG        v2.4.11
  SOURCE_SUBDIR  _no_cmake)

FetchContent_Declare(fakeit
  GIT_REPOSITORY https://github.com/eranpeer/FakeIt.git
  GIT_TAG        2.4.1
  SOURCE_SUBDIR  _no_cmake)

FetchContent_MakeAvailable(doctest fakeit)

# Target equivalent to the one doctest would export (inclusion of <doctest/doctest.h>)
add_library(doctest INTERFACE)
add_library(doctest::doctest ALIAS doctest)
target_include_directories(doctest INTERFACE ${doctest_SOURCE_DIR})

# Same library as the firmware
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../lib/lps22hh ${CMAKE_BINARY_DIR}/lib/lps22hh)

add_executable(lps22hh_tests test_lps22hh.cpp)
target_link_libraries(lps22hh_tests PRIVATE lps22hh doctest::doctest)
target_include_directories(lps22hh_tests PRIVATE
  ${fakeit_SOURCE_DIR}/single_header/doctest   # FakeIt -> doctest adapter (after doctest.h)
  ${doctest_SOURCE_DIR}/doctest)               
target_compile_options(lps22hh_tests PRIVATE -Wall -Wextra)

enable_testing()
list(APPEND CMAKE_MODULE_PATH ${doctest_SOURCE_DIR}/scripts/cmake)
include(doctest)
doctest_discover_tests(lps22hh_tests)