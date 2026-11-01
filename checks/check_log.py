#!/usr/bin/env python3
r"""check_log.py - the log checker of the mod: reads one or more play sessions' UG2Music.log and checks what the mod logged against its own
sources (src/songnames.h: the song names; src/songlogic.h: the default modes), the settings file before and after the session and the
profile save. Every log line is 'HH:MM:SS <sentence>'; a session starts at the line 'Golden Era Soundtrax ...'.
WHAT IT CHECKS
  PROBLEMS first: every line with NOT or FAILED, with its time, then 'PROBLEMS: n'.
  THE HEAD (per session): the start lines present and in order (the version, where the mod loaded from, the game's exe size, the song archive's
  size, the song table, the song names, the random draw, the 9 parts, 'Ready'), every part installed, 'Ready: 9 of 9', the retail exe size, the
  mod's archive, 27 of 27 songs, '27 titles, 3 albums and 1 artist line set'.
  THE SONG SETTINGS: a tracker of the five song sets (Underground 2's list, Most Wanted, Underground, Carbon, All Trax) with their file lines,
  seeded at each profile read (from the settings file before the session when the read's byte count = its size, else from the last save's
  state) and moved by every song setting step and Default reset. Checked at each 'Now playing' line (the song's mode in the bar's set allows the
  context), at each save (the byte count and the songs-set count = the tracker's file), and after the session: the settings file on disk = the
  last save's text; UG2's own 27 modes in the profile save = the tracker's Underground 2 set.
  THE BAR: each soundtrack step from the current value; each EA Trax open names the bar's soundtrack and its row count; each 'Now playing' names it.
  THE DEFAULT DIALOG: every question has one answer; no step, setting or preview between them.
  THE SONG NAMES: every song named in a 'Now playing' / 'Song setting' / 'Preview' line = '<game> <n> <title>' with the title of the sources.
  THE SAVE PROMPT: 'Leaving EA Trax: the song settings changed' comes only after a soundtrack step, a song setting step or a Default reset since
  the last profile read or save.
usage: python3 -I -B checks/check_log.py UG2Music.log src/songnames.h src/songlogic.h settings_before.txt settings_after.txt profile_save
  ('-' for a file that is not available: its checks are skipped). The last line: 'CHECK PASS (0 failures)' or 'CHECK FAIL (<n>)'."""
import sys, re
log, strs, logic, fbefore, fafter, fsave = sys.argv[1:7]
fails = []
def fail(ln, what): fails.append((ln, what)); print('  FAIL line %d: %s' % (ln, what))
# ---- the sources: the song names as shown, the defaults
src = open(strs, encoding='latin1').read(); lg = open(logic, encoding='latin1').read()
def arr(name, rows, cols):
    body = src.split('static const char *%s[' % name)[1].split('};')[0]
    s = [x.replace('\\"', '"') for x in re.findall(r'"((?:[^"\\]|\\.)*)"', body)]
    return [s[i * cols] for i in range(rows)]
def tab(name): return [int(x) for x in re.search(r'static const unsigned char %s\[\d+\] = \{([^}]*)\}' % name, lg)[1].split(',')]
SHOW = {'Underground 2': arr('kUg2Show', 27, 1), 'Most Wanted': arr('kMw', 27, 3)[1:], 'Underground': arr('kU1', 26, 3), 'Carbon': arr('kCb', 30, 3)}   # 1-based n -> [n - 1]
kMw, kU1, kCb = tab('kMwMode'), tab('kU1Mode'), tab('kCbMode')
SETS = ['Underground 2', 'Underground', 'Most Wanted', 'Carbon', 'All Trax']                    # the five song sets = the bar's words
N = {'Underground 2': 27, 'Underground': 26, 'Most Wanted': 26, 'Carbon': 30, 'All Trax': 109}   # Most Wanted's set has 26 (25 / 26 never shown)
ROWS = {'Underground 2': 27, 'Underground': 26, 'Most Wanted': 24, 'Carbon': 30, 'All Trax': 107}
DEF = {'Underground 2': [1 if s < 6 else 2 for s in range(27)], 'Most Wanted': kMw[1:27], 'Underground': kU1[1:27], 'Carbon': kCb[1:31]}
DEF['All Trax'] = DEF['Underground 2'] + DEF['Most Wanted'] + DEF['Underground'] + DEF['Carbon']   # by the id 0..108
assert len(DEF['All Trax']) == 109
MODE = {'Off': 0, 'Menu': 1, 'Racing': 2, 'All': 3}; MODENAME = {v: k for k, v in MODE.items()}; WORD = {0: 'OFF', 1: 'FE', 2: 'IG', 3: 'AL'}; VAL = {v: k for k, v in WORD.items()}
FILEKEY = {'Most Wanted': 'MW', 'Underground': 'UG1', 'Carbon': 'CB', 'Underground 2': 'UG2'}; KEYSET = {v: k for k, v in FILEKEY.items()}
STWORD = {'Underground 2': 'UG2', 'Underground': 'UG1', 'Most Wanted': 'MW', 'Carbon': 'CARBON', 'All Trax': 'ALL'}; WORDST = {v: k for k, v in STWORD.items()}
def songid(game, n):   # the id 0..108 of song n (1-based) of a game
    return {'Underground 2': n - 1, 'Most Wanted': 26 + n, 'Underground': 52 + n, 'Carbon': 78 + n}[game]
def idx(setname, game, n):   # the index into a set's array for song n of game (All Trax: the id; a game's own set: n - 1)
    return songid(game, n) if setname == 'All Trax' else n - 1
# ---- the tracker: live modes per set + the file lines (None = no line = the default)
class Tracker:
    def __init__(self):
        self.mode = {s: list(DEF[s]) for s in SETS}; self.line = {s: [None] * N[s] for s in SETS}; self.other = []; self.soundtrack = 'Underground 2'; self.ug2set = [False] * 27
    def seed_file(self, parsed):   # the parsed settings file: (lines per set, soundtrack, other lines)
        lines, st, other = parsed
        for s in ('Most Wanted', 'Underground', 'Carbon', 'All Trax'):
            self.line[s] = list(lines[s]); self.mode[s] = [lines[s][i] if lines[s][i] is not None else DEF[s][i] for i in range(N[s])]
        self.other = list(other); self.soundtrack = st if st else 'Underground 2'
    def seed_defaults(self):
        for s in ('Most Wanted', 'Underground', 'Carbon', 'All Trax'): self.line[s] = [None] * N[s]; self.mode[s] = list(DEF[s])
        self.other = []; self.soundtrack = 'Underground 2'
    def step(self, setname, i, to):
        self.mode[setname][i] = to
        if setname == 'Underground 2': self.ug2set[i] = True
        else: self.line[setname][i] = to
    def reset(self, setname):
        changed = sum(1 for i in range(N[setname]) if self.mode[setname][i] != DEF[setname][i])
        self.mode[setname] = list(DEF[setname])
        if setname == 'Underground 2': self.ug2set = [True] * 27
        else: self.line[setname] = [None] * N[setname]
        return changed
    def lines_count(self): return sum(1 for s in ('Most Wanted', 'Underground', 'Carbon', 'All Trax') for v in self.line[s] if v is not None)
    def text(self):   # the file text the mod writes for this state (settingsfile.h's order)
        t = 'UG2Music settings 2\r\nsoundtrack = %s\r\n' % STWORD[self.soundtrack]
        for s in ('Most Wanted', 'Underground', 'Carbon'):
            for i, v in enumerate(self.line[s]):
                if v is not None: t += '%s %02d = %s\r\n' % (FILEKEY[s], i + 1, WORD[v])
        for id_, v in enumerate(self.line['All Trax']):
            if v is not None:
                g = 'Underground 2' if id_ < 27 else 'Most Wanted' if id_ < 53 else 'Underground' if id_ < 79 else 'Carbon'
                nn = id_ + 1 if id_ < 27 else id_ - 26 if id_ < 53 else id_ - 52 if id_ < 79 else id_ - 78
                t += 'ALL %s %02d = %s\r\n' % (FILEKEY[g], nn, WORD[v])
        for o in self.other: t += o + '\r\n'
        return t
def parse_file(text):   # -> (lines per set, the soundtrack as a set name, other lines)
    lines = {s: [None] * N[s] for s in ('Most Wanted', 'Underground', 'Carbon', 'All Trax')}; st = None; other = []
    for x in text.splitlines():
        x = x.strip()
        if not x: continue
        m = re.match(r'(MW|UG1|CB) (\d\d?) = (OFF|FE|IG|AL)$', x, re.I); a = re.match(r'ALL (UG2|MW|UG1|CB) (\d\d?) = (OFF|FE|IG|AL)$', x, re.I)
        if x.lower().startswith('ug2music settings'): continue
        elif re.match(r'soundtrack\s*=', x, re.I): st = WORDST.get(x.split('=', 1)[1].strip().upper(), 'Underground 2')
        elif a: lines['All Trax'][songid(KEYSET[a[1].upper()], int(a[2]))] = VAL[a[3].upper()]
        elif m: lines[KEYSET[m[1].upper()]][int(m[2]) - 1] = VAL[m[3].upper()]
        else: other.append(x)
    return lines, st, other
BEFORE = open(fbefore, 'rb').read() if fbefore != '-' else None
# ---- the lines
RX = re.compile(r'^(\d\d):(\d\d):(\d\d) (.*)$')
raw = open(log, encoding='latin1').read().splitlines()
lines = []
for no, x in enumerate(raw, 1):
    m = RX.match(x)
    if not m: fail(no, 'not a log line (no time stamp): %r' % x[:80]); continue
    lines.append((no, m[4]))
print('%s: %d lines' % (log, len(lines)))
print('PROBLEMS:')
probs = [(no, t) for no, t in lines if re.search(r'\b(NOT|FAILED)\b', t)]
for no, t in probs: print('  line %d %s %s' % (no, raw[no - 1][:8], t))
print('PROBLEMS: %d' % len(probs))
sessions = []
for no, t in lines:
    if t.startswith('Golden Era Soundtrax '): sessions.append([])
    if not sessions: fail(no, 'a line before the first session start'); continue
    sessions[-1].append((no, t))
print('sessions: %d' % len(sessions))
HEAD = [('the version', r'^Golden Era Soundtrax [0-9.]+ \(UG2Music\.asi\) started on \d{4}-\d\d-\d\d\.$'), ('loaded from', r'^Loaded from .+\.$'), ('the game', r'^The game'),
        ('the song archive', r'^The song archive'), ('the song table', r"^UG2's song table: \d+ of 27 songs as expected\.$"), ('the song names', r'^Song names: '), ('the random draw', r'^Random draw: '),
        ('the song picker', r'^Song picker'), ('the long-song fix', r'^Long-song fix'), ('the settings folder', r'^Settings folder'), ('the top bar', r'^EA Trax top bar'), ('the row text shift', r'^Row text shift'),
        ('the list', r'^EA Trax list'), ('the save sync', r'^Saving with the profile'), ('the save prompt', r'^Save prompt'), ('the Shuffle button', r'^Shuffle button'), ('the Default button', r'^Default button'),
        ('ready', r'^Ready: ')]
PARTS = ['Song picker: installed.', 'Long-song fix: installed.', 'EA Trax top bar: installed.', 'Row text shift: installed.', 'EA Trax list: installed.', 'Saving with the profile: installed.',
         'Save prompt: installed.', 'Shuffle button: installed.', 'Default button: installed.']
SONG = r'(Underground 2|Underground|Most Wanted|Carbon) (\d+) (.+?)'
def check_song(no, game, n, title):
    n = int(n); names = SHOW[game]
    if n < 1 or n > len(names): fail(no, 'song %s %d: no such song' % (game, n)); return None
    if names[n - 1] != title: fail(no, "song %s %d: the title reads '%s', the source says '%s'" % (game, n, title, names[n - 1]))
    return n
last_save_text = None; last_tracker = None; last_seeded = False
for si, S in enumerate(sessions, 1):
    print('SESSION %d (lines %d-%d, %d lines)' % (si, S[0][0], S[-1][0], len(S)))
    pos = 0
    for what, rx in HEAD:
        k = next((j for j in range(pos, len(S)) if re.search(rx, S[j][1])), None)
        if k is None: fail(S[0][0], 'the head: %s line missing (or out of order)' % what); continue
        pos = k + 1
    texts = [t for _, t in S]
    for p in PARTS:
        if p not in texts: fail(S[0][0], "the head: '%s' missing" % p)
    for no, t in S:
        if t.startswith('Ready: ') and t != 'Ready: 9 of 9 parts installed.': fail(no, 'not every part installed: %s' % t)
        if t.startswith('The game:') and '(the retail v1.2 size)' not in t: fail(no, 'the exe is not the retail v1.2 size: %s' % t)
        if t.startswith('The song archive') and "(the mod's archive)" not in t: fail(no, "the archive is not the mod's: %s" % t)
        if t.startswith("UG2's song table:") and 'as expected' in t and not t.endswith('27 of 27 songs as expected.'): fail(no, t)
        if t.startswith('Song names:') and t != 'Song names: 27 titles, 3 albums and 1 artist line set.': fail(no, t)
    T = Tracker(); seeded = False; dialog = None; dialog_set = None; changed_since = False; first_read = True; saves = 0
    for no, t in S:
        m = re.match(r"^Profile '(.*)': song settings read from UG2Music\\(.*)\.txt \(([\d,]+) bytes, (\d+) songs set, soundtrack (.+)\)\.$", t)
        if m:
            size = int(m[3].replace(',', '')); n_set = int(m[4]); st = m[5]
            if first_read and BEFORE is not None and len(BEFORE) == size: T.seed_file(parse_file(BEFORE.decode('latin1'))); src_ = 'the settings file before the session (%d bytes)' % size
            elif last_save_text is not None and len(last_save_text) == size: T.seed_file(parse_file(last_save_text)); src_ = 'the last save'
            else: fail(no, 'the settings read (%d bytes) matches neither the file before the session nor the last save: the tracker is seeded from the line only' % size); src_ = 'the line only'; T.seed_defaults()
            if src_ != 'the line only':
                if T.lines_count() != n_set: fail(no, 'songs set %d, the file has %d lines' % (n_set, T.lines_count()))
                if T.soundtrack != st: fail(no, 'soundtrack %s, the file says %s' % (st, T.soundtrack))
            T.soundtrack = st; seeded = True; first_read = False; changed_since = False
            print('  profile read at line %d: seeded from %s; soundtrack %s, %d lines' % (no, src_, st, T.lines_count())); continue
        m = re.match(r"^Profile '(.*)': no settings file yet - the defaults \(soundtrack Underground 2\)", t)
        if m: T.seed_defaults(); seeded = True; changed_since = False; print('  profile read at line %d: no file, the defaults' % no); continue   # the before-file may still belong to a later profile's first read
        m = re.match(r'^Soundtrack: (.+) -> (.+)\.$', t)
        if m:
            if m[1] != T.soundtrack: fail(no, 'the bar stepped from %s, the tracker has %s' % (m[1], T.soundtrack))
            T.soundtrack = m[2]; changed_since = True
            if dialog: fail(no, 'a soundtrack step while a Default question is open (asked at line %d)' % dialog)
            continue
        m = re.match(r'^EA Trax opened: (.+), (\d+) songs\.$', t)
        if m:
            if m[1] != T.soundtrack: fail(no, 'EA Trax opened on %s, the bar is %s' % (m[1], T.soundtrack))
            if m[1] in ROWS and int(m[2]) != ROWS[m[1]]: fail(no, '%s has %d rows, the log says %s' % (m[1], ROWS[m[1]], m[2]))
            continue
        m = re.match(r'^Song setting: ' + SONG + r', (Off|Menu|Racing|All) -> (Off|Menu|Racing|All) \(in (.+)\)\.$', t)
        if m:
            n = check_song(no, m[1], m[2], m[3]); setname = m[6]
            if dialog: fail(no, 'a song setting step while a Default question is open (asked at line %d)' % dialog)
            if setname not in SETS: fail(no, 'unknown set %s' % setname); continue
            if setname != 'All Trax' and setname != m[1]: fail(no, 'a %s song stepped in the %s set' % (m[1], setname)); continue
            if n is None: continue
            i = idx(setname, m[1], n)
            if seeded and T.mode[setname][i] != MODE[m[4]]: fail(no, 'the step starts from %s, the tracker has %s' % (m[4], MODENAME[T.mode[setname][i]]))
            T.step(setname, i, MODE[m[5]]); changed_since = True; continue
        m = re.match(r'^Default: asked to reset the (.+) songs\.$', t)
        if m:
            if dialog: fail(no, 'a second Default question while one is open (line %d)' % dialog)
            dialog = no; dialog_set = m[1]; continue
        m = re.match(r'^Default: Yes - the (.+) songs are back to their defaults \((\d+) changed\)\.$', t)
        if m:
            if not dialog: fail(no, 'a Yes without a question')
            elif m[1] != dialog_set: fail(no, 'the reset names %s, the question %s' % (m[1], dialog_set))
            dialog = None
            if m[1] in SETS:
                exp = T.reset(m[1]); changed_since = True
                if seeded and exp != int(m[2]): fail(no, '%s changed, the tracker expected %d' % (m[2], exp))
            continue
        if t == 'Default: No - nothing changed.':
            if not dialog: fail(no, 'a No without a question')
            dialog = None; continue
        m = re.match(r'^Preview: ' + SONG + r'\.$', t) or re.match(r'^Preview of ' + SONG + r' FAILED: ', t)
        if m:
            check_song(no, m[1], m[2], m[3])
            if dialog: fail(no, 'a preview while a Default question is open (asked at line %d)' % dialog)
            continue
        m = re.match(r'^Now playing \((menu|race)\): ' + SONG + r' - (.+), shuffle (on|off)\.$', t)
        if m:
            n = check_song(no, m[2], m[3], m[4]); cat = m[5]
            if cat != T.soundtrack: fail(no, 'the pick names %s, the bar is %s' % (cat, T.soundtrack))
            if cat not in SETS or n is None: continue
            if cat != 'All Trax' and cat != m[2]: fail(no, 'a %s song picked under %s' % (m[2], cat)); continue
            md = T.mode[cat][idx(cat, m[2], n)]
            if seeded and not (md in (1, 3) if m[1] == 'menu' else md in (2, 3)): fail(no, 'the song is %s in the %s set, not allowed for the %s' % (MODENAME[md], cat, m[1]))
            continue
        m = re.match(r"^Now playing \(menu\): Underground 2 1 (.+) - UG2's first song \((.+)\)\.$", t)
        if m:
            check_song(no, 'Underground 2', 1, m[1])
            if m[2] != T.soundtrack: fail(no, 'the first song names %s, the bar is %s' % (m[2], T.soundtrack))
            continue
        m = re.match(r"^The game saved profile '(.*)': song settings written to UG2Music\\(.*)\.txt \(([\d,]+) bytes, (\d+) songs set, soundtrack (.+)\)\.$", t)
        if m:
            size = int(m[3].replace(',', '')); n_set = int(m[4]); st = m[5]; saves += 1
            if st != T.soundtrack: fail(no, 'the save names %s, the bar is %s' % (st, T.soundtrack))
            if seeded:
                if n_set != T.lines_count(): fail(no, 'the save says %d songs set, the tracker has %d lines' % (n_set, T.lines_count()))
                exp = T.text()
                if size != len(exp): fail(no, "the save says %d bytes, the tracker's file would be %d" % (size, len(exp)))
                last_save_text = exp
            changed_since = False; continue
        if t == 'Leaving EA Trax: the song settings changed, so the game asks to save.':
            if not changed_since: fail(no, 'the save prompt without a soundtrack step, a song setting step or a reset since the last read / save')
            continue
    if dialog: fail(dialog, 'a Default question without an answer')
    print('  saves %d; the bar ends on %s; %d file lines in the tracker' % (saves, T.soundtrack, T.lines_count()))
    last_tracker = T; last_seeded = seeded
# ---- after the session: the files on disk
if fafter != '-':
    after = open(fafter, 'rb').read().decode('latin1')
    if last_save_text is None: print('settings file after: no save in the log, not compared')
    elif after != last_save_text: fail(0, "the settings file after the session differs from the last save's text")
    else: print("settings file after the session = the last save's text (%d bytes)" % len(after))
if fsave != '-':
    bb = open(fsave, 'rb').read()
    if len(bb) > 0xC447 + 8 * 27:
        sv = ''.join(str(bb[0xC447 + 8 * i + 4]) for i in range(27)); so = [int.from_bytes(bb[0xC447 + 8 * i:0xC447 + 8 * i + 4], 'little') for i in range(27)]
        if so != list(range(27)): fail(0, 'the profile save: songs 0..26 not in order at 0xC447')
        if not last_seeded: print('profile save: UG2 modes %s (no seeded session to compare)' % sv)
        else:
            tr = last_tracker; same = all(sv[i] == str(tr.mode['Underground 2'][i]) for i in range(27) if tr.ug2set[i])
            print('profile save: UG2 modes %s | the tracker %s (compared where a step or reset set them: %d songs) -> %s' % (sv, ''.join(str(v) for v in tr.mode['Underground 2']), sum(tr.ug2set), 'EQUAL' if same else 'DIFFERENT'))
            if not same: fail(0, "the profile save's UG2 modes differ from the tracker where a step or reset set them")
    else: fail(0, 'the profile save is too small to hold the song modes')
print('CHECK PASS (0 failures)' if not fails else 'CHECK FAIL (%d)' % len(fails))
sys.exit(0 if not fails else 1)
