cbuffer cbTransform : register(b0)
{
    matrix Transform;
};

cbuffer cbView : register(b1)
{
    matrix View;
};

cbuffer cbProjection : register(b2)
{
    matrix Projection;
};

cbuffer cbDissolve : register(b0)
{
    float DissolveAmount;
    float HalfWidth;
    float HalfDepth;
    float UniformDissolve;
    float DissolvePixelWidth;
    float DissolvePixelDepth;
    float2 DissolvePixelPadding;
    float Opacity;
    float3 DitherPadding;
};

struct VS_INPUT
{
    float3 Pos : POSITION;
    float4 Col : COLOR;
};

struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float4 Col : COLOR;
    float2 LocalPosition : TEXCOORD0;
};

VS_OUTPUT VSMain(VS_INPUT input)
{
    VS_OUTPUT output;
    float4 position = mul(float4(input.Pos, 1.0), Transform);
    position = mul(position, View);
    output.Pos = mul(position, Projection);
    output.Col = input.Col;
    output.LocalPosition = input.Pos.xz;
    return output;
}

float Bayer4x4(int2 pixel)
{
    static const float thresholds[16] = {
         0.5,  8.5,  2.5, 10.5,
        12.5,  4.5, 14.5,  6.5,
         3.5, 11.5,  1.5,  9.5,
        15.5,  7.5, 13.5,  5.5
    };
    return thresholds[(pixel.y % 4) * 4 + (pixel.x % 4)] / 16.0;
}

float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
    const float localX = input.LocalPosition.x / (HalfWidth * 2.0) + 0.5;
    const float localY = input.LocalPosition.y / (HalfDepth * 2.0) + 0.5;
    const float diagonalPosition = (localX + localY) * 0.5;
    const float directionalDissolve = saturate(
        (saturate(DissolveAmount) * 1.35 - diagonalPosition) / 0.35);
    const float localDissolve =
        UniformDissolve > 0.5 ? saturate(DissolveAmount) : directionalDissolve;
    const int2 shadowPixel = int2(
        floor((input.LocalPosition.x + HalfWidth) / DissolvePixelWidth),
        floor((input.LocalPosition.y + HalfDepth) / DissolvePixelDepth));
    clip(Bayer4x4(shadowPixel) - localDissolve);
    return float4(input.Col.rgb, input.Col.a * Opacity);
}
