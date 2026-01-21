#include "ScriptBindings.hpp"
#include "ScriptContext.hpp"
#include "EventDispatcher.hpp"
#include "AIBehavior.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <pybind11/operators.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <game/src/entities/Entity.hpp>

#include <cmath>
#include <string>

namespace py = pybind11;

namespace Nova {
namespace Scripting {

// ============================================================================
// Script Proxy Types (must be defined before pybind11 bindings)
// ============================================================================

/**
 * @brief Transform data for Python bindings
 * Updated to match Entity.hpp API which uses Y-axis rotation for top-down games
 */
struct ScriptTransform {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
    glm::vec3 velocity{0.0f};
    float yRotation = 0.0f;  // Rotation around Y axis in radians (for top-down)
    float moveSpeed = 5.0f;  // Movement speed multiplier
};

/**
 * @brief Entity proxy for Python bindings
 */
struct EntityProxy {
    uint32_t id = 0;
};

/**
 * @brief Resource type enum for Python bindings
 */
enum class ScriptResourceType {
    Food = 0, Wood, Stone, Metal, Coins, Fuel, Medicine, Ammunition
};

/**
 * @brief Building type enum for Python bindings
 */
enum class ScriptBuildingType {
    Shelter = 0, House, Barracks, Farm, LumberMill, Quarry, Workshop,
    WatchTower, Wall, Gate, Fortress, TradingPost, Hospital, Warehouse, CommandCenter
};

// Static context pointer
ScriptContext* ScriptBindings::s_context = nullptr;

void ScriptBindings::SetContext(ScriptContext* context) {
    s_context = context;
}

ScriptContext* ScriptBindings::GetContext() {
    return s_context;
}

// ============================================================================
// Module Definition
// ============================================================================

PYBIND11_EMBEDDED_MODULE(nova_engine, m) {
    m.doc() = "Nova3D Engine Python Bindings";

    ScriptBindings::RegisterMathTypes(m);
    ScriptBindings::RegisterEntityTypes(m);
    ScriptBindings::RegisterRTSTypes(m);
    ScriptBindings::RegisterWorldQueries(m);
    ScriptBindings::RegisterUIFunctions(m);
    ScriptBindings::RegisterAITypes(m);
    ScriptBindings::RegisterEventTypes(m);
}

// ============================================================================
// Registration Functions
// ============================================================================

void ScriptBindings::RegisterAll() {
    // The PYBIND11_EMBEDDED_MODULE macro automatically registers the module
    // when the Python interpreter is initialized. Nothing else needed here.
}

void ScriptBindings::RegisterMathTypes(pybind11::module_& m) {
    // Vector2
    py::class_<glm::vec2>(m, "Vector2")
        .def(py::init<>())
        .def(py::init<float, float>())
        .def(py::init<float>())
        .def_readwrite("x", &glm::vec2::x)
        .def_readwrite("y", &glm::vec2::y)
        .def("__repr__", [](const glm::vec2& v) {
            return "Vector2(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ")";
        })
        .def("__add__", [](const glm::vec2& a, const glm::vec2& b) { return a + b; })
        .def("__sub__", [](const glm::vec2& a, const glm::vec2& b) { return a - b; })
        .def("__mul__", [](const glm::vec2& v, float s) { return v * s; })
        .def("__truediv__", [](const glm::vec2& v, float s) { return v / s; })
        .def("length", [](const glm::vec2& v) { return glm::length(v); })
        .def("normalized", [](const glm::vec2& v) { return glm::normalize(v); })
        .def("dot", [](const glm::vec2& a, const glm::vec2& b) { return glm::dot(a, b); })
        .def_static("zero", []() { return glm::vec2(0.0f); })
        .def_static("one", []() { return glm::vec2(1.0f); });

    // Vector3
    py::class_<glm::vec3>(m, "Vector3")
        .def(py::init<>())
        .def(py::init<float, float, float>())
        .def(py::init<float>())
        .def_readwrite("x", &glm::vec3::x)
        .def_readwrite("y", &glm::vec3::y)
        .def_readwrite("z", &glm::vec3::z)
        .def("__repr__", [](const glm::vec3& v) {
            return "Vector3(" + std::to_string(v.x) + ", " +
                   std::to_string(v.y) + ", " + std::to_string(v.z) + ")";
        })
        .def("__add__", [](const glm::vec3& a, const glm::vec3& b) { return a + b; })
        .def("__sub__", [](const glm::vec3& a, const glm::vec3& b) { return a - b; })
        .def("__mul__", [](const glm::vec3& v, float s) { return v * s; })
        .def("__truediv__", [](const glm::vec3& v, float s) { return v / s; })
        .def("__neg__", [](const glm::vec3& v) { return -v; })
        .def("length", [](const glm::vec3& v) { return glm::length(v); })
        .def("length_squared", [](const glm::vec3& v) { return glm::dot(v, v); })
        .def("normalized", [](const glm::vec3& v) {
            float len = glm::length(v);
            return len > 0.0001f ? v / len : glm::vec3(0.0f);
        })
        .def("dot", [](const glm::vec3& a, const glm::vec3& b) { return glm::dot(a, b); })
        .def("cross", [](const glm::vec3& a, const glm::vec3& b) { return glm::cross(a, b); })
        .def("distance_to", [](const glm::vec3& a, const glm::vec3& b) {
            return glm::length(b - a);
        })
        .def_static("zero", []() { return glm::vec3(0.0f); })
        .def_static("one", []() { return glm::vec3(1.0f); })
        .def_static("up", []() { return glm::vec3(0.0f, 1.0f, 0.0f); })
        .def_static("down", []() { return glm::vec3(0.0f, -1.0f, 0.0f); })
        .def_static("forward", []() { return glm::vec3(0.0f, 0.0f, 1.0f); })
        .def_static("right", []() { return glm::vec3(1.0f, 0.0f, 0.0f); })
        .def_static("lerp", [](const glm::vec3& a, const glm::vec3& b, float t) {
            return glm::mix(a, b, t);
        });

    // Vector4
    py::class_<glm::vec4>(m, "Vector4")
        .def(py::init<>())
        .def(py::init<float, float, float, float>())
        .def(py::init<glm::vec3, float>())
        .def_readwrite("x", &glm::vec4::x)
        .def_readwrite("y", &glm::vec4::y)
        .def_readwrite("z", &glm::vec4::z)
        .def_readwrite("w", &glm::vec4::w)
        .def("__repr__", [](const glm::vec4& v) {
            return "Vector4(" + std::to_string(v.x) + ", " + std::to_string(v.y) +
                   ", " + std::to_string(v.z) + ", " + std::to_string(v.w) + ")";
        })
        .def("xyz", [](const glm::vec4& v) { return glm::vec3(v); });

    // Quaternion
    py::class_<glm::quat>(m, "Quaternion")
        .def(py::init<>())
        .def(py::init<float, float, float, float>())
        .def_readwrite("x", &glm::quat::x)
        .def_readwrite("y", &glm::quat::y)
        .def_readwrite("z", &glm::quat::z)
        .def_readwrite("w", &glm::quat::w)
        .def("__repr__", [](const glm::quat& q) {
            return "Quaternion(" + std::to_string(q.x) + ", " + std::to_string(q.y) +
                   ", " + std::to_string(q.z) + ", " + std::to_string(q.w) + ")";
        })
        .def("__mul__", [](const glm::quat& a, const glm::quat& b) { return a * b; })
        .def("rotate_vector", [](const glm::quat& q, const glm::vec3& v) {
            return q * v;
        })
        .def("normalized", [](const glm::quat& q) { return glm::normalize(q); })
        .def("inverse", [](const glm::quat& q) { return glm::inverse(q); })
        .def_static("identity", []() { return glm::quat(1.0f, 0.0f, 0.0f, 0.0f); })
        .def_static("from_euler", [](float pitch, float yaw, float roll) {
            return glm::quat(glm::vec3(pitch, yaw, roll));
        })
        .def_static("from_axis_angle", [](const glm::vec3& axis, float angle) {
            return glm::angleAxis(angle, glm::normalize(axis));
        })
        .def_static("look_rotation", [](const glm::vec3& forward, const glm::vec3& up) {
            return glm::quatLookAt(glm::normalize(forward), up);
        })
        .def_static("slerp", [](const glm::quat& a, const glm::quat& b, float t) {
            return glm::slerp(a, b, t);
        });

    // Transform class (composite of position, rotation, scale, velocity)
    // Updated to match Entity.hpp API for top-down games
    py::class_<ScriptTransform>(m, "Transform")
        .def(py::init<>())
        .def(py::init([](const glm::vec3& pos) {
            ScriptTransform t;
            t.position = pos;
            return t;
        }))
        .def(py::init([](const glm::vec3& pos, float yRotation) {
            ScriptTransform t;
            t.position = pos;
            t.yRotation = yRotation;
            return t;
        }))
        .def_readwrite("position", &ScriptTransform::position)
        .def_readwrite("rotation", &ScriptTransform::rotation)
        .def_readwrite("scale", &ScriptTransform::scale)
        .def_readwrite("velocity", &ScriptTransform::velocity)
        .def_readwrite("y_rotation", &ScriptTransform::yRotation)
        .def_readwrite("move_speed", &ScriptTransform::moveSpeed)
        .def("forward", [](const ScriptTransform& t) {
            // For quaternion-based rotation
            return t.rotation * glm::vec3(0.0f, 0.0f, 1.0f);
        })
        .def("forward_2d", [](const ScriptTransform& t) {
            // For Y-axis rotation (top-down games) - matches Entity.hpp GetForward()
            return glm::vec3(std::sin(t.yRotation), 0.0f, std::cos(t.yRotation));
        })
        .def("right", [](const ScriptTransform& t) {
            return t.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
        })
        .def("right_2d", [](const ScriptTransform& t) {
            // For Y-axis rotation (top-down games) - matches Entity.hpp GetRight()
            return glm::vec3(std::cos(t.yRotation), 0.0f, -std::sin(t.yRotation));
        })
        .def("up", [](const ScriptTransform& t) {
            return t.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
        })
        .def("get_speed", [](const ScriptTransform& t) {
            return glm::length(t.velocity);
        })
        .def("get_position_2d", [](const ScriptTransform& t) {
            return glm::vec2(t.position.x, t.position.z);
        })
        .def("set_position_2d", [](ScriptTransform& t, float x, float z) {
            t.position.x = x;
            t.position.z = z;
        })
        .def("set_velocity_2d", [](ScriptTransform& t, float vx, float vz) {
            t.velocity = glm::vec3(vx, 0.0f, vz);
        })
        .def("look_at_2d", [](ScriptTransform& t, float x, float z) {
            float dx = x - t.position.x;
            float dz = z - t.position.z;
            t.yRotation = std::atan2(dx, dz);
        })
        .def("look_at", [](ScriptTransform& t, const glm::vec3& target) {
            glm::vec3 dir = glm::normalize(target - t.position);
            t.yRotation = std::atan2(dir.x, dir.z);
        })
        .def("translate", [](ScriptTransform& t, const glm::vec3& offset) {
            t.position += offset;
        })
        .def("translate_local", [](ScriptTransform& t, const glm::vec3& offset) {
            // Move in local space using Y rotation
            glm::vec3 forward(std::sin(t.yRotation), 0.0f, std::cos(t.yRotation));
            glm::vec3 right(std::cos(t.yRotation), 0.0f, -std::sin(t.yRotation));
            glm::vec3 up(0.0f, 1.0f, 0.0f);
            t.position += forward * offset.z + right * offset.x + up * offset.y;
        })
        .def("rotate_y", [](ScriptTransform& t, float radians) {
            t.yRotation += radians;
        });
}

void ScriptBindings::RegisterEntityTypes(pybind11::module_& m) {
    // Entity type enum - using the actual enum from Entity.hpp
    py::enum_<Vehement::EntityType>(m, "EntityType")
        .value("None", Vehement::EntityType::None)
        .value("Player", Vehement::EntityType::Player)
        .value("Zombie", Vehement::EntityType::Zombie)
        .value("NPC", Vehement::EntityType::NPC)
        .value("Projectile", Vehement::EntityType::Projectile)
        .value("Pickup", Vehement::EntityType::Pickup)
        .value("Effect", Vehement::EntityType::Effect)
        .export_values();

    // Entity proxy class for Python - Updated to match Entity.hpp API
    py::class_<EntityProxy>(m, "Entity")
        .def_readonly("id", &EntityProxy::id)
        // Position properties
        .def_property("position",
            [](const EntityProxy& e) -> glm::vec3 {
                if (auto* ctx = ScriptBindings::GetContext()) {
                    return ctx->GetEntityPosition(e.id);
                }
                return glm::vec3(0.0f);
            },
            [](EntityProxy& e, const glm::vec3& pos) {
                if (auto* ctx = ScriptBindings::GetContext()) {
                    ctx->SetEntityPosition(e.id, pos.x, pos.y, pos.z);
                }
            })
        .def("get_position_2d", [](const EntityProxy& e) -> glm::vec2 {
            if (auto* ctx = ScriptBindings::GetContext()) {
                auto pos = ctx->GetEntityPosition(e.id);
                return glm::vec2(pos.x, pos.z);
            }
            return glm::vec2(0.0f);
        }, "Get 2D position (XZ plane) for top-down games")
        .def("set_position_2d", [](EntityProxy& e, float x, float z) {
            if (auto* ctx = ScriptBindings::GetContext()) {
                auto pos = ctx->GetEntityPosition(e.id);
                ctx->SetEntityPosition(e.id, x, pos.y, z);
            }
        }, "Set 2D position (XZ plane), preserving Y", py::arg("x"), py::arg("z"))
        // Velocity property (via ScriptContext extension)
        .def_property("velocity",
            [](const EntityProxy& e) -> glm::vec3 {
                if (auto* ctx = ScriptBindings::GetContext()) {
                    return ctx->GetEntityVelocity(e.id);
                }
                return glm::vec3(0.0f);
            },
            [](EntityProxy& e, const glm::vec3& vel) {
                if (auto* ctx = ScriptBindings::GetContext()) {
                    ctx->SetEntityVelocity(e.id, vel.x, vel.y, vel.z);
                }
            })
        .def("set_velocity_2d", [](EntityProxy& e, float vx, float vz) {
            if (auto* ctx = ScriptBindings::GetContext()) {
                ctx->SetEntityVelocity(e.id, vx, 0.0f, vz);
            }
        }, "Set velocity for 2D movement", py::arg("vx"), py::arg("vz"))
        // Rotation property (Y-axis rotation for top-down)
        .def_property("rotation",
            [](const EntityProxy& e) -> float {
                if (auto* ctx = ScriptBindings::GetContext()) {
                    return ctx->GetEntityRotation(e.id);
                }
                return 0.0f;
            },
            [](EntityProxy& e, float radians) {
                if (auto* ctx = ScriptBindings::GetContext()) {
                    ctx->SetEntityRotation(e.id, radians);
                }
            })
        // Direction helpers
        .def("get_forward", [](const EntityProxy& e) -> glm::vec3 {
            if (auto* ctx = ScriptBindings::GetContext()) {
                float rot = ctx->GetEntityRotation(e.id);
                return glm::vec3(std::sin(rot), 0.0f, std::cos(rot));
            }
            return glm::vec3(0.0f, 0.0f, 1.0f);
        }, "Get forward direction vector (XZ plane)")
        .def("get_right", [](const EntityProxy& e) -> glm::vec3 {
            if (auto* ctx = ScriptBindings::GetContext()) {
                float rot = ctx->GetEntityRotation(e.id);
                return glm::vec3(std::cos(rot), 0.0f, -std::sin(rot));
            }
            return glm::vec3(1.0f, 0.0f, 0.0f);
        }, "Get right direction vector (XZ plane)")
        .def("look_at", [](EntityProxy& e, const glm::vec3& target) {
            if (auto* ctx = ScriptBindings::GetContext()) {
                auto pos = ctx->GetEntityPosition(e.id);
                float dx = target.x - pos.x;
                float dz = target.z - pos.z;
                ctx->SetEntityRotation(e.id, std::atan2(dx, dz));
            }
        }, "Rotate to face a target position", py::arg("target"))
        .def("look_at_2d", [](EntityProxy& e, float x, float z) {
            if (auto* ctx = ScriptBindings::GetContext()) {
                auto pos = ctx->GetEntityPosition(e.id);
                float dx = x - pos.x;
                float dz = z - pos.z;
                ctx->SetEntityRotation(e.id, std::atan2(dx, dz));
            }
        }, "Rotate to face a 2D position", py::arg("x"), py::arg("z"))
        // Health properties
        .def_property("health",
            [](const EntityProxy& e) -> float {
                if (auto* ctx = ScriptBindings::GetContext()) {
                    return ctx->GetEntityHealth(e.id);
                }
                return 0.0f;
            },
            [](EntityProxy& e, float health) {
                if (auto* ctx = ScriptBindings::GetContext()) {
                    ctx->SetEntityHealth(e.id, health);
                }
            })
        .def_property("max_health",
            [](const EntityProxy& e) -> float {
                if (auto* ctx = ScriptBindings::GetContext()) {
                    return ctx->GetEntityMaxHealth(e.id);
                }
                return 0.0f;
            },
            [](EntityProxy& e, float maxHealth) {
                if (auto* ctx = ScriptBindings::GetContext()) {
                    ctx->SetEntityMaxHealth(e.id, maxHealth);
                }
            })
        .def("get_health_percent", [](const EntityProxy& e) -> float {
            if (auto* ctx = ScriptBindings::GetContext()) {
                float health = ctx->GetEntityHealth(e.id);
                float maxHealth = ctx->GetEntityMaxHealth(e.id);
                return maxHealth > 0.0f ? health / maxHealth : 0.0f;
            }
            return 0.0f;
        }, "Get health as percentage [0, 1]")
        // Movement speed
        .def_property("move_speed",
            [](const EntityProxy& e) -> float {
                if (auto* ctx = ScriptBindings::GetContext()) {
                    return ctx->GetEntityMoveSpeed(e.id);
                }
                return 5.0f;
            },
            [](EntityProxy& e, float speed) {
                if (auto* ctx = ScriptBindings::GetContext()) {
                    ctx->SetEntityMoveSpeed(e.id, speed);
                }
            })
        // Collision properties
        .def_property("collision_radius",
            [](const EntityProxy& e) -> float {
                if (auto* ctx = ScriptBindings::GetContext()) {
                    return ctx->GetEntityCollisionRadius(e.id);
                }
                return 0.5f;
            },
            [](EntityProxy& e, float radius) {
                if (auto* ctx = ScriptBindings::GetContext()) {
                    ctx->SetEntityCollisionRadius(e.id, radius);
                }
            })
        .def_property("collidable",
            [](const EntityProxy& e) -> bool {
                if (auto* ctx = ScriptBindings::GetContext()) {
                    return ctx->IsEntityCollidable(e.id);
                }
                return true;
            },
            [](EntityProxy& e, bool collidable) {
                if (auto* ctx = ScriptBindings::GetContext()) {
                    ctx->SetEntityCollidable(e.id, collidable);
                }
            })
        // State properties
        .def_property_readonly("type", [](const EntityProxy& e) -> std::string {
            if (auto* ctx = ScriptBindings::GetContext()) {
                return ctx->GetEntityType(e.id);
            }
            return "";
        })
        .def_property_readonly("name", [](const EntityProxy& e) -> std::string {
            if (auto* ctx = ScriptBindings::GetContext()) {
                return ctx->GetEntityName(e.id);
            }
            return "";
        })
        .def_property_readonly("is_alive", [](const EntityProxy& e) -> bool {
            if (auto* ctx = ScriptBindings::GetContext()) {
                return ctx->IsEntityAlive(e.id);
            }
            return false;
        })
        .def_property("active",
            [](const EntityProxy& e) -> bool {
                if (auto* ctx = ScriptBindings::GetContext()) {
                    return ctx->IsEntityActive(e.id);
                }
                return true;
            },
            [](EntityProxy& e, bool active) {
                if (auto* ctx = ScriptBindings::GetContext()) {
                    ctx->SetEntityActive(e.id, active);
                }
            })
        // Actions
        .def("damage", [](EntityProxy& e, float amount, uint32_t source) {
            if (auto* ctx = ScriptBindings::GetContext()) {
                ctx->DamageEntity(e.id, amount, source);
            }
        }, "Apply damage to entity", py::arg("amount"), py::arg("source") = 0)
        .def("heal", [](EntityProxy& e, float amount) {
            if (auto* ctx = ScriptBindings::GetContext()) {
                ctx->HealEntity(e.id, amount);
            }
        }, "Heal entity by amount")
        .def("kill", [](EntityProxy& e) {
            if (auto* ctx = ScriptBindings::GetContext()) {
                ctx->KillEntity(e.id);
            }
        }, "Kill the entity immediately")
        .def("remove", [](EntityProxy& e) {
            if (auto* ctx = ScriptBindings::GetContext()) {
                ctx->DespawnEntity(e.id);
            }
        }, "Mark entity for removal")
        // Distance methods
        .def("distance_to", [](const EntityProxy& e, const EntityProxy& other) -> float {
            if (auto* ctx = ScriptBindings::GetContext()) {
                return ctx->GetDistance(e.id, other.id);
            }
            return 0.0f;
        }, "Get distance to another entity")
        .def("distance_to_point", [](const EntityProxy& e, const glm::vec3& point) -> float {
            if (auto* ctx = ScriptBindings::GetContext()) {
                auto pos = ctx->GetEntityPosition(e.id);
                return glm::length(point - pos);
            }
            return 0.0f;
        }, "Get distance to a point")
        .def("collides_with", [](const EntityProxy& e, const EntityProxy& other) -> bool {
            if (auto* ctx = ScriptBindings::GetContext()) {
                return ctx->EntitiesCollide(e.id, other.id);
            }
            return false;
        }, "Check if entities are colliding");

    // Entity factory functions
    m.def("get_entity", [](uint32_t id) -> EntityProxy {
        return EntityProxy{id};
    }, "Get an entity by ID");

    m.def("spawn_entity", [](const std::string& type, float x, float y, float z) -> uint32_t {
        if (auto* ctx = ScriptBindings::GetContext()) {
            return ctx->SpawnEntity(type, x, y, z);
        }
        return 0;
    }, "Spawn a new entity", py::arg("type"), py::arg("x"), py::arg("y"), py::arg("z"));

    m.def("despawn_entity", [](uint32_t id) {
        if (auto* ctx = ScriptBindings::GetContext()) {
            ctx->DespawnEntity(id);
        }
    }, "Remove an entity");
}

void ScriptBindings::RegisterRTSTypes(pybind11::module_& m) {
    // Resource type enum
    py::enum_<ScriptResourceType>(m, "ResourceType")
        .value("Food", ScriptResourceType::Food)
        .value("Wood", ScriptResourceType::Wood)
        .value("Stone", ScriptResourceType::Stone)
        .value("Metal", ScriptResourceType::Metal)
        .value("Coins", ScriptResourceType::Coins)
        .value("Fuel", ScriptResourceType::Fuel)
        .value("Medicine", ScriptResourceType::Medicine)
        .value("Ammunition", ScriptResourceType::Ammunition)
        .export_values();

    // Building type enum
    py::enum_<ScriptBuildingType>(m, "BuildingType")
        .value("Shelter", ScriptBuildingType::Shelter)
        .value("House", ScriptBuildingType::House)
        .value("Barracks", ScriptBuildingType::Barracks)
        .value("Farm", ScriptBuildingType::Farm)
        .value("LumberMill", ScriptBuildingType::LumberMill)
        .value("Quarry", ScriptBuildingType::Quarry)
        .value("Workshop", ScriptBuildingType::Workshop)
        .value("WatchTower", ScriptBuildingType::WatchTower)
        .value("Wall", ScriptBuildingType::Wall)
        .value("Gate", ScriptBuildingType::Gate)
        .value("Fortress", ScriptBuildingType::Fortress)
        .value("TradingPost", ScriptBuildingType::TradingPost)
        .value("Hospital", ScriptBuildingType::Hospital)
        .value("Warehouse", ScriptBuildingType::Warehouse)
        .value("CommandCenter", ScriptBuildingType::CommandCenter)
        .export_values();

    // Resource functions
    m.def("get_resource", [](const std::string& type) -> int {
        if (auto* ctx = ScriptBindings::GetContext()) {
            return ctx->GetResourceAmount(type);
        }
        return 0;
    }, "Get current amount of a resource");

    m.def("add_resource", [](const std::string& type, int amount) -> bool {
        if (auto* ctx = ScriptBindings::GetContext()) {
            return ctx->AddResource(type, amount);
        }
        return false;
    }, "Add resources to stockpile");

    m.def("remove_resource", [](const std::string& type, int amount) -> bool {
        if (auto* ctx = ScriptBindings::GetContext()) {
            return ctx->RemoveResource(type, amount);
        }
        return false;
    }, "Remove resources from stockpile");

    m.def("can_afford", [](const std::string& type, int amount) -> bool {
        if (auto* ctx = ScriptBindings::GetContext()) {
            return ctx->CanAfford(type, amount);
        }
        return false;
    }, "Check if can afford a resource cost");

    // Building functions
    m.def("get_building_at", [](int tileX, int tileY) -> uint32_t {
        if (auto* ctx = ScriptBindings::GetContext()) {
            return ctx->GetBuildingAt(tileX, tileY);
        }
        return 0;
    }, "Get building ID at tile position");

    m.def("is_building_operational", [](uint32_t buildingId) -> bool {
        if (auto* ctx = ScriptBindings::GetContext()) {
            return ctx->IsBuildingOperational(buildingId);
        }
        return false;
    }, "Check if building is operational");
}

void ScriptBindings::RegisterWorldQueries(pybind11::module_& m) {
    // Find entities in radius
    m.def("find_entities_in_radius", [](float x, float y, float z, float radius) -> py::list {
        py::list result;
        if (auto* ctx = ScriptBindings::GetContext()) {
            auto entities = ctx->FindEntitiesInRadius(x, y, z, radius);
            for (uint32_t id : entities) {
                result.append(id);
            }
        }
        return result;
    }, "Find all entities within radius of a point");

    m.def("find_entities_in_radius_vec", [](const glm::vec3& pos, float radius) -> py::list {
        py::list result;
        if (auto* ctx = ScriptBindings::GetContext()) {
            auto entities = ctx->FindEntitiesInRadius(pos.x, pos.y, pos.z, radius);
            for (uint32_t id : entities) {
                result.append(id);
            }
        }
        return result;
    }, "Find all entities within radius of a Vector3 position");

    m.def("get_nearest_entity", [](float x, float y, float z, const std::string& type) -> uint32_t {
        if (auto* ctx = ScriptBindings::GetContext()) {
            return ctx->GetNearestEntity(x, y, z, type);
        }
        return 0;
    }, "Get nearest entity, optionally filtered by type",
       py::arg("x"), py::arg("y"), py::arg("z"), py::arg("type") = "");

    m.def("get_distance", [](uint32_t entity1, uint32_t entity2) -> float {
        if (auto* ctx = ScriptBindings::GetContext()) {
            return ctx->GetDistance(entity1, entity2);
        }
        return 0.0f;
    }, "Get distance between two entities");

    // Raycast result class
    py::class_<ScriptContext::RaycastResult>(m, "RaycastResult")
        .def_readonly("hit", &ScriptContext::RaycastResult::hit)
        .def_readonly("entity_id", &ScriptContext::RaycastResult::entityId)
        .def_readonly("hit_point", &ScriptContext::RaycastResult::hitPoint)
        .def_readonly("hit_normal", &ScriptContext::RaycastResult::hitNormal)
        .def_readonly("distance", &ScriptContext::RaycastResult::distance);

    m.def("raycast", [](float startX, float startY, float startZ,
                        float dirX, float dirY, float dirZ,
                        float maxDistance) -> ScriptContext::RaycastResult {
        if (auto* ctx = ScriptBindings::GetContext()) {
            return ctx->Raycast(startX, startY, startZ, dirX, dirY, dirZ, maxDistance);
        }
        return ScriptContext::RaycastResult{};
    }, "Cast a ray and get hit information",
       py::arg("start_x"), py::arg("start_y"), py::arg("start_z"),
       py::arg("dir_x"), py::arg("dir_y"), py::arg("dir_z"),
       py::arg("max_distance") = 1000.0f);
}

void ScriptBindings::RegisterUIFunctions(pybind11::module_& m) {
    m.def("show_notification", [](const std::string& message, float duration) {
        if (auto* ctx = ScriptBindings::GetContext()) {
            ctx->ShowNotification(message, duration);
        }
    }, "Show a notification to the player",
       py::arg("message"), py::arg("duration") = 3.0f);

    m.def("show_warning", [](const std::string& message) {
        if (auto* ctx = ScriptBindings::GetContext()) {
            ctx->ShowWarning(message);
        }
    }, "Show a warning message");

    m.def("show_error", [](const std::string& message) {
        if (auto* ctx = ScriptBindings::GetContext()) {
            ctx->ShowError(message);
        }
    }, "Show an error message");

    // =========== Sound and Audio Functions ===========
    m.def("play_sound", [](const std::string& name, float x, float y, float z) {
        if (auto* ctx = ScriptBindings::GetContext()) {
            ctx->PlaySound(name, x, y, z);
        }
    }, "Play a sound effect at optional position",
       py::arg("name"), py::arg("x") = 0.0f, py::arg("y") = 0.0f, py::arg("z") = 0.0f);

    m.def("play_sound_3d", [](const std::string& name, float x, float y, float z, float volume) {
        if (auto* ctx = ScriptBindings::GetContext()) {
            ctx->PlaySound3D(name, x, y, z, volume);
        }
    }, "Play a 3D positional sound",
       py::arg("name"), py::arg("x"), py::arg("y"), py::arg("z"), py::arg("volume") = 1.0f);

    m.def("play_sound_2d", [](const std::string& name, float volume, float pitch) {
        if (auto* ctx = ScriptBindings::GetContext()) {
            ctx->PlaySound2D(name, volume, pitch);
        }
    }, "Play a 2D sound (UI, global)",
       py::arg("name"), py::arg("volume") = 1.0f, py::arg("pitch") = 1.0f);

    m.def("play_music", [](const std::string& name) {
        if (auto* ctx = ScriptBindings::GetContext()) {
            ctx->PlayMusic(name);
        }
    }, "Play background music (streaming)");

    m.def("stop_music", []() {
        if (auto* ctx = ScriptBindings::GetContext()) {
            ctx->StopMusic();
        }
    }, "Stop background music");

    m.def("set_music_volume", [](float volume) {
        if (auto* ctx = ScriptBindings::GetContext()) {
            ctx->SetMusicVolume(volume);
        }
    }, "Set music volume (0.0 to 1.0)");

    m.def("set_master_volume", [](float volume) {
        if (auto* ctx = ScriptBindings::GetContext()) {
            ctx->SetMasterVolume(volume);
        }
    }, "Set master volume (0.0 to 1.0)");

    m.def("get_master_volume", []() -> float {
        if (auto* ctx = ScriptBindings::GetContext()) {
            return ctx->GetMasterVolume();
        }
        return 1.0f;
    }, "Get master volume");

    m.def("set_sound_volume", [](const std::string& category, float volume) {
        if (auto* ctx = ScriptBindings::GetContext()) {
            ctx->SetSoundVolume(category, volume);
        }
    }, "Set volume for a sound category/bus",
       py::arg("category"), py::arg("volume"));

    // =========== Visual Effects ===========
    m.def("spawn_effect", [](const std::string& name, float x, float y, float z) {
        if (auto* ctx = ScriptBindings::GetContext()) {
            ctx->SpawnEffect(name, x, y, z);
        }
    }, "Spawn a visual effect");

    m.def("spawn_particles", [](const std::string& type, float x, float y, float z, int count) {
        if (auto* ctx = ScriptBindings::GetContext()) {
            ctx->SpawnParticles(type, x, y, z, count);
        }
    }, "Spawn particles at a position");
}

void ScriptBindings::RegisterAITypes(pybind11::module_& m) {
    // Behavior status enum
    py::enum_<BehaviorStatus>(m, "BehaviorStatus")
        .value("Running", BehaviorStatus::Running)
        .value("Success", BehaviorStatus::Success)
        .value("Failure", BehaviorStatus::Failure)
        .value("Invalid", BehaviorStatus::Invalid)
        .export_values();

    // Blackboard for AI state sharing
    py::class_<Blackboard>(m, "Blackboard")
        .def(py::init<>())
        .def("set_int", [](Blackboard& b, const std::string& key, int value) {
            b.SetValue(key, value);
        })
        .def("set_float", [](Blackboard& b, const std::string& key, float value) {
            b.SetValue(key, value);
        })
        .def("set_bool", [](Blackboard& b, const std::string& key, bool value) {
            b.SetValue(key, value);
        })
        .def("set_string", [](Blackboard& b, const std::string& key, const std::string& value) {
            b.SetValue(key, value);
        })
        .def("set_vec3", [](Blackboard& b, const std::string& key, const glm::vec3& value) {
            b.SetValue(key, value);
        })
        .def("set_entity", [](Blackboard& b, const std::string& key, uint32_t entityId) {
            b.SetValue(key, entityId);
        })
        .def("get_int", [](const Blackboard& b, const std::string& key) -> py::object {
            auto val = b.GetValue<int>(key);
            return val ? py::cast(*val) : py::none();
        })
        .def("get_float", [](const Blackboard& b, const std::string& key) -> py::object {
            auto val = b.GetValue<float>(key);
            return val ? py::cast(*val) : py::none();
        })
        .def("get_bool", [](const Blackboard& b, const std::string& key) -> py::object {
            auto val = b.GetValue<bool>(key);
            return val ? py::cast(*val) : py::none();
        })
        .def("get_string", [](const Blackboard& b, const std::string& key) -> py::object {
            auto val = b.GetValue<std::string>(key);
            return val ? py::cast(*val) : py::none();
        })
        .def("get_vec3", [](const Blackboard& b, const std::string& key) -> py::object {
            auto val = b.GetValue<glm::vec3>(key);
            return val ? py::cast(*val) : py::none();
        })
        .def("get_entity", [](const Blackboard& b, const std::string& key) -> py::object {
            auto val = b.GetValue<uint32_t>(key);
            return val ? py::cast(*val) : py::none();
        })
        .def("has", &Blackboard::Has)
        .def("remove", &Blackboard::Remove)
        .def("clear", &Blackboard::Clear)
        .def("get_keys", &Blackboard::GetKeys)
        .def_property("target_entity",
            [](const Blackboard& b) -> py::object {
                auto val = b.GetTargetEntity();
                return val ? py::cast(*val) : py::none();
            },
            [](Blackboard& b, uint32_t id) { b.SetTargetEntity(id); })
        .def_property("target_position",
            [](const Blackboard& b) -> py::object {
                auto val = b.GetTargetPosition();
                return val ? py::cast(*val) : py::none();
            },
            [](Blackboard& b, const glm::vec3& pos) { b.SetTargetPosition(pos); });
}

void ScriptBindings::RegisterEventTypes(pybind11::module_& m) {
    // Event type enum
    py::enum_<EventType>(m, "EventType")
        .value("EntitySpawn", EventType::EntitySpawn)
        .value("EntityDeath", EventType::EntityDeath)
        .value("EntityDamaged", EventType::EntityDamaged)
        .value("EntityHealed", EventType::EntityHealed)
        .value("TileEnter", EventType::TileEnter)
        .value("TileExit", EventType::TileExit)
        .value("BuildingPlaced", EventType::BuildingPlaced)
        .value("BuildingComplete", EventType::BuildingComplete)
        .value("BuildingDestroyed", EventType::BuildingDestroyed)
        .value("ResourceGathered", EventType::ResourceGathered)
        .value("DayStarted", EventType::DayStarted)
        .value("NightStarted", EventType::NightStarted)
        .value("Custom", EventType::Custom)
        .export_values();

    // Handler priority enum
    py::enum_<HandlerPriority>(m, "HandlerPriority")
        .value("Lowest", HandlerPriority::Lowest)
        .value("Low", HandlerPriority::Low)
        .value("Normal", HandlerPriority::Normal)
        .value("High", HandlerPriority::High)
        .value("Highest", HandlerPriority::Highest)
        .value("Monitor", HandlerPriority::Monitor)
        .export_values();

    // GameEvent class
    py::class_<GameEvent>(m, "GameEvent")
        .def(py::init<>())
        .def_readwrite("type", &GameEvent::type)
        .def_readwrite("custom_type", &GameEvent::customType)
        .def_readwrite("entity_id", &GameEvent::entityId)
        .def_readwrite("target_entity_id", &GameEvent::targetEntityId)
        .def_readwrite("building_id", &GameEvent::buildingId)
        .def_readwrite("x", &GameEvent::x)
        .def_readwrite("y", &GameEvent::y)
        .def_readwrite("z", &GameEvent::z)
        .def_readwrite("tile_x", &GameEvent::tileX)
        .def_readwrite("tile_y", &GameEvent::tileY)
        .def_readwrite("float_value", &GameEvent::floatValue)
        .def_readwrite("int_value", &GameEvent::intValue)
        .def_readwrite("string_value", &GameEvent::stringValue)
        .def_readwrite("cancelled", &GameEvent::cancelled)
        .def("cancel", [](GameEvent& e) { e.cancelled = true; });

    // Time functions
    m.def("get_delta_time", []() -> float {
        if (auto* ctx = ScriptBindings::GetContext()) {
            return ctx->GetDeltaTime();
        }
        return 0.0f;
    }, "Get time since last frame in seconds");

    m.def("get_game_time", []() -> float {
        if (auto* ctx = ScriptBindings::GetContext()) {
            return ctx->GetGameTime();
        }
        return 0.0f;
    }, "Get total game time in seconds");

    m.def("get_day_number", []() -> int {
        if (auto* ctx = ScriptBindings::GetContext()) {
            return ctx->GetDayNumber();
        }
        return 1;
    }, "Get current in-game day number");

    m.def("is_night", []() -> bool {
        if (auto* ctx = ScriptBindings::GetContext()) {
            return ctx->IsNight();
        }
        return false;
    }, "Check if it's currently nighttime");

    // Random functions
    m.def("random", []() -> float {
        if (auto* ctx = ScriptBindings::GetContext()) {
            return ctx->Random();
        }
        return 0.0f;
    }, "Get a random float between 0 and 1");

    m.def("random_range", [](float min, float max) -> float {
        if (auto* ctx = ScriptBindings::GetContext()) {
            return ctx->RandomRange(min, max);
        }
        return min;
    }, "Get a random float between min and max");

    m.def("random_int", [](int min, int max) -> int {
        if (auto* ctx = ScriptBindings::GetContext()) {
            return ctx->RandomInt(min, max);
        }
        return min;
    }, "Get a random integer between min and max (inclusive)");

    // =========== Logging Functions (spdlog integration) ===========
    m.def("log_trace", [](const std::string& message) {
        if (auto* ctx = ScriptBindings::GetContext()) {
            ctx->LogDebug("[TRACE] " + message);  // Map to debug level
        }
    }, "Log a trace message (verbose debugging)");

    m.def("log_debug", [](const std::string& message) {
        if (auto* ctx = ScriptBindings::GetContext()) {
            ctx->LogDebug(message);
        }
    }, "Log a debug message");

    m.def("log_info", [](const std::string& message) {
        if (auto* ctx = ScriptBindings::GetContext()) {
            ctx->LogInfo(message);
        }
    }, "Log an info message");

    m.def("log_warning", [](const std::string& message) {
        if (auto* ctx = ScriptBindings::GetContext()) {
            ctx->LogWarning(message);
        }
    }, "Log a warning message");

    m.def("log_warn", [](const std::string& message) {
        if (auto* ctx = ScriptBindings::GetContext()) {
            ctx->LogWarning(message);
        }
    }, "Log a warning message (alias)");

    m.def("log_error", [](const std::string& message) {
        if (auto* ctx = ScriptBindings::GetContext()) {
            ctx->LogError(message);
        }
    }, "Log an error message");

    // Convenience log function with level
    m.def("log", [](const std::string& level, const std::string& message) {
        if (auto* ctx = ScriptBindings::GetContext()) {
            if (level == "trace" || level == "TRACE") {
                ctx->LogDebug("[TRACE] " + message);
            } else if (level == "debug" || level == "DEBUG") {
                ctx->LogDebug(message);
            } else if (level == "info" || level == "INFO") {
                ctx->LogInfo(message);
            } else if (level == "warn" || level == "warning" || level == "WARN" || level == "WARNING") {
                ctx->LogWarning(message);
            } else if (level == "error" || level == "ERROR") {
                ctx->LogError(message);
            } else {
                ctx->LogInfo(message);  // Default to info
            }
        }
    }, "Log a message with specified level",
       py::arg("level"), py::arg("message"));

    // Print function (for convenience - logs as info)
    m.def("print", [](const py::args& args) {
        if (auto* ctx = ScriptBindings::GetContext()) {
            std::string message;
            for (size_t i = 0; i < args.size(); ++i) {
                if (i > 0) message += " ";
                message += py::str(args[i]).cast<std::string>();
            }
            ctx->LogInfo(message);
        }
    }, "Print message to log (info level)");
}

} // namespace Scripting
} // namespace Nova
