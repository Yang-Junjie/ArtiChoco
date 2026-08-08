[[vk::binding(0, 0)]] Texture2D<float4> demo_texture;
[[vk::binding(1, 0)]] SamplerState demo_sampler;

struct VertexInput
{
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float2 uv : TEXCOORD0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;
    output.position = float4(input.position, 1.0);
    output.uv = input.uv;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target0
{
    return demo_texture.Sample(demo_sampler, input.uv);
}
