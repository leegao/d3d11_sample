cmake_minimum_required(VERSION 3.20)
project(d3d11_bc7_sample VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(directxtex CONFIG REQUIRED)

add_executable(d3d11_bc7_sample main.cpp)
target_compile_definitions(d3d11_bc7_sample PRIVATE UNICODE _UNICODE)
target_link_libraries(d3d11_bc7_sample PRIVATE
    Microsoft::DirectXTex
    d3d11
    dxgi
    d3dcompiler
)

set_property(TARGET d3d11_bc7_sample PROPERTY VS_WINCOMPILED_MANIFEST False)
