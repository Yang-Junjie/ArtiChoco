struct InstanceData
{
    float4 transform;
    float4 color;
};

[[vk::binding(0, 0)]] StructuredBuffer<InstanceData> instance_data;

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
VertexOutput vertexMain(VertexInput input, uint instance_id : SV_InstanceID)
{
    const InstanceData instance = instance_data[instance_id];
    VertexOutput output;
    output.position = float4(input.position.xy * instance.transform.z + instance.transform.xy,
                             input.position.z,
                             1.0);
    output.color = input.color * instance.color.rgb;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target0
{
    return float4(input.color, 1.0);
}
