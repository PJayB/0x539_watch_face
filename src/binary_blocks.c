#include <pebble.h>
#include <time.h>
  
//#define DO_SECONDS
#define DO_DATE
#define AGGRESSIVE_CACHING
//#define LEET_EDITION
//#define VERBOSE_TITLES

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

Window* window;

#ifdef DO_DATE
TextLayer* dateBG;
#endif

TextLayer* timeBG;

typedef struct gridCell_s
{
  TextLayer* textLayer;
  InverterLayer* inverterLayer;
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
#define HOUR_COLS_12H 4
#define HOUR_COLS_24H 5
#define MINUTE_COLS 6
#define SECOND_COLS 6

gridCell_t _hour_cells[HOUR_COLS_24H];
gridCell_t _minute_cells[MINUTE_COLS];

gridRow_t hour_row = { _hour_cells, HOUR_COLS_24H, 0 };
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
  cell->textLayer = text_layer_create(rect);
#ifdef LEET_EDITION
  text_layer_set_text_alignment(cell->textLayer, GTextAlignmentCenter);
#else
  text_layer_set_text_alignment(cell->textLayer, GTextAlignmentRight);
#endif
  text_layer_set_font(cell->textLayer, fonts_get_system_font(FACE_FONT_KEY));
  text_layer_set_background_color(cell->textLayer, GColorClear);
  text_layer_set_text_color(cell->textLayer, GColorWhite);
  text_layer_set_text(cell->textLayer, cell->text);
  layer_add_child(parentLayer, (Layer *)cell->textLayer);

  // Init the inverter layer
  cell->inverterLayer = inverter_layer_create(GRect(0, 0, rect.size.w, rect.size.h));
  layer_set_hidden((Layer *)cell->inverterLayer, true);
  layer_add_child((Layer *)cell->textLayer, (Layer *)cell->inverterLayer);
}

void destroy_grid_cell(gridCell_t* cell)
{
  text_layer_destroy(cell->textLayer);
  inverter_layer_destroy(cell->inverterLayer);
}

void set_grid_cell_enabled(gridCell_t* cell, bool should_enable)
{
#ifdef LEET_EDITION
  cell->text[0] = '0' + should_enable;
#endif

  layer_set_hidden((Layer *)cell->inverterLayer, !should_enable);
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

void destroy_grid_row(gridRow_t* row)
{
  for (unsigned i = 0; i < row->cellCount; ++i)
  {
    destroy_grid_cell(&row->cells[i]);
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

void refresh_grid(const struct tm *t, TimeUnits u)
{
  if ((u & HOUR_UNIT) != 0)
  {
    unsigned hr = (unsigned)t->tm_hour;

    if (!clock_is_24h_style() && hr > 12) { hr -= 12; }

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


void handle_init() {

  window = window_create();
  window_stack_push(window, true /* Animated */);
  window_set_background_color(window, GColorBlack);

  // TIME background layer
  timeBG = text_layer_create(GRect(TIME_GRID_X, TIME_GRID_Y, GRID_OUTER_WIDTH, TIME_GRID_OUTER_HEIGHT));
  text_layer_set_background_color(timeBG, GColorClear);
  layer_add_child((Layer *)window, (Layer *)timeBG);

  // Set up 12H clock if that's what the user wants
  if ( !clock_is_24h_style() )
	hour_row.cellCount = HOUR_COLS_12H;

  // Init cells
  init_grid_row(&hour_row, (Layer *)timeBG, GRID_ROW_1_Y);
  init_grid_row(&minute_row, (Layer *)timeBG, GRID_ROW_2_Y);

#ifdef DO_SECONDS
  init_grid_row(&second_row, (Layer *)timeBG, GRID_ROW_3_Y);
#endif

#ifdef DO_DATE
  // DATE background layer
  dateBG = text_layer_create(GRect(DATE_GRID_X, DATE_GRID_Y, GRID_OUTER_WIDTH, DATE_GRID_OUTER_HEIGHT));
  text_layer_set_background_color(dateBG, GColorClear);
  layer_add_child((Layer *)window, (Layer *)dateBG);

  init_grid_row(&month_row, (Layer *)dateBG, GRID_ROW_1_Y);
  init_grid_row(&day_row, (Layer *)dateBG, GRID_ROW_2_Y);
#endif

  time_t time_now;
  time(&time_now);
  refresh_grid(localtime(&time_now), (TimeUnits)~0U);
}

void handle_deinit() {
#ifdef DO_DATE
  destroy_grid_row(&month_row);
  destroy_grid_row(&day_row);
#endif

  // Init cells
  destroy_grid_row(&hour_row);
  destroy_grid_row(&minute_row);

#ifdef DO_SECONDS
  destroy_grid_row(&second_row);
#endif

  text_layer_destroy(timeBG);
  
  window_destroy(window);
}

void handle_tick(struct tm *tick_time, TimeUnits units_changed) 
{
  refresh_grid(tick_time, units_changed);
}

int main() {
  tick_timer_service_subscribe(
    SECOND_UNIT | MINUTE_UNIT, 
    handle_tick);
  handle_init();
  app_event_loop();
  handle_deinit();
  return 0;
}
