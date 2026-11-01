// tracklist.h - the EA Trax list: the view, the row modes and the preview. The PURE parts (no Windows call, no game memory), so the offline rig runs them
// natively. Included at the end of songlogic.h (needs its ids, the settings parser, kMwMode and the live mode arrays).
//  (1) THE VIEW = what UG2's fill 0x54B310 reads instead of the live list 0x83ACB8 (27 x {int song, byte mode}) and the song table 0x7ECD80 (27 x {title, artist,
//      album, pool}): the fill's 8 absolute operands are re-pointed ONCE at load to g_view (109 x 8 bytes, the same entry layout) and g_vtab (109 x 16 bytes, the
//      same row layout), its row bound 'cmp edi,0x1b' (54bb36, imm8) := N at every screen open. A row's +0x18 = the ENTRY's id (the fill copies it as UG2 does):
//      ids 0..26 = UG2's song indices EXACTLY (so UG2's own row cycle 0x53DEB0 and preview 0x480C10 work on them unchanged), 27..108 = the added songs
//      (songlogic.h's id scheme: no view index, no translation, every UG2 row byte-identical to UG2's own). The view per soundtrack: Underground 2 = the live list's
//      27 entries in list order + the live table (UG2's own screen exactly); Underground = UG1 rows 0..25; Most Wanted = MW 1..26 but 25 / 26 (left out); Carbon =
//      Carbon 0..29 in its list order; each with that game's live modes; All Trax = UG1's 26, UG2's 27, MW's 24, then Carbon's 30 = 107 rows (<= 127 = the imm8
//      limit), every row's mode from All Trax's own set.
//  (2) THE ROW CYCLE 0x53DEB0: its search 'mov eax,0x83acb8 .. cmp ecx,0xd8' (53dec8 / 53dee0) is re-pointed to g_view (bound 109 x 8 = 0x368; the pad entries
//      carry id -1), so UG2's own body finds the row's VIEW entry, steps ITS mode byte and writes it + the row's +0x1C + the label + the sound + the arrow blink +
//      0x479220, as UG2 does. The body thus NEVER writes 0x83ACB8 - the mod's PRE-hook at the head mirrors the step into the set the view shows: a UG2 row under
//      Underground 2 -> the live list's entry for that song gets the same new mode (before the body's 0x479220 rebuilds the masks from it); an MW / UG1 / Carbon
//      row under its own category -> that game's live mode + the profile file's 'MW / UG1 / CB nn' line; any row under All Trax -> All Trax's own set + its
//      'ALL .. nn' line (the live list untouched). The step rule is copied from the bytes (the rig checks it against the exe).
//  (3) THE PREVIEW 0x480C10(mgr, idx): idx 0..26 = UG2's own path (a carrier slot 25 / 26 first given back its own wave + row, so row 'Skeptic' plays Skeptic);
//      27..108 = an added song: a carrier != g_last (the carrier rule) gets its wave + row, then UG2's own 0x47AF90(player, carrier) plays it (the pop-up shows the
//      carrier's rewritten row). MW 25 / 26 (left out) = nothing (UG2's own 'cmp ebp,0x1b; jge' would play nothing for any id >= 27 anyway).
//  (4) the modes: the live mode arrays (songlogic.h) = each game's defaults until the profile's file says otherwise; the allowed rule reads them (the picker's
//      sets follow the screen).
//  (5) the code sites (UG2-EXE, the retail speed2.exe) + the patch model (the rig checks them against the exe file).
#define LIST_MAXVIEW 109                      // UG2 27 + MW 26 (25 / 26 never shown, the slots stay reserved) + UG1 26 + Carbon 30: the cycle's search bound = 109 x 8 = 0x368
#define LIST_NTAB SONG_NSONG                   // the table rows: ids 0..108
#define LIST_SEARCH_BOUND (LIST_MAXVIEW * 8)
#define LIST_IMM8_MAX 127                     // 'cmp edi,imm8' is signed: the row count must stay below this (All Trax = 107: 26 + 27 + 24 + 30)
typedef struct { int id; unsigned char mode; unsigned char pad[3]; } List_ViewEntry;   // 8 bytes = a live-list entry (int song, byte mode at +4)
typedef struct { uint32_t title, artist, album, pool; } List_TabRow;                      // 16 bytes = a song-table row (0x7ECD80: +0 title, +4 artist, +8 album, +0xC pool)
// the live modes from the parsed settings: a file line -> its value, no line -> that game's own default; then All Trax's own set
static void List_ApplyModes(Set_Settings *s)   // s is written too (the copy fills the missing ALL lines of a header-1 / no-header file)
{
    for (int n = 0; n < 27; n++) list_mwMode[n] = (n >= 1 && s->mwMode[n] >= 0) ? (unsigned char)s->mwMode[n] : kMwMode[n];
    list_modesSet = 1;
    for (int n = 0; n < 27; n++) list_u1Mode[n] = (n >= 1 && s->u1Mode[n] >= 0) ? (unsigned char)s->u1Mode[n] : kU1Mode[n];   // the UG1 modes the same way (nn = row + 1)
    list_u1Set = 1;
    for (int n = 0; n < 31; n++) list_cbMode[n] = (n >= 1 && s->cbMode[n] >= 0) ? (unsigned char)s->cbMode[n] : kCbMode[n];   // the Carbon modes the same way (nn = c + 1)
    list_cbSet = 1;
    // ALL TRAX's own set. Header 2 (this release's format) = as written: an 'ALL .. nn' line -> its value, no line -> All Trax's own default. Header 1 / none (an
    // older release's shared settings, or a round trip through an older release that kept the ALL lines as text) = 'copy into every category': every added song
    // with a game line and no ALL line gets the game line's value as its ALL line (in s too, so the copy is made once and the next save writes header 2 + the ALL
    // lines). UG2's 27 songs: their shared modes live in UG2's list 0x83ACB8, not in the file -> the default here; the asi copies the list's modes over them right
    // after (SettingsRead, the same guarded read as BuildView).
    s->inherited = 0;
    if (s->headerVer < 2) {
        for (int n = 1; n <= 26; n++) { if (s->allMode[SONG_ID_MW(n)] < 0 && s->mwMode[n] >= 0) { s->allMode[SONG_ID_MW(n)] = s->mwMode[n]; s->inherited++; }
                                        if (s->allMode[SONG_ID_U1(n - 1)] < 0 && s->u1Mode[n] >= 0) { s->allMode[SONG_ID_U1(n - 1)] = s->u1Mode[n]; s->inherited++; } }
        for (int n = 1; n <= 30; n++) if (s->allMode[SONG_ID_CB(n - 1)] < 0 && s->cbMode[n] >= 0) { s->allMode[SONG_ID_CB(n - 1)] = s->cbMode[n]; s->inherited++; }
    }
    for (int id = 0; id < SONG_NSONG; id++) list_allMode[id] = s->allMode[id] >= 0 ? (unsigned char)s->allMode[id] : Song_AllDefault(id);
    list_allSet = 1;
}
// the view list for a soundtrack -> N (entries beyond N padded with id -1 = never a row's id). An out-of-range live-list song index is skipped + counted (never an id
// that would make the fill read outside g_vtab).
static int List_BuildView(int soundtrack, const int *listSong, const unsigned char *listMode, List_ViewEntry *out, int *bad)
{
    int n = 0; *bad = 0;
    if (soundtrack == SET_ST_UG1 || soundtrack == SET_ST_ALL)   // UG1's rows first under All Trax
        for (int r = 0; r < SONG_NU1; r++) { out[n].id = SONG_ID_U1(r); out[n].mode = soundtrack == SET_ST_ALL ? SONG_ALLMODE(SONG_ID_U1(r)) : LIST_U1MODE(r + 1); out[n].pad[0] = out[n].pad[1] = out[n].pad[2] = 0; n++; }
    if (soundtrack == SET_ST_UG2 || soundtrack == SET_ST_ALL)
        for (int p = 0; p < SONG_NUG2; p++) { if (listSong[p] < 0 || listSong[p] >= SONG_NUG2) { (*bad)++; continue; } out[n].id = listSong[p]; out[n].mode = soundtrack == SET_ST_ALL ? SONG_ALLMODE(listSong[p]) : listMode[p]; out[n].pad[0] = out[n].pad[1] = out[n].pad[2] = 0; n++; }
    if (soundtrack == SET_ST_MW || soundtrack == SET_ST_ALL)
        for (int k = 1; k <= 26; k++) { if (SONG_MW_OUT(k)) continue; out[n].id = SONG_ID_MW(k); out[n].mode = soundtrack == SET_ST_ALL ? SONG_ALLMODE(SONG_ID_MW(k)) : LIST_MWMODE(k); out[n].pad[0] = out[n].pad[1] = out[n].pad[2] = 0; n++; }
    if (soundtrack == SET_ST_CARBON || soundtrack == SET_ST_ALL)   // Carbon's rows last under All Trax, in Carbon's list order
        for (int c = 0; c < SONG_NCB; c++) { out[n].id = SONG_ID_CB(c); out[n].mode = soundtrack == SET_ST_ALL ? SONG_ALLMODE(SONG_ID_CB(c)) : LIST_CBMODE(c + 1); out[n].pad[0] = out[n].pad[1] = out[n].pad[2] = 0; n++; }
    for (int k = n; k < LIST_MAXVIEW; k++) { out[k].id = -1; out[k].mode = 0; out[k].pad[0] = out[k].pad[1] = out[k].pad[2] = 0; }
    return n;
}
// the view table: rows 0..26 = the LIVE song table's rows (row 0's artist / row 19's album included) but the carriers 25 / 26 from the rows SAVED at load (UG2's own
// strings: a borrowed title never shows as a list row); rows 27..52 = MW 1..26's strings, 53..78 = UG1's, 79..108 = Carbon's (pool 0: the fill never reads +0xC)
static void List_BuildTable(const List_TabRow *live, const List_TabRow *saved, const char *const (*mw)[3], const char *const (*u1)[3], const char *const (*cbt)[3], List_TabRow *out)   // u1 = kU1 (rows 53..78), cbt = kCb (rows 79..108)
{
    for (int c = 0; c < SONG_NCB; c++) { List_TabRow *q = &out[SONG_ID_CB(c)]; q->title = (uint32_t)(uintptr_t)cbt[c][0]; q->artist = (uint32_t)(uintptr_t)cbt[c][1]; q->album = (uint32_t)(uintptr_t)cbt[c][2]; q->pool = 0; }
    for (int r = 0; r < SONG_NU1; r++) { List_TabRow *q = &out[SONG_ID_U1(r)]; q->title = (uint32_t)(uintptr_t)u1[r][0]; q->artist = (uint32_t)(uintptr_t)u1[r][1]; q->album = (uint32_t)(uintptr_t)u1[r][2]; q->pool = 0; }
    for (int s = 0; s < SONG_NUG2; s++) out[s] = (s == SONG_CARRIER_A || s == SONG_CARRIER_B) ? saved[s] : live[s];
    for (int k = 1; k <= 26; k++) { List_TabRow *r = &out[SONG_ID_MW(k)]; r->title = (uint32_t)(uintptr_t)mw[k][0]; r->artist = (uint32_t)(uintptr_t)mw[k][1]; r->album = (uint32_t)(uintptr_t)mw[k][2]; r->pool = 0; }
}
// UG2's row mode cycle 0x53DEB0 (UG2-EXE): the hash argument 0x9120409E (left: 53deec; 53defb 'dec ebx; jns; mov ebx,3' = 0 -> 3) / 0xB5971BF1 (right: 53df72;
// 53df7a 'inc ebx; cmp ebx,3; jle; xor ebx,ebx' = 3 -> 0); any other hash = the mode unchanged (53df78 jne 0x53dfe1 writes the old byte back)
#define LIST_HASH_LEFT 0x9120409Eu
#define LIST_HASH_RIGHT 0xB5971BF1u
static int List_StepMode(int m, unsigned hash) { if (hash == LIST_HASH_LEFT) return m - 1 < 0 ? 3 : m - 1; if (hash == LIST_HASH_RIGHT) return m + 1 > 3 ? 0 : m + 1; return m; }
// the pre-hook's plan for one row step: the row's id (its +0x18) + the hash -> kind 0 = nothing to mirror (a foreign hash, or no view entry with that id: the body's search
// then ends on the last pad entry = the mod's own memory); 1 = a UG2 song: the live list's position listPos (song == id) gets newMode; 2 = an MW song mwN: the live MW mode +
// the file line; 3 = a UG1 row: mwN = nn = row + 1 -> the live UG1 mode + the 'UG1 nn' file line; 4 = a Carbon row: mwN = nn = c + 1 -> the live Carbon mode + the 'CB nn'
// line. viewIdx = the entry UG2's body will find (the first with that id) and step from oldMode to newMode.
// viewSt = the soundtrack the view was built for (what the rows show; g_viewSt = the bar's after every RefillList): All Trax -> kind 5 = All Trax's own set + its ALL
// line for EVERY row (listPos -1: the live list is not written; mwN = the song's nn for the 'ALL .. nn' line: UG2 song + 1 / MW n / UG1 row + 1 / Carbon c + 1); any other -> kinds 1-4
typedef struct { int kind, viewIdx, oldMode, newMode, listPos, mwN; } List_RowPlan;
static List_RowPlan List_PlanRowStep(int viewSt, int id, unsigned hash, const List_ViewEntry *view, const int *listSong)
{
    List_RowPlan r = { 0, -1, -1, -1, -1, 0 };
    for (int k = 0; k < LIST_MAXVIEW; k++) if (view[k].id == id) { r.viewIdx = k; break; }
    if (r.viewIdx < 0 || (hash != LIST_HASH_LEFT && hash != LIST_HASH_RIGHT)) return r;      // no entry (viewIdx -1) or a foreign hash (viewIdx set): nothing to mirror
    r.oldMode = view[r.viewIdx].mode; r.newMode = List_StepMode(r.oldMode, hash);
    if (viewSt == SET_ST_ALL) { r.kind = 5; r.mwN = (id >= 0 && id < SONG_NUG2) ? id + 1 : SONG_IS_MW(id) ? SONG_MW_N(id) : SONG_IS_U1(id) ? SONG_U1_R(id) + 1 : SONG_IS_CB(id) ? SONG_CB_C(id) + 1 : 0; return r; }   // the view shows All Trax -> All Trax's own set only
    if (id >= 0 && id < SONG_NUG2) { for (int p = 0; p < SONG_NUG2; p++) if (listSong[p] == id) { r.listPos = p; break; } r.kind = r.listPos >= 0 ? 1 : 0; }
    else if (SONG_IS_MW(id)) { r.mwN = SONG_MW_N(id); r.kind = 2; }
    else if (SONG_IS_U1(id)) { r.mwN = SONG_U1_R(id) + 1; r.kind = 3; }   // a UG1 row (nn = row + 1)
    else if (SONG_IS_CB(id)) { r.mwN = SONG_CB_C(id) + 1; r.kind = 4; }   // a Carbon row (nn = c + 1)
    return r;
}
// the preview hook's plan: action 0 = UG2's own path (idx 0..26; giveBack = the carrier slot 25 / 26 to restore first, -1 = none); 1 = an added song (MW mwN, a UG1 row
// mwN = nn = row + 1, or a Carbon row mwN = nn = c + 1) through a carrier (the carrier rule: never g_last; plan.want[] = the id, as at a pick); 2 = nothing (MW 25 / 26
// out, or an id outside 0..108: UG2's 0x47AF90 plays nothing for idx >= 27 either)
typedef struct { int action, giveBack, carrier, mwN; Song_Plan plan; } List_PrevPlan;
static List_PrevPlan List_PlanPreview(int idx, int last, int lastCarrier)
{
    List_PrevPlan p; memset(&p, 0, sizeof p); p.giveBack = -1; p.carrier = -1;
    if (idx >= 0 && idx < SONG_NUG2) { p.action = 0; if (idx == SONG_CARRIER_A || idx == SONG_CARRIER_B) p.giveBack = idx; return p; }
    if (SONG_IS_MW(idx) && !SONG_MW_OUT(SONG_MW_N(idx))) { p.action = 1; p.mwN = SONG_MW_N(idx); p.plan = Song_CarrierPlan(idx, last, lastCarrier); p.carrier = p.plan.carrier; return p; }
    if (SONG_IS_U1(idx)) { p.action = 1; p.mwN = SONG_U1_R(idx) + 1; p.plan = Song_CarrierPlan(idx, last, lastCarrier); p.carrier = p.plan.carrier; return p; }   // a UG1 row the same way
    if (SONG_IS_CB(idx)) { p.action = 1; p.mwN = SONG_CB_C(idx) + 1; p.plan = Song_CarrierPlan(idx, last, lastCarrier); p.carrier = p.plan.carrier; return p; }   // a Carbon row the same way
    p.action = 2; return p;
}
// ---- (5) the code sites. kind: 0 = disp32 -> g_view (+0, the id), 1 = disp32 -> g_view + 4 (the mode byte), 2 / 3 / 4 = disp32 -> g_vtab + 0 / 4 / 8 (title / artist /
// album), 5 = the imm8 row bound := N (written at every screen open), 6 = imm32 -> g_view (the cycle's search base), 7 = imm32 := the search bound 109 x 8, 8 = a hook
// head (a 5-byte jmp; the original bytes go to the trampoline), 9 = checked only (the continuation / callee / caller bytes the build relies on)
typedef struct { uint32_t addr; unsigned char len, patchOff, kind; const char *what; unsigned char bytes[8]; } List_Site;
static const List_Site kListSites[] = {
    { 0x0054BA66u, 7, 3, 0, "54ba66 mov edx,[edi*8+0x83acb8] (the artist's song)", { 0x8B, 0x14, 0xFD, 0xB8, 0xAC, 0x83, 0x00, 0 } },
    { 0x0054BA72u, 6, 2, 3, "54ba72 mov eax,[edx+0x7ecd84] (artist)", { 0x8B, 0x82, 0x84, 0xCD, 0x7E, 0x00, 0, 0 } },
    { 0x0054BA82u, 7, 3, 0, "54ba82 mov ecx,[edi*8+0x83acb8] (the title's song)", { 0x8B, 0x0C, 0xFD, 0xB8, 0xAC, 0x83, 0x00, 0 } },
    { 0x0054BA8Eu, 6, 2, 2, "54ba8e mov edx,[ecx+0x7ecd80] (title)", { 0x8B, 0x91, 0x80, 0xCD, 0x7E, 0x00, 0, 0 } },
    { 0x0054BA9Eu, 7, 3, 0, "54ba9e mov eax,[edi*8+0x83acb8] (the album's song)", { 0x8B, 0x04, 0xFD, 0xB8, 0xAC, 0x83, 0x00, 0 } },
    { 0x0054BAAAu, 6, 2, 4, "54baaa mov ecx,[eax+0x7ecd88] (album)", { 0x8B, 0x88, 0x88, 0xCD, 0x7E, 0x00, 0, 0 } },
    { 0x0054BABAu, 7, 3, 0, "54baba mov edx,[edi*8+0x83acb8] -> [row+0x18] (the id)", { 0x8B, 0x14, 0xFD, 0xB8, 0xAC, 0x83, 0x00, 0 } },
    { 0x0054BAC4u, 7, 3, 1, "54bac4 mov al,[edi*8+0x83acbc] -> [row+0x1c] (the mode)", { 0x8A, 0x04, 0xFD, 0xBC, 0xAC, 0x83, 0x00, 0 } },
    { 0x0054BB36u, 3, 2, 5, "54bb36 cmp edi,0x1b (the row count)", { 0x83, 0xFF, 0x1B, 0, 0, 0, 0, 0 } },
    { 0x0053DEC8u, 5, 1, 6, "53dec8 mov eax,0x83acb8 (the cycle's search base)", { 0xB8, 0xB8, 0xAC, 0x83, 0x00, 0, 0, 0 } },
    { 0x0053DEE0u, 6, 2, 7, "53dee0 cmp ecx,0xd8 (the search bound 27 x 8)", { 0x81, 0xF9, 0xD8, 0x00, 0x00, 0x00, 0, 0 } },
    { 0x0053DEB0u, 5, 0, 8, "53deb0 head sub esp,0x1c; push ebp; push esi (the row cycle)", { 0x83, 0xEC, 0x1C, 0x55, 0x56, 0, 0, 0 } },
    { 0x00480C10u, 6, 0, 8, "480c10 head mov ecx,[ecx+0xa0] (the preview)", { 0x8B, 0x89, 0xA0, 0x00, 0x00, 0x00, 0, 0 } },
    { 0x0054BB39u, 6, 0, 9, "54bb39 jl 0x54b9a0 (the loop's back edge)", { 0x0F, 0x8C, 0x61, 0xFE, 0xFF, 0xFF, 0, 0 } },
    { 0x0053DEB5u, 8, 0, 9, "53deb5 mov esi,ecx; mov eax,[esi+0x70]; mov edx,[eax+0x18] (the row + its id = the trampoline's return)", { 0x8B, 0xF1, 0x8B, 0x46, 0x70, 0x8B, 0x50, 0x18 } },
    { 0x0053DFE5u, 3, 0, 9, "53dfe5 mov [eax+4],bl (the body's one list write = the found entry's mode)", { 0x88, 0x58, 0x04, 0, 0, 0, 0, 0 } },
    { 0x00480C16u, 8, 0, 9, "480c16 test ecx,ecx; je 0x480c1f; jmp 0x47af90 (the trampoline's return)", { 0x85, 0xC9, 0x74, 0x05, 0xE9, 0x71, 0xA3, 0xFF } },
    { 0x0047AF90u, 8, 0, 9, "47af90 push ebp; mov ebp,[esp+8]; cmp ebp,0x1b (the slot player: the callee for an added song)", { 0x55, 0x8B, 0x6C, 0x24, 0x08, 0x83, 0xFD, 0x1B } },
    { 0x0056B201u, 8, 0, 9, "56b201 mov ecx,[eax+0x18]; push ecx; mov ecx,[0x82b884] (the preview's caller passes the row's id)", { 0x8B, 0x48, 0x18, 0x51, 0x8B, 0x0D, 0x84, 0xB8 } },
    // the rebuild of the rows after a bar step = UG2's own code, checked only
    { 0x004358E0u, 8, 0, 9, "4358e0 the list-destroy helper (unlink + delete each item; 23 callers incl. the list control's dtor 4cbbce / 4cbbde and the row's dtor 531619)", { 0x56, 0x8B, 0xF1, 0x8B, 0x06, 0x3B, 0xC6, 0x74 } },
    { 0x0054B32Bu, 6, 0, 9, "54b32b mov eax,[ecx+0x124] (the fill's header gate)", { 0x8B, 0x81, 0x24, 0x01, 0x00, 0x00, 0, 0 } },
    { 0x0054B341u, 6, 0, 9, "54b341 jle 0x54b994 (gate <= 0 -> the row loop only)", { 0x0F, 0x8E, 0x4D, 0x06, 0x00, 0x00, 0, 0 } },
    { 0x00553CD8u, 8, 0, 9, "553cd8 mov [ebx+0x124],4 (the ctor: 4 header items)", { 0xC7, 0x83, 0x24, 0x01, 0x00, 0x00, 0x04, 0x00 } },
    { 0x00542CC2u, 6, 0, 9, "542cc2 mov [esi],0x795cb4 (the list control's vtable, its ctor)", { 0xC7, 0x06, 0xB4, 0x5C, 0x79, 0x00, 0, 0 } },
    { 0x004CBBC3u, 8, 0, 9, "4cbbc3 lea ecx,[esi+0x10]; .. call 0x4358e0 (the list control's dtor destroys the row list the same way)", { 0x8D, 0x4E, 0x10, 0xC7, 0x44, 0x24, 0x10, 0x00 } },
    { 0x0054BA01u, 8, 0, 9, "54ba01 mov [ebx+0x28],eax; mov [ebx+0x20],1 (the fill: the first-visible row + the flag when they were 0)", { 0x89, 0x43, 0x28, 0xC7, 0x43, 0x20, 0x01, 0x00 } },
};
// the list control at screen+0x4C (its ctor 0x542CA0: vtable 0x795CB4; +0x8 / +0xC the header-item list (count +0x18), +0x10 / +0x14 the ROW list (count +0x1C), +0x20 a flag set with the
// first row, +0x24 the SELECTED row (= screen+0x70, what the cycle / preview read), +0x28 the first visible row; all of +0x18..+0x30 zeroed by the ctor). The rebuild: destroy the row list
// (0x4358E0 on +0x10, as the dtor 0x4CBBA0 does), zero +0x1C / +0x20 / +0x24 / +0x28, then the fill with [screen+0x124] temporarily 0 (its header part skipped: 54b32b / 54b341).
#define LIST_LISTCTL_OFF 0x4Cu
#define LIST_LISTCTL_VTABLE 0x00795CB4u
#define LIST_ROWLIST_OFF 0x10u
#define LIST_ROWCOUNT_OFF 0x1Cu
#define LIST_FLAG_OFF 0x20u
#define LIST_SELECTED_OFF 0x24u
#define LIST_FIRSTVIS_OFF 0x28u
#define LIST_HEADERGATE_OFF 0x124u
#define LIST_HEADERGATE_VALUE 4u
#define UA_LIST_DELLIST 0x004358E0u
#define UA_LIST_FILL 0x0054B310u
#define LIST_NSITES ((int)(sizeof kListSites / sizeof kListSites[0]))   // 19 patched sites + their checked neighbours, + 7 for the row rebuild (checked only) = 26
// the value written at site.patchOff for kinds 0..7 (4 bytes, kind 5 = 1 byte)
static uint32_t List_PatchValue(int kind, uint32_t viewBase, uint32_t tabBase, int n)
{
    switch (kind) { case 0: return viewBase; case 1: return viewBase + 4; case 2: return tabBase; case 3: return tabBase + 4; case 4: return tabBase + 8; case 5: return (uint32_t)n; case 6: return viewBase; case 7: return LIST_SEARCH_BOUND; }
    return 0;
}
static inline int List_PatchLen(int kind) { return kind == 5 ? 1 : (kind >= 0 && kind <= 7) ? 4 : 0; }   // the rig uses it; the asi writes 4 bytes for kinds 0-4 / 6-7 and 1 for the bound
