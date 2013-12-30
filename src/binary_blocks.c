#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

//#define DO_SECONDS
#define DO_DATE
#define AGGRESSIVE_CACHING
//#define LEET_EDITION
//#define VERBOSE_TITLES

#if defined(DO_DATE) && defined(VERBOSE_TITLES)
# define APP_TITLE_DATE_ED " +Cal"
#else 
# define APP_TITLE_DATE_ED
#endif

#if defined(DO_SECONDS) && defined(VERBOSE_TITLES)
# define APP_TITLE_SECONDS_ED " +Secs"
#else
# define APP_TITLE_SECONDS_ED
#endif

#ifdef LEET_EDITION
# define APP_TITLE_BASE "0x539 L33T Edition"
# define MY_UUID { 0x55, 0x39, 0xD3, 0x1B, 0x06, 0x05, 0x4B, 0xF4, 0x86, 0x1A, 0x4A, 0x3D, 0xB9, 0xAC, 0xA8, 0xE3 }
#else
# define APP_TITLE_BASE "0x539"
# define MY_UUID { 0xa1, 0xbc, 0xc4, 0xc7, 0x34, 0xc0, 0x4b, 0xa6, 0x8e, 0x90, 0x30, 0xa9, 0xe9, 0x50, 0x9b, 0x46 }
#endif


PBL_APP_INFO(MY_UUID,
             APP_TITLE_BASE APP_TITLE_DATE_ED APP_TITLE_SECONDS_ED, "GeekyPanda",
             1, 0, /* App version */
	     RESOURCE_ID_IMAGE_MENU_ICON_COUNTDOWN,
             APP_INFO_WATCH_FACE);

#define FACE_FONT_KEY 		FONT_KEY_GOTHIC_14

#define SCREEN_WIDTH 		144
#define SCREEN_HEIGHT 		168

#define SCREEN_PADDING		5

#define GRID_COLS 		6

#define GRID_CELL_WIDTH 	(SCREEN_WIDTH / GRID_COLS)

#ifdef DO_SECONDS
# define GRID_CELL_HEIGHT 	(GRID_CELL_WIDTH)
#else
# define GRID_CELL_HEIGHT	(GRID_CELL_WIDTH + 7)
#endif

#define GRID_PADDING 		1
#define GRID_OUTER_WIDTH 	SCREEN_WIDTH

#define GRID_ROW_1_Y		(GRID_PADDING)
#define GRID_ROW_2_Y		(GRID_PADDING * 2 + GRID_CELL_HEIGHT)
#define GRID_ROW_3_Y		(GRID_PADDING * 3 + GRID_CELL_HEIGHT * 2)



#ifdef DO_SECONDS
# define TIME_GRID_ROWS 	3
#else
# define TIME_GRID_ROWS 	2
#endif

#define TIME_GRID_OUTER_HEIGHT	(GRID_CELL_HEIGHT * TIME_GRID_ROWS + GRID_PADDING * (TIME_GRID_ROWS + 1))
#define TIME_GRID_X 		0
#define TIME_GRID_Y 		(SCREEN_HEIGHT - TIME_GRID_OUTER_HEIGHT - SCREEN_PADDING)

#ifdef DO_DATE
# define DATE_GRID_ROWS		2
# define DATE_GRID_OUTER_HEIGHT (GRID_CELL_HEIGHT * DATE_GRID_ROWS + GRID_PADDING * 3)
# define DATE_GRID_X		0
# define DATE_GRID_Y		(SCREEN_PADDING)
#endif

Window window;

#ifdef DO_DATE
TextLayer dateBG;
#endif

TextLayer timeBG;

typedef struct gridCell_s
{
  TextLayer textLayer;
  InverterLayer inverterLayer;
  char text[4];
} gridCell_t;

typedef struct gridRow_s
{
  gridCell_t* cells;
  unsigned cellCount;
  unsigned enabledStates;
} gridRow_t;

#define MONTH_COLS 4
#define DAY_COLS 5
#define HOUR_COLS 4
#define MINUTE_COLS 6
#define SECOND_COLS 6

gridCell_t _hour_cells[HOUR_COLS];
gridCell_t _minute_cells[MINUTE_COLS];

gridRow_t hour_row = { _hour_cells, HOUR_COLS, 0 };
gridRow_t minute_row = { _minute_cells, MINUTE_COLS, 0 };

#ifdef DO_DATE
gridCell_t _month_cells[MONTH_COLS];
gridCell_t _day_cells[DAY_COLS];

gridRow_t month_row = { _month_cells, MONTH_COLS, 0 };
gridRow_t day_row = { _day_cells, DAY_COLS, 0 };
#endif

#ifdef DO_SECONDS
gridCell_t _second_cells[SECOND_COLS];
gridRow_t second_row = { _second_cells, SECOND_COLS, 0 };
#endif

inline bool bit_set(unsigned v, unsigned bit)
{
  return ((v >> bit) & 1) != 0;
}

void init_grid_cell(gridCell_t* cell, Layer* parentLayer, GRect rect, unsigned bit)
{
#ifdef LEET_EDITION
  cell->text[0] = '0';
  cell->text[1] = 0;
#else
  // The the text
  snprintf(cell->text, sizeof(cell->text), "%u", (unsigned)(1 << bit));
#endif

  // Init the text layer
  text_layer_init(&cell->textLayer, rect);
#ifdef LEET_EDITION
  text_layer_set_text_alignment(&cell->textLayer, GTextAlignmentCenter);
#else
  text_layer_set_text_alignment(&cell->textLayer, GTextAlignmentRight);
#endif
  text_layer_set_font(&cell->textLayer, fonts_get_system_font(FACE_FONT_KEY));
  text_layer_set_background_color(&cell->textLayer, GColorClear);
  text_layer_set_text_color(&cell->textLayer, GColorWhite);
  text_layer_set_text(&cell->textLayer, cell->text);
  layer_add_child(parentLayer, &cell->textLayer.layer);

  // Init the inverter layer
  inverter_layer_init(&cell->inverterLayer, GRect(0, 0, rect.size.w, rect.size.h));
  layer_set_hidden(&cell->inverterLayer.layer, true);
  layer_add_child(&cell->textLayer.layer, &cell->inverterLayer.layer);
}

void set_grid_cell_enabled(gridCell_t* cell, bool should_enable)
{
#ifdef LEET_EDITION
  cell->text[0] = '0' + should_enable;
#endif

  layer_set_hidden(&cell->inverterLayer.layer, !should_enable);
  // dirty is set automatically
}


void init_grid_row(gridRow_t* row, Layer* parentLayer, unsigned y)
{
  row->enabledStates = 0;

  unsigned x = SCREEN_WIDTH;
  for (unsigned i = 0; i < row->cellCount; ++i)
  {
    init_grid_cell(&row->cells[i], parentLayer, GRect(x - GRID_CELL_WIDTH, y, GRID_CELL_WIDTH - 1, GRID_CELL_HEIGHT), i);

    x -= GRID_CELL_WIDTH;
  }
}

void refresh_grid_row(gridRow_t* row, unsigned b)
{
  unsigned cache = row->enabledStates;
  if (cache != b)
  {
    for (unsigned i = 0; i < row->cellCount; ++i)
    {
      bool set = bit_set(b, i);
#ifdef AGGRESSIVE_CACHING
      bool cached_set = bit_set(cache, i);
      if ((set ^ cached_set) != 0)
#endif
      {
        set_grid_cell_enabled(&row->cells[i], set);
      }
    }
  }
  row->enabledStates = b;
}

void refresh_grid(const PblTm* t, unsigned u)
{
  if ((u & HOUR_UNIT) != 0)
  {
    unsigned hr = (unsigned)t->tm_hour;
    if (hr > 12) { hr -= 12; }

    refresh_grid_row(&hour_row, hr);
  }

  if ((u & MINUTE_UNIT) != 0)
  {
    unsigned m = (unsigned)t->tm_min;
    refresh_grid_row(&minute_row, m);
  }

#ifdef DO_SECONDS
  if ((u & SECOND_UNIT) != 0)
  {
    unsigned s = (unsigned)t->tm_sec;
    refresh_grid_row(&second_row, s);
  }
#endif

#ifdef DO_DATE
  if ((u & MONTH_UNIT) != 0)
  {
    unsigned M = (unsigned)t->tm_mon + 1;
    refresh_grid_row(&month_row, M);
  }

  if ((u & DAY_UNIT) != 0)
  {
    unsigned D = (unsigned)t->tm_mday;
    refresh_grid_row(&day_row, D);
  }
#endif
}


void handle_init(AppContextRef ctx) {

  window_init(&window, "Binary Blocks");
  window_stack_push(&window, true /* Animated */);
  window_set_background_color(&window, GColorBlack);

  // TIME background layer
  text_layer_init(&timeBG, GRect(TIME_GRID_X, TIME_GRID_Y, GRID_OUTER_WIDTH, TIME_GRID_OUTER_HEIGHT));
  text_layer_set_background_color(&timeBG, GColorClear);
  layer_add_child(&window.layer, &timeBG.layer);

  // Init cells
  init_grid_row(&hour_row, &timeBG.layer, GRID_ROW_1_Y);
  init_grid_row(&minute_row, &timeBG.layer, GRID_ROW_2_Y);

#ifdef DO_SECONDS
  init_grid_row(&second_row, &timeBG.layer, GRID_ROW_3_Y);
#endif

#ifdef DO_DATE
  // DATE background layer
  text_layer_init(&dateBG, GRect(DATE_GRID_X, DATE_GRID_Y, GRID_OUTER_WIDTH, DATE_GRID_OUTER_HEIGHT));
  text_layer_set_background_color(&dateBG, GColorClear);
  layer_add_child(&window.layer, &dateBG.layer);

  init_grid_row(&month_row, &dateBG.layer, GRID_ROW_1_Y);
  init_grid_row(&day_row, &dateBG.layer, GRID_ROW_2_Y);
#endif

  PblTm time_now;
  get_time(&time_now);
  refresh_grid(&time_now, ~0U);
}

void handle_deinit(AppContextRef ctx) {
  (void)ctx;
}

void handle_tick(AppContextRef ctx, PebbleTickEvent* t) 
{
  refresh_grid(t->tick_time, t->units_changed);
}

void pbl_main(void *params) {

  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
    .deinit_handler = &handle_deinit,
    .tick_info = {
      .tick_handler = &handle_tick,
#ifdef DO_SECONDS
      .tick_units = SECOND_UNIT
#else
      .tick_units = MINUTE_UNIT
#endif
    }
  };
  app_event_loop(params, &handlers);
}
