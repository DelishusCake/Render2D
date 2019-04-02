# Render2D

A small C library for efficient 2D rendering with OpenGL (and maybe Vulkan in the future).

# Features

 * Resolution independent rendering
   * Your game always renders at the correct size/aspect ratio, even if the user resizes the window!
 * Fast 2D rendering
   * Blast sprites to the screen as fast as the GPU can!
 * Thread safe texture creation
   * Implement background texture loading without fear!
   * See assets.h/.c for an example!
 * Easy to use
   * Simple interface to let you focus on the game!
   * One header and one implementation file to include, no complicated build system

# How to Use

The entire library is contained within the render2.h/.c files.

You will need the gl3w OpenGL library to compile by default, but you can substitute your own OpenGL loader pretty easily.

# Examples

See game.h/.c for a quick example game (work in progress).
