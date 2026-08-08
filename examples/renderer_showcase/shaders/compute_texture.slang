struct ComputePushConstants
{
    float time;
};

[[vk::push_constant]] ConstantBuffer<ComputePushConstants> push_constants;
[[vk::binding(0, 0)]] RWTexture2D<float4> output_texture;

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 thread_id : SV_DispatchThreadID)
{
    uint width;
    uint height;
    output_texture.GetDimensions(width, height);
    if (thread_id.x >= width || thread_id.y >= height)
    {
        return;
    }

    const float2 uv = (float2(thread_id.xy) + 0.5) / float2(width, height);
    const float wave = 0.5 + 0.5 * sin(uv.x * 18.0 + uv.y * 12.0 + push_constants.time * 2.0);
    const float ring = 0.5 + 0.5 * cos(length(uv - 0.5) * 42.0 - push_constants.time * 3.0);
    output_texture[thread_id.xy] = float4(wave, ring, 1.0 - wave * ring, 1.0);
}
