struct DemoUniforms
{
    float4 transform;
    float4 tint;
};

[[vk::binding(0, 0)]] ConstantBuffer<DemoUniforms> demo_uniforms;

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
    VertexOutput output;
    const float2 position = input.position.xy * demo_uniforms.transform.z + demo_uniforms.transform.xy;
    output.position = float4(position, input.position.z, 1.0);
    output.color = input.color * demo_uniforms.tint.rgb;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target0
{
    return float4(input.color, 1.0);
}
