/**
 * @file IOSGLContext.mm
 * @brief OpenGL ES context implementation for iOS
 */

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES3/gl.h>
#import <OpenGLES/ES3/glext.h>
#import <QuartzCore/QuartzCore.h>

#include "IOSGLContext.hpp"

namespace Nova {

IOSGLContext::IOSGLContext() = default;

IOSGLContext::~IOSGLContext() {
    DestroyContext();
}

bool IOSGLContext::CreateContext() {
    @autoreleasepool {
        // Try to create OpenGL ES 3.0 context first
        EAGLContext* context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];

        if (!context) {
            // Fall back to OpenGL ES 2.0
            NSLog(@"Nova3D: OpenGL ES 3.0 not available, falling back to ES 2.0");
            context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
        }

        if (!context) {
            NSLog(@"Nova3D: Failed to create OpenGL ES context");
            return false;
        }

        m_context = (__bridge_retained void*)context;

        // Make current
        if (![EAGLContext setCurrentContext:context]) {
            NSLog(@"Nova3D: Failed to set current OpenGL ES context");
            DestroyContext();
            return false;
        }

        NSLog(@"Nova3D: OpenGL ES context created successfully");
        NSLog(@"Nova3D:   Version: %s", glGetString(GL_VERSION));
        NSLog(@"Nova3D:   Renderer: %s", glGetString(GL_RENDERER));
        NSLog(@"Nova3D:   Vendor: %s", glGetString(GL_VENDOR));

        return true;
    }
}

void IOSGLContext::DestroyContext() {
    @autoreleasepool {
        DeleteFramebuffer();

        if (m_context) {
            EAGLContext* context = (__bridge_transfer EAGLContext*)m_context;
            if ([EAGLContext currentContext] == context) {
                [EAGLContext setCurrentContext:nil];
            }
            m_context = nullptr;
        }
    }
}

bool IOSGLContext::MakeCurrent() {
    @autoreleasepool {
        if (!m_context) return false;
        EAGLContext* context = (__bridge EAGLContext*)m_context;
        return [EAGLContext setCurrentContext:context];
    }
}

void IOSGLContext::ClearCurrent() {
    [EAGLContext setCurrentContext:nil];
}

void IOSGLContext::CreateFramebuffer(int width, int height) {
    @autoreleasepool {
        if (!m_context) return;

        MakeCurrent();

        // Delete existing framebuffer if any
        DeleteFramebuffer();

        m_framebufferWidth = width;
        m_framebufferHeight = height;

        // Create the main framebuffer
        glGenFramebuffers(1, &m_framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

        // Create color renderbuffer
        glGenRenderbuffers(1, &m_colorRenderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, m_colorRenderbuffer);

        // Connect the renderbuffer to the drawable layer if available
        if (m_drawableLayer) {
            EAGLContext* context = (__bridge EAGLContext*)m_context;
            CAEAGLLayer* layer = (__bridge CAEAGLLayer*)m_drawableLayer;
            [context renderbufferStorage:GL_RENDERBUFFER fromDrawable:layer];

            // Get the actual dimensions from the layer
            glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &m_framebufferWidth);
            glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &m_framebufferHeight);
        } else {
            // Allocate storage manually
            glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
        }

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_colorRenderbuffer);

        // Create depth renderbuffer
        glGenRenderbuffers(1, &m_depthRenderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, m_depthRenderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_framebufferWidth, m_framebufferHeight);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthRenderbuffer);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthRenderbuffer);

        // Check framebuffer status
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            NSLog(@"Nova3D: Failed to create framebuffer: 0x%x", status);
            DeleteFramebuffer();
            return;
        }

        NSLog(@"Nova3D: Created framebuffer %dx%d", m_framebufferWidth, m_framebufferHeight);

        // Set viewport
        glViewport(0, 0, m_framebufferWidth, m_framebufferHeight);
    }
}

void IOSGLContext::ResizeFramebuffer(int width, int height) {
    if (width != m_framebufferWidth || height != m_framebufferHeight) {
        CreateFramebuffer(width, height);
    }
}

void IOSGLContext::DeleteFramebuffer() {
    @autoreleasepool {
        if (!m_context) return;

        MakeCurrent();

        if (m_framebuffer) {
            glDeleteFramebuffers(1, &m_framebuffer);
            m_framebuffer = 0;
        }
        if (m_colorRenderbuffer) {
            glDeleteRenderbuffers(1, &m_colorRenderbuffer);
            m_colorRenderbuffer = 0;
        }
        if (m_depthRenderbuffer) {
            glDeleteRenderbuffers(1, &m_depthRenderbuffer);
            m_depthRenderbuffer = 0;
        }
        if (m_stencilRenderbuffer) {
            glDeleteRenderbuffers(1, &m_stencilRenderbuffer);
            m_stencilRenderbuffer = 0;
        }
        if (m_msaaFramebuffer) {
            glDeleteFramebuffers(1, &m_msaaFramebuffer);
            m_msaaFramebuffer = 0;
        }
        if (m_msaaColorRenderbuffer) {
            glDeleteRenderbuffers(1, &m_msaaColorRenderbuffer);
            m_msaaColorRenderbuffer = 0;
        }
    }
}

void IOSGLContext::BindFramebuffer() {
    if (m_useMSAA && m_msaaFramebuffer) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_msaaFramebuffer);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    }
}

void IOSGLContext::Present() {
    @autoreleasepool {
        if (!m_context || m_paused) return;

        MakeCurrent();

        // Resolve MSAA if enabled
        if (m_useMSAA && m_msaaFramebuffer && m_framebuffer) {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, m_msaaFramebuffer);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_framebuffer);
            glBlitFramebuffer(0, 0, m_framebufferWidth, m_framebufferHeight,
                              0, 0, m_framebufferWidth, m_framebufferHeight,
                              GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }

        // Discard depth/stencil data for better performance
        const GLenum discards[] = { GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT };
        glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
        glInvalidateFramebuffer(GL_FRAMEBUFFER, 2, discards);

        // Present the color renderbuffer
        glBindRenderbuffer(GL_RENDERBUFFER, m_colorRenderbuffer);
        EAGLContext* context = (__bridge EAGLContext*)m_context;
        [context presentRenderbuffer:GL_RENDERBUFFER];
    }
}

void IOSGLContext::SetDrawableLayer(void* layer) {
    m_drawableLayer = layer;
}

void IOSGLContext::Pause() {
    m_paused = true;
    NSLog(@"Nova3D: OpenGL ES context paused");
}

void IOSGLContext::Resume() {
    m_paused = false;
    MakeCurrent();
    NSLog(@"Nova3D: OpenGL ES context resumed");
}

const char* IOSGLContext::GetVersionString() const {
    if (!m_context) return "Unknown";
    MakeCurrent();
    return reinterpret_cast<const char*>(glGetString(GL_VERSION));
}

const char* IOSGLContext::GetRendererString() const {
    if (!m_context) return "Unknown";
    MakeCurrent();
    return reinterpret_cast<const char*>(glGetString(GL_RENDERER));
}

bool IOSGLContext::CheckError(const char* operation) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        const char* errorStr = "Unknown";
        switch (error) {
            case GL_INVALID_ENUM: errorStr = "GL_INVALID_ENUM"; break;
            case GL_INVALID_VALUE: errorStr = "GL_INVALID_VALUE"; break;
            case GL_INVALID_OPERATION: errorStr = "GL_INVALID_OPERATION"; break;
            case GL_OUT_OF_MEMORY: errorStr = "GL_OUT_OF_MEMORY"; break;
            case GL_INVALID_FRAMEBUFFER_OPERATION: errorStr = "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
        }
        if (operation) {
            NSLog(@"Nova3D: OpenGL error in '%s': %s (0x%x)", operation, errorStr, error);
        } else {
            NSLog(@"Nova3D: OpenGL error: %s (0x%x)", errorStr, error);
        }
        return false;
    }
    return true;
}

} // namespace Nova
