#include "common.hlsl"
Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

float4 main(in PS_IN3 In) : SV_Target
{
    float4 albedo;
    if (Material.TextureEnable)
    {
        albedo = g_Texture.Sample(g_SamplerState, In.TexCoord);
        clip(albedo.a - 0.1f);
        albedo *= In.Diffuse;
    }
    else
    {
        albedo = In.Diffuse;
    }

    // --- N*L（Half-Lambert）---
    // flat shading：世界座標の画面微分から面法線を再構成 → 低ポリの面を立てる
    float3 N = normalize(cross(ddx(In.WPos), ddy(In.WPos)));
    float3 L = -normalize(Light.Direction.xyz); 
    float ndl = dot(N, L) * 0.5f + 0.5f; // [-1,1] → [0,1],暗い側を少し明るくするために 0.5f を足す

    // --- 量子化処理：2つのsmoothstepによるソフト境界で3段階 {0,0.5,1} に分割 ---
    float soft = ToonParams.z;
    float band = smoothstep(ToonParams.x - soft, ToonParams.x + soft, ndl) * 0.5f
               + smoothstep(ToonParams.y - soft, ToonParams.y + soft, ndl) * 0.5f;

    // 半球環境光：法線のy成分で天/地を補間（上向き=Sky, 下向き=Ground）
    float hemi_t = N.y * 0.5f + 0.5f; // [-1,1]→[0,1]
    float3 ambient = lerp(ShadowColor.rgb, SkyColor.rgb, hemi_t); // 下=Ground, 上=Sky
    
    // --- 明暗はrampで輝度を制御し、ライトの1.5倍HDR値は掛けない；さらにフレームワークの環境光を加算 ---
    float3 lit = albedo.rgb * lerp(ambient, float3(0.82, 0.82, 0.82), band) * Light.Diffuse.rgb;


    return float4(lit, albedo.a);
}