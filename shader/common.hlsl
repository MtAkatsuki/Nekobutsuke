cbuffer WorldBuffer : register(b0)
{
	matrix World;
}
cbuffer ViewBuffer : register(b1)
{
	matrix View;
}
cbuffer ProjectionBuffer : register(b2)
{
	matrix Projection;
}

struct MATERIAL
{
	float4 Ambient;
	float4 Diffuse;
	float4 Specular;
	float4 Emission;
	float Shininess;
	bool TextureEnable;
	float2 Dummy;
};

cbuffer MaterialBuffer : register(b3)
{
	MATERIAL Material;
}

struct LIGHT
{
	bool Enable;					// 使用するか否か
	bool3 Dummy;					// PADDING
	float4 Direction;				// 方向
	float4 Diffuse;					// 拡散反射用の光の強さ
	float4 Ambient;					// 環境光用の光の強さ
};

cbuffer LightBuffer : register(b4)
{
    LIGHT Light;
};

#define MAX_BONE 400
cbuffer BoneMatrixBuffer : register(b5)
{
    matrix BoneMatrix[MAX_BONE];
}

cbuffer LINEWIDTH : register(b6)
{
    float LineWidth;
    float3 PADDING;
};


struct VS_IN
{
	float4 Position		: POSITION0;
	float4 Normal		: NORMAL0;
	float4 Diffuse		: COLOR0;
	float2 TexCoord		: TEXCOORD0;
};

struct VSONESKIN_IN
{
    float4 Position		: POSITION0;
    float4 Normal		: NORMAL0;
    float4 Diffuse		: COLOR0;
    float2 TexCoord		: TEXCOORD0;
    int4   BoneIndex	: BONEINDEX;
    float4 BoneWeight	: BONEWEIGHT;
};

struct PS_IN
{
	float4 Position		: SV_POSITION;
	float4 Diffuse		: COLOR0;
	float2 TexCoord		: TEXCOORD0;
};

struct PS_IN2
{
    float4 Position : SV_POSITION;
    float4 Diffuse  : COLOR0;
    float4 Specular : COLOR1;
    float2 TexCoord : TEXCOORD0;
};

struct PS_IN3
{
    float4 Position : SV_POSITION;
    float4 Diffuse  : COLOR0;
    float2 TexCoord : TEXCOORD0;
    float3 Normal   : NORMAL;
    float3 WPos     : WORLDPOS;
};

// 3x3 逆行列を計算する関数
/*
float3x3 Inverse3x3(float3x3 m)
{
    float a = m[0].x, b = m[0].y, c = m[0].z;
    float d = m[1].x, e = m[1].y, f = m[1].z;
    float g = m[2].x, h = m[2].y, i = m[2].z;

    float A = e * i - f * h;
    float B = f * g - d * i;
    float C = d * h - e * g;

    float det = a * A + b * B + c * C;
    if (abs(det) < 1e-6)
    {
        float3x3 zeroMatrix = float3x3(0.0f, 0.0f, 0.0f,
                              0.0f, 0.0f, 0.0f,
                              0.0f, 0.0f, 0.0f);
        return zeroMatrix; // 非正則ならゼロ行列       
    }

    float invDet = 1.0 / det;

    return float3x3(
         A, c * h - b * i, b * f - c * e,
         B, a * i - c * g, c * d - a * f,
         C, b * g - a * h, a * e - b * d
    ) * invDet;
}
*/


float3x3 Inverse3x3(float3x3 m)
{
    float a = m[0].x, b = m[0].y, c = m[0].z;
    float d = m[1].x, e = m[1].y, f = m[1].z;
    float g = m[2].x, h = m[2].y, i = m[2].z;

    float A = e * i - f * h;
    float B = c * h - b * i;
    float C = b * f - c * e;
    float D = f * g - d * i;
    float E = a * i - c * g;
    float F = c * d - a * f;
    float G = d * h - e * g;
    float H = b * g - a * h;
    float I = a * e - b * d;

    float det = a * A + b * D + c * G;
    if (abs(det) < 1e-6)
    {
        return float3x3(0.0, 0.0, 0.0,
                        0.0, 0.0, 0.0,
                        0.0, 0.0, 0.0);
    }

    float invDet = 1.0 / det;

    return float3x3(
        A, B, C,
        D, E, F,
        G, H, I
    ) * invDet;
}


// ビュー変換行列からカメラ位置を取得する
float3 GetCameraPosition(matrix viewmtx)
{
    float3 pos = float3(0, 0, 0);

	// 4行目の成分を取得
    float3 m4;
    m4 = float3(viewmtx._41, viewmtx._42, viewmtx._43);

	// X座標
    float3 m1;
    m1 = float3(viewmtx._11, viewmtx._12, viewmtx._13);
    pos.x = dot(-m4, m1);
	
	// Y座標
    float3 m2;
    m2 = float3(viewmtx._21, viewmtx._22, viewmtx._23);
    pos.y = dot(-m4, m2);
	
	// Z座標
    float3 m3;
    m3 = float3(viewmtx._31, viewmtx._32, viewmtx._33);
    pos.z = dot(-m4, m3);

    return pos;
}