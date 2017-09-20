/*
 * Copyright 2017 Jacob Lifshay
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */
#include "api_objects.h"
#include "util/optional.h"
#include <iostream>
#include <type_traits>
#include <vector>
#include <algorithm>

namespace kazan
{
namespace vulkan
{
util::variant<std::unique_ptr<Vulkan_instance>, VkResult> Vulkan_instance::create(
    const VkInstanceCreateInfo &create_info)
{
    assert(create_info.sType == VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);
    assert(create_info.enabledLayerCount == 0); // we don't support layers here
    Supported_extensions extensions;
    for(std::size_t i = 0; i < create_info.enabledExtensionCount; i++)
    {
        auto extension = parse_extension_name(create_info.ppEnabledExtensionNames[i]);
        if(extension == Supported_extension::Not_supported)
        {
            std::cerr << "Error: unsupported extension passed to vkCreateInstance: "
                      << create_info.ppEnabledExtensionNames[i] << std::endl;
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        if(get_extension_scope(extension) != Extension_scope::Instance)
        {
            std::cerr << "Error: device extension passed to vkCreateInstance: "
                      << create_info.ppEnabledExtensionNames[i] << std::endl;
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        if(!std::get<1>(extensions.insert(extension)))
        {
            std::cerr << "Warning: duplicate extension passed to vkCreateInstance: "
                      << create_info.ppEnabledExtensionNames[i] << std::endl;
        }
    }
    for(auto extension : extensions)
    {
        for(auto dependency : get_extension_dependencies(extension))
        {
            if(extensions.count(dependency) == 0)
            {
                std::cerr << "Error: vkCreateInstance: enabled extension "
                          << get_extension_properties(extension).extensionName
                          << " depends on extension "
                          << get_extension_properties(dependency).extensionName << ", however "
                          << get_extension_properties(dependency).extensionName << " is not enabled"
                          << std::endl;
                return VK_ERROR_INITIALIZATION_FAILED;
            }
        }
    }
    util::optional<App_info> app_info;
    if(create_info.pApplicationInfo)
    {
        assert(create_info.pApplicationInfo->sType == VK_STRUCTURE_TYPE_APPLICATION_INFO);
        if(create_info.pApplicationInfo->apiVersion != 0
           && (VK_VERSION_MAJOR(create_info.pApplicationInfo->apiVersion) != 1
               || VK_VERSION_MINOR(create_info.pApplicationInfo->apiVersion) != 0))
        {
            return VK_ERROR_INCOMPATIBLE_DRIVER;
        }
        app_info.emplace(*create_info.pApplicationInfo);
    }
    else
    {
        app_info.emplace();
    }
    return std::make_unique<Vulkan_instance>(std::move(*app_info), std::move(extensions));
}

util::variant<std::unique_ptr<Vulkan_device>, VkResult> Vulkan_device::create(
    Vulkan_physical_device &physical_device, const VkDeviceCreateInfo &create_info)
{
    assert(create_info.sType == VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
    Supported_extensions extensions;
    Supported_extensions all_extensions = physical_device.instance.extensions;
    for(std::size_t i = 0; i < create_info.enabledExtensionCount; i++)
    {
        auto extension = parse_extension_name(create_info.ppEnabledExtensionNames[i]);
        if(extension == Supported_extension::Not_supported)
        {
            std::cerr << "Error: unsupported extension passed to vkCreateDevice: "
                      << create_info.ppEnabledExtensionNames[i] << std::endl;
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        if(get_extension_scope(extension) != Extension_scope::Device)
        {
            std::cerr << "Error: instance extension passed to vkCreateDevice: "
                      << create_info.ppEnabledExtensionNames[i] << std::endl;
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
        if(!std::get<1>(extensions.insert(extension)))
        {
            std::cerr << "Warning: duplicate extension passed to vkCreateDevice: "
                      << create_info.ppEnabledExtensionNames[i] << std::endl;
        }
        all_extensions.insert(extension);
    }
    for(auto extension : extensions)
    {
        for(auto dependency : get_extension_dependencies(extension))
        {
            if(all_extensions.count(dependency) == 0)
            {
                std::cerr << "Error: vkCreateDevice: enabled extension "
                          << get_extension_properties(extension).extensionName
                          << " depends on extension "
                          << get_extension_properties(dependency).extensionName << ", however "
                          << get_extension_properties(dependency).extensionName << " is not enabled"
                          << std::endl;
                return VK_ERROR_INITIALIZATION_FAILED;
            }
        }
    }
    // add enabled instance extensions
    for(auto extension : physical_device.instance.extensions)
        extensions.insert(extension);
    VkPhysicalDeviceFeatures enabled_features = {};
    if(create_info.pEnabledFeatures)
        enabled_features = *create_info.pEnabledFeatures;
    struct Feature_descriptor
    {
        VkBool32 VkPhysicalDeviceFeatures::*member;
        const char *name;
    };
    static constexpr std::initializer_list<Feature_descriptor> features = {
        {
            .member = &VkPhysicalDeviceFeatures::robustBufferAccess, .name = "robustBufferAccess",
        },
        {
            .member = &VkPhysicalDeviceFeatures::fullDrawIndexUint32, .name = "fullDrawIndexUint32",
        },
        {
            .member = &VkPhysicalDeviceFeatures::imageCubeArray, .name = "imageCubeArray",
        },
        {
            .member = &VkPhysicalDeviceFeatures::independentBlend, .name = "independentBlend",
        },
        {
            .member = &VkPhysicalDeviceFeatures::geometryShader, .name = "geometryShader",
        },
        {
            .member = &VkPhysicalDeviceFeatures::tessellationShader, .name = "tessellationShader",
        },
        {
            .member = &VkPhysicalDeviceFeatures::sampleRateShading, .name = "sampleRateShading",
        },
        {
            .member = &VkPhysicalDeviceFeatures::dualSrcBlend, .name = "dualSrcBlend",
        },
        {
            .member = &VkPhysicalDeviceFeatures::logicOp, .name = "logicOp",
        },
        {
            .member = &VkPhysicalDeviceFeatures::multiDrawIndirect, .name = "multiDrawIndirect",
        },
        {
            .member = &VkPhysicalDeviceFeatures::drawIndirectFirstInstance,
            .name = "drawIndirectFirstInstance",
        },
        {
            .member = &VkPhysicalDeviceFeatures::depthClamp, .name = "depthClamp",
        },
        {
            .member = &VkPhysicalDeviceFeatures::depthBiasClamp, .name = "depthBiasClamp",
        },
        {
            .member = &VkPhysicalDeviceFeatures::fillModeNonSolid, .name = "fillModeNonSolid",
        },
        {
            .member = &VkPhysicalDeviceFeatures::depthBounds, .name = "depthBounds",
        },
        {
            .member = &VkPhysicalDeviceFeatures::wideLines, .name = "wideLines",
        },
        {
            .member = &VkPhysicalDeviceFeatures::largePoints, .name = "largePoints",
        },
        {
            .member = &VkPhysicalDeviceFeatures::alphaToOne, .name = "alphaToOne",
        },
        {
            .member = &VkPhysicalDeviceFeatures::multiViewport, .name = "multiViewport",
        },
        {
            .member = &VkPhysicalDeviceFeatures::samplerAnisotropy, .name = "samplerAnisotropy",
        },
        {
            .member = &VkPhysicalDeviceFeatures::textureCompressionETC2,
            .name = "textureCompressionETC2",
        },
        {
            .member = &VkPhysicalDeviceFeatures::textureCompressionASTC_LDR,
            .name = "textureCompressionASTC_LDR",
        },
        {
            .member = &VkPhysicalDeviceFeatures::textureCompressionBC,
            .name = "textureCompressionBC",
        },
        {
            .member = &VkPhysicalDeviceFeatures::occlusionQueryPrecise,
            .name = "occlusionQueryPrecise",
        },
        {
            .member = &VkPhysicalDeviceFeatures::pipelineStatisticsQuery,
            .name = "pipelineStatisticsQuery",
        },
        {
            .member = &VkPhysicalDeviceFeatures::vertexPipelineStoresAndAtomics,
            .name = "vertexPipelineStoresAndAtomics",
        },
        {
            .member = &VkPhysicalDeviceFeatures::fragmentStoresAndAtomics,
            .name = "fragmentStoresAndAtomics",
        },
        {
            .member = &VkPhysicalDeviceFeatures::shaderTessellationAndGeometryPointSize,
            .name = "shaderTessellationAndGeometryPointSize",
        },
        {
            .member = &VkPhysicalDeviceFeatures::shaderImageGatherExtended,
            .name = "shaderImageGatherExtended",
        },
        {
            .member = &VkPhysicalDeviceFeatures::shaderStorageImageExtendedFormats,
            .name = "shaderStorageImageExtendedFormats",
        },
        {
            .member = &VkPhysicalDeviceFeatures::shaderStorageImageMultisample,
            .name = "shaderStorageImageMultisample",
        },
        {
            .member = &VkPhysicalDeviceFeatures::shaderStorageImageReadWithoutFormat,
            .name = "shaderStorageImageReadWithoutFormat",
        },
        {
            .member = &VkPhysicalDeviceFeatures::shaderStorageImageWriteWithoutFormat,
            .name = "shaderStorageImageWriteWithoutFormat",
        },
        {
            .member = &VkPhysicalDeviceFeatures::shaderUniformBufferArrayDynamicIndexing,
            .name = "shaderUniformBufferArrayDynamicIndexing",
        },
        {
            .member = &VkPhysicalDeviceFeatures::shaderSampledImageArrayDynamicIndexing,
            .name = "shaderSampledImageArrayDynamicIndexing",
        },
        {
            .member = &VkPhysicalDeviceFeatures::shaderStorageBufferArrayDynamicIndexing,
            .name = "shaderStorageBufferArrayDynamicIndexing",
        },
        {
            .member = &VkPhysicalDeviceFeatures::shaderStorageImageArrayDynamicIndexing,
            .name = "shaderStorageImageArrayDynamicIndexing",
        },
        {
            .member = &VkPhysicalDeviceFeatures::shaderClipDistance, .name = "shaderClipDistance",
        },
        {
            .member = &VkPhysicalDeviceFeatures::shaderCullDistance, .name = "shaderCullDistance",
        },
        {
            .member = &VkPhysicalDeviceFeatures::shaderFloat64, .name = "shaderFloat64",
        },
        {
            .member = &VkPhysicalDeviceFeatures::shaderInt64, .name = "shaderInt64",
        },
        {
            .member = &VkPhysicalDeviceFeatures::shaderInt16, .name = "shaderInt16",
        },
        {
            .member = &VkPhysicalDeviceFeatures::shaderResourceResidency,
            .name = "shaderResourceResidency",
        },
        {
            .member = &VkPhysicalDeviceFeatures::shaderResourceMinLod,
            .name = "shaderResourceMinLod",
        },
        {
            .member = &VkPhysicalDeviceFeatures::sparseBinding, .name = "sparseBinding",
        },
        {
            .member = &VkPhysicalDeviceFeatures::sparseResidencyBuffer,
            .name = "sparseResidencyBuffer",
        },
        {
            .member = &VkPhysicalDeviceFeatures::sparseResidencyImage2D,
            .name = "sparseResidencyImage2D",
        },
        {
            .member = &VkPhysicalDeviceFeatures::sparseResidencyImage3D,
            .name = "sparseResidencyImage3D",
        },
        {
            .member = &VkPhysicalDeviceFeatures::sparseResidency2Samples,
            .name = "sparseResidency2Samples",
        },
        {
            .member = &VkPhysicalDeviceFeatures::sparseResidency4Samples,
            .name = "sparseResidency4Samples",
        },
        {
            .member = &VkPhysicalDeviceFeatures::sparseResidency8Samples,
            .name = "sparseResidency8Samples",
        },
        {
            .member = &VkPhysicalDeviceFeatures::sparseResidency16Samples,
            .name = "sparseResidency16Samples",
        },
        {
            .member = &VkPhysicalDeviceFeatures::sparseResidencyAliased,
            .name = "sparseResidencyAliased",
        },
        {
            .member = &VkPhysicalDeviceFeatures::variableMultisampleRate,
            .name = "variableMultisampleRate",
        },
        {
            .member = &VkPhysicalDeviceFeatures::inheritedQueries, .name = "inheritedQueries",
        },
    };
    for(auto &feature : features)
    {
        if(enabled_features.*feature.member && !(physical_device.features.*feature.member))
        {
            std::cerr << "Error: vkCreateDevice: feature not supported: " << feature.name
                      << std::endl;
            return VK_ERROR_FEATURE_NOT_PRESENT;
        }
    }
    assert(create_info.queueCreateInfoCount == 1);
    assert(create_info.pQueueCreateInfos);
    assert(create_info.pQueueCreateInfos[0].sType == VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);
    assert(create_info.pQueueCreateInfos[0].queueFamilyIndex == 0);
    assert(create_info.pQueueCreateInfos[0].queueCount == 1);
    return std::make_unique<Vulkan_device>(physical_device, enabled_features, extensions);
}

std::unique_ptr<Vulkan_semaphore> Vulkan_semaphore::create(Vulkan_device &device,
                                                           const VkSemaphoreCreateInfo &create_info)
{
    assert(create_info.sType == VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
    assert(create_info.flags == 0);
    return std::make_unique<Vulkan_semaphore>();
}

VkResult Vulkan_fence::wait_multiple(std::uint32_t fence_count,
                                     const VkFence *fences,
                                     bool wait_for_all,
                                     std::uint64_t timeout)
{
    if(fence_count == 0)
        return VK_SUCCESS;
    assert(fences);

    typedef std::chrono::steady_clock::duration Duration;
    typedef std::chrono::steady_clock::time_point Time_point;

    // assume anything over 1000000 hours is
    // infinite; 1000000 hours is about 114
    // years, however, it's still way less than
    // 2^63 nanoseconds, so we won't overflow
    constexpr std::chrono::hours max_wait_time(1000000);
    util::optional<Duration> wait_duration; // nullopt means infinite timeout
    if(timeout <= static_cast<std::uint64_t>(
                      std::chrono::duration_cast<std::chrono::nanoseconds>(max_wait_time).count()))
    {
        wait_duration = std::chrono::duration_cast<Duration>(std::chrono::nanoseconds(timeout));
        if(wait_duration->count() == 0 && timeout != 0)
            wait_duration = Duration(1); // round up so we will sleep some
    }
    if(wait_duration && wait_duration->count() == 0)
    {
        bool found = false;
        bool search_for = !wait_for_all;
        for(std::uint32_t i = 0; i < fence_count; i++)
        {
            assert(fences[i]);
            if(from_handle(fences[i])->is_signaled() == search_for)
            {
                found = true;
                break;
            }
        }
        if(found && wait_for_all)
            return VK_TIMEOUT;
        if(!found && !wait_for_all)
            return VK_TIMEOUT;
        return VK_SUCCESS;
    }
    auto start_time = std::chrono::steady_clock::now();
    util::optional<Time_point> end_time; // nullopt means infinite timeout
    if(wait_duration && (start_time.time_since_epoch().count() <= 0
                         || Duration::max() - start_time.time_since_epoch() >= *wait_duration))
        end_time = start_time + *wait_duration;
    Waiter waiter(wait_for_all ? fence_count : 1);
    std::vector<std::list<Waiter *>::iterator> iters;
    iters.reserve(fence_count);
    struct Fence_cleanup
    {
        std::vector<std::list<Waiter *>::iterator> &iters;
        const VkFence *fences;
        ~Fence_cleanup()
        {
            for(std::uint32_t i = 0; i < iters.size(); i++)
            {
                auto *fence = from_handle(fences[i]);
                assert(fence);
                std::unique_lock<std::mutex> lock_it(fence->lock);
                fence->waiters.erase(iters[i]);
            }
        }
    } cleanup = {
        .iters = iters, .fences = fences,
    };
    for(std::uint32_t i = 0; i < fence_count; i++)
    {
        auto *fence = from_handle(fences[i]);
        assert(fence);
        std::unique_lock<std::mutex> lock_it(fence->lock);
        iters.push_back(fence->waiters.insert(fence->waiters.end(), &waiter));
        if(fence->signaled)
            waiter.notify(false);
    }
    assert(iters.size() == fence_count);
    return waiter.wait(end_time) ? VK_SUCCESS : VK_TIMEOUT;
}

std::unique_ptr<Vulkan_fence> Vulkan_fence::create(Vulkan_device &device,
                                                   const VkFenceCreateInfo &create_info)
{
    assert(create_info.sType == VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
    assert((create_info.flags & ~VK_FENCE_CREATE_SIGNALED_BIT) == 0);
    return std::make_unique<Vulkan_fence>(create_info.flags);
}

void Vulkan_image::clear(VkClearColorValue color) noexcept
{
    assert(memory);
    assert(descriptor.samples == VK_SAMPLE_COUNT_1_BIT && "multisample images are unimplemented");
    assert(descriptor.extent.width > 0);
    assert(descriptor.extent.height > 0);
    assert(descriptor.extent.depth > 0);

    assert(descriptor.type == VK_IMAGE_TYPE_2D && "unimplemented image type");
    assert(descriptor.extent.depth == 1);

    assert(descriptor.format == VK_FORMAT_B8G8R8A8_UNORM && "unimplemented image format");
    assert(descriptor.mip_levels == 1 && "mipmapping is unimplemented");
    assert(descriptor.array_layers == 1 && "array images are unimplemented");
#warning implement non-linear image tiling

    union
    {
        std::uint8_t bytes[4];
        std::uint32_t u32;
    } clear_color;
    float r_float = color.float32[0];
    float g_float = color.float32[1];
    float b_float = color.float32[2];
    float a_float = color.float32[3];
    auto float_to_byte = [](float v) noexcept->std::uint8_t
    {
        if(!(v >= 0))
            v = 0;
        else if(v > 1)
            v = 1;
        union
        {
            std::uint32_t i;
            float f;
        } u;
        static_assert(sizeof(std::uint32_t) == sizeof(float), "");
        u.f = 0x100;
        u.i--; // u.f = nextafter(u.f, -1)
        v *= u.f;
        return (int)v;
    };
    clear_color.bytes[0] = float_to_byte(b_float);
    clear_color.bytes[1] = float_to_byte(g_float);
    clear_color.bytes[2] = float_to_byte(r_float);
    clear_color.bytes[3] = float_to_byte(a_float);
    std::size_t pixel_count =
        static_cast<std::size_t>(descriptor.extent.width) * descriptor.extent.height;
    std::uint32_t *pixels = static_cast<std::uint32_t *>(memory.get());
    for(std::size_t i = 0; i < pixel_count; i++)
    {
        pixels[i] = clear_color.u32;
    }
}

std::unique_ptr<Vulkan_image> Vulkan_image::create(Vulkan_device &device,
                                                   const VkImageCreateInfo &create_info)
{
#warning finish implementing Vulkan_image::create
    return std::make_unique<Vulkan_image>(Vulkan_image_descriptor(create_info));
}

std::unique_ptr<Vulkan_image_view> Vulkan_image_view::create(
    Vulkan_device &device, const VkImageViewCreateInfo &create_info)
{
    assert(create_info.sType == VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
    assert(create_info.image);
    return std::make_unique<Vulkan_image_view>(*Vulkan_image::from_handle(create_info.image),
                                               create_info.viewType,
                                               create_info.format,
                                               create_info.components,
                                               create_info.subresourceRange);
}

void Vulkan_command_buffer::Command::on_record_end(Vulkan_command_buffer &command_buffer)
{
    static_cast<void>(command_buffer);
}


Vulkan_command_buffer::Vulkan_command_buffer(
    std::list<std::unique_ptr<Vulkan_command_buffer>>::iterator iter,
    Vulkan_command_pool &command_pool,
    Vulkan_device &device) noexcept : iter(iter),
                                      command_pool(command_pool),
                                      device(device),
                                      commands(),
                                      state(Command_buffer_state::Initial)
{
}

void Vulkan_command_buffer::reset(VkCommandBufferResetFlags flags)
{
    commands.clear();
    state = Command_buffer_state::Initial;
}

void Vulkan_command_buffer::begin(const VkCommandBufferBeginInfo &begin_info)
{
    commands.clear();
    state = Command_buffer_state::Recording;
}

VkResult Vulkan_command_buffer::end() noexcept
{
    if(state == Command_buffer_state::Out_of_memory)
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    assert(state == Command_buffer_state::Recording);
    try
    {
        for(auto &command : commands)
        {
            assert(command);
            command->on_record_end(*this);
        }
    }
    catch(std::bad_alloc &)
    {
        state = Command_buffer_state::Out_of_memory;
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    state = Command_buffer_state::Executable;
    return VK_SUCCESS;
}

void Vulkan_command_buffer::run() const noexcept
{
    assert(state == Command_buffer_state::Executable);
    Running_state running_state(*this);
    for(auto &command : commands)
        command->run(running_state);
}

void Vulkan_command_pool::allocate_multiple(Vulkan_device &device,
                                            const VkCommandBufferAllocateInfo &allocate_info,
                                            VkCommandBuffer *allocated_command_buffers)
{
    std::uint32_t command_buffer_count = allocate_info.commandBufferCount;
    try
    {
        std::list<std::unique_ptr<Vulkan_command_buffer>> current_command_buffers;
        for(std::uint32_t i = 0; i < command_buffer_count; i++)
        {
            auto iter = current_command_buffers.emplace(current_command_buffers.end());
            auto command_buffer = std::make_unique<Vulkan_command_buffer>(iter, *this, device);
            allocated_command_buffers[i] = to_handle(command_buffer.get());
            *iter = std::move(command_buffer);
        }
        command_buffers.splice(command_buffers.end(), current_command_buffers);
    }
    catch(...)
    {
        for(std::uint32_t i = 0; i < command_buffer_count; i++)
            allocated_command_buffers[i] = VK_NULL_HANDLE;
        throw;
    }
}

std::unique_ptr<Vulkan_command_pool> Vulkan_command_pool::create(
    Vulkan_device &device, const VkCommandPoolCreateInfo &create_info)
{
    assert(create_info.sType == VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
    assert(create_info.queueFamilyIndex < Vulkan_physical_device::queue_family_property_count);
    assert((create_info.flags
            & ~(VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
                | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT))
           == 0);
    return std::make_unique<Vulkan_command_pool>();
}
}
}
