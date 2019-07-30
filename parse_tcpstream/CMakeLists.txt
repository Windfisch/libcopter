cmake_minimum_required(VERSION 2.8.3)
PROJECT ( Test CXX )
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

FIND_PACKAGE ( Threads REQUIRED )


find_package( OpenCV REQUIRED core )


message(STATUS "OpenCV library status:")
message(STATUS "    config: ${OpenCV_DIR}")
message(STATUS "    version: ${OpenCV_VERSION}")
message(STATUS "    libraries: ${OpenCV_LIBS}")
message(STATUS "    include path: ${OpenCV_INCLUDE_DIRS}")

FIND_PACKAGE(PythonInterp)

if (PYTHONINTERP_FOUND)
        FIND_PACKAGE(Boost COMPONENTS python3 numpy3)
        FIND_PACKAGE(PythonInterp 3)
        FIND_PACKAGE(PythonLibs 3 REQUIRED)
else()
    message("Python not found")
endif()



INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS} ${PYTHON_INCLUDE_DIRS} ${OpenCV_INCLUDE_DIRS})
LINK_LIBRARIES(${Boost_LIBRARIES} ${PYTHON_LIBRARIES} -lswscale -lavcodec -lavutil ${OpenCV_LIBS} ${CMAKE_THREAD_LIBS_INIT}) # Deprecated but so convenient!

PYTHON_ADD_MODULE(libcopter python_wrapper.cpp decode_video.cpp sg500.cpp)
#FILE(COPY MyProject.py DESTINATION .) # See the whole tutorial to understand this line

add_executable(test_cpp decode_video.cpp sg500.cpp test_cpp.cpp)

