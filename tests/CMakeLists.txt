cmake_minimum_required(VERSION 3.20)
project(controller_host_tests LANGUAGES CXX)
enable_testing()
add_executable(controller_tests
    controller_tests.cpp
    ../src/controller/InputNormalization.cpp
    ../src/controller/XboxController.cpp
    ../src/controller/Bluepad32Adapter.cpp
    ../src/diagnostics/SerialDiagnostics.cpp)
target_include_directories(controller_tests PRIVATE ../src fakes)
target_compile_features(controller_tests PRIVATE cxx_std_11)
if(MSVC)
    target_compile_options(controller_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(controller_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME controller_logic COMMAND controller_tests)

add_executable(pad_mapping_tests pad_mapping_tests.cpp)
target_include_directories(pad_mapping_tests PRIVATE ../src)
target_compile_features(pad_mapping_tests PRIVATE cxx_std_11)
add_test(NAME console_pad_mapping COMMAND pad_mapping_tests)
