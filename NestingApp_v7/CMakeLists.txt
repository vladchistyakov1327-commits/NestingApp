cmake_minimum_required(VERSION 3.20)
project(NestingApp VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/Geometry.cpp
    src/Part.cpp
    src/Sheet.cpp
    src/NestingEngine.cpp
    src/GeneticAlgorithm.cpp
    src/DXFLoader.cpp
    src/LXDExporter.cpp
)

add_executable(NestingApp ${SOURCES})
target_include_directories(NestingApp PRIVATE src)
target_link_libraries(NestingApp Qt6::Core Qt6::Widgets)

if(MSVC)
    target_compile_options(NestingApp PRIVATE /O2 /openmp /utf-8 /W3)
    target_link_options(NestingApp PRIVATE /LTCG)
    set_target_properties(NestingApp PROPERTIES WIN32_EXECUTABLE TRUE)
else()
    target_compile_options(NestingApp PRIVATE -O2 -fopenmp -Wall -Wextra)
    target_link_libraries(NestingApp gomp)
endif()

if(WIN32)
    add_custom_command(TARGET NestingApp POST_BUILD
        COMMAND "${Qt6_DIR}/../../../bin/windeployqt.exe"
                --release --no-compiler-runtime --no-translations
                "$<TARGET_FILE:NestingApp>"
        COMMENT "Deploying Qt DLLs..."
        VERBATIM
    )
endif()

if(UNIX AND NOT APPLE)
    install(TARGETS NestingApp RUNTIME DESTINATION bin)
endif()

message(STATUS "NestingApp v${PROJECT_VERSION} | Qt ${Qt6_VERSION} | Build: ${CMAKE_BUILD_TYPE}")
