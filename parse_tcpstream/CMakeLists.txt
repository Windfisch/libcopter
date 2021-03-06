cmake_minimum_required(VERSION 3.9)
PROJECT ( libcopter CXX )
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(build_python_bindings "build the python bindings for libcopter" ON)



FIND_PACKAGE (Threads REQUIRED)
find_package(OpenCV REQUIRED core imgproc highgui)

if (build_python_bindings)
	FIND_PACKAGE(PythonInterp 3 REQUIRED)
	FIND_PACKAGE(Boost REQUIRED COMPONENTS python38 numpy38 system)
	FIND_PACKAGE(PythonLibs 3 REQUIRED)
else()
	FIND_PACKAGE(Boost REQUIRED COMPONENTS system)
endif()


message(STATUS "OpenCV library status:")
message(STATUS "    config: ${OpenCV_DIR}")
message(STATUS "    version: ${OpenCV_VERSION}")
message(STATUS "    libraries: ${OpenCV_LIBS}")
message(STATUS "    include path: ${OpenCV_INCLUDE_DIRS}")


set(common_libs -lswscale -lavcodec -lavutil ${OpenCV_LIBS} ${CMAKE_THREAD_LIBS_INIT})
INCLUDE_DIRECTORIES(${OpenCV_INCLUDE_DIRS})

if (build_python_bindings)
	PYTHON_ADD_MODULE(parsestream parsestream_python_wrapper.cpp decode_video.cpp)
	PYTHON_ADD_MODULE(libsg500 sg500.cpp sg500_py.cpp decode_video.cpp)

	target_include_directories(parsestream PUBLIC ${Boost_INCLUDE_DIRS} ${PYTHON_INCLUDE_DIRS})
	target_include_directories(libsg500    PUBLIC ${Boost_INCLUDE_DIRS} ${PYTHON_INCLUDE_DIRS})
	
	TARGET_LINK_LIBRARIES(parsestream ${Boost_LIBRARIES} ${PYTHON_LIBRARIES} ${common_libs})
	TARGET_LINK_LIBRARIES(libsg500    ${Boost_LIBRARIES} ${PYTHON_LIBRARIES} ${common_libs})
endif()

add_executable(decode_streamdump decode_video.cpp decode_streamdump_main.cpp)
TARGET_LINK_LIBRARIES(decode_streamdump -lswscale -lavcodec -lavutil)

add_library(copter_static STATIC decode_video.cpp sg500.cpp)
TARGET_LINK_LIBRARIES(copter_static ${common_libs} Boost::boost Boost::system)
