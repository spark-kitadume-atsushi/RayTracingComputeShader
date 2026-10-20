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

        const UINT image_width = 400;
        const UINT image_height = 225;

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

        // シェーダーとUAVをスロット0にバインド
        context->CSSetShader(computeShader.Get(), nullptr, 0);
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

        std::clog << "Done! image.ppm generated." << std::endl;
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