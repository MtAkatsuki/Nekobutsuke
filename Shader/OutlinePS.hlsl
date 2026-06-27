#include "common.hlsl"

float4 main(PS_IN In) : SV_Target
{
    return float4(OutlineColor.rgb, 1.0);
}