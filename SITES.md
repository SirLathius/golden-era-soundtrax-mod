# Everything the mod touches in UG2

This list covers UG2Music.asi as built from this source, against the retail v1.2 `speed2.exe` (4,800,512 B,
SHA-256 `f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7`). All addresses are retail
addresses. Byte patterns are written in lowercase hex.

How the mod writes:

- **Every site is byte-checked before use.** If a site does not match, the mod skips the part that needs it
  and logs why: the address, the bytes it found, and what stays stock. The mod does nothing to `speed2.exe`
  on disk. Every change is made in memory after the game has loaded.
- **The checks are all or nothing, part by part.** Each part below (picker, long-song fix, top bar, list,
  save sync, save prompt, Shuffle button, Default button) checks all of its sites before it writes any of
  them. The list and the Shuffle button only install if the top bar did. The Default button only installs if
  the Shuffle button did.
- **Code writes are read back.** Each one goes through VirtualProtect, a copy, the old protection put back
  and FlushInstructionCache, then a read-back compare. A failed write is logged as "FAILED" and its part as "NOT fully
  installed". Only the Default button undoes its first write: if its second write fails, it puts the handler slot back.
- **Data writes are guarded.** Writes to game data are compare-first: the mod writes only when the value it
  finds is UG2's own, or a value it wrote itself. Each one is read back. Every read of game memory first
  checks for a committed, readable page.

## HOOKS (head jumps with a trampoline)

| Address | Head bytes checked | Written | What runs there | Known callers |
|---|---|---|---|---|
| 0x46B6F0 EA Trax song picker | `83 ec 08 53 55 56` (6) | 5-byte jmp; trampoline = 5 bytes + jmp 0x46B6F5 | Returns the next song's slot 0..26, or -1 (no song allowed), or -2 (UG2's stock picker runs through the trampoline). Under Underground 2: always -2, with the two carrier slots given back first. Under the other categories: the mod picks from that category's set. An added song plays through carrier slot 25 or 26, never the one that played last. Riders first: at a front-end pick with [player+0x88] == 1 that changed since the mod's last menu pick, it returns -2. It falls back to -2 on a failed read, a closed front-end gate, no map found, or a failed carrier write. | 0x47B0D2 (front end), 0x47B1C9 (in game) |
| 0x727CD8 in the PT header parser 0x727A46 (the tag-0x85 sample-count store) | `8b 45 f0 89 07 e9 b7 00 00 00` (10) | 5-byte jmp over `mov eax,[ebp-0x10]; mov [edi],eax` | A stub with no calls and no I/O. When the count was 3 bytes and reads negative (UG2's reader 0x72A195 sign-extends counts above 0x7FFFFF), it masks the count to 24 bits. Then it does the store and returns to 0x727CDD (the original `jmp 0x727D99`). This lets the 9 MW songs over 233 s (waves 46 49 52 56 60 64 65 66 67) end and hand over. None of the 2,801 stock PT headers has such a count. 4-byte counts go UG2's own way. | Reached only by falling through from 0x727CD6. Callers of 0x727A46: 0x72250B (streams), 0x723B1F (in-memory sounds) |
| 0x5241E0 EA Trax top bar trigger handler | `8b 44 24 04 3d 13 ef 73 50` (9), plus its tail 0x524230 `8b 0d 84 b8 82 00 e8 e5 4f f5 ff 5e 5b c2 04 00` (16) | 5-byte jmp | Does what the head did (push ebx, push esi, mov esi,ecx), then steps the category: 0x5073EF13 (left trigger) = previous, 0xD9FEEC59 (right trigger) = next. Plays the trigger sound, sets the label and rebuilds the rows, then jumps to the handler's own tail 0x524230 (0x479220, ret 4). UG2's Random / Sequential flip in this handler no longer runs. | One caller: 0x56B324 (the jukebox handler 0x56B1C0) |
| 0x524120 top bar label routine | `0f b6 05 58 aa 83 00 83 e8 00 74 2c` (12) | 5-byte jmp | Re-reads the settings if the profile name changed, then sets the label to the category name. thiscall / ret kept. | 0x52422B (inside the trigger handler, no longer reached), 0x565E1D (after a mouse step) |
| 0x53DEB0 EA Trax row mode cycle | `83 ec 1c 55 56` (5) | 5-byte jmp; trampoline = 5 bytes + jmp 0x53DEB5 | Before UG2's body: copies the step into the set the rows show (see WRITES). Then UG2's own body steps the view entry and writes the row's +0x1C, label, sound, arrow blink and 0x479220. | The jukebox handler at 0x56B2C3 on 0x9120409E (left) / 0xB5971BF1 (right) |
| 0x480C10 EA Trax preview of the highlighted row | `8b 89 a0 00 00 00` (6) | 5-byte jmp; trampoline = the 6 bytes + jmp 0x480C16 | A UG2 row (id 0..26) takes UG2's own path; a carrier row 25 / 26 first gets its own song back. An added row: a carrier gets the song's wave and strings, then the mod calls 0x47AF90(player, carrier) itself and returns (ret 4). | The jukebox handler (0x56B201 passes the row's id [row+0x18]) on accept 0x0C407210 / 0x406415E3 |
| 0x52A460 profile "keep a copy" (the profile object 0x83A9D0 keeps a serialized copy at [+0x16778]) | `55 8b ec 56 8b f1 8b 86 78 67 01 00` (12) | 6 bytes: jmp + nop; trampoline = 6 bytes + jmp 0x52A466 | Before: notes whether the game already holds a copy. After UG2's body: if the game made a copy, snapshots the mod's own state (category + the MW, UG1, Carbon and All Trax modes, 195 B). | 10 callers (front-end screen entries, memory card flows) |
| 0x52A4C0 profile "changed?" | `83 ec 0c 56 8b f1 8b 86 78 67 01 00` (12) | 6 bytes: jmp + nop; trampoline = 6 bytes + jmp 0x52A4C6 | After UG2's body: al = UG2's answer OR "the mod's state differs from its snapshot" (only when the game held a copy). So a change to the added songs raises UG2's own save dialog. The snapshot is then dropped, as the game drops its copy. | 7 callers, each `test al,al`: 0x4AF368, 0x4B6020, 0x4ED94A, 0x4EDD7C, 0x552C88, 0x55BEFB, 0x55E5A5 |

### Re-pointed calls and the handler slot

| Address | Bytes checked | Written | What runs there | Known callers |
|---|---|---|---|---|
| 0x4D9CB2 `call 0x43E030` inside 0x4D9C60 (save current profile) | `e8 79 43 f6 ff` | rel32 at 0x4D9CB3 to a stub | Writes the mod's settings file for the profile at 0x83A9E0, with every register and the flags kept, then jumps to 0x43E030 (UG2's own args and return address unchanged). | 0x4D9C60's caller 0x4F3652 |
| 0x4D9BEA `call 0x43E030` inside 0x4D9B80 (save as name) | `e8 41 44 f6 ff` | rel32 at 0x4D9BEB | The same as 0x4D9CB2. At this point 0x83A9E0 already holds the new name (assigned at 0x4E8F41). | 0x4D9B80's caller 0x4E8F52 |
| 0x5347AB `call 0x5344D0` inside 0x534770 (load from a read buffer) | `e8 20 fd ff ff` | rel32 at 0x5347AC | Calls 0x5344D0 with the same three args (its `ret 0xC` honoured), then re-reads the settings file for the name now at 0x83A9E0 and drops unsaved steps. | 0x534770's caller 0x4A9931 |
| 0x534812 `call 0x5344D0` inside 0x5347C0 (load from the game's kept copy) | `e8 b9 fc ff ff` | rel32 at 0x534813 | The same as 0x5347AB. | 0x4FBA7C, 0x4FD628 |
| 0x54E8D7 `call 0x50C900` in the PC button bar setup 0x54E6E0 | `e8 24 e0 fb ff` | rel32 to the mod's label routine | Calls UG2's 0x50C900(obj, hash) first. Then, on the jukebox only (bar package = `EA_Trax_Jukebox.fng`, the object a text object): hash 0xBA0E1020 (slot bit 0x80000) prints "Shuffle On" / "Shuffle Off" by [0x83AA58]; hash 0xEAA1D5DF (slot bit 0x100000) prints "Default". Both print through 0x537BE0. On any other screen nothing changes. | The setup runs for every screen with a button bar (80 callers) |
| 0x55DF22 `call 0x52D170` in the bar click 0x55DE00 | `e8 49 f2 fc ff` | rel32 to the mod's click routine | Calls UG2's 0x52D170(bar, bit) first and returns its message for every bit. On the jukebox: bit 0x80000 flips Shuffle, bit 0x100000 raises the Default dialog. | The only call of 0x52D170; reached from the bar handler 0x55DFA0 |
| 0x79CB7C the jukebox vtable's handler slot (.rdata) | The vtable 0x79CB74, 16 bytes: `50 41 55 00 80 d0 40 00 c0 b1 56 00 70 0a 57 00` (slot = 0x0056B1C0) | 4 bytes: the mod's handler (thiscall, ret 0x10) | Takes the answer of its own dialog. While that dialog is up, drops the jukebox's 11 input messages (see CALLS). Every other message goes on to 0x56B1C0 with the same ecx and stack, and its return value is returned. | UG2's base handler 0x55B5C0 (0x55BC75 `call [eax+8]`), reached from the package broadcast 0x55DC50 and the send-by-name 0x55DC20 |

## PATCHES (code and data in the exe image)

- **0x565DB1, 45 bytes (to 0x565DDD): the left-arrow flip in the mouse handler 0x565D90.**
  - Checked whole (`0f be 05 ab 81 83 00 83 f8 ff 8a 1d 58 aa 83 00 74 0c ... 89 0d 58 aa 83 00 eb 3d`).
  - Replaced by `push -1; push edi; call <mod step>; add esp,8; jmp short 0x565E1B`, then nop fill.
- **0x565DF1, 42 bytes (to 0x565E1A): the right-arrow flip.**
  - Checked whole.
  - Replaced by `push 1; push edi; call <mod step>; add esp,8; jmp short 0x565E1B`, then nop fill.
  - The join both arrows jump to is checked (`8b cf e8 fe e2 fb ff 8b 0d 84 b8`) and left unchanged: `mov ecx,edi; call 0x524120; mov ecx,[0x82B884]; call 0x479220`.
  - Caller of 0x565D90: 0x56B2D1 (the mouse click message 0x9803F6E2).
  - eax / ecx / edx / bl were only temporaries in the replaced blocks.
- **0x5540BF, 30 bytes (to 0x5540DC): the EA Trax screen constructor's inline first label ([0x83AA58] -> the Random / Sequential hash).**
  - Checked whole.
  - Replaced by `mov edx,[esp+0x10]; push edx; call <mod screen open>; add esp,4; jmp short 0x55412C`, then nops.
  - The jump skips UG2's inline label copy 0x5540DD..0x55412B. Those bytes stay as they are but are no longer reached; its other entries 0x5540E9 / 0x554122 lie inside it.
  - The continuation 0x55412C is checked (`8b 74 24 10 8b ce e8 d9 71 ff ff` = `mov esi,[esp+0x10]; mov ecx,esi; call 0x54B310`).
  - At every screen open the mod: re-reads the settings if the profile changed, sets the bar label, writes rows 0 / 1's albums, builds the view and the row count, and moves the row words.
- **The list fill 0x54B310's 8 operands (4 bytes each, written once at load).** Each whole instruction is checked first.
  - 0x54BA69 / 0x54BA85 / 0x54BAA1 / 0x54BABD: the disp32 of `[edi*8+0x83ACB8]` at 0x54BA66 / 0x54BA82 / 0x54BA9E / 0x54BABA (e.g. `8b 14 fd b8 ac 83 00`). They now point at the mod's view, which uses the same 8-byte entry layout as UG2's list (int id, byte mode).
  - 0x54BAC7: `[edi*8+0x83ACBC]` at 0x54BAC4. Now points at view + 4.
  - 0x54BA90 / 0x54BA74 / 0x54BAAC: `[reg+0x7ECD80 / 84 / 88]` at 0x54BA8E / 0x54BA72 / 0x54BAAA. Now point at the mod's song table (same 16-byte row layout) + 0 / 4 / 8.
  - Row ids 0..26 stay UG2's song indices. 27..52 = MW, 53..78 = UG1, 79..108 = Carbon.
- **0x54BB38, the row count: the imm8 of `cmp edi,0x1b` at 0x54BB36 (`83 ff 1b`).**
  - Set to N rows: 27 under Underground 2, 26 under Underground, 24 under Most Wanted, 30 under Carbon, 107 under All Trax. It must stay at or below 127 (a signed imm8).
  - `83 ff` is re-checked before every write, and the byte is written only when it differs.
  - Written at load, at every screen open, every category step and every Default reset.
- **0x53DEC9, the row cycle's search base: the imm32 of `mov eax,0x83acb8` at 0x53DEC8 (`b8 b8 ac 83 00`).** Now points at the mod's view.
- **0x53DEE2, the search bound: the imm32 of `cmp ecx,0xd8` at 0x53DEE0 (`81 f9 d8 00 00 00`).** Set to 0x368 (109 x 8).
  - So UG2's own row-cycle body finds and steps the view entry, and its one list write 0x53DFE5 `mov [eax+4],bl` lands in the view. It no longer writes 0x83ACB8.
- **Checked only (the list depends on them; never written):**
  - 0x54BB39 (the fill loop's back edge)
  - 0x53DEB5 and 0x480C16 (the trampolines' returns)
  - 0x53DFE5 (the row cycle's list write)
  - 0x47AF90 (`55 8b 6c 24 08 83 fd 1b`, the slot player)
  - 0x56B201 (the preview's caller passes the row id)
  - 0x4358E0 (the list-destroy helper)
  - 0x54B32B / 0x54B341 (the fill's header gate)
  - 0x553CD8 (the constructor's 4 header items)
  - 0x542CC2 (the list control's vtable 0x795CB4)
  - 0x4CBBC3 (its destructor)
  - 0x54BA01 (the fill sets the first visible row)
- **0x7F80C8 (.data): the jukebox row's button-bar flag word, written at load.**
  - 0x00000401 (`01 04 00 00`: Quit + Back) -> 0x00080401 (+ Shuffle = slot 3, upper middle, left of Back) -> 0x00180401 (+ Default = slot 4, upper left).
  - The second write is compare-first on 0x00080401, and the handler slot 0x79CB7C is written before it.
  - Its only reader is the screen create 0x50B7FC. UG2 has no writer.
  - The slots' stock labels are "Test N2O Purge" / "Open/Close Hood". Their stock message 0xC539A4B0 is one the jukebox handler ignores.

## WRITES TO GAME MEMORY (run time)

- **The carrier audio-node words in the loaded music map image.**
  - Where: image+0x700 (node 77 = song 25 "Skeptic") and image+0x738 (node 80 = song 26 "The Celebration Song"). The image is [0x8B5710 + n x 0x928 + 0x34] for the map slot n (0..3) that matched.
  - What: the low 16 bits are set to the wave (the slot's own 21 / 32, or an added song's 44..126); the high 16 bits are kept.
  - When: at a pick or preview of an added song. They are given back under Underground 2, and before UG2's own preview of row 25 / 26.
  - Guard: compare-first, writable, read back. Put back if the row write that goes with it fails.
- **Song table rows 25 / 26 (0x7ECF10 / 0x7ECF20: +0 title, +4 artist, +8 album pointers).**
  - What: the added song's strings (static in the asi), or UG2's own pointers as saved at load.
  - When: with the carrier word above.
  - Guard: only if that row's title read UG2's stock text at load. Each pointer is compare-first and read back. +0xC is never written.
- **Row 19 "Determined" album, 0x7ECEB8.**
  - What: "Lost and Found".
  - When: once at load.
  - Guard: only over the stock pointer 0x00783FE5 (an empty string). Compare-first, read back.
- **Row 0 "Riders on the Storm" artist, 0x7ECD84.**
  - What: "Snoop Dogg/The Doors".
  - When: at load and compare-first at every pick. UG2's language fix-up 0x459D90 (0x459D9C / 0x459DB3) puts its own pointer back once after load.
  - Guard: only over UG2's 0x00789C04 ("feat.") / 0x007896EC ("avec"). Any other pointer is left alone and logged once.
- **Rows 0..26 title pointers, 0x7ECD80 + 16 x row.**
  - What: the titles without their quotes; rows 0 / 8 / 10 / 11 end in "(Remix)".
  - When: once at load.
  - Guard: only where +0 still holds the pointer checked at load. Read back. The saved copy of each row takes the new pointer, so a carrier given back shows the new title. No store to +0 was found in UG2's .text.
- **Rows 0 / 1 album pointers, 0x7ECD88 / 0x7ECD98.**
  - What: "NFS Underground 2 Exclusive".
  - When: at load, at every pick and at every EA Trax screen open. 0x459D90 (0x459DB9 / 0x459DBE) re-writes them once after load.
  - Guard: only over UG2's own three pointers: 0x00789BDC (.data), 0x00789694 (English, from [0x7ECF3C]) and 0x007896C0 (French, from [0x7ECF34]). Anything else gets no write and is logged once.
- **The live EA Trax list 0x83ACB8 (27 x {int song, byte mode}): the mode byte at +4.**
  - When, on a row step of a UG2 song while the bar shows Underground 2: the same value UG2's body gives the view entry, written before UG2's 0x479220 rebuilds the masks.
  - When, on Default while the bar shows Underground 2: every byte gets UG2's default (songs 0..5 Menu, 6..26 Racing), then 0x479220 runs.
  - Guard: writable, read back.
  - Never written for an added song, and never under any other category.
- **[0x83AA58], UG2's Random / Sequential dword.**
  - What: flipped 1 <-> 0 exactly as UG2's old bar handler did (0x52421D-0x524226), read back, then 0x479220 -> [player+0x9C].
  - When: only on a Shuffle click.
  - Note: the value is part of UG2's profile save, so UG2 keeps it and raises its own save prompt. UG2 starts on Random ("Shuffle On") with no write by the mod.
- **The EA Trax list control at screen+0x4C: rows rebuilt at a category step or a Default reset.**
  - What: UG2's 0x4358E0 destroys the row list +0x10. +0x1C / +0x20 / +0x24 / +0x28 are set to 0, as the control's constructor does. [screen+0x124] goes 4 -> 0 around a call of UG2's fill 0x54B310, then back to 4.
  - Guard: the control's vtable must read 0x795CB4, [screen+0x124] must read 4, and both regions must be writable. After a Default reset, the rows are rebuilt only when [screen+0] = 0x79CB74.
- **The row words' x positions.**
  - Where: the artist / title / album text objects of the 4 visible slots (hashes 0x66646FC4 / 0x2890C8AF / 0x77BD189E + slot 0..3). They are looked up as the screen constructor does: 0x52CEF0([screen+4]), then 0x517F80(package, hash). The x is the float at [obj+0x2C]+0x1C.
  - What: -246.2 (the FNG's -253.0 + 6.8).
  - When: at every EA Trax open. At a rebuild the 12 values are only read back.
  - Guard: only when x reads the FNG's -253.0. -246.2 already there is counted; any other value is left alone and logged. The object must be type 2 (+0x18 == 2) and its 0x44-byte data block writable. Read back.

## READS

- [0x82B884], the sound manager: +0x90 (1 = front end), +0xA0 (the player).
- [0x89E7B0], the game state (8 = front end).
- The player that UG2 passes to the picker (ecx): +0x14 (the current slot), +0xA5 (the front-end gate byte), +0x9C (random / sequential), +0x88 (the stock picker's front-end remaining set), +0xA8 (its position).
- 0x83ACB8, 27 x 8 (the live list):
  - At every pick, view build and Default.
  - When an older settings file is loaded (header 1 or no header), UG2's 27 modes are copied from it into All Trax's lines.
- 0x7ECD80, 27 x 16 (the song table): at load and at each view build. At load, each row's title must read UG2's stock text; a row that does not is never written.
- The map slots 0x8B5710 + n x 0x928 (n 0..3), +0x30..+0x54:
  - The image at [+0x34] is compared with the asi's built-in copy of the expected MusicSFx.mpf (7,504 B, 126 sample entries) in five regions: header, node table, nodes, events, samples.
  - The mod finds the map again if [+0x34] changes.
- [0x82BA08] (the music stream buffer size) and [0x82B984] (its handle): read once, at the first map find, for the log.
- 0x83A9E0 char[32] (the profile name): at every pick, screen open, category step, save and load.
- [screen+0x104] (the top bar text object): +0x18 / +0x60 / +0x1C.
- 0x8381AB / 0x8381C6 (the trigger sound ids; signed byte, -1 = none).
- [0x8384D8] (the PC button bar):
  - [bar+0x58] (its package name, compared with `EA_Trax_Jukebox.fng`).
  - [bar+0x4C] (the slot node list, at most 8 links): node +0x1C (bit), +0x18 (shown), +0x10 (text object).
- [0x83AA58], for the Shuffle text.
- [0x83A9D0+0x16778] (the game's kept profile copy; 0 = none).
- [screen+0] (vtable), [screen+4] (package name), and list control +0x18 / +0x1C / +0x24 / +0x28 (the row rebuild's checks).
- [mgr+0xA0] at a preview (the player).
- 0x78FA24 `GenericDialog_SMALL.fng`: re-checked before each dialog.

## CALLS INTO UG2

Each function's head is byte-checked at load by the part that calls it.

- **0x43BDF0() then 0x43C1C0(count): the stock random draw.**
  - Called once per random pick of the mod, so the shared state [0x7EA624] steps as it does for UG2's picker. Never called with 0.
  - Heads `83 ec 08 8d 44 24 00` / `8b 0d 24 a6 7e 00`. A mismatch sends every pick to the stock picker.
- **0x47AF90(player, slot), thiscall:** plays a carrier slot for the preview of an added row. It stops the old song, and its pop-up shows the carrier's row.
- **0x537BE0(obj, "%s", text), FEPrintf on an object:** prints the bar's category name ("Underground 2", "Underground", "Most Wanted", "Carbon", "All Trax"; at most 13 characters, the bar's text object takes 14), plus "Shuffle On" / "Shuffle Off" and "Default". Head `8b 44 24 04 85 c0 74 14 8b 54 24 08`.
- **0x50C900(obj, hash), the label by hash:** called from the re-pointed 0x54E8D7, always first. Head `56 8b 74 24 08 85 f6 74 47 8b 46 1c`.
- **0x52D170(bar, bit), the bit-to-message map:** called from the re-pointed 0x55DF22, first, for every bit. Head `8b 44 24 04 3d 00 00 04 00`.
- **0x458520 thiscall([0x82B884], id), the trigger sound:** at a category step and a Shuffle click. Head `a1 a8 aa 7f 00 85 c0 74 31`.
- **0x479220 thiscall([0x82B884]), the list set-up (masks + [player+0x9C]):**
  - Still reached through the trigger handler's tail and the mouse join.
  - Called by the mod after a Shuffle click and after a Default reset under Underground 2.
  - Head `83 ec 08 a1 58 aa 83 00`.
- **0x4358E0 thiscall(list control + 0x10) and 0x54B310 thiscall(screen):** the row rebuild.
- **0x52CEF0(name), cdecl, and 0x517F80(package, hash), thiscall:** the row word lookups.
  - Heads `a1 d0 84 83 00 85 c0 56 57 75 40` (11) / `6a ff 68 88 6f 77 00 64 a1 00 00 00 00` (13).
  - These are not part of the top bar's all or nothing: a mismatch leaves the words where UG2 puts them.
- **0x50D150(package, 1, 0):** UG2's UI sound at a Default click. Head `8b 44 24 08 8b 4c 24 0c 6b c0 1b`.
- **0x558750, UG2's yes / no dialog.**
  - Arguments: package, 0x78FA24 `GenericDialog_SMALL.fng`, label Yes 0xF770, label No 0x061C, message 0x7E16DEF1 (yes), 0x7E16DEF0 (no), 0x7E16DEF0 (Esc), 0, "%s", "Reset every song of this soundtrack^to its default setting?".
  - This is the same call UG2's own Default Settings item makes. The two answer messages are the mod's own values (no dword hits in speed2.exe).
  - Head `8b 4c 24 24 8b 54 24 20 8d 44 24 28`.
  - Yes resets only the category the bar shows. No changes nothing.
- **0x56B1C0(screen, msg, a, b, c), the jukebox handler:** gets every message the mod does not take. Head `8b 44 24 04 3d 4b 0a 1c 91 56 8b f1`.
  - While the Default dialog is up, these 11 input messages are dropped:
    - 0x911C0A4B / 0x72619778 (list steps)
    - 0x5073EF13 / 0xD9FEEC59 (triggers)
    - 0x0C407210 / 0x406415E3 (accept -> preview)
    - 0x911AB364 (back)
    - 0xC519BFC0 (bar button)
    - 0x9120409E / 0xB5971BF1 (row left / right)
    - 0x9803F6E2 (mouse click)
  - A dropped message returns itself, which is what 0x56B1C0 leaves in eax for a message it ignores (0x56B349).
  - The text refreshes 0x35F8620B / 0xC98356BA and every other message still pass.
  - A dialog that never got an answer is cleared at the next Default click or the next EA Trax open.
- **0x43E030 (jumped to) and 0x5344D0 (called):** the original targets of the four re-pointed save / load calls, run with UG2's own arguments.
- **The trampolines:** they run UG2's own head bytes and continue at 0x46B6F5, 0x53DEB5, 0x480C16, 0x52A466 and 0x52A4C6.

## FILES

**Game file changed: `SDATA\sdat.viv`.** Keep the original.

- The rebuilt archive is 893,398,656 B (SHA-256 `91b9fd48..`). It holds UG2's original contents, with MusicSFx.mus grown by:
  - Most Wanted's 26 songs as waves 44..69 (MW 25 / 26, waves 68 / 69, are never listed or picked).
  - Underground's 27 streams re-headered at 22,050 Hz as waves 70..96 (the 26 listed songs use 71..96).
  - Carbon's 30 songs at their own 36,000 Hz as waves 97..126.
- MusicSFx.mpf grows from 7,048 to 7,504 B: 126 sample entries (UG2 43 + MW 26 + UG1 27 + Carbon 30), no node changed.
- The 4 nisaudio entries moved. UG2 reads them by name through the BIG4 directory.
- UG2 sizes its music stream buffer from the table's entry n-2, which is now 190,047 B. The log names it at the first map find.
- With the original archive in place, the map check fails ("The music map was NOT found" in the log) and every pick goes to UG2's stock picker.

**The mod's own files.** They sit next to the asi, normally in `scripts\`.

- `scripts\UG2Music\` is created at load.
- `scripts\UG2Music\<profile>.txt` is one file per UG2 profile.
  - The name is the profile name at 0x83A9E0. Any character other than A-Z, a-z, 0-9, space, `-` and `_` becomes `_`.
  - It is written only when UG2 saves the profile (either save path), as `<profile>.txt.tmp`, then `MoveFileExA(REPLACE_EXISTING | WRITE_THROUGH)`.
  - It is read when a profile loads, and whenever the name at 0x83A9E0 changes (checked at load, at each pick, screen open, category step and save).
  - A step that is never saved is lost, as with UG2's own song modes. There is no ini.
  - Format (CRLF; keys and words are not case-sensitive; a missing line means the default):

    ```
    UG2Music settings 2
    soundtrack = UG2 | UG1 | MW | CARBON | ALL
    MW nn = OFF | FE | IG | AL          (nn 01..26; the Most Wanted category)
    UG1 nn = OFF | FE | IG | AL         (nn 01..26; the Underground category)
    CB nn = OFF | FE | IG | AL          (nn 01..30; the Carbon category)
    ALL UG2 | MW | UG1 | CB nn = ...    (All Trax's own set; UG2 nn 01..27)
    <any other line>                    (kept as it is)
    ```

  - The soundtrack value:
    - The default is UG2 (Underground 2).
    - An unknown value, including `OFF`, means the default.
    - Underground 2's own 27 modes are not in the file. They are UG2's own (0x83ACB8, in UG2's profile save).
  - The mode words: OFF = Off, FE = Menu, IG = Racing, AL = All.
  - Lines are written only for songs whose mode is set; Default removes them.
  - A file with header 1 or no header (the older shared settings) is upgraded once on read: each game line's value is copied into its All Trax line, and the next save writes header 2.
- `scripts\UG2Music.log` is this game start's log: one plain sentence per line after the time of day (the version, the game and archive sizes, each part installed or not and why, the profile's settings file, each song that starts, each change on the EA Trax screen, every fault). A problem line carries one word in capitals, NOT or FAILED. At each game start the previous log is moved to `scripts\UG2Music.previous.log`.

## NEVER TOUCHED

- UG2's save files and save folder: the mod never reads or writes them. Its settings file lives next to the asi.
- UG2's save-function heads 0x4D9C60 / 0x4D9B80: left untouched. The mod's save sites are the call operands inside them, at 0x4D9CB2 / 0x4D9BEA.
- The profile load 0x5344D0 (head `55 8b ec 83 ec 24`) and the helper 0x43E030: not written. Only the four calls to them are re-pointed.
- `speed2.exe` on disk: every change is made in memory after load.
- The song table's +0xC (pool) in every row. Artist pointers other than row 0 and the carriers. Album pointers other than rows 0, 1, 19 and the carriers.
- 0x83ACB8 for any added song, and under any category but Underground 2.
- The code of 0x56B1C0, 0x50C900 and 0x52D170: only one vtable slot and two call sites change, and their other callers are untouched.
- Other screens' button bars: the flag word is the jukebox row's own, and the label and click routines act only on `EA_Trax_Jukebox.fng`.
- The game's kept profile copy [0x83A9D0+0x16778]: read only.
- The dialog core's own mark 0x838974: neither read nor written.
- The FNG files / FrontB.lzc: the row words move in memory at each screen open. The row numbers, mode words, arrows, highlight and scroll bar are not written.
- The archive's MusicSFx.mpf nodes and UG2's own 43 sample entries: unchanged.
