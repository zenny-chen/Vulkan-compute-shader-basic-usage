%VK_SDK_PATH%/Bin/glslc -Os  --target-env=vulkan1.1  -fshader-stage=compute  -o simpleKernel.spv  simpleKernel.glsl
%VK_SDK_PATH%/Bin/glslc  --target-env=vulkan1.1  -fshader-stage=compute  -o advancedKernel.spv  advancedKernel.glsl
%VK_SDK_PATH%/Bin/glslangValidator  -V100  --target-env spirv1.3  -o texturingKernel.spv  texturingKernel.comp