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
    float3 N = normalize(In.Normal);
    float3 L = -normalize(Light.Direction.xyz); 
    float ndl = dot(N, L) * 0.5f + 0.5f; // [-1,1] → [0,1],暗い側を少し明るくするために 0.5f を足す

    // --- 量子化処理：2つのsmoothstepによるソフト境界で3段階 {0,0.5,1} に分割 ---
    float soft = ToonParams.z;
    float band = smoothstep(ToonParams.x - soft, ToonParams.x + soft, ndl) * 0.5f
               + smoothstep(ToonParams.y - soft, ToonParams.y + soft, ndl) * 0.5f;

    // --- 明暗はrampで輝度を制御し、ライトの1.5倍HDR値は掛けない；さらにフレームワークの環境光を加算 ---
    float3 lit = albedo.rgb * lerp(ShadowColor.rgb, float3(1, 1, 1), band);
    lit += albedo.rgb * Light.Ambient.rgb;

    return float4(lit, albedo.a);
}