# CMakeList.txt: main 项目文件

project ("asio_test")

# add_definitions(-D_WIN32_WINDOWS)
add_definitions(-D_WIN32_WINNT=0x0601)

message(CMAKE_CURRENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# add include directory
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_SOURCE_DIR}/external_dependencies/asio/include
)

link_directories(
    ${CMAKE_SOURCE_DIR}/lib
)

# get source list of the directory
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC_LIST)
# message(${SRC_LIST})

# add source to this project's library
add_executable(${PROJECT_NAME} ${SRC_LIST})