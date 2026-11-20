# Cheat hotkeys

Off by default. F10, **Cheats**, turns them on (`cheats=1` under `[present]`
in `ra2.ini`). Windows only for the keys. Every host carries the cheats out
(see LAN below).

| Keys | What |
|---|---|
| Ctrl+Shift+M | +30,000 credits. Skirmish and campaign: the local player. LAN: every human still playing. |
| Ctrl+Shift+W | Win the current campaign mission. Nothing in a skirmish or LAN game. |

They work in a battle only, not in the menus. Not with Alt held, and holding the
keys does not repeat. The computer players get nothing. The log says
`[cheats] +30000 queued` or `[cheats] campaign victory queued`, or `ignored`
with the reason.

## How it works

A cheat is an event in the game's own outgoing event ring (the one Queue Five
also writes, `run_lift.py` `QUEUE_FIVE_TARGETS`): type 3, with a tag of its
own where the event's data starts. So in a LAN game it goes to every machine
and is carried out on the same frame, as the players' orders are.

- **Queueing** (`src/runtime/host.c`, Windows): the presenter sees the key and
  posts a message to the game thread; a `WH_GETMESSAGE` hook there takes it out
  of the game's message loop and, under the machine lock, writes the event
  (`ra2_cheat_enqueue` in `src/runtime/cheats.c`).
- **Carrying out** (`run_lift.py` `CHEAT_TARGETS`, every host): the function
  that executes an event (gamemd `0x004C6CB0`, game `0x004B65C0`) asks
  `ra2_cheat_event` first. A cheat event is done there and the function
  returns as its own `ret` would; a campaign victory calls `HouseClass::Win`
  (gamemd `0x004FC9E0`, game `0x004E7EE0`) for the sender, with `push 0` as the
  game's own code does. Any other event runs the game's code unchanged.

Credits go to `HouseClass` +0x30C (Red Alert 2 +0x24C), the field GiveMoney
adds to, capped at `INT_MAX`.

## LAN

Every machine in a LAN game needs a build with this change (whether or not its
own hotkeys are on). A build without it runs the cheat event as an ordinary
type-3 event, and the games go out of sync. There is no version handshake.

## Origin

From the EnhancedYR package's cheat hotkeys v2, which patched them in with an
installer and a CMake rename of the generated event function. Here the event
is a `run_lift.py` patch like the others, built into every host, and the keys
are a presenter setting. Not yet tested in a running game.
