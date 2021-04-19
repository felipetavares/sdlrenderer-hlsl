# SDL_Renderer + HLSL

![Image of a colored Mandelbrot fractal rendered with HLSL and SDL. Orange colors.](https://raw.githubusercontent.com/felipetavares/sdlrenderer-hlsl/master/screenshots/a.jpg "Mandelbrot Fractal")

Sometimes when working with SDL you may want to have custom effects that run
fullscreen for every pixel. In some cases you can just do that on the CPU side
but that quickly becomes a CPU bottleneck as the resolution increases.

One solution to avoid that is to use shaders. The most common way to add shaders
to SDL is to use OpenGL, which is multiplatform so you can write your shaders
once and then just run those same shaders in all the platforms that you are
compiling for. However, in a few cases, you may want to have a custom shaders
that works only for DirectX when your code is running on Windows. Some reasons
you may want that are:

- you are targetting old or weird platforms that have glitches in their OpenGL
  support;
- you prefer to write HLSL code and are only running your program on Windows;
- you want the best performance possible when running on Windows;
- you want features that are easier to implement on HLSL when compared to GLSL
  (which is used in OpenGL).
  
# Core Idea 

![Image of a colored Mandelbrot fractal rendered with HLSL and SDL. Blue colors.](https://raw.githubusercontent.com/felipetavares/sdlrenderer-hlsl/master/screenshots/b.jpg "Mandelbrot Fractal")

The main idea is to define the same D3D_RenderData that is used by SDL's
Direct3d9 driver. This way we can then access everything that's inside it. In
our case, the piece of information we want to acces is the D3D device.

``` c++
typedef struct
{
  // ...
} D3D_RenderData;

// ... very code, many line ...

IDirect3DDevice9* device = reinterpret_cast<D3D_RenderData*>(renderer->driverdata)->device;
```

Then we can simply call the normal D3D functions using our device to set the shader:

``` c++
assert(D3D_OK == IDirect3DDevice9_CreatePixelShader(device, g_ps21_main, &shader));


// ...

assert(D3D_OK == IDirect3DDevice9_SetPixelShader(device, shader));
```

# Compiling HLSL Shaders

To compile HLSL shaders we use `fxc.exe`, which is part of the DirectX SDK (it
can be downloaded from Microsoft).

For example, if we have a `fractal.hlsl` that we want to compile to a binary and
put that binary in a C array in a header file, we can do that by this command:

``` batchfile
fxc /O3 /T ps_2_a fractal.hlsl /Fh fractal.h
```

Notice we have to set the shader profile (with `/T`) to `ps_2_a` (it could also
be `ps_2_0` or `ps_2_b`, but this is the less limited of the three). This is
because SDL's driver for Direct3D9 does not setup a vertex shader, so we need to
either use the version 2 or go to 3 and also make a vertex shader, but then we
would need to worry about what exactly we are passing to that vertex shader and
in the case of passing textures we would probably need to drop SDL_Renderer
altogether and almosts write a new custom driver for SDL. At that point it is
probably better to drop SDL and just go with full Windows + DirectX.

# How can I compile this example project?

First, you are going to need the Microsoft C and C++ compiler installed. Also
install CMake, the option is available directly from the download tool from
microsoft or Visual Studio if you prefer to have Visual Studio installed.
Instructions for all that are available at [Microsoft's website][installing-cl].

[installing-cl]: https://docs.microsoft.com/en-us/cpp/build/walkthrough-compiling-a-native-cpp-program-on-the-command-line?view=msvc-160

Then you can open the "Developer Command Prompt for VS 2019", go to the
directory where you downloaded the source code and:

``` batchfile
mkdir build
cd build
cmake ..
msbuild ALL_BUILD.vcxproj
```

The executable file will be under `build\Debug\`.
