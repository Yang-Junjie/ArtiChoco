struct RenderTexturePushConstants
{
    float4 animation;
};

[[vk::push_constant]] ConstantBuffer<RenderTexturePushConstants> push_constants;

struct FullscreenVertexOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
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
    const float time = push_constants.animation.x;
    const float2 centered = input.uv * 2.0 - 1.0;
    const float radius = length(centered);
    const float angle = atan2(centered.y, centered.x);
    const float rings = 0.5 + 0.5 * cos(radius * 34.0 - time * 3.2);
    const float spokes = 0.5 + 0.5 * cos(angle * 9.0 + time * 1.7);
    const float pulse = 0.5 + 0.5 * sin(time * 2.1 - radius * 12.0);
    const float mask = smoothstep(1.05, 0.18, radius);

    const float3 cyan = float3(0.08, 0.82, 0.9);
    const float3 magenta = float3(0.92, 0.16, 0.52);
    const float3 yellow = float3(1.0, 0.74, 0.12);
    float3 color = lerp(cyan, magenta, spokes);
    color = lerp(color, yellow, rings * pulse * 0.65);
    color *= (0.18 + 0.82 * mask);
    color += float3(0.015, 0.025, 0.05);
    return float4(color, 1.0);
}
