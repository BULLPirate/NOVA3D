#pragma once

namespace Nova {

/// Simple lit material: albedo tint multiplied with optional bound albedo texture.
struct Material {
    float TintR = 1.0f;
    float TintG = 1.0f;
    float TintB = 1.0f;
    float TintA = 1.0f;
    bool UseAlbedoTexture = true;
    bool ReceiveShadows = true;
};

} // namespace Nova
