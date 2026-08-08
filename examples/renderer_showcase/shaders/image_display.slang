[[vk::binding(0, 0)]] Texture2D<float4> source_image;
[[vk::binding(1, 0)]] SamplerState source_sampler;

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[shader("vertex")]
VertexOutput vertexMain(uint vertex_id : SV_VertexID)
{
    const float2 uv = float2(float((vertex_id << 1) & 2), float(vertex_id & 2));
    VertexOutput output;
    output.position = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    output.uv = uv;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target0
{
    return source_image.Sample(source_sampler, input.uv);
}
