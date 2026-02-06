cmake_minimum_required(VERSION 3.10)
project(ImageEncoder)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找JNI
find_package(JNI REQUIRED)

# 设置libjpeg-turbo路径（相对于CMakeLists.txt）
set(TURBOJPEG_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(TURBOJPEG_LIB "${CMAKE_CURRENT_SOURCE_DIR}/lib/turbojpeg.lib")

# 检查文件是否存在
if(NOT EXISTS "${TURBOJPEG_LIB}")
    message(FATAL_ERROR "未找到 turbojpeg.lib，请确保已复制到 native/lib/ 目录")
endif()

# 创建共享库 (DLL)
add_library(ImageEncoder SHARED ImageEncoder.cpp)

# 包含目录
target_include_directories(ImageEncoder PRIVATE 
    ${JNI_INCLUDE_DIRS}
    ${TURBOJPEG_INCLUDE_DIR}
)

# 链接库
target_link_libraries(ImageEncoder 
    ${TURBOJPEG_LIB}
)

# 设置输出目录（输出到项目根目录的build文件夹）
set_target_properties(ImageEncoder PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../build"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../build"
)

# Windows特定设置
if(WIN32)
    # 确保生成.dll而不是.lib
    set_target_properties(ImageEncoder PROPERTIES PREFIX "")
endif()

message(STATUS "JNI包含目录: ${JNI_INCLUDE_DIRS}")
message(STATUS "TurboJPEG库: ${TURBOJPEG_LIB}")
message(STATUS "输出目录: ${CMAKE_CURRENT_SOURCE_DIR}/../build")

