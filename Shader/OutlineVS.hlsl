#include "common.hlsl"

PS_IN main(VS_IN In)
{
    PS_IN Out;
    matrix WV = mul(World, View);
    float4 Pview = mul(In.Position, WV); // クリップ空間へ

    // 法線をビュー空間に変換（平行移動なし→3x3）
    float3 Nview = normalize(mul(In.Normal.xyz, (float3x3) WV));

    // 3D法線方向に沿って外側へ拡張。
    // 視点深度 Pview.z（LH：前方が正）を乗算することで、射影後のw除算後もスクリーン幅を一定に保持
    Pview.xyz += Nview * OutlineColor.a * Pview.z;

    Out.Position = mul(Pview, Projection);
    Out.Diffuse = float4(OutlineColor.rgb, 1.0f);
    Out.TexCoord = In.TexCoord;
    return Out;
}