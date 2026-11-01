// rowwords.h - moves the words (artist / title / album) of every EA Trax row right by 6.8, so all rows line up with room for All Trax's 3-digit row numbers.
// PURE: no game read / write here; shared by the asi (it moves the words and reads them back) and the rig. The numbers below:
//  UG2-EXE the list fill 0x54B310: each of the 4 visible slots (0..3, slot 0 = the top row) has its own text objects found by hash = base + slot - the NUMBER 0xE454E9A5 + i (54b575),
//          the ARTIST 0x66646FC4 + i (54b628), the TITLE 0x2890C8AF + i (54b6d4), the ALBUM 0x77BD189E + i (54b780); the number printed '0%d' for rows 1-9, '%d' from row 10
//          (54ba22-54ba4b) -> rows 100-107 under All Trax get 3 digits.
//  UG2-DATA EA_TRAX_Jukebox.fng (FrontB.lzc #37): NUMBER x -281.0 size 0.80 Sj 4 (left across, centred up-down); ARTIST / TITLE / ALBUM x -253.0 size 0.65
//          Sj 0 (left / top); parent 0 (the x is the screen x); no wrap / width limit; their script tracks = colour only (never x).
//  UG2-EXE the x lives at [obj+0x2C]+0x1C (the FNG's 'SA' copy 5552ff-555324; read by the placement / extent helper 0x51D070), [obj+0x18] == 2 = a text object (the ctor 553d1c).
//  UG2-EXE the ctor's own lookups (553ce4-553cfd): package = 0x52CEF0([screen+4]) cdecl, 1 arg (push esi; call; add esp,4); object = 0x517F80(ecx = package, hash) thiscall, ret 4 (517fce).
//  UG2-DATA the font ConduitMdITC_TT21i (GLOBALA.BUN 0x30201): every digit 13 units wide -> 10.4 per digit at 0.80; 3 digits end at -249.8 = 3.2 INTO the artist at -253.0; the left row
//          arrow 'Left_arrow' image 24 wide at x 99 -> its edge x 87; MW 20's full title 329.6 wide at 0.65 (the longest that fits).
//  THE SHIFT 6.8 splits the room evenly: the 3-digit numbers end 3.6 before the artist; MW 20's full title ends 3.6 before the arrow; the room before the arrow 340 -> 333.2.
#ifndef ROWWORDS_H
#define ROWWORDS_H
#include <stdint.h>
#define WORDS_SHIFT 6.8f                  // 3.2 (3 digits run into the artist) + 3.6: the gap after a 3-digit number = the gap before the arrow after the longest title
#define WORDS_X_FNG (-253.0f)             // the artist / title / album x in the FNG (the value the game puts back at every screen open)
#define WORDS_X_NEW (-246.2f)             // = the FNG's x + the shift (the rig checks the float sum bit for bit)
#define WORDS_X_NUMBER (-281.0f)          // the number objects' x (read only, expected)
#define WORDS_NWORDS 12                   // artist / title / album x 4 slots
#define WORDS_NNUMBERS 4
#define WORDS_HASH_ARTIST 0x66646FC4u     // + slot 0..3 (fill 54b628)
#define WORDS_HASH_TITLE 0x2890C8AFu      // + slot (54b6d4)
#define WORDS_HASH_ALBUM 0x77BD189Eu      // + slot (54b780)
#define WORDS_HASH_NUMBER 0xE454E9A5u     // + slot (54b575), read only
#define UA_WORDS_PACKAGE 0x0052CEF0u      // cdecl([screen+4]) -> the loaded package or 0 (the ctor 553ce4-553cea)
#define UA_WORDS_OBJBYHASH 0x00517F80u    // thiscall(ecx = package, hash) ret 4 -> the FE object or 0 (the ctor 553cf1-553cf8; 517fce 'ret 4')
#define WORDS_OBJ_KIND_OFF 0x18u          // [obj+0x18] == 2 = a text object (553d1c 'cmp [eax+0x18],2')
#define WORDS_OBJ_DATA_OFF 0x2Cu          // [obj+0x2C] = the object's data block (the 'SA' copy)
#define WORDS_DATA_X_OFF 0x1Cu            // data+0x1C = x (0x51D070 reads it; y +0x20, size x +0x38)
#define WORDS_DATA_LEN 0x44u              // the data block checked accessible as one piece before the one read / write at +0x1C
static const unsigned char kWordsPackageHead[11] = { 0xA1, 0xD0, 0x84, 0x83, 0x00, 0x85, 0xC0, 0x56, 0x57, 0x75, 0x40 };               // 52cef0: mov eax,[0x8384D0]; test eax,eax; push esi; push edi; jne
static const unsigned char kWordsObjHead[13] = { 0x6A, 0xFF, 0x68, 0x88, 0x6F, 0x77, 0x00, 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00 };       // 517f80: push -1; push 0x776F88; mov eax,fs:[0]
enum { WORDS_PLAN_SKIP = 0, WORDS_PLAN_WRITE = 1, WORDS_PLAN_ALREADY = 2 };
// the plan for one word object's x as read: the FNG's value -> WRITE the new x; the new x already -> ALREADY (counted, no write); anything else (another value, NaN) -> SKIP: logged with the value, never written
static inline int Words_Plan(float x) { if (x == WORDS_X_FNG) return WORDS_PLAN_WRITE; if (x == WORDS_X_NEW) return WORDS_PLAN_ALREADY; return WORDS_PLAN_SKIP; }
static inline uint32_t Words_WordHash(int i) { return (i < 4 ? WORDS_HASH_ARTIST : i < 8 ? WORDS_HASH_TITLE : WORDS_HASH_ALBUM) + (uint32_t)(i & 3); }   // i 0..11 = artist slot 0..3, title slot 0..3, album slot 0..3
static inline uint32_t Words_NumberHash(int slot) { return WORDS_HASH_NUMBER + (uint32_t)slot; }
// the geometry (the rig re-makes the spacing table from these): the number from x -281 left-aligned, 10.4 per digit; the words from -253 + s; the arrow edge x 87
#define WORDS_DIGIT_W 10.4f               // 13 font units x 0.80
#define WORDS_ARROW_EDGE 87.0f            // the left row arrow image: x 99 - 24 / 2
#define WORDS_MW20_WIDTH 329.6f           // MW 20's full title "Tao Of The Machine (Scott Humphrey's Remix)" at 0.65 = the longest title that fits; Carbon 22's full title (403.7) is over
#define WORDS_CB22_WIDTH 403.7f
static inline float Words_NumberEnd(int digits) { return WORDS_X_NUMBER + WORDS_DIGIT_W * (float)digits; }
static inline float Words_GapAfterNumber(int digits, float s) { return (WORDS_X_FNG + s) - Words_NumberEnd(digits); }   // < 0 = the number runs into the artist
static inline float Words_Room(float s) { return WORDS_ARROW_EDGE - (WORDS_X_FNG + s); }                                 // the room for a row string before the arrow
static inline float Words_TitleGap(float width, float s) { return WORDS_ARROW_EDGE - (WORDS_X_FNG + s + width); }        // < 0 = the title touches / crosses the arrow
#endif
