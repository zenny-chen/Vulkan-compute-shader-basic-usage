%VK_SDK_PATH%/Bin/glslc  -fshader-stage=compute  -o simpleKernel.spv  simpleKernel.glsl
%VK_SDK_PATH%/Bin/glslc  -fshader-stage=compute  -o advancedKernel.spv  advancedKernel.glsl
%VK_SDK_PATH%/Bin/glslangValidator  --target-env vulkan1.1  -o texturingKernel.spv  texturingKernel.comp