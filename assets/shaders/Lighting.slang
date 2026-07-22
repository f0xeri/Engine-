[[vk::binding(0, 0)]]
Sampler2D gTextures[];

struct PushConstants
{
    column_major float4x4 invViewProj; // matches glm's column-major storage

    float3 sunDir;
    uint albedoTexture;

    float3 sunColor;
    float sunIntensity;

    float3 ambientColor;
    float ambientIntensity;

    uint normalTexture;
    uint depthSlot;
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

static const float3 BackgroundColor = float3(0.7, 1.1, 2.0);

[shader("fragment")]
float4 fsMain(VSOutput input) : SV_Target
{
    const float depth = gTextures[gPush.depthSlot].Sample(input.uv).r;
    if (depth <= 0.0)
    {
        return float4(BackgroundColor, 1.0);
    }

    const float4 albedo = gTextures[gPush.albedoTexture].Sample(input.uv);
    const float3 normal = gTextures[gPush.normalTexture].Sample(input.uv).xyz;

    float4 world = float4(input.uv * 2.0 - 1.0, depth, 1.0);
    world = mul(gPush.invViewProj, world);
    world /= world.w;

    const float3 sun = normalize(gPush.sunDir);
    const float NdotL = max(0.0, dot(normal, sun));

    const float3 light =
        (gPush.sunColor * gPush.sunIntensity * NdotL) + (gPush.ambientColor * gPush.ambientIntensity);

    return float4(albedo.rgb * light, 1.0);
}
