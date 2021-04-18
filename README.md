# SDL_Renderer + HLSL

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

``` c++
int main(int arg_count, char** arg_vector) {
  return 0;
}
```

# Complete Source
