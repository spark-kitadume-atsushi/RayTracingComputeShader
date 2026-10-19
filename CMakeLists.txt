cmake_minimum_required(VERSION 3.10)
project(RayTracingComputeShader)

# C++の標準規格を設定（チュートリアルはモダンなC++を使います）
set ( CMAKE_CXX_STANDARD          11 )
set ( CMAKE_CXX_STANDARD_REQUIRED ON )
set ( CMAKE_CXX_EXTENSIONS        OFF )

set ( SOURCE_COMPUTE_SHADER
 src/main.cc
 src/camera.h
 src/vec3.h
 src/color.h
 src/material.h
 src/texture.h
 src/rtw_stb_image.h
 src/ray.h
 src/hittable.h
 src/hittable_list.h
 src/sphere.h
 src/quad.h
 src/interval.h
 src/aabb.h
 src/perlin.h
 src/bvh.h
 src/constant_medium.h
 src/rtweekend.h
 src/external/stb_image.h
)

# 実行フォルダ「ComputeShader」と、コンパイルするソースコードを指定
add_executable(ComputeShader ${SOURCE_COMPUTE_SHADER})

# プロジェクト全体ではなく、ComputeShaderターゲットに対してインクルードパスを設定
target_include_directories(ComputeShader PRIVATE src)