#include "ShaderApplication.h"


const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

ShaderApplication::ShaderApplication() {

}

void ShaderApplication::run() {
    initWindow("Vulkan", 500, 500);
    initVulkan();
    mainLoop();
    cleanup();
}

ShaderApplication::~ShaderApplication() {

}

void ShaderApplication::initWindow(std::string wName = "Tmp", const int width = WIDTH, const int height = HEIGHT ) {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(width, height, wName.c_str(), nullptr, nullptr);
}

int ShaderApplication::initVulkan() {
    try {
        createInstance();
        setupDebugMessenger();
        createSurface();
        getPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createRenderPass();
        createDescriptorSetLayout();
        createPushConstantRange();
        createGraphicsPipeline();
        createColourBufferImage();
        createDepthBufferImage();
        createFramebuffers();
        createCommandPool();
        createCommandBuffers();
        createTextureSampler();
        //allocateDynamicBufferTransferSpace();
        createUniformBuffers();
        createDescriptorPool();
        createDescriptorSets();
        createInputDescriptorSets();
        createSynchronisation();


        uboViewProjection.projection = glm::perspective(glm::radians(45.0f), (float)swapchainExtent.width / (float)swapchainExtent.height, 0.1f, 100.0f);  //Angle, Aspect Ratio, near, far.
        uboViewProjection.view = glm::lookAt(glm::vec3(10.0f, 0.0f, 20.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)); // The eye (where cam is), center (target, what we are looking at), orientation (where up is).

        uboViewProjection.projection[1][1] *= -1;

        // Fallback texture.
        createTexture("RGB_1.1001.png");

    }
    catch (const std::runtime_error& e) {
        printf("ERROR: %s\n", e.what());
        return EXIT_FAILURE;
    }
    return 0;

}

void ShaderApplication::updateModel(int modelID, glm::mat4 newModel)
{
    if(modelID >= modelList.size()) return;

    modelList[modelID].setModel(newModel);

}

void ShaderApplication::draw()
{
    // 1. Get next available image to draw to and set something to signal when we`re finished with the image (a semaphore)
    vkWaitForFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());
    vkResetFences(mainDevice.logicalDevice, 1, &drawFences[currentFrame]);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(mainDevice.logicalDevice, swapchain, std::numeric_limits<uint64_t>::max(), imageAvailable[currentFrame], VK_NULL_HANDLE, &imageIndex);

    recordCommands(imageIndex);
    updateUniformBuffers(imageIndex);

    // 2. Submit our command buffer to the queue for execution. Mae sure it waits for the image to be signalled as available before drawing. Signals when it is finished rendering.
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &imageAvailable[currentFrame];
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount =1;
    submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &renderFinished[currentFrame];

    VkResult result = vkQueueSubmit(graphicQueue, 1, &submitInfo, drawFences[currentFrame]);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit commandbuffer to queue!");
    }

    // 3. Present image to screen when it has singaled finished rendering.
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinished[currentFrame];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(presentationQueue, &presentInfo);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present image!");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAME_DRAWS;

}

void ShaderApplication::mainLoop() {

    float angle = 0.0f;
    float deltaTime = 0.0f;
    float lastTime = 0.0f;


    createMeshModel("geo/Alfred_Retypology.obj");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        float now = glfwGetTime();
        deltaTime = now - lastTime;
        lastTime = now;

        angle += 50.0f * deltaTime;
        if (angle > 360.0f) { angle -= 360.0f;}


        draw();
    }
}

void ShaderApplication::cleanup() {

    vkDeviceWaitIdle(mainDevice.logicalDevice);

    //_aligned_free(modelTransferSpace);

    for (size_t i = 0; i < modelList.size(); i++)
    {
        modelList[i].destroyMeshModel();
    }

    vkDestroyDescriptorPool(mainDevice.logicalDevice, inputDescriptorPool, nullptr);

    vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, inputSetLayout, nullptr);

    vkDestroyDescriptorPool(mainDevice.logicalDevice, samplerDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, samplerSetLayout, nullptr);

    vkDestroySampler(mainDevice.logicalDevice, textureSampler, nullptr);

    for (size_t i = 0; i < textureImages.size(); i++)
    {
        vkDestroyImageView(mainDevice.logicalDevice, textureImageViews[i], nullptr);
        vkDestroyImage(mainDevice.logicalDevice, textureImages[i], nullptr);
        vkFreeMemory(mainDevice.logicalDevice, textureImageMemory[i], nullptr);
    }

    for (size_t i = 0; i < depthBufferImage.size(); i++)
    {
        vkDestroyImageView(mainDevice.logicalDevice, depthBufferImageView[i], nullptr);
        vkDestroyImage(mainDevice.logicalDevice, depthBufferImage[i], nullptr);
        vkFreeMemory(mainDevice.logicalDevice, depthBufferImageMemory[i], nullptr);
    }

    for (size_t i = 0; i < colourBufferImage.size(); i++)
    {
        vkDestroyImageView(mainDevice.logicalDevice, colourBufferImageView[i], nullptr);
        vkDestroyImage(mainDevice.logicalDevice, colourBufferImage[i], nullptr);
        vkFreeMemory(mainDevice.logicalDevice, colourBufferImageMemory[i], nullptr);
    }


    vkDestroyDescriptorPool(mainDevice.logicalDevice, descriptorPool, nullptr);

    vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, descriptorSetLayout, nullptr);
    for (size_t i = 0; i < swapchainImages.size(); i++)
    {
        vkDestroyBuffer(mainDevice.logicalDevice, vpUniformBuffer[i], nullptr);
        vkFreeMemory(mainDevice.logicalDevice, vpUniformBufferMemory[i], nullptr);
        //vkDestroyBuffer(mainDevice.logicalDevice, modelDynUniformBuffer[i], nullptr);
        //vkFreeMemory(mainDevice.logicalDevice, modelDynUniformBufferMemory[i], nullptr);
    }


    for (size_t i = 0; i < MAX_FRAME_DRAWS; i++) 
    {
        vkDestroySemaphore(mainDevice.logicalDevice, renderFinished[i], nullptr);
        vkDestroySemaphore(mainDevice.logicalDevice, imageAvailable[i], nullptr);
        vkDestroyFence(mainDevice.logicalDevice, drawFences[i], nullptr);
    }

    vkDestroyCommandPool(mainDevice.logicalDevice, graphicsCommandPool, nullptr);
    for (auto framebuffer : swapchainFramebuffers) {
        vkDestroyFramebuffer(mainDevice.logicalDevice, framebuffer, nullptr);
    }
    vkDestroyPipeline(mainDevice.logicalDevice, secondPipeline, nullptr);
    vkDestroyPipelineLayout(mainDevice.logicalDevice, secondPipelineLayout, nullptr);
    vkDestroyPipeline(mainDevice.logicalDevice, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(mainDevice.logicalDevice, pipelineLayout, nullptr);
    vkDestroyRenderPass(mainDevice.logicalDevice, renderPass, nullptr);

    for (auto image : swapchainImages) {
        vkDestroyImageView(mainDevice.logicalDevice, image.imageView, nullptr);
    }
    vkDestroySwapchainKHR(mainDevice.logicalDevice, swapchain, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyDevice(mainDevice.logicalDevice, nullptr);

    if (enableValidationLayers) {
        DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    }

    vkDestroyInstance(instance, nullptr);

    glfwDestroyWindow(window);

    glfwTerminate();
}

void ShaderApplication::createInstance() {
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("validation layers requested, but not available!");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Shader Project";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    }
    else {
        createInfo.enabledLayerCount = 0;

        createInfo.pNext = nullptr;
    }

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance!");
    }
}

void ShaderApplication::createLogicalDevice(){
    QueueFamilyIndices indicies = getQueueFamilies(mainDevice.physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<int> queueFamilyIndicies = {indicies.graphicsFamily, indicies.presentationFamily };

    for (int queueFamilyIndex : queueFamilyIndicies) {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
        queueCreateInfo.queueCount = 1; //Number of queues to create.
        float priority = 1.0f;
        queueCreateInfo.pQueuePriorities = &priority;

        queueCreateInfos.push_back(queueCreateInfo);
    }


    // Info to create logical device.
    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();


    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.samplerAnisotropy = VK_TRUE;                     //Enabling anisotropy

    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    VkResult result = vkCreateDevice(mainDevice.physicalDevice, &deviceCreateInfo, nullptr, &mainDevice.logicalDevice);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create a logical device!");
    }

    vkGetDeviceQueue(mainDevice.logicalDevice, indicies.graphicsFamily, 0, &graphicQueue);
    vkGetDeviceQueue(mainDevice.logicalDevice, indicies.presentationFamily, 0, &presentationQueue);
}

void ShaderApplication::createSurface(){
    VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &surface);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create a surface!");
    }
}

void ShaderApplication::createSwapChain(){
    SwapChainDetails swapChainDetails = getSwapChainDetails(mainDevice.physicalDevice);

    //1. Choose best surface fornat.
    VkSurfaceFormatKHR surfaceFormat = chooseBestSurfaceFormat(swapChainDetails.formats);

    //2. Choose best presentation mode
    VkPresentModeKHR presentMode = chooseBestPresentationMode(swapChainDetails.presentationModes);

    //3. choose swap chain image resolution
    VkExtent2D extent = chooseSwapExtent(swapChainDetails.surfaceCapabilities);

    uint32_t imageCount = swapChainDetails.surfaceCapabilities.minImageCount + 1;
    if (swapChainDetails.surfaceCapabilities.maxImageCount > 0 && swapChainDetails.surfaceCapabilities.maxImageCount < imageCount) {
        imageCount = swapChainDetails.surfaceCapabilities.maxImageCount;
    }


    VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
    swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainCreateInfo.surface = surface;
    swapChainCreateInfo.imageFormat = surfaceFormat.format;
    swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapChainCreateInfo.presentMode = presentMode;
    swapChainCreateInfo.imageExtent = extent;
    swapChainCreateInfo.minImageCount = imageCount;
    swapChainCreateInfo.imageArrayLayers = 1;
    swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapChainCreateInfo.preTransform = swapChainDetails.surfaceCapabilities.currentTransform;
    swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapChainCreateInfo.clipped = VK_TRUE;

    QueueFamilyIndices indicies = getQueueFamilies(mainDevice.physicalDevice);
    if (indicies.graphicsFamily != indicies.presentationFamily) {

        uint32_t queueFamilyIndicies[] = {
            (uint32_t)indicies.graphicsFamily,
            (uint32_t)indicies.presentationFamily
        };

        swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapChainCreateInfo.queueFamilyIndexCount = 2;
        swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndicies;
    }
    else {
        swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapChainCreateInfo.queueFamilyIndexCount = 0;
        swapChainCreateInfo.pQueueFamilyIndices = nullptr;
    }

    swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    // Create swapchain
    VkResult result = vkCreateSwapchainKHR(mainDevice.logicalDevice, &swapChainCreateInfo, nullptr, &swapchain);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create a Swapchain!");
    }

    swapchainImageFormat = surfaceFormat.format;
    swapchainExtent = extent;

    uint32_t swapchainImageCount;
    vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapchainImageCount, nullptr);
    std::vector<VkImage> images(swapchainImageCount);
    vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapchainImageCount, images.data());

    for (VkImage image : images) {
        SwapchainImage swapchainImage = {};
        swapchainImage.image = image;
        swapchainImage.imageView = createImageView(image, swapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

        swapchainImages.push_back(swapchainImage);
    }
}

void ShaderApplication::createRenderPass()
{
    // Array of our subpasses
    std::array<VkSubpassDescription, 2> subpasses{};


    // ATTACHMENTS
    //SUBPASS 1 ATTACHMENTS & REFERENCES (INPUT ATTACHMENTS)

    //Colour Attachment (Input)
    VkAttachmentDescription colourAttachment = {};
    colourAttachment.format = chooseSupportedFormat(
        { VK_FORMAT_R8G8B8A8_UNORM },
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    colourAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colourAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    //Depth Attachment (Input)
    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format = chooseSupportedFormat(
        { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Colour Attachment (Input) Reference
    VkAttachmentReference colourAttachmentReference = {};
    colourAttachmentReference.attachment = 1; //Swap chain images are in 0 as per createFrameBuffer()
    colourAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Depth Attachment (Input) Reference
    VkAttachmentReference depthAttachmentReference = {};
    depthAttachmentReference.attachment = 2;
    depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


    // Set up subpass 1
    subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[0].colorAttachmentCount = 1;
    subpasses[0].pColorAttachments = &colourAttachmentReference;
    subpasses[0].pDepthStencilAttachment = &depthAttachmentReference;




    //SUBPASS 2 ATTACHMENTS & REFERENCES
    //Swap Chain Colour attachment
    VkAttachmentDescription swapchainColourAttachment = {};
    swapchainColourAttachment.format = swapchainImageFormat;
    swapchainColourAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    swapchainColourAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    swapchainColourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    swapchainColourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    swapchainColourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    swapchainColourAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    swapchainColourAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Swap Chain Colour Attachment Reference
    VkAttachmentReference swapchainColourAttachmentReference = {};
    swapchainColourAttachmentReference.attachment = 0;
    swapchainColourAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;


    // References to attach,emts that subpass will take input from.
    std::array<VkAttachmentReference, 2> inputReferences;
    inputReferences[0].attachment = 1;
    inputReferences[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    inputReferences[1].attachment = 2;
    inputReferences[1].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Set up subpass 2
    subpasses[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpasses[1].colorAttachmentCount = 1;
    subpasses[1].pColorAttachments = &swapchainColourAttachmentReference;
    subpasses[1].inputAttachmentCount = static_cast<uint32_t>(inputReferences.size());
    subpasses[1].pInputAttachments = inputReferences.data();


    // SUBPASS DEPENDENCIES


    std::array<VkSubpassDependency, 3> subpassDependencies;
    subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;

    subpassDependencies[0].dstSubpass = 0;
    subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependencies[0].dependencyFlags = 0;

    // Subpass 1 layout
    subpassDependencies[1].srcSubpass = 0;
    subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependencies[1].dstSubpass = 1;
    subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    subpassDependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    subpassDependencies[1].dependencyFlags = 0;

    subpassDependencies[2].srcSubpass = 0;
    subpassDependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependencies[2].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subpassDependencies[2].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    subpassDependencies[2].dependencyFlags = 0;


    std::array<VkAttachmentDescription, 3> renderPassAttachments = { swapchainColourAttachment, colourAttachment, depthAttachment };

    VkRenderPassCreateInfo renderPassCreateInfo = {};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(renderPassAttachments.size());
    renderPassCreateInfo.pAttachments = renderPassAttachments.data();
    renderPassCreateInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
    renderPassCreateInfo.pSubpasses = subpasses.data();
    renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
    renderPassCreateInfo.pDependencies = subpassDependencies.data();

    VkResult result = vkCreateRenderPass(mainDevice.logicalDevice, &renderPassCreateInfo, nullptr, &renderPass);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create a render pass!");
    }

}

void ShaderApplication::createDescriptorSetLayout()
{
    // UNIFORM VALUES DESCRIPTOR SET LAYOUT
    //UboViewProjection binding info
    VkDescriptorSetLayoutBinding vpLayoutBinding = {};
    vpLayoutBinding.binding = 0;   //Binding in vert shader location for uniform.
    vpLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    vpLayoutBinding.descriptorCount = 1;
    vpLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    vpLayoutBinding.pImmutableSamplers = nullptr;  //For Textures. None at the moment.

    // Model binding info
    /*VkDescriptorSetLayoutBinding modelLayoutBinding = {};
    modelLayoutBinding.binding = 1;
    modelLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    modelLayoutBinding.descriptorCount = 1;
    modelLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    modelLayoutBinding.pImmutableSamplers = nullptr;*/

    std::vector<VkDescriptorSetLayoutBinding> layoutBindings = {vpLayoutBinding}; //{vpLayoutBinding, modelLayoutBinding}
    // Create Descriptor Set Layout with given bindings
    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCreateInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
    layoutCreateInfo.pBindings = layoutBindings.data();

    // Create descriptor set layout
    VkResult result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &layoutCreateInfo, nullptr, &descriptorSetLayout);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Descriptor Set Layout!");
    }

    // CREATE TEXTURE SAMPLER DESCRIPTOR SET LAYOUT

    // Texture binding info
    VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    samplerLayoutBinding.pImmutableSamplers = nullptr;

    // Create a descriptor set layout with given bindings for texture.
    VkDescriptorSetLayoutCreateInfo textureLayoutCreateInfo = {};
    textureLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    textureLayoutCreateInfo.bindingCount = 1;
    textureLayoutCreateInfo.pBindings = &samplerLayoutBinding;

    // Create descriptor set layout.
    result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &textureLayoutCreateInfo, nullptr, &samplerSetLayout);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a texture Descriptor Set Layout!");
    }

    //Create inout attachment image descriptor set layout.
    // COlour input bindin
    VkDescriptorSetLayoutBinding colourInputLayoutBinding = {};
    colourInputLayoutBinding.binding = 0;
    colourInputLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    colourInputLayoutBinding.descriptorCount = 1;
    colourInputLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding depthInputLayoutBinding = {};
    depthInputLayoutBinding.binding = 1;
    depthInputLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    depthInputLayoutBinding.descriptorCount = 1;
    depthInputLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    //Array of input agtatchment bindings
    std::vector<VkDescriptorSetLayoutBinding> inputBindings = { colourInputLayoutBinding, depthInputLayoutBinding };

    //Create a descriptorset layout for input attachments
    VkDescriptorSetLayoutCreateInfo inputLayoutCreateInfo = {};
    inputLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    inputLayoutCreateInfo.bindingCount = static_cast<uint32_t>(inputBindings.size());
    inputLayoutCreateInfo.pBindings = inputBindings.data();

    // Create descriptor set layout.
    result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &inputLayoutCreateInfo, nullptr, &inputSetLayout);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a texture Descriptor Set Layout!");
    }
}

void ShaderApplication::createPushConstantRange()
{
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(Model);
}

void ShaderApplication::createGraphicsPipeline()
{
    auto vertexShaderCode = readfile("Shaders/vert.spv");
    auto fragmentShaderCode = readfile("Shaders/frag.spv");

    // Create shader modules
    VkShaderModule vertexShaderModule = createShaderModule(vertexShaderCode);
    VkShaderModule fragmentShaderModule = createShaderModule(fragmentShaderCode);

    // SHADER STAGE CREATION INFORMATION
    // Vertex stage creation information
    VkPipelineShaderStageCreateInfo vertexShaderCreateInfo = {};
    vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexShaderCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexShaderCreateInfo.module = vertexShaderModule;
    vertexShaderCreateInfo.pName = "main";

    // Fragment stage creation information
    VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo = {};
    fragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentShaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentShaderCreateInfo.module = fragmentShaderModule;
    fragmentShaderCreateInfo.pName = "main";

    // SHader stage info into array.
    // Graphics pipeline creation info requires array of shader creates.
    VkPipelineShaderStageCreateInfo shaderStages[] = { vertexShaderCreateInfo, fragmentShaderCreateInfo };

    //How the data for a single vertex (incl pos, colour, text coord ect) is as a whole.
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    // How the data for an attribute is defined within a vertex
    std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions;

    // Position attribute
    attributeDescriptions[0].binding = 0;   //Which binding the data is at.
    attributeDescriptions[0].location = 0;  //Location in shader where data will be read from.
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;   // Format the data will take. Helps define the size og the data.
    attributeDescriptions[0].offset = offsetof(Vertex, pos);    // Where this attribute is defined in the data for a single vertex.

    // Colour attribute
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, col);

    // Texture attribute
    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, tex);


    //---------CREATE PIPELINE---------------------
    // STAGE 01: Vertex input
    VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
    vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
    vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();


    // STAGE 02: Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {};
    inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;


    // STAGE 03: Viewport & Scissor
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swapchainExtent.width;
    viewport.height = (float)swapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = swapchainExtent;

    VkPipelineViewportStateCreateInfo viewportStateCrateInfo = {};
    viewportStateCrateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCrateInfo.viewportCount = 1;
    viewportStateCrateInfo.pViewports = &viewport;
    viewportStateCrateInfo.scissorCount = 1;
    viewportStateCrateInfo.pScissors = &scissor;

    // STAGE 04: Dynamic States
    // Dynamic states to enable.
    /*
    std::vector<VkDynamicState> dynamicStateEnables;
    dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT); // Dynamic viewport. Resize in command buffer. vkcmdSetViewport(commandbuffer, 0, 1, &viewport)
    dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR); //vkcmdSetScissor(commandbuffer, 0, 1, &scissor)
    ... See lesson 12.
    */

    // STAGE 05: Rasterizer
    // Convert prim to frag on the screen.
    VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = {};
    rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizerCreateInfo.depthClampEnable = VK_FALSE;
    rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizerCreateInfo.lineWidth = 1.0f;
    rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizerCreateInfo.depthBiasEnable = VK_FALSE;

    
    // STAGE 06: Multisampling
    VkPipelineMultisampleStateCreateInfo multisamplingCreateInfo = {};
    multisamplingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisamplingCreateInfo.sampleShadingEnable = VK_FALSE;
    multisamplingCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    
    // STAGE 07: Blending
    VkPipelineColorBlendAttachmentState colourState = {};
    colourState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colourState.blendEnable = VK_TRUE;

    colourState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colourState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colourState.colorBlendOp = VK_BLEND_OP_ADD;
    colourState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colourState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colourState.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colourBlendingCreateInfo = {};
    colourBlendingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colourBlendingCreateInfo.logicOpEnable = VK_FALSE;
    colourBlendingCreateInfo.attachmentCount = 1;
    colourBlendingCreateInfo.pAttachments = &colourState;


    // STAGE 08: Pipeline Layout
    std::array<VkDescriptorSetLayout, 2> descriptorSetLayouts = { descriptorSetLayout, samplerSetLayout };

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;


    // Create Pipeline Layout
    VkResult result = vkCreatePipelineLayout(mainDevice.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout!");
    }

    
    // STAGE 09: Depth Stencil Testing
    VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo = {};
    depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilCreateInfo.depthTestEnable = VK_TRUE;
    depthStencilCreateInfo.depthWriteEnable = VK_TRUE;
    depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilCreateInfo.stencilTestEnable = VK_FALSE;

    // STAGE 10: Graphics Pipeline Creation
    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.stageCount = 2;
    pipelineCreateInfo.pStages = shaderStages;
    pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
    pipelineCreateInfo.pViewportState = &viewportStateCrateInfo;
    pipelineCreateInfo.pDynamicState = nullptr;
    pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
    pipelineCreateInfo.pMultisampleState = &multisamplingCreateInfo;
    pipelineCreateInfo.pColorBlendState = &colourBlendingCreateInfo;
    pipelineCreateInfo.pDepthStencilState = &depthStencilCreateInfo;
    pipelineCreateInfo.layout = pipelineLayout;
    pipelineCreateInfo.renderPass = renderPass;
    pipelineCreateInfo.subpass = 0;

    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex = -1;

    result = vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create a graphics pipeline!");
    }


    // Destroy shader modules no longer needed.
    vkDestroyShaderModule(mainDevice.logicalDevice, fragmentShaderModule, nullptr);
    vkDestroyShaderModule(mainDevice.logicalDevice, vertexShaderModule, nullptr);



    // CREATE SECOND PASS PIPELINE
    // Second pass shaders
    auto secondVertexShaderCode = readfile("Shaders/second_vert.spv");
    auto secondFragmentShaderCode = readfile("Shaders/second_frag.spv");

    // Build shaders
    VkShaderModule secondVertexShaderModule = createShaderModule(secondVertexShaderCode);
    VkShaderModule secondFragmentShaderModule = createShaderModule(secondFragmentShaderCode);

    // Set new shaders
    vertexShaderCreateInfo.module = secondVertexShaderModule;
    fragmentShaderCreateInfo.module = secondFragmentShaderModule;

    VkPipelineShaderStageCreateInfo secondShaderStages[] = { vertexShaderCreateInfo, fragmentShaderCreateInfo };

    // No vertex data for second pass.
    vertexInputCreateInfo.vertexBindingDescriptionCount = 0;
    vertexInputCreateInfo.pVertexBindingDescriptions = nullptr;
    vertexInputCreateInfo.vertexAttributeDescriptionCount = 0;
    vertexInputCreateInfo.pVertexAttributeDescriptions = nullptr;

    // Disable depth buffer. Don't want to write to depth buffer.
    depthStencilCreateInfo.depthWriteEnable = VK_FALSE;

    // Create new pipeline layout
    VkPipelineLayoutCreateInfo secondPipelineLayoutCreateInfo = {};
    secondPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    secondPipelineLayoutCreateInfo.setLayoutCount = 1;
    secondPipelineLayoutCreateInfo.pSetLayouts = &inputSetLayout;
    secondPipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    secondPipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

    result =vkCreatePipelineLayout(mainDevice.logicalDevice, &secondPipelineLayoutCreateInfo, nullptr, &secondPipelineLayout);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Second Pipeline Layput!");
    }

    pipelineCreateInfo.pStages = secondShaderStages;
    pipelineCreateInfo.layout = secondPipelineLayout;
    pipelineCreateInfo.subpass = 1;
    
    // Create second pipeline
    result = vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &secondPipeline);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Second Pipeline!");
    }

    // Destroy second shader modules
    vkDestroyShaderModule(mainDevice.logicalDevice, secondFragmentShaderModule, nullptr);
    vkDestroyShaderModule(mainDevice.logicalDevice, secondVertexShaderModule, nullptr);

}

void ShaderApplication::createColourBufferImage()
{
    colourBufferImage.resize(swapchainImages.size());
    colourBufferImageMemory.resize(swapchainImages.size());
    colourBufferImageView.resize(swapchainImages.size());

    VkFormat colourFormat = chooseSupportedFormat(
        { VK_FORMAT_R8G8B8A8_UNORM },
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    );
    for (size_t i = 0; i < swapchainImages.size(); i++)
    {
        colourBufferImage[i] = createImage(swapchainExtent.width, swapchainExtent.height, colourFormat, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &colourBufferImageMemory[i]);

        colourBufferImageView[i] = createImageView(colourBufferImage[i], colourFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }

}

void ShaderApplication::createDepthBufferImage()
{

    depthBufferImage.resize(swapchainImages.size());
    depthBufferImageMemory.resize(swapchainImages.size());
    depthBufferImageView.resize(swapchainImages.size());

    VkFormat depthFormat = chooseSupportedFormat(
        { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

    for (size_t i = 0; i < swapchainImages.size(); i++)
    {
        depthBufferImage[i] = createImage(swapchainExtent.width, swapchainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &depthBufferImageMemory[i]);

        depthBufferImageView[i] = createImageView(depthBufferImage[i], depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
    }
}

void ShaderApplication::createFramebuffers()
{
    swapchainFramebuffers.resize(swapchainImages.size());

    for (size_t i = 0; i < swapchainFramebuffers.size(); i++) {

        std::array<VkImage, 3> attachments = {
            swapchainImages[i].imageView,
            colourBufferImageView[i],
            depthBufferImageView[i]
        };

        VkFramebufferCreateInfo framebufferCreateinfo = {};
        framebufferCreateinfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateinfo.renderPass = renderPass;
        framebufferCreateinfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferCreateinfo.pAttachments = attachments.data();
        framebufferCreateinfo.width = swapchainExtent.width;
        framebufferCreateinfo.height = swapchainExtent.height;
        framebufferCreateinfo.layers = 1;

        VkResult result = vkCreateFramebuffer(mainDevice.logicalDevice, &framebufferCreateinfo, nullptr, &swapchainFramebuffers[i]);

        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create a framebuffer!");
        }
    }
}

void ShaderApplication::createCommandPool()
{
    QueueFamilyIndices queueFamilyIndecies = getQueueFamilies(mainDevice.physicalDevice);

    VkCommandPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolCreateInfo.queueFamilyIndex = queueFamilyIndecies.graphicsFamily;

    VkResult result = vkCreateCommandPool(mainDevice.logicalDevice, &poolCreateInfo, nullptr, &graphicsCommandPool);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create a command pool!");
    }
}

void ShaderApplication::createCommandBuffers()
{
    commandBuffers.resize(swapchainFramebuffers.size());

    VkCommandBufferAllocateInfo cbAllocInfo = {};
    cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbAllocInfo.commandPool = graphicsCommandPool;
    cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    cbAllocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    VkResult result = vkAllocateCommandBuffers(mainDevice.logicalDevice, &cbAllocInfo, commandBuffers.data());

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate commandbuffers!");
    }
}

void ShaderApplication::createSynchronisation()
{

    imageAvailable.resize(MAX_FRAME_DRAWS);
    renderFinished.resize(MAX_FRAME_DRAWS);
    drawFences.resize(MAX_FRAME_DRAWS);

    // Semaphore creation information
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAME_DRAWS; i++) {
        if (vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &imageAvailable[i]) != VK_SUCCESS ||
            vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &renderFinished[i]) != VK_SUCCESS ||
            vkCreateFence(mainDevice.logicalDevice, &fenceCreateInfo, nullptr, &drawFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create a semaphore and/or fence!");
        }
    }
}

void ShaderApplication::createTextureSampler()
{
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;                                 //How to render when image is magnified on screen.
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;                                 // WHen image is minified.
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;                // How to handle texture wrap in U (x) direction.
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;                // How to handle texture wrap in V (y) direction.
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;                // How to handle texture wrap in W (z) direction.
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;                           // The coordinates are normalized (between 0 and 1). unnormalized will be 0 - size of image.
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = 0.0f;
    samplerCreateInfo.anisotropyEnable = VK_TRUE;
    samplerCreateInfo.maxAnisotropy = 16;

    VkResult result = vkCreateSampler(mainDevice.logicalDevice, &samplerCreateInfo, nullptr, &textureSampler);

    if (result != VK_SUCCESS) 
    {
        throw std::runtime_error("Failed to create a Texture Sampler");
    }


}

void ShaderApplication::createUniformBuffers()
{
    VkDeviceSize vpBufferSize= sizeof(UboViewProjection);
    //VkDeviceSize modelBufferSize = modelUniformAlignment * MAX_OBJECTS;


    // One uniform buffer for each image (and by extension, command buffer)
    vpUniformBuffer.resize(swapchainImages.size());
    vpUniformBufferMemory.resize(swapchainImages.size());
    //modelDynUniformBuffer.resize(swapchainImages.size());
    //modelDynUniformBufferMemory.resize(swapchainImages.size());

    // Create uniform buffer.
    for (size_t i = 0; i < swapchainImages.size(); i++) 
    {
        createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, vpBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &vpUniformBuffer[i], &vpUniformBufferMemory[i]);

        //createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, modelBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            //VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &modelDynUniformBuffer[i], &modelDynUniformBufferMemory[i]);
    }

}

void ShaderApplication::createDescriptorPool()
{

    // CREATE UNIFORM DESCRIPTOR POOL.

    // Types of descriptors and how many descriptors (not descriptor sets)
    VkDescriptorPoolSize vpPoolSize = {};
    vpPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    vpPoolSize.descriptorCount = static_cast<uint32_t>(vpUniformBuffer.size());

    VkDescriptorPoolSize modelPoolSize = {};
    modelPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    modelPoolSize.descriptorCount = static_cast<uint32_t>(modelDynUniformBuffer.size());

    //List of Pool Sizes
    std::vector<VkDescriptorPoolSize> descriptorPoolSizes = {vpPoolSize}; //{vpPoolSize, modelPoolSize} when enabling dynamic uniform buffer.

    // Data to create Descriptor Pool
    VkDescriptorPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.maxSets = static_cast<uint32_t>(swapchainImages.size());
    poolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
    poolCreateInfo.pPoolSizes = descriptorPoolSizes.data();

    // Create descriptor pool
    VkResult result = vkCreateDescriptorPool(mainDevice.logicalDevice, &poolCreateInfo, nullptr, &descriptorPool);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a descriptor pool!");
    }

    // CREATE SAMPLER DESCRIPTOR POOL
    // Texture sampler pool
    VkDescriptorPoolSize samplerPoolSize = {};
    samplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerPoolSize.descriptorCount = MAX_OBJECTS;

    VkDescriptorPoolCreateInfo samlerPoolCreateInfo = {};
    samlerPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    samlerPoolCreateInfo.maxSets = MAX_OBJECTS;
    samlerPoolCreateInfo.poolSizeCount = 1;
    samlerPoolCreateInfo.pPoolSizes = &samplerPoolSize;

    result = vkCreateDescriptorPool(mainDevice.logicalDevice, &samlerPoolCreateInfo, nullptr, &samplerDescriptorPool);

    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a sampler descriptor pool!");
    }

    // Create Input Attachment Descriptor Pool
    // Color Attachment Pool Size.
    VkDescriptorPoolSize colourInputPoolSize = {};
    colourInputPoolSize.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    colourInputPoolSize.descriptorCount = static_cast<uint32_t>(colourBufferImageView.size());

    // Depth Attachment Pool Size.
    VkDescriptorPoolSize depthInputPoolSize = {};
    depthInputPoolSize.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    depthInputPoolSize.descriptorCount = static_cast<uint32_t>(depthBufferImageView.size());

    std::vector<VkDescriptorPoolSize> inputPoolSizes = { colourInputPoolSize, depthInputPoolSize };

    // Ctreate input attachment pool
    VkDescriptorPoolCreateInfo inputPoolCreateInfo = {};
    inputPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    inputPoolCreateInfo.maxSets = swapchainImages.size();
    inputPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(inputPoolSizes.size());
    inputPoolCreateInfo.pPoolSizes = inputPoolSizes.data();

    result = vkCreateDescriptorPool(mainDevice.logicalDevice, &inputPoolCreateInfo, nullptr, &inputDescriptorPool);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a sampler descriptor pool!");
    }


}

void ShaderApplication::createDescriptorSets()
{
    // Resize Descriptor Set list, so one for every buffer.
    descriptorSets.resize(swapchainImages.size());

    std::vector<VkDescriptorSetLayout> setLayouts(swapchainImages.size(), descriptorSetLayout);

    VkDescriptorSetAllocateInfo setAllocInfo = {};
    setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAllocInfo.descriptorPool = descriptorPool;
    setAllocInfo.descriptorSetCount = static_cast<uint32_t>(swapchainImages.size());
    setAllocInfo.pSetLayouts = setLayouts.data();

    //Allocate descriptos sets (multiple)
    VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &setAllocInfo, descriptorSets.data());
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate Descriptor Sets!");
    }

    // Updatte all of descriptor set buffer bindings.
    for (size_t i = 0; i < swapchainImages.size(); i++)
    {

        //VIEW PROJECTION DESCRIPTOR
        VkDescriptorBufferInfo vpBufferInfo = {};
        vpBufferInfo.buffer = vpUniformBuffer[i];
        vpBufferInfo.offset = 0;
        vpBufferInfo.range = sizeof(UboViewProjection);

        VkWriteDescriptorSet vpSetWrite = {};
        vpSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        vpSetWrite.dstSet = descriptorSets[i];
        vpSetWrite.dstBinding = 0;
        vpSetWrite.dstArrayElement = 0;
        vpSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        vpSetWrite.descriptorCount = 1;
        vpSetWrite.pBufferInfo = &vpBufferInfo;

        // MODEL DESCRIPTOR
        /*VkDescriptorBufferInfo modelBufferInfo = {};
        modelBufferInfo.buffer = modelDynUniformBuffer[i];
        modelBufferInfo.offset = 0;
        modelBufferInfo.range = modelUniformAlignment;

        VkWriteDescriptorSet modelSetWrite = {};
        modelSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        modelSetWrite.dstSet = descriptorSets[i];
        modelSetWrite.dstBinding = 1;
        modelSetWrite.dstArrayElement = 0;
        modelSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        modelSetWrite.descriptorCount = 1;
        modelSetWrite.pBufferInfo = &modelBufferInfo;*/

        // List of descriptor set writes
        std::vector<VkWriteDescriptorSet> setWrites = {vpSetWrite}; //{vpSetWrite, modelSetWrite}

        vkUpdateDescriptorSets(mainDevice.logicalDevice, static_cast<uint32_t>(setWrites.size()), setWrites.data(), 0, nullptr);
    }

}

void ShaderApplication::createInputDescriptorSets()
{
    // Resize array to hold descriptor set for each swap chain image.
    inputDescriptorSets.resize(swapchainImages.size());

    // Fill Array of layouts ready for set creation
    std::vector<VkDescriptorSetLayout> setLayouts(swapchainImages.size(), inputSetLayout);

    VkDescriptorSetAllocateInfo setAllocInfo = {};
    setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAllocInfo.descriptorPool  = inputDescriptorPool;
    setAllocInfo.descriptorSetCount = static_cast<uint32_t>(swapchainImages.size());
    setAllocInfo.pSetLayouts = setLayouts.data();

    //Allocate descriptr sets
    VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &setAllocInfo, inputDescriptorSets.data());
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate Input Attachment Descriptor Sets");
    }

    // Update each descriptor set with inout attachment.
    for (size_t i = 0; i < swapchainImages.size(); i++)
    {
        // Colour Attachment Descriptor
        VkDescriptorImageInfo colourAttachmentDescriptor = {};
        colourAttachmentDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // Layout of the image when it is read from.
        colourAttachmentDescriptor.imageView = colourBufferImageView[i];
        colourAttachmentDescriptor.sampler = VK_NULL_HANDLE;

        //Colour attachment descriptor write
        VkWriteDescriptorSet colourWrite = {};
        colourWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        colourWrite.dstSet = inputDescriptorSets[i];
        colourWrite.dstBinding = 0;
        colourWrite.dstArrayElement =0;
        colourWrite.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        colourWrite.descriptorCount = 1;
        colourWrite.pImageInfo = &colourAttachmentDescriptor;

        // Depth Attachment Descriptor
        VkDescriptorImageInfo depthAttachmentDescriptor = {};
        depthAttachmentDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // Layout of the image when it is read from.
        depthAttachmentDescriptor.imageView = depthBufferImageView[i];
        depthAttachmentDescriptor.sampler = VK_NULL_HANDLE;

        // Depth attachment descriptor write
        VkWriteDescriptorSet depthWrite = {};
        depthWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        depthWrite.dstSet = inputDescriptorSets[i];
        depthWrite.dstBinding = 1;
        depthWrite.dstArrayElement = 0;
        depthWrite.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        depthWrite.descriptorCount = 1;
        depthWrite.pImageInfo = &depthAttachmentDescriptor;

        // List of input descriptor set writes
        std::vector<VkWriteDescriptorSet> setWrites = { colourWrite, depthWrite };

        // Update descriptor Sets.
        vkUpdateDescriptorSets(mainDevice.logicalDevice, static_cast<uint32_t>(setWrites.size()), setWrites.data(), 0, nullptr);
    }
}

void ShaderApplication::updateUniformBuffers(uint32_t imageIndex)
{
    // copy vp data
    void * data;
    vkMapMemory(mainDevice.logicalDevice, vpUniformBufferMemory[imageIndex], 0, sizeof(UboViewProjection), 0, &data);
    memcpy(data, &uboViewProjection, sizeof(UboViewProjection));
    vkUnmapMemory(mainDevice.logicalDevice, vpUniformBufferMemory[imageIndex]);

    // copy model data
    /*for (size_t i = 0; i < meshList.size(); i++)
    {
        UboModel * thisModel = (UboModel *)((uint64_t)modelTransferSpace  + (i * modelUniformAlignment));
        * thisModel = meshList[i].getModel();
    }

    // Map the list of model data.
    vkMapMemory(mainDevice.logicalDevice, modelDynUniformBufferMemory[imageIndex], 0, modelUniformAlignment * meshList.size(), 0, &data);
    memcpy(data, modelTransferSpace, modelUniformAlignment * meshList.size());
    vkUnmapMemory(mainDevice.logicalDevice, modelDynUniformBufferMemory[imageIndex]);*/

}

void ShaderApplication::recordCommands(uint32_t currentImage)
{
    VkCommandBufferBeginInfo bufferBeginInfo = {};
    bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VkRenderPassBeginInfo renderpassBeginInfo = {}; //Only need this info struct for graphical applications.
    renderpassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderpassBeginInfo.renderPass = renderPass;
    renderpassBeginInfo.renderArea.offset = {0,0};
    renderpassBeginInfo.renderArea.extent = swapchainExtent;

    std::array<VkClearValue, 3> clearValues = {};
    clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f};
    clearValues[1].color =  {0.1f, 0.1f, 0.1f, 1.0f};
    clearValues[2].depthStencil.depth = 1.0f;

    renderpassBeginInfo.pClearValues = clearValues.data();
    renderpassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());

    renderpassBeginInfo.framebuffer = swapchainFramebuffers[currentImage];
    VkResult result = vkBeginCommandBuffer(commandBuffers[currentImage], &bufferBeginInfo);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failes to start recording a commandbuffer!");
    }

    // Begin renderpass
    vkCmdBeginRenderPass(commandBuffers[currentImage], &renderpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Bind pipeline to be used in renderpass
    vkCmdBindPipeline(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    for (size_t j = 0; j < modelList.size(); j++)
    {


        MeshModel thisModel = modelList[j];

        glm::mat4 matModel = thisModel.getModel(); // no pointer

        vkCmdPushConstants(
            commandBuffers[currentImage],
            pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(Model),
            &matModel);

        for (size_t k = 0; k < thisModel.getMeshCount(); k++)
        {
  

            VkBuffer vertexBuffers[] = { thisModel.getMesh(k)->getVertexBuffer() };   // Buffers to bind
            VkDeviceSize offsets[] = { 0 };     //Offsets into buffers being bound.
            vkCmdBindVertexBuffers(commandBuffers[currentImage], 0, 1, vertexBuffers, offsets);    //Command to bind vertex buffer before drawing with them.

            // Bind mesh index buffer with 0 offset and using uint32 type.
            vkCmdBindIndexBuffer(commandBuffers[currentImage], thisModel.getMesh(k)->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

            // Dynamic Offset Amount
            //uint32_t dynamicOffset = static_cast<uint32_t>(modelUniformAlignment) * j;

            std::array<VkDescriptorSet, 2> descriptorSetGroup = { descriptorSets[currentImage],
                samplerDescriptorSets[thisModel.getMesh(k)->getTexId()] };

            // Bind Descriptor Sets
            vkCmdBindDescriptorSets(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                0, static_cast<uint32_t>(descriptorSetGroup.size()), descriptorSetGroup.data(), 0, nullptr);

            // Execute our pipeline
            vkCmdDrawIndexed(commandBuffers[currentImage], thisModel.getMesh(k)->getIndexCount(), 1, 0, 0, 0);
        }
    }

    //Start second subpass
    vkCmdNextSubpass(commandBuffers[currentImage], VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, secondPipeline);

    vkCmdBindDescriptorSets(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, secondPipelineLayout, 0, 1, &inputDescriptorSets[currentImage], 0, nullptr);
    vkCmdDraw(commandBuffers[currentImage], 3, 1, 0, 0);

    // End render pass
    vkCmdEndRenderPass(commandBuffers[currentImage]);

    result = vkEndCommandBuffer(commandBuffers[currentImage]);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to stop recording a commandbuffer!");
    }
}


void ShaderApplication::getPhysicalDevice(){
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("Can`t find GPUs that support Vulkan instance!");
    }

    std::vector<VkPhysicalDevice> deviceList(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, deviceList.data());

    for (const auto& device : deviceList) {
        if (checkDeviceSuitable(device)) {
            mainDevice.physicalDevice = device;
            break;
        }
    }

    // Get properties of our new device.
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(mainDevice.physicalDevice, &deviceProperties);

    //minUniformBufferOffset = deviceProperties.limits.minUniformBufferOffsetAlignment;
}

QueueFamilyIndices ShaderApplication::getQueueFamilies(VkPhysicalDevice device){
    QueueFamilyIndices indicies;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilyList(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyList.data());
    
    int i = 0;

    for (const auto& queueFamily : queueFamilyList) {
        if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indicies.graphicsFamily = i;
        }

        VkBool32 presentationSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentationSupport);
        if (queueFamily.queueCount > 0 && presentationSupport) {
            indicies.presentationFamily = i;
        }

        if (indicies.isValid())
        {
            break;
        }

        i++;
    }

    return indicies;

}

SwapChainDetails ShaderApplication::getSwapChainDetails(VkPhysicalDevice device){
    SwapChainDetails swapChainDetails;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &swapChainDetails.surfaceCapabilities);
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if (formatCount != 0) {
        swapChainDetails.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, swapChainDetails.formats.data());
    }

    uint32_t presentationCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount, nullptr);

    if (presentationCount != 0) {
        swapChainDetails.presentationModes.resize(presentationCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount, swapChainDetails.presentationModes.data());
    }

    return swapChainDetails;
}

void ShaderApplication::allocateDynamicBufferTransferSpace()
{
    /*modelUniformAlignment = (sizeof(UboModel) + minUniformBufferOffset - 1) & ~(minUniformBufferOffset -1);

    // Create space in memory to hold dynamic buffer that is alligned to our requires alignment and holds MAX_OBJECTS.
    modelTransferSpace = (UboModel *)_aligned_malloc(modelUniformAlignment * MAX_OBJECTS, modelUniformAlignment);*/
}

void ShaderApplication::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

void ShaderApplication::setupDebugMessenger() {
    if (!enableValidationLayers) return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);

    if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("failed to set up debug messenger!");
    }
}

std::vector<const char*> ShaderApplication::getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

bool ShaderApplication::checkDeviceExtensionSupport(VkPhysicalDevice device){
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    if (extensionCount == 0) {
        return false;
    }

    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());

    for (const auto &deviceExtension : deviceExtensions) {
        bool hasExtension = false;
        for (const auto& extension : extensions)
        {
            if(strcmp(deviceExtension, extension.extensionName) == 0)
            {
                hasExtension = true;
                break;
            }
        }
        if (!hasExtension) {
            return false;
        }
    }

    return true;
}

bool ShaderApplication::checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

bool ShaderApplication::checkDeviceSuitable(VkPhysicalDevice device){
    /*
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);*/

    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    QueueFamilyIndices indicies = getQueueFamilies(device);
    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainValid = false;

    if (extensionsSupported) {
        SwapChainDetails swapChainDetails = getSwapChainDetails(device);
        swapChainValid = !swapChainDetails.presentationModes.empty() && !swapChainDetails.formats.empty();
    }

    return indicies.isValid() && extensionsSupported && swapChainValid && deviceFeatures.samplerAnisotropy;
}

VkSurfaceFormatKHR ShaderApplication::chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats){
    if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
        return {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    }

    for (const auto& format : formats) {
        if ((format.format == VK_FORMAT_R8G8B8A8_UNORM || format.format == VK_FORMAT_B8G8R8A8_UNORM) && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        
        }
    }

    return formats[0];

}

VkPresentModeKHR ShaderApplication::chooseBestPresentationMode(const std::vector<VkPresentModeKHR> presentationModes){
    for (const auto& presentationMode : presentationModes) {
        if (presentationMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return presentationMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D ShaderApplication::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities){
    if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return surfaceCapabilities.currentExtent;
    }
    else {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D newExtent = {};
        newExtent.width = static_cast<uint32_t>(width);
        newExtent.height = static_cast<uint32_t>(height);

        newExtent.width = std::max(surfaceCapabilities.minImageExtent.width, std::min(surfaceCapabilities.maxImageExtent.width, newExtent.width));
        newExtent.height = std::max(surfaceCapabilities.minImageExtent.height, std::min(surfaceCapabilities.maxImageExtent.height, newExtent.height));

        return newExtent;
    }
}

VkFormat ShaderApplication::chooseSupportedFormat(const std::vector<VkFormat>& formats, VkImageTiling tiling, VkFormatFeatureFlags featureFlags)
{
    //Loop through the option and find compatible one.
    for (VkFormat format : formats) 
    {
        //Get properties for given format on this device.
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(mainDevice.physicalDevice, format, &properties);

        if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & featureFlags) == featureFlags)
        {
            return format;

        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & featureFlags) == featureFlags)
        {
            return format;
        }
    }

    throw std::runtime_error("Failed to find a matching fomrat!");

}

VkImage ShaderApplication::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags useFlags, VkMemoryPropertyFlags propFlags, VkDeviceMemory* imageMemory)
{
    // CREATE  THE IMAGE
    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.extent.width = width;
    imageCreateInfo.extent.height = height;
    imageCreateInfo.extent.depth = 1; // Just 1, no 3D aspect.
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.format = format;
    imageCreateInfo.tiling = tiling;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.usage = useFlags;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkImage image;
    VkResult result = vkCreateImage(mainDevice.logicalDevice, &imageCreateInfo, nullptr, &image);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create an image!");
    }

    // CREATE MEMORY FOR THE IMAGE

    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(mainDevice.logicalDevice, image, &memoryRequirements);


    VkMemoryAllocateInfo memoryAllocInfo = {};
    memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocInfo.allocationSize = memoryRequirements.size;
    memoryAllocInfo.memoryTypeIndex = findMemoryTypeIndex(mainDevice.physicalDevice, memoryRequirements.memoryTypeBits, propFlags);

    result = vkAllocateMemory(mainDevice.logicalDevice, &memoryAllocInfo, nullptr, imageMemory);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate memory for image!");
    }

    vkBindImageMemory(mainDevice.logicalDevice, image, *imageMemory, 0);

    return image;

}

VkImageView ShaderApplication::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags){
    VkImageViewCreateInfo viewCreateInfo = {};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.image = image;
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format = format;
    viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    viewCreateInfo.subresourceRange.aspectMask = aspectFlags;
    viewCreateInfo.subresourceRange.baseMipLevel = 0;
    viewCreateInfo.subresourceRange.levelCount = 1;
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;
    viewCreateInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    VkResult result = vkCreateImageView(mainDevice.logicalDevice, &viewCreateInfo, nullptr, &imageView);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create an Image View!");
    }

    return imageView;
}

VkShaderModule ShaderApplication::createShaderModule(const std::vector<char> &code)
{
    VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.codeSize = code.size();
    shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

    VkShaderModule shadeModule;
    VkResult result = vkCreateShaderModule(mainDevice.logicalDevice, &shaderModuleCreateInfo, nullptr, &shadeModule);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create a shader module!");
    }

    return shadeModule;
}

int ShaderApplication::createTextureImage(std::string fileName)
{
    // Load image file
    int width, height;
    VkDeviceSize imageSize;
    stbi_uc * imageData = loadTextureFile(fileName, &width, &height, &imageSize);

    // Create staging buffer to hold loaded data, ready to copy to device.
    VkBuffer imageStagingBuffer;
    VkDeviceMemory imageStagingBufferMemory;
    createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                    &imageStagingBuffer, &imageStagingBufferMemory);

    // Copy Image data to staging buffer.
    void *data;
    vkMapMemory(mainDevice.logicalDevice, imageStagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, imageData, static_cast<size_t>(imageSize));
    vkUnmapMemory(mainDevice.logicalDevice, imageStagingBufferMemory);

    // Free oriuginal image data.
    stbi_image_free(imageData);

    // Create image to hold final texture.
    VkImage texImage;
    VkDeviceMemory texImageMemory;
    texImage = createImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &texImageMemory);


    //Copy data to image
    // Transition image to be DST for copy operation
    transitionImageLayout(mainDevice.logicalDevice, graphicQueue, graphicsCommandPool, texImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // copy image data
    copyImageBuffer(mainDevice.logicalDevice, graphicQueue, graphicsCommandPool, imageStagingBuffer, texImage, width, height);

    // transition image to be shader readable for shader usage
    transitionImageLayout(mainDevice.logicalDevice, graphicQueue, graphicsCommandPool, texImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);


    // add texture data to vector for reference.
    textureImages.push_back(texImage);
    textureImageMemory.push_back(texImageMemory);

    // destroy staging buffers
    vkDestroyBuffer(mainDevice.logicalDevice, imageStagingBuffer, nullptr);
    vkFreeMemory(mainDevice.logicalDevice, imageStagingBufferMemory, nullptr);

    return textureImages.size() - 1;

}

int ShaderApplication::createTexture(std::string fileName)
{
    int textureImageLoc = createTextureImage(fileName);

    VkImageView imageView = createImageView(textureImages[textureImageLoc], VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
    textureImageViews.push_back(imageView);

    int descriptorLoc = createTextureDescriptor(imageView);

    return descriptorLoc;
}

int ShaderApplication::createTextureDescriptor(VkImageView textureImage)
{
    VkDescriptorSet descriptorSet;

    // Descriptor set allocation info
    VkDescriptorSetAllocateInfo setAllocInfo = {};
    setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAllocInfo.descriptorPool = samplerDescriptorPool;
    setAllocInfo.descriptorSetCount = 1;
    setAllocInfo.pSetLayouts = &samplerSetLayout;

    // Allocate Descriptor Set
    VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &setAllocInfo, &descriptorSet);
    if (result != VK_SUCCESS) 
    {
        throw std::runtime_error("Failed to allocate texture descriptor sets!");
    }

    // Texture Image Info
    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = textureImage;
    imageInfo.sampler = textureSampler;


    //Descriptor write info
    VkWriteDescriptorSet descriptorWrite = {};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    // Update new descriptor set.
    vkUpdateDescriptorSets(mainDevice.logicalDevice, 1, &descriptorWrite, 0, nullptr);

    // Add Descriptor Set to List
    samplerDescriptorSets.push_back(descriptorSet);

    // Return descriptor set location
    return samplerDescriptorSets.size() - 1;

}

int ShaderApplication::createMeshModel(std::string modelFile)
{
    // Import model scene
    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(modelFile, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);
    if (!scene)
    {
        throw std::runtime_error("Failed to load model! (" + modelFile + ")");
    }

    std::vector<std::string> textureNames = MeshModel::LoadMaterials(scene);

    // Cinversion from the materials list IDs to our descriptor array IDs

    std::vector<int> matToTex(textureNames.size());

    // Loop over texture names and ceatre textures for them.
    for (size_t i = 0; i < textureNames.size(); i++)
    {
        if (textureNames[i].empty())
        {
            matToTex[i] = 0;
        }
        else
        {
            matToTex[i] = createTexture(textureNames[i]);
        }
    }

    // Load in all our  meshes
    std::vector<Mesh> modelMeshes = MeshModel::LoadNode(mainDevice.physicalDevice, mainDevice.logicalDevice, graphicQueue, graphicsCommandPool, 
        scene->mRootNode, scene, matToTex);


    // Create mesh model and add to list.
    MeshModel meshModel = MeshModel(modelMeshes);
    modelList.push_back(meshModel);

    return modelList.size() - 1;
}

stbi_uc* ShaderApplication::loadTextureFile(std::string fileName, int* width, int* height, VkDeviceSize* imageSize)
{
     int channels;
     std::string fileLoc = "textures/" + fileName;
     stbi_uc * image = stbi_load(fileLoc.c_str(), width, height, &channels, STBI_rgb_alpha);

     if (!image) 
     {
         std::string fileLoc = "textures/" + fileName;
         throw std::runtime_error("Failed to load a texture file! (" + fileName + ")");
     }

     *imageSize = *width * *height * 4;

     return image;
}
           