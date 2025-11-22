#pragma once

#include <string>
#include <functional>
#include <memory>

// Forward declare pybind11 types
namespace pybind11 {
    class module_;
}

namespace Nova {

class Renderer;
class Graph;

namespace Scripting {

class PythonEngine;
class ScriptContext;

/**
 * @brief C++ to Python bindings for Nova3D/Vehement2
 *
 * Binds:
 * - Vector3, Transform, Entity classes
 * - Game systems (ResourceManager, EntityManager, etc.)
 * - World query functions (raycast, find_entities_in_radius)
 * - UI notification functions
 *
 * Usage:
 * @code
 * // Bindings are automatically registered when PythonEngine initializes
 * // In Python:
 * import nova_engine
 *
 * vec = nova_engine.Vector3(1.0, 2.0, 3.0)
 * entity = nova_engine.get_entity(entity_id)
 * entity.position = vec
 * @endcode
 */
class ScriptBindings {
public:
    /**
     * @brief Register all bindings with Python
     * Called automatically during PythonEngine::Initialize()
     */
    static void RegisterAll();

    /**
     * @brief Register math type bindings (Vector3, Transform, etc.)
     */
    static void RegisterMathTypes(pybind11::module_& m);

    /**
     * @brief Register entity-related bindings
     */
    static void RegisterEntityTypes(pybind11::module_& m);

    /**
     * @brief Register RTS system bindings (buildings, resources, etc.)
     */
    static void RegisterRTSTypes(pybind11::module_& m);

    /**
     * @brief Register world query functions
     */
    static void RegisterWorldQueries(pybind11::module_& m);

    /**
     * @brief Register UI and notification functions
     */
    static void RegisterUIFunctions(pybind11::module_& m);

    /**
     * @brief Register AI-related bindings
     */
    static void RegisterAITypes(pybind11::module_& m);

    /**
     * @brief Register event system bindings
     */
    static void RegisterEventTypes(pybind11::module_& m);

    /**
     * @brief Set the script context for bindings to use
     */
    static void SetContext(ScriptContext* context);

    /**
     * @brief Get the current script context
     */
    static ScriptContext* GetContext();

private:
    static ScriptContext* s_context;
};

/**
 * @brief Helper macros for creating Python bindings
 */
#define NOVA_BIND_PROPERTY(class_name, prop_name, getter, setter) \
    .def_property(#prop_name, &class_name::getter, &class_name::setter)

#define NOVA_BIND_READONLY(class_name, prop_name, getter) \
    .def_property_readonly(#prop_name, &class_name::getter)

#define NOVA_BIND_METHOD(class_name, method_name) \
    .def(#method_name, &class_name::method_name)

} // namespace Scripting
} // namespace Nova
