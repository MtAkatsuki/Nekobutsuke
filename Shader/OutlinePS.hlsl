#include "common.hlsl"

float4 main(PS_IN In) : SV_Target
{
    // 本体と一致するディザリングクリップ：アウトラインの抜けを本体と一致させる
    clip(DitherThreshold(In.Position.xy) - Material.Dummy.x);
    return float4(OutlineColor.rgb, 1.0);
}