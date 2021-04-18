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
    0x43425844, 0xd4d552dc, 0x3896d0cb, 0x84ff16ef, 0xc8266d44, 0x00000001,
    0x000003e0, 0x00000005, 0x00000034, 0x0000008c, 0x000000c0, 0x000000f4,
    0x00000364, 0x46454452, 0x00000050, 0x00000000, 0x00000000, 0x00000000,
    0x0000001c, 0xffff0400, 0x00000100, 0x0000001c, 0x7263694d, 0x666f736f,
    0x52282074, 0x4c482029, 0x53204c53, 0x65646168, 0x6f432072, 0x6c69706d,
    0x39207265, 0x2e32322e, 0x2e393439, 0x38343232, 0xababab00, 0x4e475349,
    0x0000002c, 0x00000001, 0x00000008, 0x00000020, 0x00000000, 0x00000000,
    0x00000003, 0x00000000, 0x00000303, 0x43584554, 0x44524f4f, 0xababab00,
    0x4e47534f, 0x0000002c, 0x00000001, 0x00000008, 0x00000020, 0x00000000,
    0x00000000, 0x00000003, 0x00000000, 0x0000000f, 0x545f5653, 0x65677261,
    0xabab0074, 0x52444853, 0x00000268, 0x00000040, 0x0000009a, 0x03001062,
    0x00101032, 0x00000000, 0x03000065, 0x001020f2, 0x00000000, 0x02000068,
    0x00000002, 0x08000036, 0x001000f2, 0x00000000, 0x00004002, 0x00000000,
    0x00000000, 0x00000000, 0x00000001, 0x05000036, 0x00100012, 0x00000001,
    0x00004001, 0x00000000, 0x01000030, 0x07000021, 0x00100022, 0x00000001,
    0x0010003a, 0x00000000, 0x00004001, 0x000000ff, 0x05000036, 0x00100012,
    0x00000001, 0x00004001, 0x00000000, 0x03040003, 0x0010001a, 0x00000001,
    0x07000038, 0x00100022, 0x00000001, 0x0010001a, 0x00000000, 0x0010001a,
    0x00000000, 0x0a000032, 0x00100022, 0x00000001, 0x0010000a, 0x00000000,
    0x0010000a, 0x00000000, 0x8010001a, 0x00000041, 0x00000001, 0x0700000f,
    0x00100042, 0x00000001, 0x00100006, 0x00000000, 0x00100556, 0x00000000,
    0x07000000, 0x00100032, 0x00000000, 0x00100596, 0x00000001, 0x00101046,
    0x00000000, 0x0a000031, 0x00100062, 0x00000001, 0x00004002, 0x00000000,
    0x40000000, 0x40000000, 0x00000000, 0x00100106, 0x00000000, 0x0700003c,
    0x00100012, 0x00000001, 0x0010002a, 0x00000001, 0x0010001a, 0x00000001,
    0x0304001f, 0x0010000a, 0x00000001, 0x05000036, 0x00100042, 0x00000000,
    0x0010003a, 0x00000000, 0x05000036, 0x00100012, 0x00000001, 0x00004001,
    0xffffffff, 0x01000002, 0x01000015, 0x0700001e, 0x00100082, 0x00000000,
    0x0010003a, 0x00000000, 0x00004001, 0x00000001, 0x01000016, 0x09000037,
    0x00100012, 0x00000000, 0x0010000a, 0x00000001, 0x0010002a, 0x00000000,
    0x00004001, 0x000000ff, 0x0500002b, 0x00100012, 0x00000000, 0x0010000a,
    0x00000000, 0x07000038, 0x00100012, 0x00000000, 0x0010000a, 0x00000000,
    0x00004001, 0x3b808081, 0x0600004d, 0x0000d000, 0x00102022, 0x00000000,
    0x0010000a, 0x00000000, 0x0600004d, 0x00102042, 0x00000000, 0x0000d000,
    0x0010000a, 0x00000000, 0x05000036, 0x00102082, 0x00000000, 0x00004001,
    0x3f800000, 0x05000036, 0x00102012, 0x00000000, 0x0010000a, 0x00000000,
    0x0100003e, 0x54415453, 0x00000074, 0x0000001b, 0x00000002, 0x00000000,
    0x00000002, 0x00000007, 0x00000002, 0x00000001, 0x00000002, 0x00000002,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000018, 0x00000001,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000
};

static const DWORD vertex_shader_binary[] = {
  0xfffe0300, 0x0016fffe, 0x42415443, 0x0000001c, 0x00000023, 0xfffe0300,
  0x00000000, 0x00000000, 0x20000100, 0x0000001c, 0x335f7376, 0x4d00305f,
  0x6f726369, 0x74666f73, 0x29522820, 0x534c4820, 0x6853204c, 0x72656461,
  0x6d6f4320, 0x656c6970, 0x2e392072, 0x392e3232, 0x322e3934, 0x00383432,
  0x05000051, 0xa00f0000, 0x41200000, 0x00000000, 0x00000000, 0x00000000,
  0x0200001f, 0x80000000, 0x900f0000, 0x0200001f, 0x80000005, 0x900f0001,
  0x0200001f, 0x80000000, 0xe00f0000, 0x0200001f, 0x80000005, 0xe0030001,
  0x03000005, 0xe0030001, 0xa0000000, 0x90e40001, 0x02000001, 0xe00f0000,
  0x90e40000, 0x0000ffff
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
    //const auto previous_vertex_shader = apply_hlsl_vertex_shader(renderer, vertex_shader);
    {
      SDL_RenderCopy(renderer, texture, nullptr, nullptr);
      SDL_RenderFlush(renderer);
    }
    //apply_hlsl_vertex_shader(renderer, previous_vertex_shader);
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
