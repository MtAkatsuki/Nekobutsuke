// FullScreenPS.hlsl
Texture2D g_SceneTex : register(t0);
SamplerState g_Samp : register(s0);
float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_Target
{
    return g_SceneTex.Sample(g_Samp, uv); // 線形=2x2平均でダウンサンプル
}