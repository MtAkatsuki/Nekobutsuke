cbuffer PostFXBuffer : register(b6)
{
    float Vignette;
    float3 _pad;
}; 

float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_Target
{
    float2 d = uv - 0.5f;
    float r2 = dot(d, d);
    
    float inner = 0.2f; 
    float a = saturate(Vignette * max(r2 - inner, 0.0f));
    return float4(0.0f, 0.0f, 0.0f, a);
}