#pragma once

// Patrones grandes en formato RLE (2-estado, B3/S23), incrustados para que el
// PatternRegistry sea determinista y no dependa de archivos en disco. Se parsean
// con parse_rle() (ver rle.h). Copias verbatim en includes/patterns/*.rle.
// Fuentes: github.com/jimblandy/golly (Patterns/Life/*) y github.com/p-ranav/cgol.

inline const char* RLE_PUFFER = R"RLE(
#C Puffer train
#C This was created simply by perturbing the sides of a B-heptomino
#C with two LWSS's. A B-heptomino is a naturally occurring object,
#C a precursor to the Herschel pattern, which lurches forward at the
#C speed c/2 before its own debris usually destroys it.
#C -- Not in this case!  The LWSS escorts keep the B-heptomino alive.
#C From Alan Hensel's "lifebc" pattern collection.
x = 5, y = 18, rule = B3/S23
3bo$4bo$o3bo$b4o4$o$boo$bbo$bbo$bo3$3bo$4bo$o3bo$b4o!
)RLE";

inline const char* RLE_CONDUIT = R"RLE(
#C A very long period oscillator: 1800 generations. In more humorous
#C moments, known as the glider racetrack. Sometimes you're a glider,
#C sometimes a spaceship, and sometimes just a hole
#C -- David Goodenough
#C
#C This uses several reactions:
#C
#C The glider starts travelling SW, and enters a three-glider->LWSS
#C synthesis found by David Buckingham. There are two parallel streams
#C of gliders moving in opposite directions, a NW P30 and and a SE
#C P60. When idling, these fall into eaters, but when *THE* glider
#C arrives, a single instance of the synthesis takes place, sending a
#C LWSS west. The gun firing SE has to be P60 to let the LWSS escape.
#C
#C Next, the LWSS hits a glider going NE from another P60 gun, and is
#C converted back into a glider going SE. I'm not sure who this
#C reaction is due to.
#C
#C Next, the kickback reaction with a NE stream from a P30 gun is used
#C to turn the glider around so it's going NW, and send it back
#C through the stream from the P60. As far as the LWSS + glider to
#C glider synthesis goes, a P30 gun would be OK. However, since you
#C can't get a glider back through a P30 stream without a crash, I
#C need a P60 stream to let the glider back through.
#C
#C Now it goes to a shuttle that turns it NE, and then it hits a
#C stream of SE gliders from a P30 gun. When it hits the stream, it
#C destroys exactly one glider in the stream, creating a hole. When
#C idling, the stream hits another shuttle that turns it NW, and then
#C runs into what would be a P30 gun, except that the inbound stream
#C keeps it permanently turned off. This is part of the switchable P30
#C LWSS gun found by David I. Bell, AKA an in-line NOT gate for glider
#C guns. When the hole arrives, it allows a single glider to escape
#C NE, which is promptly turned SE by another shuttle.
#C
#C The glider then runs into the westbound output of a P120 LWSS gun,
#C in a reaction found by Dean Hickerson. It undergoes the same
#C glider+LWSS->glider reaction, creating a glider going NE. This is
#C then reversed to SW by the "ping" reaction with a PD, and at that
#C point it's on its starting path. It continues SW, and once again
#C enters the three glider LWSS synthesis, and thus the cycle repeats.
#C
#C David Goodenough, January 1995
x = 308, y = 176, rule = B3/S23
161boo$162bo$162bobo7bo$163boo5bobo$168boo71b3o$168boo12boo56bo3bo$
168boo12boo55bo5bo$170bobo$172bo65bo7bo$238bo7bo$$239bo5bo$240bo3bo$
241b3o4$84bo$83bobo$66boo15boobo$66bobo14booboo3boo$61boo6bo13boobo4b
oo$57boobobbobbobbo13bobo$57boobboo6bo8bo5bo$66bobo7bobo$66boo9boo5$
85bo$86bo$84b3o40boo$127bobo8bo$118b3oboo4b3o6bo7bo$118b4obbo4b3o5boo
5bobo$122boo4b3o7bo4bo3boo3boo$127bobo13bo3boo3boo$93bo33boo14bo3boo$
91bobo50bobo$92boo51bo$268bo$267bobo$266boboo15boo$131bo117boo9boo3boo
boo14bobo$100bo30boo112b4o4boo5boo4boboo13bo6boo$101bo28bobo112b3oboo
bboo12bobo13bobbobbobboboo$99b3o148bo17bo5bo8bo6boobboo$274bobo7bobo$
274boo9boo$175bobbo56bobbo26bobbo$174bo59bo29bo$123boo49bo3bo55bo3bo
25bo3bo$108bo15boo48b4o56b4o26b4o$106bobo14bo155bo$107boo169boo$278bob
o$159boobb3obboo81boo7boo$159bobb5obbo81bo9bo9bo$116bo43b9o83b9o10boo$
116boo5bo33b3o9b3o77b3obb5obb3o6bobo$115bobo5bobo31bobbo7bobbo77bobbo
bb3obbobbo$126boo4boo24boo9boo79boo9boo23boo9boo$126boo4boo151boo9bobo
$126boo152bo6bo7b3o4boob3o$113boo8bobo126bo26bobo12b3o4bobb4o$112bobo
8bo126b4o6boobboo11boo3bo12b3o4boo$112bo131boo3boboboo5b4obobbobo6boo
3bo13bobo$111boo131boobbobbob3o5boobo3bo3bo4boo3bo14boo$249boboboo4bo
12bo6bobo$250b4o14bo4bo6bo$252bo19bo$268bo3bo$268bobo11$20boo$20boo4$
19b3o$19b3o$18bo3bo$17bo5bo142bo$18bo3bo101bobo39bobo$19b3o101bobbo39b
oo$114boo6boo10bo6bo$114boo4boo3bo8bo5bobo$122boo9bo6boobo$123bobbobb
oo9booboo3boo$124bobobbobb3o5boobo4boo$107boo20b4o7bobo$109bo20boo9bo$
96boo12bo8boo$17boo77boo4bo7bo8boo$18bo74boo5boo8bo$15b3o67boo5b3o5bo
bboo4bo13bo$15bo69boo6boo6b5oboo12boo$96boo4bo19boo$96boo$$110bo$111bo
$109b3o$114bobo$114boo$115bo3$64bo53bo$62b3o51bobo$61bo55boo$61boo49bo
$112b3o$115bo$114boo$$117boo$116boo$118bo3$50boo$51boo$50bo74bo$124boo
$124bobo6boo$133bo$134b3o$136bo$99bo$34boo61b3o$34bo61bo35boo$25bo6bob
o61boo33boo$22b4o6boo99bo$13bo7b4o68boo$12bobo6bobbo67bobo50boo$11bo3b
oo4b4o10boo57bo50bobbo$oo9bo3boo5b4o10boo97bo3b3o7bo6boo$oo9bo3boo8bo
9bo98b5o3bo6bo6boo$12bobo107boo9booboo3bo7bo$13bo108boo8b3oboo3bo3bobb
o$22bobo108boob4o5boo$23boo60b3o46b4o$23bo63bo47bo$86bo$27boo$27boo$$
14boo$13b3o3boobo55boo$3boo5boboo5bo3bobbo50bobo$3boo5bobbo4bo4bobboo
51bo$10boboo4b4o5boo8boo$13b3o3bo7b3o7boo$14boo11boo$26boo$26bo$59boo
9b3o$51bo7bobbo9bo$50bo3boo7bo7bo5bo$50bo5bo6bo12b4o$51b5o7bo11boobobo
3boo$59bobbo11b3obobbobboo$59boo14boobobo$76b4o$77bo!
)RLE";

inline const char* RLE_WICK = R"RLE(
#C Jason Summers, 25 Aug 1999 (minor optimizations made later).
#C From "jslife" pattern collection.
x = 127, y = 133, rule = B3/S23
56boo$52b4oboo$25b3o24b6o$53b4o$25bo37b4o$62bo3bo$57boo7bo$46bo9b4obbo
bbo$28b3o13boo9bo3boo$45boo9b4obbobbo$28b6o8bobbo4boo5boo7bo$33b3o6bob
o4bobbo9bo3bo$35bo8bo5b3o10b4o$33bobo7bobo4boo$33boo8bobo$bb3o29boo9b
oo7b6o$3boo4bo25bo17bo5bo$bb3o3bobo48bo$8bobo42bo4bo$9bo45boo3$16bo10b
obbo$15bobo13bo$14bobbo9bo3bo$15boo11b4o$24bo$23bobo$22bobbo$23boo5$
28bo60boo$27bobo55b4oboo$26bobbo55b6o$bbobo22boo57b4o$bbo33bo59b4o$bbo
19bobo10bobo57bo3bo$8bobo14bo8bobbo61bo$8bobo14bo9boo29boo15bo7boobbo
bbo$8bobo11bobbo39booboo12boo7b3o$10bo12b3o38bo3boo13bobo5boobbobbo$
10bo52boo4bo15bo13bo$10booboo47b3obbobo7b3o5b3o7bo3bo$11bobboo27bo19bo
bboobo7bo4bo13b4o$11b3oboo26bobo18boo11bo5bo$43boo33boo3bo$38b3o40boo
4b6o$40bo7bo37bo5bo$39bo7bobo42bo$46bobbo36bo4bo$47boo39boo$10boo$13b
oo$8bobboo47bobbo$8b3obb3o48bo$7bobo5bo44bo3bo$61b4o$$11bo$10boboo46bo
$10boboo45bobo54boo$boo8boo45bobbo50b4oboo$b3o12bobo40boo51b6o$b3o11bo
32bobo62b4o$b3o4bo6bo3bo31bo71b4o$oboo3bobo5bo3bo17bo13bo70bo3bo$3o3b
ooboo4bo20b3o9bobbo61boo3bo7bo$bo4booboo4bobbo16b3obo9b3o60bo3b4obbobb
o$7b3o5b3o16bo4bo56bo15bobb3oboo$8bo24boo3bo56b3o15b7obbobbo$33bo3boo
55b4o11bo8bo7bo$5bobobobo22boo37bo19boo13bobo11bo3bo$4bo7bo21b5o34bobo
18boobboo7bo15b4o$4bo7bo60boo20b5o8boboo$4bobbobobbo83b3o10boo$4b3o3b
3o101b6o$70b3o40bo5bo$72bo46bo$71bo41bo4bo$115boo$37b3o$37bobbo$37bobb
o46bobbo$91bo$41bo45bo3bo$34bo3bobbo46b4o$33b3o3boo$$27boo6b3o$27b3o7b
o4bobo$27b3o7bo3bo39bobo$27b3o11bo3bo38bo$26boboo11bo3bo38bo$26b3o12bo
39bobbo$27bo5b3o5bobbo37b3o$33b3o5b3o$34bo35bo$69b3o$31bobobobo30boob
oo$30bo7bo28b3obboo$30bo7bo29boobboo$30bobbobobbo32b3o$30b3o3b3o32boo
8$71bo$70bobo$69bo3bo$70boboo$72bo$60boo4boo$60b3obbobbo6bobo$60b3obbo
bbo5bo$60b3o4boo5bo3bo$59boboo3b3o5bo3bo$59b3o4b3o5bo$60bo4booboo4bobb
o$66b3o5b3o$67bo$$64bobobobo$63bo7bo$63bo7bo$63bobbobobbo$63b3o3b3o!
)RLE";

inline const char* RLE_AGAR = R"RLE(
#C A small spacefiller.
#C Spacefillers are the fastest-growing known pattern in Conway's
#C  Game of Life (probably the fastest possible). They fill space
#C  to a density of 1/2, conjectured to be the maximum density,
#C  and they do it at a speed of c/2 in each of the 4 directions,
#C  which has been proven to be the maximum possible speed.
#C This pattern starts with 200 cells, not the record lowest number
#C  of starting cells for a spacefiller (at the time of this writing,
#C  the record is 187).  Quadratic-growth patterns that start with
#C  as few as 26 ON cells are now known, but their growth rate is
#C  comparatively slow.
#C The population of spacefillers are easy to compute. This one's
#C  equations are:
#C       [(t+17)^2+511]/4 for t divisible by 4;
#C       [(t+17)^2+608]/4 for t mod 4 = 1;
#C       [(t+17)^2+563]/4 for t mod 4 = 2;
#C       [(t+17)^2+580]/4 for t mod 4 = 3.
#C Most spacefillers at this time have p2 stretchers on the left and
#C  right instead of the flipper stretchers in this pattern.
#C Original idea and middle part by Al Hensel; original construction
#C  and top/bottom stretchers by Hartmut Holzwart; size optimization
#C  by David Bell, Al Hensel, and Tim Coe.
#C This spacefiller by Hartmut Holzwart, 4 Nov 1998.
#C From Alan Hensel's "lifebc" pattern collection.
x = 49, y = 26, rule = B3/S23
20b3o3b3o$19bobbo3bobbo$4o18bo3bo18b4o$o3bo17bo3bo17bo3bo$o8bo12bo3bo
12bo8bo$bobbobboobbo25bobboobbobbo$6bo5bo7b3o3b3o7bo5bo$6bo5bo8bo5bo8b
o5bo$6bo5bo8b7o8bo5bo$bobbobboobbobboo4bo7bo4boobbobboobbobbo$o8bo3boo
4b11o4boo3bo8bo$o3bo9boo17boo9bo3bo$4o11b19o11b4o$16bobo11bobo$19b11o$
19bo9bo$20b9o$24bo$20b3o3b3o$22bo3bo$$21b3ob3o$21b3ob3o$20bobooboobo$
20b3o3b3o$21bo5bo!
)RLE";

