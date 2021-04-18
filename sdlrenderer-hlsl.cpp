#include <cassert>

// This has the full definition for SDL_Renderer, which allows us to reference
// private fields inside it.
#include <SDL_sysrender.h>

#include <SDL.h>
#include <d3d9.h>


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

// This is the pre-compiled HLSL shader, to run something else one would need to
// recompile the shader and put the result here.
static const DWORD shader_binary[] = {
    0xffff0201, 0x0023fffe, 0x42415443, 0x0000001c, 0x00000057, 0xffff0201,
    0x00000001, 0x0000001c, 0x20000100, 0x00000050, 0x00000030, 0x00000003,
    0x00020001, 0x00000040, 0x00000000, 0x65726373, 0x745f6e65, 0x75747865,
    0xab006572, 0x000c0004, 0x00010001, 0x00000001, 0x00000000, 0x325f7370,
    0x4d00615f, 0x6f726369, 0x74666f73, 0x29522820, 0x534c4820, 0x6853204c,
    0x72656461, 0x6d6f4320, 0x656c6970, 0x2e392072, 0x392e3232, 0x322e3934,
    0x00383432, 0x05000051, 0xa00f0000, 0x42c80000, 0x43800000, 0x43c80000,
    0x3f000000, 0x05000051, 0xa00f0001, 0x40800000, 0x3e800000, 0xc0800000,
    0xbe800000, 0x05000051, 0xa00f0002, 0x47bcc000, 0x00000000, 0x40800000,
    0x3e800000, 0x05000051, 0xa00f0003, 0x00000000, 0x3f800000, 0x3c23d70a,
    0x3b800000, 0x05000051, 0xa00f0004, 0x3f800000, 0xc0800000, 0xbe800000,
    0x46bb8000, 0x05000051, 0xa00f0005, 0x40400000, 0x40000000, 0x3f800000,
    0x00000000, 0x05000051, 0xa00f0006, 0x42c80000, 0x3c23d70a, 0x00000000,
    0xbf800000, 0x0200001f, 0x80000000, 0xb0030000, 0x0200001f, 0x90000000,
    0xa00f0800, 0x03000005, 0x80070000, 0xb0c40000, 0xa0e40000, 0x02000013,
    0x80070001, 0x80e40000, 0x03000002, 0x80070000, 0x80e40000, 0x81e40001,
    0x04000058, 0x80070001, 0x81e40001, 0xa0000003, 0xa0550003, 0x04000058,
    0x80070002, 0xb0c40000, 0xa0000003, 0xa0550003, 0x04000004, 0x80070000,
    0x80e40002, 0x80e40001, 0x80e40000, 0x04000058, 0x80030001, 0x80aa0000,
    0xa0e40001, 0xa0ee0001, 0x03000005, 0x80040000, 0x80aa0000, 0x80550001,
    0x03000002, 0x80030000, 0x80e40000, 0xa0ff0000, 0x03000005, 0x80030000,
    0x80e40000, 0xa0ee0003, 0x03000042, 0x800f0002, 0x80e40000, 0xa0e40800,
    0x02000013, 0x80010000, 0x80aa0000, 0x03000005, 0x80010000, 0x80000001,
    0x80000000, 0x02000013, 0x80020000, 0x80000000, 0x03000002, 0x80040000,
    0x80000000, 0x81550000, 0x04000058, 0x80010000, 0x80000000, 0xa0000003,
    0xa0550003, 0x04000058, 0x80020000, 0x81550000, 0xa0000003, 0xa0550003,
    0x04000004, 0x80010000, 0x80000000, 0x80550000, 0x80aa0000, 0x03000002,
    0x800f0000, 0x81000000, 0xa0e40005, 0x03000005, 0x800f0000, 0x80e40000,
    0x80e40000, 0x04000058, 0x800f0000, 0x81e40000, 0xa0550003, 0xa0000003,
    0x03000009, 0x80010000, 0x80e40002, 0x80e40000, 0x04000004, 0x80010000,
    0x80000000, 0xa0550000, 0xa0ff0000, 0x02000013, 0x80020000, 0x80000000,
    0x03000002, 0x80010000, 0x80000000, 0x81550000, 0x03000002, 0x80010000,
    0x80000000, 0xa0000002, 0x03000005, 0x80020000, 0x80000000, 0xa0550001,
    0x02000013, 0x80040000, 0x80550000, 0x03000002, 0x80020000, 0x80550000,
    0x81aa0000, 0x04000058, 0x80040000, 0x81aa0000, 0xa0000003, 0xa0550003,
    0x02000001, 0x800e0001, 0xa0e40002, 0x04000058, 0x80070001, 0x80000000,
    0x80f90001, 0xa0e40004, 0x03000005, 0x80010000, 0x80000000, 0x80aa0001,
    0x02000013, 0x80010000, 0x80000000, 0x03000005, 0x80010000, 0x80550001,
    0x80000000, 0x04000004, 0x80020000, 0x80000001, 0x80aa0000, 0x80550000,
    0x04000058, 0x80070001, 0x80550000, 0xa0e40006, 0xa1f40006, 0x03000005,
    0x80040000, 0x80550000, 0x80550001, 0x03000005, 0x80020000, 0x80550000,
    0xa0aa0003, 0x02000013, 0x80040000, 0x80aa0000, 0x03000005, 0x80040000,
    0x80000001, 0x80aa0000, 0x02000013, 0x80080000, 0x80aa0000, 0x03000002,
    0x80010001, 0x80aa0000, 0x81ff0000, 0x04000058, 0x80040000, 0x80aa0000,
    0xa0000003, 0xa0550003, 0x04000058, 0x80080000, 0x81ff0000, 0xa0000003,
    0xa0550003, 0x04000004, 0x80040000, 0x80aa0000, 0x80ff0000, 0x80000001,
    0x03000002, 0x80040000, 0x80aa0000, 0xa0ff0000, 0x03000005, 0x80040000,
    0x80aa0000, 0xa0aa0003, 0x0300000b, 0x80010001, 0x80aa0000, 0xa0000003,
    0x02000013, 0x80080001, 0x80550000, 0x03000002, 0x80020000, 0x80550000,
    0x81ff0001, 0x04000058, 0x80080001, 0x81ff0001, 0xa0000003, 0xa0550003,
    0x04000004, 0x80020000, 0x80aa0001, 0x80ff0001, 0x80550000, 0x03000002,
    0x80020000, 0x80550000, 0xa0ff0000, 0x03000005, 0x80120001, 0x80550000,
    0xa0ff0003, 0x03000042, 0x800f0001, 0x80e40001, 0xa0e40800, 0x02000013,
    0x80020000, 0x80000000, 0x03000002, 0x80040000, 0x80000000, 0x81550000,
    0x04000058, 0x80020000, 0x81550000, 0xa0000003, 0xa0550003, 0x04000058,
    0x80010000, 0x80000000, 0xa0000003, 0xa0550003, 0x04000004, 0x80010000,
    0x80000000, 0x80550000, 0x80aa0000, 0x03000002, 0x800f0000, 0x81000000,
    0xa0e40005, 0x03000005, 0x800f0000, 0x80e40000, 0x80e40000, 0x04000058,
    0x800f0000, 0x81e40000, 0xa0550003, 0xa0000003, 0x03000009, 0x80010000,
    0x80e40001, 0x80e40000, 0x04000004, 0x80010000, 0x80000000, 0xa0550000,
    0xa0ff0000, 0x02000013, 0x80020000, 0x80000000, 0x03000002, 0x80010000,
    0x80000000, 0x81550000, 0x03000002, 0x80010000, 0x80000000, 0xa0ff0004,
    0x04000058, 0x800e0000, 0x80000000, 0xa0900006, 0xa1d00006, 0x03000005,
    0x80040000, 0x80000000, 0x80aa0000, 0x03000005, 0x80010000, 0x80000000,
    0xa0aa0003, 0x02000013, 0x80040000, 0x80aa0000, 0x03000005, 0x80020000,
    0x80550000, 0x80aa0000, 0x02000013, 0x80040000, 0x80550000, 0x03000002,
    0x80010001, 0x80550000, 0x81aa0000, 0x04000058, 0x80020000, 0x80550000,
    0xa0000003, 0xa0550003, 0x04000058, 0x80040000, 0x81aa0000, 0xa0000003,
    0xa0550003, 0x04000004, 0x80020000, 0x80550000, 0x80aa0000, 0x80000001,
    0x03000002, 0x80020000, 0x80550000, 0xa0ff0000, 0x03000005, 0x80020000,
    0x80550000, 0xa0aa0003, 0x0300000b, 0x80010001, 0x80550000, 0xa0000003,
    0x02000013, 0x80020000, 0x80000000, 0x03000002, 0x80010000, 0x80000000,
    0x81550000, 0x04000058, 0x80020000, 0x81550000, 0xa0000003, 0xa0550003,
    0x04000004, 0x80010000, 0x80ff0000, 0x80550000, 0x80000000, 0x03000002,
    0x80010000, 0x80000000, 0xa0ff0000, 0x03000005, 0x80120001, 0x80000000,
    0xa0ff0003, 0x03000042, 0x800f0000, 0x80e40001, 0xa0e40800, 0x02000001,
    0x800f0000, 0x801b0000, 0x02000001, 0x800f0800, 0x80e40000, 0x0000ffff
};

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

IDirect3DPixelShader9* apply_hlsl_shader(SDL_Renderer* renderer, IDirect3DPixelShader9* shader) {
    IDirect3DDevice9* device = ((D3D_RenderData*)renderer->driverdata)->device;
    IDirect3DPixelShader9* current_shader;

    assert(D3D_OK == IDirect3DDevice9_GetPixelShader(device, &current_shader));

    assert(D3D_OK == IDirect3DDevice9_SetPixelShader(device, shader));

    return current_shader;
}


void fill_with_little_squares(SDL_Texture *texture) {
  // TODO
}

IDirect3DPixelShader9* hlsl_shader(SDL_Renderer* renderer) {
  IDirect3DDevice9* device = ((D3D_RenderData*)renderer->driverdata)->device;
  IDirect3DPixelShader9 *shader;

  assert(D3D_OK == IDirect3DDevice9_CreatePixelShader(device, shader_binary, &shader));

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
                                           SDL_TEXTUREACCESS_STATIC,
                                           window_width, window_height);
  IDirect3DPixelShader9* shader = hlsl_shader(renderer);

  fill_with_little_squares(texture);

  // Main Loop
  while (true) {
    if (should_quit()) break;

    // Do the drawing using our HLSL shader
    const auto previous_shader = apply_hlsl_shader(renderer, shader); {
      SDL_RenderCopy(renderer, texture, nullptr, nullptr);
      SDL_RenderFlush(renderer);
    } apply_hlsl_shader(renderer, previous_shader);

    // ... show the drawing
    SDL_RenderPresent(renderer);
  }

  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);

  return 0;
}
