struct LightingPushConstants
{
    float4x4 model_view_projection;
    float4x4 model;
};

[[vk::push_constant]] ConstantBuffer<LightingPushConstants> push_constants;

struct VertexInput
{
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 normal : NORMAL;
};

struct VertexOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 world_position : POSITION0;
    [[vk::location(1)]] float3 world_normal : NORMAL0;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;
    const float4 world_position = mul(push_constants.model,
            float4(input.position, 1.0));
    output.position = mul(push_constants.model_view_projection,
            float4(input.position, 1.0));
    output.world_position = world_position.xyz;
    output.world_normal = normalize(mul((float3x3)push_constants.model, input.normal));
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target0
{
    const float3 normal = normalize(input.world_normal);
    const float3 surface_to_light = normalize(float3(0.45, 1.0, 0.35));
    const float3 camera_position = float3(4.8, 3.2, 6.4);
    const float3 view_direction = normalize(camera_position - input.world_position);
    const float3 half_direction = normalize(surface_to_light + view_direction);

    const float diffuse = max(dot(normal, surface_to_light), 0.0);
    const float specular = pow(max(dot(normal, half_direction), 0.0), 48.0);
    const float3 albedo = float3(0.28, 0.58, 0.92);
    const float3 color = albedo * (0.13 + diffuse * 0.92) + specular * 0.38;
    return float4(color, 1.0);
}
