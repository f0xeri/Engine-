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
    output.normal = v.normal; // object space; fine while node transforms have no rotation
    output.uv = v.uv;
    return output;
}

// albedo * fixed-sun lambert
[shader("fragment")]
float4 fsMain(VSOutput input) : SV_Target
{
    const float4 albedo =
        gTextures[gPush.albedoTexture].Sample(input.uv) * gPush.baseColorFactor;
    if (albedo.a < 0.5)
    {
        discard; // Sponza foliage/chains are alpha-masked
    }

    const float3 sun = normalize(float3(0.4, 1.0, 0.3));
    const float light = 0.25 + (0.75 * max(0.0, dot(normalize(input.normal), sun)));
    return float4(albedo.rgb * light, 1.0);
}
