/* Cheat hotkeys (docs/cheats.md), from the EnhancedYR package's cheat hotkeys v2.
 *
 * Ctrl+Shift+M: +30,000 credits. In a LAN game for every human still playing,
 * in a skirmish or a mission for the local player. Ctrl+Shift+W: win the
 * current campaign mission (campaigns only).
 *
 * A cheat is an event in the game's own outgoing event ring, so in a LAN game
 * every machine carries it out on the same frame, as it does the player's own
 * orders: ra2_cheat_enqueue writes it (Windows presenter, on the game thread),
 * and ra2_cheat_event answers it where the game executes events
 * (run_lift.py cheat_patches). This file is built by every host, so every
 * build understands a cheat event, whether or not its own hotkeys are on.
 * Builds from before this change do not: all machines in a LAN game need it. */
#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "recomp_target.h"

/* Guest addresses as host pointers: the same on Windows; through win32hle
 * off it, where on a 64-bit host (macOS) the guest's memory is one block. */
#ifdef _WIN32
#define GUEST_PTR(va) ((void *)(uintptr_t)(uint32_t)(va))
#else
#include "win32hle.h"
#define GUEST_PTR(va) hle_ptr((uint32_t)(va))
#endif

#ifdef RA2_TARGET_GAME
#define CH_SESSION  0x00A3D298u   /* the session type: 0 campaign, 3 LAN, 5 skirmish */
#define CH_HOUSES   0x00A32298u   /* HouseClass::Array (DynamicVectorClass) */
#define CH_PLAYER   0x00A35DB4u   /* the local player's house */
#define CH_HUMAN    0x134u        /* HouseClass: a human player */
#define CH_DEFEATED 0x13Du
#define CH_OVER     0x13Eu
#define CH_CASH     0x24Cu        /* HouseClass: credits, as GiveMoney adds them */
#else
#define CH_SESSION  0x00A8B238u
#define CH_HOUSES   0x00A80228u
#define CH_PLAYER   0x00A83D4Cu
#define CH_HUMAN    0x1ECu
#define CH_DEFEATED 0x1F5u
#define CH_OVER     0x1F6u
#define CH_CASH     0x30Cu
#endif
/* The outgoing event ring Queue Five also writes (run_lift.py QUEUE_FIVE_TARGETS):
 * the count, the head, the tail, 128 events of 111 bytes and a time stamp each. */
#define CH_QUEUE      RA2_QUEUE_FIVE_COUNT_VA
#define CH_EVENTS     128u
#define CH_EVENT_SIZE 111u
#define CH_FRAME      RA2_GAME_FRAME_VA

/* The event: type 3 with one of these tags where its data starts. */
#define CH_SELF    0x4D433201u    /* the sender's money */
#define CH_ALL     0x4D433202u    /* every active human's money (LAN) */
#define CH_VICTORY 0x4D433203u    /* win the campaign mission */
#define CH_AMOUNT  30000

static uint32_t rd32(uint32_t a) { uint32_t v; memcpy(&v, GUEST_PTR(a), 4); return v; }
static void wr32(uint32_t a, uint32_t v) { memcpy(GUEST_PTR(a), &v, 4); }
static unsigned rd8(uint32_t a) { return *(volatile uint8_t *)GUEST_PTR(a); }

static uint32_t house(int i) {
    int n = (int32_t)rd32(CH_HOUSES + 16);
    uint32_t items = rd32(CH_HOUSES + 4);
    return items && n > 0 && n <= 32 && i >= 0 && i < n ? rd32(items + 4u * i) : 0;
}

static int active(uint32_t h) { return h && rd8(h + CH_HUMAN) && !rd8(h + CH_DEFEATED) && !rd8(h + CH_OVER); }

static int session_ok(uint32_t mode) { return mode == 0 || mode == 3 || mode == 5; }

/* Queue a cheat: action 1 money, 2 campaign victory. On the game thread, with
 * the machine lock held; 0 when it does not apply (menus, a mode without
 * cheats, no local player, a full ring). */
int ra2_cheat_enqueue(int action, int in_battle, uint32_t tick) {
    uint32_t mode = rd32(CH_SESSION), player = rd32(CH_PLAYER), tag;
    int index = -1;
    if (!in_battle || !rd32(CH_FRAME) || !active(player) || !session_ok(mode)) return 0;
    if (action == 2 && mode != 0) return 0;
    if (action != 1 && action != 2) return 0;
    for (int i = 0; i < 32; i++)
        if (house(i) == player) { index = i; break; }
    if (index < 0) return 0;
    uint32_t count = rd32(CH_QUEUE), head = rd32(CH_QUEUE + 4), tail = rd32(CH_QUEUE + 8);
    if (count >= CH_EVENTS || head >= CH_EVENTS || tail >= CH_EVENTS) return 0;
    tag = action == 2 ? CH_VICTORY : mode == 3 ? CH_ALL : CH_SELF;
    uint8_t e[CH_EVENT_SIZE] = { 0 };
    uint32_t frame = rd32(CH_FRAME);
    e[0] = 3;
    e[2] = (uint8_t)index;
    memcpy(e + 3, &frame, 4);
    memcpy(e + 7, &tag, 4);
    memcpy(GUEST_PTR(CH_QUEUE + 12 + tail * CH_EVENT_SIZE), e, sizeof e);
    wr32(CH_QUEUE + 12 + CH_EVENTS * CH_EVENT_SIZE + tail * 4, tick);
    wr32(CH_QUEUE + 8, (tail + 1) & (CH_EVENTS - 1));
    wr32(CH_QUEUE, count + 1);
    return 1;
}

/* An event the game is about to execute: 0 not a cheat (the game's own code
 * runs), 1 done (or refused), 2 win the mission for *winner. */
int ra2_cheat_event(uint32_t e, uint32_t *winner) {
    if (rd8(e) != 3) return 0;
    uint32_t tag = rd32(e + 7);
    if (tag != CH_SELF && tag != CH_ALL && tag != CH_VICTORY) return 0;
    uint32_t mode = rd32(CH_SESSION), sender = house((int8_t)rd8(e + 2));
    if (!session_ok(mode) || !active(sender)) return 1;
    if (tag == CH_VICTORY) {
        if (mode != 0) return 1;
        *winner = sender;
        return 2;
    }
    if ((mode == 3 && tag != CH_ALL) || (mode != 3 && tag != CH_SELF)) return 1;
    for (int i = 0; i < 32; i++) {
        uint32_t h = house(i);
        if (!active(h) || (tag == CH_SELF && h != sender)) continue;
        int32_t cash = (int32_t)rd32(h + CH_CASH);
        wr32(h + CH_CASH, (uint32_t)(cash > INT_MAX - CH_AMOUNT ? INT_MAX : cash + CH_AMOUNT));
    }
    return 1;
}
