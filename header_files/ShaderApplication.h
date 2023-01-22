#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <set>
#include <algorithm>
#include <array>

#include "stb_image.h"

#include "Mesh.h"
#include "MeshModel.h"
#include <cstring>
#include <cstdlib>
#include "Utilities.h"

class ShaderApplication
{
private:
    GLFWwindow* window;
    int currentFrame = 0;

    //Scene Objects
    std::vector<MeshModel> modelList;


    // Scene Settings
    struct UboViewProjection {
        glm::mat4 projection; // How the cam views the world (depth, flat ect.)
        glm::mat4 view; // Where cam i viewing it from. What direction.
    } uboViewProjection;

    // Vulkan Components
    // - Main

    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

    struct {
        VkPhysicalDevice physicalDevice;
        VkDevice logicalDevice;
    } mainDevice;
    VkQueue graphicQueue;
    VkQueue presentationQueue;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;

    std::vector<SwapchainImage> swapchainImages;
    std::vector<VkFramebuffer> swapchainFramebuffers;
    std::vector<VkCommandBuffer> commandBuffers;

    std::vector<VkImage> colourBufferImage;
    std::vector<VkDeviceMemory> colourBufferImageMemory;
    std::vector<VkImageView> colourBufferImageView;

    std::vector<VkImage> depthBufferImage;
    std::vector<VkDeviceMemory> depthBufferImageMemory;
    std::vector<VkImageView> depthBufferImageView;

    VkSampler textureSampler;

    // - Descriptors
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorSetLayout samplerSetLayout;
    VkDescriptorSetLayout inputSetLayout;

    VkPushConstantRange pushConstantRange;


    VkDescriptorPool descriptorPool;
    VkDescriptorPool samplerDescriptorPool;
    VkDescriptorPool inputDescriptorPool;

    std::vector<VkDescriptorSet> descriptorSets;
    std::vector<VkDescriptorSet> samplerDescriptorSets;
    std::vector<VkDescriptorSet> inputDescriptorSets;


    std::vector<VkBuffer> vpUniformBuffer;
    std::vector<VkDeviceMemory> vpUniformBufferMemory;

    std::vector<VkBuffer> modelDynUniformBuffer;
    std::vector<VkDeviceMemory> modelDynUniformBufferMemory;

    //VkDeviceSize minUniformBufferOffset;
    //size_t modelUniformAlignment;

    //UboModel * modelTransferSpace;

    // - Assets
    std::vector<VkImage> textureImages;
    std::vector<VkDeviceMemory> textureImageMemory;
    std::vector<VkImageView> textureImageViews;

    // - Pipeline
    VkPipeline graphicsPipeline;
    VkPipelineLayout pipelineLayout;

    VkPipeline secondPipeline;
    VkPipelineLayout secondPipelineLayout;

    VkRenderPass renderPass;


    // - Pools
    VkCommandPool graphicsCommandPool;

    // - Utility
    VkFormat swapchainImageFormat;
    VkExtent2D swapchainExtent;


    std::vector<VkSemaphore> imageAvailable;
    std::vector<VkSemaphore> renderFinished;
    std::vector<VkFence> drawFences;

    void initWindow(std::string wName, const int width, const int height);
    int initVulkan();

    void updateModel(int modelID, glm::mat4 newModel);

    void draw();
    void mainLoop();
    void cleanup();

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    std::vector<const char*> getRequiredExtensions();
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, 
        VkDebugUtilsMessageTypeFlagsEXT messageType, 
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, 
        void* pUserData) {

        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
        return VK_FALSE;
    };


    // Vulkan Functions
    // - Create Functions
    void createInstance();
    void setupDebugMessenger();
    void createLogicalDevice();
    void createSurface();
    void createSwapChain();
    void createRenderPass();
    void createDescriptorSetLayout();
    void createPushConstantRange();
    void createGraphicsPipeline();
    void createColourBufferImage();
    void createDepthBufferImage();
    void createFramebuffers();
    void createCommandPool();
    void createCommandBuffers();
    void createSynchronisation();
    void createTextureSampler();

    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void createInputDescriptorSets();


    void updateUniformBuffers(uint32_t imageIndex);


    // - Record functions
    void recordCommands(uint32_t currentImage);


    // - Get functions
    void getPhysicalDevice();
    QueueFamilyIndices getQueueFamilies(VkPhysicalDevice device);
    SwapChainDetails getSwapChainDetails(VkPhysicalDevice device);

    // - Allocate Functions
    void allocateDynamicBufferTransferSpace();


    // - Support Functions
    // -- Checkers
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    bool checkValidationLayerSupport();
    bool checkDeviceSuitable(VkPhysicalDevice device);

    // -- Choose functions.
    VkSurfaceFormatKHR chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats);
    VkPresentModeKHR chooseBestPresentationMode(const std::vector<VkPresentModeKHR> presentationModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &surfaceCapabilities);
    VkFormat chooseSupportedFormat(const std::vector<VkFormat> &formats, VkImageTiling tiling, VkFormatFeatureFlags featureFlags);

    // -- Create funcitons
    VkImage createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags useFlags, 
        VkMemoryPropertyFlags propFlags, VkDeviceMemory *imageMemory);
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    VkShaderModule createShaderModule(const std::vector<char> &code);

    int createTextureImage(std::string fileName);
    int createTexture(std::string fileName);
    int createTextureDescriptor(VkImageView textureImage);

    // -- Loader function.
    stbi_uc * loadTextureFile(std::string fileName, int * width, int * height, VkDeviceSize * imageSize);


public:
    int createMeshModel(std::string modelFile);

    ShaderApplication();
    ~ShaderApplication();

    void run();
};

