[[vk::binding(0, 0)]] Texture2D<float4> lighting_output;
[[vk::binding(1, 0)]] SamplerState lighting_sampler;

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
float4 fragmentMain(FullscreenVertexOutput input) : SV_Target0
{
    const float3 hdr = lighting_output.Sample(lighting_sampler, input.uv).rgb;
    const float3 mapped = hdr / (hdr + 1.0);
    return float4(mapped, 1.0);
}
