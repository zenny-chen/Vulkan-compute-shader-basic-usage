// 你好，世界。世界、今日は～ (Code Learning Studio® CPU Hacker™ Product)
// VulkanTest.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>

#ifdef _WIN32
#define FILE_BINARY_MODE                    "b"
#else
#define FILE_BINARY_MODE
#endif // _WIN32


enum MY_CONSTANTS
{
    MAX_VULKAN_LAYER_COUNT = 64,
    MAX_VULKAN_GLOBAL_EXT_PROPS = 64,
    MAX_GPU_COUNT = 8,
    MAX_QUEUE_FAMILY_PROPERTY_COUNT = 8,

    TEST_IMAGE_WIDTH = 1024,
    TEST_IMAGE_HEIGHT = 1024
};

static VkLayerProperties s_layerProperties[MAX_VULKAN_LAYER_COUNT];
static VkExtensionProperties s_instanceExtensions[MAX_VULKAN_LAYER_COUNT][MAX_VULKAN_GLOBAL_EXT_PROPS];
static uint32_t s_layerCount;
static uint32_t s_instanceExtensionCounts[MAX_VULKAN_LAYER_COUNT];

static VkInstance s_instance = NULL;
static VkDevice s_specDevice = NULL;
static uint32_t s_specQueueFamilyIndex = 0;
static VkPhysicalDeviceMemoryProperties s_memoryProperties = { 0 };

static const char* const s_deviceTypes[] = {
    "Other",
    "Integrated GPU",
    "Discrete GPU",
    "Virtual GPU",
    "CPU"
};

static VkResult init_global_extension_properties(uint32_t layerIndex)
{
    uint32_t instance_extension_count;
    VkResult res;
    VkLayerProperties* currLayer = &s_layerProperties[layerIndex];
    char const * const layer_name = currLayer->layerName;

    do {
        res = vkEnumerateInstanceExtensionProperties(layer_name, &instance_extension_count, NULL);
        if (res != VK_SUCCESS) {
            return res;
        }

        if (instance_extension_count == 0) {
            return VK_SUCCESS;
        }
        if (instance_extension_count > MAX_VULKAN_GLOBAL_EXT_PROPS) {
            instance_extension_count = MAX_VULKAN_GLOBAL_EXT_PROPS;
        }

        s_instanceExtensionCounts[layerIndex] = instance_extension_count;
        res = vkEnumerateInstanceExtensionProperties(layer_name, &instance_extension_count, s_instanceExtensions[layerIndex]);
    } while (res == VK_INCOMPLETE);

    return res;
}

static VkResult init_global_layer_properties(void)
{
    uint32_t instance_layer_count;
    VkResult res;

    /*
     * It's possible, though very rare, that the number of
     * instance layers could change. For example, installing something
     * could include new layers that the loader would pick up
     * between the initial query for the count and the
     * request for VkLayerProperties. The loader indicates that
     * by returning a VK_INCOMPLETE status and will update the
     * the count parameter.
     * The count parameter will be updated with the number of
     * entries loaded into the data pointer - in case the number
     * of layers went down or is smaller than the size given.
    */
    do
    {
        res = vkEnumerateInstanceLayerProperties(&instance_layer_count, NULL);
        if (res != VK_SUCCESS) {
            return res;
        }

        if (instance_layer_count == 0) {
            return VK_SUCCESS;
        }

        if (instance_layer_count > MAX_VULKAN_LAYER_COUNT) {
            instance_layer_count = MAX_VULKAN_LAYER_COUNT;
        }

        res = vkEnumerateInstanceLayerProperties(&instance_layer_count, s_layerProperties);
    }
    while (res == VK_INCOMPLETE);

    /*
     * Now gather the extension list for each instance layer.
    */
    s_layerCount = instance_layer_count;
    for (uint32_t i = 0; i < instance_layer_count; i++)
    {
        res = init_global_extension_properties(i);
        if (res != VK_SUCCESS)
        {
            printf("Query global extension properties error: %d\n", res);
            break;
        }
    }

    return res;
}

static VkResult InitializeInstance(void)
{
    VkResult result = init_global_layer_properties();
    if (result != VK_SUCCESS)
    {
        printf("init_global_layer_properties failed: %d\n", result);
        return result;
    }

    // Query the API version
    uint32_t apiVersion = VK_API_VERSION_1_2;
    vkEnumerateInstanceVersion(&apiVersion);
    // Zero the patch version
    apiVersion &= ~0x0fffU;
    printf("Current API version: %u.%u\n", (apiVersion >> 22) & 0x03ffU, (apiVersion >> 12) & 0x03ffU);

    // initialize the VkApplicationInfo structure
    const VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = NULL,
        .pApplicationName = "Vulkan Test",
        .applicationVersion = 1,
        .pEngineName = "My Engine",
        .engineVersion = 1,
        .apiVersion = apiVersion
    };

    // initialize the VkInstanceCreateInfo structure
    const VkInstanceCreateInfo inst_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = 0,
        .ppEnabledExtensionNames = NULL,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = NULL
    };

    result = vkCreateInstance(&inst_info, NULL, &s_instance);
    if (result == VK_ERROR_INCOMPATIBLE_DRIVER) {
        puts("cannot find a compatible Vulkan ICD");
    }
    else if (result != VK_SUCCESS) {
        printf("vkCreateInstance failed: %d\n", result);
    }

    return result;
}

static VkResult InitializeDevice(VkQueueFlagBits queueFlag, VkPhysicalDeviceMemoryProperties* pMemoryProperties)
{
    VkPhysicalDevice physicalDevices[MAX_GPU_COUNT] = { NULL };
    uint32_t gpu_count = 0;
    VkResult res = vkEnumeratePhysicalDevices(s_instance, &gpu_count, NULL);
    if (res != VK_SUCCESS)
    {
        printf("vkEnumeratePhysicalDevices failed: %d\n", res);
        return res;
    }

    if (gpu_count > MAX_GPU_COUNT) {
        gpu_count = MAX_GPU_COUNT;
    }

    res = vkEnumeratePhysicalDevices(s_instance, &gpu_count, physicalDevices);
    if (res != VK_SUCCESS)
    {
        printf("vkEnumeratePhysicalDevices failed: %d\n", res);
        return res;
    }

    // TODO: The following code is used to choose the working device and may be not necessary to other projects...
    const bool isSingle = gpu_count == 1;
    printf("This application has detected there %s %u Vulkan capable device%s installed: \n",
        isSingle ? "is" : "are",
        gpu_count,
        isSingle ? "" : "s");

    VkPhysicalDeviceProperties props = { 0 };
    for (uint32_t i = 0; i < gpu_count; i++)
    {
        vkGetPhysicalDeviceProperties(physicalDevices[i], &props);
        printf("\n======== Device %u info ========\n", i);
        printf("Device name: %s\n", props.deviceName);
        printf("Device type: %s\n", s_deviceTypes[props.deviceType]);
        printf("API version: %u.%u\n", (props.apiVersion >> 22) & 0x03ffU, (props.apiVersion >> 12) & 0x03ffU);
        printf("Driver version: 0x%.8X\n", props.driverVersion);
        puts("");
    }
    puts("Please choose which device to use...");

    char inputBuffer[8] = { '\0' };
    const char* input = gets_s(inputBuffer, sizeof(inputBuffer));
    if (input == NULL) {
        input = "0";
    }
    const unsigned deviceIndex = atoi(input);

    if (deviceIndex >= gpu_count)
    {
        printf("Your input (%u) exceeds the max number of available devices (%u)\n", deviceIndex, gpu_count);
        return VK_ERROR_DEVICE_LOST;
    }

    printf("You have chosen device[%u]...\n", deviceIndex);

    VkPhysicalDeviceSubgroupProperties subgroupProperties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES
    };
    VkPhysicalDeviceProperties2 properties2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &subgroupProperties
    };
    vkGetPhysicalDeviceProperties2(physicalDevices[deviceIndex], &properties2);
    printf("Current device subgroup size: %u\n", subgroupProperties.subgroupSize);

    vkGetPhysicalDeviceMemoryProperties(physicalDevices[deviceIndex], pMemoryProperties);

    const float queue_priorities[1] = { 0.0f };
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = NULL,
        .queueCount = 1,
        .pQueuePriorities = queue_priorities
    };

    uint32_t queueFamilyPropertyCount = 0;
    VkQueueFamilyProperties queueFamilyProperties[MAX_QUEUE_FAMILY_PROPERTY_COUNT];

    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[deviceIndex], &queueFamilyPropertyCount, NULL);
    if (queueFamilyPropertyCount > MAX_QUEUE_FAMILY_PROPERTY_COUNT) {
        queueFamilyPropertyCount = MAX_QUEUE_FAMILY_PROPERTY_COUNT;
    }

    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[deviceIndex], &queueFamilyPropertyCount, queueFamilyProperties);

    bool found = false;
    for (uint32_t i = 0; i < queueFamilyPropertyCount; i++)
    {
        if ((queueFamilyProperties[i].queueFlags & queueFlag) != 0)
        {
            queue_info.queueFamilyIndex = i;
            found = true;
            break;
        }
    }

    s_specQueueFamilyIndex = queue_info.queueFamilyIndex;

    const VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = NULL,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = NULL,
        .enabledExtensionCount = 0,
        .ppEnabledExtensionNames = NULL,
        .pEnabledFeatures = NULL
    };

    res = vkCreateDevice(physicalDevices[deviceIndex], &device_info, NULL, &s_specDevice);
    if (res != VK_SUCCESS) {
        printf("vkCreateDevice failed: %d\n", res);
    }

    return res;
}

static VkResult InitializeCommandBuffer(uint32_t queueFamilyIndex, VkDevice device, VkCommandPool *pCommandPool, 
    VkCommandBuffer commandBuffers[], uint32_t commandBufferCount)
{
    const VkCommandPoolCreateInfo cmd_pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queueFamilyIndex = queueFamilyIndex
    };

    VkResult res = vkCreateCommandPool(device, &cmd_pool_info, NULL, pCommandPool);
    if (res != VK_SUCCESS)
    {
        printf("vkCreateCommandPool failed: %d\n", res);
        return res;
    }

    /* Create the command buffer from the command pool */
    const VkCommandBufferAllocateInfo cmdInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = *pCommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = commandBufferCount
    };

    res = vkAllocateCommandBuffers(device, &cmdInfo, commandBuffers);
    return res;
}

// deviceMemories[0] as host visible memory, deviceMemories[1] as device local memory
// deviceBuffers[0] as host temporal buffer, deviceBuffers[1] as device dst buffer, deviceBuffers[2] as device src buffer
static VkResult AllocateMemoryAndBuffers(VkDevice device, const VkPhysicalDeviceMemoryProperties *pMemoryProperties, VkDeviceMemory deviceMemories[2],
    VkBuffer deviceBuffers[3], VkDeviceSize bufferSize, uint32_t queueFamilyIndex)
{
    const VkBufferCreateInfo hostBufCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .size = bufferSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = (uint32_t[]){ queueFamilyIndex }
    };

    VkResult res = vkCreateBuffer(device, &hostBufCreateInfo, NULL, &deviceBuffers[0]);
    if (res != VK_SUCCESS)
    {
        printf("vkCreateBuffer failed: %d\n", res);
        return res;
    }

    VkMemoryRequirements hostMemBufRequirements = { 0 };
    vkGetBufferMemoryRequirements(device, deviceBuffers[0], &hostMemBufRequirements);

    uint32_t memoryTypeIndex;
    // Find host visible property memory type index
    for (memoryTypeIndex = 0; memoryTypeIndex < pMemoryProperties->memoryTypeCount; memoryTypeIndex++)
    {
        const VkMemoryType memoryType = pMemoryProperties->memoryTypes[memoryTypeIndex];
        if ((memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0 &&
            (memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0 &&
            pMemoryProperties->memoryHeaps[memoryType.heapIndex].size >= hostMemBufRequirements.size)
        {
            // found our memory type!
            printf("Host visible memory size: %zuMB\n", pMemoryProperties->memoryHeaps[memoryType.heapIndex].size / (1024 * 1024));
            break;
        }
    }

    const VkMemoryAllocateInfo hostMemAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = NULL,
        .allocationSize = hostMemBufRequirements.size,
        .memoryTypeIndex = memoryTypeIndex
    };

    res = vkAllocateMemory(device, &hostMemAllocInfo, NULL, &deviceMemories[0]);
    if (res != VK_SUCCESS)
    {
        printf("vkAllocateMemory failed: %d\n", res);
        return res;
    }

    res = vkBindBufferMemory(device, deviceBuffers[0], deviceMemories[0], 0);
    if (res != VK_SUCCESS)
    {
        printf("vkBindBufferMemory failed: %d\n", res);
        return res;
    }

    const VkBufferCreateInfo deviceBufCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .size = bufferSize,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = (uint32_t[]){ queueFamilyIndex }
    };

    res = vkCreateBuffer(device, &deviceBufCreateInfo, NULL, &deviceBuffers[1]);
    if (res != VK_SUCCESS)
    {
        printf("vkCreateBuffer failed: %d\n", res);
        return res;
    }

    res = vkCreateBuffer(device, &deviceBufCreateInfo, NULL, &deviceBuffers[2]);
    if (res != VK_SUCCESS)
    {
        printf("vkCreateBuffer failed: %d\n", res);
        return res;
    }

    VkMemoryRequirements deviceMemBufRequirements = { 0 };
    vkGetBufferMemoryRequirements(device, deviceBuffers[1], &deviceMemBufRequirements);

    // two memory buffers share one device local memory.
    const VkDeviceSize deviceMemTotalSize = deviceMemBufRequirements.size * 2;
    // Find device local property memory type index
    for (memoryTypeIndex = 0; memoryTypeIndex < pMemoryProperties->memoryTypeCount; memoryTypeIndex++)
    {
        const VkMemoryType memoryType = pMemoryProperties->memoryTypes[memoryTypeIndex];
        if ((memoryType.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0 &&
            pMemoryProperties->memoryHeaps[memoryType.heapIndex].size >= deviceMemTotalSize)
        {
            // found our memory type!
            printf("Device local VRAM size: %zuMB\n", pMemoryProperties->memoryHeaps[memoryType.heapIndex].size / (1024 * 1024));
            break;
        }
    }

    const VkMemoryAllocateInfo deviceMemAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = NULL,
        .allocationSize = deviceMemTotalSize,
        .memoryTypeIndex = memoryTypeIndex
    };

    res = vkAllocateMemory(device, &deviceMemAllocInfo, NULL, &deviceMemories[1]);
    if (res != VK_SUCCESS)
    {
        printf("vkAllocateMemory failed: %d\n", res);
        return res;
    }

    res = vkBindBufferMemory(device, deviceBuffers[1], deviceMemories[1], 0);
    if (res != VK_SUCCESS)
    {
        printf("vkBindBufferMemory failed: %d\n", res);
        return res;
    }

    res = vkBindBufferMemory(device, deviceBuffers[2], deviceMemories[1], deviceMemBufRequirements.size);
    if (res != VK_SUCCESS)
    {
        printf("vkBindBufferMemory failed: %d\n", res);
        return res;
    }

    // Initialize the source host visible temporal buffer
    const int elemCount = (int)(bufferSize / sizeof(int));
    void* hostBuffer = NULL;
    res = vkMapMemory(device, deviceMemories[0], 0, bufferSize, 0, &hostBuffer);
    if (res != VK_SUCCESS)
    {
        printf("vkMapMemory failed: %d\n", res);
        return res;
    }
    int* srcMem = hostBuffer;
    for (int i = 0; i < elemCount; i++) {
        srcMem[i] = i;
    }

    vkUnmapMemory(device, deviceMemories[0]);

    return res;
}

// deviceMemories[0] as host visible memory, deviceMemories[1] as device local memory
// deviceBuffers[0] as host temporal buffer, deviceBuffers[1] as device dst buffer
static VkResult AllocateMemoryAndCreateImageWithSampler(VkDevice device, const VkPhysicalDeviceMemoryProperties* pMemoryProperties, VkDeviceMemory deviceMemories[2],
    VkBuffer deviceBuffers[2], size_t width, size_t height, uint32_t queueFamilyIndex, VkImage outImages[1], VkImageView outImageViews[1], VkSampler outSamplers[1])
{
    const size_t imageBufferSize = width * height * sizeof(uint32_t) * 4;   // each pixel element is a uvec4

    const VkBufferCreateInfo hostBufCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .size = imageBufferSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = (uint32_t[]){ queueFamilyIndex }
    };

    VkResult res = vkCreateBuffer(device, &hostBufCreateInfo, NULL, &deviceBuffers[0]);
    if (res != VK_SUCCESS)
    {
        printf("vkCreateBuffer failed: %d\n", res);
        return res;
    }

    VkMemoryRequirements hostMemBufRequirements = { 0 };
    vkGetBufferMemoryRequirements(device, deviceBuffers[0], &hostMemBufRequirements);

    uint32_t memoryTypeIndex;
    // Find host visible property memory type index
    for (memoryTypeIndex = 0; memoryTypeIndex < pMemoryProperties->memoryTypeCount; memoryTypeIndex++)
    {
        const VkMemoryType memoryType = pMemoryProperties->memoryTypes[memoryTypeIndex];
        if ((memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0 &&
            (memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0 &&
            pMemoryProperties->memoryHeaps[memoryType.heapIndex].size >= hostMemBufRequirements.size)
        {
            // found our memory type!
            printf("Host visible memory size: %zuMB\n", pMemoryProperties->memoryHeaps[memoryType.heapIndex].size / (1024 * 1024));
            break;
        }
    }

    const VkMemoryAllocateInfo hostMemAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = NULL,
        .allocationSize = hostMemBufRequirements.size,
        .memoryTypeIndex = memoryTypeIndex
    };

    res = vkAllocateMemory(device, &hostMemAllocInfo, NULL, &deviceMemories[0]);
    if (res != VK_SUCCESS)
    {
        printf("vkAllocateMemory failed: %d\n", res);
        return res;
    }

    res = vkBindBufferMemory(device, deviceBuffers[0], deviceMemories[0], 0);
    if (res != VK_SUCCESS)
    {
        printf("vkBindBufferMemory failed: %d\n", res);
        return res;
    }

    const VkImageCreateInfo imageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_UINT,
        .extent = { 1024U, 1024U, 1U },
        .mipLevels = 1U,
        .arrayLayers = 1U,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_LINEAR,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = (uint32_t[]){ queueFamilyIndex }
    };

    VkResult status = vkCreateImage(device, &imageCreateInfo, NULL, outImages);
    if (status != VK_SUCCESS)
    {
        printf("vkCreateImage failed: %d\n", status);
        return status;
    }

    VkMemoryRequirements imageMemBufRequirements = { 0 };
    vkGetImageMemoryRequirements(device, outImages[0], &imageMemBufRequirements);

    const VkBufferCreateInfo deviceBufCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .size = imageMemBufRequirements.size,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = (uint32_t[]){ queueFamilyIndex }
    };

    res = vkCreateBuffer(device, &deviceBufCreateInfo, NULL, &deviceBuffers[1]);
    if (res != VK_SUCCESS)
    {
        printf("vkCreateBuffer failed: %d\n", res);
        return res;
    }

    // Find device local property memory type index
    for (memoryTypeIndex = 0; memoryTypeIndex < pMemoryProperties->memoryTypeCount; memoryTypeIndex++)
    {
        const VkMemoryType memoryType = pMemoryProperties->memoryTypes[memoryTypeIndex];
        if ((memoryType.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0 &&
            pMemoryProperties->memoryHeaps[memoryType.heapIndex].size >= imageMemBufRequirements.size * 2)
        {
            // found our memory type!
            printf("Device local VRAM size: %zuMB\n", pMemoryProperties->memoryHeaps[memoryType.heapIndex].size / (1024U * 1024U));
            break;
        }
    }

    const VkMemoryAllocateInfo deviceMemAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = NULL,
        .allocationSize = imageMemBufRequirements.size * 2, // one memory buffer and one image buffer share the same device local memory.
        .memoryTypeIndex = memoryTypeIndex
    };

    res = vkAllocateMemory(device, &deviceMemAllocInfo, NULL, &deviceMemories[1]);
    if (res != VK_SUCCESS)
    {
        printf("vkAllocateMemory failed: %d\n", res);
        return res;
    }

    res = vkBindBufferMemory(device, deviceBuffers[1], deviceMemories[1], 0);
    if (res != VK_SUCCESS)
    {
        printf("vkBindBufferMemory failed: %d\n", res);
        return res;
    }

    res = vkBindImageMemory(device, outImages[0], deviceMemories[1], imageMemBufRequirements.size);
    if (res != VK_SUCCESS)
    {
        printf("vkBindImageMemory failed: %d\n", res);
        return res;
    }

    const VkImageViewCreateInfo imageViewCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .image = outImages[0],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_UINT,
        .components = { 
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0U,
            .levelCount = 1U,
            .baseArrayLayer = 0U,
            .layerCount = 1U
        }
    };

    status = vkCreateImageView(device, &imageViewCreateInfo, NULL, outImageViews);
    if (status != VK_SUCCESS)
    {
        printf("vkCreateImageView failed: %d\n", status);
        return status;
    }

    const VkSamplerCreateInfo samplerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 0.0f,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_NEVER,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK,
        .unnormalizedCoordinates = VK_TRUE
    };

    status = vkCreateSampler(device, &samplerCreateInfo, NULL, outSamplers);
    if (status != VK_SUCCESS)
    {
        printf("vkCreateSampler failed: %d\n", status);
        return status;
    }

    // Initialize the source host visible temporal buffer
    const uint32_t elemCount = (uint32_t)(imageBufferSize / sizeof(uint32_t));
    void* hostBuffer = NULL;
    res = vkMapMemory(device, deviceMemories[0], 0, imageBufferSize, 0, &hostBuffer);
    if (res != VK_SUCCESS)
    {
        printf("vkMapMemory failed: %d\n", res);
        return res;
    }
    uint32_t* srcMem = hostBuffer;
    for (uint32_t i = 0U; i < elemCount; i++) {
        srcMem[i] = i;
    }

    vkUnmapMemory(device, deviceMemories[0]);

    return status;
}

static void WriteBufferAndSync(VkCommandBuffer commandBuffer, uint32_t queueFamilyIndex, VkBuffer dstDeviceBuffer, VkBuffer srcHostBuffer, size_t size)
{
    const VkBufferCopy copyRegion = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size
    };
    vkCmdCopyBuffer(commandBuffer, srcHostBuffer, dstDeviceBuffer, 1, &copyRegion);

    const VkBufferMemoryBarrier bufferBarrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .srcQueueFamilyIndex = queueFamilyIndex,
        .dstQueueFamilyIndex = queueFamilyIndex,
        .buffer = dstDeviceBuffer,
        .offset = 0,
        .size = VK_WHOLE_SIZE
    };

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
        0, NULL, 1, &bufferBarrier, 0, NULL);
}

static void CopyToImageBufferAndSync(VkCommandBuffer commandBuffer, uint32_t queueFamilyIndex, VkImage dstImage, VkBuffer srcHostBuffer, size_t width, size_t height)
{
    // The following barrier operation only intends to transit the image layout from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL.
    const VkImageMemoryBarrier transitionBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = 0,
        .dstAccessMask = 0,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = queueFamilyIndex,
        .dstQueueFamilyIndex = queueFamilyIndex,
        .image = dstImage,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0U,
            .levelCount = 1U,
            .baseArrayLayer = 0U,
            .layerCount = 1U
        }
    };
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
        0, NULL, 0, NULL, 1, &transitionBarrier);

    const VkBufferImageCopy copyRegion = {
        .bufferOffset = 0U,
        .bufferRowLength = 0U,
        .bufferImageHeight = 0U,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0U,
            .baseArrayLayer = 0U,
            .layerCount = 1U
        },
        .imageOffset = { 0U, 0U, 0U },
        .imageExtent = { (uint32_t)width, (uint32_t)height, 1U }
    };

    vkCmdCopyBufferToImage(commandBuffer, srcHostBuffer, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    const VkImageMemoryBarrier imageCopyBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = queueFamilyIndex,
        .dstQueueFamilyIndex = queueFamilyIndex,
        .image = dstImage,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0U,
            .levelCount = 1U,
            .baseArrayLayer = 0U,
            .layerCount = 1U
        }
    };
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
        0, NULL, 0, NULL, 1, &imageCopyBarrier);
}

static void SyncAndReadBuffer(VkCommandBuffer commandBuffer, uint32_t queueFamilyIndex, VkBuffer dstHostBuffer, VkBuffer srcDeviceBuffer, size_t size)
{
    const VkBufferCopy copyRegion = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size
    };
    vkCmdCopyBuffer(commandBuffer, srcDeviceBuffer, dstHostBuffer, 1, &copyRegion);

    const VkBufferMemoryBarrier bufferBarrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .srcQueueFamilyIndex = queueFamilyIndex,
        .dstQueueFamilyIndex = queueFamilyIndex,
        .buffer = srcDeviceBuffer,
        .offset = 0,
        .size = VK_WHOLE_SIZE
    };

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
        0, NULL, 1, &bufferBarrier, 0, NULL);
}

static VkResult CreateShaderModule(VkDevice device, const char *fileName, VkShaderModule *pShaderModule)
{
    FILE* fp = fopen(fileName, "r" FILE_BINARY_MODE);
    if (fp == NULL)
    {
        printf("Shader file %s not found!\n", fileName);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    fseek(fp, 0, SEEK_END);
    size_t fileLen = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    uint32_t* codeBuffer = malloc(fileLen);
    if(codeBuffer != NULL)
        fread(codeBuffer, 1, fileLen, fp);
    fclose(fp);

    const VkShaderModuleCreateInfo moduleCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .codeSize = fileLen,
        .pCode = codeBuffer
    };

    VkResult res = vkCreateShaderModule(device, &moduleCreateInfo, NULL, pShaderModule);
    if (res != VK_SUCCESS) {
        printf("vkCreateShaderModule failed: %d\n", res);
    }

    free(codeBuffer);

    return res;
}

static VkResult CreateComputePipelineSimple(VkDevice device, VkShaderModule computeShaderModule, VkPipeline *pComputePipeline,
    VkPipelineLayout *pPipelineLayout, VkDescriptorSetLayout *pDescLayout)
{
    const VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[2] = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, 0},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, 0}
    };
    const uint32_t bindingCount = (uint32_t)(sizeof(descriptorSetLayoutBindings) / sizeof(descriptorSetLayoutBindings[0]));
    const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        NULL, 0, bindingCount, descriptorSetLayoutBindings
    };

    VkResult res = vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, NULL, pDescLayout);
    if (res != VK_SUCCESS)
    {
        printf("vkCreateDescriptorSetLayout failed: %d\n", res);
        return res;
    }

    const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = pDescLayout,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = NULL
    };

    res = vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, NULL, pPipelineLayout);
    if (res != VK_SUCCESS)
    {
        printf("vkCreatePipelineLayout failed: %d\n", res);
        return res;
    }

    const VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = computeShaderModule,
        .pName = "main",
        .pSpecializationInfo = NULL
    };

    const VkComputePipelineCreateInfo computePipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stage = shaderStageCreateInfo,
        .layout = *pPipelineLayout,
        .basePipelineHandle = NULL,
        .basePipelineIndex = 0
    };
    res = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, NULL, pComputePipeline);
    if (res != VK_SUCCESS) {
        printf("vkCreateComputePipelines failed: %d\n", res);
    }

    return res;
}

static VkResult CreateComputePipelineAdvanced(VkDevice device, VkShaderModule computeShaderModule, int sharedMemorySize, VkPipeline* pComputePipeline,
    VkPipelineLayout* pPipelineLayout, VkDescriptorSetLayout* pDescLayout)
{
    const VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[2] = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, 0},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, 0}
    };
    const uint32_t bindingCount = (uint32_t)(sizeof(descriptorSetLayoutBindings) / sizeof(descriptorSetLayoutBindings[0]));
    const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        NULL, 0, bindingCount, descriptorSetLayoutBindings
    };

    VkResult res = vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, NULL, pDescLayout);
    if (res != VK_SUCCESS)
    {
        printf("vkCreateDescriptorSetLayout failed: %d\n", res);
        return res;
    }

    const VkPushConstantRange pushConstRange = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = sizeof(int)
    };

    const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = pDescLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstRange
    };

    res = vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, NULL, pPipelineLayout);
    if (res != VK_SUCCESS)
    {
        printf("vkCreatePipelineLayout failed: %d\n", res);
        return res;
    }

    const VkSpecializationMapEntry mapEntry = {
        .constantID = 0,
        .offset = 0,
        .size = sizeof(sharedMemorySize)
    };
    
    const VkSpecializationInfo specializationInfo = {
        .mapEntryCount = 1,
        .pMapEntries = &mapEntry,
        .dataSize = mapEntry.size,
        .pData = &sharedMemorySize
    };

    const VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = computeShaderModule,
        .pName = "main",
        .pSpecializationInfo = &specializationInfo
    };

    const VkComputePipelineCreateInfo computePipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stage = shaderStageCreateInfo,
        .layout = *pPipelineLayout,
        .basePipelineHandle = NULL,
        .basePipelineIndex = 0
    };
    res = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, NULL, pComputePipeline);
    if (res != VK_SUCCESS) {
        printf("vkCreateComputePipelines failed: %d\n", res);
    }

    return res;
}

static VkResult CreateComputePipelineTexturing(VkDevice device, VkShaderModule computeShaderModule, VkPipeline* pComputePipeline,
    VkPipelineLayout* pPipelineLayout, VkDescriptorSetLayout * pDescLayout)
{
    const VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[2] = {
        // buffer
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = NULL
        },
        // sampler
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = NULL
        }
    }; 

    const uint32_t bindingCount = (uint32_t)(sizeof(descriptorSetLayoutBindings) / sizeof(descriptorSetLayoutBindings[0]));
    const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        NULL, 0, bindingCount, descriptorSetLayoutBindings
    };

    VkResult res = vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, NULL, pDescLayout);
    if (res != VK_SUCCESS)
    {
        printf("vkCreateDescriptorSetLayout failed: %d\n", res);
        return res;
    }

    const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = pDescLayout,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = NULL
    };

    res = vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, NULL, pPipelineLayout);
    if (res != VK_SUCCESS)
    {
        printf("vkCreatePipelineLayout failed: %d\n", res);
        return res;
    }

    const VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = computeShaderModule,
        .pName = "main",
        .pSpecializationInfo = NULL
    };

    const VkComputePipelineCreateInfo computePipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stage = shaderStageCreateInfo,
        .layout = *pPipelineLayout,
        .basePipelineHandle = NULL,
        .basePipelineIndex = 0
    };
    res = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, NULL, pComputePipeline);
    if (res != VK_SUCCESS) {
        printf("vkCreateComputePipelines failed: %d\n", res);
    }

    return res;
}

static VkResult CreateDescriptorSets(VkDevice device, VkBuffer deviceBuffers[2], VkDescriptorSetLayout descLayout,
    VkDescriptorPool *pDescriptorPool, VkDescriptorSet *pDescSets)
{
    const VkDescriptorPoolCreateInfo descriptorPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .maxSets = 2,
        .poolSizeCount = 1,
        .pPoolSizes = (VkDescriptorPoolSize[]) { 
            {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 2} 
        }
    };
    VkResult res = vkCreateDescriptorPool(device, &descriptorPoolInfo, NULL, pDescriptorPool);
    if (res != VK_SUCCESS)
    {
        printf("vkCreateDescriptorPool failed: %d\n", res);
        return res;
    }

    const VkDescriptorSetAllocateInfo descAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = NULL,
        .descriptorPool = *pDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &descLayout
    };
    res = vkAllocateDescriptorSets(device, &descAllocInfo, pDescSets);
    if (res != VK_SUCCESS)
    {
        printf("vkAllocateDescriptorSets failed: %d\n", res);
        return res;
    }

    const VkDescriptorBufferInfo dstDescBufferInfo = {
        .buffer = deviceBuffers[0],
        .offset = 0,
        .range = VK_WHOLE_SIZE
    };

    const VkDescriptorBufferInfo srcDescBufferInfo = {
        .buffer = deviceBuffers[1],
        .offset = 0,
        .range = VK_WHOLE_SIZE
    };

    const VkWriteDescriptorSet writeDescSet = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = NULL,
        .dstSet = *pDescSets,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 2,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pImageInfo = NULL,
        .pBufferInfo = (VkDescriptorBufferInfo[]) { dstDescBufferInfo, srcDescBufferInfo },
        .pTexelBufferView = NULL
    };

    vkUpdateDescriptorSets(device, 1, &writeDescSet, 0, NULL);

    return res;
}

static VkResult CreateDescriptorSetsForTexturing(VkDevice device, VkBuffer dstBuffer, VkImageView srcImageView, VkSampler sampler, 
    VkDescriptorSetLayout descLayout, VkDescriptorPool* pDescriptorPool, VkDescriptorSet* pDescSets)
{
    const VkDescriptorPoolCreateInfo descriptorPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .maxSets = 2,
        .poolSizeCount = 2,
        .pPoolSizes = (VkDescriptorPoolSize[]) {
            {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1},
            {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1}
        }
    };
    VkResult res = vkCreateDescriptorPool(device, &descriptorPoolInfo, NULL, pDescriptorPool);
    if (res != VK_SUCCESS)
    {
        printf("vkCreateDescriptorPool failed: %d\n", res);
        return res;
    }

    const VkDescriptorSetAllocateInfo descAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = NULL,
        .descriptorPool = *pDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &descLayout
    };
    res = vkAllocateDescriptorSets(device, &descAllocInfo, pDescSets);
    if (res != VK_SUCCESS)
    {
        printf("vkAllocateDescriptorSets failed: %d\n", res);
        return res;
    }

    const VkDescriptorBufferInfo dstDescBufferInfo = {
        .buffer = dstBuffer,
        .offset = 0,
        .range = VK_WHOLE_SIZE
    };

    const VkDescriptorImageInfo srcImageInfo = {
        .sampler = sampler,
        .imageView = srcImageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    const VkWriteDescriptorSet writeDescSets[2] = {
        // dstBuffer write descriptor set
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = NULL,
            .dstSet = *pDescSets,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo = NULL,
            .pBufferInfo = &dstDescBufferInfo,
            .pTexelBufferView = NULL
        },
        // srcImageView write descriptor set
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = NULL,
            .dstSet = *pDescSets,
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &srcImageInfo,
            .pBufferInfo = NULL,
            .pTexelBufferView = NULL
        }
    };

    vkUpdateDescriptorSets(device, 2, writeDescSets, 0, NULL);

    return res;
}

static VkResult InitializeInstanceAndeDevice(void)
{
    VkResult result = InitializeInstance();
    if (result != VK_SUCCESS)
    {
        puts("InitializeInstance failed!");
        return result;
    }

    result = InitializeDevice(VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT, &s_memoryProperties);
    if (result != VK_SUCCESS) {
        puts("InitializeDevice failed!");
    }

    return result;
}

static void DestroyInstanceAndDevice(void)
{
    if (s_specDevice != NULL) {
        vkDestroyDevice(s_specDevice, NULL);
    }
    if (s_instance != NULL) {
        vkDestroyInstance(s_instance, NULL);
    }
}

static void SimpleComputeTest(void)
{
    puts("\n================ Begin simple compute test ================\n");

    VkDeviceMemory deviceMemories[2] = { NULL };
    // deviceBuffers[0] as host temporal buffer, deviceBuffers[1] as device dst buffer, deviceBuffers[2] as device src buffer
    VkBuffer deviceBuffers[3] = { NULL };
    VkShaderModule computeShaderModule = NULL;
    VkPipeline computePipeline = NULL;
    VkDescriptorSetLayout descriptorSetLayout = NULL;
    VkPipelineLayout pipelineLayout = NULL;
    VkDescriptorPool descriptorPool = NULL;
    VkCommandPool commandPool = NULL;
    VkCommandBuffer commandBuffers[1] = { NULL };
    uint32_t const commandBufferCount = (uint32_t)(sizeof(commandBuffers) / sizeof(commandBuffers[0]));

    do
    {
        const uint32_t elemCount = 100 * 1024 * 1024;
        const VkDeviceSize bufferSize = elemCount * sizeof(int);

        VkResult result = AllocateMemoryAndBuffers(s_specDevice, &s_memoryProperties, deviceMemories, deviceBuffers, bufferSize, s_specQueueFamilyIndex);
        if (result != VK_SUCCESS)
        {
            puts("AllocateMemoryAndBuffers failed!");
            break;
        }

        result = CreateShaderModule(s_specDevice, "simpleKernel.spv", &computeShaderModule);
        if (result != VK_SUCCESS)
        {
            puts("CreateShaderModule failed!");
            break;
        }

        result = CreateComputePipelineSimple(s_specDevice, computeShaderModule, &computePipeline, &pipelineLayout, &descriptorSetLayout);
        if (result != VK_SUCCESS)
        {
            puts("CreateComputePipeline failed!");
            break;
        }

        // There's no need to destroy `descriptorSet`, since VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT flag is not set
        // in `flags` in `VkDescriptorPoolCreateInfo`
        VkDescriptorSet descriptorSet = NULL;
        result = CreateDescriptorSets(s_specDevice, &deviceBuffers[1], descriptorSetLayout, &descriptorPool, &descriptorSet);
        if (result != VK_SUCCESS)
        {
            puts("CreateDescriptorSets failed!");
            break;
        }

        result = InitializeCommandBuffer(s_specQueueFamilyIndex, s_specDevice, &commandPool, commandBuffers, commandBufferCount);
        if (result != VK_SUCCESS)
        {
            puts("InitializeCommandBuffer failed!");
            break;
        }

        VkQueue queue = NULL;
        vkGetDeviceQueue(s_specDevice, s_specQueueFamilyIndex, 0, &queue);

        const VkCommandBufferBeginInfo cmdBufBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = NULL,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = NULL
        };
        result = vkBeginCommandBuffer(commandBuffers[0], &cmdBufBeginInfo);
        if (result != VK_SUCCESS)
        {
            printf("vkBeginCommandBuffer failed: %d\n", result);
            break;
        }

        vkCmdBindPipeline(commandBuffers[0], VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
        vkCmdBindDescriptorSets(commandBuffers[0], VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);

        WriteBufferAndSync(commandBuffers[0], s_specQueueFamilyIndex, deviceBuffers[2], deviceBuffers[0], bufferSize);

        vkCmdDispatch(commandBuffers[0], elemCount / 1024, 1, 1);

        SyncAndReadBuffer(commandBuffers[0], s_specQueueFamilyIndex, deviceBuffers[0], deviceBuffers[1], bufferSize);

        result = vkEndCommandBuffer(commandBuffers[0]);
        if (result != VK_SUCCESS)
        {
            printf("vkEndCommandBuffer failed: %d\n", result);
            break;
        }

        const VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = NULL,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = NULL,
            .pWaitDstStageMask = NULL,
            .commandBufferCount = commandBufferCount,
            .pCommandBuffers = commandBuffers,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = NULL
        };

        result = vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
        if (result != VK_SUCCESS)
        {
            printf("vkQueueSubmit failed: %d\n", result);
            break;
        }

        result = vkQueueWaitIdle(queue);
        if (result != VK_SUCCESS)
        {
            printf("vkQueueWaitIdle failed: %d\n", result);
            break;
        }

        // Verify the result
        void* hostBuffer = NULL;
        result = vkMapMemory(s_specDevice, deviceMemories[0], 0, bufferSize, 0, &hostBuffer);
        if (result != VK_SUCCESS)
        {
            printf("vkMapMemory failed: %d\n", result);
            break;
        }
        int* dstMem = hostBuffer;
        for (int i = 0; i < (int)elemCount; i++)
        {
            if (dstMem[i] != i * 2)
            {
                printf("Result error @ %d, result is: %d\n", i, dstMem[i]);
                break;
            }
        }

        printf("The first 5 elements sum = %d\n", dstMem[0] + dstMem[1] + dstMem[2] + dstMem[3] + dstMem[4]);

        vkUnmapMemory(s_specDevice, deviceMemories[0]);

    } while (false);

    if (commandPool != NULL)
    {
        vkFreeCommandBuffers(s_specDevice, commandPool, commandBufferCount, commandBuffers);
        vkDestroyCommandPool(s_specDevice, commandPool, NULL);
    }
    if (descriptorPool != NULL) {
        vkDestroyDescriptorPool(s_specDevice, descriptorPool, NULL);
    }
    if (descriptorSetLayout != NULL) {
        vkDestroyDescriptorSetLayout(s_specDevice, descriptorSetLayout, NULL);
    }
    if (pipelineLayout != NULL) {
        vkDestroyPipelineLayout(s_specDevice, pipelineLayout, NULL);
    }
    if (computePipeline != NULL) {
        vkDestroyPipeline(s_specDevice, computePipeline, NULL);
    }
    if (computeShaderModule != NULL) {
        vkDestroyShaderModule(s_specDevice, computeShaderModule, NULL);
    }

    for (size_t i = 0; i < sizeof(deviceBuffers) / sizeof(deviceBuffers[0]); i++)
    {
        if (deviceBuffers[i] != NULL) {
            vkDestroyBuffer(s_specDevice, deviceBuffers[i], NULL);
        }
    }
    for (size_t i = 0; i < sizeof(deviceMemories) / sizeof(deviceMemories[0]); i++)
    {
        if (deviceMemories[i] != NULL) {
            vkFreeMemory(s_specDevice, deviceMemories[i], NULL);
        }
    }

    puts("\n================ Complete simple compute test ================\n");
}

static void AdvancedComputeTest(void)
{
    puts("================ Begin advanced compute test ================\n");

    VkDeviceMemory deviceMemories[2] = { NULL };
    // deviceBuffers[0] as host temporal buffer, deviceBuffers[1] as device dst buffer, deviceBuffers[2] as device src buffer
    VkBuffer deviceBuffers[3] = { NULL };
    VkShaderModule computeShaderModule = NULL;
    VkPipeline computePipeline = NULL;
    VkDescriptorSetLayout descriptorSetLayout = NULL;
    VkPipelineLayout pipelineLayout = NULL;
    VkDescriptorPool descriptorPool = NULL;
    VkCommandPool commandPool = NULL;
    VkCommandBuffer commandBuffers[1] = { NULL };
    uint32_t const commandBufferCount = (uint32_t)(sizeof(commandBuffers) / sizeof(commandBuffers[0]));

    if (s_instance == NULL || s_specDevice == NULL) {
        return;
    }

    do
    {
        const uint32_t elemCount = 1024;
        const VkDeviceSize bufferSize = elemCount * sizeof(int);

        VkResult result = AllocateMemoryAndBuffers(s_specDevice, &s_memoryProperties, deviceMemories, deviceBuffers, bufferSize, s_specQueueFamilyIndex);
        if (result != VK_SUCCESS)
        {
            puts("AllocateMemoryAndBuffers failed!");
            break;
        }

        result = CreateShaderModule(s_specDevice, "advancedKernel.spv", &computeShaderModule);
        if (result != VK_SUCCESS)
        {
            puts("CreateShaderModule failed!");
            break;
        }

        result = CreateComputePipelineAdvanced(s_specDevice, computeShaderModule, elemCount, &computePipeline, &pipelineLayout, &descriptorSetLayout);
        if (result != VK_SUCCESS)
        {
            puts("CreateComputePipeline failed!");
            break;
        }

        // There's no need to destroy `descriptorSet`, since VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT flag is not set
        // in `flags` in `VkDescriptorPoolCreateInfo`
        VkDescriptorSet descriptorSet = NULL;
        result = CreateDescriptorSets(s_specDevice, &deviceBuffers[1], descriptorSetLayout, &descriptorPool, &descriptorSet);
        if (result != VK_SUCCESS)
        {
            puts("CreateDescriptorSets failed!");
            break;
        }

        result = InitializeCommandBuffer(s_specQueueFamilyIndex, s_specDevice, &commandPool, commandBuffers, commandBufferCount);
        if (result != VK_SUCCESS)
        {
            puts("InitializeCommandBuffer failed!");
            break;
        }

        VkQueue queue = NULL;
        vkGetDeviceQueue(s_specDevice, s_specQueueFamilyIndex, 0, &queue);

        const VkCommandBufferBeginInfo cmdBufBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = NULL,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = NULL
        };
        result = vkBeginCommandBuffer(commandBuffers[0], &cmdBufBeginInfo);
        if (result != VK_SUCCESS)
        {
            printf("vkBeginCommandBuffer failed: %d\n", result);
            break;
        }

        vkCmdBindPipeline(commandBuffers[0], VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
        vkCmdBindDescriptorSets(commandBuffers[0], VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1,
            &descriptorSet, 0, NULL);

        const int constValue = 10;
        vkCmdPushConstants(commandBuffers[0], pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(constValue), &constValue);

        WriteBufferAndSync(commandBuffers[0], s_specQueueFamilyIndex, deviceBuffers[2], deviceBuffers[0], bufferSize);

        vkCmdDispatch(commandBuffers[0], elemCount / 1024, 1, 1);

        SyncAndReadBuffer(commandBuffers[0], s_specQueueFamilyIndex, deviceBuffers[0], deviceBuffers[1], bufferSize);

        result = vkEndCommandBuffer(commandBuffers[0]);
        if (result != VK_SUCCESS)
        {
            printf("vkEndCommandBuffer failed: %d\n", result);
            break;
        }

        const VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = NULL,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = NULL,
            .pWaitDstStageMask = NULL,
            .commandBufferCount = commandBufferCount,
            .pCommandBuffers = commandBuffers,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = NULL
        };

        result = vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
        if (result != VK_SUCCESS)
        {
            printf("vkQueueSubmit failed: %d\n", result);
            break;
        }

        result = vkQueueWaitIdle(queue);
        if (result != VK_SUCCESS)
        {
            printf("vkQueueWaitIdle failed: %d\n", result);
            break;
        }

        // Verify the result
        int sum = 0;
        for (int i = 0; i < (int)elemCount; i++)
            sum += i + constValue;
        void* hostBuffer = NULL;
        result = vkMapMemory(s_specDevice, deviceMemories[0], 0, bufferSize, 0, &hostBuffer);
        if (result != VK_SUCCESS)
        {
            printf("vkMapMemory failed: %d\n", result);
            break;
        }
        int* dstMem = hostBuffer;
        for (int i = 0; i < (int)elemCount; i++)
        {
            if (dstMem[i] != sum)
            {
                printf("Result error @ %d, result is: %d\n", i, dstMem[i]);
                break;
            }
        }

        printf("The first 5 elements sum = %d\n", dstMem[0] + dstMem[1] + dstMem[2] + dstMem[3] + dstMem[4]);

        vkUnmapMemory(s_specDevice, deviceMemories[0]);

    } while (false);

    if (commandPool != NULL)
    {
        vkFreeCommandBuffers(s_specDevice, commandPool, commandBufferCount, commandBuffers);
        vkDestroyCommandPool(s_specDevice, commandPool, NULL);
    }
    if (descriptorPool != NULL) {
        vkDestroyDescriptorPool(s_specDevice, descriptorPool, NULL);
    }
    if (descriptorSetLayout != NULL) {
        vkDestroyDescriptorSetLayout(s_specDevice, descriptorSetLayout, NULL);
    }
    if (pipelineLayout != NULL) {
        vkDestroyPipelineLayout(s_specDevice, pipelineLayout, NULL);
    }
    if (computePipeline != NULL) {
        vkDestroyPipeline(s_specDevice, computePipeline, NULL);
    }
    if (computeShaderModule != NULL) {
        vkDestroyShaderModule(s_specDevice, computeShaderModule, NULL);
    }

    for (size_t i = 0; i < sizeof(deviceBuffers) / sizeof(deviceBuffers[0]); i++)
    {
        if (deviceBuffers[i] != NULL) {
            vkDestroyBuffer(s_specDevice, deviceBuffers[i], NULL);
        }
    }
    for (size_t i = 0; i < sizeof(deviceMemories) / sizeof(deviceMemories[0]); i++)
    {
        if (deviceMemories[i] != NULL) {
            vkFreeMemory(s_specDevice, deviceMemories[i], NULL);
        }
    }

    puts("\n================ Complete advanced compute test ================\n");
}

static void TexturingComputeTest(void)
{
    puts("\n================ Begin texturing compute test ================\n");

    VkDeviceMemory deviceMemories[2] = { NULL };
    // deviceBuffers[0] as host temporal buffer, deviceBuffers[1] as device dst buffer
    VkBuffer deviceBuffers[2] = { NULL };
    VkImage images[1] = { NULL };
    VkImageView imageViews[1] = { NULL };
    VkSampler samplers[1] = { NULL };
    VkShaderModule computeShaderModule = NULL;
    VkPipeline computePipeline = NULL;
    VkDescriptorSetLayout descriptorSetLayout = NULL;
    VkPipelineLayout pipelineLayout = NULL;
    VkDescriptorPool descriptorPool = NULL;
    VkCommandPool commandPool = NULL;
    VkCommandBuffer commandBuffers[1] = { NULL };
    uint32_t const commandBufferCount = (uint32_t)(sizeof(commandBuffers) / sizeof(commandBuffers[0]));

    do
    {
        VkResult result = AllocateMemoryAndCreateImageWithSampler(s_specDevice, &s_memoryProperties, deviceMemories, deviceBuffers,
            TEST_IMAGE_WIDTH, TEST_IMAGE_HEIGHT, s_specQueueFamilyIndex, images, imageViews, samplers);
        if (result != VK_SUCCESS)
        {
            puts("AllocateMemoryAndBuffers failed!");
            break;
        }

        result = CreateShaderModule(s_specDevice, "texturingKernel.spv", &computeShaderModule);
        if (result != VK_SUCCESS)
        {
            puts("CreateShaderModule failed!");
            break;
        }

        result = CreateComputePipelineTexturing(s_specDevice, computeShaderModule, &computePipeline, &pipelineLayout, &descriptorSetLayout);
        if (result != VK_SUCCESS)
        {
            puts("CreateComputePipeline failed!");
            break;
        }

        // There's no need to destroy `descriptorSet`, since VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT flag is not set
        // in `flags` in `VkDescriptorPoolCreateInfo`
        VkDescriptorSet descriptorSet = NULL;
        result = CreateDescriptorSetsForTexturing(s_specDevice, deviceBuffers[1], imageViews[0], samplers[0], 
                                        descriptorSetLayout, &descriptorPool, &descriptorSet);
        if (result != VK_SUCCESS)
        {
            puts("CreateDescriptorSets failed!");
            break;
        }

        result = InitializeCommandBuffer(s_specQueueFamilyIndex, s_specDevice, &commandPool, commandBuffers, commandBufferCount);
        if (result != VK_SUCCESS)
        {
            puts("InitializeCommandBuffer failed!");
            break;
        }

        VkQueue queue = NULL;
        vkGetDeviceQueue(s_specDevice, s_specQueueFamilyIndex, 0, &queue);

        const VkCommandBufferBeginInfo cmdBufBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = NULL,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = NULL
        };
        result = vkBeginCommandBuffer(commandBuffers[0], &cmdBufBeginInfo);
        if (result != VK_SUCCESS)
        {
            printf("vkBeginCommandBuffer failed: %d\n", result);
            break;
        }

        vkCmdBindPipeline(commandBuffers[0], VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
        vkCmdBindDescriptorSets(commandBuffers[0], VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);

        const size_t bufferSize = TEST_IMAGE_WIDTH * TEST_IMAGE_HEIGHT * sizeof(uint32_t) * 4;

        CopyToImageBufferAndSync(commandBuffers[0], s_specQueueFamilyIndex, images[0], deviceBuffers[0], TEST_IMAGE_WIDTH, TEST_IMAGE_HEIGHT);

        vkCmdDispatch(commandBuffers[0], TEST_IMAGE_WIDTH / 16, TEST_IMAGE_HEIGHT / 16, 1);

        SyncAndReadBuffer(commandBuffers[0], s_specQueueFamilyIndex, deviceBuffers[0], deviceBuffers[1], bufferSize);

        result = vkEndCommandBuffer(commandBuffers[0]);
        if (result != VK_SUCCESS)
        {
            printf("vkEndCommandBuffer failed: %d\n", result);
            break;
        }

        const VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = NULL,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = NULL,
            .pWaitDstStageMask = NULL,
            .commandBufferCount = commandBufferCount,
            .pCommandBuffers = commandBuffers,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = NULL
        };

        result = vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
        if (result != VK_SUCCESS)
        {
            printf("vkQueueSubmit failed: %d\n", result);
            break;
        }

        result = vkQueueWaitIdle(queue);
        if (result != VK_SUCCESS)
        {
            printf("vkQueueWaitIdle failed: %d\n", result);
            break;
        }

        // Verify the result
        void* hostBuffer = NULL;
        result = vkMapMemory(s_specDevice, deviceMemories[0], 0, bufferSize, 0, &hostBuffer);
        if (result != VK_SUCCESS)
        {
            printf("vkMapMemory failed: %d\n", result);
            break;
        }
        uint32_t* dstMem = hostBuffer;
        const uint32_t rowElems = TEST_IMAGE_WIDTH * 4;
        uint32_t counter = 0;
        for (uint32_t row = 0; row < TEST_IMAGE_HEIGHT; ++row)
        {
            for (uint32_t col = 0; col < rowElems; ++col, ++counter)
            {
                if (dstMem[row * rowElems + col] != counter / 2)
                {
                    printf("Result error @col: %u, @row: %u, result is: %u\n",
                            col, row, dstMem[row * rowElems + col]);
                    break;
                }
            }
        }

        printf("The first 5 elements sum = %d\n", dstMem[0] + dstMem[1] + dstMem[2] + dstMem[3] + dstMem[4]);

        vkUnmapMemory(s_specDevice, deviceMemories[0]);

    } while (false);

    if (commandPool != NULL)
    {
        vkFreeCommandBuffers(s_specDevice, commandPool, commandBufferCount, commandBuffers);
        vkDestroyCommandPool(s_specDevice, commandPool, NULL);
    }
    if (descriptorPool != NULL) {
        vkDestroyDescriptorPool(s_specDevice, descriptorPool, NULL);
    }
    if (descriptorSetLayout != NULL) {
        vkDestroyDescriptorSetLayout(s_specDevice, descriptorSetLayout, NULL);
    }
    if (pipelineLayout != NULL) {
        vkDestroyPipelineLayout(s_specDevice, pipelineLayout, NULL);
    }
    if (computePipeline != NULL) {
        vkDestroyPipeline(s_specDevice, computePipeline, NULL);
    }
    if (computeShaderModule != NULL) {
        vkDestroyShaderModule(s_specDevice, computeShaderModule, NULL);
    }

    for (size_t i = 0; i < sizeof(deviceBuffers) / sizeof(deviceBuffers[0]); i++)
    {
        if (deviceBuffers[i] != NULL) {
            vkDestroyBuffer(s_specDevice, deviceBuffers[i], NULL);
        }
    }
    for (size_t i = 0; i < sizeof(images) / sizeof(images[0]); i++)
    {
        if (images[i] != NULL) {
            vkDestroyImage(s_specDevice, images[i], NULL);
        }
    }
    for (size_t i = 0; i < sizeof(imageViews) / sizeof(imageViews[0]); i++)
    {
        if (imageViews[i] != NULL) {
            vkDestroyImageView(s_specDevice, imageViews[i], NULL);
        }
    }
    for (size_t i = 0; i < sizeof(samplers) / sizeof(samplers[0]); i++)
    {
        if (samplers[i] != NULL) {
            vkDestroySampler(s_specDevice, samplers[i], NULL);
        }
    }
    for (size_t i = 0; i < sizeof(deviceMemories) / sizeof(deviceMemories[0]); i++)
    {
        if (deviceMemories[i] != NULL) {
            vkFreeMemory(s_specDevice, deviceMemories[i], NULL);
        }
    }

    puts("\n================ Complete texturing compute test ================\n");
}

int main(int argc, const char* argv[])
{
    if (InitializeInstanceAndeDevice() == VK_SUCCESS)
    {
        SimpleComputeTest();

        AdvancedComputeTest();

        TexturingComputeTest();
    }

    DestroyInstanceAndDevice();
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
