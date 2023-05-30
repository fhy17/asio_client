# CMakeList.txt: 顶层 CMake 项目文件，在此处执行全局配置
# 并包含子项目。
cmake_minimum_required (VERSION 3.8)



# 声明C使用c99标准，C++使用c++17标准
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
elseif()
    set(CMAKE_C_STANDARD 99)
    set(CMAKE_CXX_STANDARD 17)
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /EHsc")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")


# 设置多线程运行时选项
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")


if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
    # set the output directory for the debug version of the executable
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE})
    # set the output directory for the debug version of the library file
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/lib/${CMAKE_BUILD_TYPE})

    # set the suffix of the library file of the debug version
    # set(CMAKE_DEBUG_POSTFIX "_d")
elseif(${CMAKE_BUILD_TYPE} MATCHES "Release")
    # set the output directory for the release version of the executable
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE})
    # set the output directory for the release version of the library file
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/lib/${CMAKE_BUILD_TYPE})

    # set the suffix of the library file of the release version
    # set(CMAKE_RELEASE_POSTFIX "_r")
endif()

if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
    message(STATUS "CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG " ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG})
    message(STATUS "CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG " ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG})
elseif(${CMAKE_BUILD_TYPE} MATCHES "Release")
    message(STATUS "CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE " ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE})
    message(STATUS "CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE " ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE})
endif()

# message("EXECUTABLE_OUTPUT_PATH"  ${EXECUTABLE_OUTPUT_PATH})
# message("LIBRARY_OUTPUT_PATH" ${LIBRARY_OUTPUT_PATH})

message(STATUS "CMAKE_GENERATOR_PLATFORM: " ${CMAKE_GENERATOR_PLATFORM})

if(${CMAKE_GENERATOR_PLATFORM} MATCHES [xX]64)
    set(CURRENT_PLATFORM "x64")
elseif(${CMAKE_GENERATOR_PLATFORM} MATCHES [wW]in32)
    set(CURRENT_PLATFORM "x86")
endif()
message(STATUS "CURRENT_PLATFORM: " ${CURRENT_PLATFORM})

if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
    set(BUILD_TYPE "debug")
elseif(${CMAKE_BUILD_TYPE} MATCHES "Release")
    set(BUILD_TYPE "release")
endif()



project ("fhy" VERSION 1.0.0.0)


add_subdirectory("asio-1.28")

add_subdirectory("asio-1.10")