#include "common.hlsl"
float4 main(PS_IN In) : SV_Target
{
    float d = length(In.TexCoord - 0.5f) * 2.0f; // 中心=0、外周=1
    float a = 1.0f - smoothstep(0.6f, 1.0f, d); // 0.6ー1.0範囲でソフトエッジ化
    a *= 0.5f; // Blob強度（仮設定、後でb7パラメータ化）
    return float4(0.0f, 0.0f, 0.0f, a); // 黒色 + Alpha
}