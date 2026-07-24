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

// shared layout with GBuffer so the sandbox pushes one struct; only a prefix is read here
struct PushConstants
{
    column_major float4x4 model;
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
    float2 uv;
};

[shader("vertex")]
VSOutput vsMain(uint vertexId : SV_VertexID)
{
    const Vertex v = gVertexBuffers[gPush.vertexBufferSlot][gPush.baseVertex + vertexId];
    const ViewConstants view = gViewBuffers[gPush.viewSlot].Load<ViewConstants>(gPush.viewOffset);

    VSOutput output;
    output.position = mul(view.lightViewProj, mul(gPush.model, float4(v.position, 1.0)));
    output.uv = v.uv;
    return output;
}

[shader("fragment")]
void fsMain(VSOutput input)
{
    const float alpha =
        gTextures[gPush.albedoTexture].Sample(input.uv).a * gPush.baseColorFactor.a;
    if (alpha < 0.5)
    {
        discard;
    }
}
