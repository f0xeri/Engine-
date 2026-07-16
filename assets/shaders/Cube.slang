static const float3 kCorners[8] = {
    float3(-0.5, -0.5, -0.5),
    float3(0.5, -0.5, -0.5),
    float3(0.5, 0.5, -0.5),
    float3(-0.5, 0.5, -0.5),
    float3(-0.5, -0.5, 0.5),
    float3(0.5, -0.5, 0.5),
    float3(0.5, 0.5, 0.5),
    float3(-0.5, 0.5, 0.5),
};

static const uint kIndices[36] = {
    0, 1, 2, 0, 2, 3, // -Z
    4, 6, 5, 4, 7, 6, // +Z
    0, 3, 7, 0, 7, 4, // -X
    1, 5, 6, 1, 6, 2, // +X
    0, 4, 5, 0, 5, 1, // -Y
    3, 2, 6, 3, 6, 7, // +Y
};

struct PushConstants
{
    column_major float4x4 mvp; // matches glm's column-major storage
    float4 tint;
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
    const float3 pos = kCorners[kIndices[vertexId]];

    VSOutput output;
    output.position = mul(gPush.mvp, float4(pos, 1.0));
    output.color = pos + 0.5;
    return output;
}

[shader("fragment")]
float4 fsMain(VSOutput input) : SV_Target
{
    return float4(input.color * gPush.tint.rgb, 1.0);
}
