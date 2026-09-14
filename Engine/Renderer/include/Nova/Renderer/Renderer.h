#pragma once

#include <cstdint>
#include <memory>

namespace Nova {

class Window;
struct Camera;
struct Mat4;
struct Material;
struct DirectionalLight;
struct ShadowSettings;

/// Abstract GPU renderer. First backend is Metal (macOS).
/// Lifetime: Init after Window is created, Shutdown before Window is destroyed.
class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual bool Init(Window& window) = 0;
    virtual void Shutdown() = 0;
    virtual bool IsInitialized() const = 0;

    /// Acquire swapchain image and build the render pass (no encoding yet).
    virtual void BeginFrame() = 0;
    /// Start the render encoder (call after UI NewFrame if using ImGui).
    virtual void BeginDrawing() = 0;
    /// Draw queued meshes, optional overlay, present.
    virtual void EndFrame() = 0;

    virtual void SetClearColor(float r, float g, float b, float a) = 0;
    virtual void OnResize(uint32_t width, uint32_t height) = 0;

    /// View + projection for the current frame (call before BeginFrame).
    virtual void SetCamera(const Camera& camera) = 0;

    /// Object transform in world space (call before BeginFrame).
    virtual void SetModelMatrix(const Mat4& model) = 0;

    virtual void SetMaterial(const Material& material) = 0;
    virtual void SetDirectionalLight(const DirectionalLight& light) = 0;
    virtual void SetShadowSettings(const ShadowSettings& settings) = 0;

    /// Queue mesh draws for the current frame (flushed in EndFrame).
    virtual void ClearMeshDraws() = 0;
    virtual void EnqueueMeshDraw(const Mat4& model, const Material& material) = 0;

    /// Metal MTLDevice* for editor UI backends. Null if unavailable.
    virtual void* GetNativeDevice() const = 0;

    /// Called each frame after the render pass descriptor is ready (ImGui Metal NewFrame).
    using RenderPassReadyCallback = void (*)(void* renderPassDescriptor, void* userData);
    virtual void SetRenderPassReadyCallback(RenderPassReadyCallback callback, void* userData) = 0;

    /// Optional UI overlay after the 3D pass (e.g. ImGui), same render encoder.
    using FrameOverlayCallback = void (*)(void* commandBuffer, void* renderEncoder, void* userData);
    virtual void SetFrameOverlayCallback(FrameOverlayCallback callback, void* userData) = 0;
};

/// Factory — currently always returns the Metal backend.
std::unique_ptr<IRenderer> CreateRenderer();

} // namespace Nova
