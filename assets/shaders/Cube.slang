// matches Asset::Vertex byte for byte (scalar layout)
struct Vertex
{
    float3 position;
    float3 normal;
    float2 uv;
};

// the engine-wide bindless set: binding 1 = storage buffers
[[vk::binding(1, 0)]]
StructuredBuffer<Vertex, ScalarDataLayout> gVertexBuffers[];

struct PushConstants
{
    column_major float4x4 mvp; // matches glm's column-major storage
    float4 tint;
    uint vertexBufferSlot;
    uint baseVertex;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> gPush;

struct VSOutput
{
    float4 position : SV_Position;
    float3 color;
};

[shader("vertex")]
VSOutput vsMain(uint vertexId : SV_VertexID)
{
    const Vertex v = gVertexBuffers[gPush.vertexBufferSlot][gPush.baseVertex + vertexId];

    VSOutput output;
    output.position = mul(gPush.mvp, float4(v.position, 1.0));
    output.color = v.position + 0.5;
    return output;
}

[shader("fragment")]
float4 fsMain(VSOutput input) : SV_Target
{
    return float4(input.color * gPush.tint.rgb, 1.0);
}
