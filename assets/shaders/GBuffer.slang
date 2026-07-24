struct Vertex
{
    float3 position;
    float3 normal;
    float2 uv;
    float4 tangent;
};

[[vk::binding(0, 0)]]
Sampler2D gTextures[];
[[vk::binding(1, 0)]]
StructuredBuffer<Vertex, ScalarDataLayout> gVertexBuffers[];
[[vk::binding(1, 0)]]
ByteAddressBuffer gViewBuffers[];

struct ViewConstants
{
    column_major float4x4 viewProj;
    column_major float4x4 lightViewProj;
};

struct PushConstants
{
    column_major float4x4 model; // matches glm's column-major storage
    float4 baseColorFactor;
    uint vertexBufferSlot;
    uint baseVertex;
    uint albedoTexture;
    uint metallicRoughnessTexture;
    uint normalTexture;
    float metallicFactor;
    float roughnessFactor;
    uint viewSlot;
    uint viewOffset;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> gPush;

struct VSOutput
{
    float4 position : SV_Position;
    float3 normal;  // world space
    float4 tangent; // world xyz + bitangent handedness
    float2 uv;
};

[shader("vertex")]
VSOutput vsMain(uint vertexId : SV_VertexID)
{
    const Vertex v = gVertexBuffers[gPush.vertexBufferSlot][gPush.baseVertex + vertexId];
    const ViewConstants view = gViewBuffers[gPush.viewSlot].Load<ViewConstants>(gPush.viewOffset);

    // mat3(model): correct normal transform for rigid + uniform scale (see pbr.md)
    const float3x3 normalMat = (float3x3)gPush.model;

    VSOutput output;
    output.position = mul(view.viewProj, mul(gPush.model, float4(v.position, 1.0)));
    output.normal = mul(normalMat, v.normal);
    output.tangent = float4(mul(normalMat, v.tangent.xyz), v.tangent.w);
    output.uv = v.uv;
    return output;
}

struct FSOutput
{
    float4 albedo : SV_Target0;
    float4 normal : SV_Target1;
    float4 material : SV_Target2; // R metallic, G roughness, B occlusion
};

[shader("fragment")]
FSOutput fsMain(VSOutput input)
{
    const float4 albedoColor =
        gTextures[gPush.albedoTexture].Sample(input.uv) * gPush.baseColorFactor;
    if (albedoColor.a < 0.5)
    {
        discard; // Sponza foliage/chains are alpha-masked
    }

    const float4 mr = gTextures[gPush.metallicRoughnessTexture].Sample(input.uv);
    const float metallic = mr.b * gPush.metallicFactor;
    const float roughness = mr.g * gPush.roughnessFactor;

    // tangent-space normal -> world via the interpolated TBN. Gram-Schmidt re-orthogonalizes
    // T against N since interpolation across the triangle skews them
    const float3 N = normalize(input.normal);
    const float3 T = normalize(input.tangent.xyz - N * dot(N, input.tangent.xyz));
    const float3 B = cross(N, T) * input.tangent.w;
    const float3 tn = gTextures[gPush.normalTexture].Sample(input.uv).xyz * 2.0 - 1.0;
    const float3 worldN = normalize(tn.x * T + tn.y * B + tn.z * N);

    FSOutput output;
    output.albedo = albedoColor;
    output.normal = float4(worldN, 0.0);
    output.material = float4(metallic, roughness, 1.0, 0.0);
    return output;
}

