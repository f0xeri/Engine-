#include "GeometryCommon.slang"

struct VSOutput
{
    float4 position : SV_Position;
    float3 normal;  // world space
    float4 tangent; // world xyz + bitangent handedness
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

    const float3x3 normalMat = (float3x3)draw.normalMatrix;

    VSOutput output;
    output.position = mul(view.viewProj, mul(draw.model, float4(v.position, 1.0)));
    output.normal = mul(normalMat, v.normal);
    output.tangent = float4(mul(normalMat, v.tangent.xyz), v.tangent.w);
    output.uv = v.uv;
    output.materialIndex = draw.materialIndex;
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
    const GpuMaterial m = gBuffers[gPush.bufferSlot].Load<GpuMaterial>(
        gPush.materialOffset + input.materialIndex * MaterialStride);

    const float4 albedoColor = gTextures[m.albedoTexture].Sample(input.uv) * m.baseColorFactor;
    if (albedoColor.a < 0.5)
    {
        discard; // Sponza foliage/chains are alpha-masked
    }

    const float4 mr = gTextures[m.metallicRoughnessTexture].Sample(input.uv);
    const float metallic = mr.b * m.metallicFactor;
    const float roughness = mr.g * m.roughnessFactor;

    // tangent-space normal -> world via the interpolated TBN. Gram-Schmidt re-orthogonalizes
    // T against N since interpolation across the triangle skews them
    const float3 N = normalize(input.normal);
    const float3 T = normalize(input.tangent.xyz - N * dot(N, input.tangent.xyz));
    const float3 B = cross(N, T) * input.tangent.w;
    const float3 tn = gTextures[m.normalTexture].Sample(input.uv).xyz * 2.0 - 1.0;
    const float3 worldN = normalize(tn.x * T + tn.y * B + tn.z * N);

    FSOutput output;
    output.albedo = albedoColor;
    output.normal = float4(worldN, 0.0);
    output.material = float4(metallic, roughness, 1.0, 0.0);
    return output;
}
