#version 450
#extension GL_KHR_shader_subgroup_basic : enable
#extension GL_KHR_shader_subgroup_vote : enable

layout(local_size_x = 1024, local_size_y = 1, local_size_z = 1) in;

layout(std430, set = 0, binding = 0) buffer dst {
    highp int dstBuffer[];
};

layout(std430, set = 0, binding = 1) buffer src {
    highp int srcBuffer[];
};

void main(void)
{
    const uint gid = gl_GlobalInvocationID.x;
    dstBuffer[gid] = srcBuffer[gid] + srcBuffer[gid];

    subgroupBarrier();

    if (gid == 1)
    {
        bool res1 = subgroupAny(gid == 1);
        bool res2 = subgroupAll(gid < 1024);
        dstBuffer[1] = int(res1) + int(res2);
    }
}

