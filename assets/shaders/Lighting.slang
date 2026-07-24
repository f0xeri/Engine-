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
    uint materialSlot;

    float3 cameraPos;
    float exposure;
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

// stub sky in physical radiance (nits) until real atmosphere/IBL
static const float3 SkyRadiance = float3(8000.0, 13000.0, 24000.0);

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

static const float PI = 3.14159265;

// a is alpha = perceptualRoughness^2
float D_GGX(float NoH, float a)
{
    const float a2 = a * a;
    const float f = (NoH * a2 - NoH) * NoH + 1.0;
    return a2 / (PI * f * f);
}

float3 F_Schlick(float u, float3 f0)
{
    return f0 + (1.0 - f0) * pow(saturate(1.0 - u), 5.0);
}

// height-correlated Smith, already folds the 1 / (4 NoV NoL) of the specular denom
float V_SmithGGXCorrelated(float NoV, float NoL, float a)
{
    const float a2 = a * a;
    const float GGXL = NoV * sqrt((-NoL * a2 + NoL) * NoL + a2);
    const float GGXV = NoL * sqrt((-NoV * a2 + NoV) * NoV + a2);
    return 0.5 / (GGXV + GGXL);
}

float Fd_Lambert()
{
    return 1.0 / PI;
}

// Cook-Torrance direct BRDF: f(l,v) = Fd + Fr, without radiance or NoL
float3 brdf(float3 n, float3 v, float3 l, float3 diffuseColor, float3 f0, float perceptualRoughness)
{
    const float3 h = normalize(v + l);

    const float NoV = abs(dot(n, v)) + 1e-5;
    const float NoL = clamp(dot(n, l), 0.0, 1.0);
    const float NoH = clamp(dot(n, h), 0.0, 1.0);
    const float LoH = clamp(dot(l, h), 0.0, 1.0);

    const float roughness = perceptualRoughness * perceptualRoughness; // perceptual -> alpha

    const float D = D_GGX(NoH, roughness);
    const float3 F = F_Schlick(LoH, f0);
    const float V = V_SmithGGXCorrelated(NoV, NoL, roughness);

    const float3 Fr = (D * V) * F;
    const float3 Fd = diffuseColor * Fd_Lambert();
    return Fd + Fr;
}

[shader("fragment")]
float4 fsMain(VSOutput input) : SV_Target
{
    const LightingParams p = gBuffers[gPush.paramsSlot].Load<LightingParams>(gPush.paramsOffset);

    const float depth = gTextures[p.depthSlot].Sample(input.uv).r;
    if (depth <= 0.0)
    {
        return float4(SkyRadiance * p.exposure, 1.0);
    }

    const float3 albedo = gTextures[p.albedoTexture].Sample(input.uv).rgb;
    const float3 N = normalize(gTextures[p.normalTexture].Sample(input.uv).xyz);
    const float2 mr = gTextures[p.materialSlot].Sample(input.uv).rg;
    const float metallic = mr.r;
    const float perceptualRoughness = max(mr.g, 0.045); // floor stops the NDF spiking to a mirror

    float4 world = mul(p.invViewProj, float4(input.uv * 2.0 - 1.0, depth, 1.0));
    world /= world.w;

    const float3 v = normalize(p.cameraPos - world.xyz);
    const float3 l = normalize(p.sunDir);
    const float NdotL = max(dot(N, l), 0.0);

    // split the material once; brdf and ambient share it
    const float3 f0 = lerp(float3(0.04), albedo, metallic);
    const float3 diffuseColor = (1.0 - metallic) * albedo;

    const float shadow = shadowFactor(
        p.lightViewProj, world.xyz, NdotL, p.shadowBias, p.shadowMapSize, p.shadowSlot);
    const float3 direct =
        brdf(N, v, l, diffuseColor, f0, perceptualRoughness) * p.sunRadiance * NdotL * shadow;

    // flat-environment ambient until IBL. ambientRadiance is sky irradiance, so the diffuse
    // response carries the same 1/PI as the sun term; flat f0 (not grazing Fresnel) for the
    // specular part, or every silhouette rim-glows against the constant ambient
    const float3 ambient = (p.ambientRadiance / PI) * (diffuseColor + f0);

    // pre-exposure
    return float4((direct + ambient) * p.exposure, 1.0);
}
