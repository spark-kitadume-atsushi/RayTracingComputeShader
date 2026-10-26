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

// BVH構築とフラット化関数
// 戻り値はフラット配列におけるこのノードのインデックス
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

struct SceneConfig {
    std::string name;

    std::vector<GpuSphere>  spheres;
    std::vector<GpuQuad>    quads;
    std::vector<GpuVolume>  volumes;

    std::string texture_filename;

    point3 lookfrom = point3(13, 2, 3);
    point3 lookat   = point3(0, 0, 0);
    vec3   vup      = vec3(0, 1, 0);

    double vfov             = 20.0;
    double defocus_angle    = 0.0;
    double focus_dist       = 10.0;
    color background        = color(-1, -1, -1);

    UINT image_width        = 800;
    UINT image_height       = 800;
    int total_samples       = 250;
    int samples_per_pass    = 50;
    int max_depth           = 40;
};

SceneConfig scene_bouncing_spheres() {
    SceneConfig sc;
    sc.name = "Bouncing_Spheres";
    sc.image_width = 1200;
    sc.image_height = 675;
    sc.defocus_angle = 0.6;
    sc.focus_dist = 10.0;

    sc.spheres.push_back(make_gpu_sphere({ 0.0f, -1000.0f, 0.0f }, 1000.0f, { 0.0f, 0.0f, 0.0f }, { 0.5f, 0.5f, 0.5f }, 0, 0.0f));

    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            auto choose_mat = random_double();
            point3 center(a + 0.9 * random_double(), 0.2, b + 0.9 * random_double());

            if ((center - point3(4, 0.2, 0)).length() > 0.9) {
                if (choose_mat < 0.8) {
                    auto albedo = color::random() * color::random();
                    auto center2 = center + vec3(0, random_double(0, 0.5), 0);
                    sc.spheres.push_back(make_gpu_sphere(center, 0.2f, center2 - center, albedo, 0, 0.0f));
                }
                else if (choose_mat < 0.95) {
                    auto albedo = color::random(0.5, 1);
                    auto fuzz = static_cast<float>(random_double(0, 0.5));
                    sc.spheres.push_back(make_gpu_sphere(center, 0.2f, { 0.0f, 0.0f, 0.0f }, albedo, 1, fuzz));
                }
                else {
                    sc.spheres.push_back(make_gpu_sphere(center, 0.2f, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, 2, 1.5f));
                }
            }
        }
    }

    sc.spheres.push_back(make_gpu_sphere({ 0.0f, 1.0f, 0.0f }, 1.0f, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, 2, 1.5f));
    sc.spheres.push_back(make_gpu_sphere({ -4.0f, 1.0f, 0.0f }, 1.0f, { 0.0f, 0.0f, 0.0f }, { 0.4f, 0.2f, 0.1f }, 0, 0.0f));
    sc.spheres.push_back(make_gpu_sphere({ 4.0f, 1.0f, 0.0f }, 1.0f, { 0.0f, 0.0f, 0.0f }, { 0.7f, 0.6f, 0.5f }, 1, 0.0f));
    return sc;
}

// 第3章: チェッカー球
SceneConfig scene_checkered_spheres() {
    SceneConfig sc;
    sc.name = "Checkered Spheres";
    sc.image_width = 1200;
    sc.image_height = 675;

    GpuMaterial mat = {};
    mat.type = 0;
    mat.text_type = 1;
    mat.scale = 0.32f;
    color(0.2f, 0.3f, 0.1f).copy_to(mat.albedo);
    color(0.9f, 0.9f, 0.9f).copy_to(mat.albedo2);

    sc.spheres.push_back(make_gpu_sphere({ 0.0f, -10.0f, 0.0f }, 10.0f, { 0.0f, 0.0f, 0.0f }, mat));
    sc.spheres.push_back(make_gpu_sphere({ 0.0f,  10.0f, 0.0f }, 10.0f, { 0.0f, 0.0f, 0.0f }, mat));
    return sc;
}

// 第4章: 地球儀 (テクスチャマッピング)
SceneConfig scene_earth() {
    SceneConfig sc;
    sc.name = "Earth";
    sc.texture_filename = "earthmap.jpg";
    sc.lookfrom = point3(0, 0, 12);
    sc.lookat = point3(0, 0, 0);

    GpuMaterial mat = {};
    mat.type = 0;
    mat.text_type = 2; // Image Texture
    sc.spheres.push_back(make_gpu_sphere({ 0.0f, 0.0f, 0.0f }, 2.0f, { 0.0f, 0.0f, 0.0f }, mat));
    return sc;
}

// 第5章: パーリン大理石球
SceneConfig scene_perlin_spheres() {
    SceneConfig sc;
    sc.name = "Perlin Spheres";
    sc.image_width = 1200;
    sc.image_height = 675;

    GpuMaterial mat = {};
    mat.type = 0;
    mat.text_type = 3; // Perlin Marble
    mat.scale = 4.0f;
    color(1.0f, 1.0f, 1.0f).copy_to(mat.albedo);

    sc.spheres.push_back(make_gpu_sphere({ 0.0f, -1000.0f, 0.0f }, 1000.0f, { 0.0f, 0.0f, 0.0f }, mat));
    sc.spheres.push_back(make_gpu_sphere({ 0.0f, 2.0f, 0.0f }, 2.0f, { 0.0f, 0.0f, 0.0f }, mat));
    return sc;
}

// 第6章: 四角形ポリゴン
SceneConfig scene_quads() {
    SceneConfig sc;
    sc.name = "Quads";
    sc.image_width = 400;
    sc.image_height = 400;
    sc.vfov = 80.0;
    sc.lookfrom = point3(0, 0, 9);
    sc.lookat = point3(0, 0, 0);

    sc.quads.push_back(make_gpu_quad(point3(-3, -2, 5), vec3(0, 0, -4), vec3(0, 4, 0), make_solid_material({ 1.0f, 0.2f, 0.2f }, 0, 0)));
    sc.quads.push_back(make_gpu_quad(point3(-2, -2, 0), vec3(4, 0, 0), vec3(0, 4, 0), make_solid_material({ 0.2f, 1.0f, 0.2f }, 0, 0)));
    sc.quads.push_back(make_gpu_quad(point3(3, -2, 1), vec3(0, 0, 4), vec3(0, 4, 0), make_solid_material({ 0.2f, 0.2f, 1.0f }, 0, 0)));
    sc.quads.push_back(make_gpu_quad(point3(-2, 3, 1), vec3(4, 0, 0), vec3(0, 0, 4), make_solid_material({ 1.0f, 0.5f, 0.0f }, 0, 0)));
    sc.quads.push_back(make_gpu_quad(point3(-2, -3, 5), vec3(4, 0, 0), vec3(0, 0, -4), make_solid_material({ 0.2f, 0.8f, 0.8f }, 0, 0)));
    return sc;
}

// 第7章: 面光源
SceneConfig scene_sample_light() {
    SceneConfig sc;
    sc.name = "Light Sample";
    sc.image_width = 1200;
    sc.image_height = 675;
    sc.background = color(0, 0, 0);
    sc.lookfrom = point3(26, 3, 6);
    sc.lookat = point3(0, 2, 0);

    GpuMaterial perlin_mat = {};
    perlin_mat.type = 0;
    perlin_mat.text_type = 3;
    perlin_mat.scale = 4.0f;
    color(1.0f, 1.0f, 1.0f).copy_to(perlin_mat.albedo);

    sc.spheres.push_back(make_gpu_sphere({ 0.0f, -1000.0f, 0.0f }, 1000.0f, { 0.0f, 0.0f, 0.0f }, perlin_mat));
    sc.spheres.push_back(make_gpu_sphere({ 0.0f, 2.0f, 0.0f }, 2.0f, { 0.0f, 0.0f, 0.0f }, perlin_mat));
    sc.spheres.push_back(make_gpu_sphere({ 0.0f, 7.0f, 0.0f }, 2.0f, { 0.0f, 0.0f, 0.0f }, make_solid_material({ 0.2f, 0.2f, 0.8f }, 3, 0)));

    sc.quads.push_back(make_gpu_quad(point3(2, 1, -2), vec3(2, 0, 0), vec3(0, 2, 0), make_solid_material({ 4.0f, 4.0f, 4.0f }, 3, 0)));
    return sc;
}

// 第8章: コーネルボックス
SceneConfig scene_cornell_box() {
    SceneConfig sc;
    sc.name = "Cornell Box";
    sc.image_width = 600;
    sc.image_height = 600;
    sc.vfov = 40.0;
    sc.lookfrom = point3(278, 278, -800);
    sc.lookat = point3(278, 278, 0);
    sc.background = color(0, 0, 0);

    GpuMaterial red = make_solid_material({ 0.65f, 0.05f, 0.05f }, 0, 0);
    GpuMaterial white = make_solid_material({ 0.73f, 0.73f, 0.73f }, 0, 0);
    GpuMaterial green = make_solid_material({ 0.12f, 0.45f, 0.15f }, 0, 0);
    GpuMaterial light = make_solid_material({ 15.0f, 15.0f, 15.0f }, 3, 0);

    sc.quads.push_back(make_gpu_quad(point3(555, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555), green));
    sc.quads.push_back(make_gpu_quad(point3(0, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555), red));
    sc.quads.push_back(make_gpu_quad(point3(343, 554, 332), vec3(-130, 0, 0), vec3(0, 0, -105), light));
    sc.quads.push_back(make_gpu_quad(point3(0, 0, 0), vec3(555, 0, 0), vec3(0, 0, 555), white));
    sc.quads.push_back(make_gpu_quad(point3(555, 555, 555), vec3(-555, 0, 0), vec3(0, 0, -555), white));
    sc.quads.push_back(make_gpu_quad(point3(0, 0, 555), vec3(555, 0, 0), vec3(0, 555, 0), white));

    add_box(point3(0, 0, 0), point3(165, 330, 165), white, 15.0, vec3(265, 0, 295), sc.quads);
    add_box(point3(0, 0, 0), point3(165, 165, 165), white, -18.0, vec3(130, 0, 65), sc.quads);
    return sc;
}

// 第9章: コーネルスモーク
SceneConfig scene_cornell_smoke() {
    SceneConfig sc = scene_cornell_box();
    sc.name = "Cornell Smoke";
    // 固体ボックス（末尾の12枚のQuad）を除去してVolumeに差し替え
    sc.quads.resize(6);

    GpuVolume smoke1 = {};
    smoke1.shape_type = 1;
    smoke1.density = 0.01f;
    color(0, 0, 0).copy_to(smoke1.albedo);
    point3(0, 0, 0).copy_to(smoke1.box_min);
    point3(165, 330, 165).copy_to(smoke1.box_max);
    smoke1.rotate_y_deg = 15.0f;
    vec3(265, 0, 295).copy_to(smoke1.translation);
    sc.volumes.push_back(smoke1);

    GpuVolume smoke2 = {};
    smoke2.shape_type = 1;
    smoke2.density = 0.01f;
    color(1, 1, 1).copy_to(smoke2.albedo);
    point3(0, 0, 0).copy_to(smoke2.box_min);
    point3(165, 165, 165).copy_to(smoke2.box_max);
    smoke2.rotate_y_deg = -18.0f;
    vec3(130, 0, 65).copy_to(smoke2.translation);
    sc.volumes.push_back(smoke2);
    return sc;
}

// 第10章: 最終シーン level:0,1,2
SceneConfig scene_final_scene(int level) {
    SceneConfig sc;
    sc.texture_filename = "earthmap.jpg";

    switch (level) {
    case 0: // 低設定0.6秒程度
        sc.image_width = 400;
        sc.image_height = 400;
        sc.total_samples = 250;
        sc.max_depth = 4;
        break;

    case 1: // 中設定40秒程度
        sc.image_width = 800;
        sc.image_height = 800;
        sc.total_samples = 1000;
        sc.max_depth = 40;
        break;

    case 2: // 最高設定6分程度
        sc.image_width = 800;
        sc.image_height = 800;
        sc.total_samples = 10000;
        sc.max_depth = 40;
        break;
    }

    sc.name = "Finale Scene (Level:" + std::to_string(level) + ")";
    sc.samples_per_pass = 25;

    sc.vfov = 40.0;
    sc.lookfrom = point3(478, 278, -600);
    sc.lookat = point3(278, 278, 0);
    sc.background = color(0.0, 0.0, 0.0);

    // 1. 地面のランダム直方体群
    GpuMaterial ground_mat = make_solid_material({ 0.48f, 0.83f, 0.53f }, 0, 0.0f);
    int boxes_per_side = 20;
    for (int i = 0; i < boxes_per_side; ++i) {
        for (int j = 0; j < boxes_per_side; ++j) {
            auto w = 100.0;
            auto x0 = -1000.0 + i * w;
            auto z0 = -1000.0 + j * w;
            auto x1 = x0 + w;
            auto y1 = random_double(1, 101);
            auto z1 = z0 + w;
            add_box(point3(x0, 0, z0), point3(x1, y1, z1), ground_mat, 0.0, vec3(0, 0, 0), sc.quads);
        }
    }

    // 2. 天井照明
    sc.quads.push_back(make_gpu_quad(point3(123, 554, 147), vec3(300, 0, 0), vec3(0, 0, 265), make_solid_material({ 7.0f, 7.0f, 7.0f }, 3, 0.0f)));

    // 3. 各種球体
    sc.spheres.push_back(make_gpu_sphere(point3(400, 400, 200), 50.0f, vec3(30, 0, 0), make_solid_material({ 0.7f, 0.3f, 0.1f }, 0, 0.0f))); // モーションブラー
    sc.spheres.push_back(make_gpu_sphere(point3(260, 150, 45), 50.0f, vec3(0, 0, 0), make_solid_material({ 1.0f, 1.0f, 1.0f }, 2, 1.5f)));  // ガラス
    sc.spheres.push_back(make_gpu_sphere(point3(0, 150, 145), 50.0f, vec3(0, 0, 0), make_solid_material({ 0.8f, 0.8f, 0.9f }, 1, 1.0f)));  // 金属
    sc.spheres.push_back(make_gpu_sphere(point3(360, 150, 145), 70.0f, vec3(0, 0, 0), make_solid_material({ 1.0f, 1.0f, 1.0f }, 2, 1.5f)));  // 青煙外殻

    GpuMaterial earth_mat = {};
    earth_mat.type = 0;
    earth_mat.text_type = 2;
    sc.spheres.push_back(make_gpu_sphere(point3(400, 200, 400), 100.0f, vec3(0, 0, 0), earth_mat)); // 地球儀

    GpuMaterial pertext_mat = {};
    pertext_mat.type = 0;
    pertext_mat.text_type = 3;
    pertext_mat.scale = 0.2f;
    color(1.0f, 1.0f, 1.0f).copy_to(pertext_mat.albedo);
    sc.spheres.push_back(make_gpu_sphere(point3(220, 280, 300), 80.0f, vec3(0, 0, 0), pertext_mat)); // 大理石

    // 4. 小球体クラスター (1000個)
    GpuMaterial white = make_solid_material({ 0.73f, 0.73f, 0.73f }, 0, 0.0f);
    for (int j = 0; j < 1000; ++j) {
        point3 p(random_double(0, 165), random_double(0, 165), random_double(0, 165));
        point3 p_trans = rotate_vec_y(p, 15.0) + vec3(-100, 270, 395);
        sc.spheres.push_back(make_gpu_sphere(p_trans, 10.0f, vec3(0, 0, 0), white));
    }

    // 5. 参加媒質ボリューム
    GpuVolume blue_smoke = {};
    blue_smoke.shape_type = 0;
    blue_smoke.density = 0.2f;
    color(0.2f, 0.4f, 0.9f).copy_to(blue_smoke.albedo);
    point3(360, 150, 145).copy_to(blue_smoke.center);
    blue_smoke.radius = 70.0f;
    sc.volumes.push_back(blue_smoke);

    GpuVolume world_fog = {};
    world_fog.shape_type = 0;
    world_fog.density = 0.0001f;
    color(1.0f, 1.0f, 1.0f).copy_to(world_fog.albedo);
    point3(0, 0, 0).copy_to(world_fog.center);
    world_fog.radius = 5000.0f;
    sc.volumes.push_back(world_fog);

    return sc;
}

template<typename T>
void create_structured_buffer_and_srv(
    ID3D11Device* device,
    const std::vector<T>& data,
    ComPtr<ID3D11Buffer>& out_buffer,
    ComPtr<ID3D11ShaderResourceView>& out_srv)
{
    // サイズの0バッファ作成エラーを防ぐために最低1要素を確保
    std::vector<T> safe_data = data.empty() ? std::vector<T>(1) : data;

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = static_cast<UINT>(sizeof(T) * safe_data.size());
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = sizeof(T);

    D3D11_SUBRESOURCE_DATA init_data = {};
    init_data.pSysMem = safe_data.data();
    CheckHR(device->CreateBuffer(&desc, &init_data, &out_buffer));

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format = DXGI_FORMAT_UNKNOWN;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srv_desc.Buffer.FirstElement = 0;
    srv_desc.Buffer.NumElements = static_cast<UINT>(safe_data.size());
    CheckHR(device->CreateShaderResourceView(out_buffer.Get(), &srv_desc, &out_srv));
}

void create_image_texture_and_srv(
    ID3D11Device* device,
    const std::string& filename,
    ComPtr<ID3D11Texture2D>& out_tex,
    ComPtr<ID3D11ShaderResourceView>& out_srv)
{
    rtw_image img(filename.empty() ? "" : filename.c_str());
    UINT width = static_cast<UINT>(img.width() > 0 ? img.width() : 1);
    UINT height = static_cast<UINT>(img.height() > 0 ? img.height() : 1);

    std::vector<uint8_t> rgba_data(width * height * 4, 255);
    if (img.width() > 0) {
        for (UINT y = 0; y < height; ++y) {
            for (UINT x = 0; x < width; ++x) {
                auto p = img.pixel_data(x, y);
                size_t idx = (y * width + x) * 4;
                rgba_data[idx + 0] = p[0];
                rgba_data[idx + 1] = p[1];
                rgba_data[idx + 2] = p[2];
                rgba_data[idx + 3] = 255;
            }
        }
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init_data = {};
    init_data.pSysMem = rgba_data.data();
    init_data.SysMemPitch = width * 4;

    CheckHR(device->CreateTexture2D(&desc, &init_data, &out_tex));
    CheckHR(device->CreateShaderResourceView(out_tex.Get(), nullptr, &out_srv));
}

int run_gpu_renderer(const SceneConfig sc) {

    //-----------------------------------------------------------------------------------------------
    // Initialize
    //-----------------------------------------------------------------------------------------------
    std::clog << "Initialize DirectX 11 for [" << sc.name << "]...\n";

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

    //-----------------------------------------------------------------------------------------------
    // Compute Shader のコンパイルと登録
    //-----------------------------------------------------------------------------------------------
    // エラーメッセージを格納するための変数
    ComPtr<ID3DBlob> errorBlob;
    // コンパイル済みのシェーダーバイナリを格納する変数
    ComPtr<ID3DBlob> shaderBlob;

    std::clog << "Compiling Compute Shader..." << std::endl;

    UINT compileFlags = D3DCOMPILE_OPTIMIZATION_LEVEL3; // 最高レベルの最適化
#ifdef _DEBUG
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    hr = D3DCompileFromFile(
        L"shader/RayTracingCS.hlsl",    // シェーダーファイル名（同じ階層に配置）
        nullptr,                        // マクロ定義（なし）
        nullptr,                        // インクルードインターフェース（なし）
        "CSMain",                       // エントリーポイント関数名
        "cs_5_0",                       // シェーダープロファイル（CS 5.0）
        compileFlags,                              // コンパイルフラグ（最適化など）
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

    //-----------------------------------------------------------------------------------------------
    // リソース用バッファ（オブジェクト、BVH、テクスチャ）
    //-----------------------------------------------------------------------------------------------
    // BVH用参照リストの統合
    std::vector<PrimitiveRef> prim_refs;
    for (size_t i = 0; i < sc.spheres.size(); ++i) {
        prim_refs.emplace_back(
            0,   // Sphere
            static_cast<int>(i),
            compute_sphere_aabb(sc.spheres[i]));
    }
    for (size_t i = 0; i < sc.quads.size(); ++i) {
        prim_refs.emplace_back(
            1,  // Quad
            static_cast<int>(i),
            compute_quad_aabb(sc.quads[i]));
    }
    for (size_t i = 0; i < sc.volumes.size(); ++i) {
        prim_refs.emplace_back(
            2,  // Volume
            static_cast<int>(i),
            compute_volume_aabb(sc.volumes[i]));
    }

    std::vector<GpuBvhNode> bvh_nodes;
    build_flat_bvh_generic(prim_refs, 0, static_cast<int>(prim_refs.size()), bvh_nodes);

    // バッファリソースの作成（t0, t1, t3, t4）
    ComPtr<ID3D11Buffer> sphereBuffer, bvhBuffer, quadBuffer, volBuffer;
    ComPtr<ID3D11ShaderResourceView> sphereSRV, bvhSRV, quadSRV, volSRV;
    create_structured_buffer_and_srv(device.Get(), sc.spheres,  sphereBuffer,   sphereSRV);
    create_structured_buffer_and_srv(device.Get(), bvh_nodes,   bvhBuffer,      bvhSRV);
    create_structured_buffer_and_srv(device.Get(), sc.quads,    quadBuffer,     quadSRV);
    create_structured_buffer_and_srv(device.Get(), sc.volumes,  volBuffer,      volSRV);

    // テクスチャとサンプラーの作成（t2, s0）
    ComPtr<ID3D11Texture2D> imageTex;
    ComPtr<ID3D11ShaderResourceView> imageSRV;
    create_image_texture_and_srv(device.Get(), sc.texture_filename, imageTex, imageSRV);

    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    ComPtr<ID3D11SamplerState> linearSampler;
    CheckHR(device->CreateSamplerState(&sampDesc, &linearSampler));

    //-----------------------------------------------------------------------------------------------
    // カメラ定数バッファ
    //-----------------------------------------------------------------------------------------------
    // 初期化
    auto image_width = static_cast<double>(sc.image_width);
    auto image_height = static_cast<double>(sc.image_height);
    auto theta = degrees_to_radians(sc.vfov);
    auto h = std::tan(theta / 2.0);
    auto viewport_height = 2.0 * h * sc.focus_dist;
    auto viewport_width = viewport_height * (image_width / image_height);

    auto w = unit_vector(sc.lookfrom - sc.lookat);
    auto u = unit_vector(cross(sc.vup, w));
    auto v = cross(w, u);

    auto viewport_u = viewport_width * u;
    auto viewport_v = viewport_height * -v;

    auto pixel_delta_u = viewport_u / image_width;
    auto pixel_delta_v = viewport_v / image_height;

    auto viewport_upper_left = sc.lookfrom - (sc.focus_dist * w) - viewport_u / 2.0 - viewport_v / 2.0;
    auto pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

    auto defocus_radius = sc.focus_dist * std::tan(degrees_to_radians(sc.defocus_angle / 2.0));
    auto defocus_disk_u = u * defocus_radius;
    auto defocus_disk_v = v * defocus_radius;

    // バッファ転送用データに格納
    CameraBufferData cbData = {};
    sc.lookfrom.copy_to(cbData.camera_center);
    pixel00_loc.copy_to(cbData.pixel00_loc);
    pixel_delta_u.copy_to(cbData.pixel_delta_u);
    pixel_delta_v.copy_to(cbData.pixel_delta_v);
    defocus_disk_u.copy_to(cbData.defocus_disk_u);
    defocus_disk_v.copy_to(cbData.defocus_disk_v);
    sc.background.copy_to(cbData.background);

    cbData.samples_this_pass    = sc.samples_per_pass;
    cbData.max_depth            = sc.max_depth;
    cbData.defocus_angle        = static_cast<float>(sc.defocus_angle);

    // サンプリング光源をリストアップ
    // 天井照明、ガラス球などを指定可能
    int target_count = 0;
    const static int MAX_LIGHT_TARGETS = 4;

    // 1. 発光Quadを追加
    for (size_t i = 0; i < sc.quads.size() && target_count < MAX_LIGHT_TARGETS; ++i) {
        if (sc.quads[i].mat.type == 3) {
            cbData.light_target_data[target_count][0] = 1; // 1: Quad
            cbData.light_target_data[target_count][1] = static_cast<int>(i);
            target_count++;
        }
    }

    // 2. 発光Sphereを追加
    for (size_t i = 0; i < sc.spheres.size() && target_count < MAX_LIGHT_TARGETS; ++i) {
        if (sc.spheres[i].mat.type == 3) {
            cbData.light_target_data[target_count][0] = 2; // 2: Sphere
            cbData.light_target_data[target_count][1] = static_cast<int>(i);
            target_count++;
        }
    }
    cbData.num_light_targets = target_count;

    // 定数バッファの作成
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(CameraBufferData);    // 16の倍数
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    D3D11_SUBRESOURCE_DATA cbInit = {};
    cbInit.pSysMem = &cbData;
    ComPtr<ID3D11Buffer> cameraBuffer;
    CheckHR(device->CreateBuffer(&cbDesc, &cbInit, &cameraBuffer));

    //-----------------------------------------------------------------------------------------------
    // GPU書き込み用テクスチャ（Default）& UAV(Unordered Access View：順序なしアクセスビュー)の作成
    //-----------------------------------------------------------------------------------------------
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = sc.image_width;
    texDesc.Height = sc.image_height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    ComPtr<ID3D11Texture2D> outputTexture;
    CheckHR(device->CreateTexture2D(&texDesc, nullptr, &outputTexture));

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = texDesc.Format;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    ComPtr<ID3D11UnorderedAccessView> uav;
    CheckHR(device->CreateUnorderedAccessView(outputTexture.Get(), &uavDesc, &uav));

    //-----------------------------------------------------------------------------------------------
    // CPU読み取り用ステージングテクスチャ(Staging)の作成
    //-----------------------------------------------------------------------------------------------
    D3D11_TEXTURE2D_DESC stagingDesc = texDesc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> stagingTexture;
    CheckHR(device->CreateTexture2D(&stagingDesc, nullptr, &stagingTexture));

    //-----------------------------------------------------------------------------------------------
    // GPU同期用イベントクエリの作成
    //-----------------------------------------------------------------------------------------------
    D3D11_QUERY_DESC queryDesc = {};
    queryDesc.Query = D3D11_QUERY_EVENT;
    ComPtr<ID3D11Query> query;
    CheckHR(device->CreateQuery(&queryDesc, &query));

    //-----------------------------------------------------------------------------------------------
    // パイプラインバインド
    //-----------------------------------------------------------------------------------------------
    context->CSSetShader(computeShader.Get(), nullptr, 0);
    ID3D11ShaderResourceView* srvArray[] = { sphereSRV.Get(), bvhSRV.Get(), imageSRV.Get(), quadSRV.Get(), volSRV.Get() };
    context->CSSetShaderResources(0, 5, srvArray);
    ID3D11SamplerState* samplerArray[] = { linearSampler.Get() };
    context->CSSetSamplers(0, 1, samplerArray);
    ID3D11Buffer* cbArray[] = { cameraBuffer.Get() };
    context->CSSetConstantBuffers(0, 1, cbArray);
    ID3D11UnorderedAccessView* uavArray[] = { uav.Get() };
    context->CSSetUnorderedAccessViews(0, 1, uavArray, nullptr);

    // スレッドグループ数を計算（1グループあたり16x16 = 256スレッド）
    UINT dispatchX = (sc.image_width + 15) / 16;
    UINT dispatchY = (sc.image_height + 15) / 16;
    int num_pass = sc.total_samples / sc.samples_per_pass;

    auto start_time = std::chrono::high_resolution_clock::now();
    std::clog << "Rendering [" << sc.name <<  "] (" << sc.total_samples << " samples, " << sc.max_depth << " depth)...\n";

    for (int pass = 0; pass < num_pass; ++pass) {
        // パス番号を定数バッファに更新
        cbData.pass_index = pass;
        context->UpdateSubresource(cameraBuffer.Get(), 0, nullptr, &cbData, 0, 0);

        // 1パス分(50サンプル)を実行
        context->Dispatch(dispatchX, dispatchY, 1);

        // GPUにイベントを発行し、このパスの完了を待機
        context->End(query.Get());
        while (context->GetData(query.Get(), nullptr, 0, 0) == S_FALSE) {
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

    auto gpu_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> gpu_time = gpu_end - start_time;
    std::clog << "\nRendering finished! [GPU Compute Time: " << gpu_time.count() << " s]\n";

    // 使用したリソースのバインドを解除（後続の安全のため）
    ID3D11UnorderedAccessView* nullUAV[] = { nullptr };
    context->CSSetUnorderedAccessViews(0, 1, nullUAV, nullptr);

    //-----------------------------------------------------------------------------------------------
    // GPUからCPUへの転送とデータ読み取り（Readback）
    //-----------------------------------------------------------------------------------------------
    // GPU内部でDefaultからStagingへテクスチャを丸ごとコピー
    context->CopyResource(stagingTexture.Get(), outputTexture.Get());
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    CheckHR(context->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped));

    //-----------------------------------------------------------------------------------------------
    // PPMファイルへの書き出し
    //-----------------------------------------------------------------------------------------------
    std::clog << "Saving to image.ppm..." << std::endl;
    std::cout << "P3\n" << image_width << ' ' << image_height << "\n255\n";

    // メモリの各行の先頭アドレスをRowPitch（パディング考慮）を使って走査
    const uint8_t* rowPtr = static_cast<const uint8_t*>(mapped.pData);

    for (UINT y = 0; y < image_height; ++y) {
        const float* pixel = reinterpret_cast<const float*>(rowPtr);
        for (UINT x = 0; x < image_width; x++) {
            // 合計輝度を総サンプル数で割って平均化
            color pixel_color(
                pixel[0] / static_cast<float>(sc.total_samples),
                pixel[1] / static_cast<float>(sc.total_samples),
                pixel[2] / static_cast<float>(sc.total_samples)
            );
            write_color(std::cout, pixel_color);
            pixel += 4; // 次のピクセル（float4分進める）
        }
        rowPtr += mapped.RowPitch;
    }

    context->Unmap(stagingTexture.Get(), 0);

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    std::clog << "\rDone. (" << elapsed.count() << " seconds)                       \n";
    return 0;
 }

int main() {
    try {
        // レンダリングしたいシーンを選択
        // 1: Bouncing Spheres
        // 2: Checkered Spheres
        // 3: Earth
        // 4: Perlin Spheres
        // 5: Quads
        // 6: Sample Light
        // 7: Cornell Box
        // 8: Cornell Smoke
        // 9: Final Scene (Preview 250 samples)
        // 10: Final Scene (High Detail 10,000 samples)
        int scene_id = 10;

        SceneConfig sc;
        switch (scene_id) {
        case 1:  sc = scene_bouncing_spheres(); break;
        case 2:  sc = scene_checkered_spheres(); break;
        case 3:  sc = scene_earth(); break;
        case 4:  sc = scene_perlin_spheres(); break;
        case 5:  sc = scene_quads(); break;
        case 6:  sc = scene_sample_light(); break;
        case 7:  sc = scene_cornell_box(); break;
        case 8:  sc = scene_cornell_smoke(); break;
        case 9:  sc = scene_final_scene(0); break;
        case 10: sc = scene_final_scene(1);  break;
        default: sc = scene_final_scene(2); break;
        }

        return run_gpu_renderer(sc);
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}