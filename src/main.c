#include <stdio.h>
#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdbool.h>

struct Renderer {
	VkInstance instance;
};

const char * VALIDATION_LAYERS[] = {
	"VK_LAYER_KHRONOS_validation"
};



int check_validation_layers_support() {
	uint32_t available_layers_size;
	vkEnumerateInstanceLayerProperties(&available_layers_size, NULL);

	VkLayerProperties layers[available_layers_size];
	vkEnumerateInstanceLayerProperties(&available_layers_size, layers);
	
	bool found = false;
	int validation_layers_size = 1;
	for (int i = 0; i < validation_layers_size; i++) {
		bool layer_found = false;
		for (int j = 0; j < available_layers_size; j++) {

		}
	}
	return 0;
}

void create_vk_instance() {
	VkApplicationInfo info;
	info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	info.pApplicationName = "Hello Vulkan";
	info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	info.pEngineName = "Vulkan Engine";
	info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	info.apiVersion = VK_API_VERSION_1_0;

	uint32_t glfw_ext_count;
	const char** glfw_ext_names;
	glfw_ext_names = glfwGetRequiredInstanceExtensions(&glfw_ext_count);

	VkInstanceCreateInfo create_info;
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &info;
	create_info.enabledExtensionCount = glfw_ext_count;
	create_info.ppEnabledExtensionNames = glfw_ext_names;
	create_info.enabledLayerCount = 0;

	printf("Available Vulkan extensions:\n");
	
	uint32_t available_ext_count = 0;
	vkEnumerateInstanceExtensionProperties(NULL, &available_ext_count, NULL);

	VkExtensionProperties properties[available_ext_count];
	
	vkEnumerateInstanceExtensionProperties(NULL, &available_ext_count, properties);

	for (uint32_t i = 0; i < available_ext_count; i++) {
		printf(" * %s\n", properties[i].extensionName);
	}

	VkInstance instance;
	VkResult result = vkCreateInstance(&create_info, NULL, &instance);

	if (result != VK_SUCCESS) {
		printf("Failed to initialize Vulkan.");
	} else {
		printf("Vulkan initialized successfully.");
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
	create_vk_instance();
	while(!glfwWindowShouldClose(window)) {
		glfwPollEvents();
	}
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
