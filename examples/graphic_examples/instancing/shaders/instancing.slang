struct InstancingPushConstants
{
    float4x4 view_projection;
    float4 animation;
};

[[vk::push_constant]] ConstantBuffer<InstancingPushConstants> push_constants;

struct VertexInput
{
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 normal : NORMAL;
    [[vk::location(2)]] float4 translation_and_scale : INSTANCE_TRANSFORM;
    [[vk::location(3)]] float4 color : INSTANCE_COLOR;
    uint instance_id : SV_InstanceID;
};

struct VertexOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 world_position : POSITION0;
    [[vk::location(1)]] float3 world_normal : NORMAL0;
    [[vk::location(2)]] float3 color : COLOR0;
};

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    const float time = push_constants.animation.x;
    const float phase = float(input.instance_id) * 0.173;
    const float angle = time * (0.32 + frac(phase) * 0.38) + phase;
    const float cosine = cos(angle);
    const float sine = sin(angle);
    const float3 local_position = input.position * input.translation_and_scale.w;
    const float3 rotated_position = float3(
            cosine * local_position.x + sine * local_position.z,
            local_position.y,
            -sine * local_position.x + cosine * local_position.z);
    const float3 rotated_normal = normalize(float3(
            cosine * input.normal.x + sine * input.normal.z,
            input.normal.y,
            -sine * input.normal.x + cosine * input.normal.z));
    const float bob = sin(time * 1.35 + phase * 2.0) * 0.16;
    const float3 world_position = rotated_position +
            input.translation_and_scale.xyz + float3(0.0, bob, 0.0);

    VertexOutput output;
    output.position = mul(push_constants.view_projection, float4(world_position, 1.0));
    output.world_position = world_position;
    output.world_normal = rotated_normal;
    output.color = input.color.rgb;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target0
{
    const float3 normal = normalize(input.world_normal);
    const float3 light_direction = normalize(float3(0.5, 1.0, 0.35));
    const float diffuse = max(dot(normal, light_direction), 0.0);
    const float height_tint = saturate(input.world_position.y * 0.08 + 0.5);
    float3 color = input.color * (0.16 + diffuse * 0.9) +
            float3(0.04, 0.06, 0.08) * height_tint;
    const float camera_distance = distance(push_constants.animation.yzw,
            input.world_position);
    const float camera_orbit_distance = length(push_constants.animation.yzw);
    const float fog_start = camera_orbit_distance * 0.68;
    const float fog_range = max(camera_orbit_distance * 0.64, 1.0);
    const float fog = saturate((camera_distance - fog_start) / fog_range) * 0.38;
    color = lerp(color, float3(0.018, 0.024, 0.035), fog);
    return float4(color, 1.0);
}
