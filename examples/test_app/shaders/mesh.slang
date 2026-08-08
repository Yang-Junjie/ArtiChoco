struct FrameUniforms
{
    float4x4 model_view_projection;
    float4 tint;
};

[[vk::push_constant]] ConstantBuffer<FrameUniforms> frame_uniforms;
[[vk::binding(1, 0)]] StructuredBuffer<float4> material_data;
[[vk::binding(2, 0)]] Texture2D<float4> base_color_texture;
[[vk::binding(3, 0)]] SamplerState base_color_sampler;

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
    output.position = mul(frame_uniforms.model_view_projection, float4(input.position, 1.0));
    output.uv = input.uv;
    return output;
}

struct FragmentOutput
{
    float4 color : SV_Target0;
    float4 auxiliary : SV_Target1;
};

[shader("fragment")]
FragmentOutput fragmentMain(VertexOutput input)
{
    FragmentOutput output;
    output.color = base_color_texture.Sample(base_color_sampler, input.uv) * frame_uniforms.tint * material_data[0];
    output.auxiliary = float4(input.uv, output.color.r, 1.0);
    return output;
}
