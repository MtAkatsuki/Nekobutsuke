#include "common.hlsl"
PS_IN main(VS_IN In)
{
    PS_IN Out;
    Out.Position = mul(In.Position, mul(mul(World, View), Projection));
    Out.TexCoord = In.TexCoord;
    Out.Diffuse = float4(0, 0, 0, 1);
    return Out;
}