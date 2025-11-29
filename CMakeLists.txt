cmake_minimum_required(VERSION 3.20)

project(LearnProject VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SOURCE_CPP
    src/**/*.cpp    
    src/*.cpp
)

# Add the executable
add_executable(LearnApp ${SOURCE_CPP})

# 启用所有警告，这对学习和高质量代码很重要
if(MSVC)
    target_compile_options(LearnApp PRIVATE /W4)
else()
    target_compile_options(LearnApp PRIVATE -Wall -Wextra -Wpedantic)
endif()


# Add the fast executable

