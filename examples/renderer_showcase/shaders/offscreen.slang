struct OffscreenPushConstants
{
    float4 transform;
};

[[vk::push_constant]] ConstantBuffer<OffscreenPushConstants> push_constants;

struct VertexInput
{
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 color : COLOR0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 color : COLOR0;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    const float cosine = push_constants.transform.x;
    const float sine = push_constants.transform.y;
    const float scale = push_constants.transform.z;
    const float2 rotated = float2(input.position.x * cosine - input.position.y * sine,
                                  input.position.x * sine + input.position.y * cosine);
    VertexOutput output;
    output.position = float4(rotated * scale, input.position.z, 1.0);
    output.color = input.color;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target0
{
    const float vignette = 0.65 + 0.35 * (1.0 - length(input.position.xy) * 0.001);
    return float4(input.color * vignette, 1.0);
}
