cmake_minimum_required(VERSION 3.15)
project(scheduler LANGUAGES CXX)

option(BUILD_TESTS "Build unit and integration tests" ON)
option(USE_TSAN "Enable ThreadSanitizer for all targets" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BUILD_TESTS "Build unit and integration tests" ON)
option(USE_TSAN "Enable ThreadSanitizer for all targets" OFF)

# Only set these flags as a list, not a string
if(USE_TSAN)
  message(STATUS "Building EVERYTHING with ThreadSanitizer")
  add_compile_options(-fsanitize=thread -fno-omit-frame-pointer -fsanitize=undefined)
  add_link_options(-fsanitize=thread -fno-omit-frame-pointer -fsanitize=undefined)
endif()

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

add_executable(main main.cpp)
target_link_libraries(main PRIVATE scheduler)

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
