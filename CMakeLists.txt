cmake_minimum_required(VERSION 3.5)

project(refl)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    add_compile_options(/std:c++20 /bigobj)
else()
    add_compile_options(--std=c++20)
endif()

set(SOURCES
    JSON.cpp
    Object.cpp
    ReflMgrInit.cpp
    TypeID.cpp
    main.cpp
)

add_executable(refl ${SOURCES})
