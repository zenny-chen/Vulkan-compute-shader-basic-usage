#version 450
#extension GL_EXT_shader_8bit_storage : enable
#extension GL_EXT_shader_16bit_storage : enable
#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_EXT_shader_atomic_int64 : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int16 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_float16 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_float32 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_float64 : enable
#extension GL_EXT_shader_atomic_float : enable
#extension GL_ARB_shader_clock : enable
#extension GL_EXT_shader_realtime_clock : enable

layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

// shared threadgroup memory size must be larger than zero.
// This constant will be specialized when creating the compute pipeline.
layout(constant_id = 3) const int SHARED_MEM_SIZE = 128;

layout(push_constant) uniform pushConstants{
    highp int constValue;
};

layout(set = 0, binding = 0) buffer writeonly dst {
    highp int dstBuffer[];
};

layout(set = 0, binding = 1) buffer readonly src {
    highp int srcBuffer[];
};

shared highp int share_buffer[SHARED_MEM_SIZE];
shared highp int sharedResult;

void main(void)
{
    const uint lid = gl_LocalInvocationIndex;

    sharedResult = 0;
    share_buffer[lid] = srcBuffer[lid] + constValue;

    memoryBarrierShared();
    barrier();

    if (lid == 0)
    {
        highp int sum = 0;
        for (int i = 0; i < SHARED_MEM_SIZE; i++) {
            sum += share_buffer[i];
        }

        sharedResult = sum;
    }

    memoryBarrierShared();
    barrier();

    dstBuffer[lid] = sharedResult;
}

