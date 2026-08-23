[[vk::binding(0, 0)]] Texture2D<float4> render_texture;
[[vk::binding(1, 0)]] SamplerState render_texture_sampler;

struct DisplayPushConstants
{
    float4x4 model_view_projection;
    float4x4 model;
};

[[vk::push_constant]] ConstantBuffer<DisplayPushConstants> push_constants;

struct VertexInput
{
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float2 uv : TEXCOORD0;
    [[vk::location(2)]] float3 normal : NORMAL;
};

struct VertexOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
    [[vk::location(1)]] float3 world_normal : NORMAL0;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;
    output.position = mul(push_constants.model_view_projection,
            float4(input.position, 1.0));
    output.uv = input.uv;
    output.world_normal = normalize(mul((float3x3)push_constants.model, input.normal));
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target0
{
    const float3 rendered_color = render_texture.Sample(
            render_texture_sampler, input.uv).rgb;
    const float3 normal = normalize(input.world_normal);
    const float diffuse = max(dot(normal, normalize(float3(0.4, 0.9, 0.55))), 0.0);
    return float4(rendered_color * (0.3 + diffuse * 0.9), 1.0);
}
