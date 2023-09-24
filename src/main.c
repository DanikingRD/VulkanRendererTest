#include <stdio.h>
#include <vulkan/vk_platform.h>
#define GLFW_INCLUDE_VULKAN
#include <vulkan/vulkan_core.h>
#include <GLFW/glfw3.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

const char * VALIDATION_LAYERS[] = {
	"VK_LAYER_KHRONOS_validation"
};

const char* DEVICE_EXTENSIONS[] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME 
};

struct Renderer {
	VkSurfaceKHR surface;
	VkInstance instance;
	VkPhysicalDevice physical_device;
	VkDevice logical_device;
	VkQueue graphics_queue;
	VkQueue present_queue;
	VkSwapchainKHR swap_chain;
	VkImage* swap_chain_images;
	uint32_t swap_chain_image_count;
	VkImageView* swap_chain_image_views;
	VkFormat swap_chain_image_format;
	VkExtent2D swap_chain_extent;
	VkPipelineLayout pipeline_layout;
	VkRenderPass render_pass;
	VkPipeline graphics_pipeline;
	VkFramebuffer* swapchain_frame_buffers;
	VkCommandPool command_pool;
	VkCommandBuffer command_buffer;
	VkSemaphore image_available_semaphore;
	VkSemaphore render_finished_semaphore;
	VkFence in_flight_fence;
};


int clamp(int val, int min, int max) {
	if (val < min) {
		return min;
	} else if (val > max) {
		return max;
	} else {
		return val;
	}
}

struct SwapChainDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	VkSurfaceFormatKHR* formats;
	VkPresentModeKHR* preset_modes;
	uint32_t present_count;
	uint32_t format_count;
};


char* read_file(const char* file_name, long* size) {
	FILE* fp = fopen(file_name, "r");
	char* buffer = NULL;
	if (fp != NULL) {
		fseek(fp, 0, SEEK_END);
		*size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		buffer = malloc(*size);
		fread(buffer, *size, 1, fp);
	}
	fclose(fp);
	return buffer;
}

void create_sync_objects(struct Renderer* renderer) {
	VkSemaphoreCreateInfo semaphore_info = {};
	semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fence_info = {};
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	if (vkCreateSemaphore(renderer->logical_device, &semaphore_info, NULL, &renderer->image_available_semaphore) != VK_SUCCESS ||
    	vkCreateSemaphore(renderer->logical_device, &semaphore_info, NULL, &renderer->render_finished_semaphore) != VK_SUCCESS ||
    	vkCreateFence(renderer->logical_device, &fence_info, NULL, &renderer->in_flight_fence) != VK_SUCCESS) {
	}
}


void record_command_buffer(struct Renderer* renderer, uint32_t image_index) {
	VkCommandBufferBeginInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	VkResult begin_buffer_res = vkBeginCommandBuffer(renderer->command_buffer, &info);

	if (begin_buffer_res != VK_SUCCESS) {
		printf("Failed to create begin command buffer. code: %d\n", begin_buffer_res);
	}
	VkOffset2D offs = {0,0 };
	VkRenderPassBeginInfo begin_render_pass_info = {};
	begin_render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
 	begin_render_pass_info.renderPass = renderer->render_pass;
	begin_render_pass_info.framebuffer = renderer->swapchain_frame_buffers[image_index];
	begin_render_pass_info.renderArea.offset = offs; 
	begin_render_pass_info.renderArea.extent = renderer->swap_chain_extent;
	
	VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
	begin_render_pass_info.clearValueCount = 1;
	begin_render_pass_info.pClearValues = &clearColor;

	vkCmdBeginRenderPass(renderer->command_buffer, &begin_render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(renderer->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->graphics_pipeline);
	vkCmdDraw(renderer->command_buffer, 3, 1, 0, 0);
	vkCmdEndRenderPass(renderer->command_buffer);

	if (vkEndCommandBuffer(renderer->command_buffer) != VK_SUCCESS) {
		printf("Failed to end command buffer\n");
	}
}


void draw_frame(struct Renderer* renderer) {
	vkWaitForFences(renderer->logical_device, 1, &renderer->in_flight_fence, VK_TRUE, UINT64_MAX);
	vkResetFences(renderer->logical_device, 1, &renderer->in_flight_fence);
	
	uint32_t image_index;
	vkAcquireNextImageKHR
		(renderer->logical_device, renderer->swap_chain, UINT64_MAX, renderer->image_available_semaphore, VK_NULL_HANDLE, &image_index);
	vkResetCommandBuffer(renderer->command_buffer, 0);
	record_command_buffer(renderer, image_index);

	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore wait_semaphores[] = {renderer->image_available_semaphore};
	VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &renderer->command_buffer;

	VkSemaphore signal[] = {renderer->render_finished_semaphore};
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal;

	if (vkQueueSubmit(renderer->graphics_queue, 1, &submit_info, renderer->in_flight_fence) != VK_SUCCESS) {
		printf("Failed to submit work.\n");
	}

	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = signal;
	
	VkSwapchainKHR swapchains[] = {renderer->swap_chain};
	present_info.swapchainCount = 1;
	present_info.pSwapchains = swapchains;
	present_info.pImageIndices = &image_index;

	present_info.pResults = NULL;
	vkQueuePresentKHR(renderer->present_queue, &present_info);
}


void create_command_buffer(struct Renderer* renderer) {
	VkCommandBufferAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = renderer->command_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 1;

	VkResult result = vkAllocateCommandBuffers(renderer->logical_device, &alloc_info, &renderer->command_buffer);

	if (result != VK_SUCCESS) {
		printf("Failed to create command buffer. code: %d\n", result);
	}


}

void create_frame_buffers(struct Renderer* renderer) {
	renderer->swapchain_frame_buffers = malloc(renderer->swap_chain_image_count);

	for (int i = 0; i < renderer->swap_chain_image_count; i++) {
		VkImageView attachments[] = {
			renderer->swap_chain_image_views[i],
		};
		VkFramebufferCreateInfo frame_buffer_info = {};
		frame_buffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frame_buffer_info.renderPass = renderer->render_pass;
		frame_buffer_info.attachmentCount = 1;
		frame_buffer_info.pAttachments = attachments;
		frame_buffer_info.width = renderer->swap_chain_extent.width;
		frame_buffer_info.height = renderer->swap_chain_extent.height;
		frame_buffer_info.layers = 1;

		VkResult result = 
			vkCreateFramebuffer(renderer->logical_device, &frame_buffer_info, NULL, &renderer->swapchain_frame_buffers[i]);
		if (result != VK_SUCCESS) {
			printf("Failed to create FrameBuffer. Error code: %d", result);
		}
	}
}

void create_render_pass(struct Renderer* renderer) {
	VkAttachmentDescription color_attachment = {};
	color_attachment.format = renderer->swap_chain_image_format;
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_attachment_ref = {};
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;

	VkSubpassDependency dep = {};
	dep.srcSubpass = VK_SUBPASS_EXTERNAL;
	dep.dstSubpass = 0;
	dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dep.srcAccessMask = 0;
	dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_info.attachmentCount = 1;
	render_pass_info.pAttachments = &color_attachment;
	render_pass_info.subpassCount = 1;
	render_pass_info.pSubpasses = &subpass;
	render_pass_info.dependencyCount = 1;
	render_pass_info.pDependencies = &dep;
		
	VkResult result = vkCreateRenderPass(renderer->logical_device, &render_pass_info, NULL, &renderer->render_pass);

	if (result != VK_SUCCESS) {
		printf("Failed to create Render Pass. Error code: %d", result);
	}
}

VkShaderModule create_shader_module(VkDevice device, char* code, uint32_t size) {
	VkShaderModuleCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.codeSize = size;
	create_info.pCode = (uint32_t*) code;
	VkShaderModule module = NULL;
	VkResult result = vkCreateShaderModule(device, &create_info, NULL, &module);
	if (result != VK_SUCCESS) {
		printf("Failed to create shader module. Error code: %d", result);
	}
	return module;

}

void create_graphics_pipeline(struct Renderer* renderer) {
	long vshader_size;
	long fshader_size;
	char* vshader_code = read_file("shaders/vert.spv", &vshader_size);
	char* fshader_code = read_file("shaders/frag.spv", &fshader_size);
	VkShaderModule vert_shader_module = create_shader_module(renderer->logical_device, vshader_code, vshader_size);
	VkShaderModule frag_shader_module = create_shader_module(renderer->logical_device, fshader_code, fshader_size);
	
	VkPipelineShaderStageCreateInfo vert_shader_stage_info = {};
	vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vert_shader_stage_info.pSpecializationInfo = NULL;
	vert_shader_stage_info.module = vert_shader_module;
	vert_shader_stage_info.pName = "main";

	VkPipelineShaderStageCreateInfo frag_shader_stage_info = {};
	frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	frag_shader_stage_info.pSpecializationInfo = NULL;
	frag_shader_stage_info.module = frag_shader_module;
	frag_shader_stage_info.pName = "main";
	
	VkPipelineShaderStageCreateInfo stages[] = {vert_shader_stage_info, frag_shader_stage_info};

	VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
	vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_info.vertexBindingDescriptionCount = 0;
	vertex_input_info.pVertexBindingDescriptions = NULL;
	vertex_input_info.vertexAttributeDescriptionCount = 0;
	vertex_input_info.pVertexAttributeDescriptions = NULL;

	VkPipelineInputAssemblyStateCreateInfo assembly_info = {};
	assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	assembly_info.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = renderer->swap_chain_extent.width;
	viewport.height = renderer->swap_chain_extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {
		.offset = {0, 0},
		.extent = renderer->swap_chain_extent,
	};

	VkPipelineViewportStateCreateInfo viewport_state = {};
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f;
	multisampling.pSampleMask = NULL;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState color_blend_attachment = {};
	color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT 
		| VK_COLOR_COMPONENT_G_BIT
		| VK_COLOR_COMPONENT_B_BIT
		| VK_COLOR_COMPONENT_A_BIT;
	color_blend_attachment.blendEnable = VK_FALSE;
	color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
	color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional
	
	VkPipelineColorBlendStateCreateInfo color_bleding = {};
	color_bleding.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_bleding.logicOpEnable = VK_FALSE;
	color_bleding.logicOp = VK_LOGIC_OP_COPY;
	color_bleding.attachmentCount = 1;
	color_bleding.pAttachments = &color_blend_attachment;
	color_bleding.blendConstants[0] = 0.0f;
	color_bleding.blendConstants[1] = 0.0f;
	color_bleding.blendConstants[2] = 0.0f;
	color_bleding.blendConstants[3] = 0.0f;

	VkPipelineLayoutCreateInfo pipeline_layout = {};
	pipeline_layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout.setLayoutCount = 0; // Optional
	pipeline_layout.pSetLayouts = NULL; // Optional
	pipeline_layout.pushConstantRangeCount = 0; // Optional
	pipeline_layout.pPushConstantRanges = NULL; // Optional
	VkResult result = vkCreatePipelineLayout(renderer->logical_device, &pipeline_layout, NULL, &renderer->pipeline_layout);
	if (result != VK_SUCCESS) {
		printf("Failed to create pipeline layout. Error code: %d\n", result);
	}

	VkGraphicsPipelineCreateInfo pipeline_info = {};
	pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_info.stageCount = 2;
	pipeline_info.pStages = stages;
	// stages
	pipeline_info.pVertexInputState = &vertex_input_info;
	pipeline_info.pInputAssemblyState = &assembly_info;
	pipeline_info.pViewportState = &viewport_state;
	pipeline_info.pRasterizationState = &rasterizer;
	pipeline_info.pMultisampleState = &multisampling;
	pipeline_info.pDepthStencilState = NULL;
	pipeline_info.pColorBlendState = &color_bleding;
	pipeline_info.pDynamicState = NULL;
	pipeline_info.layout = renderer->pipeline_layout;
	pipeline_info.renderPass = renderer->render_pass;
	pipeline_info.subpass = 0;
	pipeline_info.basePipelineIndex = -1;
	pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
	
	VkResult pipeline_result = 
		vkCreateGraphicsPipelines(renderer->logical_device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &renderer->graphics_pipeline);
	if (pipeline_result != VK_SUCCESS) {
		printf("Failed to create graphics pipeline. Error code: %d\n", result);
	} else {
		printf("Graphics pipeline ready.\n");
	}
	vkDestroyShaderModule(renderer->logical_device, vert_shader_module, NULL);
	vkDestroyShaderModule(renderer->logical_device, frag_shader_module, NULL);

	free(vshader_code);
	free(fshader_code);
}

struct OptionFamily {
	bool is_present;
	uint32_t value;
	
};

struct QueueFamily {
	struct OptionFamily graphics;
	struct OptionFamily presentation;
};

void create_image_views(struct Renderer* renderer) {
	renderer->swap_chain_image_views = malloc(sizeof(VkImageView) * renderer->swap_chain_image_count);

	for (uint32_t i = 0; i < renderer->swap_chain_image_count; i++) {
		VkImageViewCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		create_info.image = renderer->swap_chain_images[i];
		create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		create_info.format = renderer->swap_chain_image_format;
		create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		// setup sub-resource
		create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		create_info.subresourceRange.baseMipLevel = 0;
		create_info.subresourceRange.levelCount = 1;
		create_info.subresourceRange.baseArrayLayer = 0;
		create_info.subresourceRange.layerCount = 1;

		VkResult result =
			vkCreateImageView(renderer->logical_device, &create_info, NULL, &renderer->swap_chain_image_views[i]);
		if (result != VK_SUCCESS) {
			printf("Failed to create image view. Code: %d\n", result);
		}
		
	}
}

VkExtent2D choose_swap_extent(GLFWwindow* window,  struct SwapChainDetails* details) {

	if (details->capabilities.currentExtent.width != INT_MAX) {
		return details->capabilities.currentExtent;
	}
	int w, h;

	glfwGetFramebufferSize(window, &w, &h);
	
	VkExtent2D extent = {
		.width = w,
		.height = h,
	};
	
	extent.width =
		clamp(extent.width, details->capabilities.minImageExtent.width, details->capabilities.maxImageExtent.width);
	
	extent.height = 
		clamp(extent.height, details->capabilities.minImageExtent.height, details->capabilities.maxImageExtent.height);

	return extent;
}

VkPresentModeKHR choose_swapchain_present_mode(struct SwapChainDetails* details) {
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkSurfaceFormatKHR choose_swapchain_surface_format(struct SwapChainDetails* details) {
	for (uint32_t i = 0; i < details->format_count; i++) {
		VkSurfaceFormatKHR format = details->formats[i];

		if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			printf("The SRGB presentation format was picked!\n");
			return format;
		}
	}
	return details->formats[0];
}


struct SwapChainDetails query_swapchain_details(struct Renderer* renderer) {
	struct SwapChainDetails details = {};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(renderer->physical_device, renderer->surface, &details.capabilities);

	uint32_t formats = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(renderer->physical_device, renderer->surface, &formats, NULL);
	if (formats != 0) {
		details.format_count = formats;
		details.formats = malloc(sizeof(VkSurfaceFormatKHR) * formats);
		vkGetPhysicalDeviceSurfaceFormatsKHR(renderer->physical_device, renderer->surface, &formats, details.formats);
	}
	
	uint32_t present_modes = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(renderer->physical_device, renderer->surface, &present_modes, NULL);

	if (present_modes != 0) {
		details.present_count = present_modes;
		details.preset_modes = malloc(sizeof(VkPresentModeKHR) * present_modes);
		vkGetPhysicalDeviceSurfacePresentModesKHR(renderer->physical_device, renderer->surface, &present_modes, details.preset_modes);
	}

	return details;
}
struct QueueFamily find_queue_families(struct Renderer* renderer, VkPhysicalDevice device) {
	struct QueueFamily family;
	uint32_t count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &count, NULL);
	VkQueueFamilyProperties family_properties[count];
	vkGetPhysicalDeviceQueueFamilyProperties(device, &count, family_properties);

	for (uint32_t i = 0; i < count; i++) {
		VkQueueFamilyProperties props = family_properties[i];
		
		if (props.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			struct OptionFamily graphics_family = {
				.value = i,
				.is_present = true,
			};
			family.graphics = graphics_family;
		}
		VkBool32 present_support = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, renderer->surface, &present_support);
		if (present_support) {
			struct OptionFamily present_family = {
				.value = i,
				.is_present = true,
			};
			family.presentation = present_family;
		} 
	}
	return family;
}


void create_command_pool(struct Renderer* renderer) {
	struct QueueFamily family = find_queue_families(renderer, renderer->physical_device);
	VkCommandPoolCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	info.queueFamilyIndex = family.graphics.value;

	VkResult result = vkCreateCommandPool(renderer->logical_device, &info, NULL, &renderer->command_pool);

	if (result != VK_SUCCESS) {
		printf("Failed to create command pool. Error code: %d\n", result);
	}
}

void create_swap_chain(GLFWwindow* window, struct Renderer* renderer) {
	struct SwapChainDetails details = query_swapchain_details(renderer);
	VkSurfaceFormatKHR surface_format = choose_swapchain_surface_format(&details);
	VkPresentModeKHR present_mode = choose_swapchain_present_mode(&details);
	VkExtent2D extent = choose_swap_extent(window, &details);
	uint32_t image_count = details.capabilities.minImageCount + 1;

	if (details.capabilities.maxImageCount > 0 && image_count > details.capabilities.maxImageCount) {
		image_count = details.capabilities.maxImageCount;
	}
	
	VkSwapchainCreateInfoKHR create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.surface = renderer->surface;
	create_info.minImageCount = image_count;
	create_info.imageFormat = surface_format.format;
	create_info.imageColorSpace = surface_format.colorSpace;
	create_info.imageExtent = extent;
	create_info.imageArrayLayers = 1;
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	struct QueueFamily family = find_queue_families(renderer, renderer->physical_device);
	uint32_t queue_indices[2] = { family.graphics.value, family.presentation.value };
	if (family.graphics.value != family.presentation.value) {
		create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		create_info.queueFamilyIndexCount = 2;
		create_info.pQueueFamilyIndices = queue_indices;
	} else {
		create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		create_info.queueFamilyIndexCount = 0;
		create_info.pQueueFamilyIndices = NULL;
	}
	create_info.preTransform = details.capabilities.currentTransform;
	create_info.compositeAlpha= VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	create_info.presentMode = present_mode;
	create_info.clipped = VK_TRUE;
	create_info.oldSwapchain = NULL;
	
	VkResult result = vkCreateSwapchainKHR(renderer->logical_device, &create_info, NULL, &renderer->swap_chain);
	if (result != VK_SUCCESS) {
		printf("Failed to create swapchain. Error code: %d\n", result);
	}

	vkGetSwapchainImagesKHR(renderer->logical_device, renderer->swap_chain, &image_count, NULL);
	renderer->swap_chain_images = malloc(sizeof(VkImage) * image_count);
	vkGetSwapchainImagesKHR(renderer->logical_device, renderer->swap_chain, &image_count, renderer->swap_chain_images);
	renderer->swap_chain_image_format = surface_format.format;
	renderer->swap_chain_extent = extent; 
	renderer->swap_chain_image_count = image_count;
	printf("Swapchain created.\n");

	free(details.formats);
	free(details.preset_modes);
	details.formats = NULL;
	details.preset_modes = NULL;
}

int check_validation_layers_support() {
	uint32_t available_layers_size;
	vkEnumerateInstanceLayerProperties(&available_layers_size, NULL);
	
	printf("Available layers count: %d\n", available_layers_size);
	VkLayerProperties layers[available_layers_size];
	vkEnumerateInstanceLayerProperties(&available_layers_size, layers);
	
	bool found = false;
	uint32_t validation_layers_size = 1;
	for (uint32_t i = 0; i < validation_layers_size; i++) {
		const char* required_layer = VALIDATION_LAYERS[i];
		for (uint32_t j = 0; j < available_layers_size; j++) {
			const char* available_layer = layers[j].layerName;
			printf("Found layer: %d\n", available_layers_size);
			if (strcmp(required_layer, available_layer) == 0) {
				found = true;
				break;
			}
		}
		if (!found) {
			return false;
		} 
	}
	return true;
}



bool check_device_extension_support(VkPhysicalDevice device) {
	uint32_t count;
	vkEnumerateDeviceExtensionProperties(device, NULL, &count, NULL);
	VkExtensionProperties available_extensions[count];
	vkEnumerateDeviceExtensionProperties(device, NULL, &count, available_extensions);
	const uint32_t DEVICE_EXT_COUNT = 1;
	bool found = false;
	for (uint32_t i = 0; i < DEVICE_EXT_COUNT; i++) {
		const char* ext_name = DEVICE_EXTENSIONS[i];
		for (uint32_t j = 0; j < count; j++) {
			VkExtensionProperties ext = available_extensions[i];
			if (strcmp(ext_name, ext.extensionName) == 0) {
				found = true;
				break;
			}
		}
		if (!found) {
			return false;
		}
		
	}
	return true;
}

bool is_queue_family_ready(struct QueueFamily family) {
	return family.graphics.is_present && family.presentation.is_present;
}

void create_logical_device(struct Renderer* renderer) {
	struct QueueFamily family = find_queue_families(renderer, renderer->physical_device);
		
	const uint32_t QUEUE_COUNT = 2; // 0. graphics. 1. presentation
	VkDeviceQueueCreateInfo queue_infos[QUEUE_COUNT];

	float priority = 1.0f;
	for (uint32_t i = 0; i < QUEUE_COUNT; i++) {
		VkDeviceQueueCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		create_info.queueFamilyIndex = i;
		create_info.queueCount = 1;
		create_info.pQueuePriorities = &priority;
		queue_infos[i] = create_info;
	}

	VkPhysicalDeviceFeatures device_features = {};

	VkDeviceCreateInfo logical_create_info = {};
	logical_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	logical_create_info.pQueueCreateInfos = queue_infos;
	logical_create_info.queueCreateInfoCount = QUEUE_COUNT;
	logical_create_info.pEnabledFeatures = &device_features;
	logical_create_info.enabledExtensionCount = 1;
	logical_create_info.ppEnabledExtensionNames = DEVICE_EXTENSIONS;
	

	if (vkCreateDevice(renderer->physical_device, &logical_create_info, NULL, &renderer->logical_device) != VK_SUCCESS) {
		printf("Failed to create Logical Device.");
		return;
	} 
	
	printf("Logical Device created.\n");
	vkGetDeviceQueue(renderer->logical_device, family.graphics.value, 0, &renderer->graphics_queue);
	vkGetDeviceQueue(renderer->logical_device, family.presentation.value, 0, &renderer->present_queue);
}



VkResult create_debug_extension(
	VkInstance instance,
	const VkDebugUtilsMessengerCreateInfoEXT* create_info,
	const VkAllocationCallbacks* allocator,
	VkDebugUtilsMessengerEXT* debug_messenger
) {
	PFN_vkCreateDebugUtilsMessengerEXT fn = (PFN_vkCreateDebugUtilsMessengerEXT)
	vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

	if (fn != NULL) {
		return fn(instance, create_info, allocator, debug_messenger);
	}
	return VK_ERROR_EXTENSION_NOT_PRESENT;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debug_cb(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT type,
	const VkDebugUtilsMessengerCallbackDataEXT* cb_data,
	void* user_data
) {
	printf("Validation Layer: %s\n", cb_data->pMessage);
	
	if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		// message is important enough to show
	}
	return VK_FALSE;
}	

void setup_debug_messenger(VkInstance instance) {
	VkDebugUtilsMessengerCreateInfoEXT	create_info;
	create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	create_info.messageSeverity = 
	VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
	VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
	VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	create_info.messageType = 
	VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
	VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | 
	VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	create_info.pfnUserCallback = debug_cb;
	create_info.pUserData = NULL;
	VkDebugUtilsMessengerEXT messenger;
	if (create_debug_extension(instance, &create_info, NULL, &messenger) != VK_SUCCESS) {
		printf("Failed to setup debug messenger.");
	} else {
		printf("Debug messenger has been setup.");
	}
}
void create_surface(GLFWwindow* window, struct Renderer* renderer) {
	if (glfwCreateWindowSurface(renderer->instance, window, NULL, &renderer->surface) != VK_SUCCESS) {
		printf("Failed to create window surface");
		return;
	}
	printf("The window surface has been created.\n");
}

void create_vk_instance(struct Renderer* renderer) {
	
	if (!check_validation_layers_support()) {
		printf("Missing validation layers.\n");
		return;
	}
	printf("Validation layers are present.\n");

	VkApplicationInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	info.pApplicationName = "Hello Vulkan";
	info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	info.pEngineName = "Vulkan Engine";
	info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	info.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &info;

	uint32_t glfw_ext_count;
	const char** glfw_ext_names = glfw_ext_names = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
	const char* required_exts[glfw_ext_count + 1];
	for (uint32_t i = 0; i < glfw_ext_count; i++) {
		required_exts[i] = glfw_ext_names[i];
	}
	required_exts[glfw_ext_count] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
	
	for (uint32_t i = 0; i < glfw_ext_count + 1; i++) {
		printf(" * Required extension: [%s]\n", required_exts[i]);
	}

	create_info.enabledExtensionCount = glfw_ext_count;
	create_info.ppEnabledExtensionNames = glfw_ext_names;
	create_info.enabledLayerCount = 1;
	create_info.ppEnabledLayerNames = VALIDATION_LAYERS;

	printf("Available Vulkan extensions:\n");
	
	uint32_t available_ext_count = 0;
	vkEnumerateInstanceExtensionProperties(NULL, &available_ext_count, NULL);
	VkExtensionProperties properties[available_ext_count];
	
	vkEnumerateInstanceExtensionProperties(NULL, &available_ext_count, properties);

	for (uint32_t i = 0; i < available_ext_count; i++) {
		printf(" * %s\n", properties[i].extensionName);
	}
	VkResult result = vkCreateInstance(&create_info, NULL, &renderer->instance); 
	if (result != VK_SUCCESS) {
		printf("Failed to initialize Vulkan. code: %d\n", result);
	} else {
		printf("Vulkan initialized successfully.\n");
	}
}

void pick_physical_device(struct Renderer* renderer){
	uint32_t device_count = 0;
	vkEnumeratePhysicalDevices(renderer->instance, &device_count, NULL);

	if (device_count == 0) {
		printf("Failed to find GPUs with VULKAN support");
	}

	VkPhysicalDevice devices[device_count];
	vkEnumeratePhysicalDevices(renderer->instance, &device_count, devices);
	
	// check if device is suitable
	for (uint32_t i = 0; i< device_count; i++) {
		VkPhysicalDevice device = devices[i];
		VkPhysicalDeviceProperties properties;
		VkPhysicalDeviceFeatures features;
		vkGetPhysicalDeviceProperties(device, &properties);
		vkGetPhysicalDeviceFeatures(device, &features);
		if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && features.geometryShader) {
			struct QueueFamily family = find_queue_families(renderer, device);
	 		if (is_queue_family_ready(family)) {
				renderer->physical_device = device;
				struct SwapChainDetails details = query_swapchain_details(renderer);
				if (!details.preset_modes) {
					printf("Failed to collect presentation modes. should abort.\n");
				}
				create_logical_device(renderer);
				printf("GPU: %s has been picked.\n", properties.deviceName);
				break;
			} 
		} 
	}
	if (renderer->physical_device == NULL) {
		printf("Failed to find a suitable GPU.\n");
	}
}

void freeMemory(GLFWwindow* window, struct Renderer* renderer) {
	vkDestroySemaphore(renderer->logical_device, renderer->image_available_semaphore, NULL);
	vkDestroySemaphore(renderer->logical_device, renderer->render_finished_semaphore, NULL);
	vkDestroyFence(renderer->logical_device, renderer->in_flight_fence, NULL);
	vkDestroyCommandPool(renderer->logical_device, renderer->command_pool, NULL);
	for (uint32_t i = 0; i < renderer->swap_chain_image_count; i++) {
		vkDestroyFramebuffer(renderer->logical_device, renderer->swapchain_frame_buffers[i], NULL);
	}
	vkDestroyPipeline(renderer->logical_device, renderer->graphics_pipeline, NULL);
	vkDestroyPipelineLayout(renderer->logical_device, renderer->pipeline_layout, NULL);
	vkDestroyRenderPass(renderer->logical_device, renderer->render_pass, NULL);
	for (uint32_t i = 0; i < renderer->swap_chain_image_count; i++) {
		vkDestroyImageView(renderer->logical_device, renderer->swap_chain_image_views[i], NULL);
	}
	vkDestroySwapchainKHR(renderer->logical_device, renderer->swap_chain, NULL);
	vkDestroyDevice(renderer->logical_device, NULL);
	vkDestroySurfaceKHR(renderer->instance, renderer->surface, NULL);
	vkDestroyInstance(renderer->instance, NULL);
	free(renderer->swap_chain_image_views);

	glfwDestroyWindow(window);
	glfwTerminate();
}

int main(void) {
    if (!glfwInit()) {
		printf("Failed to initialize GLFW.");
		return -1;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	GLFWwindow* window = glfwCreateWindow(800, 600, "Hello Vulkan", NULL, NULL);
	
	if (!window) {
		glfwTerminate();
		return -1;
	}
	struct Renderer renderer = {};
	create_vk_instance(&renderer);
	create_surface(window, &renderer);
	pick_physical_device(&renderer);
	create_swap_chain(window, &renderer);
	create_image_views(&renderer);
	create_render_pass(&renderer);
	create_graphics_pipeline(&renderer);
	create_frame_buffers(&renderer);
	create_command_pool(&renderer);
	create_command_buffer(&renderer);
	create_sync_objects(&renderer);
	while(!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		draw_frame(&renderer);
	}
	vkDeviceWaitIdle(renderer.logical_device);
	freeMemory(window, &renderer);
	return 0;
}
