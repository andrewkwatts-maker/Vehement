// This file contains the image loading implementation for AssetBrowser.cpp
// To integrate: Replace the LoadImageThumbnail function in AssetBrowser.cpp with this implementation
// Add the includes at the top of AssetBrowser.cpp

// ===== INCLUDES TO ADD AT TOP OF AssetBrowser.cpp =====
/*
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#ifdef _WIN32
    #include <glad/glad.h>
#else
    #include <GL/gl.h>
#endif
*/

// ===== REPLACEMENT FOR LoadImageThumbnail =====
/*
ImTextureID ThumbnailCache::LoadImageThumbnail(const std::string& path) {
    // Load image using stb_image
    int width, height, channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4); // Force RGBA

    if (!data) {
        spdlog::warn("ThumbnailCache: Failed to load image: {}", path);
        return (ImTextureID)0;
    }

    // Create OpenGL texture
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Upload image data to GPU
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    // Free CPU image data
    stbi_image_free(data);

    spdlog::debug("ThumbnailCache: Loaded image thumbnail: {} ({}x{})", path, width, height);

    return (ImTextureID)(intptr_t)textureID;
}
*/

// ===== REPLACEMENT FOR Clear (to properly release OpenGL textures) =====
/*
void ThumbnailCache::Clear() {
    // Properly release GPU resources
    for (auto& pair : m_thumbnails) {
        if (pair.second != nullptr) {
            // Check if this is an actual OpenGL texture (not a placeholder hash)
            uintptr_t textureIDValue = (uintptr_t)pair.second;
            if (textureIDValue < 1000000) { // Simple heuristic: low values are likely real texture IDs
                GLuint textureID = (GLuint)textureIDValue;
                glDeleteTextures(1, &textureID);
            }
        }
    }
    m_thumbnails.clear();
}
*/
