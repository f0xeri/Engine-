#include "GeometryCommon.slang"

// Depth-only + alpha mask; uses subset of the shared layout
struct VSOutput
{
    float4 position : SV_Position;
    float2 uv;
    nointerpolation uint materialIndex;
};

[shader("vertex")]
VSOutput vsMain(uint vertexId : SV_VertexID, uint drawIndex : SV_DrawIndex)
{
    const DrawBlock draw =
        gBuffers[gPush.bufferSlot].Load<DrawBlock>(gPush.drawOffset + drawIndex * DrawStride);
    const ViewBlock view = gBuffers[gPush.bufferSlot].Load<ViewBlock>(gPush.viewOffset);
    const Vertex v = gVertexBuffers[draw.vertexBufferSlot][draw.baseVertex + vertexId];

    VSOutput output;
    output.position = mul(view.lightViewProj, mul(draw.model, float4(v.position, 1.0)));
    output.uv = v.uv;
    output.materialIndex = draw.materialIndex;
    return output;
}

[shader("fragment")]
void fsMain(VSOutput input)
{
    const GpuMaterial m = gBuffers[gPush.bufferSlot].Load<GpuMaterial>(
        gPush.materialOffset + input.materialIndex * MaterialStride);
    const float alpha = gTextures[m.albedoTexture].Sample(input.uv).a * m.baseColorFactor.a;
    if (alpha < 0.5)
    {
        discard;
    }
}
