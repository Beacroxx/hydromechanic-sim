#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "logger.hpp"
#include "textures.hpp"

#include <limits>
#include <memory>

std::vector<TextureImage> textures;

void loadTextures() {
  std::vector<TextureImage> loadedTextures;
  loadedTextures.reserve(embeddedTextureCount);

  for (std::size_t i = 0; i < embeddedTextureCount; ++i) {
    const EmbeddedTexture &texture = embeddedTextures[i];
    if (texture.bytes > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
      throw ERROR_LOC("embedded texture '{}' exceeds the decoder size limit", texture.name);
    }

    int width = 0;
    int height = 0;
    int sourceChannels = 0;
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
        stbi_load_from_memory(texture.data, static_cast<int>(texture.bytes), &width, &height, &sourceChannels,
                              STBI_rgb_alpha),
        stbi_image_free);
    if (!pixels) {
      throw ERROR_LOC("failed to decode embedded texture '{}': {}", texture.name, stbi_failure_reason());
    }

    const std::size_t widthSize = static_cast<std::size_t>(width);
    const std::size_t heightSize = static_cast<std::size_t>(height);
    if (width <= 0 || height <= 0 ||
        heightSize > std::numeric_limits<std::size_t>::max() / STBI_rgb_alpha ||
        widthSize > std::numeric_limits<std::size_t>::max() / (heightSize * STBI_rgb_alpha)) {
      throw ERROR_LOC("embedded texture '{}' has invalid dimensions", texture.name);
    }

    const std::size_t pixelBytes = widthSize * heightSize * STBI_rgb_alpha;
    TextureImage image {
        .pixels = std::vector<unsigned char>(pixels.get(), pixels.get() + pixelBytes),
        .width = width,
        .height = height,
        .channels = STBI_rgb_alpha,
    };
    loadedTextures.push_back(std::move(image));
  }

  textures = std::move(loadedTextures);
}