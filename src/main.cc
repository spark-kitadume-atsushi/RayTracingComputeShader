#include "rtweekend.h"

#include "bvh.h"
#include "camera.h"
#include "constant_medium.h"
#include "hittable.h"
#include "hittable_list.h"
#include "material.h"
#include "sphere.h"
#include "quad.h"
#include "texture.h"

// 必須ヘッダー
#include <d3d11.h>          // DirectX 11 本体
#include <d3dcompiler.h>    // HLSLシェーダーのコンパイル用
#include <wrl/client.h>     // Microsoft::WRL::ComPtr 用

// エラーチェックなどで使う標準ライブラリ
#include <iostream>
#include <vector>
#include <stdexcept>
#include <algorithm>

// ComPtr を短く書くための記述
using Microsoft::WRL::ComPtr;

// DirectX API の呼び出し結果をチェックするマクロ
#define CheckHR(hr) if (FAILED(hr)) { throw std::runtime_error("DirectX Error!"); }

void bouncing_spheres() {
    // World
    hittable_list world;

    auto checker = make_shared<checker_texture>(0.32, color(.2, .3, .1), color(.9, .9, .9));
    world.add(make_shared<sphere>(point3(0, -1000, 0), 1000, make_shared<lambertian>(checker)));

    int num = 11;
    double radius = 0.2;
    for (int a = -num; a < num; a++) {
        for (int b = -num; b < num; b++) {
            auto choose_mat = random_double();
            point3 center(a + 0.9 * random_double(), radius, b + 0.9 * random_double());

            if ((center - point3(4, radius, 0)).length() > 0.9) {
                shared_ptr<material> sphere_material;

                if (choose_mat < 0.8) {
                    // diffuse
                    auto albedo = color::random() * color::random();
                    sphere_material = make_shared<lambertian>(albedo);
                    auto center2 = center + vec3(0, random_double(0, .5), 0);
                    world.add(make_shared<sphere>(center, center2, radius, sphere_material));
                }
                else if (choose_mat < 0.95) {
                    // metal
                    auto albedo = color::random(0.5, 1);
                    auto fuzz = random_double(0, 0.5);
                    sphere_material = make_shared<metal>(albedo, fuzz);
                    world.add(make_shared<sphere>(center, radius, sphere_material));
                }
                else {
                    // glass
                    sphere_material = make_shared<dielectric>(1.5);
                    world.add(make_shared<sphere>(center, radius, sphere_material));
                }

                
            }
        }
    }

    radius = 1.0;
    auto material1 = make_shared<dielectric>(1.5);
    world.add(make_shared<sphere>(point3(0, radius, 0), radius, material1));

    auto material2 = make_shared<lambertian>(color(0.4, 0.2, 0.1));
    world.add(make_shared<sphere>(point3(-4, radius, 0), radius, material2));

    auto material3 = make_shared<metal>(color(0.7, 0.6, 0.5), 0.0);
    world.add(make_shared<sphere>(point3(4, radius, 0), radius, material3));

    world = hittable_list(make_shared<bvh_node>(world));

    // Camera
    camera cam;
    cam.aspect_ratio = 16.0 / 9.0; // width / height
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = color(0.70, 0.80, 1.00);

    cam.vfov = 20;
    cam.lookfrom = point3(13, 2, 3);
    cam.lookat = point3(0, 0, 0);
    cam.vup = vec3(0, 1, 0);

    cam.defocus_angle = 0.6;
    cam.focus_dist = 10.0;

    cam.render(world);
}

void checkered_spheres() {
    hittable_list world;

    auto checker = make_shared<checker_texture>(0.32, color(.2, .3, .1), color(.9,.9,.9));

    world.add(make_shared<sphere>(point3(0, -10, 0), 10, make_shared<lambertian>(checker)));
    world.add(make_shared<sphere>(point3(0,  10, 0), 10, make_shared<lambertian>(checker)));

    camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = color(0.70, 0.80, 1.00);

    cam.vfov = 20;
    cam.lookfrom = point3(13, 2, 3);
    cam.lookat = point3(0, 0, 0);
    cam.vup = vec3(0, 1, 0);

    cam.defocus_angle = 0;
    cam.render(world);
}

void earth() {
    auto earth_texture = make_shared<image_texture>("earthmap.jpg");
    auto earth_surface = make_shared<lambertian>(earth_texture);
    auto globe = make_shared<sphere>(point3(0, 0, 0), 2, earth_surface);

    camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = color(0.70, 0.80, 1.00);

    cam.vfov = 20;
    cam.lookfrom = point3(0,0,12);
    cam.lookat = point3(0,0,0);
    cam.vup = vec3(0,1,0);

    cam.defocus_angle = 0;

    cam.render(hittable_list(globe));
}

void perlin_spheres() {
    hittable_list world;

    auto pertext = make_shared<noise_texture>(4);
    world.add(make_shared<sphere>(point3(0, -1000,0), 1000, make_shared<lambertian>(pertext)));
    world.add(make_shared<sphere>(point3(0, 2, 0), 2, make_shared<lambertian>(pertext)));

    camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = color(0.70, 0.80, 1.00);

    cam.vfov = 20;
    cam.lookfrom = point3(13, 2, 3);
    cam.lookat = point3(0, 0, 0);
    cam.vup = vec3(0, 1, 0);

    cam.defocus_angle = 0;

    cam.render(world);
}

void quads() {
    hittable_list world;
    
    // Materials
    auto left_red       = make_shared<lambertian>(color(1.0, 0.2, 0.2));
    auto back_green     = make_shared<lambertian>(color(0.2, 1.0, 0.2));
    auto right_blue     = make_shared<lambertian>(color(0.2, 0.2, 1.0));
    auto upper_orange   = make_shared<lambertian>(color(1.0, 0.5, 0.0));
    auto lower_teal     = make_shared<lambertian>(color(0.2, 0.8, 0.8));

    // Quads
    world.add(make_shared<quad>(point3(-3,-2, 5), vec3(0, 0,-4), vec3(0, 4, 0), left_red));
    world.add(make_shared<quad>(point3(-2,-2, 0), vec3(4, 0, 0), vec3(0, 4, 0), back_green));
    world.add(make_shared<quad>(point3( 3,-2, 1), vec3(0, 0, 4), vec3(0, 4, 0), right_blue));
    world.add(make_shared<quad>(point3(-2, 3, 1), vec3(4, 0, 0), vec3(0, 0, 4), upper_orange));
    world.add(make_shared<quad>(point3(-2,-3, 5), vec3(4, 0, 0), vec3(0, 0,-4), lower_teal));

    camera cam;

    cam.aspect_ratio = 1.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = color(0.70, 0.80, 1.00);

    cam.vfov = 80;
    cam.lookfrom = point3(0, 0, 9);
    cam.lookat = point3(0, 0, 0);
    cam.vup = vec3(0, 1, 0);

    cam.defocus_angle = 0;

    cam.render(world);
}

void sample_light() {
    hittable_list world;

    auto pertext = make_shared<noise_texture>(4);
    world.add(make_shared<sphere>(point3(0, -1000, 0), 1000, make_shared<lambertian>(pertext)));
    world.add(make_shared<sphere>(point3(0, 2, 0), 2, make_shared<lambertian>(pertext)));

    world.add(make_shared<sphere>(point3(0, 7, 0), 2, make_shared<diffuse_light>(color(0.2, 0.2, 0.8))));
    world.add(make_shared<quad>(point3(2, 1, -2), vec3(2, 0, 0), vec3(0, 2, 0), make_shared<diffuse_light>(color(4, 4, 4))));

    camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = color(0, 0, 0);

    cam.vfov = 20;
    cam.lookfrom = point3(26, 3, 6);
    cam.lookat = point3(0, 2, 0);
    cam.vup = vec3(0, 1, 0);

    cam.defocus_angle = 0;

    cam.render(world);
}

void cornell_box() {
    hittable_list world;

    // Materials
    auto red = make_shared<lambertian>(color(.65, .05, .05));
    auto white = make_shared<lambertian>(color(.73, .73, .73));
    auto green = make_shared<lambertian>(color(.12, .45, .15));
    auto light = make_shared<diffuse_light>(color(15, 15, 15));

    // Quads
    world.add(make_shared<quad>(point3(555, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555), green));
    world.add(make_shared<quad>(point3(0, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555), red));
    world.add(make_shared<quad>(point3(343, 554, 332), vec3(-130, 0, 0), vec3(0, 0, -105), light));
    world.add(make_shared<quad>(point3(0, 0, 0), vec3(555, 0, 0), vec3(0, 0, 555), white));
    world.add(make_shared<quad>(point3(555, 555, 555), vec3(-555, 0, 0), vec3(0, 0, -555), white));
    world.add(make_shared<quad>(point3(0, 0, 555), vec3(555, 0, 0), vec3(0, 555, 0), white));

    shared_ptr<hittable> box1 = box(point3(0, 0, 0), point3(165, 330, 165), white);
    box1 = make_shared<rotate_y>(box1, 15);
    box1 = make_shared<translate>(box1, vec3(265, 0, 295));
    world.add(box1);

    shared_ptr<hittable> box2 = box(point3(0,0,0), point3(165, 165, 165), white);
    box2 = make_shared<rotate_y>(box2, -18);
    box2 = make_shared<translate>(box2, vec3(130, 0, 65));
    world.add(box2);

    camera cam;

    cam.aspect_ratio = 1.0;
    cam.image_width = 600;
    cam.samples_per_pixel = 200;
    cam.max_depth = 50;
    cam.background = color(0, 0, 0);

    cam.vfov = 40;
    cam.lookfrom = point3(278, 278, -800);
    cam.lookat = point3(278, 278, 0);
    cam.vup = vec3(0, 1, 0);

    cam.defocus_angle = 0;

    cam.render(world);
}

void cornell_smoke() {
    hittable_list world;

    // Materials
    auto red = make_shared<lambertian>(color(.65, .05, .05));
    auto white = make_shared<lambertian>(color(.73, .73, .73));
    auto green = make_shared<lambertian>(color(.12, .45, .15));
    auto light = make_shared<diffuse_light>(color(15, 15, 15));

    // Quads
    world.add(make_shared<quad>(point3(555, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555), green));
    world.add(make_shared<quad>(point3(0, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555), red));
    world.add(make_shared<quad>(point3(343, 554, 332), vec3(-130, 0, 0), vec3(0, 0, -105), light));
    world.add(make_shared<quad>(point3(0, 0, 0), vec3(555, 0, 0), vec3(0, 0, 555), white));
    world.add(make_shared<quad>(point3(555, 555, 555), vec3(-555, 0, 0), vec3(0, 0, -555), white));
    world.add(make_shared<quad>(point3(0, 0, 555), vec3(555, 0, 0), vec3(0, 555, 0), white));

    shared_ptr<hittable> box1 = box(point3(0, 0, 0), point3(165, 330, 165), white);
    box1 = make_shared<rotate_y>(box1, 15);
    box1 = make_shared<translate>(box1, vec3(265, 0, 295));

    shared_ptr<hittable> box2 = box(point3(0, 0, 0), point3(165, 165, 165), white);
    box2 = make_shared<rotate_y>(box2, -18);
    box2 = make_shared<translate>(box2, vec3(130, 0, 65));

    world.add(make_shared<constant_medium>(box1, 0.01, color(0, 0, 0)));
    world.add(make_shared<constant_medium>(box2, 0.01, color(1, 1, 1)));

    camera cam;

    cam.aspect_ratio = 1.0;
    cam.image_width = 600;
    cam.samples_per_pixel = 200;
    cam.max_depth = 50;
    cam.background = color(0, 0, 0);

    cam.vfov = 40;
    cam.lookfrom = point3(278, 278, -800);
    cam.lookat = point3(278, 278, 0);
    cam.vup = vec3(0, 1, 0);

    cam.defocus_angle = 0;

    cam.render(world);
}

void final_scene(int image_width, int samples_per_pixel, int max_depth) {
    hittable_list boxes1;
    auto ground = make_shared<lambertian>(color(0.48, 0.83, 0.53));

    int boxes_per_side = 20;
    for (int i = 0; i < boxes_per_side; i++) {
        for (int j = 0; j < boxes_per_side; j++) {
            auto w = 100.0;
            auto x0 = -1000.0 + i * w;
            auto z0 = -1000.0 + j * w;
            auto y0 = 0.0;
            auto x1 = x0 + w;
            auto y1 = random_double(1, 101);
            auto z1 = z0 + w;

            boxes1.add(box(point3(x0, y0, z0), point3(x1, y1, z1), ground));
        }
    }

    hittable_list world;
    world.add(make_shared<bvh_node>(boxes1));
    
    auto light = make_shared<diffuse_light>(color(7, 7, 7));
    world.add(make_shared<quad>(point3(123,554,147), vec3(300, 0, 0), vec3(0,0,265), light));

    auto center1 = point3(400, 400, 200);
    auto center2 = center1 + vec3(30,0,0);
    auto sphere_material = make_shared<lambertian>(color(0.7, 0.3, 0.1));
    world.add(make_shared<sphere>(center1, center2, 50, sphere_material));

    world.add(make_shared<sphere>(point3(260, 150, 45), 50, make_shared<dielectric>(1.5)));
    world.add(make_shared<sphere>(
        point3(0, 150, 145), 50, make_shared<metal>(color(0.8, 0.8, 0.9), 1.0)
    ));

    auto boundary = make_shared<sphere>(point3(360, 150, 145), 70, make_shared<dielectric>(1.5));
    world.add(boundary);
    world.add(make_shared<constant_medium>(boundary, 0.2, color(0.2, 0.4, 0.9)));
    boundary = make_shared<sphere>(point3(0, 0, 0), 5000, make_shared<dielectric>(1.5));
    world.add(make_shared<constant_medium>(boundary, .0001, color(1, 1, 1)));

    auto emat = make_shared<lambertian>(make_shared<image_texture>("earthmap.jpg"));
    world.add(make_shared<sphere>(point3(400, 200, 400), 100, emat));
    auto pertext = make_shared<noise_texture>(0.2);
    world.add(make_shared<sphere>(point3(220, 280, 300), 80, make_shared<lambertian>(pertext)));

    hittable_list boxes2;
    auto white = make_shared<lambertian>(color(.73, .73, .73));
    int ns = 1000;
    for (int j = 0; j < ns; j++) {
        boxes2.add(make_shared<sphere>(point3::random(0, 165), 10, white));
    }

    world.add(make_shared<translate>(
        make_shared<rotate_y>(
            make_shared<bvh_node>(boxes2), 15),
        vec3(-100, 270, 395))
    );

    camera cam;

    cam.aspect_ratio = 1.0;
    cam.image_width = image_width;
    cam.samples_per_pixel = samples_per_pixel;
    cam.max_depth = max_depth;
    cam.background = color(0, 0, 0);

    cam.vfov = 40;
    cam.lookfrom = point3(478, 278, -600);
    cam.lookat = point3(278, 278, 0);
    cam.vup = vec3(0, 1, 0);

    cam.defocus_angle = 0;

    cam.render(world);
}

// GPU
GpuAabb compute_sphere_aabb(const GpuSphere& s)
{
    GpuAabb bbox;
    // 静止球と動く球、両方に対応する
    float c0[3] = { s.center[0], s.center[1] , s.center[2] };
    float c1[3] = { s.center[0] + s.center_vec[0],s.center[1] + s.center_vec[1] ,s.center[2] + s.center_vec[2] };

    for(int i = 0; i< 3;i ++) {
        bbox.min_point[i] = std::fmin(c0[i], c1[i]) - s.radius;
        bbox.max_point[i] = std::fmax(c0[i], c1[i]) + s.radius;
    }

    return bbox;
}

GpuAabb merge_aabb(const GpuAabb& a, const GpuAabb& b)
{
    GpuAabb bbox;
    for (int i = 0; i < 3; i++) {
        bbox.min_point[i] = std::fmin(a.min_point[i], b.min_point[i]);
        bbox.max_point[i] = std::fmax(a.max_point[i], b.max_point[i]);
    }
    return bbox;
}

// BVH構築とフラット化関数
// 戻り値はフラット配列におけるこのノードのインデックス
int build_flat_bvh(std::vector<GpuSphere>& spheres, std::vector<int>& indices, int start, int end, std::vector<GpuBvhNode>& flat_nodes)
{
    int span = end - start;
    int current_node_index = static_cast<int>(flat_nodes.size());
    flat_nodes.push_back(GpuBvhNode()); // 仮のノードを確保
    GpuBvhNode& current_node = flat_nodes[current_node_index];

    if (span == 1) {
        // 葉ノード（球を1つ含む）
        current_node.left_child_index = -1;
        current_node.right_child_index = -1;
        current_node.sphere_index = indices[start];
        current_node.bbox = compute_sphere_aabb(spheres[indices[start]]);
        return current_node_index;
    }

    // 分割実の決定
    GpuAabb bounds = compute_sphere_aabb(spheres[indices[start]]);
    for (int i = start + 1; i < end; i++) {
        bounds = merge_aabb(bounds, compute_sphere_aabb(spheres[indices[i]]));
    }

    float extent_x = bounds.max_point[0] - bounds.min_point[0];
    float extent_y = bounds.max_point[1] - bounds.min_point[1];
    float extent_z = bounds.max_point[2] - bounds.min_point[2];

    int axis = 0;
    if (extent_y > extent_x && extent_y > extent_z) {
        axis = 1;
    }
    else if (extent_z > extent_x && extent_z > extent_y){
        axis = 2;
    }

    // 軸でソート
    std::sort(indices.begin() + start, indices.begin() + end,
        [&spheres, axis](int a, int b) {
            return spheres[a].center[axis] < spheres[b].center[axis];
        });

    int mid = start + span / 2;

    // 再帰的に子ノードを構築（※再帰呼び出してvectorが再アロケートされると
    // current_nodeの参照が壊れるため、インデックスで取得し直す
    int left_idx = build_flat_bvh(spheres, indices, start, mid, flat_nodes);
    int right_idx = build_flat_bvh(spheres, indices, mid, end, flat_nodes);

    flat_nodes[current_node_index].left_child_index = left_idx;
    flat_nodes[current_node_index].right_child_index = right_idx;
    flat_nodes[current_node_index].sphere_index = -1;       // 内部ノード
    flat_nodes[current_node_index].bbox = merge_aabb(flat_nodes[left_idx].bbox, flat_nodes[right_idx].bbox);

    return current_node_index;
}

int d3d_exe() {
    try {
        std::clog << "Initializing DirectX 11..." << std::endl;

        // デバイスとコンテキストを格納するスマートポインタ
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;

        // 要求する機能レベル(Directx 11.0または11.1)
        D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0
        };

        HRESULT hr = D3D11CreateDevice(
            nullptr,                    // デフォルトのアダプターを使用
            D3D_DRIVER_TYPE_HARDWARE,   // GPUハードウェアアクセラレーション
            nullptr,                    // ソフトウェアモジュール（未使用）
            D3D11_CREATE_DEVICE_DEBUG,  // デバッグレイヤー有効（開発時のみ）
            featureLevels,              // 要求する機能レベル配列
            ARRAYSIZE(featureLevels),   // 配列の要素数
            D3D11_SDK_VERSION,          // SDKバージョン
            &device,                    // [出力] デバイスポインタ
            nullptr,                    // [出力] 実際に使用される機能レベル
            &context                    // [出力] コンテキストポインタ
        );

        CheckHR(hr);

        std::clog << "DirectX 11 initialized successfully!" << std::endl;

        // --- Compute Shader のコンパイルと登録 ---

        // エラーメッセージを格納するための変数
        ComPtr<ID3DBlob> errorBlob;
        // コンパイル済みのシェーダーバイナリを格納する変数
        ComPtr<ID3DBlob> shaderBlob;

        std::clog << "Compiling Compute Shader..." << std::endl;
        hr = D3DCompileFromFile(
            L"shader/RayTracingCS.hlsl",    // シェーダーファイル名（同じ階層に配置）
            nullptr,                        // マクロ定義（なし）
            nullptr,                        // インクルードインターフェース（なし）
            "CSMain",                       // エントリーポイント関数名
            "cs_5_0",                       // シェーダープロファイル（CS 5.0）
            0,                              // コンパイルフラグ（最適化など）
            0,                              // エフェクトフラグ（なし）
            &shaderBlob,                    // [出力] コンパイルされたバイナリ
            &errorBlob                      // [出力] エラーメッセージ
        );

        // コンパイルエラーのハンドリング
        if (FAILED(hr)) {
            if (errorBlob) {
                // errorBlobからエラーメッセージの文字列を取り出して表示
                std::string errorMsg(static_cast<const char*>(errorBlob->GetBufferPointer()), errorBlob->GetBufferSize());
                std::cerr << "Shader CompilationError:\n" << errorMsg << std::endl;
            }
            else {
                // errorBlob が nullptr の場合はファイルアクセス周りのエラー
                std::cerr << "Shader File Error (File not found or inaccessible). HRESULT: 0x"
                    << std::hex << hr << std::dec << std::endl;
            }
            throw std::runtime_error("Failed to compile Compute Shader");
        }

        // デバイスを使ってコンパイル済みバイナリからComputeShaderオブジェクトを作成
        ComPtr<ID3D11ComputeShader> computeShader;
        hr = device->CreateComputeShader(
            shaderBlob->GetBufferPointer(),     // コンパイル済みのバイナリのポインタ
            shaderBlob->GetBufferSize(),        // バイナリのサイズ
            nullptr,                            // クラスリンケージ（なし）
            &computeShader                      // [出力] 作成されたComputeShaderオブジェクト
        );

        CheckHR(hr);

        std::clog << "Compute Shader compiled and created successfully!" << std::endl;

        // シェーダーコンパイル確認後に実行

        const UINT image_width = 1200;
        const UINT image_height = 675;
        const int samples_per_pixel = 100;
        const int max_depth = 50;

        // --------------------------------------------------------------------------
        // シーン配置物
        // --------------------------------------------------------------------------
        std::vector<GpuSphere> scene;

#if false
        // 地面 (静止)
        scene.push_back({
            {0.0f, -1000.0f, 0.0f}, 1000.0f,
            {0.0f, 0.0f, 0.0f}, 0.0f,
            { {0.5f, 0.5f, 0.5f}, 0, 0.0f }
            });

        // ランダムな小球体群
        for (int a = -11; a < 11; a++) {
            for (int b = -11; b < 11; b++) {
                auto choose_mat = random_double();
                point3 center(a + 0.9 * random_double(), 0.2, b + 0.9 * random_double());

                if ((center - point3(4, 0.2, 0)).length() > 0.9) {
                    if (choose_mat < 0.8) {
                        // 拡散 (Lambertian)
                        auto albedo = color::random() * color::random();
                        auto center2 = center + vec3(0, random_double(0, 0.5), 0);
                        vec3 center_vec = center2 - center;
                        scene.push_back({
                            {(float)center.x(), (float)center.y(), (float)center.z()},
                            0.2f,
                            {(float)center_vec.x(), (float)center_vec.y(), (float)center_vec.z()},
                            0.0f,
                            { {(float)albedo.x(), (float)albedo.y(), (float)albedo.z()}, 0, 0.0f }
                         });
                    }
                    else if (choose_mat < 0.95) {
                        // 金属 (Metal)
                        auto albedo = color::random(0.5, 1);
                        auto fuzz = (float)random_double(0, 0.5);
                        scene.push_back({
                            {(float)center.x(), (float)center.y(), (float)center.z()},
                            0.2f,
                            {0.0f, 0.0f, 0.0f}, 0.0f,
                            { {(float)albedo.x(), (float)albedo.y(), (float)albedo.z()}, 1, fuzz }
                        });
                    }
                    else {
                        // ガラス (Dielectric)
                        scene.push_back({
                            {(float)center.x(), (float)center.y(), (float)center.z()},
                            0.2f,
                            {0.0f, 0.0f, 0.0f}, 0.0f,
                            { {1.0f, 1.0f, 1.0f}, 2, 1.5f }
                        });
                    }
                }
            }
        }

        // 3つの大球体
        scene.push_back({ {0.0f, 1.0f, 0.0f}, 1.0f,  {0.0f, 0.0f, 0.0f}, 0.0f, { {1.0f, 1.0f, 1.0f}, 2, 1.5f } }); // ガラス
        scene.push_back({ {-4.0f, 1.0f, 0.0f}, 1.0f, {0.0f, 0.0f, 0.0f}, 0.0f, { {0.4f, 0.2f, 0.1f}, 0, 0.0f } }); // 拡散
        scene.push_back({ {4.0f, 1.0f, 0.0f}, 1.0f,  {0.0f, 0.0f, 0.0f}, 0.0f, { {0.7f, 0.6f, 0.5f}, 1, 0.0f } }); // 金属
#elseif false
        GpuMaterial checker_mat = {};
        checker_mat.type = 0;
        checker_mat.text_type = 1;
        checker_mat.scale = 0.32f;

        // 色1: 暗めの緑 (0.2, 0.3, 0.1)
        checker_mat.albedo[0] = 0.2f; checker_mat.albedo[1] = 0.3f; checker_mat.albedo[2] = 0.1f;
        // 色2: 明るい白 (0.9, 0.9, 0.9)
        checker_mat.albedo2[0] = 0.9f; checker_mat.albedo2[1] = 0.9f; checker_mat.albedo2[2] = 0.9f;

        // 下の球
        scene.push_back({
            {0.0f, -10.0f, 0.0f}, 10.0f,
            {0.0f, 0.0f, 0.0f}, 0.0f,
            checker_mat
            });

        // 上の球
        scene.push_back({
            {0.0f, 10.0f, 0.0f}, 10.0f,
            {0.0f, 0.0f, 0.0f}, 0.0f,
            checker_mat
            });
#elseif false
        GpuMaterial earth_mat = {};
        earth_mat.type = 0;       // Lambertian
        earth_mat.text_type = 2;  // 2: Image Texture

        // 中心 (0, 0, 0), 半径 2.0 の地球儀
        scene.push_back({
            {0.0f, 0.0f, 0.0f}, 2.0f,
            {0.0f, 0.0f, 0.0f}, 0.0f,
            earth_mat
            });
#else
        GpuMaterial perlin_mat = {};
        perlin_mat.type = 0;        // Lambertian
        perlin_mat.text_type = 3;   // 3: Perlin Marble
        perlin_mat.scale = 4.0f;    // 周波数スケール
        perlin_mat.albedo[0] = 1.0f;
        perlin_mat.albedo[1] = 1.0f;
        perlin_mat.albedo[2] = 1.0f;
        
        // 地面の巨大球 (半径 1000)
        scene.push_back({
            {0.0f, -1000.0f, 0.0f}, 1000.0f,
            {0.0f, 0.0f, 0.0f}, 0.0f,
            perlin_mat
            });
        
        // 浮遊球 (半径 2)
        scene.push_back({
            {0.0f, 2.0f, 0.0f}, 2.0f,
            {0.0f, 0.0f, 0.0f}, 0.0f,
            perlin_mat
            });
#endif
        // --------------------------------------------------------------------------
        // BVHの構築とフラット化
        // --------------------------------------------------------------------------
        std::vector<int> sphere_indices(scene.size());
        for (int i = 0; i < scene.size(); ++i) sphere_indices[i] = i;

        std::vector<GpuBvhNode> bvh_nodes;
        // ルートノードから構築を開始（戻り値は常に0になるはず）
        build_flat_bvh(scene, sphere_indices, 0, static_cast<int>(scene.size()), bvh_nodes);

        // --------------------------------------------------------------------------
        // 描画準備
        // --------------------------------------------------------------------------

        // 描画時間計測タイマーセット
        auto start_time = std::chrono::high_resolution_clock::now();

        // --------------------------------------------------------------------------
        // StructureBufferとSRVの作成 球体(t0)
        // --------------------------------------------------------------------------
        D3D11_BUFFER_DESC sbDesc = {};
        sbDesc.ByteWidth = static_cast<UINT>(sizeof(GpuSphere) * scene.size());
        sbDesc.Usage = D3D11_USAGE_DEFAULT;
        sbDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        sbDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;   // 構造体用の設定
        sbDesc.StructureByteStride = sizeof(GpuSphere);

        D3D11_SUBRESOURCE_DATA sbInitData = {};
        sbInitData.pSysMem = scene.data();

        ComPtr<ID3D11Buffer> sphereBuffer;
        hr = device->CreateBuffer(&sbDesc, &sbInitData, &sphereBuffer);
        CheckHR(hr);

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = static_cast<UINT>(scene.size());

        ComPtr<ID3D11ShaderResourceView> sphereSRV;
        hr = device->CreateShaderResourceView(sphereBuffer.Get(), &srvDesc, &sphereSRV);
        CheckHR(hr);

        // --------------------------------------------------------------------------
        // StructureBufferとSRVの作成 VBHノード(t1)
        // --------------------------------------------------------------------------
        D3D11_BUFFER_DESC bvhDesc = {};
        bvhDesc.ByteWidth = static_cast<UINT>(sizeof(GpuBvhNode) * bvh_nodes.size());
        bvhDesc.Usage = D3D11_USAGE_DEFAULT;
        bvhDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        bvhDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        bvhDesc.StructureByteStride = sizeof(GpuBvhNode);

        D3D11_SUBRESOURCE_DATA bvhInitData = {};
        bvhInitData.pSysMem = bvh_nodes.data();

        ComPtr<ID3D11Buffer> bvhBuffer;
        hr = device->CreateBuffer(&bvhDesc, &bvhInitData, &bvhBuffer);
        CheckHR(hr);

        D3D11_SHADER_RESOURCE_VIEW_DESC bvhSrvDesc = {};
        bvhSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
        bvhSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        bvhSrvDesc.Buffer.FirstElement = 0;
        bvhSrvDesc.Buffer.NumElements = static_cast<UINT>(bvh_nodes.size());

        ComPtr<ID3D11ShaderResourceView> bvhSRV;
        hr = device->CreateShaderResourceView(bvhBuffer.Get(), &bvhSrvDesc, &bvhSRV);
        CheckHR(hr);

        // --------------------------------------------------------------------------
        // サンプラーステートの作成(s0)
        // --------------------------------------------------------------------------
        D3D11_SAMPLER_DESC sampDesc = {};
        sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;  // バイリニア補間
        sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;     // 経度方向はリピート
        sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;    // 緯度（極）方向はクランプ
        sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sampDesc.MinLOD = 0;
        sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

        ComPtr<ID3D11SamplerState> linearSampler;
        hr = device->CreateSamplerState(&sampDesc, &linearSampler);
        CheckHR(hr);

#if false
        // --------------------------------------------------------------------------
        // テクスチャSRVの作成(t2)
        // 現時点では1x1の白ピクセル。世界地図実装時はここを画像データに置き換える
        // --------------------------------------------------------------------------
        uint32_t white_pixel = 0xFFFFFFFF;
        D3D11_TEXTURE2D_DESC imgDesc = {};
        imgDesc.Width = 1;
        imgDesc.Height = 1;
        imgDesc.MipLevels = 1;
        imgDesc.ArraySize = 1;
        imgDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        imgDesc.SampleDesc.Count = 1;
        imgDesc.Usage = D3D11_USAGE_IMMUTABLE;
        imgDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA imgInitData = {};
        imgInitData.pSysMem = &white_pixel;
        imgInitData.SysMemPitch = sizeof(uint32_t);
#else

        // --------------------------------------------------------------------------
        // 画像テクスチャのロードとSRV作成(t2)
        // --------------------------------------------------------------------------
        std::string texture_name = "earthmap.jpg";
        rtw_image earth_img(texture_name.c_str());
        if (earth_img.width() <= 0) {
            std::cerr << "Warning: Failed to load " << texture_name << " Check file path." << std::endl;
        }

        UINT tex_width  = static_cast<UINT>(earth_img.width() > 0 ? earth_img.width(): 1);
        UINT tex_height = static_cast<UINT>(earth_img.height() > 0 ? earth_img.height() : 1);

        // 3バイトRGBから4バイトRGBAへの変換
        std::vector<uint8_t> rgba_data(tex_width * tex_height * 4, 255);
        if (earth_img.width() > 0) {
            for (UINT y = 0; y < tex_height; ++y) {
                for (UINT x = 0; x < tex_width; ++x) {
                    auto p = earth_img.pixel_data(x, y);
                    size_t idx = (y * tex_width + x) * 4;
                    rgba_data[idx + 0] = p[0];
                    rgba_data[idx + 1] = p[1];
                    rgba_data[idx + 2] = p[2];
                    rgba_data[idx + 3] = 255;
                }
            }
        }

        D3D11_TEXTURE2D_DESC imgDesc = {};
        imgDesc.Width = tex_width;
        imgDesc.Height = tex_height;
        imgDesc.MipLevels = 1;
        imgDesc.ArraySize = 1;
        imgDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        imgDesc.SampleDesc.Count = 1;
        imgDesc.Usage = D3D11_USAGE_IMMUTABLE;
        imgDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA imgInitData = {};
        imgInitData.pSysMem = rgba_data.data();
        imgInitData.SysMemPitch = tex_width * 4;        // 1行あたりのバイト数

#endif
        ComPtr<ID3D11Texture2D> imageTex;
        hr = device->CreateTexture2D(&imgDesc, &imgInitData, &imageTex);
        CheckHR(hr);

        ComPtr<ID3D11ShaderResourceView> imageSRV;
        hr = device->CreateShaderResourceView(imageTex.Get(), nullptr, &imageSRV);
        CheckHR(hr);

        // カメラパラメータの計算
        point3 lookfrom(13, 2, 3);
        point3 lookat(0, 0, 0);
        vec3 vup(0,1,0);

        double vfov = 20.0;
        double defocus_angle = 0.0;
        double focus_dist = 10.0;

        auto theta = degrees_to_radians(vfov);
        auto h = std::tan(theta / 2.0);
        auto viewport_height = 2.0 * h * focus_dist;
        auto viewport_width = viewport_height * ((double)image_width / (double)image_height);

        auto w = unit_vector(lookfrom - lookat);
        auto u = unit_vector(cross(vup, w));
        auto v = cross(w, u);

        auto viewport_u = viewport_width * u;
        auto viewport_v = viewport_height * -v;

        auto pixel_delta_u = viewport_u / (double)image_width;
        auto pixel_delta_v = viewport_v / (double)image_height;

        auto viewport_upper_left = lookfrom - (focus_dist * w) - viewport_u / 2.0 - viewport_v / 2.0;
        auto pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

        auto defocus_radius = focus_dist * std::tan(degrees_to_radians(defocus_angle / 2.0));
        auto defocus_disk_u = u * defocus_radius;
        auto defocus_disk_v = v * defocus_radius;

        // バッファ転送用データに格納
        CameraBufferData cbData = {};
        cbData.camera_center[0] = (float)lookfrom.x();
        cbData.camera_center[1] = (float)lookfrom.y();
        cbData.camera_center[2] = (float)lookfrom.z();
        cbData.num_spheres = static_cast<int>(scene.size());

        cbData.pixel00_loc[0] = (float)pixel00_loc.x();
        cbData.pixel00_loc[1] = (float)pixel00_loc.y();
        cbData.pixel00_loc[2] = (float)pixel00_loc.z();
        cbData.samples_per_pixel = samples_per_pixel;

        cbData.pixel_delta_u[0] = (float)pixel_delta_u.x();
        cbData.pixel_delta_u[1] = (float)pixel_delta_u.y();
        cbData.pixel_delta_u[2] = (float)pixel_delta_u.z();
        cbData.max_depth = max_depth;

        cbData.pixel_delta_v[0] = (float)pixel_delta_v.x();
        cbData.pixel_delta_v[1] = (float)pixel_delta_v.y();
        cbData.pixel_delta_v[2] = (float)pixel_delta_v.z();
        cbData.defocus_angle = (float)defocus_angle;

        cbData.defocus_disk_u[0] = (float)defocus_disk_u.x();
        cbData.defocus_disk_u[1] = (float)defocus_disk_u.y();
        cbData.defocus_disk_u[2] = (float)defocus_disk_u.z();

        cbData.defocus_disk_v[0] = (float)defocus_disk_v.x();
        cbData.defocus_disk_v[1] = (float)defocus_disk_v.y();
        cbData.defocus_disk_v[2] = (float)defocus_disk_v.z();

        // 定数バッファの作成
        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.ByteWidth = sizeof(CameraBufferData);    // 16の倍数
        cbDesc.Usage = D3D11_USAGE_DEFAULT;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = &cbData;

        ComPtr<ID3D11Buffer> cameraBuffer;
        hr = device->CreateBuffer(&cbDesc, &initData, &cameraBuffer);
        CheckHR(hr);

        //-----------------------------------------------------------------------------------------------
        // 1:GPU書き込み用テクスチャ（Default）& UAV(Unordered Access View：順序なしアクセスビュー)の作成
        //-----------------------------------------------------------------------------------------------
        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = image_width;
        texDesc.Height = image_height;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;

        ComPtr<ID3D11Texture2D> outputTexture;
        hr = device->CreateTexture2D(&texDesc, nullptr, &outputTexture);
        CheckHR(hr);

        D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = texDesc.Format;
        uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;

        ComPtr<ID3D11UnorderedAccessView> uav;
        hr = device->CreateUnorderedAccessView(outputTexture.Get(), &uavDesc, &uav);
        CheckHR(hr);

        //-----------------------------------------------------------------------------------------------
        // 2:CPU読み取り用ステージングテクスチャ(Staging)の作成
        //-----------------------------------------------------------------------------------------------
        D3D11_TEXTURE2D_DESC stagingDesc = texDesc;
        stagingDesc.Usage = D3D11_USAGE_STAGING;
        stagingDesc.BindFlags = 0;
        stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

        ComPtr<ID3D11Texture2D> stagingTexture;
        hr = device->CreateTexture2D(&stagingDesc, nullptr, &stagingTexture);
        CheckHR(hr);

        //-----------------------------------------------------------------------------------------------
        // 3:Compute Shader の実行（パイプラインへバインド & Dispatch）
        //-----------------------------------------------------------------------------------------------
        std::clog << "Dispatching Compute Shader..." << std::endl;

        // シェーダーをスロット0にバインド
        context->CSSetShader(computeShader.Get(), nullptr, 0);

        // SRV（球体バッファ:t0）とSRV（BVHバッファ:t1）をバインド
        ID3D11ShaderResourceView* srvArray[] = { sphereSRV.Get(), bvhSRV.Get(), imageSRV.Get()};
        context->CSSetShaderResources(0, 3, srvArray);

        // サンプラーをセット
        ID3D11SamplerState* samplerArray[] = { linearSampler.Get() };
        context->CSSetSamplers(0, 1, samplerArray);

        // 定数バッファをスロット0(b0)にバインド
        ID3D11Buffer* cbArray[] = { cameraBuffer.Get() };
        context->CSSetConstantBuffers(0, 1, cbArray);

        ID3D11UnorderedAccessView* uavArray[] = { uav.Get() };
        context->CSSetUnorderedAccessViews(0, 1, uavArray, nullptr);

        // スレッドグループ数を計算（1グループあたり8x8 = 64スレッド）
        UINT dispatchX = (image_width + 7) / 8;
        UINT dispatchY = (image_height + 7) / 8;
        context->Dispatch(dispatchX, dispatchY, 1);

        // 使用したリソースのバインドを解除（後続の安全のため）
        ID3D11UnorderedAccessView* nullUAV[] = { nullptr };
        context->CSSetUnorderedAccessViews(0, 1, nullUAV, nullptr);

        //-----------------------------------------------------------------------------------------------
        // 4:GPUからCPUへの転送とデータ読み取り（Readback）
        //-----------------------------------------------------------------------------------------------
        // GPU内部でDefaultからStagingへテクスチャを丸ごとコピー
        context->CopyResource(stagingTexture.Get(), outputTexture.Get());

        D3D11_MAPPED_SUBRESOURCE mapped = {};
        hr = context->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
        CheckHR(hr);

        //-----------------------------------------------------------------------------------------------
        // 5:PPMファイルへの書き出し
        //-----------------------------------------------------------------------------------------------
        std::clog << "Saving to image.ppm..." << std::endl;
        std::cout << "P3\n" << image_width << ' ' << image_height << "\n255\n";

        // メモリの各行の先頭アドレスをRowPitch（パディング考慮）を使って走査
        const uint8_t* rowPtr = static_cast<const uint8_t*>(mapped.pData);

        for (UINT y = 0; y < image_height; ++y) {
            const float* pixel = reinterpret_cast<const float*>(rowPtr);
            for (UINT x = 0; x < image_width; x++) {
                // float4 (R, G, B, A)を取得して0～255に変換
                int r = static_cast<int>(255.999f * pixel[0]);
                int g = static_cast<int>(255.999f * pixel[1]);
                int b = static_cast<int>(255.999f * pixel[2]);

                std::cout << r << ' ' << g << ' ' << b << '\n';
                pixel += 4; // 次のピクセル（float4分進める）
            }
            rowPtr += mapped.RowPitch;
        }

        context->Unmap(stagingTexture.Get(), 0);

        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end_time - start_time;

        std::clog << "\rDone. (" << elapsed.count() << " seconds)                       \n";
    }
    catch (const std::exception e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

int main() {

    if (d3d_exe())
    {
        return 1;
    }

    switch (0) {
        case 1:bouncing_spheres(); break;
        case 2:checkered_spheres(); break;
        case 3:earth(); break;
        case 4:perlin_spheres(); break;
        case 5:quads(); break;
        case 6:sample_light(); break;
        case 7:cornell_box(); break;
        case 8:cornell_smoke(); break;
        case 9:final_scene(800, 10000, 40); break;
    }
}