// Hardware regression: execute the shipping exposure SPIR-V on controlled HDR
// inputs, read its float texel, and compare against independent source equations.
#include <vulkan/vulkan.h>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

void Check(VkResult result) {
    if (result != VK_SUCCESS) throw std::runtime_error("Vulkan result " + std::to_string(result));
}
struct Probe {
    VkInstance instance{}; VkPhysicalDevice physical{}; VkDevice device{}; VkQueue queue{};
    VkCommandPool pool{}; VkCommandBuffer cmd{}; VkRenderPass pass{}; VkFramebuffer framebuffer{};
    VkPipelineLayout layout{}; VkPipeline pipeline{}; VkSampler sampler{}; VkDescriptorPool descriptors{};
    std::array<VkDescriptorSetLayout,2> setLayouts{}; std::array<VkDescriptorSet,2> sets{};
    std::vector<VkImage> images; std::vector<VkImageView> views; std::vector<VkDeviceMemory> memory;
    std::vector<VkBuffer> buffers; std::vector<VkShaderModule> shaders;
    void* uniforms{}; void* readback{};
    VkDeviceMemory uniformMemory{}, readbackMemory{};
    ~Probe() {
        if (device) {
            vkDeviceWaitIdle(device);
            if (uniforms) vkUnmapMemory(device, uniformMemory);
            if (readback) vkUnmapMemory(device, readbackMemory);
            vkDestroyPipeline(device,pipeline,nullptr); vkDestroyPipelineLayout(device,layout,nullptr);
            vkDestroyFramebuffer(device,framebuffer,nullptr); vkDestroyRenderPass(device,pass,nullptr);
            vkDestroyDescriptorPool(device,descriptors,nullptr);
            for(auto value:setLayouts) vkDestroyDescriptorSetLayout(device,value,nullptr);
            vkDestroySampler(device,sampler,nullptr); vkDestroyCommandPool(device,pool,nullptr);
            for(auto value:shaders) vkDestroyShaderModule(device,value,nullptr);
            for(auto value:views) vkDestroyImageView(device,value,nullptr);
            for(auto value:images) vkDestroyImage(device,value,nullptr);
            for(auto value:buffers) vkDestroyBuffer(device,value,nullptr);
            for(auto value:memory) vkFreeMemory(device,value,nullptr);
            vkDestroyDevice(device,nullptr);
        }
        vkDestroyInstance(instance,nullptr);
    }
    VkDeviceMemory Allocate(VkMemoryRequirements req, VkMemoryPropertyFlags flags) {
        VkPhysicalDeviceMemoryProperties props{}; vkGetPhysicalDeviceMemoryProperties(physical,&props);
        for (std::uint32_t i=0;i<props.memoryTypeCount;++i) {
            if ((req.memoryTypeBits & (1u<<i)) && (props.memoryTypes[i].propertyFlags & flags)==flags) {
                VkMemoryAllocateInfo info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
                info.allocationSize=req.size; info.memoryTypeIndex=i;
                VkDeviceMemory value{}; Check(vkAllocateMemory(device,&info,nullptr,&value));
                memory.push_back(value); return value;
            }
        }
        throw std::runtime_error("Required Vulkan memory type unavailable");
    }
    VkBuffer Buffer(VkDeviceSize size, VkBufferUsageFlags usage, void** mapped, VkDeviceMemory& mem) {
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; info.size=size; info.usage=usage;
        VkBuffer value{}; Check(vkCreateBuffer(device,&info,nullptr,&value)); buffers.push_back(value);
        VkMemoryRequirements req{}; vkGetBufferMemoryRequirements(device,value,&req);
        mem=Allocate(req,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        Check(vkBindBufferMemory(device,value,mem,0)); Check(vkMapMemory(device,mem,0,size,0,mapped)); return value;
    }
    void Image() {
        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO}; info.imageType=VK_IMAGE_TYPE_2D;
        info.format=VK_FORMAT_R32G32B32A32_SFLOAT; info.extent={1,1,1}; info.mipLevels=info.arrayLayers=1;
        info.samples=VK_SAMPLE_COUNT_1_BIT; info.tiling=VK_IMAGE_TILING_OPTIMAL;
        info.usage=VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        VkImage value{}; Check(vkCreateImage(device,&info,nullptr,&value)); images.push_back(value);
        VkMemoryRequirements req{}; vkGetImageMemoryRequirements(device,value,&req);
        Check(vkBindImageMemory(device,value,Allocate(req,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),0));
        VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO}; view.image=value;
        view.viewType=VK_IMAGE_VIEW_TYPE_2D; view.format=info.format; view.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        VkImageView handle{}; Check(vkCreateImageView(device,&view,nullptr,&handle)); views.push_back(handle);
    }
    VkShaderModule Shader(const std::filesystem::path& path) {
        std::ifstream file(path,std::ios::binary|std::ios::ate);
        if(!file) throw std::runtime_error("Missing shader: "+path.string());
        const auto size=static_cast<std::size_t>(file.tellg());
        if(!size || size%4) throw std::runtime_error("Invalid SPIR-V");
        std::vector<std::uint32_t> code(size/4); file.seekg(0); file.read(reinterpret_cast<char*>(code.data()),size);
        VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO}; info.codeSize=size; info.pCode=code.data();
        VkShaderModule value{}; Check(vkCreateShaderModule(device,&info,nullptr,&value)); shaders.push_back(value); return value;
    }
    explicit Probe(const std::filesystem::path& shaderRoot, bool tone = false) {
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO}; app.apiVersion=VK_API_VERSION_1_1;
        VkInstanceCreateInfo instanceInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; instanceInfo.pApplicationInfo=&app;
        Check(vkCreateInstance(&instanceInfo,nullptr,&instance));
        std::uint32_t count=0; Check(vkEnumeratePhysicalDevices(instance,&count,nullptr));
        std::vector<VkPhysicalDevice> devices(count); Check(vkEnumeratePhysicalDevices(instance,&count,devices.data()));
        std::uint32_t family=0; bool found=false;
        for(auto candidate:devices) {
            vkGetPhysicalDeviceQueueFamilyProperties(candidate,&count,nullptr);
            std::vector<VkQueueFamilyProperties> families(count); vkGetPhysicalDeviceQueueFamilyProperties(candidate,&count,families.data());
            for(std::uint32_t i=0;i<count;++i) if(families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {physical=candidate;family=i;found=true;break;}
            if(found) break;
        }
        if(!found) throw std::runtime_error("No graphics queue");
        float priority=1; VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queueInfo.queueFamilyIndex=family; queueInfo.queueCount=1; queueInfo.pQueuePriorities=&priority;
        VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; deviceInfo.queueCreateInfoCount=1; deviceInfo.pQueueCreateInfos=&queueInfo;
        Check(vkCreateDevice(physical,&deviceInfo,nullptr,&device)); vkGetDeviceQueue(device,family,0,&queue);
        VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; poolInfo.queueFamilyIndex=family;
        poolInfo.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; Check(vkCreateCommandPool(device,&poolInfo,nullptr,&pool));
        VkCommandBufferAllocateInfo cmdInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; cmdInfo.commandPool=pool;
        cmdInfo.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; cmdInfo.commandBufferCount=1; Check(vkAllocateCommandBuffers(device,&cmdInfo,&cmd));
        Image(); Image(); Image();
        auto uniform=Buffer(4624,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,&uniforms,uniformMemory);
        Buffer(16,VK_BUFFER_USAGE_TRANSFER_DST_BIT,&readback,readbackMemory); std::memset(uniforms,0,4624);
        VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter=samplerInfo.minFilter=VK_FILTER_NEAREST;
        samplerInfo.addressModeU=samplerInfo.addressModeV=samplerInfo.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        Check(vkCreateSampler(device,&samplerInfo,nullptr,&sampler));
        constexpr VkShaderStageFlags shaderStages=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutBinding ubo{0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,shaderStages,nullptr};
        VkDescriptorSetLayoutCreateInfo setInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO}; setInfo.bindingCount=1; setInfo.pBindings=&ubo;
        Check(vkCreateDescriptorSetLayout(device,&setInfo,nullptr,&setLayouts[0]));
        std::array<VkDescriptorSetLayoutBinding,5> inputs{};
        for(std::uint32_t i=0;i<5;++i) inputs[i]={i,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,shaderStages,nullptr};
        setInfo.bindingCount=5; setInfo.pBindings=inputs.data(); Check(vkCreateDescriptorSetLayout(device,&setInfo,nullptr,&setLayouts[1]));
        const std::array<VkDescriptorPoolSize,2> sizes{{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,5}}};
        VkDescriptorPoolCreateInfo descInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO}; descInfo.maxSets=2;
        descInfo.poolSizeCount=2; descInfo.pPoolSizes=sizes.data(); Check(vkCreateDescriptorPool(device,&descInfo,nullptr,&descriptors));
        VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO}; allocate.descriptorPool=descriptors;
        allocate.descriptorSetCount=2; allocate.pSetLayouts=setLayouts.data(); Check(vkAllocateDescriptorSets(device,&allocate,sets.data()));
        VkDescriptorBufferInfo bufferInfo{uniform,0,4624};
        std::array<VkDescriptorImageInfo,2> imageInfo{{{sampler,views[0],VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},{sampler,views[1],VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}}};
        std::array<VkWriteDescriptorSet,6> writes{};
        for(int i=0;i<6;++i) {
            writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; writes[i].descriptorCount=1;
            writes[i].dstSet=sets[i==0?0:1]; writes[i].dstBinding=i==0?0:i-1;
            writes[i].descriptorType=i==0?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            if(i==0) writes[i].pBufferInfo=&bufferInfo; else writes[i].pImageInfo=&imageInfo[i==5?1:0];
        }
        vkUpdateDescriptorSets(device,6,writes.data(),0,nullptr);
        VkAttachmentDescription attachment{}; attachment.format=VK_FORMAT_R32G32B32A32_SFLOAT; attachment.samples=VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE; attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        attachment.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED; attachment.finalLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        VkAttachmentReference color{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}; VkSubpassDescription subpass{};
        subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS; subpass.colorAttachmentCount=1; subpass.pColorAttachments=&color;
        VkSubpassDependency dependency{0,VK_SUBPASS_EXTERNAL,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,0};
        VkRenderPassCreateInfo passInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO}; passInfo.attachmentCount=1; passInfo.pAttachments=&attachment;
        passInfo.subpassCount=1; passInfo.pSubpasses=&subpass; passInfo.dependencyCount=1; passInfo.pDependencies=&dependency;
        Check(vkCreateRenderPass(device,&passInfo,nullptr,&pass));
        VkFramebufferCreateInfo fbInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO}; fbInfo.renderPass=pass; fbInfo.attachmentCount=1;
        fbInfo.pAttachments=&views[2]; fbInfo.width=fbInfo.height=fbInfo.layers=1; Check(vkCreateFramebuffer(device,&fbInfo,nullptr,&framebuffer));
        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; layoutInfo.setLayoutCount=2; layoutInfo.pSetLayouts=setLayouts.data();
        Check(vkCreatePipelineLayout(device,&layoutInfo,nullptr,&layout));
        const std::array<VkPipelineShaderStageCreateInfo,2> stages{{
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_VERTEX_BIT,Shader(shaderRoot/(tone?"SerenityToneMap.vert.spv":"NativeHybridScreenSpace.vert.spv")),"main",nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_FRAGMENT_BIT,Shader(shaderRoot/(tone?"SerenityToneMap.frag.spv":"SerenityExposure.frag.spv")),"main",nullptr}}};
        VkPipelineVertexInputStateCreateInfo vertex{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO}; assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkViewport viewport{0,0,1,1,0,1}; VkRect2D scissor{{0,0},{1,1}};
        VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO}; vp.viewportCount=vp.scissorCount=1; vp.pViewports=&viewport; vp.pScissors=&scissor;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO}; raster.polygonMode=VK_POLYGON_MODE_FILL; raster.lineWidth=1;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO}; ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState blendAttachment{}; blendAttachment.colorWriteMask=15;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO}; blend.attachmentCount=1; blend.pAttachments=&blendAttachment;
        VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO}; pipelineInfo.stageCount=2; pipelineInfo.pStages=stages.data();
        pipelineInfo.pVertexInputState=&vertex; pipelineInfo.pInputAssemblyState=&assembly; pipelineInfo.pViewportState=&vp;
        pipelineInfo.pRasterizationState=&raster; pipelineInfo.pMultisampleState=&ms; pipelineInfo.pColorBlendState=&blend;
        pipelineInfo.layout=layout; pipelineInfo.renderPass=pass;
        Check(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pipelineInfo,nullptr,&pipeline));
    }
    std::array<float,4> Run(std::array<float,4> source, std::array<float,4> history, bool valid, float dt, float speed) {
        auto* u=static_cast<float*>(uniforms); u[(4304+8*16)/4]=speed;
        u[4576/4]=dt; u[4576/4+1]=valid?1.f:0.f;
        Check(vkResetCommandBuffer(cmd,0)); VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; Check(vkBeginCommandBuffer(cmd,&begin));
        for(int i=0;i<2;++i) {
            VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER}; barrier.image=images[i];
            barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
            barrier.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED; barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
            VkClearColorValue value{}; std::memcpy(value.float32,(i==0?source:history).data(),16);
            vkCmdClearColorImage(cmd,images[i],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&value,1,&barrier.subresourceRange);
            barrier.oldLayout=barrier.newLayout; barrier.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_VERTEX_SHADER_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
        }
        VkRenderPassBeginInfo passBegin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO}; passBegin.renderPass=pass; passBegin.framebuffer=framebuffer; passBegin.renderArea.extent={1,1};
        vkCmdBeginRenderPass(cmd,&passBegin,VK_SUBPASS_CONTENTS_INLINE); vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
        vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,layout,0,2,sets.data(),0,nullptr); vkCmdDraw(cmd,3,1,0,0); vkCmdEndRenderPass(cmd);
        VkBufferImageCopy copy{}; copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; copy.imageExtent={1,1,1};
        vkCmdCopyImageToBuffer(cmd,images[2],VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffers[1],1,&copy);
        VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER}; host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);
        Check(vkEndCommandBuffer(cmd)); VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount=1; submit.pCommandBuffers=&cmd;
        Check(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE)); Check(vkQueueWaitIdle(queue));
        std::array<float,4> result{}; std::memcpy(result.data(),readback,16); return result;
    }
};

void Near(float got, double expected, const char* label) {
    if(!std::isfinite(got) || std::abs(got-expected)>std::max(0.000001,std::abs(expected)*0.002))
        throw std::runtime_error(std::string(label)+": got "+std::to_string(got)+", expected "+std::to_string(expected));
}
void Responses(const std::array<float,4>& sample) {
    Near(sample[2],.18/std::log2(double(sample[0])*2.5+1.045)*.62,"cone response");
    Near(sample[3],std::max(.012/std::log2(double(sample[1])+1.002)-.1,0.)*1.2,"rod response");
}
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("Usage: VulkanSerenityExposureProbe <compiled-shader-directory>");
        Probe gpu(argv[1]);
        const auto first=gpu.Run({2,.5f,.125f,1},{999,999,0,0},false,1.f/60,1);
        Near(first[0],2*.21+.5*.72+.125*.07,"source RGB weights; ignore invalid history"); Near(first[1],.08,"rod ceiling"); Responses(first);
        const auto adapted=gpu.Run({2,.5f,.125f,1},{.2f,.01f,0,0},true,1.f/60,1);
        Near(adapted[0],first[0]*.05+.2*.95,"photopic retention"); Near(adapted[1],.08*.015+.01*.985,"rod retention"); Responses(adapted);
        const auto fast=gpu.Run({2,.5f,.125f,1},{.2f,.01f,0,0},true,1.f/30,2);
        Near(fast[0],first[0]*(1-std::pow(.95,4))+.2*std::pow(.95,4),"time and speed scaling");
        const auto black=gpu.Run({0,0,0,1},{0,0,0,0},false,1.f/60,1);
        Near(black[0],.00003051757,"black scene floor"); Responses(black);
        auto sample=adapted;
        for(int i=0;i<60;++i) sample=gpu.Run({2,.5f,.125f,1},sample,true,1.f/60,1);
        Near(sample[0],first[0]+(adapted[0]-first[0])*std::pow(.95,60),"persistent recurrence"); Responses(sample);
        Probe tone(argv[1],true);
        auto* uniforms=static_cast<float*>(tone.uniforms);
        uniforms[4592/4]=2; uniforms[4304/4]=1;
        const auto manual=tone.Run({2,.5f,.125f,1},{1,1,1,1},false,1.f/60,1);
        uniforms[4304/4]=32;
        const auto multiplier=tone.Run({2,.5f,.125f,1},{1,1,1,1},false,1.f/60,1);
        for(int c=0;c<3;++c) Near(multiplier[c],manual[c],"manual mode ignores auto multiplier in shipping tone shaders");
        uniforms[4592/4]=.25f;
        const auto low=tone.Run({2,.5f,.125f,1},{1,1,1,1},false,1.f/60,1);
        if(!(manual[0]>low[0]+.01f && manual[1]>low[1]+.01f)) throw std::runtime_error("Manual exposure has no tone shader effect");
        std::cout<<"Serenity GPU exposure: RGB meter, cone/rod history, speed/time, black safety, 60-frame recurrence and manual tone shader output passed\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
