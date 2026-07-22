[[vk::binding(0, 0)]]
Sampler2D gTextures[];

struct PushConstants
{
    uint hdrTexture;
    float exposure;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> gPush;

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv;
};

[shader("vertex")]
VSOutput vsMain(uint vertexId : SV_VertexID)
{
    VSOutput output;
    output.uv = float2((vertexId << 1) & 2, vertexId & 2);
    output.position = float4(output.uv * 2.0 - 1.0, 0.0, 1.0);
    return output;
}

// Khronos PBR Neutral
// https://github.com/KhronosGroup/ToneMapping/blob/main/PBR_Neutral/pbrNeutral.glsl
float3 tonemap(float3 color)
{
    const float startCompression = 0.8 - 0.04;
    const float desaturation = 0.15;

    const float x = min(color.r, min(color.g, color.b));
    const float offset = x < 0.08 ? x - (6.25 * x * x) : 0.04;
    color -= offset;

    const float peak = max(color.r, max(color.g, color.b));
    if (peak < startCompression)
    {
        return color;
    }

    const float d = 1.0 - startCompression;
    const float newPeak = 1.0 - ((d * d) / (peak + d - startCompression));
    color *= newPeak / peak;

    const float g = 1.0 - (1.0 / ((desaturation * (peak - newPeak)) + 1.0));
    return lerp(color, float3(newPeak), g);
}

[shader("fragment")]
float4 fsMain(VSOutput input) : SV_Target
{
    // non-negative linear input color
    const float3 hdr = max(gTextures[gPush.hdrTexture].Sample(input.uv).rgb, 0.0);

    // exposure answers "how bright", the curve answers "how to compress"
    return float4(tonemap(hdr * gPush.exposure), 1.0);
}
