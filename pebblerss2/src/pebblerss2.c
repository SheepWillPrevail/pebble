#include "pebble.h"
#include "pebble_fonts.h"
#include "resource_ids.auto.h"

#define TITLE_SIZE 96 + 1
#define CHUNK_BUFFER_SIZE 3024

AppTimer *command_timer = NULL;
BitmapLayer *refresh_layer = NULL;
Window *window[4];
MenuLayer *menu_layer[2];
TextLayer *messagetext_layer;
ScrollLayer *message_layer;
BitmapLayer *image_layer;
GBitmap *refresh_bitmap;
GBitmap image;

char feed_names[16][TITLE_SIZE], item_names[128][TITLE_SIZE];
char chunk_buffer[CHUNK_BUFFER_SIZE];

int current_level = 0, current_slot = 0, current_command = 0;
int feed_receive_idx = 0, item_receive_idx = 0, message_receive_idx = 0, chunk_receive_idx = 0;
int feed_count = 0, item_count = 0;
int selected_item_id = 0, has_thumbnail = 0; 
int fontfeed = 0, fontitem = 0, fontmessage = 0, cellheight = 0;
int imagew, imageh, imageb;

void handle_resend(void* data);

void request_command(int slot, int command) {
	AppMessageResult result;
	DictionaryIterator *dict;
	result = app_message_outbox_begin(&dict);
	if (result == APP_MSG_OK) {
		dict_write_uint8(dict, slot, command);
		dict_write_end(dict);
		result = app_message_outbox_send();
		if (result == APP_MSG_OK) {
			app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
			if (command_timer != NULL) {
				app_timer_cancel(command_timer);
				command_timer = NULL;
			}
		}
	}
	else {
		current_slot = slot;
		current_command = command;
		if (command_timer == NULL)
			command_timer = app_timer_register(250, handle_resend, NULL);
	}
}

void handle_resend(void* data) {
	request_command(current_slot, current_command);
}

char* fontid2resource(int id) {
	switch (id) {
	case 0:
		return FONT_KEY_GOTHIC_14;
	case 1:
		return FONT_KEY_GOTHIC_14_BOLD;
	case 2:
		return FONT_KEY_GOTHIC_18;
	case 3:
		return FONT_KEY_GOTHIC_18_BOLD;
	case 4:
		return FONT_KEY_GOTHIC_24;
	case 5:
		return FONT_KEY_GOTHIC_24_BOLD;
	case 6:
		return FONT_KEY_GOTHIC_28;
	case 7:
		return FONT_KEY_GOTHIC_28_BOLD;
	case 8:
		return FONT_KEY_BITHAM_30_BLACK;
	case 9:
		return FONT_KEY_BITHAM_42_BOLD;
	case 10:
		return FONT_KEY_BITHAM_42_LIGHT;
	case 11:
		return FONT_KEY_ROBOTO_CONDENSED_21;
	case 12:
		return FONT_KEY_DROID_SERIF_28_BOLD;
	}
	return 0;
}

uint16_t menu_get_num_rows_callback(MenuLayer *me, uint16_t section_index, void *data) {
	switch (current_level) {
	case 0:
		return feed_count;
	case 1:
		return item_count;
	default:
		return 0;
	}
}

int16_t menu_get_cell_height_callback(MenuLayer *me, MenuIndex *cell_index, void *data) {
	return cellheight;
}

void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
	graphics_context_set_text_color(ctx, GColorBlack);
	GRect bounds = layer_get_bounds(cell_layer);
	switch (current_level) {
	case 0:
		graphics_draw_text(ctx, feed_names[cell_index->row], fonts_get_system_font(fontid2resource(fontfeed)), bounds, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
		break;
	case 1:
		graphics_draw_text(ctx, item_names[cell_index->row], fonts_get_system_font(fontid2resource(fontitem)), bounds, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
		break;	
	}
}

Window* get_window();

void push_new_level() {
	current_level++;
	window[current_level] = get_window();
}

void show_loading() {
	Layer *window_layer = window_get_root_layer(window[current_level]);
	if (refresh_layer == NULL) {
		refresh_layer = bitmap_layer_create(layer_get_bounds(window_layer));
		refresh_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_REFRESH);
		bitmap_layer_set_alignment(refresh_layer, GAlignCenter);
		bitmap_layer_set_background_color(refresh_layer, GColorClear);
		bitmap_layer_set_bitmap(refresh_layer, refresh_bitmap);
		layer_add_child(window_layer, bitmap_layer_get_layer(refresh_layer));
	}
	layer_set_hidden(bitmap_layer_get_layer(refresh_layer), false);
}

void hide_loading() {
	if (refresh_layer != NULL)
		layer_set_hidden(bitmap_layer_get_layer(refresh_layer), true);
}

void menu_select_callback(MenuLayer *me, MenuIndex *cell_index, void *data) {
	if (current_level == 0 && feed_count == 0) return;
	if (current_level == 1 && item_count == 0) return;

	push_new_level();
	
	switch (current_level) {
	case 1:
		item_receive_idx = 0; item_count = 0;
		request_command(1091, cell_index->row);
		break;
	case 2:
		message_receive_idx = 0;
		selected_item_id = cell_index->row;
		request_command(1092, selected_item_id);
		break;
	}
}

void item_long_click(ClickRecognizerRef recognizer, void *context) {
	if (chunk_receive_idx == 0)
		request_command(1093, selected_item_id);
}

void image_back_click(ClickRecognizerRef recognizer, void *context) {
	if (chunk_receive_idx == 0)
		window_stack_pop(true);
}

void image_click_config_provider(void* context) {
	window_single_click_subscribe(BUTTON_ID_BACK, image_back_click);
	window_long_click_subscribe(BUTTON_ID_SELECT, 500, item_long_click, NULL);
}

void message_click(ClickRecognizerRef recognizer, void *context) {
	if (has_thumbnail && chunk_receive_idx == 0) {
		push_new_level();
		request_command(1094, selected_item_id);
	}
}

void message_click_config_provider(void* context) {
	window_single_click_subscribe(BUTTON_ID_SELECT, message_click);
	window_long_click_subscribe(BUTTON_ID_SELECT, 500, item_long_click, NULL);
}

void window_load(Window *me) {
	Layer *window_layer = window_get_root_layer(me);
	switch (current_level) {
	case 0:
	case 1:
		if (current_level == 1)
			for (int i = 0; i < item_count; i++)
				memset(item_names[i], 0, TITLE_SIZE);
		menu_layer[current_level] = menu_layer_create(layer_get_bounds(window_layer));
		menu_layer_set_callbacks(menu_layer[current_level], NULL, (MenuLayerCallbacks){
			.get_cell_height = menu_get_cell_height_callback,
			.get_num_rows = menu_get_num_rows_callback,
			.draw_row = menu_draw_row_callback,
			.select_click = menu_select_callback,
		});
		menu_layer_set_click_config_onto_window(menu_layer[current_level], me);
		layer_add_child(window_layer, menu_layer_get_layer(menu_layer[current_level]));
		break;
	case 2:
		messagetext_layer = text_layer_create(GRect(0, 0, 144, 1024));
		text_layer_set_text(messagetext_layer, chunk_buffer);
		message_layer = scroll_layer_create(layer_get_bounds(window_layer));
		scroll_layer_add_child(message_layer, text_layer_get_layer(messagetext_layer));
		scroll_layer_set_callbacks(message_layer, (ScrollLayerCallbacks){
			.click_config_provider = message_click_config_provider
		});
		scroll_layer_set_click_config_onto_window(message_layer, me);
		layer_set_hidden(scroll_layer_get_layer(message_layer), true);
		layer_add_child(window_layer, scroll_layer_get_layer(message_layer));
		break;
	case 3:
		memset(chunk_buffer, 0, CHUNK_BUFFER_SIZE);
		image_layer = bitmap_layer_create(layer_get_bounds(window_layer));
		bitmap_layer_set_alignment(image_layer, GAlignCenter);
		bitmap_layer_set_background_color(image_layer, GColorClear);
		bitmap_layer_set_bitmap(image_layer, &image);
		layer_set_hidden(bitmap_layer_get_layer(image_layer), true);
		layer_add_child(window_layer, bitmap_layer_get_layer(image_layer));
		window_set_click_config_provider(me, image_click_config_provider);
		break;
	}
}

void window_unload(Window *me) {
	switch (current_level) {
	case 1: // back to feed list
		if (feed_receive_idx != feed_count) request_command(1090, 2); // continue loading
		menu_layer_destroy(menu_layer[1]);
		break;
	case 2: // back to item list
		if (item_receive_idx != item_count) request_command(1090, 3); // continue loading
		scroll_layer_destroy(message_layer);
		text_layer_destroy(messagetext_layer);
		break;
	case 3: // back to message
		request_command(1092, selected_item_id);		
		bitmap_layer_destroy(image_layer);
		layer_set_hidden(scroll_layer_get_layer(message_layer), true);		
	}	
	current_level--;
}

Window* get_window() {
	Window* me = window[current_level];
	if (!me) me = window_create();
	window_set_window_handlers(me, (WindowHandlers){
		.load = window_load,
		.unload = window_unload,
	});
	window_stack_push(me, true);
	return me;
}

void throttle() {
	app_comm_set_sniff_interval(SNIFF_INTERVAL_NORMAL);
}

void ack() {
	request_command(1090, 1);
}

void show_messagetext_layer() {
	text_layer_set_font(messagetext_layer, fonts_get_system_font(fontid2resource(fontmessage)));
	GSize size = text_layer_get_content_size(messagetext_layer);
	size.h += 4;
	scroll_layer_set_content_offset(message_layer, GPoint(0, 0), false);
	scroll_layer_set_content_size(message_layer, size);
	layer_mark_dirty(scroll_layer_get_layer(message_layer));
	layer_set_hidden(scroll_layer_get_layer(message_layer), false);
}

void msg_in_rcv_handler(DictionaryIterator *received, void *context) {
	Tuple *feed_tuple = dict_find(received, 1001);
	if (feed_tuple) {
		Tuple *total = dict_find(received, 1011);
		Tuple *offset = dict_find(received, 1012);

		memcpy(&feed_names[offset->value->uint8], feed_tuple->value->cstring, feed_tuple->length);

		if (feed_receive_idx == 0) {
			feed_count = total->value->uint8;
			menu_layer_reload_data(menu_layer[0]);
			hide_loading();
		}

		layer_mark_dirty(menu_layer_get_layer(menu_layer[0]));	

		if (current_level == 0) {
			if (++feed_receive_idx == feed_count) // received all
				throttle();
			else request_command(1090, 2);
		}
	}  

	Tuple *item_tuple = dict_find(received, 1002);
	if (item_tuple) {
		Tuple *total = dict_find(received, 1021);
		Tuple *offset = dict_find(received, 1022);

		memcpy(&item_names[offset->value->uint8], item_tuple->value->cstring, item_tuple->length);

		if (item_receive_idx == 0) {
			item_count = total->value->uint8;
			menu_layer_reload_data(menu_layer[1]);
		}

		layer_mark_dirty(menu_layer_get_layer(menu_layer[1]));

		if (current_level == 1) {
			if (++item_receive_idx == item_count) // received all
				throttle();
			else request_command(1090, 3);
		}
	}

	Tuple *font_feed = dict_find(received, 1013);
	if (font_feed) {
		Tuple *font_item = dict_find(received, 1014);
		Tuple *font_message = dict_find(received, 1015);
		Tuple *cell_height = dict_find(received, 1016);
		fontfeed = font_feed->value->uint8;
		fontitem = font_item->value->uint8;
		fontmessage = font_message->value->uint8;
		cellheight = cell_height->value->uint8;	
		menu_layer_reload_data(menu_layer[0]);
		layer_mark_dirty(menu_layer_get_layer(menu_layer[0]));	
		if (current_level > 0) {
			menu_layer_reload_data(menu_layer[1]);	
			layer_mark_dirty(menu_layer_get_layer(menu_layer[1]));
		}
		if (current_level > 1)
			show_messagetext_layer();
	}

	Tuple *refresh_packet = dict_find(received, 1017);
	if (refresh_packet && current_level == 0 && feed_receive_idx == 0)
		show_loading();
	
	Tuple *image_w_packet = dict_find(received, 1018);
	if (image_w_packet) {
		Tuple *image_h_packet = dict_find(received, 1019);
		Tuple *image_b_packet = dict_find(received, 1020);		
		imagew = image_w_packet->value->uint16;
		imageh = image_h_packet->value->uint16;
		imageb = image_b_packet->value->uint8;
		image = (GBitmap) {
		  .addr = &chunk_buffer,
		  .bounds = GRect(0, 0, imagew, imageh),
		  .info_flags = 1,
		  .row_size_bytes = imageb		
		};
		ack();
	}
	
	Tuple *chunk_d_packet = dict_find(received, 9999);
	if (chunk_d_packet) {
		Tuple *chunk_o_packet = dict_find(received, 9998);
		Tuple *chunk_l_packet = dict_find(received, 9997);
		Tuple *chunk_t_packet = dict_find(received, 9996);
		
		if (chunk_receive_idx == 0) {
			show_loading();
			memset(chunk_buffer, 0, CHUNK_BUFFER_SIZE);			
			if (current_level == 3)
				layer_set_hidden(bitmap_layer_get_layer(image_layer), false);
		}

		memcpy(&chunk_buffer[chunk_o_packet->value->uint16], chunk_d_packet->value->data, chunk_l_packet->value->uint8);

		if (++chunk_receive_idx == chunk_t_packet->value->uint8) {
			chunk_receive_idx = 0;
			throttle();			
			hide_loading();
			if (current_level == 2)
				show_messagetext_layer();			
		}
		else ack();		
		
		if (current_level == 3)
			layer_mark_dirty(bitmap_layer_get_layer(image_layer));
	}
	
	Tuple *item_status = dict_find(received, 1023);
	if (item_status) {
	  has_thumbnail = item_status->value->uint8;
	  ack();
	}
}

void handle_init() {
	app_message_register_inbox_received(msg_in_rcv_handler);
	app_message_open(124, 32);
	window[0] = get_window();
	request_command(1090, 0); // hello
}

void handle_deinit() {
	if (refresh_layer != NULL) {
		 bitmap_layer_destroy(refresh_layer);
		 gbitmap_destroy(refresh_bitmap);
	}
	menu_layer_destroy(menu_layer[0]);
	for (int i = 0; i < 4; i++)
		if (window[i]) window_destroy(window[i]);
}

int main() {
	handle_init();
	app_event_loop();
	handle_deinit();
}
