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

# Оптимизация
if(MSVC)
    target_compile_options(NestingApp PRIVATE /O2 /openmp /utf-8 /W3)
    target_link_options(NestingApp PRIVATE /LTCG)
else()
    target_compile_options(NestingApp PRIVATE -O2 -fopenmp -Wall)
    target_link_libraries(NestingApp gomp)
endif()

# Windows: деплой Qt DLL
if(WIN32)
    add_custom_command(TARGET NestingApp POST_BUILD
        COMMAND ${Qt6_DIR}/../../../bin/windeployqt
                --no-compiler-runtime
                $<TARGET_FILE:NestingApp>
        COMMENT "Deploying Qt DLLs..."
        VERBATIM
    )
endif()

# Вывод пути бинарника
message(STATUS "NestingApp binary: ${CMAKE_BINARY_DIR}/NestingApp")
