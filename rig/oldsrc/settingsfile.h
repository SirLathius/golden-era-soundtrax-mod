// rig/oldsrc/settingsfile.h - the settings-file parser of an OLDER release of this mod, kept so the rig can prove an older release still reads a file written
// by this one (rig/compat_old.c; tools/rig.sh checks this file's hash first). Its code is the older release's, with this release's code names. Its file format:
//        UG2Music settings 1                  the header
//        soundtrack = MW                      the bar's soundtrack (a missing line / an unknown or uninstalled value = the default, counted)
//        MW nn / UG1 nn / CB nn = FE | IG | AL | OFF   the added songs' modes, ONE set shared by every category (no line = that game's own default)
//      every other non-empty line is kept verbatim and written back (64 at most, the rest dropped) - this is where a newer file's ALL lines end up.
//      It holds the same soundtrack values, arrow step, filter and code sites as the current settingsfile.h.
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define SET_ST_UG2 0
#define SET_ST_UG1 1
#define SET_ST_MW 2
#define SET_ST_CARBON 3
#define SET_ST_ALL 4
#define SET_ST_OFF 5
#define SET_NST 6
static const char *const kSetName[SET_NST] = { "UG2", "UG1", "MW", "CARBON", "ALL", "OFF" };     // the file's words, in value order
// the texts the asi prints into the top bar. UG2's print routine 0x537B30 cuts the text at the object's length field [obj+0x68] (14 for this bar), so every
// text is <= 13 characters. Off = UG2's own label (unused: Off is not installed)
static const char *const kSetText[SET_NST] = { "Underground 2", "Underground", "Most Wanted", "Carbon", "All Trax", "Off" };
#define SET_TEXT_MAX 13   // the bar's text object takes 14 characters (its +0x68); the rig checks every text against this
#define SET_INSTALLED ((1u << SET_ST_UG2) | (1u << SET_ST_UG1) | (1u << SET_ST_MW) | (1u << SET_ST_CARBON) | (1u << SET_ST_ALL))   // every value but Off
#define SET_DEFAULT SET_ST_UG2                                                                      // a new profile / no file / no line = Underground 2 = the stock game
static int Set_Installed(int st) { return st >= 0 && st < SET_NST && ((SET_INSTALLED >> st) & 1u) != 0; }
// one arrow step: dir -1 = left / previous, +1 = right / next, along kSetOrder = All Trax, Underground, Underground 2, Most Wanted, Carbon; wraps (Carbon -> All
// Trax); skips the uninstalled; an uninstalled start counts from the default. The value numbers / file words are in their own order (kSetName).
#define SET_NORDER 5
static const int kSetOrder[SET_NORDER] = { SET_ST_ALL, SET_ST_UG1, SET_ST_UG2, SET_ST_MW, SET_ST_CARBON };
static int Set_Step(int st, int dir)
{
    if (!Set_Installed(st)) st = SET_DEFAULT;
    if (dir != -1 && dir != 1) return st;
    int k = -1; for (int i = 0; i < SET_NORDER; i++) if (kSetOrder[i] == st) k = i;
    if (k < 0) return SET_DEFAULT;
    for (int n = 0; n < SET_NORDER; n++) { k = (k + dir + SET_NORDER) % SET_NORDER; if (Set_Installed(kSetOrder[k])) return kSetOrder[k]; }
    return SET_DEFAULT;
}
#define SET_DEC_STOCK (-2)          // UG2: the mod returns -2 = the stock picker runs (UG2 exactly), the carriers given back first
#define SET_DEC_NONE (-1)           // Off: -1 = UG2's own 'no song' path (47b0d7 / 47b1ce 'cmp eax,-1')
#define SET_DEC_OWN 0               // UG1 / MW / Carbon / All: the mod's own pick (songlogic.h) over the filtered allowed set
static int Set_Decision(int st) { return st == SET_ST_UG2 ? SET_DEC_STOCK : st == SET_ST_OFF ? SET_DEC_NONE : SET_DEC_OWN; }
// the soundtrack filter on an id (0..26 = UG2's songs, 27..52 = MW 1..26, 53..78 = UG1 rows 0..25, 79..108 = Carbon 0..29): All = every id; MW = MW ids; UG1 = UG1 ids;
// Carbon = Carbon ids; UG2 = UG2 ids (never used by the asi's own pick: UG2 = -2); Off = nothing
static int Set_InSoundtrack(int st, int id) { return st == SET_ST_ALL ? 1 : st == SET_ST_MW ? (id >= 27 && id < 53) : st == SET_ST_UG1 ? (id >= 53 && id < 79) : st == SET_ST_CARBON ? (id >= 79 && id < 109) : st == SET_ST_UG2 ? (id < 27) : 0; }
static int set_soundtrack = SET_ST_ALL;   // read by the allowed rule (songlogic.h); the asi sets it from the settings before every own pick; ALL (no filter) until then, as the rig's picker checks expect
// ---- (2) the settings file
#define SET_HEADER "UG2Music settings 1"
#define SET_KEY "soundtrack"
#define SET_MAXLINES 64
#define SET_LINELEN 120
typedef struct {
    int soundtrack;                  // the value in memory (always installed)
    int headerSeen, lineSeen;        // the header line / the soundtrack line were in the text
    int badValue;                    // the soundtrack line named an unknown or uninstalled value (-> the default)
    int nOther, dropped;             // the other lines kept verbatim; lines beyond 64 dropped (counted)
    char other[SET_MAXLINES][SET_LINELEN];
    signed char mwMode[27];          // 'MW nn = FE | IG | AL | OFF' -> 1 / 2 / 3 / 0 for MW song nn (1..26); -1 = no line = MW's DefPlay default (kMwMode)
    signed char u1Mode[27];          // 'UG1 nn = FE | IG | AL | OFF' for UG1 row nn - 1 (nn = 1..26); -1 = no line = UG1's own default (kU1Mode)
    signed char cbMode[31];          // 'CB nn = FE | IG | AL | OFF' for Carbon list position nn - 1 (nn = 1..30); -1 = no line = Racing (kCbMode)
    int nSong, badSong;              // the song lines parsed; the ones with a bad mode word (kept verbatim in other[] instead)
} Set_Settings;
static void Set_Defaults(Set_Settings *s) { memset(s, 0, sizeof *s); s->soundtrack = SET_DEFAULT; memset(s->mwMode, -1, sizeof s->mwMode); memset(s->u1Mode, -1, sizeof s->u1Mode); memset(s->cbMode, -1, sizeof s->cbMode); }
static int Set_IEq(const char *a, const char *b)   // case-insensitive equal
{
    for (;; a++, b++) { int ca = *a, cb = *b; if (ca >= 'a' && ca <= 'z') ca -= 32; if (cb >= 'a' && cb <= 'z') cb -= 32; if (ca != cb) return 0; if (!ca) return 1; }
}
static int Set_ParseValue(const char *v)            // -> the soundtrack named (0..5) or -1
{
    for (int i = 0; i < SET_NST; i++) if (Set_IEq(v, kSetName[i])) return i;
    return -1;
}
static void Set_Trim(char *s)                        // spaces / tabs at both ends removed in place
{
    char *b = s; while (*b == ' ' || *b == '\t') b++;
    size_t n = strlen(b); while (n && (b[n - 1] == ' ' || b[n - 1] == '\t')) n--;
    memmove(s, b, n); s[n] = 0;
}
// the per-song line 'MW nn = FE | IG | AL | OFF' (FE = Menu 1, IG = Racing 2, AL = All 3, OFF = Off 0 = UG2's own mode values; MW's DefPlay words)
static const char *const kSetModeWord[4] = { "OFF", "FE", "IG", "AL" };
static int Set_ParseMode(const char *v) { for (int m = 0; m < 4; m++) if (Set_IEq(v, kSetModeWord[m])) return m; return -1; }
static int Set_SongKey(const char *key)    // 'MW nn' (the letters case-insensitive, spaces optional, nn = 1..26 in 1-2 digits) -> nn, anything else 0
{
    if ((key[0] != 'M' && key[0] != 'm') || (key[1] != 'W' && key[1] != 'w')) return 0;
    const char *p = key + 2; while (*p == ' ' || *p == '\t') p++;
    if (*p < '0' || *p > '9') return 0;
    int v = 0, d = 0; while (*p >= '0' && *p <= '9' && d < 3) { v = v * 10 + (*p - '0'); p++; d++; }
    if (*p) return 0;
    return (v >= 1 && v <= 26) ? v : 0;
}
// 'UG1 nn' (the letters case-insensitive, spaces optional, nn = 1..26 in 1-2 digits) -> nn, anything else 0 (the same shape as the MW key; 'UG2 ..' / 'MW ..' -> 0)
static int Set_U1Key(const char *key)
{
    if ((key[0] != 'U' && key[0] != 'u') || (key[1] != 'G' && key[1] != 'g') || key[2] != '1') return 0;
    const char *p = key + 3; while (*p == ' ' || *p == '\t') p++;
    if (*p < '0' || *p > '9') return 0;
    int v = 0, d = 0; while (*p >= '0' && *p <= '9' && d < 3) { v = v * 10 + (*p - '0'); p++; d++; }
    if (*p) return 0;
    return (v >= 1 && v <= 26) ? v : 0;
}
// 'CB nn' (the letters case-insensitive, spaces optional, nn = 1..30 in 1-2 digits) -> nn, anything else 0 (the same shape; 'CB' alone / 'CB 0' / 'CB 31' -> 0)
static int Set_CbKey(const char *key)
{
    if ((key[0] != 'C' && key[0] != 'c') || (key[1] != 'B' && key[1] != 'b')) return 0;
    const char *p = key + 2; while (*p == ' ' || *p == '\t') p++;
    if (*p < '0' || *p > '9') return 0;
    int v = 0, d = 0; while (*p >= '0' && *p <= '9' && d < 3) { v = v * 10 + (*p - '0'); p++; d++; }
    if (*p) return 0;
    return (v >= 1 && v <= 30) ? v : 0;
}
// the text of the file (len bytes, any line ending) -> s. A 'soundtrack = X' line (the key case-insensitive, spaces optional) sets the value when X is a known and
// installed soundtrack; a line starting with 'UG2Music settings' is the header (not kept); an MW / UG1 / CB line with a good mode word fills its mode; every
// other non-empty line is kept verbatim (truncated to the line length - 1).
static void Set_Parse(Set_Settings *s, const char *text, int len)
{
    Set_Defaults(s);
    int i = 0;
    while (i < len) {
        char line[SET_LINELEN]; int n = 0;
        while (i < len && text[i] != '\n') { if (text[i] != '\r' && n < SET_LINELEN - 1) line[n++] = text[i]; i++; }
        i++; line[n] = 0; Set_Trim(line);
        if (!line[0]) continue;
        if (!strncmp(line, "UG2Music settings", 17)) { s->headerSeen = 1; continue; }
        char *eq = strchr(line, '=');
        if (eq) {
            char key[SET_LINELEN]; size_t kl = (size_t)(eq - line); memcpy(key, line, kl); key[kl] = 0; Set_Trim(key);
            if (Set_IEq(key, SET_KEY)) {
                char val[SET_LINELEN]; strcpy(val, eq + 1); Set_Trim(val);
                int v = Set_ParseValue(val); s->lineSeen = 1;
                if (v >= 0 && Set_Installed(v)) s->soundtrack = v; else { s->badValue = 1; s->soundtrack = SET_DEFAULT; }
                continue;
            }
            int sn = Set_SongKey(key);   // 'MW nn = FE | IG | AL | OFF'; a bad mode word -> counted, the line kept verbatim below
            if (sn) { char val[SET_LINELEN]; strcpy(val, eq + 1); Set_Trim(val); int m = Set_ParseMode(val); if (m >= 0) { s->mwMode[sn] = (signed char)m; s->nSong++; continue; } s->badSong++; }
            int un = sn ? 0 : Set_U1Key(key);   // 'UG1 nn = FE | IG | AL | OFF' the same way (nn = row + 1)
            if (un) { char val[SET_LINELEN]; strcpy(val, eq + 1); Set_Trim(val); int m = Set_ParseMode(val); if (m >= 0) { s->u1Mode[un] = (signed char)m; s->nSong++; continue; } s->badSong++; }
            int cn = (sn || un) ? 0 : Set_CbKey(key);   // 'CB nn = FE | IG | AL | OFF' the same way (nn = list position + 1)
            if (cn) { char val[SET_LINELEN]; strcpy(val, eq + 1); Set_Trim(val); int m = Set_ParseMode(val); if (m >= 0) { s->cbMode[cn] = (signed char)m; s->nSong++; continue; } s->badSong++; }
        }
        if (s->nOther < SET_MAXLINES) strcpy(s->other[s->nOther++], line); else s->dropped++;
    }
}
// s -> the file text (CRLF): the header, the soundtrack line, the MW / UG1 / CB lines, the kept lines. -> the length (0 = the buffer is too small)
static int Set_Format(const Set_Settings *s, char *out, int cap)
{
    int n = 0; const char *name = kSetName[Set_Installed(s->soundtrack) ? s->soundtrack : SET_DEFAULT];
    int need = (int)strlen(SET_HEADER) + 2 + (int)strlen(SET_KEY) + 3 + (int)strlen(name) + 2;
    for (int k = 0; k < s->nOther; k++) need += (int)strlen(s->other[k]) + 2;
    for (int k = 1; k <= 26; k++) if (s->mwMode[k] >= 0) need += 14;                               // 'MW nn = OFF' + CRLF = 13 at most
    for (int k = 1; k <= 26; k++) if (s->u1Mode[k] >= 0) need += 15;                               // 'UG1 nn = OFF' + CRLF = 14 at most
    for (int k = 1; k <= 30; k++) if (s->cbMode[k] >= 0) need += 14;                               // 'CB nn = OFF' + CRLF = 13 at most
    if (need + 1 > cap) return 0;
    n += sprintf(out + n, "%s\r\n%s = %s\r\n", SET_HEADER, SET_KEY, name);
    for (int k = 1; k <= 26; k++) if (s->mwMode[k] >= 0) n += sprintf(out + n, "MW %02d = %s\r\n", k, kSetModeWord[s->mwMode[k] & 3]);   // one line per MW song whose mode is known
    for (int k = 1; k <= 26; k++) if (s->u1Mode[k] >= 0) n += sprintf(out + n, "UG1 %02d = %s\r\n", k, kSetModeWord[s->u1Mode[k] & 3]);  // the UG1 lines after the MW lines
    for (int k = 1; k <= 30; k++) if (s->cbMode[k] >= 0) n += sprintf(out + n, "CB %02d = %s\r\n", k, kSetModeWord[s->cbMode[k] & 3]);   // the CB lines after the UG1 lines
    for (int k = 0; k < s->nOther; k++) n += sprintf(out + n, "%s\r\n", s->other[k]);
    return n;
}
// the profile name (up to 31 bytes, the first 0 ends it) -> the file name part: A-Z a-z 0-9 space - _ kept, anything else '_'. -> the length (0 = no profile yet)
static int Set_SafeName(const char *in, int inMax, char *out, int cap)
{
    int n = 0;
    for (int i = 0; i < inMax && i < 31 && n < cap - 1; i++) {
        unsigned char c = (unsigned char)in[i]; if (!c) break;
        int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '_';
        out[n++] = ok ? (char)c : '_';
    }
    out[n] = 0; return n;
}
// ---- (3) the code sites (UG2-EXE, retail speed2.exe)
#define UA_BAR_TRIG 0x005241E0u        // the top bar's trigger handler: thiscall(screen, hash) ret 4; one caller 56b324 (56b1df LTRIGGER / 56b31f RTRIGGER lead there)
#define UA_BAR_TRIG_TAIL 0x00524230u   // its tail: mov ecx,[0x82B884]; call 0x479220; pop esi; pop ebx; ret 4 = the stub jumps there after the step (esi / ebx pushed)
#define UA_BAR_LABEL 0x00524120u       // the label routine: thiscall(screen); callers 52422b (inside the trigger handler) + 565e1d (the mouse path)
#define UA_BAR_MOUSE1 0x00565DB1u      // 0x565D90's inline flip for the left arrow [edi+0x10C] (sound [0x8381AB]) .. 0x565DDE: 45 bytes, ends 'jmp 0x565E1B'
#define UA_BAR_MOUSE2 0x00565DF1u      // the inline flip for the right arrow [edi+0x110] (sound [0x8381C6]) .. 0x565E1B: 42 bytes, falls into 565e1b
#define UA_BAR_MOUSE_JOIN 0x00565E1Bu  // mov ecx,edi; call 0x524120 (the label); mov ecx,[0x82B884]; call 0x479220; ...
#define UA_BAR_CTOR 0x005540BFu        // the screen ctor's first label: [0x83AA58] -> the hash in ecx .. 0x5540DD, then the inline 0x50C900 copy .. 0x55412C: 30 bytes
#define UA_BAR_CTOR_AFTER 0x0055412Cu  // mov esi,[esp+0x10]; mov ecx,esi; call 0x54B310 (the fill) = where the patch continues
#define UA_BAR_PRINTF_OBJ 0x00537BE0u  // FEPrintf on an object: cdecl(obj, fmt, ...) -> 0x537B30 (the buffer 0x864218, 0x535FE0 sets the text, flags |= 2); 116 callers
#define UA_BAR_SETLABEL 0x0050C900u    // cdecl(obj, hash): +0x60 := hash (+0x1C |= 0x400000 on a change), the notifier [0x838478], bit 2 handled; 508 callers
#define UA_BAR_SOUND 0x00458520u       // thiscall(ecx = [0x82B884], sound id) ret 4 = the trigger sound, as 5241f5-524218 / 565dc3-565dca play it
#define UA_BAR_SND_LEFT 0x008381ABu    // byte, movsx; -1 = no sound
#define UA_BAR_SND_RIGHT 0x008381C6u
#define UA_BAR_SOUNDMGR 0x0082B884u
#define UA_BAR_PROFILE 0x0083A9E0u     // char[32] the profile name (44 readers in the exe)
#define UA_BAR_OBJ_OFF 0x104u          // [screen+0x104] = the top bar text object TRACKMODE_TYPE (the ctor 553cf1 push 0xCA74A2FA .. 553d29 mov [ebx+0x104],eax)
#define SET_HASH_LTRIGGER 0x5073EF13u
#define SET_HASH_RTRIGGER 0xD9FEEC59u
#define SET_HASH_ALL 0x9CCE64C4u      // UG2's label 'All' (the row mode cycle 0x53DEB0 53e028 / the fill 54baf3 use it)
#define SET_HASH_OFF 0x9CCE9F86u      // UG2's label 'Off' (53e013 / 54bade)
static const unsigned char kBarTrigHead[9] = { 0x8B, 0x44, 0x24, 0x04, 0x3D, 0x13, 0xEF, 0x73, 0x50 };                      // mov eax,[esp+4]; cmp eax,0x5073EF13
static const unsigned char kBarTrigTail[16] = { 0x8B, 0x0D, 0x84, 0xB8, 0x82, 0x00, 0xE8, 0xE5, 0x4F, 0xF5, 0xFF, 0x5E, 0x5B, 0xC2, 0x04, 0x00 };
static const unsigned char kBarLabelHead[12] = { 0x0F, 0xB6, 0x05, 0x58, 0xAA, 0x83, 0x00, 0x83, 0xE8, 0x00, 0x74, 0x2C };  // movzx eax,[0x83AA58]; sub eax,0; je
#define SET_MOUSE1_LEN 45
#define SET_MOUSE2_LEN 42
#define SET_CTOR_LEN 30
static const unsigned char kBarMouse1[SET_MOUSE1_LEN] = {
    0x0F, 0xBE, 0x05, 0xAB, 0x81, 0x83, 0x00, 0x83, 0xF8, 0xFF, 0x8A, 0x1D, 0x58, 0xAA, 0x83, 0x00, 0x74, 0x0C, 0x8B, 0x0D, 0x84, 0xB8, 0x82, 0x00, 0x50,
    0xE8, 0x51, 0x27, 0xEF, 0xFF, 0x33, 0xC9, 0x84, 0xDB, 0x0F, 0x94, 0xC1, 0x89, 0x0D, 0x58, 0xAA, 0x83, 0x00, 0xEB, 0x3D };
static const unsigned char kBarMouse2[SET_MOUSE2_LEN] = {
    0x0F, 0xBE, 0x05, 0xC6, 0x81, 0x83, 0x00, 0x83, 0xF8, 0xFF, 0x8A, 0x1D, 0x58, 0xAA, 0x83, 0x00, 0x74, 0x0C, 0x8B, 0x0D, 0x84, 0xB8, 0x82, 0x00, 0x50,
    0xE8, 0x11, 0x27, 0xEF, 0xFF, 0x33, 0xC0, 0x84, 0xDB, 0x0F, 0x94, 0xC0, 0xA3, 0x58, 0xAA, 0x83, 0x00 };
static const unsigned char kBarMouseJoin[11] = { 0x8B, 0xCF, 0xE8, 0xFE, 0xE2, 0xFB, 0xFF, 0x8B, 0x0D, 0x84, 0xB8 };          // mov ecx,edi; call 0x524120; mov ecx,[0x82B884]
static const unsigned char kBarCtor[SET_CTOR_LEN] = {
    0x0F, 0xB6, 0x05, 0x58, 0xAA, 0x83, 0x00, 0x2B, 0xC5, 0x74, 0x0E, 0x48, 0x74, 0x04, 0x33, 0xC9, 0xEB, 0x0C, 0xB9, 0xF7, 0xC8, 0xC9, 0xA9, 0xEB, 0x05,
    0xB9, 0x89, 0x6B, 0xA3, 0x4C };
static const unsigned char kBarCtorAfter[11] = { 0x8B, 0x74, 0x24, 0x10, 0x8B, 0xCE, 0xE8, 0xD9, 0x71, 0xFF, 0xFF };          // mov esi,[esp+0x10]; mov ecx,esi; call 0x54B310
static const unsigned char kBarPrintfHead[12] = { 0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x74, 0x14, 0x8B, 0x54, 0x24, 0x08 };
static const unsigned char kBarSetLabelHead[12] = { 0x56, 0x8B, 0x74, 0x24, 0x08, 0x85, 0xF6, 0x74, 0x47, 0x8B, 0x46, 0x1C };
static const unsigned char kBarSoundHead[9] = { 0xA1, 0xA8, 0xAA, 0x7F, 0x00, 0x85, 0xC0, 0x74, 0x31 };
// the patch bytes over an inline block: kind 0 = the mouse left block, 1 = the mouse right block, 2 = the ctor block. target = the asi function the block calls
// (cdecl; the mouse blocks push the direction then the screen (edi), the ctor block pushes the screen from [esp+0x10]); after the call the block jumps to the join.
// -> the length written (= the block's whole length: the rest is nop), 0 = a bad kind.
static int Set_MakePatch(int kind, uint32_t site, uint32_t target, unsigned char *out)
{
    int len = kind == 0 ? SET_MOUSE1_LEN : kind == 1 ? SET_MOUSE2_LEN : kind == 2 ? SET_CTOR_LEN : 0, n = 0;
    uint32_t join = kind == 2 ? UA_BAR_CTOR_AFTER : UA_BAR_MOUSE_JOIN;
    if (!len) return 0;
    memset(out, 0x90, len);
    if (kind == 2) { out[n++] = 0x8B; out[n++] = 0x54; out[n++] = 0x24; out[n++] = 0x10; out[n++] = 0x52; }       // mov edx,[esp+0x10]; push edx (the screen)
    else { out[n++] = 0x6A; out[n++] = kind == 0 ? 0xFF : 0x01; out[n++] = 0x57; }                                  // push -1 / +1; push edi (the screen)
    out[n++] = 0xE8; uint32_t rel = target - (site + n + 4); memcpy(out + n, &rel, 4); n += 4;                       // call target
    out[n++] = 0x83; out[n++] = 0xC4; out[n++] = kind == 2 ? 0x04 : 0x08;                                           // add esp, 4 / 8
    { uint32_t next = site + n + 2; out[n++] = 0xEB; out[n++] = (unsigned char)(join - next); }                       // jmp rel8 -> the join (rel = join - the address after the jmp)
    return len;
}
