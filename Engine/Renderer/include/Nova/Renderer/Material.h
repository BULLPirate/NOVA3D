#pragma once

namespace Nova {

/// Simple lit material: albedo tint multiplied with optional bound albedo texture.
struct Material {
    float TintR = 1.0f;
    float TintG = 1.0f;
    float TintB = 1.0f;
    float TintA = 1.0f;
    bool UseAlbedoTexture = true;
    /// When false, only diffuse/ambient lighting is used (avoids self-shadow flicker on a lone mesh).
    bool ReceiveShadows = false;
};

} // namespace Nova
