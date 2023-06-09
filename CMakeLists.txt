project(tcamopencvsaveimage)
 
cmake_minimum_required(VERSION 2.6)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


find_package(PkgConfig REQUIRED)
find_package(OpenCV REQUIRED)
set(CMAKE_BUILD_TYPE Debug)

pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0)

pkg_check_modules(TCAM tcam)
if( TCAM_FOUND)
    message(STATUS "TCAM INC : " ${TCAM_INCLUDE_DIRS})
    message(STATUS "TCAM LIB : " ${TCAM_LIBRARIES}) 
else()
    message(FATAL_ERROR "TCAM package missing.")
endif( TCAM_FOUND)

include_directories( 
    ${CMAKE_CURRENT_BINARY_DIR}  
    ${GSTREAMER_INCLUDE_DIRS} 
    ${TCAM_INCLUDE_DIRS}  
    ${OpenCV_INCLUDE_DIRS} 
    )

add_definitions( ${GSTREAMER_CFLAGS_OTHER} )  

add_executable(tcamopencvsaveimage 
                ./src/cpp/main.cpp
                ./src/cpp/tcamcamera.cpp
                ./src/cpp/ctcamproperty.cpp 
                ./src/c/CircularBuffer.c
            )

target_link_libraries(tcamopencvsaveimage 
                    ${TCAM_LIBRARIES} 
                    ${GSTREAMER_LIBRARIES} 
                    ${OpenCV_LIBS} 
                    )

install(TARGETS tcamopencvsaveimage RUNTIME DESTINATION bin)
 
