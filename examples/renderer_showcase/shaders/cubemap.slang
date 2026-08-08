[[vk::binding(0, 0)]] TextureCube<float4> demo_cubemap;
[[vk::binding(1, 0)]] SamplerState demo_sampler;

struct CubemapPushConstants
{
    float4 view;
};

[[vk::push_constant]] ConstantBuffer<CubemapPushConstants> push_constants;

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
    const float pi = 3.14159265359;
    const float longitude = (input.uv.x * 2.0 - 1.0) * pi + push_constants.view.x * 0.22;
    const float latitude = (0.5 - input.uv.y) * pi;
    const float cos_latitude = cos(latitude);
    const float3 direction = normalize(float3(
        cos_latitude * cos(longitude),
        sin(latitude),
        cos_latitude * sin(longitude)));
    const float mip_level = saturate(input.uv.x) * push_constants.view.y;
    const float3 hdr_color = demo_cubemap.SampleLevel(demo_sampler, direction, mip_level).rgb;
    const float3 tone_mapped = hdr_color / (hdr_color + 1.0);
    return float4(tone_mapped, 1.0);
}
