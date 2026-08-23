[[vk::binding(0, 0)]] Texture2D<float4> color_output;
[[vk::binding(1, 0)]] Texture2D<float4> auxiliary_output;
[[vk::binding(2, 0)]] SamplerState output_sampler;

struct FullscreenVertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[shader("vertex")]
FullscreenVertexOutput vertexMain(uint vertex_id : SV_VertexID)
{
    const float2 uv = float2(float((vertex_id << 1) & 2), float(vertex_id & 2));
    FullscreenVertexOutput output;
    output.position = float4(uv.x * 2.0 - 1.0,
            1.0 - uv.y * 2.0, 0.0, 1.0);
    output.uv = uv;
    return output;
}

[shader("fragment")]
float4 fragmentMain(FullscreenVertexOutput input) : SV_Target
{
    if (input.uv.x < 0.5)
    {
        const float2 sample_uv = float2(input.uv.x * 2.0, input.uv.y);
        return color_output.Sample(output_sampler, sample_uv);
    }

    const float2 sample_uv = float2((input.uv.x - 0.5) * 2.0, input.uv.y);
    const float4 auxiliary = auxiliary_output.Sample(output_sampler, sample_uv);
    return float4(auxiliary.rgb, 1.0);
}
