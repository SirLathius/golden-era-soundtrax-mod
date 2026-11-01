// savesync.h - keeps the mod's settings file in step with UG2's own profile saves and loads. The pure parts (no Windows call, no game memory), so the
// offline rig runs the same code natively.
// THE RULE: the soundtrack on the top bar and the added songs' modes stay IN MEMORY when stepped; the mod's file <scripts>\UG2Music\<profile>.txt is WRITTEN
// when the game saves the profile and READ when a profile is loaded; a quit without saving loses the steps - exactly what UG2 does with its own 27 modes (they
// live in the profile block the game writes at a save and reads at a load).
//  (1) THE SAVE: UG2's two save entry points (the memcard manager's 'save current profile' 0x4D9C60, caller 4f3652 the FE memcard screen, and 'save as name'
//      0x4D9B80, caller 4e8f52 the memcard flow - the only callers of the write op 0x6FBF13) both call the same helper 0x43E030(0x835BB8, name, 0x10) (a char ->
//      wide copy of the name into the memcard's name field) BEFORE the write op: 4d9cb2 / 4d9bea 'e8 .. call 0x43e030'. The mod re-points those two call operands
//      (rel32) to its stubs, which write the file and then jump to 0x43E030 itself (the cdecl args and the return address untouched). UG2's save-function heads,
//      left untouched: the mod writes nothing in 0x4D9C60..+0x0F or 0x4D9B80..+0x0A (the rig checks the distance). At the save-as site 0x83A9E0 already holds
//      the name (4e8f41 'mov ecx,0x83a9e0; call 0x502f40' = the name assigned right before 4e8f52), so the file key = 0x83A9E0 at both sites, as everywhere in the mod.
//  (2) THE LOAD: every profile load ends in 0x5344D0 (thiscall(this = 0x83A9E0 = the profile from the name on, buf, size, flag) ret 0xC): called at 5347ab (inside
//      0x534770 = the load from a read buffer, caller 4a9931) and at 534812 (inside 0x5347C0 = the load from the game's kept copy [profile+0x16778], callers 4fba7c /
//      4fd628). The mod re-points those two call operands to one stub that calls 0x5344D0 itself (the three args re-pushed, ret 0xC honoured) and THEN re-reads the
//      mod's file for the name now at 0x83A9E0 - even when the name did not change (a re-load of the same profile after an unsaved session drops the in-memory
//      steps, as the game's own modes are dropped).
//  (3) THE STATE RULE (the rig checks it): a step -> dirty (in memory only, 0 writes); a save -> the file written (dirty or not: the game writes its whole profile
//      too), clean; a load -> the file read, the unsaved steps dropped, clean; a quit without a save -> nothing (the file holds the last save).
#include <stdint.h>
#define UA_SAVE_CALL1 0x004D9CB2u     // in 0x4D9C60 (save current profile): 'e8 79 43 f6 ff' call 0x43E030 - past the 16-byte window at the function's head (0x4D9C60..0x4D9C6F)
#define UA_SAVE_CALL2 0x004D9BEAu     // in 0x4D9B80 (save as name): 'e8 41 44 f6 ff' call 0x43E030 - past the 11-byte window at the function's head (0x4D9B80..0x4D9B8A)
#define UA_SAVE_FN 0x0043E030u    // the helper both sites call (cdecl, 3 args, the caller cleans: 4d9cd1.. 'add esp,0x24' / 4d9bfd 'add esp,0x14')
#define UA_LOAD_CALL1 0x005347ABu     // in 0x534770: 'e8 20 fd ff ff' call 0x5344D0 (then 'ret 8')
#define UA_LOAD_CALL2 0x00534812u     // in 0x5347C0: 'e8 b9 fc ff ff' call 0x5344D0 (then the kept copy freed)
#define UA_LOAD_FN 0x005344D0u    // the profile load: thiscall(this, buf, size, flag) ret 0xC (5346e7 'c2 0c 00'); prologue 55 8B EC 83 EC 24 (not touched)
#define UA_SAVE_PROFILE 0x0083A9E0u   // the profile name = 'this' of the load
#define SAVE_HEAD_WIN 16      // the byte window at UG2's save-function head 0x4D9C60 that the mod leaves alone
#define SAVEAS_HEAD_WIN 11    // .. at the save-as head 0x4D9B80
typedef struct { uint32_t addr; uint32_t target; int site; const char *what; } Save_Site;   // site 1 / 2 = save current / save as; 3 / 4 = load from buffer / from the kept copy
static const Save_Site kSaveSites[4] = {
    { UA_SAVE_CALL1, UA_SAVE_FN, 1, "4d9cb2 call 0x43e030 (in 0x4D9C60 save current profile)" },
    { UA_SAVE_CALL2, UA_SAVE_FN, 2, "4d9bea call 0x43e030 (in 0x4D9B80 save as name)" },
    { UA_LOAD_CALL1, UA_LOAD_FN, 3, "5347ab call 0x5344d0 (in 0x534770 the load from a read buffer)" },
    { UA_LOAD_CALL2, UA_LOAD_FN, 4, "534812 call 0x5344d0 (in 0x5347c0 the load from the kept copy)" },
};
#define SAVE_NSITES 4
static uint32_t Save_Rel(uint32_t addr, uint32_t target) { return target - (addr + 5u); }   // the rel32 a 5-byte call at addr holds to reach target
// the 5 bytes a site must show before the patch (E8 + the rel32 to its original target) and the 4 bytes the patch writes at addr+1 (the rel32 to the stub)
static void Save_Expected(const Save_Site *s, unsigned char out[5]) { uint32_t r = Save_Rel(s->addr, s->target); out[0] = 0xE8; out[1] = (unsigned char)r; out[2] = (unsigned char)(r >> 8); out[3] = (unsigned char)(r >> 16); out[4] = (unsigned char)(r >> 24); }
// ---- (3) the state rule
typedef struct { int dirty, steps, writes, reads, dropped; } Save_State;
static inline void Save_Step(Save_State *s) { s->dirty = 1; s->steps++; }                                   // a bar step or an added row's step: memory only
static inline void Save_Write(Save_State *s) { s->writes++; s->dirty = 0; s->steps = 0; }                    // the game saves: the file written, dirty or not
static inline void Save_Read(Save_State *s) { s->reads++; s->dropped += s->steps; s->dirty = 0; s->steps = 0; }   // a profile load: the file read, the unsaved steps dropped
static inline void Save_Quit(Save_State *s) { s->dropped += s->steps; s->dirty = 0; s->steps = 0; }         // a quit without a save: nothing written
