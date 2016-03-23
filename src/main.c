#include <pebble.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

// Languages
#define LANG_DUTCH 0
#define LANG_ENGLISH 1
#define LANG_FRENCH 2
#define LANG_GERMAN 3
#define LANG_SPANISH 4
#define LANG_PORTUGUESE 5
#define LANG_SWEDISH 6
#define LANG_MAX 7
  
#define KEY_TEMPERATURE 11
#define KEY_CONDITIONS 1
#define KEY_TEMP_MAX 2
#define KEY_SUNRISE 3
#define KEY_SUNSET 4
#define KEY_CONDITION_CODE 10


typedef struct {
  bool noon_low;
  bool hours_24;
} ClockConf;

static ClockConf clock_conf;

// WEATHER
static BitmapLayer *s_weather_icon_layer;
static GBitmap *s_weather_icon;


static Window *s_main_window;
static TextLayer *s_time_layer, *s_min_layer;
static TextLayer *s_month_layer, *s_date_layer, *s_week_layer;
static TextLayer *s_temp_layer, *s_max_layer, *s_bt_layer;

static Layer *s_hands_layer;
static GPath *s_hour_arrow;
static GPath *s_sunrise_arrow;
static GPath *s_sunset_arrow;

static Layer *s_canvas_layer;

static Layer *s_battery_layer;
static int s_battery_level;


static int angle_90 = TRIG_MAX_ANGLE / 4;
static int angle_180 = TRIG_MAX_ANGLE / 2;
static int angle_270 = 3 * TRIG_MAX_ANGLE / 4;

static int32_t min_a, hour_a;

static int sunrise_hour, sunrise_min, sunset_hour, sunset_min;

static char show_weather[4] = "T";
static char show_hour_arrow[4] = "T";
static char show_battery[4] = "T";

const char monthName[LANG_MAX][12][6] = {
	{ "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" }, // Dutch
	{ "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" },	// English
	{ "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" },	// French
	{ "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" },	// German
	{ "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" },	// Spanish
	{ "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" },	// Portuguese
	{ "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" }	// Swedish
};

const char weekDay[LANG_MAX][7][6] = {
	{ "ZON", "MAA", "DIN", "WOE", "DON", "VRI", "ZAT" },	// Dutch
	{ "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" },	// English
	{ "DIM", "LUN", "MAR", "MER", "JEU", "VEN", "SAM" },	// French
	{ "SON", "MON", "DIE", "MIT", "DON", "FRE", "SAM" },	// German
	{ "DOM", "LUN", "MAR", "MIE", "JUE", "VIE", "SAB" },	// Spanish
	{ "DOM", "SEG", "TER", "QUA", "QUI", "SEX", "SAB" },	// Portuguese
	{ "sön", "mån", "Tis", "ons", "tor", "fre", "lör" }	  // Swedish
};

static int curLang = LANG_ENGLISH;


enum {
  C_BACKGROUND = 0,
  C_FACE,
  C_SWEEP,
  C_TEXT,
  C_TEXT_ALT,
  C_OUTLINE,
  C_TICKS,
  C_APEX,
  C_RISESET
};

static GColor color[9];

enum {
  KEY_COLOR_BACKGROUND = 100,
  KEY_COLOR_FACE       = 110,
  KEY_COLOR_TEXT       = 119,
  KEY_COLOR_TEXT_ALT   = 120,
  KEY_COLOR_SWEEP      = 130,
  KEY_COLOR_OUTLINE    = 140,
  KEY_COLOR_TICKS      = 150,
  KEY_COLOR_APEX       = 160,
  KEY_COLOR_RISESET    = 170,
  KEY_TEMP_FORMAT      = 300,
  KEY_SHOW_WEATHER     = 308,
  KEY_SHOW_HOUR_ARROW  = 310,
  KEY_SHOW_BATTERY     = 312,
  KEY_NOON_LOW         = 330
};

enum {
  WEATHER_THUNDER  = 200,
  WEATHER_DRIZZLE  = 300,
  WEATHER_RAIN     = 500,
  WEATHER_SNOW     = 600,
  WEATHER_ATMOS    = 700,
  WEATHER_CLEAR    = 800,
  WEATHER_CLOUDS   = 802,
  WEATHER_WIND     = 950,
  WEATHER_EXTREME  = 900
};




static const GPathInfo HOUR_HAND_POINTS = {
  3, (GPoint []){
    {-6, -29},
    {6, -29},
    {0, -40}
  }
};

static const GPathInfo SUN_HAND_POINTS = {
  4, (GPoint []){
    { 0, -44},
    { 6, -40},
    { 0, -36},
    {-6, -40}
  }
};


static void bluetooth_callback(bool connected) {
  // Show icon if disconnected
  layer_set_hidden(text_layer_get_layer(s_bt_layer), connected);

  if(!connected) {
    // Issue a vibrating alert
    vibes_double_pulse();
  }
}


static void battery_update_proc(Layer *layer, GContext *ctx) {
//   GRect bounds = layer_get_bounds(layer);

  // Find the width of the bar
  int height = (int)(float)(((float)s_battery_level / 100.0F) * 168.0F);

  // Draw the background
//   graphics_context_set_fill_color(ctx, color[C_BACKGROUND]);
//   graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Draw the bar
  graphics_context_set_fill_color(ctx, color[C_SWEEP]);
  graphics_fill_rect(ctx, GRect(0, 168 - height, 1, height), 0, GCornerNone);
  
}

static void battery_callback(BatteryChargeState state) {
  // Record the new battery level
  s_battery_level = state.charge_percent;
//   APP_LOG(APP_LOG_LEVEL_INFO, "Battery is %d", s_battery_level);
  // Update meter
  layer_mark_dirty(s_battery_layer);
}



char *upcase(char *str) {
    char *s = str;
    while (*s)
    {
        *s++ = toupper((int)*s);
    }
    return str;
}


/*\
|*| DrawArc function thanks to Cameron MacFarland (http://forums.getpebble.com/profile/12561/Cameron%20MacFarland)
\*/
static void graphics_draw_arc_old(GContext *ctx, GPoint center, int radius, int thickness, int start_angle, int end_angle, GColor c) {
	int32_t xmin = 65535000, xmax = -65535000, ymin = 65535000, ymax = -65535000;
	int32_t cosStart, sinStart, cosEnd, sinEnd;
	int32_t r, t;
	
	while (start_angle < 0) start_angle += TRIG_MAX_ANGLE;
	while (end_angle < 0) end_angle += TRIG_MAX_ANGLE;

	start_angle %= TRIG_MAX_ANGLE;
	end_angle %= TRIG_MAX_ANGLE;
	
	if (end_angle == 0) end_angle = TRIG_MAX_ANGLE;
	
	if (start_angle > end_angle) {
    graphics_draw_arc_old(ctx, center, radius, thickness, start_angle, TRIG_MAX_ANGLE, c);
		graphics_draw_arc_old(ctx, center, radius, thickness, 0, end_angle, c);
	} else {
		// Calculate bounding box for the arc to be drawn
		cosStart = cos_lookup(start_angle);
		sinStart = sin_lookup(start_angle);
		cosEnd = cos_lookup(end_angle);
		sinEnd = sin_lookup(end_angle);
		
		r = radius;
		// Point 1: radius & start_angle
		t = r * cosStart;
		if (t < xmin) xmin = t;
		if (t > xmax) xmax = t;
		t = r * sinStart;
		if (t < ymin) ymin = t;
		if (t > ymax) ymax = t;

		// Point 2: radius & end_angle
		t = r * cosEnd;
		if (t < xmin) xmin = t;
		if (t > xmax) xmax = t;
		t = r * sinEnd;
		if (t < ymin) ymin = t;
		if (t > ymax) ymax = t;
		
		r = radius - thickness;
		// Point 3: radius-thickness & start_angle
		t = r * cosStart;
		if (t < xmin) xmin = t;
		if (t > xmax) xmax = t;
		t = r * sinStart;
		if (t < ymin) ymin = t;
		if (t > ymax) ymax = t;

		// Point 4: radius-thickness & end_angle
		t = r * cosEnd;
		if (t < xmin) xmin = t;
		if (t > xmax) xmax = t;
		t = r * sinEnd;
		if (t < ymin) ymin = t;
		if (t > ymax) ymax = t;
		
		// Normalization
		xmin /= TRIG_MAX_RATIO;
		xmax /= TRIG_MAX_RATIO;
		ymin /= TRIG_MAX_RATIO;
		ymax /= TRIG_MAX_RATIO;
				
		// Corrections if arc crosses X or Y axis
		if ((start_angle < angle_90) && (end_angle > angle_90)) {
			ymax = radius;
		}
		
		if ((start_angle < angle_180) && (end_angle > angle_180)) {
			xmin = -radius;
		}
		
		if ((start_angle < angle_270) && (end_angle > angle_270)) {
			ymin = -radius;
		}
		
		// Slopes for the two sides of the arc
		float sslope = (float)cosStart/ (float)sinStart;
		float eslope = (float)cosEnd / (float)sinEnd;
	 
		if (end_angle == TRIG_MAX_ANGLE) eslope = -1000000;
	 
		int ir2 = (radius - thickness) * (radius - thickness);
		int or2 = radius * radius;
	 
		graphics_context_set_stroke_color(ctx, c);

		for (int x = xmin; x <= xmax; x++) {
			for (int y = ymin; y <= ymax; y++)
			{
				int x2 = x * x;
				int y2 = y * y;
	 
				if (
					(x2 + y2 < or2 && x2 + y2 >= ir2) && (
						(y > 0 && start_angle < angle_180 && x <= y * sslope) ||
						(y < 0 && start_angle > angle_180 && x >= y * sslope) ||
						(y < 0 && start_angle <= angle_180) ||
						(y == 0 && start_angle <= angle_180 && x < 0) ||
						(y == 0 && start_angle == 0 && x > 0)
					) && (
						(y > 0 && end_angle < angle_180 && x >= y * eslope) ||
						(y < 0 && end_angle > angle_180 && x <= y * eslope) ||
						(y > 0 && end_angle >= angle_180) ||
						(y == 0 && end_angle >= angle_180 && x < 0) ||
						(y == 0 && start_angle == 0 && x > 0)
					)
				)
				graphics_draw_pixel(ctx, GPoint(center.x+x, center.y+y));
			}
		}
	}
}




static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  int htime;
  
  // Create a long-lived buffer
  static char month[] = "JAN";
  static char date[] = "31";
  static char week[] = "MON";
  static char hour[] = "00";
  static char minutes[] = "00";
  
  // Month, Date, Week
  snprintf(month, 4, "%s", monthName[curLang][tick_time->tm_mon]);
  snprintf(date, 3, "%.2d", tick_time->tm_mday);
  snprintf(week, 4, "%s", weekDay[curLang][tick_time->tm_wday]);
  
  // Write the current hours and minutes into the buffer
  if (clock_is_24h_style() == true) {
    // Use 24 hour format
    strftime(hour, sizeof("00"), "%H", tick_time);
  } else {
    // Use 12 hour format
    strftime(hour, sizeof("00"), "%I", tick_time);
    htime = (((int)tick_time->tm_hour + 11) % 12) + 1;
    if (htime < 10) memmove(hour, hour+1, strlen (hour+1) + 1);
  }
  // Minutes
  strftime(minutes, sizeof("00"), "%M", tick_time);

  
  
  // Display this time on the TextLayer
  text_layer_set_text(s_month_layer, month);
  text_layer_set_text(s_date_layer, date);
  text_layer_set_text(s_week_layer, week);
  text_layer_set_text(s_time_layer, hour);
  text_layer_set_text(s_min_layer, minutes);
}

static void sweep_math () {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  int32_t hour_angle;
  int32_t hours;

  // Minute
  min_a = TRIG_MAX_ANGLE * t->tm_min / 60 - angle_90;
  
  // Hour
  hour_angle = clock_conf.noon_low ? angle_90 : -angle_90;
  hours = clock_conf.hours_24 ? 24 : 12;
  hour_a = (TRIG_MAX_ANGLE * (((t->tm_hour % hours) * 6) + (t->tm_min / 10))) / (hours * 6) + hour_angle ;  
}

static void tick_handler(struct tm *t, TimeUnits units_changed) {
  sweep_math();
  
  update_time();
}



static void draw_clocks(Layer *this_layer, GContext *ctx) {
  GPoint point, point2;
  
  int32_t sunrise_set_angle;
  
  // Draw the hours
  /////////////////////////////////
  point = GPoint(100, 43);
  // Fill
  graphics_context_set_fill_color(ctx, color[C_FACE]);
  graphics_fill_circle(ctx, point, 40);
  // Outline
  graphics_context_set_stroke_color(ctx, color[C_OUTLINE]);
#ifdef PBL_COLOR
  graphics_context_set_stroke_width(ctx, 1);
#endif  
  graphics_draw_circle(ctx, point, 40);
  // Arc  
  graphics_draw_arc_old(ctx, point, 40+1, 7,
                        clock_conf.noon_low ? angle_90 : -angle_90,
                        hour_a, color[C_SWEEP]);  

  // New method - ctx, GRect rect, GOvalScaleMode scale_mode, angle_start, angle_end);
//   graphics_draw_arc(ctx, GRect rect, GOvalScaleMode scale_mode, +angle_90, hour_a);
  
  // Draw the minutes
  /////////////////////////////////
  point = GPoint(100, 124);
  // Fill
  graphics_context_set_fill_color(ctx, color[C_FACE]);
  graphics_fill_circle(ctx, point, 40);
  // Outline
  graphics_context_set_stroke_color(ctx, color[C_OUTLINE]);
#ifdef PBL_COLOR
  graphics_context_set_stroke_width(ctx, 1);
#endif
  graphics_draw_circle(ctx, point, 40);
  // Arc
  graphics_draw_arc_old(ctx, point, 40+1, 7, -angle_90, min_a, color[C_SWEEP]);

  

  // Sunrise / Sunset arrows
  graphics_context_set_fill_color(ctx, color[C_BACKGROUND]);
  graphics_context_set_stroke_color(ctx, color[C_RISESET]);
  sunrise_set_angle = clock_conf.noon_low ? angle_180 : 0;
  // Sunrise
  gpath_rotate_to(s_sunrise_arrow, (TRIG_MAX_ANGLE * (((sunrise_hour % 24) * 6) + (sunrise_min / 10))) / (24 * 6) + sunrise_set_angle);
  gpath_draw_filled(ctx, s_sunrise_arrow);
  gpath_draw_outline(ctx, s_sunrise_arrow);  
  // Sunset
  gpath_rotate_to(s_sunset_arrow, (TRIG_MAX_ANGLE * (((sunset_hour % 24) * 6) + (sunset_min / 10))) / (24 * 6) + sunrise_set_angle);
  gpath_draw_filled(ctx, s_sunset_arrow);
  gpath_draw_outline(ctx, s_sunset_arrow);

  
  
  // Draw ticks
  /////////////////////////////////
  point = GPoint(99, 83-6);
  point2 = GPoint(99, 84+6);
  graphics_context_set_stroke_color(ctx, color[C_APEX]);
#ifdef PBL_COLOR
  graphics_context_set_stroke_width(ctx, 3);
#endif
  graphics_draw_line(ctx, point, point2);

  // Hour ticks
  graphics_context_set_stroke_color(ctx, color[C_TICKS]);
  graphics_draw_line(ctx, GPoint(138, 43), GPoint(140, 43)); // 6p  
  graphics_draw_line(ctx, GPoint(60, 43), GPoint(62, 43));   // 6a
  graphics_draw_line(ctx, GPoint(100, 3), GPoint(100, 5));   // noon  
  
  // Minute ticks
  graphics_context_set_stroke_color(ctx, color[C_TICKS]);  
  graphics_draw_line(ctx, GPoint(138, 124), GPoint(140, 124)); // 15  
  graphics_draw_line(ctx, GPoint(60, 124), GPoint(62, 124));   // 45
  graphics_draw_line(ctx, GPoint(100, 162), GPoint(100, 164)); // 30  
}


static void twelve_hour_proc(Layer *layer, GContext *ctx) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  
  // Twelve hour arrow
  graphics_context_set_fill_color(ctx, color[C_TEXT]);
  graphics_context_set_stroke_color(ctx, color[C_BACKGROUND]);

  gpath_rotate_to(s_hour_arrow, (TRIG_MAX_ANGLE * (((t->tm_hour % 12) * 6) + (t->tm_min / 10))) / (12 * 6));
  gpath_draw_filled(ctx, s_hour_arrow);
  gpath_draw_outline(ctx, s_hour_arrow);
}


static void main_window_text_load(Window *window) {
  
  // CALENDAR //////////////////////////////////////////////////////////
  
  // Month
  s_month_layer = text_layer_create(GRect(2,11,58,20));  
  text_layer_set_background_color(s_month_layer, GColorClear);
  text_layer_set_text_color(s_month_layer, color[C_TEXT_ALT]);
  text_layer_set_font(s_month_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_month_layer, GTextAlignmentCenter);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_month_layer));  
    
  // Date
  s_date_layer = text_layer_create(GRect(2,25,58,40));  
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, color[C_TEXT_ALT]);
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_date_layer));

  // Week
  s_week_layer = text_layer_create(GRect(2,53,58,20));  
  text_layer_set_background_color(s_week_layer, GColorClear);
  text_layer_set_text_color(s_week_layer, color[C_TEXT_ALT]);
  text_layer_set_font(s_week_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_week_layer, GTextAlignmentCenter);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_week_layer));  
    
  // If weather is hidden, then push calendar down
  if (strcmp(show_weather, "F") == 0) {
    // Month
    layer_set_frame(text_layer_get_layer(s_month_layer), GRect(2,52,58,20));
    layer_mark_dirty(text_layer_get_layer(s_month_layer));  
    // Date
    layer_set_frame(text_layer_get_layer(s_date_layer), GRect(2,66,58,40));
    layer_mark_dirty(text_layer_get_layer(s_date_layer));  
    // Week
    layer_set_frame(text_layer_get_layer(s_week_layer), GRect(2,94,58,20));
    layer_mark_dirty(text_layer_get_layer(s_week_layer));  
  }
  
  
  // WEATHER //////////////////////////////////////////////////////////
  
  // Temp
  s_temp_layer = text_layer_create(GRect(2,106,58,40));  
  text_layer_set_background_color(s_temp_layer, GColorClear);
  text_layer_set_text_color(s_temp_layer, color[C_TEXT_ALT]);
  text_layer_set_font(s_temp_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_temp_layer, GTextAlignmentCenter);
  text_layer_set_text(s_temp_layer, "...");
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_temp_layer));
  layer_set_hidden(text_layer_get_layer(s_temp_layer), strcmp(show_weather, "F") == 0);

  // Max
  s_max_layer = text_layer_create(GRect(2,132,58,20));  
  text_layer_set_background_color(s_max_layer, GColorClear);
  text_layer_set_text_color(s_max_layer, color[C_TEXT_ALT]);
  text_layer_set_font(s_max_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_max_layer, GTextAlignmentCenter);
  text_layer_set_text(s_max_layer, "...");
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_max_layer));
  layer_set_hidden(text_layer_get_layer(s_max_layer), strcmp(show_weather, "F") == 0);
  
  
  // CLOCK //////////////////////////////////////////////////////////
  
  // Hours
  s_time_layer = text_layer_create(GRect(69,17,60,50));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, color[C_TEXT]);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_layer));
  
  // Minutes
  s_min_layer = text_layer_create(GRect(69,97,60,50));  
  text_layer_set_background_color(s_min_layer, GColorClear);
  text_layer_set_text_color(s_min_layer, color[C_TEXT]);  
  text_layer_set_font(s_min_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_min_layer, GTextAlignmentCenter);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_min_layer));

}


static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);
  
  // Create battery meter Layer
  s_battery_layer = layer_create(GRect(0, 0, 2, window_bounds.size.h));
  layer_add_child(window_layer, s_battery_layer);
  layer_set_update_proc(s_battery_layer, battery_update_proc);
  layer_set_hidden(s_battery_layer, strcmp(show_battery, "F") == 0);
  
  // Create Layer
  s_canvas_layer = layer_create(GRect(0, 0, window_bounds.size.w, window_bounds.size.h));
  layer_add_child(window_layer, s_canvas_layer);
  layer_set_update_proc(s_canvas_layer, draw_clocks);

  // Twelve hour hand
  s_hands_layer = layer_create(GRect(0, 0, window_bounds.size.w, window_bounds.size.h));
  layer_add_child(window_layer, s_hands_layer);  
  layer_set_update_proc(s_hands_layer, twelve_hour_proc);
  layer_set_hidden(s_hands_layer, strcmp(show_hour_arrow, "F") == 0);
    
  // Load text layers
  main_window_text_load(window);

  // Create the BitmapLayer to display the GBitmap WEATHER
  s_weather_icon_layer = bitmap_layer_create(GRect(19,90,24,24));
  s_weather_icon = gbitmap_create_with_resource(RESOURCE_ID_WEATHER_WHITE_UNKNOWN);  
  bitmap_layer_set_bitmap(s_weather_icon_layer, s_weather_icon);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_weather_icon_layer)); 
  layer_set_hidden(bitmap_layer_get_layer(s_weather_icon_layer), strcmp(show_weather, "F") == 0);

  
  // NO BT
  s_bt_layer = text_layer_create(GRect(3,74,58,20));  
  text_layer_set_background_color(s_bt_layer, GColorClear);
  text_layer_set_text_color(s_bt_layer, color[C_TEXT_ALT]);
  text_layer_set_font(s_bt_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_bt_layer, GTextAlignmentCenter);
  text_layer_set_text(s_bt_layer, "NO BT");
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_bt_layer));    
  // Show the correct state of the BT connection from the start
  bluetooth_callback(bluetooth_connection_service_peek());

  // If weather is hidden, then push calendar down
  if (strcmp(show_weather, "F") == 0) {
    layer_set_frame(text_layer_get_layer(s_bt_layer), GRect(3,150,58,20));
    layer_mark_dirty(text_layer_get_layer(s_bt_layer));  
  }
  
  
}



static void main_window_text_unload(Window *window) {
  text_layer_destroy(s_month_layer);
  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_week_layer);
  text_layer_destroy(s_temp_layer);
  text_layer_destroy(s_max_layer);
  text_layer_destroy(s_bt_layer);
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_min_layer);
}



static void main_window_unload(Window *window) {
  // Destroy weather icons
  gbitmap_destroy(s_weather_icon);
  bitmap_layer_destroy(s_weather_icon_layer);  
  // Destroy Layers  
  layer_destroy(s_battery_layer);
  layer_destroy(s_hands_layer);
  layer_destroy(s_canvas_layer);
  // Destroy text layers
  main_window_text_unload(window);
}



unsigned int StringToHex(const char * s) {
 unsigned int result = 0;
 int c ;
 if ('0' == *s && 'x' == *(s+1)) { s+=2;
  while (*s) {
   result = result << 4;
   if (c=(*s-'0'),(c>=0 && c <=9)) result|=c;
   else if (c=(*s-'A'),(c>=0 && c <=5)) result|=(c+10);
   else if (c=(*s-'a'),(c>=0 && c <=5)) result|=(c+10);
   else break;
   ++s;
  }
 }
 return result;
}

char *strdup (const char *s) {
    char *d = malloc (strlen (s) + 1);   // Space for length plus nul
    if (d == NULL) return NULL;          // No memory
    strcpy (d,s);                        // Copy the characters
    return d;                            // Return the new string
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  // Store incoming information
  static char temp_buffer[32];
  static char temperature_buffer[8];
//   static char temp_format_buffer[8];
  static char temp_max_buffer[8];
//   static char conditions_buffer[32];
  static char sunrise_buffer[64];
  static char sunset_buffer[64];
  
  static int condition_int;
  
  static char *colorData;
  
  // Read first item
  Tuple *t = dict_read_first(iterator);

  // For all items
  while(t != NULL) {
    // Which key was received?
    switch(t->key) {
      case KEY_TEMP_FORMAT:
        // NOTE: I don't think this is used, format stays on JS side - we only get temp and max returned in whatever format is selected
//         snprintf(temp_format_buffer, sizeof(temp_format_buffer), "%d", (int)t->value->int32);
        break;
      case KEY_TEMPERATURE:
        snprintf(temperature_buffer, sizeof(temperature_buffer), "%d", (int)t->value->int32);
        text_layer_set_text(s_temp_layer, temperature_buffer);
        // For some reason the watch shows a temp before final temp...
        // Why???
        APP_LOG(APP_LOG_LEVEL_INFO, "Temperature %s", temperature_buffer);
        break;
      case KEY_TEMP_MAX:
        snprintf(temp_max_buffer, sizeof(temp_max_buffer), "%d", (int)t->value->int32);
        text_layer_set_text(s_max_layer, temp_max_buffer);
        break;
//       case KEY_CONDITIONS:
//         snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", t->value->cstring);
//         text_layer_set_text(s_cond_layer, upcase(conditions_buffer));
//         break;
      case KEY_CONDITION_CODE:
        condition_int = (int)t->value->int32;
        switch(condition_int) {
          case WEATHER_THUNDER: s_weather_icon = gbitmap_create_with_resource(RESOURCE_ID_WEATHER_WHITE_THUNDER); break;
          case WEATHER_DRIZZLE: s_weather_icon = gbitmap_create_with_resource(RESOURCE_ID_WEATHER_WHITE_DRIZZLE); break;
          case WEATHER_RAIN: s_weather_icon = gbitmap_create_with_resource(RESOURCE_ID_WEATHER_WHITE_RAIN); break;
          case WEATHER_SNOW: s_weather_icon = gbitmap_create_with_resource(RESOURCE_ID_WEATHER_WHITE_SNOW); break;
          case WEATHER_ATMOS: s_weather_icon = gbitmap_create_with_resource(RESOURCE_ID_WEATHER_WHITE_ATMOS); break;
          case WEATHER_CLEAR: s_weather_icon = gbitmap_create_with_resource(RESOURCE_ID_WEATHER_WHITE_SUNNY); break;
          case WEATHER_CLOUDS: s_weather_icon = gbitmap_create_with_resource(RESOURCE_ID_WEATHER_WHITE_CLOUDY); break;
          case WEATHER_WIND: s_weather_icon = gbitmap_create_with_resource(RESOURCE_ID_WEATHER_WHITE_WIND); break;
          case WEATHER_EXTREME: s_weather_icon = gbitmap_create_with_resource(RESOURCE_ID_WEATHER_WHITE_EXTREME); break;
          default: s_weather_icon = gbitmap_create_with_resource(RESOURCE_ID_WEATHER_WHITE_UNKNOWN); break;
        }
        bitmap_layer_set_bitmap(s_weather_icon_layer, s_weather_icon);
        break;
      case KEY_SUNRISE:
        snprintf(sunrise_buffer, sizeof(sunrise_buffer), "%d", (int)t->value->int32);
        // Sunrise
        sunrise_hour = atoi(sunrise_buffer) / 100;
        sunrise_min = atoi(sunrise_buffer) % 100;
//         APP_LOG(APP_LOG_LEVEL_INFO, "Sunrise is %d: %d", sunrise_hour, sunrise_min);
        break;
      case KEY_SUNSET:
        snprintf(sunset_buffer, sizeof(sunset_buffer), "%d", (int)t->value->int32);
        // Sunset
        sunset_hour = atoi(sunset_buffer) / 100;
        sunset_min = atoi(sunset_buffer) % 100;
//         APP_LOG(APP_LOG_LEVEL_INFO, "Sunrise is %d: %d", sunset_hour, sunset_min);
        break;
      case KEY_COLOR_BACKGROUND:
        #ifdef PBL_COLOR
          colorData = t->value->cstring;
          color[C_BACKGROUND] = GColorFromHEX(StringToHex(colorData));
          persist_write_string(KEY_COLOR_BACKGROUND, colorData);      
          window_set_background_color(s_main_window, color[C_BACKGROUND]);
        #endif
        break;
      case KEY_COLOR_FACE:
        #ifdef PBL_COLOR
          colorData = t->value->cstring;          
          color[C_FACE] = GColorFromHEX(StringToHex(colorData));
          persist_write_string(KEY_COLOR_FACE, colorData);
        #endif
        break;
      case KEY_COLOR_SWEEP:
        #ifdef PBL_COLOR
          colorData = t->value->cstring;
          color[C_SWEEP] = GColorFromHEX(StringToHex(colorData));
          persist_write_string(KEY_COLOR_SWEEP, colorData);
        #endif
        break;
      case KEY_COLOR_OUTLINE:
        #ifdef PBL_COLOR
          colorData = t->value->cstring;
          color[C_OUTLINE] = GColorFromHEX(StringToHex(colorData));
          persist_write_string(KEY_COLOR_OUTLINE, colorData);
      #endif
        break;
      case KEY_COLOR_TICKS:
        #ifdef PBL_COLOR
          colorData = t->value->cstring;          
          color[C_TICKS] = GColorFromHEX(StringToHex(colorData));
          persist_write_string(KEY_COLOR_TICKS, colorData);      
        #endif
        break;
      case KEY_COLOR_APEX:
        #ifdef PBL_COLOR
          colorData = t->value->cstring;          
          color[C_APEX] = GColorFromHEX(StringToHex(colorData));
          persist_write_string(KEY_COLOR_APEX, colorData);      
        #endif
        break;
      case KEY_COLOR_RISESET:
        #ifdef PBL_COLOR
          colorData = t->value->cstring;          
          color[C_RISESET] = GColorFromHEX(StringToHex(colorData));
          persist_write_string(KEY_COLOR_RISESET, colorData);      
        #endif
        break;
      case KEY_COLOR_TEXT:
        #ifdef PBL_COLOR
          colorData = t->value->cstring;
          color[C_TEXT] = GColorFromHEX(StringToHex(colorData));
          persist_write_string(KEY_COLOR_TEXT, colorData);
          text_layer_set_text_color(s_time_layer, color[C_TEXT]);
          text_layer_set_text_color(s_min_layer, color[C_TEXT]);
          update_time();
        #endif
        break;
      case KEY_COLOR_TEXT_ALT:
        #ifdef PBL_COLOR
          colorData = t->value->cstring;
          color[C_TEXT_ALT] = GColorFromHEX(StringToHex(colorData));
          persist_write_string(KEY_COLOR_TEXT_ALT, colorData);
          text_layer_set_text_color(s_month_layer, color[C_TEXT_ALT]);
          text_layer_set_text_color(s_date_layer, color[C_TEXT_ALT]);
          text_layer_set_text_color(s_week_layer, color[C_TEXT_ALT]);
          text_layer_set_text_color(s_temp_layer, color[C_TEXT_ALT]);
          text_layer_set_text_color(s_max_layer, color[C_TEXT_ALT]);
          update_time();
        #endif
        break;
      case KEY_SHOW_WEATHER:
        snprintf(temp_buffer, sizeof(temp_buffer), "%s", t->value->cstring);
        if (strcmp("T", temp_buffer) == 0) {
          snprintf(show_weather, sizeof(show_weather), "%s", "T");
          // Update positions of other elements
          // Month
          layer_set_frame(text_layer_get_layer(s_month_layer), GRect(2,11,58,20));
          layer_mark_dirty(text_layer_get_layer(s_month_layer));  
          // Date
          layer_set_frame(text_layer_get_layer(s_date_layer), GRect(2,25,58,40));
          layer_mark_dirty(text_layer_get_layer(s_date_layer));  
          // Week
          layer_set_frame(text_layer_get_layer(s_week_layer), GRect(2,53,58,20));
          layer_mark_dirty(text_layer_get_layer(s_week_layer));  
          // NO BT
          layer_set_frame(text_layer_get_layer(s_bt_layer), GRect(3,74,58,20));
          layer_mark_dirty(text_layer_get_layer(s_bt_layer)); 
        } else {
          snprintf(show_weather, sizeof(show_weather), "%s", "F");
          // Update positions of other elements
          // Month
          layer_set_frame(text_layer_get_layer(s_month_layer), GRect(2,52,58,20));
          layer_mark_dirty(text_layer_get_layer(s_month_layer));  
          // Date
          layer_set_frame(text_layer_get_layer(s_date_layer), GRect(2,66,58,40));
          layer_mark_dirty(text_layer_get_layer(s_date_layer));  
          // Week
          layer_set_frame(text_layer_get_layer(s_week_layer), GRect(2,94,58,20));
          layer_mark_dirty(text_layer_get_layer(s_week_layer));  
          // NO BT
          layer_set_frame(text_layer_get_layer(s_bt_layer), GRect(3,150,58,20));
          layer_mark_dirty(text_layer_get_layer(s_bt_layer)); 
        }
        // Show/hide weather layers
        layer_set_hidden(bitmap_layer_get_layer(s_weather_icon_layer), strcmp(show_weather, "F") == 0);  
        layer_set_hidden(text_layer_get_layer(s_temp_layer), strcmp(show_weather, "F") == 0);
        layer_set_hidden(text_layer_get_layer(s_max_layer), strcmp(show_weather, "F") == 0);
        persist_write_string(KEY_SHOW_WEATHER, show_weather);
        break;
      
      case KEY_SHOW_HOUR_ARROW:
        snprintf(temp_buffer, sizeof(temp_buffer), "%s", t->value->cstring);
        if (strcmp("T", temp_buffer) == 0) {
          snprintf(show_hour_arrow, sizeof(show_hour_arrow), "%s", "T");
        } else {
          snprintf(show_hour_arrow, sizeof(show_hour_arrow), "%s", "F");          
        }
        layer_set_hidden(s_hands_layer, strcmp(show_hour_arrow, "F") == 0);
        persist_write_string(KEY_SHOW_HOUR_ARROW, show_hour_arrow);
        break;
      case KEY_SHOW_BATTERY:
        snprintf(temp_buffer, sizeof(temp_buffer), "%s", t->value->cstring);
        if (strcmp("T", temp_buffer) == 0) {
          snprintf(show_battery, sizeof(show_battery), "%s", "T");
        } else {
          snprintf(show_battery, sizeof(show_battery), "%s", "F");          
        }
        layer_set_hidden(s_battery_layer, strcmp(show_battery, "F") == 0);
        persist_write_string(KEY_SHOW_BATTERY, show_battery);
        break;
      case KEY_NOON_LOW:
        snprintf(temp_buffer, sizeof(temp_buffer), "%s", t->value->cstring);
        persist_write_bool(KEY_NOON_LOW, strcmp("T", temp_buffer) == 0);
        clock_conf.noon_low = strcmp("T", temp_buffer) == 0;
        sweep_math();
      default:
        APP_LOG(APP_LOG_LEVEL_ERROR, "Key %d not recognized!", (int)t->key);
        break;
    }

    // Look for next item
    t = dict_read_next(iterator);
  }

}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}


static void init() {
  
//   persist_write_bool(TRUTH_KEY, true);
//   bool truth = persist_read_bool(TRUTH_KEY);
  
//   persist_write_int(DOUGLAS_KEY, 42);
//   int douglas_value = persist_read_int(DOUGLAS_KEY);
 
  clock_conf.hours_24 = true;
  
  // Load persistant data
  ////////////////////////
  
  // Noon Low or High
  if (persist_exists(KEY_NOON_LOW)) clock_conf.noon_low = persist_read_bool(KEY_NOON_LOW);
  else clock_conf.noon_low = true;
  
  // Show weather
  if (persist_exists(KEY_SHOW_WEATHER)) persist_read_string(KEY_SHOW_WEATHER, show_weather, sizeof(show_weather));
  else strcpy(show_weather, "T");

  // Hour arrow
  if (persist_exists(KEY_SHOW_HOUR_ARROW)) persist_read_string(KEY_SHOW_HOUR_ARROW, show_hour_arrow, sizeof(show_hour_arrow));
  else strcpy(show_hour_arrow, "T");
  // Battery
  if (persist_exists(KEY_SHOW_BATTERY)) persist_read_string(KEY_SHOW_BATTERY, show_battery, sizeof(show_battery));
  else strcpy(show_battery, "F");
    
  
  // Set colors
#ifdef PBL_COLOR
  char colorStr[20] = "0xFFFFFF";

  // Background
  if (persist_exists(KEY_COLOR_BACKGROUND)) persist_read_string(KEY_COLOR_BACKGROUND, colorStr, sizeof(colorStr));
  else strcpy(colorStr, "0x000000");  
  color[C_BACKGROUND] = GColorFromHEX(StringToHex(colorStr));
  // Text
  if (persist_exists(KEY_COLOR_TEXT)) persist_read_string(KEY_COLOR_TEXT, colorStr, sizeof(colorStr));
  else strcpy(colorStr, "0xFFFFFF");  
  color[C_TEXT] = GColorFromHEX(StringToHex(colorStr));
  // Text alt
  if (persist_exists(KEY_COLOR_TEXT_ALT)) persist_read_string(KEY_COLOR_TEXT_ALT, colorStr, sizeof(colorStr));
  else strcpy(colorStr, "0xFFFFFF");  
  color[C_TEXT_ALT] = GColorFromHEX(StringToHex(colorStr));
  // Face
  if (persist_exists(KEY_COLOR_FACE)) persist_read_string(KEY_COLOR_FACE, colorStr, sizeof(colorStr));
  else strcpy(colorStr, "0x000000");    
  color[C_FACE] = GColorFromHEX(StringToHex(colorStr));
  // Outline
  if (persist_exists(KEY_COLOR_OUTLINE)) persist_read_string(KEY_COLOR_OUTLINE, colorStr, sizeof(colorStr));
  else strcpy(colorStr, "0x0055FF");
  color[C_OUTLINE] = GColorFromHEX(StringToHex(colorStr));
  // RiseSet
  if (persist_exists(KEY_COLOR_RISESET)) persist_read_string(KEY_COLOR_RISESET, colorStr, sizeof(colorStr));
  else strcpy(colorStr, "0x0055FF");  
  color[C_RISESET] = GColorFromHEX(StringToHex(colorStr));
  // Sweep
  if (persist_exists(KEY_COLOR_SWEEP)) persist_read_string(KEY_COLOR_SWEEP, colorStr, sizeof(colorStr));
  else strcpy(colorStr, "0x0055FF");
  color[C_SWEEP] = GColorFromHEX(StringToHex(colorStr));
  // Apex
  if (persist_exists(KEY_COLOR_APEX)) persist_read_string(KEY_COLOR_APEX, colorStr, sizeof(colorStr));
  else strcpy(colorStr, "0xFFFFFF");  
  color[C_APEX] = GColorFromHEX(StringToHex(colorStr));
  // Ticks
  if (persist_exists(KEY_COLOR_TICKS)) persist_read_string(KEY_COLOR_TICKS, colorStr, sizeof(colorStr));
  else strcpy(colorStr, "0xFFFFFF");  
  color[C_TICKS] = GColorFromHEX(StringToHex(colorStr));
#else
  color[C_BACKGROUND] = GColorBlack;
  color[C_TEXT] = GColorWhite;
  color[C_TEXT_ALT] = GColorWhite;
  color[C_FACE] = GColorBlack;
  color[C_OUTLINE] = GColorWhite;
  color[C_RISESET] = GColorWhite;
  color[C_SWEEP] = GColorWhite;
  color[C_APEX] = GColorWhite;
  color[C_TICKS] = GColorWhite;  
#endif
  
  
  // Set sweep starting arcs
  min_a = TRIG_MAX_ANGLE * 0 - angle_90;
  hour_a = TRIG_MAX_ANGLE * 0 + (clock_conf.noon_low ? angle_90: -angle_90);
  
 
  // init hand paths
  s_hour_arrow = gpath_create(&HOUR_HAND_POINTS);
  GRect bounds = GRect(57,0,86,86);
  GPoint center = grect_center_point(&bounds);
  gpath_move_to(s_hour_arrow, center);  

  // init sunrise/set paths
  s_sunrise_arrow = gpath_create(&SUN_HAND_POINTS);
  gpath_move_to(s_sunrise_arrow, center);  
  s_sunset_arrow = gpath_create(&SUN_HAND_POINTS);
  gpath_move_to(s_sunset_arrow, center);  

  
  // Register callbacks
  app_message_register_inbox_received(inbox_received_callback);
  // Open AppMessage
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
  // Register callbacks  
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);  

  
  // Create main Window element and assign to pointer
  s_main_window = window_create();
  window_set_background_color(s_main_window, color[C_BACKGROUND]);
  
  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);
  
  // Make sure the time is displayed from the start
  update_time();
  
  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
 
  // Register for battery level updates
  battery_state_service_subscribe(battery_callback);  
  // Ensure battery level is displayed from the start
  battery_callback(battery_state_service_peek());
 
  // Register for Bluetooth connection updates
  bluetooth_connection_service_subscribe(bluetooth_callback);
  
}



static void deinit() {
  gpath_destroy(s_hour_arrow);
  gpath_destroy(s_sunrise_arrow);
  gpath_destroy(s_sunset_arrow);  
  // Destroy Window
  window_destroy(s_main_window);
}



int main(void) {
  init();
  app_event_loop();
  deinit();
}