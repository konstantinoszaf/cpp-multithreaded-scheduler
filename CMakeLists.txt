cmake_minimum_required(VERSION 3.15)
project(scheduler LANGUAGES CXX)

option(BUILD_TESTS "Build unit and integration tests" ON)
option(BUILD_EXAMPLE  "Build the example 'main' executable (and pull spdlog)" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

file(GLOB_RECURSE SCHEDULER_SRC
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

add_library(scheduler SHARED ${SCHEDULER_SRC})

target_include_directories(scheduler
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

find_package(Threads REQUIRED)
target_link_libraries(scheduler PUBLIC Threads::Threads)

if(BUILD_EXAMPLE)
  include(FetchContent)
  FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.11.0
  )
  FetchContent_MakeAvailable(spdlog)

  add_executable(main main.cpp)
  target_link_libraries(main
    PRIVATE
      scheduler
      spdlog::spdlog_header_only
  )

  add_custom_target(example
    DEPENDS main
    COMMENT "Build the example/main executable"
  )
endif()

install(TARGETS scheduler
        EXPORT schedulerTargets
        LIBRARY DESTINATION lib)

install(DIRECTORY include/scheduler
        DESTINATION include
        FILES_MATCHING PATTERN "*.h")

install(EXPORT schedulerTargets
        FILE schedulerTargets.cmake
        NAMESPACE scheduler::
        DESTINATION lib/cmake/scheduler)

if(BUILD_TESTS)
  add_subdirectory(test)
endif()
