struct ComputePushConstants
{
    float time;
};

[[vk::push_constant]] ConstantBuffer<ComputePushConstants> push_constants;

[[vk::binding(0, 0)]] Texture2D<float4> source_texture;
[[vk::binding(1, 0)]] SamplerState source_sampler;
[[vk::binding(2, 0)]] RWTexture2D<float4> output_texture;

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint width;
    uint height;
    output_texture.GetDimensions(width, height);
    if (dispatch_thread_id.x >= width || dispatch_thread_id.y >= height)
    {
        return;
    }

    const float2 uv = (float2(dispatch_thread_id.xy) + 0.5) / float2(width, height);
    float4 color = source_texture.SampleLevel(source_sampler, uv, 0.0);
    const float pulse = 0.92 + 0.08 * sin(push_constants.time);
    color.rgb *= float3(pulse, 1.0, 2.0 - pulse);
    output_texture[dispatch_thread_id.xy] = color;
}
