#include "PhysicsDebugDraw.hpp"
#include "../graphics/debug/DebugDraw.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>

namespace Nova {

PhysicsDebugDraw::PhysicsDebugDraw() = default;

PhysicsDebugDraw::PhysicsDebugDraw(DebugDraw* debugDraw)
    : m_debugDraw(debugDraw) {
}

void PhysicsDebugDraw::DrawWorld(const PhysicsWorld& world) {
    if (!m_debugDraw) return;

    const auto& bodies = world.GetBodies();

    for (const auto& body : bodies) {
        if (body && body->IsEnabled()) {
            DrawBody(*body);
        }
    }

    // Draw recorded raycasts and queries
    DrawRaycasts();
    DrawQueries();
}

void PhysicsDebugDraw::DrawBody(const CollisionBody& body) {
    DrawBody(body, body.GetTransformMatrix());
}

void PhysicsDebugDraw::DrawBody(const CollisionBody& body, const glm::mat4& transform) {
    if (!m_debugDraw) return;

    glm::vec4 color = GetBodyColor(body);

    // Draw AABB if enabled
    if (m_options.drawAABBs) {
        AABB aabb = body.GetWorldAABB();
        DrawAABB(aabb, m_options.aabbColor);
    }

    // Draw shapes
    if (m_options.drawShapes || m_options.drawOBBs) {
        for (const auto& shape : body.GetShapes()) {
            glm::vec4 shapeColor = color;

            if (shape.IsTrigger() && m_options.drawTriggers) {
                shapeColor = m_options.triggerColor;
            }

            if (m_options.drawShapes) {
                DrawShape(shape, transform, shapeColor);
            }

            if (m_options.drawOBBs) {
                OBB obb = shape.ComputeWorldOBB(transform);
                DrawOBB(obb, shapeColor * 0.7f);
            }
        }
    }

    // Draw velocity
    if (m_options.drawVelocities && body.IsDynamic()) {
        DrawVelocity(body.GetPosition(), body.GetLinearVelocity());
    }

    // Draw center of mass
    if (m_options.drawCenterOfMass) {
        DrawCenterOfMass(body.GetPosition());
    }
}

void PhysicsDebugDraw::DrawShape(const CollisionShape& shape, const glm::mat4& worldTransform,
                                  const glm::vec4& color) {
    if (!m_debugDraw) return;

    ShapeType type = shape.GetType();
    OBB obb = shape.ComputeWorldOBB(worldTransform);

    switch (type) {
        case ShapeType::Box: {
            DrawBox(obb.center, obb.halfExtents, obb.orientation, color);
            break;
        }

        case ShapeType::Sphere: {
            if (auto* params = shape.GetParams<ShapeParams::Sphere>()) {
                DrawSphere(obb.center, params->radius, color);
            }
            break;
        }

        case ShapeType::Capsule: {
            if (auto* params = shape.GetParams<ShapeParams::Capsule>()) {
                DrawCapsule(obb.center, params->radius, params->height,
                           obb.orientation, color);
            }
            break;
        }

        case ShapeType::Cylinder: {
            if (auto* params = shape.GetParams<ShapeParams::Cylinder>()) {
                DrawCylinder(obb.center, params->radius, params->height,
                            obb.orientation, color);
            }
            break;
        }

        case ShapeType::ConvexHull: {
            if (auto* params = shape.GetParams<ShapeParams::ConvexHull>()) {
                glm::mat4 localTransform = shape.GetLocalTransform().ToMatrix();
                glm::mat4 fullTransform = worldTransform * localTransform;
                DrawConvexHull(params->vertices, fullTransform, color);
            }
            break;
        }

        case ShapeType::TriangleMesh: {
            if (auto* params = shape.GetParams<ShapeParams::TriangleMesh>()) {
                glm::mat4 localTransform = shape.GetLocalTransform().ToMatrix();
                glm::mat4 fullTransform = worldTransform * localTransform;

                // Draw triangle edges
                for (size_t i = 0; i + 2 < params->indices.size(); i += 3) {
                    glm::vec3 v0 = glm::vec3(fullTransform * glm::vec4(params->vertices[params->indices[i]], 1.0f));
                    glm::vec3 v1 = glm::vec3(fullTransform * glm::vec4(params->vertices[params->indices[i + 1]], 1.0f));
                    glm::vec3 v2 = glm::vec3(fullTransform * glm::vec4(params->vertices[params->indices[i + 2]], 1.0f));

                    m_debugDraw->AddLine(v0, v1, color);
                    m_debugDraw->AddLine(v1, v2, color);
                    m_debugDraw->AddLine(v2, v0, color);
                }
            }
            break;
        }

        case ShapeType::Compound: {
            if (auto* params = shape.GetParams<ShapeParams::Compound>()) {
                for (const auto& child : params->children) {
                    if (child) {
                        DrawShape(*child, worldTransform, color);
                    }
                }
            }
            break;
        }
    }
}

void PhysicsDebugDraw::DrawBox(const glm::vec3& center, const glm::vec3& halfExtents,
                                const glm::quat& orientation, const glm::vec4& color) {
    if (!m_debugDraw) return;

    glm::mat4 transform = glm::translate(glm::mat4(1.0f), center) * glm::mat4_cast(orientation);
    m_debugDraw->AddBox(transform, halfExtents, color);
}

void PhysicsDebugDraw::DrawSphere(const glm::vec3& center, float radius, const glm::vec4& color) {
    if (!m_debugDraw) return;
    m_debugDraw->AddSphere(center, radius, color);
}

void PhysicsDebugDraw::DrawCapsule(const glm::vec3& center, float radius, float height,
                                    const glm::quat& orientation, const glm::vec4& color) {
    if (!m_debugDraw) return;

    // Calculate capsule endpoints
    glm::vec3 axis = orientation * glm::vec3(0, height * 0.5f, 0);
    glm::vec3 top = center + axis;
    glm::vec3 bottom = center - axis;

    m_debugDraw->AddCapsule(bottom, top, radius, color);
}

void PhysicsDebugDraw::DrawCylinder(const glm::vec3& center, float radius, float height,
                                     const glm::quat& orientation, const glm::vec4& color) {
    if (!m_debugDraw) return;

    // Calculate base position
    glm::vec3 halfAxis = orientation * glm::vec3(0, height * 0.5f, 0);
    glm::vec3 base = center - halfAxis;

    m_debugDraw->AddCylinder(base, height, radius, color);
}

void PhysicsDebugDraw::DrawConvexHull(const std::vector<glm::vec3>& vertices,
                                       const glm::mat4& transform, const glm::vec4& color) {
    if (!m_debugDraw || vertices.size() < 2) return;

    // Transform vertices
    std::vector<glm::vec3> worldVertices;
    worldVertices.reserve(vertices.size());
    for (const auto& v : vertices) {
        worldVertices.push_back(glm::vec3(transform * glm::vec4(v, 1.0f)));
    }

    // Draw edges between all vertex pairs (simple visualization)
    // A proper implementation would compute actual hull edges
    for (size_t i = 0; i < worldVertices.size(); ++i) {
        for (size_t j = i + 1; j < worldVertices.size(); ++j) {
            float dist = glm::distance(worldVertices[i], worldVertices[j]);
            // Only draw edges that are reasonably short (heuristic for hull edges)
            if (dist < 2.0f) {
                m_debugDraw->AddLine(worldVertices[i], worldVertices[j], color);
            }
        }
    }

    // Also draw vertices as points
    for (const auto& v : worldVertices) {
        m_debugDraw->AddPoint(v, 0.02f, color);
    }
}

void PhysicsDebugDraw::DrawAABB(const AABB& aabb, const glm::vec4& color) {
    if (!m_debugDraw) return;
    m_debugDraw->AddAABB(aabb.min, aabb.max, color);
}

void PhysicsDebugDraw::DrawOBB(const OBB& obb, const glm::vec4& color) {
    if (!m_debugDraw) return;

    glm::mat4 transform = glm::translate(glm::mat4(1.0f), obb.center) *
                          glm::mat4_cast(obb.orientation);
    m_debugDraw->AddBox(transform, obb.halfExtents, color);
}

void PhysicsDebugDraw::DrawContactPoint(const glm::vec3& point, const glm::vec3& normal,
                                         float penetration) {
    if (!m_debugDraw) return;

    if (m_options.drawContactPoints) {
        m_debugDraw->AddPoint(point, m_options.contactPointSize, m_options.contactPointColor);
    }

    if (m_options.drawContactNormals) {
        glm::vec3 normalEnd = point + normal * m_options.normalLength;
        m_debugDraw->AddArrow(point, normalEnd, m_options.contactNormalColor);
    }
}

void PhysicsDebugDraw::DrawContacts(const ContactInfo& contact) {
    if (!m_debugDraw) return;

    for (const auto& cp : contact.points) {
        DrawContactPoint(cp.position, cp.normal, cp.penetration);
    }
}

void PhysicsDebugDraw::RecordRaycast(const glm::vec3& origin, const glm::vec3& direction,
                                      float maxDistance, bool hit,
                                      const glm::vec3& hitPoint, const glm::vec3& hitNormal,
                                      float lifetime) {
    DebugRaycast raycast;
    raycast.origin = origin;
    raycast.direction = glm::normalize(direction);
    raycast.maxDistance = maxDistance;
    raycast.hit = hit;
    raycast.hitPoint = hitPoint;
    raycast.hitNormal = hitNormal;
    raycast.lifetime = lifetime;
    m_raycasts.push_back(raycast);
}

void PhysicsDebugDraw::RecordSphereQuery(const glm::vec3& center, float radius,
                                          bool hadResults, float lifetime) {
    DebugQuery query;
    query.type = DebugQuery::Type::Sphere;
    query.center = center;
    query.radius = radius;
    query.hadResults = hadResults;
    query.lifetime = lifetime;
    m_queries.push_back(query);
}

void PhysicsDebugDraw::RecordBoxQuery(const glm::vec3& center, const glm::vec3& halfExtents,
                                       const glm::quat& orientation, bool hadResults,
                                       float lifetime) {
    DebugQuery query;
    query.type = DebugQuery::Type::Box;
    query.center = center;
    query.halfExtents = halfExtents;
    query.orientation = orientation;
    query.hadResults = hadResults;
    query.lifetime = lifetime;
    m_queries.push_back(query);
}

void PhysicsDebugDraw::DrawRaycasts() {
    if (!m_debugDraw) return;

    for (const auto& ray : m_raycasts) {
        glm::vec3 end = ray.origin + ray.direction * ray.maxDistance;

        if (ray.hit) {
            // Draw ray up to hit point in green
            m_debugDraw->AddLine(ray.origin, ray.hitPoint, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
            // Draw rest of ray in red (blocked)
            m_debugDraw->AddLine(ray.hitPoint, end, glm::vec4(1.0f, 0.0f, 0.0f, 0.3f));
            // Draw hit point
            m_debugDraw->AddPoint(ray.hitPoint, 0.05f, glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));
            // Draw hit normal
            m_debugDraw->AddArrow(ray.hitPoint, ray.hitPoint + ray.hitNormal * 0.3f,
                                  glm::vec4(0.0f, 1.0f, 1.0f, 1.0f));
        } else {
            // Draw full ray in gray (no hit)
            m_debugDraw->AddLine(ray.origin, end, glm::vec4(0.5f, 0.5f, 0.5f, 0.5f));
        }
    }
}

void PhysicsDebugDraw::DrawQueries() {
    if (!m_debugDraw) return;

    for (const auto& query : m_queries) {
        glm::vec4 color = query.hadResults ?
            glm::vec4(0.0f, 1.0f, 0.0f, 0.3f) :
            glm::vec4(1.0f, 0.0f, 0.0f, 0.3f);

        switch (query.type) {
            case DebugQuery::Type::Sphere:
                m_debugDraw->AddSphere(query.center, query.radius, color);
                break;

            case DebugQuery::Type::Box: {
                glm::mat4 transform = glm::translate(glm::mat4(1.0f), query.center) *
                                      glm::mat4_cast(query.orientation);
                m_debugDraw->AddBox(transform, query.halfExtents, color);
                break;
            }

            case DebugQuery::Type::AABB:
                m_debugDraw->AddAABB(query.center - query.halfExtents,
                                     query.center + query.halfExtents, color);
                break;
        }
    }
}

void PhysicsDebugDraw::Update(float deltaTime) {
    // Update raycast lifetimes
    for (auto& ray : m_raycasts) {
        ray.lifetime -= deltaTime;
    }

    // Remove expired raycasts
    m_raycasts.erase(
        std::remove_if(m_raycasts.begin(), m_raycasts.end(),
            [](const DebugRaycast& r) { return r.lifetime <= 0.0f; }),
        m_raycasts.end()
    );

    // Update query lifetimes
    for (auto& query : m_queries) {
        query.lifetime -= deltaTime;
    }

    // Remove expired queries
    m_queries.erase(
        std::remove_if(m_queries.begin(), m_queries.end(),
            [](const DebugQuery& q) { return q.lifetime <= 0.0f; }),
        m_queries.end()
    );
}

void PhysicsDebugDraw::ClearRecorded() {
    m_raycasts.clear();
    m_queries.clear();
}

void PhysicsDebugDraw::DrawVelocity(const glm::vec3& position, const glm::vec3& velocity) {
    if (!m_debugDraw) return;

    float speed = glm::length(velocity);
    if (speed < 0.01f) return;

    glm::vec3 end = position + velocity * m_options.velocityScale;
    m_debugDraw->AddArrow(position, end, m_options.velocityColor);
}

void PhysicsDebugDraw::DrawCenterOfMass(const glm::vec3& position) {
    if (!m_debugDraw) return;

    // Draw a small cross at center of mass
    float size = 0.1f;
    glm::vec4 color(1.0f, 0.0f, 1.0f, 1.0f);

    m_debugDraw->AddLine(position - glm::vec3(size, 0, 0), position + glm::vec3(size, 0, 0), color);
    m_debugDraw->AddLine(position - glm::vec3(0, size, 0), position + glm::vec3(0, size, 0), color);
    m_debugDraw->AddLine(position - glm::vec3(0, 0, size), position + glm::vec3(0, 0, size), color);
}

void PhysicsDebugDraw::DrawTransform(const glm::mat4& transform, float size) {
    if (!m_debugDraw) return;
    m_debugDraw->AddTransform(transform, size);
}

glm::vec4 PhysicsDebugDraw::GetBodyColor(const CollisionBody& body) const {
    if (body.IsSleeping() && m_options.drawSleepState) {
        return m_options.sleepingColor;
    }

    if (m_options.drawBodyType) {
        switch (body.GetBodyType()) {
            case BodyType::Static:
                return m_options.staticColor;
            case BodyType::Kinematic:
                return m_options.kinematicColor;
            case BodyType::Dynamic:
                return m_options.dynamicColor;
        }
    }

    return m_options.dynamicColor;
}

} // namespace Nova
