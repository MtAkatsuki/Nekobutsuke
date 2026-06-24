#include "common.hlsl"

PS_IN3 main(in VS_IN In)
{
    PS_IN3 Out;

    matrix wvp = mul(World, View);
    wvp = mul(wvp, Projection);
    Out.Position = mul(In.Position, wvp);
    
    Out.WPos = mul(In.Position, World).xyz; //ワールド座標（rim/ライティング用）
    //scaleがある場合、normalはWorldの逆転置行列で変換する必要があるが、等比縮小の場合はそのままworld3x3で変換してもよい
    Out.Normal = normalize(mul(In.Normal.xyz, (float3x3) World));
    Out.TexCoord = In.TexCoord;
    Out.Diffuse = Material.Diffuse;
    return Out;
}