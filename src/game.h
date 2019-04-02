#ifndef GAME_H
#define GAME_H

#include "core.h"

#include "geom.h"
#include "draw.h"
#include "assets.h"

void init_game(assets_t *assets);
void update_and_draw_game(f64 delta, assets_t *assets, draw_list_t *draw_list);

#endif