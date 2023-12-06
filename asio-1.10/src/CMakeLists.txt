# CMakeList.txt: main 项目文件

project ("asio-1.10")

# add_definitions(-D_WIN32_WINDOWS)
add_definitions(-D_WIN32_WINNT=0x0601 -DASIO_STANDALONE -DASIO_HAS_STD_CHRONO)

# message(STATUS CMAKE_CURRENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(ASIO_PATH "asio-1.10")
# add include directory
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_SOURCE_DIR}/extern/${ASIO_PATH}
)
# message(STATUS link_dir: ${CMAKE_SOURCE_DIR}/external_dependencies/${ASIO_PATH})

link_directories(
    ${CMAKE_SOURCE_DIR}/lib
)

# get source list of the directory
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC_LIST)
# message(${SRC_LIST})

# add source to this project's library
add_executable(${PROJECT_NAME} ${SRC_LIST})