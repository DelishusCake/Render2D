#ifndef GAME_H
#define GAME_H

#include "core.h"

#include "geom.h"
#include "draw.h"
#include "assets.h"

void game_init(assets_t *assets);
void game_update_and_draw(f64 delta, assets_t *assets, draw_list_t *draw_list);

#endif