// RayTracingCS.hlsl

static const float PI     = 3.14159265358979323846f;
static const float TWO_PI = 6.28318530717958647692f;

// 被写界深度パラメータを追加したカメラ定数バッファ（96bytes:16の倍数）
cbuffer CameraBuffer : register(b0)
{
    float3 CameraCenter;
    int NumSpheres;
    float3 Pixel00Loc;
    int SamplesPerPixel;
    float3 PixelDeltaU;
    int MaxDepth;
    float3 PixelDeltaV;
    float DefocusAngle;
    float3 DefocusDiskU;
    float Pad0;
    float3 DefocusDiskV;
    float Pad1;
}

// マテリアル(48bytes)
struct Material
{
    float3 albedo;  // 基本色 / チェッカー1
    int type;       // 0: Lambertian, 1: Metal, 2: Dielectric
    float3 albedo2; // チェッカー2
    int text_type;  // 0:Solid 1:Checker 2:Image
    float param;    // fuzz / ref_idx
    float scale;    // checker scale
    float2 pad;
};

// 球体
struct Sphere
{
    float3 center;
    float radius;
    float3 center_vec;
    float pad;
    Material mat;
};

// レイの定義
struct Ray
{
    float3 origin;
    float3 direction;
    float time;         // シャッター時間[0,1]
};

// 当たり判定の情報
struct HitRecord
{
    float t;
    float3 p;
    float3 normal;
    float2 uv;
    bool front_face;
    Material mat;
};

// AABB（境界ボックス:32bytes）
struct Aabb
{
    float3 min_point;
    float pad0;
    float3 max_point;
    float pad1;
};

// フラットBVHノード（48bytes）
struct BvhNode
{
    Aabb bbox;
    int left_child_index;
    int right_child_index;
    int sphere_index;
    int pad;
};

// 出力先テクスチャ（UAV:u0 スロットにバインド）
RWTexture2D<float4> OutputTexture : register(u0);
// C++から転送された球体バッファ(t0)とBVHノードバッファ(t1)
StructuredBuffer<Sphere> Spheres : register(t0);
StructuredBuffer<BvhNode> BvhNodes : register(t1);

// 画像テクスチャ（t2）とバイリニアサンプラー（s0）
Texture2D<float4> ImageTexture : register(t2);
SamplerState LinearSampler : register(s0);

//--------------------------------------------
// 高速疑似乱数（PCG Hash）
//--------------------------------------------
uint pcg_hash(inout uint state)
{
    state = state * 747796405u + 2891336453u;
    uint world = (state >> ((state >> 28u) + 4u) ^ state) * 277803737u;
    return (world >> 22u) ^ world;
}

float rand_float(inout uint state)
{
    return (pcg_hash(state) & 0x00FFFFFFu) / 16777216.0f;
}

float3 random_uint_vector(inout uint state)
{
    // 球面座標系による一様サンプリング
    float z = rand_float(state) * 2.0f - 1.0f;
    float a = rand_float(state) * TWO_PI;
    float r = sqrt(max(0.0f, 1.0f - z * z));
    return float3(r * cos(a), r * sin(a), z);
}

// レンズ絞り（単位円盤）内のサンプリング
float3 random_in_unit_disk(inout uint state)
{
    // 単位円盤内の一様分布：半径rは平方根、角度 theraは0～2π
    float r = sqrt(rand_float(state));
    float theta = rand_float(state) * TWO_PI;
    
    return float3(r * cos(theta), r * sin(theta), 0.0f);
}

// 単位法線ベクトルから球面UV座標
float2 get_sphere_uv(float3 outward_normal)
{
    float theta = acos(-outward_normal.y);
    float phi = atan2(-outward_normal.z, outward_normal.x) + PI;
    return float2(phi / (TWO_PI), 1.0f - theta / PI);
}

//--------------------------------------------
// パーリンノイズ（3D勾配ノイズ）
//--------------------------------------------

// 高品質な3D整数ハッシュ(PCG3D)
uint3 pcg3d(uint3 v)
{
    uint T = 1664525u;
    v = v * T + 1013904223u;
    v.x += v.y * T;
    v.y += v.z * T;
    v.z += v.x * T;
    v ^= v >> 16u;
    v.x += v.y * T;
    v.y += v.z * T;
    v.z += v.x * T;
    v ^= v >> 16u;
    return v;
}

// 格子点(int3)から決定論的な単位勾配ベクトルを生成するハッシュ
float3 hash_gradient(int3 p)
{
    uint3 u = (uint3) p;
    uint3 h = pcg3d(u);

    // [-1,1]のベクトルにして正規化
    float3 g = (float3(h) / 4294967295.0f) * 2.0f - 1.0f;
    return normalize(g);
}

// 単一オクターブの3Dパーリンノイズ[-1,1]
float perlin_noise(float3 p)
{
    int3 i = int3(floor(p));
    float3 f = frac(p);

    // 5次エルミート補間（滑らかな微笑変化:6f^5 - 15f^4 + 10f^3）
    float3 u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);

    // 8つの格子点の寄与を計s何して三直線補間
    float n000 = dot(hash_gradient(i + int3(0, 0, 0)), f - float3(0, 0, 0));
    float n100 = dot(hash_gradient(i + int3(1, 0, 0)), f - float3(1, 0, 0));
    float n010 = dot(hash_gradient(i + int3(0, 1, 0)), f - float3(0, 1, 0));
    float n110 = dot(hash_gradient(i + int3(1, 1, 0)), f - float3(1, 1, 0));
    float n001 = dot(hash_gradient(i + int3(0, 0, 1)), f - float3(0, 0, 1));
    float n101 = dot(hash_gradient(i + int3(1, 0, 1)), f - float3(1, 0, 1));
    float n011 = dot(hash_gradient(i + int3(0, 1, 1)), f - float3(0, 1, 1));
    float n111 = dot(hash_gradient(i + int3(1, 1, 1)), f - float3(1, 1, 1));
    
    float nx00 = lerp(n000, n100, u.x);
    float nx10 = lerp(n010, n110, u.x);
    float nx01 = lerp(n001, n101, u.x);
    float nx11 = lerp(n011, n111, u.x);

    float nxy0 = lerp(nx00, nx10, u.y);
    float nxy1 = lerp(nx01, nx11, u.y);

    return lerp(nxy0, nxy1, u.z);
}

// タービュランス（多重低周波の絶対値加算）
float perlin_turb(float3 p, int depth)
{
    float accum = 0.0f;
    float3 temp_p = p;
    float weight = 1.0f;
    
    [unroll]
    for (int i = 0; i < 7; i++)
    {
        if (i >= depth)
            break;
        
        accum += weight * abs(perlin_noise(temp_p));
        weight *= 0.5f;
        temp_p *= 2.0f;
    }

    return accum;
}

// テクスチャ種別に応じた表面色を計算
float3 get_material_albedo(Material mat, float3 p, float2 uv)
{
    if (mat.text_type == 1)              // 3D Checker
    {
        float inv_scale = 1.0f / mat.scale;
        int3 ip = int3(floor(inv_scale * p));
        bool is_even = ((ip.x + ip.y + ip.z) % 2) == 0;
        return is_even ? mat.albedo : mat.albedo2;
    }
    else if (mat.text_type == 2)    // Image Texture
    {
        // Compute ShaderではSampleLevelを使用
        return ImageTexture.SampleLevel(LinearSampler, uv, 0).rgb;
    }
    else if (mat.text_type == 3)    // Perlin Marble Noise
    {
        // 大理石模様
        float s = 0.5f * (1.0f + sin(mat.scale * p.z + 10.0f * perlin_turb(p, 7)));
        return s * mat.albedo;
    }
    
    // 単色（Solid）
    return mat.albedo;
}

// 球との交差判定関数
// 衝突した場合は最小のt>0を返し、衝突しない場合は府の値を返す
bool hit_sphere(Sphere s, Ray r, float ray_tmin, float ray_tmax, inout HitRecord rec)
{
    // コンパイラの未初期化チェックを通すため冒頭で0クリアする
    rec = (HitRecord) 0;

    // 時間 r.timeにおける球の中心座標
    float3 current_center = s.center + r.time * s.center_vec;
    
    float3 oc = current_center - r.origin;
    float a = dot(r.direction, r.direction);
    float h = dot(r.direction, oc);
    float c = dot(oc, oc) - s.radius * s.radius;
    float discriminant = h * h - a * c;

    if (discriminant < 0.0f)
    {
        return false;
    }
    
    float sqrtd = sqrt(discriminant);
    float root = (h - sqrtd) / a;
    
    if (root <= ray_tmin || ray_tmax <= root)
    {
        root = (h + sqrtd) / a;
        if (root <= ray_tmin || ray_tmax <= root)
            return false;
    }
    
    rec.t = root;
    rec.p = r.origin + rec.t * r.direction;
    float3 outward_normal = (rec.p - current_center) / s.radius;
    rec.front_face = dot(r.direction, outward_normal) < 0.0f;
    rec.normal = rec.front_face ? outward_normal : -outward_normal;
    rec.uv = get_sphere_uv(outward_normal);     // UV座標を格納
    rec.mat = s.mat;
    return true;
}

// 光線とAABBの交差点（スラブ法）
bool hit_aabb(Aabb box, Ray r, float ray_tmin, float ray_tmax)
{
    [unroll]
    for (int a = 0; a < 3; ++a)
    {
        float invD = 1.0f / r.direction[a];
        float t0 = (box.min_point[a] - r.origin[a]) * invD;
        float t1 = (box.max_point[a] - r.origin[a]) * invD;

        if (invD < 0.0f)
        {
            float tmp = t0;
            t0 = t1;
            t1 = tmp;
        }

        ray_tmin = max(t0, ray_tmin);
        ray_tmax = min(t1, ray_tmax);
        
        if (ray_tmax < ray_tmin)
            return false;
    }

    return true;
}

// BVHを平坦化した上で実行する
bool hit_world(Ray r, float ray_tmin, float ray_tmax, inout HitRecord rec)
{
    HitRecord temp_rec = (HitRecord)0;  // ゼロ初期化
    bool hit_anything = false;
    float closest_so_far = ray_tmax;

    // GPUローカルメモリにスタックを確保（深さ64あれば数万個のオブジェクトに対応可能）
    int stack[64];
    int stack_ptr = 0;
    
    // ルートノード
    stack[stack_ptr++] = 0;
    
    while (stack_ptr > 0)
    {
        int node_idx = stack[--stack_ptr];
        BvhNode node = BvhNodes[node_idx];

        // ノードのAABBに当たらなければこのノードおよびその配下はスキップ
        if (!hit_aabb(node.bbox, r, ray_tmin, closest_so_far))
            continue;

        if (node.sphere_index >= 0)
        {
            // 葉ノード(Leaf):含まれる球体との交差判定を行う
            if (hit_sphere(Spheres[node.sphere_index], r, ray_tmin, closest_so_far, temp_rec))
            {
                hit_anything = true;
                closest_so_far = temp_rec.t;
                rec = temp_rec;
            }
        }
        else
        {
            // 内部ノードの場合、node.sphere_index = -1が与えられる
            // 内部ノード(Branch):子ノードをスタックに積む
            // （手前の子から探索できるよう、right->leftの順にPush）
            if (stack_ptr < 62)
            {
                if (node.right_child_index >= 0)
                    stack[stack_ptr++] = node.right_child_index;
                if (node.left_child_index >= 0)
                    stack[stack_ptr++] = node.left_child_index;
            }
        }
    }

    return hit_anything;
}

//--------------------------------------------
// マテリアルの散乱計算(Lambertian / Metal / Dielectric)
//--------------------------------------------
float reflectance(float cosine, float ref_idx)
{
    float r0 = (1.0f - ref_idx) / (1.0f + ref_idx);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * pow(1.0f - cosine, 5.0f);
}

bool scatter(Ray r_in, HitRecord rec, out float3 attenuation, out Ray scattered, inout uint rng)
{
    if (rec.mat.type == 0) // Lambertian
    {
        float3 scatter_direction = rec.normal + random_uint_vector(rng);
        if (dot(scatter_direction, scatter_direction) < 1e-8f)
            scatter_direction = rec.normal;

        scattered.origin = rec.p;
        scattered.direction = normalize(scatter_direction);
        scattered.time = r_in.time;
        attenuation = get_material_albedo(rec.mat, rec.p, rec.uv);
        return true;
    }
    else if (rec.mat.type == 1) // Metal
    {
        float3 reflected = reflect(r_in.direction, rec.normal);
        float3 scatter_direction = reflected + rec.mat.param * random_uint_vector(rng);
        scattered.origin = rec.p;
        scattered.direction = normalize(scatter_direction);
        scattered.time = r_in.time;
        attenuation = get_material_albedo(rec.mat, rec.p, rec.uv);
        return dot(scattered.direction, rec.normal) > 0.0f;
    }
    else // Dielectric
    {
        attenuation = float3(1.0f, 1.0f, 1.0f);
        float ri = rec.front_face ? (1.0f / rec.mat.param) : rec.mat.param;
        
        float cos_theta = min(dot(-r_in.direction, rec.normal), 1.0f);
        float sin_theta = sqrt(max(0.0f, 1.0f - cos_theta * cos_theta));

        bool cannot_refract = (ri * sin_theta) > 1.0f;
        float3 direction;
        
        if (cannot_refract || reflectance(cos_theta, ri) > rand_float(rng))
            direction = reflect(r_in.direction, rec.normal);
        else
            direction = refract(r_in.direction, rec.normal, ri);
        
        scattered.origin = rec.p;
        scattered.direction = normalize(direction);
        scattered.time = r_in.time;
        return true;
    }
}

// エントリポイント関数。1スレッドグループあたり（8,8,1）スレッドで実行
[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    // 出力テクスチャの解像度を取得
    uint width, height;
    OutputTexture.GetDimensions(width, height);
    
    // テクスチャの範囲外スレッドは除外
    if (id.x >= width || id.y >= height)
        return;

    // スレッド（ピクセル）ごとに固有の乱数シード
    uint rng = id.x * 1973U + id.y * 9277u + (uint) SamplesPerPixel * 26699u;

    float3 total_color = float3(0.0f, 0.0f, 0.0f);
    
    // マルチサンプリング（アンチエイリアス）ループ
    for (int s = 0; s < SamplesPerPixel; ++s)
    {
        // ピクセル平面上のサンプリング店
        float rx = rand_float(rng) - 0.5f;
        float ry = rand_float(rng) - 0.5f;
        float3 pixel_sample = Pixel00Loc + ((((float) id.x + rx) * PixelDeltaU) + (((float) id.y + ry) * PixelDeltaV));

        // 被写界深度（デフォーカスブラー）を適用したレイ原点
        float3 ray_origin = CameraCenter;
        if (DefocusAngle > 0.0f)
        {
            float3 disk_p = random_in_unit_disk(rng);
            ray_origin = CameraCenter + (disk_p.x * DefocusDiskU) + (disk_p.y * DefocusDiskV);
        }

        Ray cur_ray;
        cur_ray.origin = ray_origin;
        cur_ray.direction = normalize(pixel_sample - ray_origin);
        cur_ray.time = rand_float(rng); // シャッター時間[0,1]をサンプリング
        
        float3 throughput = float3(1.0f, 1.0f, 1.0f);
        float3 sample_color = float3(0.0f, 0.0f, 0.0f);

        // 反復パストレース（再帰の代わりにループを使用
        for (int bounce = 0; bounce < MaxDepth; ++bounce)
        {
            HitRecord rec = (HitRecord)0;   // ゼロ初期化
            if (hit_world(cur_ray, 0.001f, 1e9f, rec))
            {
                Ray scattered;
                float3 attenuation;
                if (scatter(cur_ray, rec, attenuation, scattered, rng))
                {
                    throughput *= attenuation;
                    cur_ray = scattered;
                }
                else
                {
                    break;
                }
            }
            else
            {
                // 空に到達
                float a = 0.5f * (cur_ray.direction.y + 1.0f);
                float3 sky_color = lerp(float3(1.0f, 1.0f, 1.0f), float3(0.5f, 0.7f, 1.0f), a);
                sample_color = throughput * sky_color;
                break;
            }
        }
        total_color += sample_color;
    }
    
    // ガンマ補正(gamma 2.0)
    float3 final_color = sqrt(total_color / (float)SamplesPerPixel);
    OutputTexture[id.xy] = float4(final_color, 1.0f);
}
