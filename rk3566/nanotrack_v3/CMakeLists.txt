cmake_minimum_required(VERSION 3.13)
project(nanotrack_v3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
find_package(Threads REQUIRED)

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

add_library(nanotrack_detector_adapter src/yolo_detector_adapter.cpp)
target_include_directories(nanotrack_detector_adapter PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nanotrack_detector_adapter PUBLIC nanotrack_controller_support Threads::Threads)
target_compile_options(nanotrack_detector_adapter PRIVATE -Wall -Wextra -Wpedantic)

add_library(nanotrack_yolov8_postprocess src/yolov8_postprocess.cpp)
target_include_directories(nanotrack_yolov8_postprocess PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nanotrack_yolov8_postprocess PUBLIC nanotrack_math)
target_compile_options(nanotrack_yolov8_postprocess PRIVATE -Wall -Wextra -Wpedantic)

add_library(nanotrack_sha256 src/sha256.cpp)
target_include_directories(nanotrack_sha256 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(nanotrack_sha256 PRIVATE -Wall -Wextra -Wpedantic)

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

  add_executable(test_yolo_detector_adapter tests/test_yolo_detector_adapter.cpp)
  target_link_libraries(test_yolo_detector_adapter PRIVATE nanotrack_detector_adapter)
  add_test(NAME yolo_detector_adapter COMMAND test_yolo_detector_adapter)

  add_executable(test_yolov8_postprocess tests/test_yolov8_postprocess.cpp)
  target_link_libraries(test_yolov8_postprocess PRIVATE nanotrack_yolov8_postprocess)
  add_test(NAME yolov8_postprocess COMMAND test_yolov8_postprocess)

  add_executable(test_sha256 tests/test_sha256.cpp)
  target_link_libraries(test_sha256 PRIVATE nanotrack_sha256)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(test_sha256 PRIVATE stdc++fs)
  endif()
  add_test(NAME sha256 COMMAND test_sha256)
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

  add_library(nanotrack_yolo_detector src/yolo_rknn_engine.cpp)
  target_include_directories(nanotrack_yolo_detector
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${RKNN_INCLUDE_DIR} ${OpenCV_INCLUDE_DIRS}
  )
  target_link_libraries(nanotrack_yolo_detector
    PUBLIC nanotrack_detector_adapter nanotrack_yolov8_postprocess
           nanotrack_sha256 ${RKNN_LIBRARY} ${OpenCV_LIBS} dl
  )
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(nanotrack_yolo_detector PUBLIC stdc++fs)
  endif()
  target_compile_options(nanotrack_yolo_detector PRIVATE -Wall -Wextra -Wpedantic)

  add_executable(nanotrack_v3 src/main.cpp)
  target_link_libraries(nanotrack_v3
    PRIVATE nanotrack_cli nanotrack_runtime nanotrack_yolo_detector
            nanotrack_detector_adapter ${OpenCV_LIBS}
  )
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(nanotrack_v3 PRIVATE stdc++fs)
  endif()
  target_compile_options(nanotrack_v3 PRIVATE -Wall -Wextra -Wpedantic)

  add_executable(yolo_detector_smoke src/yolo_detector_smoke.cpp)
  target_link_libraries(yolo_detector_smoke
    PRIVATE nanotrack_yolo_detector ${OpenCV_LIBS}
  )
  target_compile_options(yolo_detector_smoke PRIVATE -Wall -Wextra -Wpedantic)
endif()
