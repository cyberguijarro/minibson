# build.cmake

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Release)
endif(NOT CMAKE_BUILD_TYPE)

if(NOT DEFINED BUILD_SHARED_LIBS)
  set(BUILD_SHARED_LIBS ON)
endif()

if(${CMAKE_BUILD_TYPE} STREQUAL Debug)
  enable_testing()
  include(CTest)
endif()


# TODO add flags for msvc
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
else()
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic -g -O0")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")


option(leak_check "set leak_check" 0)
option(profiling "set profiling" 0)
option(thread_check "set thread_check" 0)

if(${CMAKE_BUILD_TYPE} STREQUAL Debug AND leak_check)
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer")
endif()

if(${CMAKE_BUILD_TYPE} STREQUAL Debug AND thread_check)
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=thread")
endif()

if(${CMAKE_BUILD_TYPE} STREQUAL Debug AND profiling)
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -pg")
  set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -pg")
  set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -pg")
  set(CMAKE_STATIC_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -pg")
endif()


if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()


message(STATUS "buid type        " ${CMAKE_BUILD_TYPE})
message(STATUS "Project          " ${PROJECT_NAME})
message(STATUS "c compiler       " ${CMAKE_C_COMPILER})
message(STATUS "cxx compiler     " ${CMAKE_CXX_COMPILER})
message(STATUS "build tests      " ${BUILD_TESTING})
message(STATUS "build shared     " ${BUILD_SHARED_LIBS})
message(STATUS "leak   sanitizer " ${leak_check})
message(STATUS "thread sanitizer " ${thread_check})
message(STATUS "profiling        " ${profiling})
