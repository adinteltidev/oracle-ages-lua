#pragma once
// Shared picture geometry; no frontend or engine calls.
// The RAM the renderer reads (the same address in both games unless two are given)
typedef struct {
  uint16_t tileset_flags, lcd_behaviour, camera_y, camera_x;
  uint16_t group, room, modifier, pack, dungeon_index, map_position, floor, dungeon_properties, dungeon_flags_h, game_over, next_lcd_behaviour;
  int room_packs, pack_seasons;       // roomPackData (bank 4; Ages a page per group), Seasons roomPackSeasonTable (bank 1)
  int pack_seasons_eu;                // roomPackSeasonTable in the European ROM
  uint16_t loading_room;              // wLoadingRoom: the room whose layout was loaded last
} GameRam;
static const GameRam ages_ram = {0xcc34, 0xff9b, 0xffaa, 0xffac, 0xcc2d, 0xcc30, 0xcc32, 0xcc31, 0xcc39, 0xcc3a, 0xcc3b, 0xcc3c, 0xcc3d, 0xcdd6, 0xff9d,
                                 4 * 0x4000 + 0x075c, 0, 0, 0xcc2f};
static const GameRam seasons_ram = {0xcc50, 0xff99, 0xffa8, 0xffaa, 0xcc49, 0xcc4c, 0xcc4e, 0xcc4d, 0xcc55, 0xcc56, 0xcc57, 0xcc58, 0xcc59, 0xcc35, 0xff9b,
                                    4 * 0x4000 + 0x073c, 1 * 0x4000 + 0x3e50, 1 * 0x4000 + 0x2a68, 0xcc4b};
enum {
  W_SCROLL_MODE = 0xcd00, W_OPENED_MENU = 0xcbcb, W_OFFSET_Y = 0xcd08, W_OFFSET_X = 0xcd09,
  W_PALETTE_FADE = 0xc4ab,            // wPaletteThread_mode: non-zero while a screen is fading
  W_ROOM_WIDTH = 0xcd0a, W_ROOM_HEIGHT = 0xcd0b,
  W2_DUNGEON_LAYOUT = 0xc00,          // $dc00 in WRAM bank 2: $40 bytes a floor, 8x8
  TILESETFLAG_SIDESCROLL = 0x20, TILESETFLAG_DUNGEON = 0x08, STATUS_BAR_LINES = 16, ROOMFLAG_VISITED = 0x10,
};

static inline uint8_t rd(const GBSample *s, uint16_t a) {
  if (a >= 0xff80) return s->hram[a - 0xff80];
  return s->wram[0][a - 0xc000];
}

// When the strips show rooms: the status bar split on and drawn, no menu open (the game-over screen is not
// opened as a menu, so it has its own flag), and either normal play (scroll mode 1; bit 7 means the camera is moving) or a scrolling screen
// transition (states 3-5), during which wActiveRoom is already the next room while the scroll and
// wScreenOffsetX/Y still count from the previous one. Scroll mode 0 is a scripted scene. Scroll
// mode 4 is a transition Link has only just asked for (state 3 and the direction already set): the
// screen and the scroll are still the room he is leaving, so it shows as normal play in that room.
// The game steps wActiveRoom (in a dungeon wDungeonMapPosition too) at the start of the main-loop
// pass that loads the next room and sets mode 8 only once the tileset is in, so a frame can end
// with either room in wActiveRoom; wLoadingRoom still holds the one on screen until the load. Taken
// from the wrong room, the strips show the rooms beside another one for a frame (the European Ages
// with Impa following lands samples on both sides of that step).
typedef struct {
  bool shown;
  bool moving;                  // a scrolling transition: every strip pixel comes from decoded rooms
  int dx, dy;                   // the grid step from the previous room to the current one while moving
  bool stepped;                 // scroll mode 4 with wActiveRoom already one step (bx, by) past the
  int bx, by;                   // room on screen
} View;

enum { W_TRANSITION_DIRECTION = 0xcd02, W_TRANSITION_STATE = 0xcd04, W_ACTIVE_LANGUAGE = 0xc62a };

// The European ROMs (game code AZ8P, AZ7P) keep the same RAM; a few ROM tables moved.
static inline bool rom_is_eu(const WideOptions *o) { return o->rom && o->rom_size > 0x142 && o->rom[0x142] == 'P'; }
enum { AGES_ANIMAL_COMPANION = 0xc610, SEASONS_ANIMAL_COMPANION = 0xc610, AGES_JABU_WATER_LEVEL = 0xc6e9, AGES_PAST_ROOM_FLAGS = 0xc800,
       SPECIALOBJECT_RICKY = 0x0b, SPECIALOBJECT_MOOSH = 0x0d, ROOMPACK_NUUN = 0x7f };

// Behaviour 2 is the status bar followed by a scroll per line (the underwater wave): the LCD
// interrupt drops it to 0 once the bar is drawn, and the next vblank restores it.
static inline int lcd_split(const GBSample *s, const GameRam *r) {
  int split = rd(s, r->lcd_behaviour);
  return split < 2 ? rd(s, r->next_lcd_behaviour) : split;
}

// Scenes drawn over the whole screen (Ages' ending and credits, Seasons' opening) keep the split
// behaviour but set wGfxRegsFinal.LYC past the last line, so LCDC never switches at the bar's end.
// A lit frame shows whether it switched; a blank one only has the LYC the next frame will get.
enum { W_GFX_REGS_FINAL_LYC = 0xc49c };
static inline bool whole_screen(const GBSample *s) {
  if (s->line_lcdc[0] & 0x80) return s->line_lcdc[0] == s->line_lcdc[STATUS_BAR_LINES];
  return rd(s, W_GFX_REGS_FINAL_LYC) >= FB_H;
}

// Whether the game has already moved wActiveRoom one grid step (dx, dy) on from wLoadingRoom, the
// room on screen: on the overworld's grid the next room number, in a dungeon the room the step
// left behind on the floor's layout. Rooms that load another room's layout (Seasons' Temple
// Remains, Ages' D3 spinner) keep the two apart without being a step apart, and stay as they are.
static inline bool stepped_on(const GBSample *s, const GameRam *r, int dx, int dy) {
  int room = rd(s, r->room), loading = rd(s, r->loading_room);
  if (room == loading) return false;
  if (rd(s, r->group) <= 1) return ((loading + dy * 16 + dx) & 0xff) == room;
  int pos = rd(s, r->map_position) - dy * 8 - dx, floor = rd(s, r->floor);
  if (pos < 0 || pos > 63 || floor > 15) return false;
  return s->wram[2][W2_DUNGEON_LAYOUT + floor * 0x40 + pos] == loading;
}

static inline View view_of(const GBSample *s, const GameRam *r) {
  View v = {false, false, 0, 0, false, 0, 0};
  int split = lcd_split(s, r), mode = rd(s, W_SCROLL_MODE) & 0x0f;
  if (!(split == 2 || split == 3) || whole_screen(s) || rd(s, W_OPENED_MENU) || rd(s, r->game_over)) return v;
  int state = rd(s, W_TRANSITION_STATE), dir = rd(s, W_TRANSITION_DIRECTION);
  static const int step[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};  // up, right, down, left
  if (mode == 0x01 || mode == 0x04) {
    v.shown = true;
    if (mode == 0x04 && dir <= 3 && stepped_on(s, r, step[dir][0], step[dir][1])) {
      v.stepped = true;
      v.bx = step[dir][0];
      v.by = step[dir][1];
    }
    return v;
  }
  if (!(mode & 0x0e) || state < 3 || state > 5 || dir > 3) return v;
  v.shown = v.moving = true;
  v.dx = step[dir][0];
  v.dy = step[dir][1];
  return v;
}

// The status bar is up whenever the split is on, menu or no menu. A menu drawn over the whole screen
// (the map) leaves the split flag set but never switches LCDC at the bar's end, so its top lines are
// its own picture, not a status bar.
static inline bool bar_up(const GBSample *s, const GameRam *r) {
  int split = lcd_split(s, r);
  return (split == 2 || split == 3) &&
         (view_of(s, r).shown || (rd(s, W_OPENED_MENU) && s->line_lcdc[0] != s->line_lcdc[STATUS_BAR_LINES]));
}

static inline bool room_shown(const GBSample *s, const GameRam *r) {
  View v = view_of(s, r);
  return v.shown && !v.moving;
}

// A register difference as a small signed offset (the camera plus the screen shake of up to 3)
enum { SHAKE = 3 };
static inline int wrap_offset(int v) { v &= 0xff; return v > 200 ? v - 256 : v; }
