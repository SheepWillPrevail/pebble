#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

// 8c536740-aaa8-11e2-9e96-0800200c9a66
#define MY_UUID { 0x8c, 0x53, 0x67, 0x40, 0xAA, 0xA8, 0x11, 0xE2, 0x9E, 0x96, 0x08, 0x00, 0x20, 0x0C, 0x9A, 0x66 }
PBL_APP_INFO(MY_UUID,
			 "VeryPlain", "sWp",
			 1, 1, /* App version */
			 RESOURCE_ID_IMAGE_MENU_ICON,
			 APP_INFO_WATCH_FACE);

Window window;
Layer hour_display_layer, minute_display_layer, second_display_layer;
GPath hour_hand_path, minute_hand_path, second_hand_path;

const GPathInfo HOUR_HAND_PATH_POINTS = {
	4,
	(GPoint []) {
		{-4, 4},
		{4, 4},
		{4, -35},
		{-4,  -35},
	}
};

const GPathInfo MINUTE_HAND_PATH_POINTS = {
	4,
	(GPoint []) {
		{-4, 4},
		{4, 4},
		{4, -70},
		{-4,  -70},
	}
};

const GPathInfo SECOND_HAND_PATH_POINTS = {
	4,
	(GPoint []) {
		{-2, 2},
		{2, 2},
		{2, -120},
		{-2,  -120},
	}
};

void initLayerPathAndCenter(Layer *layer, GPath *path, const GPathInfo *pathInfo, const void *updateProc) {
	layer_init(layer, window.layer.frame);
	layer->update_proc = updateProc;
	layer_add_child(&window.layer, layer);
	gpath_init(path, pathInfo);
	gpath_move_to(path, grect_center_point(&layer->frame));
}

void hour_display_layer_update_callback(Layer *me, GContext* ctx) {
	(void)me;

	PblTm t;
	get_time(&t);

	unsigned int angle = (t.tm_hour * 30) + (t.tm_min / 2);
	gpath_rotate_to(&hour_hand_path, (TRIG_MAX_ANGLE / 360) * angle);

	graphics_context_set_fill_color(ctx, GColorWhite);
	gpath_draw_filled(ctx, &hour_hand_path);
}

void minute_display_layer_update_callback(Layer *me, GContext* ctx) {
	(void)me;

	PblTm t;
	get_time(&t);

	unsigned int angle = (t.tm_min * 6) + (t.tm_sec / 10);
	gpath_rotate_to(&minute_hand_path, (TRIG_MAX_ANGLE / 360) * angle);

	graphics_context_set_fill_color(ctx, GColorWhite);
	gpath_draw_filled(ctx, &minute_hand_path);
}

void second_display_layer_update_callback(Layer *me, GContext* ctx) {
	(void)me;

	PblTm t;
	get_time(&t);

	unsigned int angle = t.tm_sec * 6;
	gpath_rotate_to(&second_hand_path, (TRIG_MAX_ANGLE / 360) * angle);

	graphics_context_set_fill_color(ctx, GColorWhite);
	gpath_draw_filled(ctx, &second_hand_path);
}

void handle_init(AppContextRef ctx) {
	(void)ctx;

	window_init(&window, "VeryPlain");
	window_stack_push(&window, true);
	window_set_background_color(&window, GColorBlack);

	resource_init_current_app(&APP_RESOURCES);

	initLayerPathAndCenter(&hour_display_layer, &hour_hand_path, &HOUR_HAND_PATH_POINTS, &hour_display_layer_update_callback);
	initLayerPathAndCenter(&minute_display_layer, &minute_hand_path, &MINUTE_HAND_PATH_POINTS, &minute_display_layer_update_callback);
	//initLayerPathAndCenter(&second_display_layer, &second_hand_path, &SECOND_HAND_PATH_POINTS, &second_display_layer_update_callback);
}

void handle_minute_tick(AppContextRef ctx, PebbleTickEvent *t){
	(void)t;
	(void)ctx;

	if(t->tick_time->tm_min%2==0) {
		layer_mark_dirty(&hour_display_layer);
	}
	layer_mark_dirty(&minute_display_layer);
}

void pbl_main(void *params) {
	PebbleAppHandlers handlers = {
		.init_handler = &handle_init,
		.tick_info = {
			.tick_handler = &handle_minute_tick,
			.tick_units = MINUTE_UNIT //SECOND_UNIT
		}
	};
	app_event_loop(params, &handlers);
}
