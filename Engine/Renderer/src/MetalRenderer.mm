#include <Nova/Renderer/Renderer.h>
#include <Nova/Renderer/RenderViewport.h>
#include <Nova/Renderer/Camera.h>
#include <Nova/Renderer/Mesh.h>
#include <Nova/Renderer/Texture.h>
#include <Nova/Renderer/Material.h>
#include <Nova/Renderer/Lighting.h>
#include <Nova/Math/Mat4.h>
#include <Nova/Platform/Window.h>
#include <Nova/Core/Log.h>

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <array>
#include <cstring>
#include <vector>

namespace Nova {

static const char* kMeshShader = R"(
#include <metal_stdlib>
using namespace metal;

struct FrameUniforms {
    float4x4 mvp;
    float4x4 model;
    float4x4 lightViewProj;
    float4 lightDir;   // xyz = direction toward light; w = ambient
    float4 lightColor; // rgb; w = 1 to sample albedo texture
    float4 tint;
    float4 shadowParams; // x = bias, y = strength, z = global enabled, w = material receives
    float4 pointPos;     // xyz world, w = range (0 = off)
    float4 pointColor;   // rgb * intensity
};

struct VertexIn {
    float3 position [[attribute(0)]];
    float3 normal   [[attribute(1)]];
    float2 texCoord [[attribute(2)]];
};

struct VertexOut {
    float4 position [[position]];
    float3 normal;
    float3 worldPos;
    float2 texCoord;
};

vertex float4 shadow_vertex(VertexIn in [[stage_in]],
                            constant FrameUniforms& u [[buffer(1)]]) {
    float4 world = u.model * float4(in.position, 1.0);
    return u.lightViewProj * world;
}

vertex VertexOut vertex_main(VertexIn in [[stage_in]],
                             constant FrameUniforms& u [[buffer(1)]]) {
    VertexOut out;
    float4 world = u.model * float4(in.position, 1.0);
    out.position = u.mvp * world;
    out.worldPos = world.xyz;
    out.normal = in.normal;
    out.texCoord = in.texCoord;
    return out;
}

fragment float4 fragment_main(VertexOut in [[stage_in]],
                              constant FrameUniforms& u [[buffer(1)]],
                              texture2d<float> albedo [[texture(0)]],
                              depth2d<float> shadowMap [[texture(1)]],
                              sampler texSampler [[sampler(0)]],
                              sampler shadowSampler [[sampler(1)]]) {
    float3 N = normalize((u.model * float4(in.normal, 0.0)).xyz);
    float3 L = normalize(u.lightDir.xyz);
    float NdotL = saturate(dot(N, L) * 0.55 + 0.45);

    float3 base = u.tint.rgb;
    if (u.lightColor.w > 0.5) {
        base *= albedo.sample(texSampler, in.texCoord).rgb;
    }

    const float ambient = u.lightDir.w;
    const float3 diffuse = u.lightColor.rgb * NdotL;

    float shadow = 1.0;
    if (u.shadowParams.z > 0.5 && u.shadowParams.w > 0.5 && NdotL > 0.001) {
        float4 lightClip = u.lightViewProj * float4(in.worldPos, 1.0);
        float3 ndc = lightClip.xyz / lightClip.w;
        float2 shadowUV = ndc.xy * 0.5 + 0.5;
        shadowUV.y = 1.0 - shadowUV.y;
        bool inBounds = shadowUV.x >= 0.0 && shadowUV.x <= 1.0 &&
                        shadowUV.y >= 0.0 && shadowUV.y <= 1.0;
        if (inBounds) {
            float slopeBias = u.shadowParams.x * (1.0 - NdotL);
            float depth = ndc.z - (u.shadowParams.x + slopeBias);
            shadow = shadowMap.sample_compare(shadowSampler, shadowUV, depth);
        }
    }

    float3 pointLit = float3(0.0);
    if (u.pointPos.w > 0.001) {
        float3 toL = u.pointPos.xyz - in.worldPos;
        float dist = length(toL);
        float3 Lp = toL / max(dist, 1e-5);
        float nd = saturate(dot(N, Lp));
        float t = saturate(1.0 - dist / u.pointPos.w);
        pointLit = u.pointColor.rgb * nd * (t * t);
    }

    const float shade = mix(1.0 - u.shadowParams.y, 1.0, shadow);
    const float3 lit = base * ambient + base * diffuse * shade + base * pointLit;
    return float4(lit, u.tint.a);
}
)";

struct FrameUniforms {
    float mvp[16];
    float model[16];
    float lightViewProj[16];
    float lightDir[4];
    float lightColor[4];
    float tint[4];
    float shadowParams[4];
    float pointPos[4];
    float pointColor[4];
};

static constexpr uint32_t kShadowMapSize = 2048;

class MetalRenderer final : public IRenderer {
public:
    MetalRenderer() = default;
    ~MetalRenderer() override { Shutdown(); }

    MetalRenderer(const MetalRenderer&) = delete;
    MetalRenderer& operator=(const MetalRenderer&) = delete;

    bool Init(Window& window) override {
        if (m_Initialized) {
            NOVA_LOG_WARN("MetalRenderer already initialized");
            return true;
        }

        m_Device = MTLCreateSystemDefaultDevice();
        if (!m_Device) {
            NOVA_LOG_FATAL("MTLCreateSystemDefaultDevice failed — no Metal GPU");
            return false;
        }

        m_Queue = [m_Device newCommandQueue];
        if (!m_Queue) {
            NOVA_LOG_FATAL("Failed to create MTLCommandQueue");
            Shutdown();
            return false;
        }

        void* layerPtr = window.GetNativeMetalLayer();
        if (!layerPtr) {
            NOVA_LOG_FATAL("Window has no CAMetalLayer — Metal view was not created");
            Shutdown();
            return false;
        }

        m_Layer = (__bridge CAMetalLayer*)layerPtr;
        m_Layer.device = m_Device;
        m_Layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        m_Layer.framebufferOnly = YES;
        m_Layer.opaque = YES;
        m_Layer.contentsScale = window.GetContentScale();

        uint32_t fbW = 0, fbH = 0;
        window.GetFramebufferSize(fbW, fbH);
        OnResize(fbW, fbH);

        if (!CreateMeshPipeline()) {
            Shutdown();
            return false;
        }

        if (!CreateShadowResources()) {
            Shutdown();
            return false;
        }

        m_Window = &window;
        m_Initialized = true;

        NOVA_LOG_INFO("Metal renderer ready: {} ({}x{})",
                      [[m_Device name] UTF8String], fbW, fbH);
        return true;
    }

    void Shutdown() override {
        m_Drawable = nil;
        m_Encoder = nil;
        m_CommandBuffer = nil;
        m_Pipeline = nil;
        m_ShadowPipeline = nil;
        m_ShadowMap = nil;
        m_ShadowSampler = nil;
        m_GpuMeshes.clear();
        m_UniformBuffer = nil;
        m_GpuTextures.clear();
        m_Sampler = nil;
        m_DepthStencilState = nil;
        m_DepthTexture = nil;
        m_Layer = nil;
        m_Queue = nil;
        m_Device = nil;
        m_Window = nullptr;
        m_Initialized = false;
    }

    bool IsInitialized() const override { return m_Initialized; }

    void BeginFrame() override {
        if (!m_Initialized || !m_Layer) return;

        uint32_t fbW = 0, fbH = 0;
        m_Window->GetFramebufferSize(fbW, fbH);
        if (fbW != m_FbWidth || fbH != m_FbHeight) {
            OnResize(fbW, fbH);
        }

        m_Drawable = [m_Layer nextDrawable];
        if (!m_Drawable) {
            NOVA_LOG_WARN("CAMetalLayer nextDrawable returned nil");
            return;
        }

        MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = m_Drawable.texture;
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass.colorAttachments[0].clearColor = MTLClearColorMake(
            m_ClearColor[0], m_ClearColor[1], m_ClearColor[2], m_ClearColor[3]);

        if (m_DepthTexture) {
            pass.depthAttachment.texture = m_DepthTexture;
            pass.depthAttachment.loadAction = MTLLoadActionClear;
            pass.depthAttachment.storeAction = MTLStoreActionDontCare;
            pass.depthAttachment.clearDepth = 1.0;
        }

        m_FramePass = pass;
        m_CommandBuffer = [m_Queue commandBuffer];

        if (m_PassReadyCallback) {
            m_PassReadyCallback((__bridge void*)pass, m_PassReadyUser);
        }
    }

    void BeginDrawing() override {
        if (!m_CommandBuffer || !m_FramePass) return;

        if (ShouldRenderShadowPass()) {
            RenderShadowPass(m_CommandBuffer);
        }

        m_Encoder = [m_CommandBuffer renderCommandEncoderWithDescriptor:m_FramePass];
        m_FramePass = nil;
    }

    void EndFrame() override {
        if (!m_CommandBuffer) return;

        if (m_Encoder) {
            DrawQueuedMeshes(m_Encoder);

            if (m_OverlayCallback) {
                m_OverlayCallback((__bridge void*)m_CommandBuffer,
                                    (__bridge void*)m_Encoder,
                                    m_OverlayUser);
            }

            [m_Encoder endEncoding];
            m_Encoder = nil;
        }

        if (m_Drawable) {
            [m_CommandBuffer presentDrawable:m_Drawable];
            m_Drawable = nil;
        }

        [m_CommandBuffer commit];
        m_CommandBuffer = nil;
    }

    void ClearMeshDraws() override { m_MeshDraws.clear(); }

    MeshGpuHandle CreateGpuMesh(const TexturedMeshData& mesh) override {
        if (!m_Device || mesh.Vertices.empty() || mesh.Indices.empty()) {
            return kDefaultMeshGpuHandle;
        }

        GpuMeshBuffers gpu;
        gpu.IndexCount = static_cast<NSUInteger>(mesh.Indices.size());
        gpu.VertexBuffer = [m_Device newBufferWithBytes:mesh.Vertices.data()
                                                 length:mesh.Vertices.size() * sizeof(TexturedVertex)
                                                options:MTLResourceStorageModeShared];
        gpu.IndexBuffer = [m_Device newBufferWithBytes:mesh.Indices.data()
                                                length:mesh.Indices.size() * sizeof(uint32_t)
                                               options:MTLResourceStorageModeShared];
        m_GpuMeshes.push_back(gpu);
        return static_cast<MeshGpuHandle>(m_GpuMeshes.size() - 1);
    }

    TextureGpuHandle CreateGpuTexture(const ImageRGBA& image) override {
        if (!m_Device || !image.IsValid()) {
            return kDefaultTextureGpuHandle;
        }

        MTLTextureDescriptor* desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                        width:image.Width
                                                                                       height:image.Height
                                                                                    mipmapped:NO];
        desc.usage = MTLTextureUsageShaderRead;
        id<MTLTexture> texture = [m_Device newTextureWithDescriptor:desc];
        if (!texture) {
            return kDefaultTextureGpuHandle;
        }

        MTLRegion region = MTLRegionMake2D(0, 0, image.Width, image.Height);
        [texture replaceRegion:region
                   mipmapLevel:0
                     withBytes:image.Pixels.data()
                   bytesPerRow:image.Width * 4];
        m_GpuTextures.push_back(texture);
        return static_cast<TextureGpuHandle>(m_GpuTextures.size() - 1);
    }

    void EnqueueMeshDraw(const Mat4& model,
                         const Material& material,
                         MeshGpuHandle meshHandle,
                         TextureGpuHandle albedoHandle) override {
        m_MeshDraws.push_back({model, material, meshHandle, albedoHandle});
    }

    void* GetNativeDevice() const override {
        return (__bridge void*)m_Device;
    }

    void SetRenderPassReadyCallback(RenderPassReadyCallback callback, void* userData) override {
        m_PassReadyCallback = callback;
        m_PassReadyUser = userData;
    }

    void SetFrameOverlayCallback(FrameOverlayCallback callback, void* userData) override {
        m_OverlayCallback = callback;
        m_OverlayUser = userData;
    }

    void SetClearColor(float r, float g, float b, float a) override {
        m_ClearColor = {r, g, b, a};
    }

    void SetRenderViewport(const RenderViewport& viewport) override { m_Viewport = viewport; }

    void OnResize(uint32_t width, uint32_t height) override {
        if (width == 0 || height == 0) return;
        m_FbWidth = width;
        m_FbHeight = height;
        if (m_Layer) {
            m_Layer.drawableSize = CGSizeMake(static_cast<CGFloat>(width),
                                              static_cast<CGFloat>(height));
            NOVA_LOG_DEBUG("Metal drawable resized to {}x{}", width, height);
        }
        RecreateDepthTexture(width, height);
    }

    void SetCamera(const Camera& camera) override {
        m_ViewProj = camera.GetViewProjectionMatrix();
        UploadFrameUniforms();
    }

    void SetModelMatrix(const Mat4& model) override {
        m_Model = model;
        UploadFrameUniforms();
    }

    void SetMaterial(const Material& material) override {
        m_Material = material;
        UploadFrameUniforms();
    }

    void SetDirectionalLight(const DirectionalLight& light) override {
        m_Light = light;
        m_Light.Direction = m_Light.Direction.Normalized();
        UploadFrameUniforms();
    }

    void SetPointLight(const PointLight& light) override {
        m_PointLight = light;
        UploadFrameUniforms();
    }

    void SetShadowSettings(const ShadowSettings& settings) override {
        m_ShadowSettings = settings;
        UploadFrameUniforms();
    }

private:
    struct GpuMeshBuffers {
        id<MTLBuffer> VertexBuffer = nil;
        id<MTLBuffer> IndexBuffer = nil;
        NSUInteger IndexCount = 0;
    };

    struct MeshDrawItem {
        Mat4 model;
        Material material;
        MeshGpuHandle mesh = kDefaultMeshGpuHandle;
        TextureGpuHandle albedo = kDefaultTextureGpuHandle;
    };

    bool ShouldRenderShadowPass() const {
        if (!m_ShadowSettings.Enabled || !m_ShadowPipeline || !m_ShadowMap) {
            return false;
        }
        for (const MeshDrawItem& draw : m_MeshDraws) {
            if (draw.material.ReceiveShadows) return true;
        }
        return m_Material.ReceiveShadows;
    }

    const GpuMeshBuffers& MeshBuffers(MeshGpuHandle handle) const {
        if (handle < m_GpuMeshes.size()) {
            return m_GpuMeshes[handle];
        }
        return m_GpuMeshes[kDefaultMeshGpuHandle];
    }

    id<MTLTexture> AlbedoTexture(TextureGpuHandle handle) const {
        if (handle < m_GpuTextures.size() && m_GpuTextures[handle]) {
            return m_GpuTextures[handle];
        }
        return m_GpuTextures.empty() ? nil : m_GpuTextures[kDefaultTextureGpuHandle];
    }

    void DrawIndexedMesh(id<MTLRenderCommandEncoder> encoder,
                         MeshGpuHandle meshHandle,
                         TextureGpuHandle albedoHandle) {
        const GpuMeshBuffers& gpu = MeshBuffers(meshHandle);
        if (!m_Pipeline || !gpu.VertexBuffer || !gpu.IndexBuffer || gpu.IndexCount == 0) return;

        if (!m_Viewport.Active) {
            MTLViewport viewport{};
            viewport.originX = 0;
            viewport.originY = 0;
            viewport.width = static_cast<double>(m_FbWidth);
            viewport.height = static_cast<double>(m_FbHeight);
            viewport.znear = 0.0;
            viewport.zfar = 1.0;
            [encoder setViewport:viewport];
        }
        [encoder setRenderPipelineState:m_Pipeline];
        [encoder setDepthStencilState:m_DepthStencilState];
        [encoder setFrontFacingWinding:MTLWindingCounterClockwise];
        [encoder setCullMode:MTLCullModeBack];
        [encoder setVertexBuffer:gpu.VertexBuffer offset:0 atIndex:0];
        [encoder setVertexBuffer:m_UniformBuffer offset:0 atIndex:1];
        [encoder setFragmentBuffer:m_UniformBuffer offset:0 atIndex:1];
        [encoder setFragmentTexture:AlbedoTexture(albedoHandle) atIndex:0];
        [encoder setFragmentTexture:m_ShadowMap atIndex:1];
        [encoder setFragmentSamplerState:m_Sampler atIndex:0];
        [encoder setFragmentSamplerState:m_ShadowSampler atIndex:1];
        [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                            indexCount:gpu.IndexCount
                             indexType:MTLIndexTypeUInt32
                           indexBuffer:gpu.IndexBuffer
                     indexBufferOffset:0];
    }

    void ApplyViewportScissor(id<MTLRenderCommandEncoder> encoder) {
        if (!m_Viewport.Active || m_Viewport.Width == 0 || m_Viewport.Height == 0) {
            return;
        }
        const double yBottom = static_cast<double>(m_FbHeight) - m_Viewport.Y - m_Viewport.Height;
        MTLViewport vp = {
            static_cast<double>(m_Viewport.X),
            yBottom,
            static_cast<double>(m_Viewport.Width),
            static_cast<double>(m_Viewport.Height),
            0.0,
            1.0,
        };
        [encoder setViewport:vp];
        MTLScissorRect scissor = {
            m_Viewport.X,
            static_cast<NSUInteger>(yBottom),
            m_Viewport.Width,
            m_Viewport.Height,
        };
        [encoder setScissorRect:scissor];
    }

    void DrawQueuedMeshes(id<MTLRenderCommandEncoder> encoder) {
        ApplyViewportScissor(encoder);
        if (m_MeshDraws.empty()) {
            DrawIndexedMesh(encoder, kDefaultMeshGpuHandle, kDefaultTextureGpuHandle);
            return;
        }

        for (const MeshDrawItem& draw : m_MeshDraws) {
            m_Model = draw.model;
            m_Material = draw.material;
            UploadFrameUniforms();
            DrawIndexedMesh(encoder, draw.mesh, draw.albedo);
        }
    }

    void DrawShadowMeshes(id<MTLRenderCommandEncoder> encoder) {
        if (m_MeshDraws.empty()) {
            const GpuMeshBuffers& gpu = MeshBuffers(kDefaultMeshGpuHandle);
            [encoder setVertexBuffer:gpu.VertexBuffer offset:0 atIndex:0];
            [encoder setVertexBuffer:m_UniformBuffer offset:0 atIndex:1];
            [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                indexCount:gpu.IndexCount
                                 indexType:MTLIndexTypeUInt32
                               indexBuffer:gpu.IndexBuffer
                         indexBufferOffset:0];
            return;
        }

        for (const MeshDrawItem& draw : m_MeshDraws) {
            m_Model = draw.model;
            m_Material = draw.material;
            UploadFrameUniforms();
            const GpuMeshBuffers& gpu = MeshBuffers(draw.mesh);
            [encoder setVertexBuffer:gpu.VertexBuffer offset:0 atIndex:0];
            [encoder setVertexBuffer:m_UniformBuffer offset:0 atIndex:1];
            [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                indexCount:gpu.IndexCount
                                 indexType:MTLIndexTypeUInt32
                               indexBuffer:gpu.IndexBuffer
                         indexBufferOffset:0];
        }
    }

    void UploadFrameUniforms() {
        const Mat4 mvp = m_ViewProj * m_Model;
        std::memcpy(m_Uniforms.mvp, mvp.Data(), sizeof(m_Uniforms.mvp));
        std::memcpy(m_Uniforms.model, m_Model.Data(), sizeof(m_Uniforms.model));

        const Vec3 lightDir = m_Light.Direction.Normalized();
        m_Uniforms.lightDir[0] = lightDir.x;
        m_Uniforms.lightDir[1] = lightDir.y;
        m_Uniforms.lightDir[2] = lightDir.z;
        m_Uniforms.lightDir[3] = m_Light.Ambient;

        m_Uniforms.lightColor[0] = m_Light.Color.x;
        m_Uniforms.lightColor[1] = m_Light.Color.y;
        m_Uniforms.lightColor[2] = m_Light.Color.z;
        m_Uniforms.lightColor[3] = m_Material.UseAlbedoTexture ? 1.0f : 0.0f;

        m_Uniforms.tint[0] = m_Material.TintR;
        m_Uniforms.tint[1] = m_Material.TintG;
        m_Uniforms.tint[2] = m_Material.TintB;
        m_Uniforms.tint[3] = m_Material.TintA;

        const Mat4 lightViewProj = ComputeDirectionalLightViewProjection(
            m_Light.Direction, {0.0f, 0.0f, 0.0f},
            m_ShadowSettings.OrthoHalfExtent,
            m_ShadowSettings.NearPlane,
            m_ShadowSettings.FarPlane);
        std::memcpy(m_Uniforms.lightViewProj, lightViewProj.Data(), sizeof(m_Uniforms.lightViewProj));

        m_Uniforms.shadowParams[0] = m_ShadowSettings.Bias;
        m_Uniforms.shadowParams[1] = m_ShadowSettings.Strength;
        m_Uniforms.shadowParams[2] = m_ShadowSettings.Enabled ? 1.0f : 0.0f;
        m_Uniforms.shadowParams[3] = m_Material.ReceiveShadows ? 1.0f : 0.0f;

        m_Uniforms.pointPos[0] = m_PointLight.Position.x;
        m_Uniforms.pointPos[1] = m_PointLight.Position.y;
        m_Uniforms.pointPos[2] = m_PointLight.Position.z;
        m_Uniforms.pointPos[3] = m_PointLight.Range;
        m_Uniforms.pointColor[0] = m_PointLight.Color.x;
        m_Uniforms.pointColor[1] = m_PointLight.Color.y;
        m_Uniforms.pointColor[2] = m_PointLight.Color.z;
        m_Uniforms.pointColor[3] = 1.0f;

        if (m_UniformBuffer) {
            std::memcpy([m_UniformBuffer contents], &m_Uniforms, sizeof(m_Uniforms));
        }
    }

    void RecreateDepthTexture(uint32_t width, uint32_t height) {
        if (!m_Device || width == 0 || height == 0) return;

        MTLTextureDescriptor* desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                                                          width:width
                                                                                         height:height
                                                                                      mipmapped:NO];
        desc.usage = MTLTextureUsageRenderTarget;
        desc.storageMode = MTLStorageModePrivate;
        m_DepthTexture = [m_Device newTextureWithDescriptor:desc];
        m_DepthTexture.label = @"NovaDepth";
    }

    bool CreateMeshPipeline() {
        NSError* error = nil;
        id<MTLLibrary> library = [m_Device newLibraryWithSource:@(kMeshShader)
                                                        options:nil
                                                          error:&error];
        if (!library) {
            NOVA_LOG_FATAL("Metal shader compile failed: {}",
                           error ? [[error localizedDescription] UTF8String] : "unknown");
            return false;
        }

        id<MTLFunction> vs = [library newFunctionWithName:@"vertex_main"];
        id<MTLFunction> fs = [library newFunctionWithName:@"fragment_main"];
        if (!vs || !fs) {
            NOVA_LOG_FATAL("Metal shader entry points vertex_main/fragment_main not found");
            return false;
        }

        MTLVertexDescriptor* vertexDesc = [[MTLVertexDescriptor alloc] init];
        vertexDesc.attributes[0].format = MTLVertexFormatFloat3;
        vertexDesc.attributes[0].offset = 0;
        vertexDesc.attributes[0].bufferIndex = 0;
        vertexDesc.attributes[1].format = MTLVertexFormatFloat3;
        vertexDesc.attributes[1].offset = sizeof(float) * 3;
        vertexDesc.attributes[1].bufferIndex = 0;
        vertexDesc.attributes[2].format = MTLVertexFormatFloat2;
        vertexDesc.attributes[2].offset = sizeof(float) * 6;
        vertexDesc.attributes[2].bufferIndex = 0;
        vertexDesc.layouts[0].stride = sizeof(TexturedVertex);
        vertexDesc.layouts[0].stepRate = 1;
        vertexDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

        MTLRenderPipelineDescriptor* pipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
        pipelineDesc.label = @"NovaTexturedMesh";
        pipelineDesc.vertexFunction = vs;
        pipelineDesc.fragmentFunction = fs;
        pipelineDesc.vertexDescriptor = vertexDesc;
        pipelineDesc.colorAttachments[0].pixelFormat = m_Layer.pixelFormat;
        pipelineDesc.colorAttachments[0].blendingEnabled = YES;
        pipelineDesc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        pipelineDesc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        pipelineDesc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
        pipelineDesc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        pipelineDesc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;

        m_Pipeline = [m_Device newRenderPipelineStateWithDescriptor:pipelineDesc error:&error];
        if (!m_Pipeline) {
            NOVA_LOG_FATAL("Metal pipeline creation failed: {}",
                           error ? [[error localizedDescription] UTF8String] : "unknown");
            return false;
        }

        m_GpuMeshes.clear();
        CreateGpuMesh(CreateUnitCubeTexturedMesh());
        CreateGpuMesh(CreateUnitPlaneTexturedMesh());
        CreateGpuMesh(CreateUnitSphereTexturedMesh());

        if (!CreateDefaultAlbedoTexture()) {
            return false;
        }

        MTLSamplerDescriptor* sampDesc = [[MTLSamplerDescriptor alloc] init];
        sampDesc.minFilter = MTLSamplerMinMagFilterLinear;
        sampDesc.magFilter = MTLSamplerMinMagFilterLinear;
        sampDesc.sAddressMode = MTLSamplerAddressModeRepeat;
        sampDesc.tAddressMode = MTLSamplerAddressModeRepeat;
        m_Sampler = [m_Device newSamplerStateWithDescriptor:sampDesc];

        MTLDepthStencilDescriptor* depthDesc = [[MTLDepthStencilDescriptor alloc] init];
        depthDesc.depthCompareFunction = MTLCompareFunctionLess;
        depthDesc.depthWriteEnabled = YES;
        m_DepthStencilState = [m_Device newDepthStencilStateWithDescriptor:depthDesc];

        m_UniformBuffer = [m_Device newBufferWithLength:sizeof(FrameUniforms)
                                                options:MTLResourceStorageModeShared];
        m_UniformBuffer.label = @"NovaFrameUBO";
        m_Model = Mat4::Identity();
        m_ViewProj = Mat4::Identity();
        m_Light = DefaultDirectionalLight();
        UploadFrameUniforms();

        NOVA_LOG_INFO("Lit textured mesh pipeline ready");
        return true;
    }

    bool CreateShadowResources() {
        NSError* error = nil;
        id<MTLLibrary> library = [m_Device newLibraryWithSource:@(kMeshShader)
                                                        options:nil
                                                          error:&error];
        if (!library) {
            NOVA_LOG_FATAL("Metal shadow shader compile failed: {}",
                           error ? [[error localizedDescription] UTF8String] : "unknown");
            return false;
        }

        id<MTLFunction> shadowVs = [library newFunctionWithName:@"shadow_vertex"];
        if (!shadowVs) {
            NOVA_LOG_FATAL("Metal shadow_vertex entry point not found");
            return false;
        }

        MTLVertexDescriptor* vertexDesc = [[MTLVertexDescriptor alloc] init];
        vertexDesc.attributes[0].format = MTLVertexFormatFloat3;
        vertexDesc.attributes[0].offset = 0;
        vertexDesc.attributes[0].bufferIndex = 0;
        vertexDesc.attributes[1].format = MTLVertexFormatFloat3;
        vertexDesc.attributes[1].offset = sizeof(float) * 3;
        vertexDesc.attributes[1].bufferIndex = 0;
        vertexDesc.attributes[2].format = MTLVertexFormatFloat2;
        vertexDesc.attributes[2].offset = sizeof(float) * 6;
        vertexDesc.attributes[2].bufferIndex = 0;
        vertexDesc.layouts[0].stride = sizeof(TexturedVertex);
        vertexDesc.layouts[0].stepRate = 1;
        vertexDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

        MTLRenderPipelineDescriptor* pipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
        pipelineDesc.label = @"NovaShadowDepth";
        pipelineDesc.vertexFunction = shadowVs;
        pipelineDesc.fragmentFunction = nil;
        pipelineDesc.vertexDescriptor = vertexDesc;
        pipelineDesc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;

        m_ShadowPipeline = [m_Device newRenderPipelineStateWithDescriptor:pipelineDesc error:&error];
        if (!m_ShadowPipeline) {
            NOVA_LOG_FATAL("Metal shadow pipeline failed: {}",
                           error ? [[error localizedDescription] UTF8String] : "unknown");
            return false;
        }

        MTLTextureDescriptor* texDesc = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                           width:kShadowMapSize
                                          height:kShadowMapSize
                                       mipmapped:NO];
        texDesc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        texDesc.storageMode = MTLStorageModePrivate;
        m_ShadowMap = [m_Device newTextureWithDescriptor:texDesc];
        m_ShadowMap.label = @"NovaShadowMap";

        MTLSamplerDescriptor* shadowSamp = [[MTLSamplerDescriptor alloc] init];
        shadowSamp.minFilter = MTLSamplerMinMagFilterLinear;
        shadowSamp.magFilter = MTLSamplerMinMagFilterLinear;
        shadowSamp.sAddressMode = MTLSamplerAddressModeClampToEdge;
        shadowSamp.tAddressMode = MTLSamplerAddressModeClampToEdge;
        shadowSamp.compareFunction = MTLCompareFunctionLessEqual;
        m_ShadowSampler = [m_Device newSamplerStateWithDescriptor:shadowSamp];

        NOVA_LOG_INFO("Directional shadow map ready ({}x{})", kShadowMapSize, kShadowMapSize);
        return true;
    }

    void RenderShadowPass(id<MTLCommandBuffer> commandBuffer) {
        MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.depthAttachment.texture = m_ShadowMap;
        pass.depthAttachment.loadAction = MTLLoadActionClear;
        pass.depthAttachment.storeAction = MTLStoreActionStore;
        pass.depthAttachment.clearDepth = 1.0;

        id<MTLRenderCommandEncoder> encoder =
            [commandBuffer renderCommandEncoderWithDescriptor:pass];
        if (!encoder) return;

        MTLViewport viewport{};
        viewport.width = static_cast<double>(kShadowMapSize);
        viewport.height = static_cast<double>(kShadowMapSize);
        viewport.znear = 0.0;
        viewport.zfar = 1.0;
        [encoder setViewport:viewport];
        [encoder setRenderPipelineState:m_ShadowPipeline];
        [encoder setDepthStencilState:m_DepthStencilState];
        [encoder setFrontFacingWinding:MTLWindingCounterClockwise];
        [encoder setCullMode:MTLCullModeBack];
        DrawShadowMeshes(encoder);
        [encoder endEncoding];
    }

    bool CreateDefaultAlbedoTexture() {
        m_GpuTextures.clear();
        const ImageRGBA image = CreateCheckerboardImage(128, 8);
        if (!image.IsValid()) {
            NOVA_LOG_FATAL("Failed to create checkerboard texture image");
            return false;
        }
        return CreateGpuTexture(image) == kDefaultTextureGpuHandle;
    }

    bool m_Initialized = false;
    Window* m_Window = nullptr;
    uint32_t m_FbWidth = 0;
    uint32_t m_FbHeight = 0;
    Mat4 m_Model = Mat4::Identity();
    Mat4 m_ViewProj = Mat4::Identity();
    std::array<float, 4> m_ClearColor{0.08f, 0.09f, 0.12f, 1.0f};
    RenderViewport m_Viewport{};

    id<MTLDevice>               m_Device = nil;
    id<MTLCommandQueue>         m_Queue = nil;
    CAMetalLayer*               m_Layer = nil;
    id<CAMetalDrawable>         m_Drawable = nil;
    id<MTLCommandBuffer>        m_CommandBuffer = nil;
    id<MTLRenderCommandEncoder> m_Encoder = nil;
    id<MTLRenderPipelineState>  m_Pipeline = nil;
    std::vector<GpuMeshBuffers> m_GpuMeshes;
    id<MTLBuffer>               m_UniformBuffer = nil;
    id<MTLTexture>                m_DepthTexture = nil;
    std::vector<id<MTLTexture>>     m_GpuTextures;
    id<MTLSamplerState>           m_Sampler = nil;
    id<MTLDepthStencilState>      m_DepthStencilState = nil;
    id<MTLRenderPipelineState>  m_ShadowPipeline = nil;
    id<MTLTexture>                m_ShadowMap = nil;
    id<MTLSamplerState>           m_ShadowSampler = nil;
    FrameUniforms               m_Uniforms{};
    Material                    m_Material{};
    DirectionalLight            m_Light = DefaultDirectionalLight();
    PointLight                  m_PointLight{};
    ShadowSettings              m_ShadowSettings{};
    std::vector<MeshDrawItem>   m_MeshDraws;
    MTLRenderPassDescriptor*    m_FramePass = nil;
    RenderPassReadyCallback     m_PassReadyCallback = nullptr;
    void*                       m_PassReadyUser = nullptr;
    FrameOverlayCallback        m_OverlayCallback = nullptr;
    void*                       m_OverlayUser = nullptr;
};

std::unique_ptr<IRenderer> CreateRenderer() {
    return std::make_unique<MetalRenderer>();
}

} // namespace Nova
