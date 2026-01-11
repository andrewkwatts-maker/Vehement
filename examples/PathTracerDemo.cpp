/**
 * @file PathTracerDemo.cpp
 * @brief Complete demonstration of the Nova Engine path tracer
 *
 * This example shows:
 * - Setting up the path tracer
 * - Creating various materials (diffuse, metal, glass)
 * - Rendering with dispersion and caustics
 * - Performance monitoring
 * - Interactive camera controls
 */

#include "../engine/graphics/PathTracerIntegration.hpp"
#include "../engine/scene/Camera.hpp"
#include "../engine/scene/FlyCamera.hpp"
#include "../engine/core/Window.hpp"
#include "../engine/core/Time.hpp"
#include "../engine/input/InputManager.hpp"
#include "../engine/core/Logger.hpp"
#include <iostream>

using namespace Nova;

class PathTracerDemo {
public:
    bool Initialize() {
        // Create window
        m_window = std::make_unique<Window>();
        if (!m_window->Create(1920, 1080, "Path Tracer Demo")) {
            Logger::Error("Failed to create window");
            return false;
        }

        // Initialize path tracer
        m_pathTracer = std::make_unique<PathTracerIntegration>();
        if (!m_pathTracer->Initialize(1920, 1080, true)) {
            Logger::Error("Failed to initialize path tracer");
            return false;
        }

        // Set quality preset
        m_pathTracer->SetQualityPreset(PathTracerIntegration::QualityPreset::Realtime);

        // Enable adaptive quality for consistent 120 FPS
        m_pathTracer->SetAdaptiveQuality(true, 120.0f);

        // Setup camera
        m_camera = std::make_unique<FlyCamera>();
        m_camera->SetPerspective(45.0f, 16.0f / 9.0f, 0.1f, 100.0f);
        m_camera->LookAt(glm::vec3(0, 2, 5), glm::vec3(0, 0, 0));

        // Create demo scene
        CreateDemoScene();

        Logger::Info("Path Tracer Demo initialized");
        Logger::Info("Controls:");
        Logger::Info("  WASD - Move camera");
        Logger::Info("  Mouse - Look around");
        Logger::Info("  1-5 - Switch scenes");
        Logger::Info("  Q - Toggle dispersion");
        Logger::Info("  E - Toggle denoising");
        Logger::Info("  R - Reset accumulation");
        Logger::Info("  ESC - Exit");

        return true;
    }

    void Run() {
        Time::Reset();

        while (!m_window->ShouldClose()) {
            float deltaTime = Time::GetDeltaTime();
            Time::Update();

            ProcessInput(deltaTime);
            Update(deltaTime);
            Render();

            m_window->PollEvents();
        }
    }

    void Shutdown() {
        m_pathTracer->Shutdown();
        m_window->Close();
    }

private:
    void CreateDemoScene() {
        // Start with Cornell Box
        m_currentScene = 0;
        m_primitives = m_pathTracer->CreateCornellBox();
    }

    void ProcessInput(float deltaTime) {
        auto& input = InputManager::Instance();

        // Camera movement
        bool cameraMoved = false;

        if (input.IsKeyPressed(Key::W)) {
            m_camera->MoveForward(5.0f * deltaTime);
            cameraMoved = true;
        }
        if (input.IsKeyPressed(Key::S)) {
            m_camera->MoveForward(-5.0f * deltaTime);
            cameraMoved = true;
        }
        if (input.IsKeyPressed(Key::A)) {
            m_camera->MoveRight(-5.0f * deltaTime);
            cameraMoved = true;
        }
        if (input.IsKeyPressed(Key::D)) {
            m_camera->MoveRight(5.0f * deltaTime);
            cameraMoved = true;
        }

        // Mouse look
        if (input.IsMouseButtonPressed(MouseButton::Right)) {
            auto delta = input.GetMouseDelta();
            m_camera->Rotate(delta.y * 0.1f, delta.x * 0.1f);
            cameraMoved = true;
        }

        // Reset accumulation on camera movement
        if (cameraMoved) {
            m_pathTracer->ResetAccumulation();
        }

        // Scene switching
        if (input.IsKeyJustPressed(Key::Num1)) {
            m_primitives = m_pathTracer->CreateCornellBox();
            m_pathTracer->ResetAccumulation();
            Logger::Info("Scene: Cornell Box");
        }
        if (input.IsKeyJustPressed(Key::Num2)) {
            m_primitives = m_pathTracer->CreateRefractionScene();
            m_pathTracer->ResetAccumulation();
            Logger::Info("Scene: Refraction Test");
        }
        if (input.IsKeyJustPressed(Key::Num3)) {
            m_primitives = m_pathTracer->CreateCausticsScene();
            m_pathTracer->ResetAccumulation();
            Logger::Info("Scene: Caustics Demo");
        }
        if (input.IsKeyJustPressed(Key::Num4)) {
            m_primitives = m_pathTracer->CreateDispersionScene();
            m_pathTracer->ResetAccumulation();
            Logger::Info("Scene: Dispersion (Rainbow)");
        }
        if (input.IsKeyJustPressed(Key::Num5)) {
            CreateCustomScene();
            m_pathTracer->ResetAccumulation();
            Logger::Info("Scene: Custom");
        }

        // Toggle features
        if (input.IsKeyJustPressed(Key::Q)) {
            m_dispersionEnabled = !m_dispersionEnabled;
            m_pathTracer->SetEnableDispersion(m_dispersionEnabled);
            m_pathTracer->ResetAccumulation();
            Logger::Info("Dispersion: %s", m_dispersionEnabled ? "ON" : "OFF");
        }
        if (input.IsKeyJustPressed(Key::E)) {
            m_denoisingEnabled = !m_denoisingEnabled;
            m_pathTracer->SetEnableDenoising(m_denoisingEnabled);
            Logger::Info("Denoising: %s", m_denoisingEnabled ? "ON" : "OFF");
        }

        // Manual reset
        if (input.IsKeyJustPressed(Key::R)) {
            m_pathTracer->ResetAccumulation();
            Logger::Info("Accumulation reset");
        }

        // Exit
        if (input.IsKeyJustPressed(Key::Escape)) {
            m_window->SetShouldClose(true);
        }
    }

    void Update(float deltaTime) {
        m_camera->Update(deltaTime);

        // Print stats every second
        m_statsTimer += deltaTime;
        if (m_statsTimer >= 1.0f) {
            m_statsTimer = 0.0f;
            PrintStats();
        }
    }

    void Render() {
        // Render with path tracer
        m_pathTracer->Render(*m_camera, m_primitives);

        // Display output texture
        auto texture = m_pathTracer->GetOutputTexture();
        if (texture) {
            // TODO: Render texture to screen (fullscreen quad)
            // For now, the texture is available for display
        }

        m_window->SwapBuffers();
    }

    void PrintStats() {
        const auto& stats = m_pathTracer->GetStats();

        std::cout << "\n=== Path Tracer Stats ===\n";
        std::cout << "FPS:          " << std::fixed << std::setprecision(1) << stats.fps << "\n";
        std::cout << "Frame Time:   " << stats.renderTimeMs << " ms\n";
        std::cout << "  Trace:      " << stats.traceTimeMs << " ms\n";
        std::cout << "  ReSTIR:     " << stats.restirTimeMs << " ms\n";
        std::cout << "  Denoise:    " << stats.denoiseTimeMs << " ms\n";
        std::cout << "Total Rays:   " << stats.totalRays << "\n";
        std::cout << "Avg Bounces:  " << stats.averageBounces << "\n";
        std::cout << "Frame Count:  " << stats.frameCount << "\n";
        std::cout << "=========================\n";
    }

    void CreateCustomScene() {
        m_primitives.clear();

        // Ground plane
        m_primitives.push_back(
            PathTracerIntegration::CreateSpherePrimitive(
                glm::vec3(0, -1000.5f, 0), 1000.0f,
                glm::vec3(0.5f, 0.5f, 0.5f)
            )
        );

        // Create a grid of spheres with different materials
        for (int x = -2; x <= 2; x++) {
            for (int z = -2; z <= 2; z++) {
                glm::vec3 pos(x * 1.5f, 0, z * 1.5f);

                // Vary material based on position
                if ((x + z) % 3 == 0) {
                    // Glass
                    m_primitives.push_back(
                        PathTracerIntegration::CreateGlassSphere(
                            pos, 0.4f, 1.5f + x * 0.1f, 0.02f
                        )
                    );
                } else if ((x + z) % 3 == 1) {
                    // Metal
                    glm::vec3 color(
                        0.5f + x * 0.1f,
                        0.5f + z * 0.1f,
                        0.8f
                    );
                    m_primitives.push_back(
                        PathTracerIntegration::CreateMetalSphere(
                            pos, 0.4f, color, 0.05f + x * 0.05f
                        )
                    );
                } else {
                    // Diffuse
                    glm::vec3 color(
                        0.8f,
                        0.5f + x * 0.1f,
                        0.5f + z * 0.1f
                    );
                    m_primitives.push_back(
                        PathTracerIntegration::CreateSpherePrimitive(
                            pos, 0.4f, color
                        )
                    );
                }
            }
        }

        // Add some lights
        m_primitives.push_back(
            PathTracerIntegration::CreateLightSphere(
                glm::vec3(-3, 4, -3), 0.5f,
                glm::vec3(1, 0.8, 0.6), 20.0f
            )
        );

        m_primitives.push_back(
            PathTracerIntegration::CreateLightSphere(
                glm::vec3(3, 4, 3), 0.5f,
                glm::vec3(0.6, 0.8, 1), 20.0f
            )
        );
    }

    std::unique_ptr<Window> m_window;
    std::unique_ptr<PathTracerIntegration> m_pathTracer;
    std::unique_ptr<FlyCamera> m_camera;
    std::vector<SDFPrimitive> m_primitives;

    int m_currentScene = 0;
    bool m_dispersionEnabled = true;
    bool m_denoisingEnabled = true;
    float m_statsTimer = 0.0f;
};

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
    Logger::Info("Nova Engine - Path Tracer Demo");
    Logger::Info("Version 1.0");

    PathTracerDemo demo;

    if (!demo.Initialize()) {
        Logger::Error("Failed to initialize demo");
        return -1;
    }

    demo.Run();
    demo.Shutdown();

    Logger::Info("Demo exited successfully");
    return 0;
}
