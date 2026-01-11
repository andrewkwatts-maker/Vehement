#include "scene/Scene.hpp"
#include "scene/SceneNode.hpp"
#include "scene/Camera.hpp"
#include "scene/FlyCamera.hpp"
#include "graphics/Renderer.hpp"
#include "core/JobSystem.hpp"
#include "core/Profiler.hpp"

#include <algorithm>

namespace Nova {

Scene::Scene() {
    m_root = std::make_unique<SceneNode>("Root");
    m_camera = std::make_unique<FlyCamera>();
}

Scene::~Scene() {
    Shutdown();
}

bool Scene::Initialize() {
    return true;
}

void Scene::Update(float deltaTime) {
    if (m_root) {
        m_root->Update(deltaTime);
    }
}

void Scene::Render(Renderer& renderer) {
    if (m_camera) {
        renderer.SetCamera(m_camera.get());
    }

    if (m_root) {
        m_root->Render(renderer);
    }
}

void Scene::Shutdown() {
    m_root.reset();
    m_camera.reset();
}

void Scene::SetCamera(std::unique_ptr<Camera> camera) {
    m_camera = std::move(camera);
}

SceneNode* Scene::FindNode(std::string_view name) const {
    if (!m_root) {
        return nullptr;
    }

    // Check root itself
    if (m_root->GetName() == name) {
        return m_root.get();
    }

    return m_root->FindChild(name, true);
}

std::vector<SceneNode*> Scene::FindNodes(
    const std::function<bool(const SceneNode&)>& predicate) const {
    std::vector<SceneNode*> results;

    if (!m_root) {
        return results;
    }

    // Check root
    if (predicate(*m_root)) {
        results.push_back(m_root.get());
    }

    m_root->FindAll(predicate, results);
    return results;
}

void Scene::ForEachNode(const std::function<void(SceneNode&)>& func) {
    if (m_root) {
        m_root->ForEach(func);
    }
}

void Scene::ForEachNode(const std::function<void(const SceneNode&)>& func) const {
    if (m_root) {
        m_root->ForEach(func);
    }
}

size_t Scene::GetNodeCount() const {
    if (!m_root) {
        return 0;
    }

    size_t count = 0;
    m_root->ForEach([&count](const SceneNode&) {
        ++count;
    });
    return count;
}

// =============================================================================
// Performance Optimizations
// =============================================================================

void Scene::UpdateParallel(float deltaTime, bool useParallel) {
    NOVA_PROFILE_SCOPE("Scene::UpdateParallel");

    if (!m_root) {
        return;
    }

    // Build flat list for cache-efficient iteration
    auto nodes = BuildFlatNodeList();

    if (!useParallel || nodes.size() < 100 || !JobSystem::Instance().IsInitialized()) {
        // Sequential update for small scenes
        for (auto* node : nodes) {
            node->Update(deltaTime);
        }
    } else {
        // Parallel update - nodes must be independent for this to be safe
        // Only update leaf nodes in parallel, then propagate transforms
        JobSystem::Instance().ParallelFor(0, nodes.size(), [&](size_t i) {
            nodes[i]->Update(deltaTime);
        });
    }

    // Mark batch as dirty after update
    m_renderBatchDirty = true;
}

void Scene::RenderBatched(Renderer& renderer) {
    NOVA_PROFILE_SCOPE("Scene::RenderBatched");

    if (m_camera) {
        renderer.SetCamera(m_camera.get());
    }

    if (!m_root) {
        return;
    }

    // Rebuild batch if dirty
    if (m_renderBatchDirty) {
        NOVA_PROFILE_SCOPE("Scene::RebuildBatch");
        CollectRenderBatch(m_renderBatch);
        m_renderBatchDirty = false;
    }

    // Render all nodes using cached batch
    NOVA_PROFILE_SCOPE("Scene::RenderNodes");
    for (size_t i = 0; i < m_renderBatch.nodes.size(); ++i) {
        SceneNode* node = m_renderBatch.nodes[i];
        if (node->HasMesh() && node->HasMaterial()) {
            renderer.DrawMesh(*node->GetMesh(), *node->GetMaterial(), m_renderBatch.transforms[i]);
        }
    }
}

std::vector<SceneNode*> Scene::BuildFlatNodeList() const {
    NOVA_PROFILE_SCOPE("Scene::BuildFlatNodeList");

    std::vector<SceneNode*> nodes;

    if (!m_root) {
        return nodes;
    }

    // Estimate size for better allocation
    nodes.reserve(256);

    // Stack-based traversal (avoids recursion overhead)
    std::vector<SceneNode*> stack;
    stack.reserve(64);
    stack.push_back(m_root.get());

    while (!stack.empty()) {
        SceneNode* node = stack.back();
        stack.pop_back();

        nodes.push_back(node);

        // Add children in reverse order so they're processed in order
        const auto& children = node->GetChildren();
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            stack.push_back(it->get());
        }
    }

    return nodes;
}

void Scene::CollectRenderBatch(RenderBatch& batch) const {
    batch.Clear();

    if (!m_root) {
        return;
    }

    // Estimate capacity
    batch.Reserve(256);

    // Stack-based traversal with visibility culling
    std::vector<SceneNode*> stack;
    stack.reserve(64);
    stack.push_back(m_root.get());

    while (!stack.empty()) {
        SceneNode* node = stack.back();
        stack.pop_back();

        // Skip invisible nodes and their children
        if (!node->IsVisible()) {
            continue;
        }

        // Add renderable nodes to batch
        if (node->HasMesh() && node->HasMaterial()) {
            batch.nodes.push_back(node);
            batch.transforms.push_back(node->GetWorldTransform());
            // Material ID could be used for sorting/batching by material
            batch.materialIds.push_back(0);  // Placeholder
        }

        // Add children
        const auto& children = node->GetChildren();
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            stack.push_back(it->get());
        }
    }

    // Sort by material for better batching (optional)
    // This groups draws by material to reduce state changes
    if (batch.nodes.size() > 1) {
        // Create index array for sorting
        std::vector<size_t> indices(batch.nodes.size());
        for (size_t i = 0; i < indices.size(); ++i) {
            indices[i] = i;
        }

        // Sort indices by material pointer for grouping
        std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
            return batch.nodes[a]->GetMaterial().get() < batch.nodes[b]->GetMaterial().get();
        });

        // Reorder arrays according to sorted indices
        RenderBatch sorted;
        sorted.Reserve(batch.nodes.size());
        for (size_t i : indices) {
            sorted.nodes.push_back(batch.nodes[i]);
            sorted.transforms.push_back(batch.transforms[i]);
            sorted.materialIds.push_back(batch.materialIds[i]);
        }
        batch = std::move(sorted);
    }
}

void Scene::PrecomputeTransforms() {
    NOVA_PROFILE_SCOPE("Scene::PrecomputeTransforms");

    if (!m_root) {
        return;
    }

    // Force world transform computation for all nodes
    // This ensures transforms are ready before rendering
    m_root->ForEach([](SceneNode& node) {
        // Calling GetWorldTransform() triggers lazy evaluation
        (void)node.GetWorldTransform();
    });
}

} // namespace Nova
