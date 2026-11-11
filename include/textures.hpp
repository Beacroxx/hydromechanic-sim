#pragma once

#include <cstddef>
#include <vector>

struct EmbeddedTexture {
  const char *name;
  const unsigned char *data;
  std::size_t bytes;
};

struct TextureImage {
  std::vector<unsigned char> pixels;
  int width;
  int height;
  int channels;
};

extern const EmbeddedTexture embeddedTextures[];
extern const std::size_t embeddedTextureCount;

extern std::vector<TextureImage> textures;

void loadTextures();
