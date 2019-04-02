#ifndef GAME_H
#define GAME_H

#include "core.h"

#include "geom.h"
#include "assets.h"
#include "render2d.h"

bool init_game();
void free_game();

void update_and_draw_game(i32 width, i32 height, f64 delta);

#endif