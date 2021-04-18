#include <cassert>
#include <cstdint>

// This has the full definition for SDL_Renderer, which allows us to reference
// private fields inside it. Needs to be imported before SDL.h so we dont
// redefine anything in there, which would break the build.
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
  // Constant red
  // 0xffff0200, 0x0016fffe, 0x42415443, 0x0000001c, 0x00000023, 0xffff0200,
  // 0x00000000, 0x00000000, 0x20000100, 0x0000001c, 0x325f7370, 0x4d00305f,
  // 0x6f726369, 0x74666f73, 0x29522820, 0x534c4820, 0x6853204c, 0x72656461,
  // 0x6d6f4320, 0x656c6970, 0x2e392072, 0x392e3232, 0x322e3934, 0x00383432,
  // 0x05000051, 0xa00f0000, 0x3f800000, 0x00000000, 0x00000000, 0x3f800000,
  // 0x02000001, 0x800f0800, 0xa0e40000, 0x0000ffff

  // Read texture and write to screen as is
  // 0xffff0200, 0x0023fffe, 0x42415443, 0x0000001c, 0x00000057, 0xffff0200,
  // 0x00000001, 0x0000001c, 0x20000100, 0x00000050, 0x00000030, 0x00000003,
  // 0x00020001, 0x00000040, 0x00000000, 0x75706e69, 0x65745f74, 0x72757478,
  // 0xabab0065, 0x000c0004, 0x00010001, 0x00000001, 0x00000000, 0x325f7370,
  // 0x4d00305f, 0x6f726369, 0x74666f73, 0x29522820, 0x534c4820, 0x6853204c,
  // 0x72656461, 0x6d6f4320, 0x656c6970, 0x2e392072, 0x392e3232, 0x322e3934,
  // 0x00383432, 0x0200001f, 0x80000000, 0xb0030000, 0x0200001f, 0x90000000,
  // 0xa00f0800, 0x03000042, 0x800f0000, 0xb0e40000, 0xa0e40800, 0x02000001,
  // 0x800f0800, 0x80e40000, 0x0000ffff

  // Read texture and write to screen as is, ps_3_0 version
  0xffff0300, 0x0020fffe, 0x42415443, 0x0000001c, 0x0000004b, 0xffff0300,
  0x00000001, 0x0000001c, 0x20000100, 0x00000044, 0x00000030, 0x00000003,
  0x00020001, 0x00000034, 0x00000000, 0x00786574, 0x000c0004, 0x00010001,
  0x00000001, 0x00000000, 0x335f7370, 0x4d00305f, 0x6f726369, 0x74666f73,
  0x29522820, 0x534c4820, 0x6853204c, 0x72656461, 0x6d6f4320, 0x656c6970,
  0x2e392072, 0x392e3232, 0x322e3934, 0x00383432, 0x05000051, 0xa00f0000,
  0x43960000, 0x00000000, 0x00000000, 0x3f800000, 0x05000051, 0xa00f0001,
  0x40000000, 0x437f0000, 0x3b808081, 0x00000000, 0x05000030, 0xf00f0000,
  0x000000fe, 0x00000000, 0x00000000, 0x00000000, 0x0200001f, 0x80000005,
  0x90030000, 0x0200001f, 0x90000000, 0xa00f0800, 0x03000005, 0x80030000,
  0xa0000000, 0x90e40000, 0x03000042, 0x800f0000, 0x80e40000, 0xa0e40800,
  0x02000001, 0x800f0001, 0xa0ea0000, 0x02000001, 0x80010002, 0xa0aa0000,
  0x01000026, 0xf0e40000, 0x03000005, 0x80020002, 0x80550001, 0x80550001,
  0x04000004, 0x80020002, 0x80000001, 0x80000001, 0x81550002, 0x0400005a,
  0x80040002, 0x80000001, 0x80550001, 0xa0aa0000, 0x04000004, 0x80030001,
  0x90e40000, 0xa0000000, 0x80e90002, 0x03000002, 0x80060002, 0x81d00001,
  0xa0000001, 0x04000058, 0x80060002, 0x80e40002, 0xa0aa0000, 0xa0ff0000,
  0x03000002, 0x80020002, 0x80aa0002, 0x80550002, 0x04000058, 0x80010002,
  0x81550002, 0xa0aa0000, 0xa0ff0000, 0x02050029, 0x80000002, 0x81000002,
  0x02000001, 0x80040001, 0x80ff0001, 0x02000001, 0x80010002, 0xa0ff0000,
  0x0205002d, 0xa0ff0000, 0xa1ff0000, 0x0000002b, 0x03000002, 0x80080001,
  0x80ff0001, 0xa0ff0000, 0x00000027, 0x04000058, 0x80010001, 0x81000002,
  0xa0550001, 0x80aa0001, 0x03000005, 0x80010001, 0x80000001, 0xa0aa0001,
  0x02000001, 0x800e0001, 0xa0ec0000, 0x03000002, 0x800f0800, 0x80e40000,
  0x80e40001, 0x0000ffff

  // Mandelbrot
  // 0xffff0300, 0x0020fffe, 0x42415443, 0x0000001c, 0x0000004b, 0xffff0300,
  // 0x00000001, 0x0000001c, 0x20000100, 0x00000044, 0x00000030, 0x00000003,
  // 0x00020001, 0x00000034, 0x00000000, 0x00786574, 0x000c0004, 0x00010001,
  // 0x00000001, 0x00000000, 0x335f7370, 0x4d00305f, 0x6f726369, 0x74666f73,
  // 0x29522820, 0x534c4820, 0x6853204c, 0x72656461, 0x6d6f4320, 0x656c6970,
  // 0x2e392072, 0x392e3232, 0x322e3934, 0x00383432, 0x05000051, 0xa00f0000,
  // 0x00000000, 0x00000000, 0x3f800000, 0x40000000, 0x05000051, 0xa00f0001,
  // 0x437f0000, 0x3b808081, 0x00000000, 0x00000000, 0x05000030, 0xf00f0000,
  // 0x000000fe, 0x00000000, 0x00000000, 0x00000000, 0x0200001f, 0x80000005,
  // 0x90030000, 0x0200001f, 0x90000000, 0xa00f0800, 0x03000042, 0x800f0000,
  // 0x90e40000, 0xa0e40800, 0x02000001, 0x800f0001, 0xa0950000, 0x02000001,
  // 0x80010002, 0xa0550000, 0x01000026, 0xf0e40000, 0x03000005, 0x80020002,
  // 0x80550001, 0x80550001, 0x04000004, 0x80020002, 0x80000001, 0x80000001,
  // 0x81550002, 0x0400005a, 0x80040002, 0x80000001, 0x80550001, 0xa0550000,
  // 0x03000002, 0x80030001, 0x80e90002, 0x90e40000, 0x03000002, 0x80060002,
  // 0x81d00001, 0xa0ff0000, 0x04000058, 0x80060002, 0x80e40002, 0xa0550000,
  // 0xa0aa0000, 0x03000002, 0x80020002, 0x80aa0002, 0x80550002, 0x04000058,
  // 0x80010002, 0x81550002, 0xa0550000, 0xa0aa0000, 0x02050029, 0x80000002,
  // 0x81000002, 0x02000001, 0x80040001, 0x80ff0001, 0x02000001, 0x80010002,
  // 0xa0aa0000, 0x0205002d, 0xa0aa0000, 0xa1aa0000, 0x0000002b, 0x03000002,
  // 0x80080001, 0x80ff0001, 0xa0aa0000, 0x00000027, 0x04000058, 0x80010001,
  // 0x81000002, 0xa0000001, 0x80aa0001, 0x03000005, 0x80010001, 0x80000001,
  // 0xa0550001, 0x02000001, 0x800e0001, 0xa0980000, 0x03000002, 0x800f0800,
  // 0x80e40000, 0x80e40001, 0x0000ffff
};

static const DWORD vertex_shader_binary[] = {
  0xfffe0300, 0x0016fffe, 0x42415443, 0x0000001c, 0x00000023, 0xfffe0300,
  0x00000000, 0x00000000, 0x20000100, 0x0000001c, 0x335f7376, 0x4d00305f,
  0x6f726369, 0x74666f73, 0x29522820, 0x534c4820, 0x6853204c, 0x72656461,
  0x6d6f4320, 0x656c6970, 0x2e392072, 0x392e3232, 0x322e3934, 0x00383432,
  0x0200001f, 0x80000000, 0x900f0000, 0x0200001f, 0x80000005, 0x900f0001,
  0x0200001f, 0x80000000, 0xe00f0000, 0x0200001f, 0x80000005, 0xe00f0001,
  0x02000001, 0xe00f0000, 0x90e40000, 0x02000001, 0xe00f0001, 0x90e40001,
  0x0000ffff
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

IDirect3DPixelShader9* apply_hlsl_pixel_shader(SDL_Renderer* renderer, IDirect3DPixelShader9* shader) {
    IDirect3DDevice9* device = reinterpret_cast<D3D_RenderData*>(renderer->driverdata)->device;
    IDirect3DPixelShader9* current_shader;

    assert(D3D_OK == IDirect3DDevice9_GetPixelShader(device, &current_shader));

    assert(D3D_OK == IDirect3DDevice9_SetPixelShader(device, shader));

    return current_shader;
}

IDirect3DVertexShader9* apply_hlsl_vertex_shader(SDL_Renderer* renderer, IDirect3DVertexShader9* shader) {
    IDirect3DDevice9* device = reinterpret_cast<D3D_RenderData*>(renderer->driverdata)->device;
    IDirect3DVertexShader9* current_shader;

    assert(D3D_OK == IDirect3DDevice9_GetVertexShader(device, &current_shader));

    assert(D3D_OK == IDirect3DDevice9_SetVertexShader(device, shader));

    return current_shader;
}

void fill_with_little_squares(SDL_Texture *texture) {
  uint32_t *pixels;
  int pitch;

  SDL_LockTexture(texture, nullptr, reinterpret_cast<void**>(&pixels), &pitch);

  const size_t size = window_width*window_height;
  for (size_t i=0;i<size;i++) {
    // Chooses white or black, changes the color if we move in either the x or y
    // direction
    pixels[i] = ((i/window_width)+(i%window_width))%2 == 0 ? 0xffffffff : 0x000000ff;
  }

  SDL_UnlockTexture(texture);
}

IDirect3DPixelShader9* hlsl_pixel_shader(SDL_Renderer* renderer) {
  IDirect3DDevice9* device = reinterpret_cast<D3D_RenderData*>(renderer->driverdata)->device;
  IDirect3DPixelShader9 *shader;

  assert(D3D_OK == IDirect3DDevice9_CreatePixelShader(device, shader_binary, &shader));

  return shader;
}

IDirect3DVertexShader9* hlsl_vertex_shader(SDL_Renderer* renderer) {
  IDirect3DDevice9* device = reinterpret_cast<D3D_RenderData*>(renderer->driverdata)->device;
  IDirect3DVertexShader9 *shader;

  assert(D3D_OK == IDirect3DDevice9_CreateVertexShader(device, vertex_shader_binary, &shader));

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
                                           window_width, window_height);
  IDirect3DPixelShader9* shader = hlsl_pixel_shader(renderer);
  IDirect3DVertexShader9* vertex_shader = hlsl_vertex_shader(renderer);

  fill_with_little_squares(texture);

  // Main Loop
  while (true) {
    if (should_quit()) break;

    // Do the drawing using our HLSL shader
    const auto previous_shader = apply_hlsl_pixel_shader(renderer, shader);
    const auto previous_vertex_shader = apply_hlsl_vertex_shader(renderer, vertex_shader);
    {
      SDL_RenderCopy(renderer, texture, nullptr, nullptr);
      SDL_RenderFlush(renderer);
    }
    apply_hlsl_pixel_shader(renderer, previous_shader);
    apply_hlsl_vertex_shader(renderer, previous_vertex_shader);


    // ... show the drawing
    SDL_RenderPresent(renderer);
  }

  // FIXME: delete shaders

  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);

  return 0;
}
