#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>

#include<vector>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;
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
        vkDestroy(&instance, nullptr);

        glfwDestroyWindow(window);

        glfwTerminate();
    }

    void createInstance()
    {
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
        createInfo.enabledLayerCount = 0;
        createInfo.ppEnabledLayerNames = nullptr;
        createInfo.enabledExtensionCount = glfwExtensionsCount;
        createInfo.ppEnabledExtensionNames = glfwExtensions;

        if(vkCreateInstance(&createInfo, nullptr, &vkInstance) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to creaete instance!");
        }
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