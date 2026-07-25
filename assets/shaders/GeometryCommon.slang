// Shared types and bindings for the geometry passes (GBuffer, Shadow).

struct Vertex
{
    float3 position;
    float3 normal;
    float2 uv;
    float4 tangent;
};

// per-draw data
struct DrawBlock
{
    column_major float4x4 model;
    column_major float4x4 normalMatrix;
    uint vertexBufferSlot;
    uint baseVertex;
    uint materialIndex;
    uint _pad;
};
static const uint DrawStride = 144;

struct GpuMaterial
{
    float4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    uint albedoTexture;
    uint metallicRoughnessTexture;
    uint normalTexture;
};
static const uint MaterialStride = 48;

struct ViewBlock
{
    column_major float4x4 viewProj;
    column_major float4x4 lightViewProj;
};

[[vk::binding(0, 0)]]
Sampler2D gTextures[];
[[vk::binding(1, 0)]]
StructuredBuffer<Vertex, ScalarDataLayout> gVertexBuffers[];
[[vk::binding(1, 0)]]
ByteAddressBuffer gBuffers[];


struct GeometryPush
{
    uint bufferSlot; // FrameUniforms buffer holding view + draw + material data this frame
    uint viewOffset;
    uint drawOffset;
    uint materialOffset;
};

[[vk::push_constant]]
ConstantBuffer<GeometryPush> gPush;
