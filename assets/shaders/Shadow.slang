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
    column_major float4x4 mvp;
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
    float2 uv;
};

[shader("vertex")]
VSOutput vsMain(uint vertexId : SV_VertexID)
{
    const Vertex v = gVertexBuffers[gPush.vertexBufferSlot][gPush.baseVertex + vertexId];

    VSOutput output;
    output.position = mul(gPush.mvp, float4(v.position, 1.0));
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
