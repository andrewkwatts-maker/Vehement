// ========================================
// RAY-CASTING IMPLEMENTATION FOR OBJECT SELECTION
// Paste these implementations into StandaloneEditor.cpp
// ========================================

// Replace ScreenToWorldRay (around line 2489)
glm::vec3 StandaloneEditor::ScreenToWorldRay(int screenX, int screenY) {
    // Use the camera's ScreenToWorldRay method if available
    if (m_currentCamera) {
        auto& window = Nova::Engine::Instance().GetWindow();
        glm::vec2 screenPos(static_cast<float>(screenX), static_cast<float>(screenY));
        glm::vec2 screenSize(static_cast<float>(window.GetWidth()), static_cast<float>(window.GetHeight()));
        return m_currentCamera->ScreenToWorldRay(screenPos, screenSize);
    }

    // Fallback: Manual implementation using editor camera
    auto& window = Nova::Engine::Instance().GetWindow();
    float screenWidth = static_cast<float>(window.GetWidth());
    float screenHeight = static_cast<float>(window.GetHeight());

    // Convert screen coordinates to NDC (Normalized Device Coordinates)
    float x = (2.0f * screenX) / screenWidth - 1.0f;
    float y = 1.0f - (2.0f * screenY) / screenHeight;  // Flip Y axis

    // Create view and projection matrices using editor camera
    glm::vec3 cameraPos = m_editorCameraPos;
    glm::vec3 cameraTarget = m_editorCameraTarget;
    glm::vec3 up(0.0f, 1.0f, 0.0f);

    glm::mat4 view = glm::lookAt(cameraPos, cameraTarget, up);
    float aspectRatio = screenWidth / screenHeight;
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 1000.0f);

    // Unproject: screen -> NDC -> clip -> eye -> world
    glm::vec4 rayClip(x, y, -1.0f, 1.0f);
    glm::vec4 rayEye = glm::inverse(projection) * rayClip;
    rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);

    glm::vec3 rayWorld = glm::vec3(glm::inverse(view) * rayEye);
    return glm::normalize(rayWorld);
}

// Replace RayIntersectsAABB (around line 2497)
// Add check for positive distance only
bool StandaloneEditor::RayIntersectsAABB(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                                          const glm::vec3& aabbMin, const glm::vec3& aabbMax,
                                          float& distance) {
    // Ray-AABB intersection using slab method
    glm::vec3 invDir = 1.0f / rayDir;

    float tmin = (aabbMin.x - rayOrigin.x) * invDir.x;
    float tmax = (aabbMax.x - rayOrigin.x) * invDir.x;

    if (tmin > tmax) std::swap(tmin, tmax);

    float tymin = (aabbMin.y - rayOrigin.y) * invDir.y;
    float tymax = (aabbMax.y - rayOrigin.y) * invDir.y;

    if (tymin > tymax) std::swap(tymin, tymax);

    if ((tmin > tymax) || (tymin > tmax)) {
        return false;
    }

    if (tymin > tmin) tmin = tymin;
    if (tymax < tmax) tmax = tymax;

    float tzmin = (aabbMin.z - rayOrigin.z) * invDir.z;
    float tzmax = (aabbMax.z - rayOrigin.z) * invDir.z;

    if (tzmin > tzmax) std::swap(tzmin, tzmax);

    if ((tmin > tzmax) || (tzmin > tmax)) {
        return false;
    }

    if (tzmin > tmin) tmin = tzmin;
    if (tzmax < tmax) tmax = tzmax;

    // Only return positive distances (in front of ray origin)
    distance = tmin >= 0.0f ? tmin : tmax;
    return distance >= 0.0f;
}

// Replace SelectObject (around line 1696)
void StandaloneEditor::SelectObject(const glm::vec3& rayOrigin, const glm::vec3& rayDir) {
    if (m_sceneObjects.empty()) {
        spdlog::info("No objects in scene to select");
        return;
    }

    int closestObjectIndex = -1;
    float closestDistance = std::numeric_limits<float>::max();

    // Test ray against all scene objects
    for (size_t i = 0; i < m_sceneObjects.size(); ++i) {
        const auto& obj = m_sceneObjects[i];

        // Calculate world-space AABB for this object
        glm::vec3 aabbMin = obj.position + (obj.boundingBoxMin * obj.scale);
        glm::vec3 aabbMax = obj.position + (obj.boundingBoxMax * obj.scale);

        // Test ray intersection with AABB
        float distance;
        if (RayIntersectsAABB(rayOrigin, rayDir, aabbMin, aabbMax, distance)) {
            // Found an intersection - check if it's closer than previous hits
            if (distance < closestDistance) {
                closestDistance = distance;
                closestObjectIndex = static_cast<int>(i);
            }
        }
    }

    // Select the closest object if we found one
    if (closestObjectIndex >= 0) {
        SelectObjectByIndex(closestObjectIndex, false);
        spdlog::info("Selected object '{}' at distance {:.2f}",
                    m_sceneObjects[closestObjectIndex].name, closestDistance);
    } else {
        ClearSelection();
        spdlog::info("No object hit by ray");
    }
}

// Replace SelectObjectAtScreenPos (around line 2288)
void StandaloneEditor::SelectObjectAtScreenPos(int x, int y) {
    // Generate ray from screen position
    glm::vec3 rayDir = ScreenToWorldRay(x, y);

    // Ray origin is the camera position
    glm::vec3 rayOrigin;
    if (m_currentCamera) {
        rayOrigin = m_currentCamera->GetPosition();
    } else {
        rayOrigin = m_editorCameraPos;
    }

    spdlog::info("Ray-casting from screen position ({}, {}) - Origin: ({:.2f}, {:.2f}, {:.2f}), Dir: ({:.2f}, {:.2f}, {:.2f})",
                x, y,
                rayOrigin.x, rayOrigin.y, rayOrigin.z,
                rayDir.x, rayDir.y, rayDir.z);

    // Perform ray-object intersection
    SelectObject(rayOrigin, rayDir);
}
