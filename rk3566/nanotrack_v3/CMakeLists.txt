cmake_minimum_required(VERSION 3.13)
project(nanotrack_v3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

option(ENABLE_RKNN_RUNTIME "Build the RK3566 RKNN/OpenCV executable" OFF)

add_library(nanotrack_math src/tracker_math.cpp)
target_include_directories(nanotrack_math PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(nanotrack_math PRIVATE -Wall -Wextra -Wpedantic)

add_library(nanotrack_cli src/cli.cpp)
target_include_directories(nanotrack_cli PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nanotrack_cli PUBLIC nanotrack_math)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
  target_link_libraries(nanotrack_cli PUBLIC stdc++fs)
endif()
target_compile_options(nanotrack_cli PRIVATE -Wall -Wextra -Wpedantic)

add_library(nanotrack_controller_support
  src/controller_config.cpp
  src/long_term_controller.cpp
  src/long_term_runtime.cpp
  src/long_term_types.cpp
  src/motion_estimator.cpp
  src/tracking_log.cpp
)
target_include_directories(nanotrack_controller_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nanotrack_controller_support PUBLIC nanotrack_math)
target_compile_options(nanotrack_controller_support PRIVATE -Wall -Wextra -Wpedantic)

if(BUILD_TESTING)
  add_executable(test_tracker_math tests/test_tracker_math.cpp)
  target_link_libraries(test_tracker_math PRIVATE nanotrack_math)
  add_test(NAME tracker_math COMMAND test_tracker_math)

  add_executable(test_cli tests/test_cli.cpp)
  target_link_libraries(test_cli PRIVATE nanotrack_cli)
  add_test(NAME cli COMMAND test_cli)

  add_executable(test_controller_support tests/test_controller_support.cpp)
  target_link_libraries(test_controller_support PRIVATE nanotrack_controller_support)
  add_test(NAME controller_support COMMAND test_controller_support)

  add_executable(test_long_term_controller tests/test_long_term_controller.cpp)
  target_link_libraries(test_long_term_controller PRIVATE nanotrack_controller_support)
  add_test(NAME long_term_controller COMMAND test_long_term_controller)

  add_executable(test_long_term_runtime tests/test_long_term_runtime.cpp)
  target_link_libraries(test_long_term_runtime PRIVATE nanotrack_controller_support)
  add_test(NAME long_term_runtime COMMAND test_long_term_runtime)

  add_executable(test_tracking_log tests/test_tracking_log.cpp)
  target_link_libraries(test_tracking_log PRIVATE nanotrack_controller_support)
  add_test(NAME tracking_log COMMAND test_tracking_log)

  add_executable(test_long_term_replay tests/test_long_term_replay.cpp)
  target_link_libraries(test_long_term_replay PRIVATE nanotrack_controller_support)
  add_test(NAME long_term_replay COMMAND test_long_term_replay)
endif()

if(ENABLE_RKNN_RUNTIME)
  if(NOT DEFINED RKNN_INCLUDE_DIR OR NOT DEFINED RKNN_LIBRARY)
    message(FATAL_ERROR "Set RKNN_INCLUDE_DIR and RKNN_LIBRARY")
  endif()
  find_package(OpenCV REQUIRED)
  add_library(nanotrack_runtime
    src/rknn_model.cpp
    src/nanotrack_v3.cpp
  )
  target_include_directories(nanotrack_runtime
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${RKNN_INCLUDE_DIR} ${OpenCV_INCLUDE_DIRS}
  )
  target_link_libraries(nanotrack_runtime PUBLIC nanotrack_controller_support ${RKNN_LIBRARY} ${OpenCV_LIBS} dl)
  target_compile_options(nanotrack_runtime PRIVATE -Wall -Wextra -Wpedantic)

  add_executable(nanotrack_v3 src/main.cpp)
  target_link_libraries(nanotrack_v3 PRIVATE nanotrack_cli nanotrack_runtime ${OpenCV_LIBS})
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(nanotrack_v3 PRIVATE stdc++fs)
  endif()
  target_compile_options(nanotrack_v3 PRIVATE -Wall -Wextra -Wpedantic)
endif()
