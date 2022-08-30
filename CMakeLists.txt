cmake_minimum_required(VERSION 3.22)
project(mdestimator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG -Wno-format -fno-exceptions -fno-rtti -ggdb -O0 -fvisibility=hidden")
set(CMAKE_C_FLAGS_DEBUG "-DDEBUG -Wno-format -ggdb -O0 -fvisibility=hidden")
set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -Wno-format -O3 -fno-exceptions -fno-rtti -fvisibility=hidden")
set(CMAKE_C_FLAGS_RELEASE "-DNDEBUG -Wno-format -O3 -fvisibility=hidden")

set(FILES
        acumulator.h
        CircularQueue.h
        MovingCounter.h
        SendSideBandwidthEstimation.cpp
        SendSideBandwidthEstimation.h
        mdestimator.cpp
        mdestimator/mdestimator.h)

add_library(${PROJECT_NAME} SHARED ${FILES})

include_directories(${PROJECT_SOURCE_DIR})

set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER mdestimator/mdestimator.h)
include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
        PUBLIC_HEADER
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mdestimator
        LIBRARY
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/)
