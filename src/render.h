#ifndef RENDER_H
#define RENDER_H

#include <GL\gl3w.h>

#include "core.h"
#include "geom.h"

#include "assets.h"

bool render_init();
void render_free();

void render(u32 width, u32 height);

#endif