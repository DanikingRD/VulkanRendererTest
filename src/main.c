#include <stdio.h>
#include <vulkan/vk_platform.h>
#define GLFW_INCLUDE_VULKAN
#include <vulkan/vulkan_core.h>
#include <GLFW/glfw3.h>
#include <stdbool.h>
#include <string.h>

const char * VALIDATION_LAYERS[] = {
	"VK_LAYER_KHRONOS_validation"
};

struct Renderer {
	VkSurfaceKHR surface;
	VkInstance instance;
	VkPhysicalDevice physical_device;
	VkDevice logical_device;
	VkQueue graphics_queue;
	VkQueue present_queue;
};

void create_surface(GLFWwindow* window, struct Renderer* renderer) {
	if (glfwCreateWindowSurface(renderer->instance, window, NULL, &renderer->surface) != VK_SUCCESS) {
		printf("Failed to create window surface");
		return;
	}

	printf("The window surface has been created.\n");
}

struct OptionFamily {
	bool is_present;
	uint32_t value;
	
};

struct QueueFamily {
	struct OptionFamily graphics;
	struct OptionFamily presentation;
};


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
bool is_queue_family_ready(struct QueueFamily family) {
	return family.graphics.is_present && family.presentation.is_present;
}


void create_logical_device(struct Renderer* renderer) {
	struct QueueFamily family = find_queue_families(renderer, renderer->physical_device);
	// create_info.pQueuePriorities = &prio;dd
		
	const uint32_t QUEUE_COUNT = 2; // 0. graphics. 1. presentation
	VkDeviceQueueCreateInfo queue_infos[QUEUE_COUNT];
	// uint32_t families[] = { family.graphics.value, family.presentation.value };

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
	logical_create_info.pQueueCreateInfos = queue_infos;
	logical_create_info.queueCreateInfoCount = QUEUE_COUNT;
	logical_create_info.pEnabledFeatures = &device_features;
	logical_create_info.enabledExtensionCount = 0;

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
	const char** glfw_ext_names;
	glfw_ext_names = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
	
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
		printf("Failed to initialize Vulkan.\n");
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
	vkEnumeratePhysicalDevices(renderer->instance, &device_count,	devices);
	
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
				create_logical_device(renderer);
				printf("GPU: %s has been picked.\n", properties.deviceName);
				break;
			} 
		} else {
			printf("Queue not ready.\n");
		}
	}

	if (renderer->physical_device == NULL) {
		printf("Failed to find a suitable GPU.\n");
	}
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
	struct Renderer renderer = { 0 };
	create_vk_instance(&renderer);
	create_surface(window, &renderer);
	pick_physical_device(&renderer);
	while(!glfwWindowShouldClose(window)) {
		glfwPollEvents();
	}
	glfwDestroyWindow(window);
	glfwTerminate();
	vkDestroySurfaceKHR(renderer.instance, renderer.surface, NULL);
	vkDestroyInstance(renderer.instance, NULL);
	return 0;
}
