#include "Texture.hpp"

#include "Rendering/VulkanInclUde.hpp"

Sampler::operator VkSampler () const { return vkSampler; }
VkSampler* Sampler::operator&() const { return (VkSampler*)&vkSampler; }

Texture::Texture(const Texture& other) : name(other.name), width(other.width), height(other.height), depth(other.depth), size(other.size), mipmapLevels(other.mipmapLevels),
format(other.format), sampler(other.sampler), image(other.image), imageView(other.imageView), allocation(other.allocation)
{}
Texture::Texture(Texture&& other) : name(Move(other.name)), width(other.width), height(other.height), depth(other.depth), size(other.size), mipmapLevels(other.mipmapLevels),
format(other.format), sampler(other.sampler), image(other.image), imageView(other.imageView), allocation(other.allocation)
{}