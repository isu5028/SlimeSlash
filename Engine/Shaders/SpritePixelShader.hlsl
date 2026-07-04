Texture2D SpriteTexture : register(t0);
SamplerState SpriteSampler : register(s0);

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 color = SpriteTexture.Sample(SpriteSampler, input.UV);
    clip(color.a - (1.0f / 255.0f));
    return color;
}
