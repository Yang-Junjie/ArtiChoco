struct PushConstants
{
    float4x4 model_view_projection;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> push_constants;

[[vk::binding(0, 0)]] Texture2D<float4> base_color_texture;
[[vk::binding(1, 0)]] SamplerState base_color_sampler;

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
    output.position = mul(push_constants.model_view_projection, float4(input.position, 1.0));
    output.uv = input.uv;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    return base_color_texture.Sample(base_color_sampler, input.uv);
}
