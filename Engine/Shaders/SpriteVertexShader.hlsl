cbuffer cbTransform : register(b0)
{
    matrix Transform;
}

cbuffer cbView : register(b1)
{
    matrix View;
}

cbuffer cbProjection : register(b2)
{
    matrix Projection;
}

struct VS_INPUT
{
    float3 Pos : POSITION;
    float2 UV : TEXCOORD;
};

struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    float4 position = mul(float4(input.Pos, 1.0f), Transform);
    position = mul(position, View);
    output.Pos = mul(position, Projection);
    output.UV = input.UV;
    return output;
}
