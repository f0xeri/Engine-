struct Vertex
{
    float3 position;
    float3 normal;
    float2 uv;
};

[[vk::binding(0, 0)]]
Sampler2D gTextures[];
[[vk::binding(1, 0)]]
StructuredBuffer<Vertex, ScalarDataLayout> gVertexBuffers[];

struct PushConstants
{
    column_major float4x4 mvp; // matches glm's column-major storage
    float4 baseColorFactor;
    uint vertexBufferSlot;
    uint baseVertex;
    uint albedoTexture;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> gPush;

struct VSOutput
{
    float4 position : SV_Position;
    float3 normal;
    float2 uv;
};

[shader("vertex")]
VSOutput vsMain(uint vertexId : SV_VertexID)
{
    const Vertex v = gVertexBuffers[gPush.vertexBufferSlot][gPush.baseVertex + vertexId];

    VSOutput output;
    output.position = mul(gPush.mvp, float4(v.position, 1.0));
    output.normal = v.normal;
    output.uv = v.uv;
    return output;
}

struct FSOutput
{
    float4 albedo : SV_Target0;
    float4 normal : SV_Target1;
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

    FSOutput output;
    output.albedo = albedoColor;
    output.normal = float4(normalize(input.normal), 0.0);
    return output;
}

