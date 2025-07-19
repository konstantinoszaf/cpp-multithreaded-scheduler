cmake_minimum_required(VERSION 3.15)
project(scheduler_tests LANGUAGES CXX)

enable_testing()

# Pull in GoogleTest & GoogleMock
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        release-1.12.1
)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

file(GLOB UNIT_TEST_SOURCES
     "${CMAKE_CURRENT_SOURCE_DIR}/src/unit/*.cpp")
file(GLOB INT_TEST_SOURCES
     "${CMAKE_CURRENT_SOURCE_DIR}/src/integration/*.cpp")
set(TEST_SOURCES
    ${UNIT_TEST_SOURCES}
    ${INT_TEST_SOURCES}
)

foreach(test_src IN LISTS TEST_SOURCES)
  get_filename_component(test_name ${test_src} NAME_WE)

  add_executable(${test_name} ${test_src})

  # Public scheduler headers
  target_include_directories(${test_name}
    PRIVATE
      ${CMAKE_SOURCE_DIR}/include
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${CMAKE_SOURCE_DIR}/src
      # ${CMAKE_CURRENT_SOURCE_DIR}/mock
  )

  target_link_libraries(${test_name}
    PRIVATE
      scheduler
      gmock_main
  )

  gtest_discover_tests(${test_name})
endforeach()
