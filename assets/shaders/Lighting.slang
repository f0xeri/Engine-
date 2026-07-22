[[vk::binding(0, 0)]]
Sampler2D gTextures[];
[[vk::binding(1, 0)]]
ByteAddressBuffer gBuffers[];
[[vk::binding(2, 0)]]
Sampler2DShadow gShadowMaps[];

struct LightingParams
{
    column_major float4x4 invViewProj;   // screen NDC -> world
    column_major float4x4 lightViewProj; // world -> light clip

    float3 sunDir;
    float shadowBias;

    float3 sunRadiance;
    float shadowMapSize;

    float3 ambientRadiance;
    uint albedoTexture;

    uint normalTexture;
    uint depthSlot;
    uint shadowSlot;
    uint pad;
};

struct PushConstants
{
    uint paramsSlot;
    uint paramsOffset;
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

// 1 = lit, 0 = occluded
float shadowFactor(float4x4 lightViewProj,
                   float3 worldPos,
                   float NdotL,
                   float bias,
                   float mapSize,
                   uint mapSlot)
{
    float4 lightClip = mul(lightViewProj, float4(worldPos, 1.0));
    lightClip /= lightClip.w;

    const float2 lightUv = (lightClip.xy * 0.5) + 0.5;

    if (lightUv.x < 0.0 || lightUv.x > 1.0 || lightUv.y < 0.0 || lightUv.y > 1.0 ||
        lightClip.z <= 0.0)
    {
        return 1.0; // outside the map - lit
    }

    // bigger angle between normal and sun dir - bigger bias
    const float reference = lightClip.z + (bias / max(NdotL, 0.15));

    // 3x3 PCF
    // For each texel fetched (based on the sampler configuration of the filter mode), 
    // SampleCmp performs a comparison of the z value (3rd component of input) 
    // from the shader against the texel value (1 if the comparison passes; otherwise 0). 
    // SampleCmp then blends these 0 and 1 results for each texel together as in 
    // normal texture filtering (not an average) and returns the resulting [0..1] value to the shader.
    const float texelSize = 1.0 / mapSize;
    float sum = 0.0;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            const float2 offset = float2(x, y) * texelSize;
            sum += gShadowMaps[mapSlot].SampleCmpLevelZero(lightUv + offset, reference);
        }
    }

    return sum / 9.0;
}

[shader("fragment")]
float4 fsMain(VSOutput input) : SV_Target
{
    const LightingParams p = gBuffers[gPush.paramsSlot].Load<LightingParams>(gPush.paramsOffset);

    const float depth = gTextures[p.depthSlot].Sample(input.uv).r;
    if (depth <= 0.0)
    {
        return float4(BackgroundColor, 1.0);
    }

    const float4 albedo = gTextures[p.albedoTexture].Sample(input.uv);
    const float3 normal = gTextures[p.normalTexture].Sample(input.uv).xyz;

    float4 world = mul(p.invViewProj, float4(input.uv * 2.0 - 1.0, depth, 1.0));
    world /= world.w;

    const float3 sun = normalize(p.sunDir);
    const float NdotL = max(0.0, dot(normal, sun));
    const float shadow = shadowFactor(
        p.lightViewProj, world.xyz, NdotL, p.shadowBias, p.shadowMapSize, p.shadowSlot);

    const float3 light = (p.sunRadiance * NdotL * shadow) + p.ambientRadiance;

    return float4(albedo.rgb * light, 1.0);
}
