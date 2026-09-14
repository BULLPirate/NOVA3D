#include <Nova/Renderer/Renderer.h>
#include <Nova/Renderer/Camera.h>
#include <Nova/Renderer/Mesh.h>
#include <Nova/Math/Mat4.h>
#include <Nova/Platform/Window.h>
#include <Nova/Core/Log.h>

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <array>
#include <cstring>

namespace Nova {

static const char* kMeshShader = R"(
#include <metal_stdlib>
using namespace metal;

struct FrameUniforms {
    float4x4 mvp;
};

struct VertexIn {
    float3 position [[attribute(0)]];
    float4 color    [[attribute(1)]];
};

struct VertexOut {
    float4 position [[position]];
    float4 color;
};

vertex VertexOut vertex_main(VertexIn in [[stage_in]],
                             constant FrameUniforms& u [[buffer(1)]]) {
    VertexOut out;
    out.position = u.mvp * float4(in.position, 1.0);
    out.color = in.color;
    return out;
}

fragment float4 fragment_main(VertexOut in [[stage_in]]) {
    return in.color;
}
)";

struct FrameUniforms {
    float mvp[16];
};

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
        m_VertexBuffer = nil;
        m_IndexBuffer = nil;
        m_UniformBuffer = nil;
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

        m_CommandBuffer = [m_Queue commandBuffer];
        m_Encoder = [m_CommandBuffer renderCommandEncoderWithDescriptor:pass];
    }

    void EndFrame() override {
        if (!m_CommandBuffer) return;

        if (m_Encoder) {
            if (m_Pipeline && m_VertexBuffer && m_IndexBuffer) {
                MTLViewport viewport{};
                viewport.originX = 0;
                viewport.originY = 0;
                viewport.width = static_cast<double>(m_FbWidth);
                viewport.height = static_cast<double>(m_FbHeight);
                viewport.znear = 0.0;
                viewport.zfar = 1.0;
                [m_Encoder setViewport:viewport];
                [m_Encoder setRenderPipelineState:m_Pipeline];
                [m_Encoder setVertexBuffer:m_VertexBuffer offset:0 atIndex:0];
                [m_Encoder setVertexBuffer:m_UniformBuffer offset:0 atIndex:1];
                [m_Encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                      indexCount:m_IndexCount
                                       indexType:MTLIndexTypeUInt32
                                     indexBuffer:m_IndexBuffer
                               indexBufferOffset:0];
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

    void SetClearColor(float r, float g, float b, float a) override {
        m_ClearColor = {r, g, b, a};
    }

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
        UploadMvp();
    }

    void SetModelMatrix(const Mat4& model) override {
        m_Model = model;
        UploadMvp();
    }

private:
    void UploadMvp() {
        const Mat4 mvp = m_ViewProj * m_Model;
        std::memcpy(m_Uniforms.mvp, mvp.Data(), sizeof(m_Uniforms.mvp));
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
        vertexDesc.attributes[1].format = MTLVertexFormatFloat4;
        vertexDesc.attributes[1].offset = sizeof(float) * 3;
        vertexDesc.attributes[1].bufferIndex = 0;
        vertexDesc.layouts[0].stride = sizeof(ColoredVertex);
        vertexDesc.layouts[0].stepRate = 1;
        vertexDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

        MTLRenderPipelineDescriptor* pipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
        pipelineDesc.label = @"NovaColoredMesh";
        pipelineDesc.vertexFunction = vs;
        pipelineDesc.fragmentFunction = fs;
        pipelineDesc.vertexDescriptor = vertexDesc;
        pipelineDesc.colorAttachments[0].pixelFormat = m_Layer.pixelFormat;
        pipelineDesc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;

        m_Pipeline = [m_Device newRenderPipelineStateWithDescriptor:pipelineDesc error:&error];
        if (!m_Pipeline) {
            NOVA_LOG_FATAL("Metal pipeline creation failed: {}",
                           error ? [[error localizedDescription] UTF8String] : "unknown");
            return false;
        }

        const MeshData cube = CreateUnitCubeMesh();
        m_IndexCount = static_cast<NSUInteger>(cube.Indices.size());
        m_VertexBuffer = [m_Device newBufferWithBytes:cube.Vertices.data()
                                               length:cube.Vertices.size() * sizeof(ColoredVertex)
                                              options:MTLResourceStorageModeShared];
        m_VertexBuffer.label = @"NovaMeshVB";
        m_IndexBuffer = [m_Device newBufferWithBytes:cube.Indices.data()
                                              length:cube.Indices.size() * sizeof(uint32_t)
                                             options:MTLResourceStorageModeShared];
        m_IndexBuffer.label = @"NovaMeshIB";

        m_UniformBuffer = [m_Device newBufferWithLength:sizeof(FrameUniforms)
                                                options:MTLResourceStorageModeShared];
        m_UniformBuffer.label = @"NovaFrameUBO";
        m_Model = Mat4::Identity();
        m_ViewProj = Mat4::Identity();
        UploadMvp();

        NOVA_LOG_INFO("Mesh pipeline ready (indexed cube + depth buffer)");
        return true;
    }

    bool m_Initialized = false;
    Window* m_Window = nullptr;
    uint32_t m_FbWidth = 0;
    uint32_t m_FbHeight = 0;
    NSUInteger m_IndexCount = 0;
    Mat4 m_Model = Mat4::Identity();
    Mat4 m_ViewProj = Mat4::Identity();
    std::array<float, 4> m_ClearColor{0.08f, 0.09f, 0.12f, 1.0f};

    id<MTLDevice>               m_Device = nil;
    id<MTLCommandQueue>         m_Queue = nil;
    CAMetalLayer*               m_Layer = nil;
    id<CAMetalDrawable>         m_Drawable = nil;
    id<MTLCommandBuffer>        m_CommandBuffer = nil;
    id<MTLRenderCommandEncoder> m_Encoder = nil;
    id<MTLRenderPipelineState>  m_Pipeline = nil;
    id<MTLBuffer>               m_VertexBuffer = nil;
    id<MTLBuffer>               m_IndexBuffer = nil;
    id<MTLBuffer>               m_UniformBuffer = nil;
    id<MTLTexture>                m_DepthTexture = nil;
    FrameUniforms               m_Uniforms{};
};

std::unique_ptr<IRenderer> CreateRenderer() {
    return std::make_unique<MetalRenderer>();
}

} // namespace Nova
