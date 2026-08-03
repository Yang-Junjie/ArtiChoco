struct VertexOutput
{
    float4 position : SV_Position;
    float3 color : COLOR0;
};

[shader("vertex")]
VertexOutput vertexMain(uint vertex_id : SV_VertexID)
{
    static const float2 positions[3] = {
        float2(0.0, -0.65),
        float2(0.6, 0.55),
        float2(-0.6, 0.55),
    };
    static const float3 colors[3] = {
        float3(0.95, 0.24, 0.20),
        float3(0.12, 0.72, 0.42),
        float3(0.20, 0.48, 0.96),
    };

    VertexOutput output;
    output.position = float4(positions[vertex_id], 0.0, 1.0);
    output.color = colors[vertex_id];
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    return float4(input.color, 1.0);
}
