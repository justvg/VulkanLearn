#include <iostream>
#include <assert.h>

#include <vector>
#include <algorithm>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#define VOLK_IMPLEMENTATION
#include <volk.h>

#define FAST_OBJ_IMPLEMENTATION
#include <fast_obj.h>
#include <meshoptimizer.h>

#include "vkl_math.h"

#define VK_CHECK(call) \
    { \
        VkResult result_ = call; \
        assert(result_ == VK_SUCCESS); \
    }

#ifndef ARRAYSIZE
#define ARRAYSIZE(array) (sizeof(array) / sizeof((array)[0]))
#endif

VkInstance createInstance()
{
    // TODO: In real Vulkan application you should probably check if 1.2 is available via vkEnumerateInstanceVersion
    VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo createInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    createInfo.pApplicationInfo = &appInfo;

#if _DEBUG
    const char* debugLayers[] =
    {
        "VK_LAYER_KHRONOS_validation"
    };

    createInfo.ppEnabledLayerNames = debugLayers;
    createInfo.enabledLayerCount = ARRAYSIZE(debugLayers);
#endif

    const char* extensions[] =
    {
        VK_KHR_SURFACE_EXTENSION_NAME,

#ifdef VK_USE_PLATFORM_WIN32_KHR
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
    };

    createInfo.ppEnabledExtensionNames = extensions;
    createInfo.enabledExtensionCount = ARRAYSIZE(extensions);

    VkInstance instance = 0;
    VK_CHECK(vkCreateInstance(&createInfo, 0, &instance));

    return instance;
}

VkBool32 debugReportCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object,
                             size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
{
    const char *type = (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) ? "ERROR" : 
                       (flags & (VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)) ? "WARNING" : "INFO";

    char message[4096];
    snprintf(message, ARRAYSIZE(message), "%s: %s\n\n", type, pMessage);
    
    printf("%s", message);

#ifdef _WIN32
    OutputDebugStringA(message);
#endif
    
    if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
        assert(!"Validation error encountered!");
    
    return VK_FALSE;
}

VkDebugReportCallbackEXT registerDebugCallback(VkInstance instance)
{
    VkDebugReportCallbackCreateInfoEXT createInfo = { VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT };
    createInfo.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT;
    createInfo.pfnCallback = debugReportCallback;

    VkDebugReportCallbackEXT callback = 0;
    VK_CHECK(vkCreateDebugReportCallbackEXT(instance, &createInfo, 0, &callback));

    return callback;
}

uint32_t getGraphicsFamilyIndex(VkPhysicalDevice physicalDevice)
{
    uint32_t queueFamilyPropertyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertyCount, 0);

    std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertyCount, queueFamilyProperties.data());

    for (uint32_t i = 0; i < queueFamilyPropertyCount; i++)
        if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            return i;

    return VK_QUEUE_FAMILY_IGNORED;
}

bool supportsPresentation(VkPhysicalDevice physicalDevice, uint32_t familyIndex)
{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    return vkGetPhysicalDeviceWin32PresentationSupportKHR(physicalDevice, familyIndex);
#else
    return true;
#endif
}

VkPhysicalDevice pickPhysicalDevice(VkPhysicalDevice *physicalDevices, uint32_t physicalDeviceCount)
{
    VkPhysicalDevice discrete = 0;
    VkPhysicalDevice fallback = 0;

    for(uint32_t i = 0; i < physicalDeviceCount; i++)
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physicalDevices[i], &props);

        printf("GPU%d: %s\n", i, props.deviceName);

        uint32_t familyIndex = getGraphicsFamilyIndex(physicalDevices[i]);
        if (familyIndex == VK_QUEUE_FAMILY_IGNORED)
            continue;

        if (!supportsPresentation(physicalDevices[i], familyIndex))
            continue;

        if(!discrete && (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU))
        {
            discrete = physicalDevices[i];
        }

        if (!fallback)
        {
            fallback = physicalDevices[i];
        }
    }

    VkPhysicalDevice result = discrete ? discrete : fallback;
    if (result)
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(result, &props);
        printf("Selected GPU: %s\n", props.deviceName);
    }
    else
    {
        printf("ERROR: No GPUs found!\n");
    }

    return result;
}

VkDevice createDevice(VkPhysicalDevice physicalDevice, uint32_t familyIndex)
{
    float queuePriorities[] = { 1.0f };
    
    VkDeviceQueueCreateInfo queueInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    queueInfo.queueFamilyIndex = familyIndex;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = queuePriorities;

    const char* extensions[] = 
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME
    };

    VkDeviceCreateInfo createInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueInfo;
    createInfo.enabledExtensionCount = 1;
    createInfo.ppEnabledExtensionNames = extensions;
    createInfo.enabledExtensionCount= ARRAYSIZE(extensions);
    createInfo.ppEnabledExtensionNames = extensions;

    VkDevice device = 0;
    VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, 0, &device));

    return device;
}

VkSurfaceKHR createSurface(VkInstance instance, GLFWwindow* window)
{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    VkWin32SurfaceCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
    createInfo.hinstance = GetModuleHandle(0);
    createInfo.hwnd = glfwGetWin32Window(window);

    VkSurfaceKHR surface = 0;
    vkCreateWin32SurfaceKHR(instance, &createInfo, 0, &surface);
#else
#error Unsupported platform
#endif

    return surface;
}

VkFormat getSwapchainFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    uint32_t formatCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, 0));
    assert(formatCount > 0);

    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data()));

    if ((formatCount == 1) && (formats[0].format == VK_FORMAT_UNDEFINED))
        return VK_FORMAT_R8G8B8A8_UNORM;

    for (uint32_t i = 0; i < formatCount; i++)
        if ((formats[i].format == VK_FORMAT_R8G8B8A8_UNORM) || (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM))
            return formats[i].format;

    return formats[0].format;
}

VkSwapchainKHR createSwapchain(VkDevice device, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR surfaceCaps, uint32_t familyIndex, VkFormat format, 
                               uint32_t width, uint32_t height, VkSwapchainKHR oldSwapchain = 0)
{
    VkCompositeAlphaFlagBitsKHR surfaceComposite = (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR :
                                                   (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) ? VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR :
                                                   (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) ? VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR :
                                                    VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;

    VkSwapchainCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    createInfo.surface = surface;
    createInfo.minImageCount = std::max(2u, surfaceCaps.minImageCount);
    createInfo.imageFormat = format; 
    createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    createInfo.imageExtent.width = width;
    createInfo.imageExtent.height = height;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.queueFamilyIndexCount = 1;
    createInfo.pQueueFamilyIndices = &familyIndex;
    createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    createInfo.compositeAlpha = surfaceComposite;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    createInfo.oldSwapchain = oldSwapchain;

    VkSwapchainKHR swapchain = 0;
    VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, 0, &swapchain));

    return swapchain;
}

VkSemaphore createSemaphore(VkDevice device)
{
    VkSemaphoreCreateInfo createInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkSemaphore semaphore = 0;
    VK_CHECK(vkCreateSemaphore(device, &createInfo, 0, &semaphore));

    return semaphore;
}

VkCommandPool createCommandPool(VkDevice device, uint32_t familyIndex)
{
    VkCommandPoolCreateInfo createInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    createInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    createInfo.queueFamilyIndex = familyIndex;

    VkCommandPool commandPool = 0;
    VK_CHECK(vkCreateCommandPool(device, &createInfo, 0, &commandPool));

    return commandPool;
}

VkRenderPass createRenderPass(VkDevice device, VkFormat format)
{
    VkAttachmentDescription attachments[1] = {};
    attachments[0].format = format;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachments = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; 
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachments;

    VkRenderPassCreateInfo createInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    createInfo.attachmentCount = ARRAYSIZE(attachments);
    createInfo.pAttachments = attachments;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;

    VkRenderPass renderPass = 0;
    VK_CHECK(vkCreateRenderPass(device, &createInfo, 0, &renderPass));

    return renderPass;
}

VkImageView createImageView(VkDevice device, VkImage image, VkFormat format)
{
    VkImageViewCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    createInfo.image = image;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = format;
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.layerCount = 1;

    VkImageView view = 0;
    VK_CHECK(vkCreateImageView(device, &createInfo, 0, &view));

    return view;
}

VkFramebuffer createFramebuffer(VkDevice device, VkRenderPass renderPass, VkImageView imageView, uint32_t width, uint32_t height)
{
    VkFramebufferCreateInfo createInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    createInfo.renderPass = renderPass;
    createInfo.attachmentCount = 1;
    createInfo.pAttachments = &imageView;
    createInfo.width = width;
    createInfo.height = height;
    createInfo.layers = 1;

    VkFramebuffer framebuffer = 0;
    VK_CHECK(vkCreateFramebuffer(device, &createInfo, 0, &framebuffer));

    return framebuffer;
}

VkShaderModule loadShader(VkDevice device, const char *path)
{
    FILE *file = fopen(path, "rb");
    assert(file);

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = new char[length];
    assert(buffer);

    size_t rc = fread(buffer, 1, length, file);
    assert(rc == size_t(length)); 
    fclose(file);

    assert(length % 4 == 0);

    VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    createInfo.codeSize = length;
    createInfo.pCode = reinterpret_cast<const uint32_t *>(buffer);

    VkShaderModule shaderModule = 0;
    VK_CHECK(vkCreateShaderModule(device, &createInfo, 0, &shaderModule));

    return shaderModule;
}

VkPipelineLayout createPipelineLayout(VkDevice device)
{
    VkDescriptorSetLayoutBinding setBindings[1] = {};
    setBindings[0].binding = 0;
    setBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    setBindings[0].descriptorCount = 1;
    setBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayout setLayout = 0;
    VkDescriptorSetLayoutCreateInfo setCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    setCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    setCreateInfo.bindingCount = ARRAYSIZE(setBindings);
    setCreateInfo.pBindings = setBindings;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &setCreateInfo, 0, &setLayout));

    VkPipelineLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    createInfo.setLayoutCount = 1;
    createInfo.pSetLayouts = &setLayout;

    VkPipelineLayout layout = 0;
    VK_CHECK(vkCreatePipelineLayout(device, &createInfo, 0, &layout));

    // TODO: is this safe? NOTE: IT IS NOT
    // vkDestroyDescriptorSetLayout(device, setLayout, 0);

    return layout;
}

VkPipeline createGraphicsPipeline(VkDevice device, VkPipelineCache pipelineCache, VkRenderPass renderPass, VkShaderModule vs, VkShaderModule fs, VkPipelineLayout layout)
{
    VkGraphicsPipelineCreateInfo createInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vs;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fs;
    stages[1].pName = "main";

    createInfo.stageCount = ARRAYSIZE(stages);
    createInfo.pStages = stages;

    VkPipelineVertexInputStateCreateInfo vertexInput = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    createInfo.pVertexInputState = &vertexInput;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    createInfo.pInputAssemblyState = &inputAssembly;

    VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;
    createInfo.pViewportState = &viewportState;

    VkPipelineRasterizationStateCreateInfo rasterizationState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rasterizationState.lineWidth = 1.0f;
    createInfo.pRasterizationState = &rasterizationState;

    VkPipelineMultisampleStateCreateInfo multisampleState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.pMultisampleState = &multisampleState;

    VkPipelineDepthStencilStateCreateInfo depthStencilTest = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    createInfo.pDepthStencilState = &depthStencilTest;

    VkPipelineColorBlendAttachmentState colorAttachementState = {};
    colorAttachementState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    colorBlendState.attachmentCount = 1;
    colorBlendState.pAttachments = &colorAttachementState;
    createInfo.pColorBlendState = &colorBlendState;

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamicState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamicState.dynamicStateCount = ARRAYSIZE(dynamicStates);
    dynamicState.pDynamicStates = dynamicStates;
    createInfo.pDynamicState = &dynamicState;

    createInfo.layout = layout;
    createInfo.renderPass = renderPass;
    
    VkPipeline pipeline = 0;
    VK_CHECK(vkCreateGraphicsPipelines(device, pipelineCache, 1, &createInfo, 0, &pipeline));

    return pipeline;
}

VkImageMemoryBarrier imageBarrier(VkImage image, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkImageMemoryBarrier result = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };

    result.srcAccessMask = srcAccessMask;
    result.dstAccessMask = dstAccessMask;
    result.oldLayout = oldLayout;
    result.newLayout = newLayout;
    result.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    result.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    result.image = image;
    result.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    result.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    result.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    return result;
}

struct Swapchain
{
    VkSwapchainKHR swapchain;

    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
    std::vector<VkFramebuffer> framebuffers;

    uint32_t width, height;
    uint32_t imageCount;
};

void createSwapchain(Swapchain &result, VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface,
                     uint32_t familyIndex, VkFormat format, VkRenderPass renderPass, VkSwapchainKHR oldSwapchain = 0)
{
    VkSurfaceCapabilitiesKHR surfaceCaps;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCaps));

    uint32_t width = surfaceCaps.currentExtent.width;
    uint32_t height = surfaceCaps.currentExtent.height;

    VkSwapchainKHR swapchain = createSwapchain(device, surface, surfaceCaps, familyIndex, format, width, height, oldSwapchain);
    assert(swapchain);
    
    uint32_t imageCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, 0));

    std::vector<VkImage> images(imageCount);
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images.data()));

    std::vector<VkImageView> imageViews(imageCount);
    for (uint32_t i = 0; i < imageCount; i++)
    {
        imageViews[i] = createImageView(device, images[i], format);
        assert(imageViews[i]);
    }

    std::vector<VkFramebuffer> framebuffers(imageCount);
    for (uint32_t i = 0; i < imageCount; i++)
    {
        framebuffers[i] = createFramebuffer(device, renderPass, imageViews[i], width, height);
        assert(framebuffers[i]);
    }

    result.swapchain = swapchain;

    result.images = images;
    result.imageViews = imageViews;
    result.framebuffers = framebuffers;

    result.width = width; 
    result.height = height;
    result.imageCount = imageCount;
}

void destroySwapchain(VkDevice device, const Swapchain& swapchain)
{
    for (uint32_t i = 0; i < swapchain.imageCount; i++)
        vkDestroyFramebuffer(device, swapchain.framebuffers[i], 0);

    for (uint32_t i = 0; i < swapchain.imageCount; i++)
        vkDestroyImageView(device, swapchain.imageViews[i], 0);

    vkDestroySwapchainKHR(device, swapchain.swapchain, 0);
}

void resizeSwapchainIfNecessary(Swapchain& result, VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface,
                                uint32_t familyIndex, VkFormat format, VkRenderPass renderPass)
{
    VkSurfaceCapabilitiesKHR surfaceCaps;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCaps));

    uint32_t newWidth = surfaceCaps.currentExtent.width;
    uint32_t newHeight = surfaceCaps.currentExtent.height;

    if((newWidth == result.width) && (newHeight == result.height))
        return;

    Swapchain old = result;

    createSwapchain(result, physicalDevice, device, surface, familyIndex, format, renderPass, old.swapchain);

    VK_CHECK(vkDeviceWaitIdle(device));

    destroySwapchain(device, old);
}

struct Vertex
{
    float vx, vy, vz;
    float nx, ny, nz;
    float tu, tv;
};

struct Mesh
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

bool loadMesh(Mesh& result, const char* path)
{
    fastObjMesh* file = fast_obj_read(path);
    if (!file)
        return false;

    size_t index_count = 0;
    for (uint32_t i = 0; i < file->face_count; i++)
    {
        assert((file->face_vertices[i] == 3) || ((file->face_vertices[i] == 4)));
        index_count += 3*(file->face_vertices[i] - 2);
    }

    size_t vertex_offset = 0;
    size_t index_offset = 0;
    std::vector<Vertex> vertices(index_count);
    for (uint32_t i = 0; i < file->face_count; i++)
    {
        for(uint32_t j = 0; j < file->face_vertices[i]; j++)
        {
            if (j >= 3)
            {
                vertices[vertex_offset] = vertices[vertex_offset - 3];
                vertices[vertex_offset + 1] = vertices[vertex_offset - 1];
                vertex_offset += 2;
            }

            Vertex& vertex = vertices[vertex_offset++];

            int vIndex = file->indices[index_offset].p;
            int nIndex = file->indices[index_offset].n;
            int tIndex = file->indices[index_offset].t;

            vertex.vx = file->positions[3 * vIndex];
            vertex.vy = file->positions[3 * vIndex + 1];
            vertex.vz = file->positions[3 * vIndex + 2];

            vertex.nx = nIndex < 0 ? 0.0f : file->normals[3 * nIndex];
            vertex.ny = nIndex < 0 ? 0.0f : file->normals[3 * nIndex + 1];
            vertex.nz = nIndex < 0 ? 1.0f : file->normals[3 * nIndex + 2];

            vertex.tu = tIndex < 0 ? 0.0f : file->texcoords[2 * tIndex];
            vertex.tv = tIndex < 0 ? 0.0f : file->texcoords[2 * tIndex + 1];
            
            index_offset++;
        }
    }

    assert(vertex_offset == index_count);

    if (1)
    {
        result.vertices = vertices;
        result.indices.resize(index_count);

        for (size_t i = 0; i < index_count; i++)
            result.indices[i] = (uint32_t)i;
    }
    else
    {
        std::vector<uint32_t> remap(index_count);
        size_t unique_vertices = meshopt_generateVertexRemap(remap.data(), 0, index_count, vertices.data(), index_count, sizeof(Vertex));

        result.vertices.resize(unique_vertices);
        result.indices.resize(index_count);

        meshopt_remapVertexBuffer(result.vertices.data(), vertices.data(), index_count, sizeof(Vertex), remap.data());
        meshopt_remapIndexBuffer(result.indices.data(), 0, index_count, remap.data());
    }

    fast_obj_destroy(file);
    
    return true;
}

struct Buffer
{
    VkBuffer buffer;
    VkDeviceMemory memory;
    void *data;
    size_t size;
};

uint32_t selectMemoryType(const VkPhysicalDeviceMemoryProperties& memoryProperties, uint32_t memoryTypeBits, VkMemoryPropertyFlags flags)
{
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
        if (((memoryTypeBits & (1 << i)) != 0) && ((memoryProperties.memoryTypes[i].propertyFlags & flags) == flags))
            return i;

    assert(!"No compatible memory type found!");
    return UINT32_MAX;
}

void createBuffer(Buffer &result, VkDevice device, const VkPhysicalDeviceMemoryProperties& memoryProperties, size_t size, VkBufferUsageFlags usage)
{
    VkBufferCreateInfo createInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    createInfo.size = size;
    createInfo.usage = usage;

    VkBuffer buffer = 0;
    VK_CHECK(vkCreateBuffer(device, &createInfo, 0, &buffer));

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);

    uint32_t memoryTypeIndex = selectMemoryType(memoryProperties, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    assert(memoryTypeIndex != UINT32_MAX);

    VkMemoryAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = memoryTypeIndex;

    VkDeviceMemory memory = 0;
    VK_CHECK(vkAllocateMemory(device, &allocateInfo, 0, &memory));

    VK_CHECK(vkBindBufferMemory(device, buffer, memory, 0));

    void* data = 0;
    VK_CHECK(vkMapMemory(device, memory, 0, memoryRequirements.size, 0, &data)); 

    result.buffer = buffer;
    result.memory = memory;
    result.size = memoryRequirements.size;
    result.data = data;
}

void destroyBuffer(const Buffer& buffer, VkDevice device)
{
    vkFreeMemory(device, buffer.memory, 0);
    vkDestroyBuffer(device, buffer.buffer, 0);
}

int main() 
{
    int rc = glfwInit();
    assert(rc);

    VK_CHECK(volkInitialize());

    VkInstance instance = createInstance();
    assert(instance);

    volkLoadInstance(instance);

#if _DEBUG
    VkDebugReportCallbackEXT debugCallback = registerDebugCallback(instance);
#endif

    VkPhysicalDevice physicalDevices[16];
    uint32_t physicalDeviceCount = ARRAYSIZE(physicalDevices);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices));

    VkPhysicalDevice physicalDevice = pickPhysicalDevice(physicalDevices, physicalDeviceCount);
    assert(physicalDevice);

    uint32_t familyIndex = getGraphicsFamilyIndex(physicalDevice);
    assert(familyIndex != VK_QUEUE_FAMILY_IGNORED);

    VkDevice device = createDevice(physicalDevice, familyIndex);
    assert(device);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(1024, 768, "vulkan learning", 0, 0);
    assert(window);

    VkSurfaceKHR surface = createSurface(instance, window);
    assert(surface);

    VkBool32 presentSupported = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, familyIndex, surface, &presentSupported));
    assert(presentSupported);

    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);

    VkFormat swapchainFormat = getSwapchainFormat(physicalDevice, surface);

    VkSemaphore acquireSemaphore = createSemaphore(device);
    assert(acquireSemaphore);

    VkSemaphore releaseSemaphore = createSemaphore(device);
    assert(releaseSemaphore);

    VkQueue queue = 0;
    vkGetDeviceQueue(device, familyIndex, 0, &queue);

    VkRenderPass renderPass = createRenderPass(device, swapchainFormat);
    assert(renderPass);

    VkShaderModule triangleVS = loadShader(device, "shaders_bytecode\\triangle.vert.spv");
    assert(triangleVS);
    VkShaderModule triangleFS = loadShader(device, "shaders_bytecode\\triangle.frag.spv");
    assert(triangleFS);

    // TODO: this is critical for performance!
    VkPipelineCache pipelineCache = 0;

    VkPipelineLayout triangleLayout = createPipelineLayout(device);
    assert(triangleLayout);

    VkPipeline trianglePipeline = createGraphicsPipeline(device, pipelineCache, renderPass, triangleVS, triangleFS, triangleLayout);
    assert(trianglePipeline);

    Swapchain swapchain;
    createSwapchain(swapchain, physicalDevice, device, surface, familyIndex, swapchainFormat, renderPass);
 
    VkCommandPool commandPool = createCommandPool(device, familyIndex);
    assert(commandPool);

    VkCommandBufferAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    allocateInfo.commandPool = commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = 0;
    VK_CHECK(vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer));

    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    Mesh mesh;
    bool rcm = loadMesh(mesh, "meshes\\kitten.obj");

    Buffer vb = {};
    createBuffer(vb, device, memoryProperties, 128 * 1024 * 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    Buffer ib = {};
    createBuffer(ib, device, memoryProperties, 128 * 1024 * 1024, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    assert(vb.size >= mesh.vertices.size() * sizeof(Vertex));
    memcpy(vb.data, mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));

    assert(vb.size >= mesh.indices.size() * sizeof(uint32_t));
    memcpy(ib.data, mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t));

    while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        resizeSwapchainIfNecessary(swapchain, physicalDevice, device, surface, familyIndex, swapchainFormat, renderPass);

        uint32_t imageIndex = 0;
        VK_CHECK(vkAcquireNextImageKHR(device, swapchain.swapchain, UINT64_MAX, acquireSemaphore, VK_NULL_HANDLE, &imageIndex));

        VK_CHECK(vkResetCommandPool(device, commandPool, 0));

        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
        
        VkImageMemoryBarrier renderBeginBarrier = imageBarrier(swapchain.images[imageIndex], 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &renderBeginBarrier);

        VkClearColorValue color = { 48.0f / 255.0f, 10.0f / 255.0f, 36.0f / 255.0f, 1 };
        VkClearValue clearColor = { color };

        VkRenderPassBeginInfo passBeginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        passBeginInfo.renderPass = renderPass;
        passBeginInfo.framebuffer = swapchain.framebuffers[imageIndex];
        passBeginInfo.renderArea.extent.width = swapchain.width;
        passBeginInfo.renderArea.extent.height = swapchain.height;
        passBeginInfo.clearValueCount = 1;
        passBeginInfo.pClearValues = &clearColor;
        vkCmdBeginRenderPass(commandBuffer, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport = { 0, float(swapchain.height), float(swapchain.width), -float(swapchain.height), 0, 1 };
        VkRect2D scissor = { {0, 0}, {swapchain.width, swapchain.height} };

        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);

        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = vb.buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = vb.size;

        VkWriteDescriptorSet descriptors[1] = {};
        descriptors[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptors[0].dstBinding = 0;
        descriptors[0].descriptorCount = 1;
        descriptors[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptors[0].pBufferInfo = &bufferInfo;
        
        vkCmdPushDescriptorSetKHR(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, triangleLayout, 0, 1, descriptors);
        
        vkCmdBindIndexBuffer(commandBuffer, ib.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, mesh.indices.size(), 1, 0, 0, 0);

        vkCmdEndRenderPass(commandBuffer);

        VkImageMemoryBarrier renderEndBarrier = imageBarrier(swapchain.images[imageIndex], VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &renderEndBarrier);


        VK_CHECK(vkEndCommandBuffer(commandBuffer));

        VkPipelineStageFlags submitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &acquireSemaphore;
        submitInfo.pWaitDstStageMask = &submitStageMask;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &releaseSemaphore;
        VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

        VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &releaseSemaphore;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain.swapchain;
        presentInfo.pImageIndices = &imageIndex;

        VK_CHECK(vkQueuePresentKHR(queue, &presentInfo));

        VK_CHECK(vkDeviceWaitIdle(device));
    }

    VK_CHECK(vkDeviceWaitIdle(device));

    destroyBuffer(vb, device);
    destroyBuffer(ib, device);

    vkDestroyCommandPool(device, commandPool, 0);
    
    destroySwapchain(device, swapchain);

    vkDestroyPipeline(device, trianglePipeline, 0);
    vkDestroyPipelineLayout(device, triangleLayout, 0);

    vkDestroyShaderModule(device, triangleFS, 0);
    vkDestroyShaderModule(device, triangleVS, 0);

    vkDestroyRenderPass(device, renderPass, 0);

    vkDestroySemaphore(device, releaseSemaphore, 0);
    vkDestroySemaphore(device, acquireSemaphore, 0);

    vkDestroySurfaceKHR(instance, surface, 0);

    glfwDestroyWindow(window);

    vkDestroyDevice(device, 0);

#if _DEBUG
    vkDestroyDebugReportCallbackEXT(instance, debugCallback, 0);
#endif

    vkDestroyInstance(instance, 0);

    return 0;
}