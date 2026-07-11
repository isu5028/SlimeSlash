cbuffer cbBackground : register(b0)
{
    float Time;
    float2 Resolution;
    float BackgroundPadding;
    float4 ColorA;
    float4 ColorB;
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
};

VS_OUTPUT VSMain(uint vertexId : SV_VertexID)
{
    VS_OUTPUT output;
    const float2 position = float2(
        vertexId == 2 ? 3.0 : -1.0,
        vertexId == 1 ? 3.0 : -1.0);
    output.Position = float4(position, 0.9999, 1.0);
    output.UV = float2(position.x * 0.5 + 0.5, 0.5 - position.y * 0.5);
    return output;
}

float Bayer2x2(int2 pixel)
{
    static const float threshold[4] = {
        0.5, 2.5,
        3.5, 1.5
    };
    const int2 p = pixel & 1;
    return threshold[p.y * 2 + p.x] / 4.0;
}

float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
    float2 uv = input.UV;
    uv.x *= Resolution.x / max(Resolution.y, 1.0);

    // Broad waves with different directions and periods make the background
    // shift gently without the irregular texture of noise.
    const float waveA = sin(uv.x * 3.1 + uv.y * 1.15 + Time * 0.20);
    const float waveB = sin(uv.x * -1.35 + uv.y * 2.45 - Time * 0.145 + 1.8);
    const float waveC = sin(uv.x * 1.05 + uv.y * -1.7 + Time * 0.095 + 4.1);
    const float field = saturate(
        0.5 + waveA * 0.22 + waveB * 0.16 + waveC * 0.10);

    // Ordered dithering turns the continuous field into exactly two flat colors.
    // Sample the 2x2 pattern on a 2-pixel grid for a finer dither texture.
    const float choice = step(Bayer2x2(int2(input.Position.xy) / 2), field);
    return lerp(ColorA, ColorB, choice);
}
