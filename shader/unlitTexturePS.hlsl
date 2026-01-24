#include "common.hlsl"

Texture2D		g_Texture : register(t0);
SamplerState	g_SamplerState : register(s0);

float4 main(in PS_IN In) : SV_Target
{
    float4 outDiffuse;
	
	if (Material.TextureEnable)
	{
		outDiffuse = g_Texture.Sample(g_SamplerState, In.TexCoord);
		// [新規追加] アルファクリップ (Alpha Clip)
		// ピクセルのアルファ値が0.1未満の場合、そのピクセルを破棄する
		// 深度バッファへの書き込みを防ぎ、黒い縁などが表示されないようにする
        clip(outDiffuse.a - 0.1f);
		outDiffuse *= In.Diffuse;
	}
	else
	{
		outDiffuse = In.Diffuse;
	}

    return outDiffuse;
}
