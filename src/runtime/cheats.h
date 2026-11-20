/* Cheat hotkeys (docs/cheats.md). */
#pragma once
#include <stdint.h>

/* cheats.c, every host: queue a cheat event (game thread, machine lock held) */
int ra2_cheat_enqueue(int action, int in_battle, uint32_t tick);
/* cheats.c: an event the game executes; run_lift.py cheat_patches calls it */
int ra2_cheat_event(uint32_t e, uint32_t *winner);
/* host.c, Windows: from the presenter, action 1 money, 2 campaign victory */
int host_cheat_post(int action);
