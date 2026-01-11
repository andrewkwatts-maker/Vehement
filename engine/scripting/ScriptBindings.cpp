#include "ScriptBindings.hpp"
#include "ScriptContext.hpp"
#include "EventDispatcher.hpp"
#include "AIBehavior.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <pybind11/operators.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace py = pybind11;

namespace Nova {
namespace Scripting {

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

    // Transform class (composite of position, rotation, scale)
    py::class_<struct Transform>(m, "Transform")
        .def(py::init<>())
        .def(py::init([](const glm::vec3& pos) {
            Transform t;
            t.position = pos;
            return t;
        }))
        .def_readwrite("position", &Transform::position)
        .def_readwrite("rotation", &Transform::rotation)
        .def_readwrite("scale", &Transform::scale)
        .def("forward", [](const Transform& t) {
            return t.rotation * glm::vec3(0.0f, 0.0f, 1.0f);
        })
        .def("right", [](const Transform& t) {
            return t.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
        })
        .def("up", [](const Transform& t) {
            return t.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
        });

    // Define Transform struct
    struct Transform {
        glm::vec3 position{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 scale{1.0f};
    };
}

void ScriptBindings::RegisterEntityTypes(pybind11::module_& m) {
    // Entity type enum
    py::enum_<int>(m, "EntityType")
        .value("None", 0)
        .value("Player", 1)
        .value("Zombie", 2)
        .value("NPC", 3)
        .value("Projectile", 4)
        .value("Pickup", 5)
        .value("Effect", 6)
        .export_values();

    // Entity proxy class for Python
    py::class_<struct EntityProxy>(m, "Entity")
        .def_readonly("id", &EntityProxy::id)
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
        .def_property_readonly("type", [](const EntityProxy& e) -> std::string {
            if (auto* ctx = ScriptBindings::GetContext()) {
                return ctx->GetEntityType(e.id);
            }
            return "";
        })
        .def_property_readonly("is_alive", [](const EntityProxy& e) -> bool {
            if (auto* ctx = ScriptBindings::GetContext()) {
                return ctx->IsEntityAlive(e.id);
            }
            return false;
        })
        .def("damage", [](EntityProxy& e, float amount, uint32_t source) {
            if (auto* ctx = ScriptBindings::GetContext()) {
                ctx->DamageEntity(e.id, amount, source);
            }
        }, py::arg("amount"), py::arg("source") = 0)
        .def("heal", [](EntityProxy& e, float amount) {
            if (auto* ctx = ScriptBindings::GetContext()) {
                ctx->HealEntity(e.id, amount);
            }
        })
        .def("distance_to", [](const EntityProxy& e, const EntityProxy& other) -> float {
            if (auto* ctx = ScriptBindings::GetContext()) {
                return ctx->GetDistance(e.id, other.id);
            }
            return 0.0f;
        })
        .def("distance_to_point", [](const EntityProxy& e, const glm::vec3& point) -> float {
            if (auto* ctx = ScriptBindings::GetContext()) {
                auto pos = ctx->GetEntityPosition(e.id);
                return glm::length(point - pos);
            }
            return 0.0f;
        });

    // Define EntityProxy struct
    struct EntityProxy {
        uint32_t id = 0;
    };

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
    py::enum_<int>(m, "ResourceType")
        .value("Food", 0)
        .value("Wood", 1)
        .value("Stone", 2)
        .value("Metal", 3)
        .value("Coins", 4)
        .value("Fuel", 5)
        .value("Medicine", 6)
        .value("Ammunition", 7)
        .export_values();

    // Building type enum
    py::enum_<int>(m, "BuildingType")
        .value("Shelter", 0)
        .value("House", 1)
        .value("Barracks", 2)
        .value("Farm", 3)
        .value("LumberMill", 4)
        .value("Quarry", 5)
        .value("Workshop", 6)
        .value("WatchTower", 7)
        .value("Wall", 8)
        .value("Gate", 9)
        .value("Fortress", 10)
        .value("TradingPost", 11)
        .value("Hospital", 12)
        .value("Warehouse", 13)
        .value("CommandCenter", 14)
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
                result.push_back(id);
            }
        }
        return result;
    }, "Find all entities within radius of a point");

    m.def("find_entities_in_radius_vec", [](const glm::vec3& pos, float radius) -> py::list {
        py::list result;
        if (auto* ctx = ScriptBindings::GetContext()) {
            auto entities = ctx->FindEntitiesInRadius(pos.x, pos.y, pos.z, radius);
            for (uint32_t id : entities) {
                result.push_back(id);
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

    m.def("play_sound", [](const std::string& name, float x, float y, float z) {
        if (auto* ctx = ScriptBindings::GetContext()) {
            ctx->PlaySound(name, x, y, z);
        }
    }, "Play a sound effect",
       py::arg("name"), py::arg("x") = 0.0f, py::arg("y") = 0.0f, py::arg("z") = 0.0f);

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

    // Logging functions
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

    m.def("log_error", [](const std::string& message) {
        if (auto* ctx = ScriptBindings::GetContext()) {
            ctx->LogError(message);
        }
    }, "Log an error message");
}

} // namespace Scripting
} // namespace Nova
