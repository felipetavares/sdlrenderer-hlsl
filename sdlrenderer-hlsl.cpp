#include <cassert>
#include <cstdint>

// This has the full definition for SDL_Renderer, which allows us to reference
// private fields inside it. Needs to be imported before SDL.h so we dont
// redefine anything in there, which would break the build.
#include <SDL_sysrender.h>

#include <SDL.h>
#include <d3d9.h>

#include "fractal.h"

// These are from SDL_render_d3d.c (inside SDL)
// We need to copy them here because they are not included by the header files.
typedef struct
{
    SDL_Rect viewport;
    SDL_bool viewport_dirty;
    SDL_Texture *texture;
    SDL_BlendMode blend;
    SDL_bool cliprect_enabled;
    SDL_bool cliprect_enabled_dirty;
    SDL_Rect cliprect;
    SDL_bool cliprect_dirty;
    SDL_bool is_copy_ex;
    LPDIRECT3DPIXELSHADER9 shader;
} D3D_DrawStateCache;

typedef struct
{
    void* d3dDLL;
    IDirect3D9 *d3d;
    IDirect3DDevice9 *device;
    UINT adapter;
    D3DPRESENT_PARAMETERS pparams;
    SDL_bool updateSize;
    SDL_bool beginScene;
    SDL_bool enableSeparateAlphaBlend;
    D3DTEXTUREFILTERTYPE scaleMode[8];
    IDirect3DSurface9 *defaultRenderTarget;
    IDirect3DSurface9 *currentRenderTarget;
    void* d3dxDLL;
    LPDIRECT3DPIXELSHADER9 shaders[3];
    LPDIRECT3DVERTEXBUFFER9 vertexBuffers[8];
    size_t vertexBufferSize[8];
    int currentVertexBuffer;
    SDL_bool reportedVboProblem;
    D3D_DrawStateCache drawstate;
} D3D_RenderData;

const static char window_title[] = "SDL_Renderer + HLSL";
const static int window_width = 640;
const static int window_height = 480;

bool should_quit() {
  SDL_Event event;

  while (SDL_PollEvent(&event)) {
    switch(event.type) {
    case SDL_QUIT:
      return true;
      default:
        ; // We are only interested in the "quit" event
    }
  }

  return false;
}


SDL_Renderer* direct3d9_renderer(SDL_Window* window) {
  SDL_SetHint(SDL_HINT_RENDER_DRIVER, "direct3d9");
  const static int use_first_renderer_supporting_flags = -1;
  SDL_Renderer* renderer = SDL_CreateRenderer(window,
                                              use_first_renderer_supporting_flags,
                                              SDL_RENDERER_ACCELERATED);

  SDL_RendererInfo renderer_details;
  SDL_GetRendererInfo(renderer, &renderer_details);

  // Since we are relying on a hint to provide a Direct3D9 renderer, we may get
  // an OpenGL one still, if Direct3D9 is not supported. To avoid hard bugs
  // down the line, we make sure that we are returning a direct3d9 renderer.
  //
  // In a real application we would select the shading language (HLSL or GLSL)
  // based on the renderer we get instead of forcing it to be direct3d9.
  assert(!strcmp(renderer_details.name, "direct3d"));

  return renderer;
}

IDirect3DPixelShader9* apply_hlsl_pixel_shader(SDL_Renderer* renderer, IDirect3DPixelShader9* shader) {
    IDirect3DDevice9* device = reinterpret_cast<D3D_RenderData*>(renderer->driverdata)->device;
    IDirect3DPixelShader9* current_shader;

    assert(D3D_OK == IDirect3DDevice9_GetPixelShader(device, &current_shader));

    assert(D3D_OK == IDirect3DDevice9_SetPixelShader(device, shader));

    return current_shader;
}


void send_byte_using_texture(SDL_Texture *texture, uint8_t byte) {
  uint32_t *pixels;
  int pitch;

  SDL_LockTexture(texture, nullptr, reinterpret_cast<void**>(&pixels), &pitch);

  pixels[0] = byte;

  SDL_UnlockTexture(texture);
}

IDirect3DPixelShader9* hlsl_pixel_shader(SDL_Renderer* renderer) {
  IDirect3DDevice9* device = reinterpret_cast<D3D_RenderData*>(renderer->driverdata)->device;
  IDirect3DPixelShader9 *shader;

  assert(D3D_OK == IDirect3DDevice9_CreatePixelShader(device, g_ps21_main, &shader));

  return shader;
}


int main(int arg_count, char** arg_vector) {
  SDL_Window* window = SDL_CreateWindow(window_title,
                                        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                        window_width, window_height,
                                        SDL_WINDOW_SHOWN);
  SDL_Renderer* renderer = direct3d9_renderer(window);
  SDL_Texture* texture = SDL_CreateTexture(renderer,
                                           SDL_PIXELFORMAT_RGBA8888,
                                           SDL_TEXTUREACCESS_STREAMING,
                                           1, 1);
  IDirect3DPixelShader9* shader = hlsl_pixel_shader(renderer);

  uint8_t frame_counter = 0;

  // Main Loop
  while (true) {
    if (should_quit()) break;

    // Allow the shader to do effects that change with time
    send_byte_using_texture(texture, frame_counter++);

    // Do the drawing using our HLSL shader
    const auto previous_shader = apply_hlsl_pixel_shader(renderer, shader);
    {
      SDL_RenderCopy(renderer, texture, nullptr, nullptr);
      SDL_RenderFlush(renderer);
    }
    apply_hlsl_pixel_shader(renderer, previous_shader);


    // ... show the drawing
    SDL_RenderPresent(renderer);
  }

  // FIXME: delete shaders

  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);

  return 0;
}
