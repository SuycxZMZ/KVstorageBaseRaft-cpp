# 设置cmake的最低版本和项目名称
cmake_minimum_required(VERSION 3.10)

project(KVRaftCpp)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

# only build examples if this is the main project
if(CMAKE_PROJECT_NAME STREQUAL "KVRaftCpp")
    option(KVRaftCpp_BUILD_EXAMPLES "Build KVRaftCpp examples" ON)
endif()

# 设置全局编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_FILE_OFFSET_BITS=64 -std=c++20")

# 分别为调试和发布版本设置编译选项
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O2 -DNDEBUG")

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
include_directories(${PROJECT_SOURCE_DIR}/src)

# 设置项目库文件搜索路径 -L
link_directories(
    ${PROJECT_SOURCE_DIR}/lib 
    /usr/local/lib            
)

add_subdirectory(src)
if (KVRaftCpp_BUILD_EXAMPLES)
    add_subdirectory(example)
endif()


# 添加格式化目标 start
# from : https://blog.csdn.net/guotianqing/article/details/121661067
add_custom_target(format
        COMMAND bash ${PROJECT_SOURCE_DIR}/format.sh
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMAND echo "format done!"
)
# 添加格式化目标 end