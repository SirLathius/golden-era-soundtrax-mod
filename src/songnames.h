// songnames.h - the song strings the EA Trax rows and the song pop-up show (pure data, shared by the asi and the rig). Static strings inside the asi; the rows'
// +0xC pool pointer is never touched.
// The rules the shown strings follow:
//  - a title has no quotes around it, and '(<remixer> Remix)' is shown as '(Remix)' (a title whose own words name the version keeps them: Carbon's 'One Of Dem
//    Days (Remix)', MW's 'You Must Follow (Evol Intent VIP)');
//  - a line naming more than one act joins them with '/' instead of feat / and / & / +; one act's own name stays as it is ('Lil Jon & the Eastside Boyz',
//    'Grandmaster Flash And The Furious Five');
//  - an album the game leaves blank gets one where the mod has it (the game exclusives read 'NFS <game> Exclusive').
// Widths at the rows' 0.65: the widest title shown = Carbon's 'People Always Talk About The Weather (Remix)' 317.9 = 15.3 before the row arrow (rowwords.h).
// MW 23's artist 'Evol Intent/Mayhem/Thinktank' carries 8 trailing spaces (36 chars): the pop-up picks its LONG bar from the longest line's strlen (UG2-EXE
// 0x4AC7D0); the MID bar would cut that artist 12.0 short.
// kUg2Titles = UG2's own 27 titles as the rows 0x7ECD80 hold them in the stock exe (the load-time check; also the log's name for a UG2 song).
static const char *kUg2Titles[27] = { "\"Riders on the Storm (Fredwreck Remix)\"", "\"I Need Speed\"", "\"I Do\"", "\"That'z My Name\"", "\"LAX\"", "\"Lean Back\"",
    "\"Switch/Twitch\"", "\"Rush Hour\"", "\"Rocket Ride (Soulwax Remix)\"", "\"Hard EBM\"", "\"Mind Killer (Jagz Kooner Remix)\"", "\"Nothing But You (Cirrus Remix)\"",
    "\"E-Ville\"", "\"The Death & Resurrection Show\"", "\"Give it All\"", "\"Scavenger\"", "\"Notice of Eviction\"", "\"No W\"", "\"In My Head\"", "\"Determined\"",
    "\"I Am Weightless\"", "\"Crashing Foreign Cars\"", "\"Back on a Mission\"", "\"Black Betty\"", "\"Nobody\"", "\"Skeptic\"", "\"The Celebration Song\"" };
// UG2's 27 titles AS SHOWN = the stock text without its two quotes, rows 0 / 8 / 10 / 11 '(<remixer> Remix)' -> '(Remix)' (Riders on the Storm / Rocket Ride /
// Mind Killer / Nothing But You). ug2music.c WriteTitlesUg2 points every row's +0 at these ONCE at load, after SaveRows checked the stock text above; the saved
// rows take the new pointers too (the carriers 25 / 26 give them back after a borrowed song). The widest UG2 title shown = row 13 'The Death & Resurrection
// Show' 216.5 at the rows' 0.65.
static const char *kUg2Show[27] = { "Riders on the Storm (Remix)", "I Need Speed", "I Do", "That'z My Name", "LAX", "Lean Back",
    "Switch/Twitch", "Rush Hour", "Rocket Ride (Remix)", "Hard EBM", "Mind Killer (Remix)", "Nothing But You (Remix)",
    "E-Ville", "The Death & Resurrection Show", "Give it All", "Scavenger", "Notice of Eviction", "No W", "In My Head", "Determined",
    "I Am Weightless", "Crashing Foreign Cars", "Back on a Mission", "Black Betty", "Nobody", "Skeptic", "The Celebration Song" };
// MW songs 1..26 (MW's attributes.bin 'music' rows): title / artist / album by the rules above; two trailing spaces dropped; MW 25 / 26 (never shown) keep empty albums.
static const char *kMw[27][3] = { { "", "", "" },
    { "Fired Up", "Hush", "Bulletproof" }, { "Let's Move", "The Perceptionists", "Black Dialogue" }, { "B-Side Wins Again/Chuck D", "DJ Spooky/Dave Lombardo", "Drums Of Death" },
    { "Hand Of Blood", "Bullet For My Valentine", "Hand Of Blood" }, { "Tilted", "Lupe Fiasco", "Food &..." }, { "You'll Be Under My Wheels", "The Prodigy", "Always Outnumbered, Never Outgunned" },
    { "One Good Reason", "Celldweller", "Celldweller" }, { "I Am Rock", "Rock", "NFS Most Wanted Exclusive" }, { "Blinded In Chains", "Avenged Sevenfold", "City Of Evil" },
    { "We Control", "Hyper", "We Control" }, { "Skinnyman", "Static-X", "Start A War" }, { "Decadence", "Disturbed", "Ten Thousand Fists" },
    { "In A Hood Near You", "Suni Clay", "Suni Clay" }, { "Shapeshifter", "Celldweller/Styles Of Beyond", "Celldweller" }, { "Nine Thou (Remix)", "Styles Of Beyond", "Megadef" },
    { "Sets Go Up", "Juvenile", "Reality Check" }, { "Do Ya Thang", "T.I. Presents The P$$C", "25 To Life" }, { "Blood And Thunder", "Mastodon", "Leviathan" },
    { "Feels Just Like It Should (Remix)", "Jamiroquai", "Dynamite" }, { "Tao Of The Machine (Remix)", "The Roots/BT", "Blade II Soundtrack" },
    { "Feed The Addiction", "Ils", "Bohemia" }, { "Barrier Break", "Diesel Boy/Kaos", "Barrier Break / Submission" }, { "Broken Sword", "Evol Intent/Mayhem/Thinktank        ", "The HUMAN Resource" },
    { "You Must Follow (Evol Intent VIP)", "Stratus", "Dungeonmaster's Guide" }, { "Most Wanted Mash Up", "Paul Linford and Chris Vrenna", "" }, { "The Mann", "Paul Linford and Chris Vrenna", "" } };
// UG2's own row 19 'Determined' (Mudvayne) has an empty album in the stock exe (0x7ECEB8 -> 00783FE5 = ""); the mod points that row's +8 at this string
// once at load (ug2music.c WriteAlbum19; the rule is in songlogic.h).
static const char kUg2Album19[] = "Lost and Found";
// UG2's own row 0 'Riders on the Storm' artist 'Snoop Dogg feat. The Doors' (00789C04; French 'Snoop Dogg avec The Doors' 007896EC) -> the '/' rule.
// UG2's language fix-up 0x459D90 re-writes that pointer after the asi loaded, so ug2music.c WriteArtist0 writes it at load and compare-first at every pick.
static const char kUg2Artist0[] = "Snoop Dogg/The Doors";
// UG2's own rows 0 'Riders on the Storm' / 1 'I Need Speed' album = UG2-EXE 0x00789BDC 'Need for Speed Underground 2 Exclusive' in .data, re-written by UG2's
// language fix-up 0x459D90 after the asi loaded (459db9 / 459dbe: [0x7ECF3C] 0x00789694 English / [0x7ECF34] 0x007896C0 French) -> ug2music.c WriteAlbumsUg2
// points both rows' +8 at this string at load AND compare-first at every pick and every screen open (as WriteArtist0; the rule is in songlogic.h). Named like
// Carbon's own 'NFS Carbon Exclusive'; the MW / UG1 exclusives in the tables are the mod's own table strings (no game memory).
static const char kUg2AlbumNfs2[] = "NFS Underground 2 Exclusive";
// UG1's 26 rows = UG1-EXE (UG1's retail exe 8234b86f.., .data table 0x6F4D08: 26 x {title*, artist*, album*, mode*}, the strings' bytes as the exe holds them),
// by the rules above ('Kimosabe' / 'Body Rock' get the album UG1's own table leaves ""; no other UG1 title / artist line joins two acts - the rig checks it).
// Index r = UG1's row 0..25 = id 53 + r; the fill reads these three pointers for the rows 53..78 of g_vtab; a carrier row gets them at a UG1 pick / preview.
static const char *kU1[26][3] = {
    { "Doomsday", "Overseer", "Wreckage" },   // row 0
    { "Born Too Slow", "The Crystal Method", "Legion of Boom" },
    { "Out of Control", "Rancid", "Indestructible" },
    { "Two-Lane Blacktop", "Rob Zombie", "Past, Present & Future" },
    { "Kimosabe", "BT", "Emotional Technology" },
    { "The Only", "Static-X", "Shadow Zone" },   // row 5
    { "Broken Promises", "Element Eighty", "Element Eighty" },
    { "Fortress Europe", "Asian Dub Foundation", "Enemy of the Enemy" },
    { "Invisible", "Hotwire", "The Routine" },
    { "And the Hero Will Drown", "Story of the Year", "Page Avenue" },
    { "The Wonders of You", "Andy Hunter", "Exodus" },   // row 10
    { "Action Radius", "Junkie XL", "Big Sounds of the Drags" },
    { "Quarter", "Fuel", "Natural Selection" },
    { "Sucked In", "Jerk", "When Pure is Defiled" },
    { "Snapshot", "Fluke", "Puppy" },
    { "Ride", "lostprophets", "Start Something" },   // row 15
    { "Supermoves", "Overseer", "Wreckage" },
    { "Glitterball", "FC Kahuna", "Machine Says Yes" },
    { "Swallow", "Blindside", "About a Burning Fire" },
    { "Get Low", "Lil Jon & the Eastside Boyz", "Kings of Crunk" },
    { "Smashing the Gas (Get Faster)", "Mystikal", "NFS Underground Exclusive" },   // row 20
    { "Who's Who", "Dilated Peoples", "Neighborhood Watch" },
    { "Keep It Coming", "Nate Dogg", "NFS Underground Exclusive" },
    { "Body Rock", "X-ecutioners", "NFS Underground Exclusive" },
    { "Need For Speed", "Petey Pablo", "NFS Underground Exclusive" },
    { "24's", "T.I.", "Trap Muzik" },   // row 25
};
// Carbon's 30 songs = Carbon's vault (Carbon's GLOBAL\attributes.bin d6423466.., the class 'music' rows; the strings' bytes as the vault holds them) in the list
// order CarClass 1 (11) -> 2 (9) -> 3 (10), each in vault order (vault rows 1 3 5 8 11 12 15 17 18 19 29 | 2 4 6 7 16 23 26 27 28 | 9 10 13 14 20 21 22 24 25 30),
// by the rules above (c 0's artist 'Part 2 Feat. Fallacy' -> 'Part 2/Fallacy'; 'Joker And The Thief' = a title). No blank album. The longest lines the mod shows:
// the title 44 chars (c 21 'People Always Talk About The Weather (Remix)'), the artist 38 (c 6).
// Index c = the list position 0..29 = id 79 + c; the fill reads these three pointers for the rows 79..108 of g_vtab; a carrier row gets them at a Carbon pick / preview.
static const char *kCb[30][3] = {
    { "One Of Dem Days (Remix)", "Part 2/Fallacy", "Live From The Breadline" },   // c 0 (vault row 1, wave 29)
    { "Bounce", "Dynamite MC", "Styles Galore" },   // c 1 (vault row 3, wave 13)
    { "No Love", "Roots Manuva", "Alternately Deep" },   // c 2 (vault row 5, wave 21)
    { "What It Look Like", "Spank Rock", "YoYoYoYoYo" },   // c 3 (vault row 8, wave 2)
    { "Hard Drivers", "Ekstrak", "NFS Carbon Exclusive" },   // c 4 (vault row 11, wave 19)
    { "Love Me Or Hate Me", "Lady Sovereign", "Public Warning" },   // c 5 (vault row 12, wave 28)
    { "Scorpio", "Grandmaster Flash And The Furious Five", "The Message" },   // c 6 (vault row 15, wave 30)
    { "Show You How To Hustle", "Pharrell", "Pharrell" },   // c 7 (vault row 17, wave 31)
    { "Girl Fight (Remix)", "Tigarah", "Tigarah" },   // c 8 (vault row 18, wave 24)
    { "Hype Boys", "Sway", "This Is My Demo" },   // c 9 (vault row 19, wave 14)
    { "After Party", "Dynamite MC", "Styles Galore" },   // c 10 (vault row 29, wave 18)
    { "Signs Of Life", "Every Move A Picture", "Heart=Weapon" },   // c 11 (vault row 2, wave 10)
    { "Don't Speak (I Came To Make A Bang!)", "Eagles Of Death Metal", "Death By Sexy" },   // c 12 (vault row 4, wave 3)
    { "Heatseeker", "Valient Thorr", "Legend Of The World" },   // c 13 (vault row 6, wave 25)
    { "Around The Horn", "The Bronx", "The Bronx" },   // c 14 (vault row 7, wave 22)
    { "Hurricane", "Kyuss", "...And The Circus Leaves Town" },   // c 15 (vault row 16, wave 9)
    { "Thee Small Faces", "Metro Riots", "R.I.P. Mother" },   // c 16 (vault row 23, wave 20)
    { "I'm No Good", "The Vacation", "The Vacation" },   // c 17 (vault row 26, wave 23)
    { "I Am The Night, Colour Me Black", "Priestess", "Hello Master" },   // c 18 (vault row 27, wave 15)
    { "Joker And The Thief", "Wolfmother", "Wolfmother" },   // c 19 (vault row 28, wave 17)
    { "Good As Gold", "Tiga", "Sexor" },   // c 20 (vault row 9, wave 11)
    { "People Always Talk About The Weather (Remix)", "Yonderboi", "Splendid Isolation" },   // c 21 (vault row 10, wave 26)
    { "My Friend Dario", "Vitalic", "Ok Cowboy" },   // c 22 (vault row 13, wave 16)
    { "Washing Up (Remix)", "Tomas Andersson", "Washing Up" },   // c 23 (vault row 14, wave 7)
    { "Steamworks", "The Presets", "Beams" },   // c 24 (vault row 20, wave 6)
    { "Sugar (Remix)", "Ladytron", "Extended Play" },   // c 25 (vault row 21, wave 4)
    { "Ride A White Horse (Remix)", "Goldfrapp", "Supernature" },   // c 26 (vault row 22, wave 5)
    { "Feel The Rush (Remix)", "Melody", "Melody" },   // c 27 (vault row 24, wave 27)
    { "Are 'Friends' Electric ?", "Gary Numan", "Replicas" },   // c 28 (vault row 25, wave 8)
    { "Fighting In Built Up Areas", "Ladytron", "Witching Hour" },   // c 29 (vault row 30, wave 12)
};
