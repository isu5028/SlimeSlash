Texture2D SpriteTexture : register(t0);
SamplerState SpriteSampler : register(s0);

cbuffer cbSpriteColor : register(b0)
{
    float4 FlashColor;
    float FlashAmount;
    float DissolveAmount;
    float2 SpriteColorPadding;
    float4 DissolveTextureSize;
};

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

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 color = SpriteTexture.Sample(SpriteSampler, input.UV);
    clip(color.a - (1.0f / 255.0f));

    const float frameWidth = DissolveTextureSize.x;
    const float sheetWidth = DissolveTextureSize.y;
    const float frameHeight = DissolveTextureSize.z;
    const int2 sourcePixel = int2(
        fmod(floor(input.UV.x * sheetWidth), frameWidth),
        min(floor(input.UV.y * frameHeight), frameHeight - 1.0));
    const float localX = (sourcePixel.x + 0.5) / frameWidth;
    const float localYFromBottom = 1.0 - (sourcePixel.y + 0.5) / frameHeight;
    const float diagonalPosition = (localX + localYFromBottom) * 0.5;
    const float localDissolve = saturate(
        (saturate(DissolveAmount) * 1.35 - diagonalPosition) / 0.35);
    clip(Bayer4x4(sourcePixel) - localDissolve);

    color.rgb = lerp(color.rgb, FlashColor.rgb, saturate(FlashAmount));
    return color;
}
