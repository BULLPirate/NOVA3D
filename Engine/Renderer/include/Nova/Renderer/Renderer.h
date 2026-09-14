#pragma once

#include <cstdint>
#include <memory>

namespace Nova {

class Window;
struct Camera;

/// Abstract GPU renderer. First backend is Metal (macOS).
/// Lifetime: Init after Window is created, Shutdown before Window is destroyed.
class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual bool Init(Window& window) = 0;
    virtual void Shutdown() = 0;
    virtual bool IsInitialized() const = 0;

    /// Acquire drawable and begin a render pass that clears the backbuffer.
    virtual void BeginFrame() = 0;
    /// Present the drawable and submit the command buffer.
    virtual void EndFrame() = 0;

    virtual void SetClearColor(float r, float g, float b, float a) = 0;
    virtual void OnResize(uint32_t width, uint32_t height) = 0;

    /// View + projection for the current frame (call before BeginFrame).
    virtual void SetCamera(const Camera& camera) = 0;
};

/// Factory — currently always returns the Metal backend.
std::unique_ptr<IRenderer> CreateRenderer();

} // namespace Nova
