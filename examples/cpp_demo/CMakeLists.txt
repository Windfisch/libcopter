cmake_minimum_required(VERSION 3.9)
PROJECT (libcopter_cpp_demo CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


find_package(OpenCV REQUIRED core imgproc highgui)

add_subdirectory("../../parse_tcpstream" libcopter_build EXCLUDE_FROM_ALL)
INCLUDE_DIRECTORIES("../../parse_tcpstream" ${OpenCV_INCLUDE_DIRS})

add_executable(demo main.cpp)
target_link_libraries(demo copter_static)


