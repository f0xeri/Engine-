static const float2 kPositions[3] = {
    float2(0.0, -1),
    float2(1, 1),
    float2(-1, 1),
};

static const float3 kColors[3] = {
    float3(1.0, 0, 0.),
    float3(0, 1.0, 0),
    float3(0, 0.0, 1.0),
};

struct PushConstants
{
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
    VSOutput output;
    output.position = float4(kPositions[vertexId], 0.5, 1.0);
    output.color = kColors[vertexId];
    return output;
}

[shader("fragment")]
float4 fsMain(VSOutput input) : SV_Target
{
    return float4(input.color * gPush.tint.rgb, 1.0);
}
