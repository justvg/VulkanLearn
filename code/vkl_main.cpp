#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>

#include <vector>

#include <cstring>

#define Assert(Expression) if(!(Expression)) { *(int *)0 = 0; }
#include "vkl_math.h"

struct optional_u32
{
    bool hasValue = false;
    uint32_t Val;

    bool has_value() { return hasValue; }
    uint32_t value() { Assert(hasValue); return Val; }

    void operator = (const uint32_t NewValue) 
    { 
        hasValue = true;
        Val = NewValue;
    }
};

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::vector<const char *> validationLayers = 
{
    "VK_LAYER_KHRONOS_validation"
};

#ifdef _DEBUG
const bool enableValidationLayers = true;
#else
const bool enableValidationLayers = false;
#endif

class HelloTriangleApplication
{
public:
    void run()
    {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow *window;

    VkInstance vkInstance;
    VkPhysicalDevice vkPhysicalDevice = VK_NULL_HANDLE;
    VkDevice vkLogicalDevice;
    VkQueue vkGraphicsQueue;

private:
    void initWindow()
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        glfwInit();

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    void initVulkan()
    {
        createInstance();
        pickPhysicalDevice();
        createLogicalDevice();
    }

    void mainLoop()
    {
        while(!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
        }
    }

    void cleanup()
    {
        vkDestroyDevice(vkLogicalDevice, nullptr);
        vkDestroyInstance(vkInstance, nullptr);

        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void createInstance()
    {
        if(enableValidationLayers && !checkValidationLayersSupport())
        {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        uint32_t extensionsCount;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionsCount, nullptr);
        std::vector<VkExtensionProperties> extensions(extensionsCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionsCount, extensions.data());
        std::cout << "available extensions:\n";
        for(const VkExtensionProperties &extension : extensions)
        {
            std::cout << '\t' << extension.extensionName << '\n';
        }

        uint32_t glfwExtensionsCount;
        const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionsCount);

        VkApplicationInfo appInfo;
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pNext = nullptr;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.flags = 0;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledLayerCount = enableValidationLayers ? static_cast<uint32_t>(validationLayers.size()) : 0;
        createInfo.ppEnabledLayerNames = enableValidationLayers ? validationLayers.data() : nullptr;
        createInfo.enabledExtensionCount = glfwExtensionsCount;
        createInfo.ppEnabledExtensionNames = glfwExtensions;

        if(vkCreateInstance(&createInfo, nullptr, &vkInstance) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to creaete instance!");
        }
    }

    void pickPhysicalDevice()
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(vkInstance, &deviceCount, nullptr);

        if(deviceCount == 0)
        {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(vkInstance, &deviceCount, devices.data());

        for(const VkPhysicalDevice &device : devices)
        {
            if(isDeviceSuitable(device))
            {
                vkPhysicalDevice = device;
                break;
            }
        }

        if(vkPhysicalDevice == VK_NULL_HANDLE)
        {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    void createLogicalDevice()
    {
        QueueFamilyIndices familyIndices = findQueueFamilies(vkPhysicalDevice);

        VkDeviceQueueCreateInfo queueCreateInfo;
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.pNext = nullptr;
        queueCreateInfo.flags = 0;
        queueCreateInfo.queueFamilyIndex = familyIndices.graphicsFamily.value();
        queueCreateInfo.queueCount = 1;
        float queuePriority = 1.0f;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkPhysicalDeviceFeatures deviceFeatures = {};

        VkDeviceCreateInfo deviceCreateInfo;
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = nullptr;
        deviceCreateInfo.flags = 0;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
        deviceCreateInfo.enabledLayerCount = enableValidationLayers ? static_cast<uint32_t>(validationLayers.size()) : 0;
        deviceCreateInfo.ppEnabledLayerNames = enableValidationLayers ? validationLayers.data() : nullptr;
        deviceCreateInfo.enabledExtensionCount = 0;
        deviceCreateInfo.ppEnabledExtensionNames = nullptr;
        deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

        if(vkCreateDevice(vkPhysicalDevice, &deviceCreateInfo, nullptr, &vkLogicalDevice) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create logical device!");
        }

        vkGetDeviceQueue(vkLogicalDevice, familyIndices.graphicsFamily.value(), 0, &vkGraphicsQueue);
    }

    bool isDeviceSuitable(VkPhysicalDevice device)
    {
        QueueFamilyIndices familyIndices = findQueueFamilies(device);

        return familyIndices.isComplete();
    }

    struct QueueFamilyIndices
    {
        optional_u32 graphicsFamily;

        bool isComplete()
        {
            return graphicsFamily.has_value();
        }
    };
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device)
    {
        QueueFamilyIndices familyIndices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        for(uint32_t I = 0; I < queueFamilies.size(); I++)
        {
            if(queueFamilies[I].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                familyIndices.graphicsFamily = I;
            }

            if(familyIndices.isComplete())
            {
                break;
            }
        }

        return familyIndices;
    }

    bool checkValidationLayersSupport()
    {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        bool AllFound = true;
        for(const char *layerName : validationLayers)
        {
            bool layerFound = false;
            
            for(const VkLayerProperties &layerProperties : availableLayers)
            {
                if(strcmp(layerName, layerProperties.layerName) == 0)
                {
                    layerFound = true;
                    break;
                }
            }

            if(!layerFound)
            {
                AllFound = false;
                break;
            }
        }

        return AllFound;
    }
};

int main()
{
    HelloTriangleApplication app;

    try 
    {
        app.run();
    }
    catch(const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}