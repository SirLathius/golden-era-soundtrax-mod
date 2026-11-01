// shuffle.h - the Shuffle button on the EA Trax screen. The sites (UG2-EXE, retail speed2.exe f9dd86c0..), the bytes the asi checks before it writes, and the
// PURE rules the rig runs on the same code: the text by UG2's byte, the flip, the label / click gates, the slot-node walk, the log fold.
// HOW: UG2's PC button bar (one object at [0x8384D8]) has SIX fixed slots; a screen's flag word (the screen table row +0xC) says which show, each set bit taking
// the next slot in bit order (the setup 0x54E6E0, 54e886-54e958). The jukebox row's word 0x7F80C8 = 0x401 (Quit 0x1 -> lower right, Back 0x400 -> upper right).
// + bit 0x80000 -> the THIRD slot = upper middle = LEFT OF BACK, in UG2's own button look; its stock label = 'Test N2O Purge' (hash 0xBA0E1020, 52d46c-52d489),
// its message 0xC539A4B0 (52d237-52d24c) = one the jukebox handler 0x56B1C0 never compares (ignored). The mod prints 'Shuffle On' / 'Shuffle Off' on that slot
// (the one call 54e8d7 re-pointed) and acts on its click (the one call 55df22 re-pointed): [0x83AA58] flipped exactly as UG2's old handler 0x5241E0 did
// (52421d-524226: xor eax,eax; test bl,bl; sete al; mov [0x83AA58],eax), then 0x479220 (ecx = [0x82B884]; 524230-524236) -> [player+0x9C] = what UG2's picker
// (46b746 / 46b914) and the mod's Decide read. The byte is in UG2's profile save (52a2db-52a2e3: [0x83A9E0+0x48] 15 dwords, +0x30) = a flip is kept by UG2's
// own save and raises UG2's own save prompt. UG2 starts on Random (0x5116E0 51174c: [+0x54] := 1) = 'Shuffle On' with NO write by the mod; the mod writes the
// byte only on a click.
#ifndef SHUFFLE_H
#define SHUFFLE_H
#include <stdint.h>
#include <string.h>
// ---- (1) the sites
#define UA_SHUF_FLAGWORD 0x007F80C8u     // .data (file 0x3F80C8): the jukebox row's flag word (row 0x7F80BC + 0xC); one reader 50b7fc (the screen create); no writer in UG2
#define SHUF_FLAGS_OLD 0x00000401u      // Quit + Back
#define SHUF_FLAGS_NEW 0x00080401u      // + bit 0x80000 = slot 3 (upper middle, left of Back); 0x80000 > 0x400 so Back keeps slot 2; not one of the setup's skipped bits
#define SHUF_BIT 0x00080000u
#define UA_SHUF_LABELCALL 0x0054E8D7u    // the setup's walk: 'call 0x50C900' E8 24 E0 FB FF (file 0x14E8D7), cdecl(obj = [node+0x10], hash); 54e8dc 'add esp,8', eax reloaded
#define UA_SHUF_CLICKCALL 0x0055DF22u    // the bar click 0x55DE00: 'call 0x52D170' E8 49 F2 FC FF (file 0x15DF22), thiscall(ecx = bar, bit) ret 4 -> eax = the message
#define UA_SHUF_LABEL_ORIG 0x0050C900u   // cdecl(obj, hash): the label by hash (the soundtrack bar's label routine uses it too; its head is checked)
#define UA_SHUF_MSG_ORIG 0x0052D170u     // thiscall(bar, bit) ret 4: the bit -> message map (one caller 55df22)
#define UA_SHUF_LISTSETUP 0x00479220u    // thiscall(ecx = [0x82B884]) no stack args, 'ret' (4792ab): [0x83AA58] -> 0x46BAA0(.., play mode) -> [player+0x9C] (46bae7)
#define UA_SHUF_PLAYMODE 0x0083AA58u     // dword (UG2 writes a dword 524226, reads the byte 5241ea / the dword 479223): 1 = Random = Shuffle On, 0 = Sequential = Shuffle Off
#define UA_SHUF_BAR 0x008384D8u          // [0x8384D8] = the PC button bar object (551fdb)
#define SHUF_BAR_PKG_OFF 0x58u          // [bar+0x58] = the package name (char *) the setup stored (54e774) = the screen's name; 0x55DDA0 sends the message to it by name
#define SHUF_BAR_LIST_OFF 0x4Cu         // [bar+0x4C] = the first slot node's link; node = link - 4; [node+4] = the next link; the list ends when the link == bar+0x4C
#define SHUF_NODE_TEXT 0x10u            // [node+0x10] = the slot's text object (type 2)
#define SHUF_NODE_SHOWN 0x18u           // byte: 1 = shown (54e8e7)
#define SHUF_NODE_BIT 0x1Cu             // dword: the bit this slot carries (54e8e4)
#define SHUF_HASH_N2O 0xBA0E1020u       // bit 0x80000's stock label 'Test N2O Purge' = what the setup hands the node right before the re-pointed call
#define SHUF_PKG "EA_Trax_Jukebox.fng"  // the jukebox row's name (0x793db8)
static const unsigned char kShufFlagBytes[4] = { 0x01, 0x04, 0x00, 0x00 };
static const unsigned char kShufLabelCall[5] = { 0xE8, 0x24, 0xE0, 0xFB, 0xFF };
static const unsigned char kShufClickCall[5] = { 0xE8, 0x49, 0xF2, 0xFC, 0xFF };
static const unsigned char kShufMsgHead[9] = { 0x8B, 0x44, 0x24, 0x04, 0x3D, 0x00, 0x00, 0x04, 0x00 };       // 52d170 mov eax,[esp+4]; cmp eax,0x40000
static const unsigned char kShufListSetupHead[8] = { 0x83, 0xEC, 0x08, 0xA1, 0x58, 0xAA, 0x83, 0x00 };      // 479220 sub esp,8; mov eax,[0x83AA58]
// ---- (2) the pure rules
static inline const char *Shuf_Text(uint32_t playMode) { return (playMode & 0xFFu) ? "Shuffle On" : "Shuffle Off"; }   // UG2 tests the BYTE (5241ea 'mov bl,[0x83AA58]'); 10 / 11 chars < the slot's 16
static inline uint32_t Shuf_Flip(uint32_t playMode) { return (playMode & 0xFFu) == 0 ? 1u : 0u; }                       // 52421d-524224: xor eax,eax; test bl,bl; sete al -> the dword written
static inline int Shuf_IsJukebox(const char *pkg) { return pkg != NULL && strcmp(pkg, SHUF_PKG) == 0; }
static inline int Shuf_LabelWanted(uint32_t hash, const char *pkg, uint32_t objKind) { return hash == SHUF_HASH_N2O && Shuf_IsJukebox(pkg) && objKind == 2; }
static inline int Shuf_ClickWanted(uint32_t bit, const char *pkg) { return bit == SHUF_BIT && Shuf_IsJukebox(pkg); }
static inline uint32_t Shuf_Rel32(uint32_t site, uint32_t target) { return target - (site + 5u); }                       // the call operand (E8 rel32) from a site to a target
// ---- (3) the slot-node walk (as the setup's 54e853-54e958 and the click's 55de29-55de6a walk it), through read callbacks (the asi: its guarded reads; the rig: a fake memory).
// -> the node (= link - 4) whose +0x1C == bit and +0x18 == 1, or 0 (not shown / not found / a read failed / more than 8 links = not UG2's 6-slot list).
typedef int (*Shuf_Rd32)(void *ctx, uint32_t a, uint32_t *v);
typedef int (*Shuf_Rd8)(void *ctx, uint32_t a, uint32_t *v);
static inline uint32_t Shuf_FindNode(uint32_t bar, uint32_t bit, Shuf_Rd32 rd32, Shuf_Rd8 rd8, void *ctx, int *steps)
{
    uint32_t head = bar + SHUF_BAR_LIST_OFF, link = 0; *steps = 0;
    if (!rd32(ctx, head, &link)) return 0;
    while (link && link != head && *steps < 8) {
        uint32_t node = link - 4u, b = 0, shown = 0; (*steps)++;
        if (!rd32(ctx, node + SHUF_NODE_BIT, &b) || !rd8(ctx, node + SHUF_NODE_SHOWN, &shown)) return 0;
        if (b == bit && shown == 1) return node;
        if (!rd32(ctx, node + 4u, &link)) return 0;
    }
    return 0;
}
// ---- (4) the log fold: the '-1 no song allowed' line is logged for the first pick of a run + one count line when the run ends (a pick that answers otherwise).
// hit = this pick answered -1 from that path. -> 1 = log the first line (count = 1), 2 = the run ended: log its count (count = the run's length; the run reset), 0 = nothing.
typedef struct { int run; } Shuf_NoneFold;
static inline int Shuf_Fold(Shuf_NoneFold *f, int hit, int *count)
{
    if (hit) { f->run++; *count = f->run; return f->run == 1 ? 1 : 0; }
    if (f->run) { *count = f->run; f->run = 0; return 2; }
    *count = 0; return 0;
}
#endif
