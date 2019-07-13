cmake_minimum_required(VERSION 2.8.3)
FIND_PACKAGE(PythonInterp)

if (PYTHONINTERP_FOUND)
        FIND_PACKAGE(Boost COMPONENTS python3 numpy3)
        FIND_PACKAGE(PythonInterp 3)
        FIND_PACKAGE(PythonLibs 3 REQUIRED)
else()
    message("Python not found")
endif()



INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS} ${PYTHON_INCLUDE_DIRS})
LINK_LIBRARIES(${Boost_LIBRARIES} ${PYTHON_LIBRARIES} -lswscale -lavcodec -lavutil) # Deprecated but so convenient!

PYTHON_ADD_MODULE(libcopter python_wrapper.cpp decode_video.cpp)
#FILE(COPY MyProject.py DESTINATION .) # See the whole tutorial to understand this line
