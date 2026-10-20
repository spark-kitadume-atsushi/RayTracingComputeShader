// RayTracingCS.hlsl

// 出力先テクスチャ（UAV:u0 スロットにバインド）
RWTexture2D<float4> OutputTexture : register(u0);

// カメラ定数バッファ（b0） - 16バイトアライメントに注意
cbuffer CameraBuffer : register(b0)
{
    float3 CameraCenter;
    float  Pd0;
    float3 Pixel00Loc;
    float  Pad1;
    float3 PixelDeltaU;
    float  Pad2;
    float3 PixelDeltaV;
    float  Pad3;
}

// レイの定義
struct Ray
{
    float3 origin;
    float3 direction;
};

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
    
    // ピクセル中心のワールド座標を計算
    //float3 pixel_sample = Pixel00Loc + ((float)id.x * PixelDeltaU) + ((float) id.y * PixelDeltaV);
    
    // カメラ原点からピクセルへ向かうレイを生成
    //Ray r;
    //r.origin = CameraCenter;
    //r.direction = normalize(pixel_sample = CameraCenter);
    
    // テスト用のUVグラデーション（左上が黒、右下が赤）
    float r = (float) id.x / (float) (width - 1);
    float g = (float) id.y / (float) (height - 1);
    float b = 0.25f;
    
    OutputTexture[id.xy] = float4(r, g, b, 1.0f);
}
