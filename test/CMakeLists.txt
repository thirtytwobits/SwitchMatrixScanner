
cmake_minimum_required(VERSION 3.9)

project(SwitchMatrixScannerTests CXX)

# Download and unpack googletest at configure time
configure_file(CMakeLists.txt.in googletest-download/CMakeLists.txt)

execute_process(COMMAND
                    ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
                RESULT_VARIABLE
                    result
                WORKING_DIRECTORY
                    ${CMAKE_CURRENT_BINARY_DIR}/googletest-download)

if(result)
    message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif()

execute_process(COMMAND 
                    ${CMAKE_COMMAND} --build .
                RESULT_VARIABLE
                    result
                WORKING_DIRECTORY
                    ${CMAKE_CURRENT_BINARY_DIR}/googletest-download)

if(result)
    message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()

# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/googletest-src
                 ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
                 EXCLUDE_FROM_ALL)

                 
add_executable(SwitchMatrixScannerTest SwitchMatrixScannerTest.cpp)
target_include_directories(SwitchMatrixScannerTest PRIVATE 
    "${CMAKE_SOURCE_DIR}/../src")
target_link_libraries(SwitchMatrixScannerTest gmock_main)
target_compile_options(gmock_main PUBLIC
                       "-Wno-switch-enum"
                       "-Wno-zero-as-null-pointer-constant"
                       "-Wno-missing-declarations"
                       "-Wno-sign-conversion"
                       "-std=c++11"
                       "-fno-rtti"
                       "-fno-threadsafe-statics"
                       "-fno-exceptions"
                       )

add_custom_target(
    run_SwitchMatrixScannerTest
    COMMAND
        ${CMAKE_CURRENT_BINARY_DIR}/SwitchMatrixScannerTest
    DEPENDS
        SwitchMatrixScannerTest
)
