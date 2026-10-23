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
#include <thread>

// ComPtr を短く書くための記述
using Microsoft::WRL::ComPtr;

#define FINAL_SCENE_HIGH_DETAIL

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

// Y軸周りの3D回転ヘルパ
inline vec3 rotate_vec_y(const vec3& v, double angle_deg) {
    auto rad = degrees_to_radians(angle_deg);
    auto sin_theta = std::sin(rad);
    auto cos_theta = std::cos(rad);
    return vec3(
        cos_theta * v.x() + sin_theta * v.z(),
        v.y(),
        -sin_theta * v.x() + cos_theta * v.z()
    );
}

GpuMaterial make_solid_material(const color& albedo, int type, float param)
{
    GpuMaterial mat = {};
    albedo.copy_to(mat.albedo);
    mat.type = type;
    mat.param = param;
    return mat;
}

// Spehre作成ヘルパ
GpuSphere make_gpu_sphere(const point3& center, float radius, const vec3& center_vec, const GpuMaterial& mat)
{
    GpuSphere s = {};
    center.copy_to(s.center);
    s.radius = radius;
    center_vec.copy_to(s.center_vec);
    s.mat = mat;
    return s;
}

GpuSphere make_gpu_sphere(const point3& center, float radius, const vec3& center_vec, const color& albedo, int type, float param)
{
    auto mat = make_solid_material(albedo, type, param);
    return make_gpu_sphere(center, radius, center_vec, mat);
}

// GPU
GpuAabb compute_sphere_aabb(const GpuSphere& s)
{
    GpuAabb bbox;
    // 静止球と動く球、両方に対応する
    float c0[3] = { s.center[0], s.center[1] , s.center[2] };
    float c1[3] = { s.center[0] + s.center_vec[0],s.center[1] + s.center_vec[1] ,s.center[2] + s.center_vec[2] };

    for (int i = 0; i < 3;i++) {
        bbox.min_point[i] = std::fmin(c0[i], c1[i]) - s.radius;
        bbox.max_point[i] = std::fmax(c0[i], c1[i]) + s.radius;
    }

    return bbox;
}

// Quad作成ヘルパ
GpuQuad make_gpu_quad(const point3& Q, const vec3& u, const vec3& v, const GpuMaterial& mat)
{
    GpuQuad q = {};
    Q.copy_to(q.Q);
    u.copy_to(q.u);
    v.copy_to(q.v);

    auto n = cross(u, v);
    auto normal = unit_vector(n);
    normal.copy_to(q.normal);
    q.D = (float)dot(normal, Q);

    auto w = n / dot(n, n);
    w.copy_to(q.w);
    q.mat = mat;
    return q;
}

GpuAabb compute_quad_aabb(const GpuQuad& q) {
    point3 p0 = vec3::convert(q.Q);
    point3 p1 = p0 + vec3::convert(q.u);
    point3 p2 = p0 + vec3::convert(q.v);
    point3 p3 = p0 + vec3::convert(q.u) + vec3::convert(q.v);

    point3 min_p(
        std::fmin(std::fmin(p0.x(), p1.x()), std::fmin(p2.x(), p3.x())),
        std::fmin(std::fmin(p0.y(), p1.y()), std::fmin(p2.y(), p3.y())),
        std::fmin(std::fmin(p0.z(), p1.z()), std::fmin(p2.z(), p3.z()))
    );

    point3 max_p(
        std::fmax(std::fmax(p0.x(), p1.x()), std::fmax(p2.x(), p3.x())),
        std::fmax(std::fmax(p0.y(), p1.y()), std::fmax(p2.y(), p3.y())),
        std::fmax(std::fmax(p0.z(), p1.z()), std::fmax(p2.z(), p3.z()))
    );

    // 平面で厚みが0になるのを防ぐ微笑幅パディング
    GpuAabb bbox = {};
    const float eps = 0.0001f;
    for (int i = 0; i < 3; ++i) {
        bbox.min_point[i] = (float)min_p[i] - eps;
        bbox.max_point[i] = (float)max_p[i] + eps;
    }

    return bbox;
}

GpuAabb compute_box_volume_aabb(const GpuVolume& vol) {
    point3 bmin(vol.box_min);
    point3 bmax(vol.box_max);
    vec3 offset(vol.translation);

    point3 corners[8] = {
        point3(bmin.x(), bmin.y(), bmin.z()),
        point3(bmax.x(), bmin.y(), bmin.z()),
        point3(bmin.x(), bmax.y(), bmin.z()),
        point3(bmax.x(), bmax.y(), bmin.z()),
        point3(bmin.x(), bmin.y(), bmax.z()),
        point3(bmax.x(), bmin.y(), bmax.z()),
        point3(bmin.x(), bmax.y(), bmax.z()),
        point3(bmax.x(), bmax.y(), bmax.z())
    };

    point3 min_p(1e9, 1e9, 1e9);
    point3 max_p(-1e9, -1e9, -1e9);

    for (int i = 0; i < 8; ++i) {
        point3 p = rotate_vec_y(corners[i], vol.rotate_y_deg) + offset;
        min_p = point3(std::fmin(min_p.x(), p.x()), std::fmin(min_p.y(), p.y()), std::fmin(min_p.z(), p.z()));
        max_p = point3(std::fmax(max_p.x(), p.x()), std::fmax(max_p.y(), p.y()), std::fmax(max_p.z(), p.z()));
    }

    GpuAabb bbox = {};
    const float eps = 0.0001f;
    for (int i = 0; i < 3; ++i) {
        bbox.min_point[i] = (float)min_p[i] - eps;
        bbox.max_point[i] = (float)max_p[i] + eps;
    }
    return bbox;
}

GpuAabb compute_volume_aabb(const GpuVolume& vol) {
    if (vol.shape_type == 0) {  // Sphere
        GpuAabb bbox = {};
        for (int i = 0; i < 3; ++i) {
            bbox.min_point[i] = vol.center[i] - vol.radius;
            bbox.max_point[i] = vol.center[i] + vol.radius;
        }
        return bbox;
    }
    else {
        return compute_box_volume_aabb(vol);
    }
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

#if false
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
#else
int build_flat_bvh_generic(std::vector<PrimitiveRef>& prims, int start, int end, std::vector<GpuBvhNode>& flat_nodes) {
    int span = end - start;
    int current_node_index = static_cast<int>(flat_nodes.size());
    flat_nodes.push_back(GpuBvhNode());

    if (span == 1) {
        flat_nodes[current_node_index].left_child_index = -1;
        flat_nodes[current_node_index].right_child_index = -1;
        flat_nodes[current_node_index].primitive_type = prims[start].type;
        flat_nodes[current_node_index].primitive_index = prims[start].index;
        flat_nodes[current_node_index].bbox = prims[start].bbox;
        return current_node_index;
    }

    GpuAabb bounds = prims[start].bbox;
    for (int i = start + 1; i < end; i++) {
        bounds = merge_aabb(bounds, prims[i].bbox);
    }

    float extent[3] = {
        bounds.max_point[0] - bounds.min_point[0],
        bounds.max_point[1] - bounds.min_point[1],
        bounds.max_point[2] - bounds.min_point[2]
    };

    int axis = 0;
    if (extent[1] > extent[0] && extent[1] > extent[2])
        axis = 1;
    else if (extent[2] > extent[0] && extent[2] > extent[1])
        axis = 2;

    std::sort(prims.begin() + start, prims.begin() + end,
        [axis](const PrimitiveRef& a, const PrimitiveRef& b) {
            return a.center[axis] < b.center[axis];
        });

    int mid = start + span / 2;
    int left_idx = build_flat_bvh_generic(prims, start, mid, flat_nodes);
    int right_idx = build_flat_bvh_generic(prims, mid, end, flat_nodes);

    flat_nodes[current_node_index].left_child_index = left_idx;
    flat_nodes[current_node_index].right_child_index = right_idx;
    flat_nodes[current_node_index].primitive_type = -1;
    flat_nodes[current_node_index].primitive_index = -1;
    flat_nodes[current_node_index].bbox = merge_aabb(flat_nodes[left_idx].bbox, flat_nodes[right_idx].bbox);

    return current_node_index;
}
#endif

// 直方体
void add_box(
    const point3& a, const point3& b,
    const GpuMaterial& mat,
    double angle_deg,
    const vec3& offset,
    std::vector<GpuQuad>& quad_scene)
{
    point3 min_p(std::fmin(a.x(), b.x()), std::fmin(a.y(), b.y()), std::fmin(a.z(), b.z()));
    point3 max_p(std::fmax(a.x(), b.x()), std::fmax(a.y(), b.y()), std::fmax(a.z(), b.z()));

    vec3 dx(max_p.x() - min_p.x(), 0, 0);
    vec3 dy(0, max_p.y() - min_p.y(), 0);
    vec3 dz(0, 0, max_p.z() - min_p.z());

    struct Face { point3 Q; vec3 u; vec3 v; };
    Face faces[6] = {
        { point3(min_p.x(), min_p.y(), max_p.z()),  dx,  dy }, // 手前 (front)
        { point3(max_p.x(), min_p.y(), max_p.z()), -dz,  dy }, // 右面 (right)
        { point3(max_p.x(), min_p.y(), min_p.z()), -dx,  dy }, // 奥面 (back)
        { point3(min_p.x(), min_p.y(), min_p.z()),  dz,  dy }, // 左面 (left)
        { point3(min_p.x(), max_p.y(), max_p.z()),  dx, -dz }, // 上面 (top)
        { point3(min_p.x(), min_p.y(), min_p.z()),  dx,  dz }  // 底面 (bottom)
    };

    for (const auto& f : faces) {
        // 頂点Qは回転、平衡移動
        point3 Q_rot = rotate_vec_y(f.Q, angle_deg) + offset;
        // ベクトルu,vは変位なので回転のみ
        vec3 u_rot = rotate_vec_y(f.u, angle_deg);
        vec3 v_rot = rotate_vec_y(f.v, angle_deg);

        quad_scene.push_back(make_gpu_quad(Q_rot, u_rot, v_rot, mat));
    }
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
#ifdef FINAL_SCENE_HIGH_DETAIL
        const UINT image_width = 800;
        const UINT image_height = 800;
        const int total_samples = 10000;
        const int max_depth = 40;
#else
        const UINT image_width = 400;
        const UINT image_height = 400;
        const int total_samples = 250;
        const int max_depth = 4;
#endif
        const int samples_per_pass = 50;
        const int num_pass = total_samples / samples_per_pass;
        // --------------------------------------------------------------------------
        // シーン配置物
        // --------------------------------------------------------------------------
#if false
        std::vector<GpuSphere> scene;

#if false
        // 地面 (静止)
        scene.push_back(make_gpu_sphere({ 0.0f, -1000.0f, 0.0f }, 1000.0f, { 0.0f, 0.0f, 0.0f }, { 0.5f, 0.5f, 0.5f }, 0, 0.0f));

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
                        scene.push_back(make_gpu_sphere(center, 0.2f, center_vec, albedo, 0, 0.0f));
                    }
                    else if (choose_mat < 0.95) {
                        // 金属 (Metal)
                        auto albedo = color::random(0.5, 1);
                        auto fuzz = (float)random_double(0, 0.5);
                        scene.push_back(make_gpu_sphere(center, 0.2f, { 0.0f, 0.0f, 0.0f }, albedo, 1, fuzz));
                    }
                    else {
                        // ガラス (Dielectric)
                        scene.push_back(make_gpu_sphere(center, 0.2f, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 2, 1.5f));
                    }
                }
            }
        }

        // 3つの大球体
        scene.push_back(make_gpu_sphere({0.0f, 1.0f, 0.0f},  1.0f, {0.0f, 0.0f, 0.0f}, { 1.0f, 1.0f, 1.0f}, 2, 1.5f)); // ガラス
        scene.push_back(make_gpu_sphere({-4.0f, 1.0f, 0.0f}, 1.0f, {0.0f, 0.0f, 0.0f}, { 0.4f, 0.2f, 0.1f}, 0, 0.0f)); // 拡散
        scene.push_back(make_gpu_sphere({4.0f, 1.0f, 0.0f},  1.0f, {0.0f, 0.0f, 0.0f}, { 0.7f, 0.6f, 0.5f}, 1, 0.0f)); // 金属
#elif false
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
#elif false
        GpuMaterial earth_mat = {};
        earth_mat.type = 0;       // Lambertian
        earth_mat.text_type = 2;  // 2: Image Texture

        // 中心 (0, 0, 0), 半径 2.0 の地球儀
        scene.push_back({
            {0.0f, 0.0f, 0.0f}, 2.0f,
            {0.0f, 0.0f, 0.0f}, 0.0f,
            earth_mat
            });
#elif false
        GpuMaterial perlin_mat = {};
        perlin_mat.type = 0;        // Lambertian
        perlin_mat.text_type = 3;   // 3: Perlin Marble
        perlin_mat.scale = 4.0f;    // 周波数スケール
        perlin_mat.albedo[0] = 1.0f;
        perlin_mat.albedo[1] = 1.0f;
        perlin_mat.albedo[2] = 1.0f;
        
        // 地面の巨大球 (半径 1000)
        scene.push_back(make_gpu_sphere({ 0.0f, -1000.0f, 0.0f }, 1000.0f, { 0.0f, 0.0f, 0.0f }, perlin_mat));
        // 浮遊球 (半径 2)
        scene.push_back(make_gpu_sphere({ 0.0f, 2.0f, 0.0f}, 2.0f, {0.0f, 0.0f, 0.0f}, perlin_mat));
#endif
        // --------------------------------------------------------------------------
        // BVHの構築とフラット化
        // --------------------------------------------------------------------------
        std::vector<int> sphere_indices(scene.size());
        for (int i = 0; i < scene.size(); ++i) sphere_indices[i] = i;

        std::vector<GpuBvhNode> bvh_nodes;
        // ルートノードから構築を開始（戻り値は常に0になるはず）
        build_flat_bvh(scene, sphere_indices, 0, static_cast<int>(scene.size()), bvh_nodes);
#elif false
        // 球体バッファ(サイズ0回避のためにダミーを1要素配置)
        std::vector<GpuSphere> sphere_scene;
        sphere_scene.push_back({});

        // 四角形のシーン構築
        std::vector<GpuQuad> quad_scene;

        // 教科書通りの 5 枚の四角形
        quad_scene.push_back(make_gpu_quad(point3(-3, -2, 5), vec3(0, 0, -4), vec3(0, 4,  0), make_solid_material({ 1.0f, 0.2f, 0.2f }, 0, 0))); // 左: 赤
        quad_scene.push_back(make_gpu_quad(point3(-2, -2, 0), vec3(4, 0,  0), vec3(0, 4,  0), make_solid_material({ 0.2f, 1.0f, 0.2f }, 0, 0))); // 奥: 緑
        quad_scene.push_back(make_gpu_quad(point3( 3, -2, 1), vec3(0, 0,  4), vec3(0, 4,  0), make_solid_material({ 0.2f, 0.2f, 1.0f }, 0, 0))); // 右: 青
        quad_scene.push_back(make_gpu_quad(point3(-2,  3, 1), vec3(4, 0,  0), vec3(0, 0,  4), make_solid_material({ 1.0f, 0.5f, 0.0f }, 0, 0))); // 上: 橙
        quad_scene.push_back(make_gpu_quad(point3(-2, -3, 5), vec3(4, 0,  0), vec3(0, 0, -4), make_solid_material({ 0.2f, 0.8f, 0.8f }, 0, 0))); // 下: 青緑

        // BVH用参照リストの作成（Quadを登録）
        std::vector<PrimitiveRef> prim_refs;
        for (size_t i = 0; i < quad_scene.size(); ++i) {
            PrimitiveRef ref;
            ref.type = 1;       // Quad
            ref.index = static_cast<int>(i);
            ref.bbox = compute_quad_aabb(quad_scene[i]);
            ref.center[0] = (ref.bbox.min_point[0] + ref.bbox.max_point[0]) * 0.5f;
            ref.center[1] = (ref.bbox.min_point[1] + ref.bbox.max_point[1]) * 0.5f;
            ref.center[2] = (ref.bbox.min_point[2] + ref.bbox.max_point[2]) * 0.5f;
            prim_refs.push_back(ref);
        }

        std::vector<GpuBvhNode> bvh_nodes;
        build_flat_bvh_generic(prim_refs, 0, static_cast<int>(prim_refs.size()), bvh_nodes);

        auto scene = sphere_scene;
#elif true
        // sample_light()シーンの構築
        std::vector<GpuSphere> sphere_scene;
        std::vector<GpuQuad> quad_scene;
        std::vector<GpuVolume> volume_scene;
#if false
        // マテリアル定義
        GpuMaterial perlin_mat = {};
        perlin_mat.type = 0;        // Lambertian
        perlin_mat.text_type = 3;   // Perlin Marble
        perlin_mat.scale = 4.0f;
        color(1.0f, 1.0f, 1.0f).copy_to(perlin_mat.albedo);

        GpuMaterial light_sphere_mat = {};
        light_sphere_mat.type = 3;  // 3: DiffuseLight
        color(0.2f, 0.2f, 0.8f).copy_to(light_sphere_mat.albedo);   // 青白い弱光

        GpuMaterial light_quad_mat = {};
        light_quad_mat.type = 3;    // 3: DiffuseLight
        color(4.0f, 4.0f, 4.0f).copy_to(light_quad_mat.albedo);     // 白い強光

        // 球体 3 つ
        // 1. 地面の大理石球
        sphere_scene.push_back(make_gpu_sphere({0.0f, -1000.0f, 0.0f}, 1000.0f, {0.0f, 0.0f, 0.0f}, perlin_mat));
        // 2. 中央の大理石球
        sphere_scene.push_back(make_gpu_sphere({0.0f, 2.0f, 0.0f}, 2.0f, {0.0f, 0.0f, 0.0f}, perlin_mat));
        // 3. 上空の発光球体ライト
        sphere_scene.push_back(make_gpu_sphere({ 0.0f, 7.0f, 0.0f }, 2.0f, { 0.0f, 0.0f, 0.0f }, light_sphere_mat));

        // 四角形 1 つ（手前の高輝度ライトパネル）
        quad_scene.push_back(make_gpu_quad(
            point3(2, 1, -2), vec3(2, 0, 0), vec3(0, 2, 0), light_quad_mat
        ));
#elif false

        // 球体バッファ (サイズ 0 回避のダミー)
        sphere_scene.push_back({});

        GpuMaterial red = make_solid_material({ 0.65f, 0.05f, 0.05f }, 0, 0);
        GpuMaterial white = make_solid_material({ 0.73f, 0.73f, 0.73f }, 0, 0);
        GpuMaterial green = make_solid_material({ 0.12f, 0.45f, 0.15f }, 0, 0);

        GpuMaterial light = make_solid_material({ 15.0f, 15.0f, 15.0f }, 3, 0);

        // 1. コーネルボックスの外壁 (5面) と天井ライト (1面)
        quad_scene.push_back(make_gpu_quad(point3(555, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555), green)); // 右壁 (緑)
        quad_scene.push_back(make_gpu_quad(point3(0, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555), red));   // 左壁 (赤)
        quad_scene.push_back(make_gpu_quad(point3(343, 554, 332), vec3(-130, 0, 0), vec3(0, 0, -105), light)); // 天井ライト
        quad_scene.push_back(make_gpu_quad(point3(0, 0, 0), vec3(555, 0, 0), vec3(0, 0, 555), white)); // 床
        quad_scene.push_back(make_gpu_quad(point3(555, 555, 555), vec3(-555, 0, 0), vec3(0, 0, -555), white)); // 天井
        quad_scene.push_back(make_gpu_quad(point3(0, 0, 555), vec3(555, 0, 0), vec3(0, 555, 0), white)); // 奥壁

#if false
        // 箱の追加
        add_box(point3(0, 0, 0), point3(165, 330, 165), white, 15.0, vec3(265, 0, 295), quad_scene);

        add_box(point3(0, 0, 0), point3(165, 165, 165), white, -18.0, vec3(130, 0, 65), quad_scene);
#else
        GpuVolume smoke1 = {};
        smoke1.shape_type = 1;
        smoke1.density = 0.01f;
        color(0, 0, 0).copy_to(smoke1.albedo);
        point3(0, 0, 0).copy_to(smoke1.box_min);
        point3(165, 330, 165).copy_to(smoke1.box_max);
        smoke1.rotate_y_deg = 15.0f;
        vec3(265, 0, 295).copy_to(smoke1.translation);
        volume_scene.push_back(smoke1);

        GpuVolume smoke2 = {};
        smoke2.shape_type = 1; // Box
        smoke2.density = 0.01f;
        color(1.0, 1.0, 1.0).copy_to(smoke2.albedo);
        point3(0, 0, 0).copy_to(smoke2.box_min);
        point3(165, 165, 165).copy_to(smoke2.box_max);
        smoke2.rotate_y_deg = -18.0f;
        vec3(130, 0, 65).copy_to(smoke2.translation);
        volume_scene.push_back(smoke2);
#endif
#elif true
        // -------------------------------------------------------------
        // 1. 地面のランダムな高さの直方体群 (20x20 = 400個のBox = 2400枚のQuad)
        // -------------------------------------------------------------
        GpuMaterial ground_mat = make_solid_material({ 0.48f, 0.83f, 0.53f }, 0, 0.0f);
        int boxes_per_side = 20;
        for (int i = 0; i < boxes_per_side; ++i) {
            for (int j = 0; j < boxes_per_side; ++j) {
                auto w = 100.0;
                auto x0 = -1000.0 + i * w;
                auto z0 = -1000.0 + j * w;
                auto y0 = 0.0;
                auto x1 = x0 + w;
                auto y1 = random_double(1, 101);
                auto z1 = z0 + w;

                add_box(point3(x0, y0, z0), point3(x1, y1, z1), ground_mat, 0.0, vec3(0, 0, 0), quad_scene);
            }
        }

        // -------------------------------------------------------------
        // 2. 天井の面光源 (Quad)
        // -------------------------------------------------------------
        GpuMaterial light_mat = make_solid_material({ 7.0f, 7.0f, 7.0f }, 3, 0.0f);
        quad_scene.push_back(make_gpu_quad(point3(123, 554, 147), vec3(300, 0, 0), vec3(0, 0, 265), light_mat));

        // -------------------------------------------------------------
        // 3. 各種オブジェクト球体群
        // -------------------------------------------------------------
        // モーションブラー球体 (茶色)
        GpuMaterial orange_mat = make_solid_material({ 0.7f, 0.3f, 0.1f }, 0, 0.0f);
        point3 center1(400, 400, 200);
        vec3   center_vec(30, 0, 0);
        sphere_scene.push_back(make_gpu_sphere(center1, 50.0f, center_vec, orange_mat));

        // ガラス球体 (屈折率 1.5)
        GpuMaterial glass_mat = make_solid_material({ 1.0f, 1.0f, 1.0f }, 2, 1.5f);
        sphere_scene.push_back(make_gpu_sphere(point3(260, 150, 45), 50.0f, vec3(0, 0, 0), glass_mat));

        // 金属球体 (粗さ 1.0)
        GpuMaterial metal_mat = make_solid_material({ 0.8f, 0.8f, 0.9f }, 1, 1.0f);
        sphere_scene.push_back(make_gpu_sphere(point3(0, 150, 145), 50.0f, vec3(0, 0, 0), metal_mat));

        // 青いガラス球 (外殻)
        sphere_scene.push_back(make_gpu_sphere(point3(360, 150, 145), 70.0f, vec3(0, 0, 0), glass_mat));

        // 地球儀 (画像テクスチャ)
        GpuMaterial earth_mat = {};
        earth_mat.type = 0;
        earth_mat.text_type = 2; // Image
        sphere_scene.push_back(make_gpu_sphere(point3(400, 200, 400), 100.0f, vec3(0, 0, 0), earth_mat));

        // パーリン大理石球 (スケール 0.2)
        GpuMaterial pertext_mat = {};
        pertext_mat.type = 0;
        pertext_mat.text_type = 3; // Perlin Marble
        pertext_mat.scale = 0.2f;
        pertext_mat.albedo[0] = 1.0f; pertext_mat.albedo[1] = 1.0f; pertext_mat.albedo[2] = 1.0f;
        sphere_scene.push_back(make_gpu_sphere(point3(220, 280, 300), 80.0f, vec3(0, 0, 0), pertext_mat));

        // -------------------------------------------------------------
        // 4. 小さな白い球体群のクラスター (1000個の小球を回転・移動して配置)
        // -------------------------------------------------------------
        GpuMaterial white = make_solid_material({ 0.73f, 0.73f, 0.73f }, 0, 0.0f);
        int ns = 1000;
        for (int j = 0; j < ns; ++j) {
            point3 p(random_double(0, 165), random_double(0, 165), random_double(0, 165));
            point3 p_trans = rotate_vec_y(p, 15.0) + vec3(-100, 270, 395);
            sphere_scene.push_back(make_gpu_sphere(p_trans, 10.0f, vec3(0, 0, 0), white));
        }

        // -------------------------------------------------------------
        // 5. 参加媒質ボリューム群
        // -------------------------------------------------------------
        // 青いガラス球の内部の青い煙 (density = 0.2)
        GpuVolume blue_smoke = {};
        blue_smoke.shape_type = 0; // Sphere
        blue_smoke.density = 0.2f;
        color(0.2f, 0.4f, 0.9f).copy_to(blue_smoke.albedo);
        point3(360, 150, 145).copy_to(blue_smoke.center);
        blue_smoke.radius = 70.0f;
        volume_scene.push_back(blue_smoke);

        // 空間全体を包む薄い白い霧 (density = 0.0001, 半径 5000)
        GpuVolume world_fog = {};
        world_fog.shape_type = 0; // Sphere
        world_fog.density = 0.0001f;
        color(1.0f, 1.0f, 1.0f).copy_to(world_fog.albedo);
        point3(0, 0, 0).copy_to(world_fog.center);
        world_fog.radius = 5000.0f;
        volume_scene.push_back(world_fog);
#endif
        // BVH用参照リストの統合
        std::vector<PrimitiveRef> prim_refs;
        for (size_t i = 0; i < sphere_scene.size(); ++i) {
           prim_refs.emplace_back(
               0,   // Sphere
               static_cast<int>(i),
               compute_sphere_aabb(sphere_scene[i]));
        }
        for (size_t i = 0; i < quad_scene.size(); ++i) {
            prim_refs.emplace_back(
                1,  // Quad
                static_cast<int>(i),
                compute_quad_aabb(quad_scene[i]));
        }
        for (size_t i = 0; i < volume_scene.size(); ++i) {
            prim_refs.emplace_back(
                2,  // Volume
                static_cast<int>(i),
                compute_volume_aabb(volume_scene[i]));
        }

        std::vector<GpuBvhNode> bvh_nodes;
        build_flat_bvh_generic(prim_refs, 0, static_cast<int>(prim_refs.size()), bvh_nodes);

        auto scene = sphere_scene;
#endif
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

        // --------------------------------------------------------------------------
        // StructureBufferとSRVの作成 Quad(t3)
        // --------------------------------------------------------------------------
        D3D11_BUFFER_DESC quadDesc = {};
        quadDesc.ByteWidth = static_cast<UINT>(sizeof(GpuQuad) * quad_scene.size());
        quadDesc.Usage = D3D11_USAGE_DEFAULT;
        quadDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        quadDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        quadDesc.StructureByteStride = sizeof(GpuQuad);

        D3D11_SUBRESOURCE_DATA quadInit = {};
        quadInit.pSysMem = quad_scene.data();

        ComPtr<ID3D11Buffer> quadBuffer;
        hr = device->CreateBuffer(&quadDesc, &quadInit, &quadBuffer);
        CheckHR(hr);

        D3D11_SHADER_RESOURCE_VIEW_DESC quadSrvDesc = {};
        quadSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
        quadSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        quadSrvDesc.Buffer.FirstElement = 0;
        quadSrvDesc.Buffer.NumElements = static_cast<UINT>(quad_scene.size());

        ComPtr<ID3D11ShaderResourceView> quadSRV;
        hr = device->CreateShaderResourceView(quadBuffer.Get(), &quadSrvDesc, &quadSRV);

        // --------------------------------------------------------------------------
        // StructureBufferとSRVの作成 Quad(t4)
        // --------------------------------------------------------------------------
        D3D11_BUFFER_DESC volDesc = {};
        volDesc.ByteWidth = static_cast<UINT>(sizeof(GpuVolume) * volume_scene.size());
        volDesc.Usage = D3D11_USAGE_DEFAULT;
        volDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        volDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        volDesc.StructureByteStride = sizeof(GpuVolume);

        D3D11_SUBRESOURCE_DATA volInit = {};
        volInit.pSysMem = volume_scene.data();

        ComPtr<ID3D11Buffer> volBuffer;
        hr = device->CreateBuffer(&volDesc, &volInit, &volBuffer);
        CheckHR(hr);

        D3D11_SHADER_RESOURCE_VIEW_DESC volSrvDesc = {};
        volSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
        volSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        volSrvDesc.Buffer.FirstElement = 0;
        volSrvDesc.Buffer.NumElements = static_cast<UINT>(volume_scene.size());

        ComPtr<ID3D11ShaderResourceView> volumeSRV;
        hr = device->CreateShaderResourceView(volBuffer.Get(), &volSrvDesc, &volumeSRV);
        CheckHR(hr);
#if true
        point3 lookfrom(478, 278, -600);
        point3 lookat(278, 278, 0);
        vec3   vup(0, 1, 0);

        double vfov = 40.0;
        double defocus_angle = 0.0;
        double focus_dist = 10.0;
#endif
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
        lookfrom.copy_to(cbData.camera_center);
        pixel00_loc.copy_to(cbData.pixel00_loc);

        cbData.samples_this_pass = samples_per_pass;

        pixel_delta_u.copy_to(cbData.pixel_delta_u);
        cbData.max_depth = max_depth;

        pixel_delta_v.copy_to(cbData.pixel_delta_v);
        cbData.defocus_angle = (float)defocus_angle;

        defocus_disk_u.copy_to(cbData.defocus_disk_u);
        defocus_disk_v.copy_to(cbData.defocus_disk_v);

        color(0, 0, 0).copy_to(cbData.background);

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

        // SRVを全てバインド
        ID3D11ShaderResourceView* srvArray[] = {
            sphereSRV.Get(),
            bvhSRV.Get(),
            imageSRV.Get(),
            quadSRV.Get(),
            volumeSRV.Get(),
        };
        context->CSSetShaderResources(0, 5, srvArray);

        // サンプラーをセット
        ID3D11SamplerState* samplerArray[] = { linearSampler.Get() };
        context->CSSetSamplers(0, 1, samplerArray);

        // 定数バッファをスロット0(b0)にバインド
        ID3D11Buffer* cbArray[] = { cameraBuffer.Get() };
        context->CSSetConstantBuffers(0, 1, cbArray);

        ID3D11UnorderedAccessView* uavArray[] = { uav.Get() };
        context->CSSetUnorderedAccessViews(0, 1, uavArray, nullptr);

        // GPU同期用イベントクエリの作成
        D3D11_QUERY_DESC queryDesc = {};
        queryDesc.Query = D3D11_QUERY_EVENT;
        ComPtr<ID3D11Query> query;
        hr = device->CreateQuery(&queryDesc, &query);
        CheckHR(hr);

        // スレッドグループ数を計算（1グループあたり8x8 = 64スレッド）
        UINT dispatchX = (image_width + 7) / 8;
        UINT dispatchY = (image_height + 7) / 8;

        std::clog << "Rendering Final Scene (" << total_samples << " samples)...\n";

        for (int pass = 0; pass < num_pass; ++pass) {
            // パス番号を定数バッファに更新
            cbData.pass_index = pass;
            context->UpdateSubresource(cameraBuffer.Get(), 0, nullptr, &cbData, 0, 0);

            // 1パス分(50サンプル)を実行
            context->Dispatch(dispatchX, dispatchY, 1);

            // GPUにイベントを発行し、このパスの完了を待機
            context->End(query.Get());
            while (context-> GetData(query.Get(), nullptr, 0, 0) == S_FALSE) {
                // 1ミリ秒スリープ
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            // 進捗の計算
            auto current_time = std::chrono::high_resolution_clock::now();
            double elapsed = std::chrono::duration<double>(current_time - start_time).count();
            double progress = (double)(pass + 1) / num_pass;
            double eta = (elapsed / progress) - elapsed;

            std::clog << "\rPass " << (pass + 1) << "/" << num_pass
                      << "[" << static_cast<int>(progress * 100.0) << "%]"
                      << "Elapsed: " << static_cast<int>(elapsed) << "s "
                      << "ETA: " << static_cast<int>(eta) << "s   " << std::flush;
        }

        std::clog << "\nRendering finished! Transferring to CPU...\n";

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
#if false
                // float4 (R, G, B, A)を取得して0～255に変換
                int r = static_cast<int>(255.999f * pixel[0]);
                int g = static_cast<int>(255.999f * pixel[1]);
                int b = static_cast<int>(255.999f * pixel[2]);

                std::cout << r << ' ' << g << ' ' << b << '\n';
#else
                // 合計輝度を総サンプル数で割って平均化
                color pixel_color(
                    pixel[0] / static_cast<float>(total_samples),
                    pixel[1] / static_cast<float>(total_samples),
                    pixel[2] / static_cast<float>(total_samples)
                );
                write_color(std::cout, pixel_color);
#endif
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