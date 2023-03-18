#include <stdbool.h>
#include <string.h>

#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include "xdg-shell-protocol.c"
#include "xdg-decoration-client-protocol.h"
#include "xdg-decoration-protocol.c"

#include <wayland-egl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <GL/gl.h>

bool GlobalRunning;

typedef struct wayland_state
{
    int BufferWidth;
    int BufferHeight;
    
    bool ShouldUpdateGeometry;
    bool WaitForConfigure;
    uint32_t ConfigureSerial;
    
    struct wl_display *Display;
    struct wl_registry *Registry;
    struct wl_compositor *Compositor;
    struct wl_surface *Surface;
    struct xdg_wm_base *XDGWMBase;
    struct xdg_toplevel *XDGToplevel;
    struct xdg_surface *XDGSurface;
    struct zxdg_toplevel_decoration_v1 *XDGToplevelDecoration;
    struct zxdg_decoration_manager_v1 *XDGDecorationManager;
    struct wl_egl_window *Window;
    
    struct wl_registry_listener RegistryListener;
    struct xdg_wm_base_listener XDGWMBaseListener;
    struct xdg_toplevel_listener XDGToplevelListener;
    struct xdg_surface_listener XDGSurfaceListener;
    struct zxdg_toplevel_decoration_v1_listener XDGToplevelDecorationListener;
} wayland_state;

typedef struct egl_state
{
    EGLDisplay Display;
    EGLContext Context;
    EGLConfig Config;
    EGLSurface Surface;
} egl_state;

void HandleRegistryGlobalRemove() {}

void HandleRegistryGlobal(void *Data, struct wl_registry *Registry, uint32_t Name,
                          const char *Interface, uint32_t Version)
{
    wayland_state *Wayland = Data;
    
    if(strcmp(Interface, wl_compositor_interface.name) == 0)
    {
        Wayland->Compositor = wl_registry_bind(Registry, Name, &wl_compositor_interface, 4);
    }
    else if(strcmp(Interface, xdg_wm_base_interface.name) == 0)
    {
        Wayland->XDGWMBase = wl_registry_bind(Registry, Name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(Wayland->XDGWMBase, &Wayland->XDGWMBaseListener, Wayland);
    }
    else if(strcmp(Interface, zxdg_decoration_manager_v1_interface.name) == 0)
    {
#if USE_DECORATIONS
        Wayland->XDGDecorationManager = wl_registry_bind(Registry, Name, &zxdg_decoration_manager_v1_interface, 1);
#endif
    }
}

void HandleXDGWMBasePing(void *Data, struct xdg_wm_base *XDGWMBase, uint32_t Serial)
{
    xdg_wm_base_pong(XDGWMBase, Serial);
}

void HandleXDGToplevelConfigure(void *Data, struct xdg_toplevel *XDGToplevel,
                                int RequestedWidth, int RequestedHeight, struct wl_array *WindowStates)
{
    wayland_state *Wayland = Data;
    
    if(RequestedWidth && RequestedHeight)
    {
        Wayland->BufferWidth = RequestedWidth;
        Wayland->BufferHeight = RequestedHeight;
    }
    
    Wayland->ShouldUpdateGeometry = true;
}

void HandleXDGToplevelClose(void *Data, struct xdg_toplevel *XDGToplevel)
{
    GlobalRunning = false;
}

void HandleXDGSurfaceConfigure(void *Data, struct xdg_surface *XDGSurface, uint32_t Serial)
{
    wayland_state *Wayland = Data;
    
    Wayland->WaitForConfigure = false;
    Wayland->ConfigureSerial = Serial;
}

void HandleXDGToplevelDecorationConfigure(void *Data, struct zxdg_toplevel_decoration_v1 *XDGToplevelDecoration, uint32_t Mode)
{
    zxdg_toplevel_decoration_v1_set_mode(XDGToplevelDecoration, Mode);
}

void UpdateGeometry(wayland_state *Wayland)
{
    if(Wayland->Window)
    {
        wl_egl_window_resize(Wayland->Window, Wayland->BufferWidth, Wayland->BufferHeight, 0, 0);
    }
    
    Wayland->ShouldUpdateGeometry = false;
}

int main(int ArgumentCount, char **Arguments)
{
    wayland_state WaylandState = {0};
    
    WaylandState.RegistryListener.global = HandleRegistryGlobal;
    WaylandState.RegistryListener.global_remove = HandleRegistryGlobalRemove;
    WaylandState.XDGWMBaseListener.ping = HandleXDGWMBasePing;
    WaylandState.XDGToplevelListener.configure = HandleXDGToplevelConfigure;
    WaylandState.XDGToplevelListener.close = HandleXDGToplevelClose;
    WaylandState.XDGSurfaceListener.configure = HandleXDGSurfaceConfigure;
    WaylandState.XDGToplevelDecorationListener.configure = HandleXDGToplevelDecorationConfigure;
    
    WaylandState.BufferWidth = 640;
    WaylandState.BufferHeight = 480;
    
    WaylandState.Display = wl_display_connect(0);
    WaylandState.Registry = wl_display_get_registry(WaylandState.Display);
    wl_registry_add_listener(WaylandState.Registry, &WaylandState.RegistryListener, &WaylandState);
    
    wl_display_roundtrip(WaylandState.Display);
    
    WaylandState.Surface = wl_compositor_create_surface(WaylandState.Compositor);
    
    WaylandState.XDGSurface = xdg_wm_base_get_xdg_surface(WaylandState.XDGWMBase, WaylandState.Surface);
    xdg_surface_add_listener(WaylandState.XDGSurface, &WaylandState.XDGSurfaceListener, &WaylandState);
    
    WaylandState.XDGToplevel = xdg_surface_get_toplevel(WaylandState.XDGSurface);
    xdg_toplevel_add_listener(WaylandState.XDGToplevel, &WaylandState.XDGToplevelListener, &WaylandState);
    
    if(WaylandState.XDGDecorationManager)
    {
        WaylandState.XDGToplevelDecoration = zxdg_decoration_manager_v1_get_toplevel_decoration(WaylandState.XDGDecorationManager, WaylandState.XDGToplevel);
        
        zxdg_toplevel_decoration_v1_add_listener(WaylandState.XDGToplevelDecoration, &WaylandState.XDGToplevelDecorationListener, &WaylandState);
        zxdg_toplevel_decoration_v1_set_mode(WaylandState.XDGToplevelDecoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    }
    
    WaylandState.WaitForConfigure = true;
    wl_surface_commit(WaylandState.Surface);
    
    int NumEvents = 0;
    while(WaylandState.WaitForConfigure &&  NumEvents != -1)
    {
        NumEvents = wl_display_dispatch(WaylandState.Display);
    }
    
    xdg_surface_ack_configure(WaylandState.XDGSurface, WaylandState.ConfigureSerial);
    WaylandState.ConfigureSerial = 0;

    if(WaylandState.ShouldUpdateGeometry)
    {
        UpdateGeometry(&WaylandState);
    }
    
    WaylandState.Window = wl_egl_window_create(WaylandState.Surface, WaylandState.BufferWidth, WaylandState.BufferHeight);
    
    egl_state EGLState = {0};
    
    EGLState.Display = eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR, WaylandState.Display, 0);
    
    EGLint Major;
    EGLint Minor;
    eglInitialize(EGLState.Display, &Major, &Minor);
    
    eglBindAPI(EGL_OPENGL_API);
    
    EGLint ConfigAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_CONFORMANT, EGL_OPENGL_BIT,
        EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,
        
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_GREEN_SIZE, 8,
        EGL_NONE
    };
    
    EGLAttrib SurfaceAttribs[] =
    {
        EGL_RENDER_BUFFER, EGL_BACK_BUFFER,
        EGL_NONE,
    };
    
    EGLint ContextAttribs[] =
    {
        EGL_CONTEXT_MAJOR_VERSION, 4,
        EGL_CONTEXT_MINOR_VERSION, 5,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT,
        //EGL_CONTEXT_OPENGL_DEBUG, EGL_TRUE,
        EGL_NONE,
    };
    
    int Dummy;
    eglChooseConfig(EGLState.Display, ConfigAttribs, &EGLState.Config, 1, &Dummy);
    
    EGLState.Surface = eglCreatePlatformWindowSurface(EGLState.Display, EGLState.Config,
                                                      WaylandState.Window, SurfaceAttribs);
    
    EGLState.Context = eglCreateContext(EGLState.Display, EGLState.Config, EGL_NO_CONTEXT, ContextAttribs);
    
    eglMakeCurrent(EGLState.Display, EGLState.Surface, EGLState.Surface, EGLState.Context);
    
    int FrameCounter = 0;
    
    GlobalRunning = true;
    while(GlobalRunning)
    {
        int NumEvents = wl_display_dispatch_pending(WaylandState.Display);
        if(NumEvents == -1)
        {
            GlobalRunning = false;
            continue;
        }
        
        if(++FrameCounter == 60)
        {
            WaylandState.BufferWidth = 1280;
            WaylandState.BufferHeight = 720;
            WaylandState.ShouldUpdateGeometry = true;
        }

        if(WaylandState.ShouldUpdateGeometry)
        {
            UpdateGeometry(&WaylandState);
        }

        if(WaylandState.ConfigureSerial)
        {
            xdg_surface_ack_configure(WaylandState.XDGSurface, WaylandState.ConfigureSerial);
            wl_surface_commit(WaylandState.Surface);
            WaylandState.ConfigureSerial = 0;
        }
        
        glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        eglSwapBuffers(EGLState.Display, EGLState.Surface);
    }
    
    return 0;
}
