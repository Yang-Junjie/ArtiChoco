struct CubemapPushConstants
{
    float4 color;
};

[[vk::push_constant]] ConstantBuffer<CubemapPushConstants> push_constants;

struct VertexOutput
{
    float4 position : SV_Position;
};

[shader("vertex")]
VertexOutput vertexMain(uint vertex_id : SV_VertexID)
{
    const float2 positions[3] = {
        float2(-1.0, -1.0),
        float2(3.0, -1.0),
        float2(-1.0, 3.0),
    };
    VertexOutput output;
    output.position = float4(positions[vertex_id], 0.0, 1.0);
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target0
{
    return push_constants.color;
}
