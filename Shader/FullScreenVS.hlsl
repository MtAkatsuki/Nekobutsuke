#include "common.hlsl"   // VS_IN（POSITION/NORMAL/COLOR/TEXCOORD）

struct VS_OUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VS_OUT main(VS_IN In)
{
    VS_OUT o;
    o.pos = float4(In.Position.xyz, 1.0f); // 頂点は既にNDC（クリップ空間）
    o.uv = In.TexCoord;
    return o;
}