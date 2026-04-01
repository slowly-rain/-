cmake_minimum_required(VERSION 3.28)
project(CMakeProject LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 选项：是否构建为共享库，共享库设为 ON；静态库设为 OFF
# option(BUILD_SHARED_LIBS "Build libraries as shared" OFF)

add_executable(snake src/snake.cpp)
target_compile_features(snake PRIVATE cxx_std_17)

# 在Windows系统中（使用 MinGW-w64）构建时
# 静态链接需要将libstdc++、libgcc编译进可执行文件中，避免运行时依赖外部 DLL
if(WIN32 AND (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")))
  target_link_libraries(snake PRIVATE
      -static-libgcc
      -static-libstdc++
    )
endif()

# 测试 & 打包
enable_testing()
add_test(NAME SmokeTest
         COMMAND snake
         WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
set_tests_properties(SmokeTest PROPERTIES
  TIMEOUT 15
)

set(CPACK_PACKAGE_NAME "CMakeProject")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_VENDOR "HDU")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "CMake-Project")
set(CPACK_PACKAGE_CONTACT "contact@example.com")
include(CPack)