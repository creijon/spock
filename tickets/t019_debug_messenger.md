[t019] Debug Messenger

#### Goal

No persistent debug messenger is created.  The callback is only chained into InstanceCreateInfo, so it only covers vkCreateInstance/vkDestroyInstance, and the message filtering in debugUtilsMessengerCallback never applies after that.  makeDebugUtilsMessengerCreateInfoEXT() is unused.


#### Claude
