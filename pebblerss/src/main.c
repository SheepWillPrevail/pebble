#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

#include "resource_ids.auto.h"

#define MY_NAME "Pebble RSS"
#define MY_UUID { 0x19, 0x41, 0xE6, 0x14, 0x91, 0x63, 0x49, 0xBD, 0xBA, 0x01, 0x6D, 0x7F, 0xA7, 0x1E, 0xED, 0xAC }
PBL_APP_INFO(MY_UUID,
             MY_NAME, "sWp",
             1, 0, /* App version */
             DEFAULT_MENU_ICON,
             APP_INFO_STANDARD_APP);

Window window[3];
MenuLayer menu_layer[2];
ScrollLayer message_layer;
TextLayer messagetext_layer;
BmpContainer refresh_image;

#define TITLE_SIZE 96 + 1

int currentLevel = 0, feed_count = 0, item_count = 0, selected_item_id = 0;
int feed_receive_idx = 0, item_receive_idx = 0, message_receive_idx = 0;
char feed_names[48][TITLE_SIZE], item_names[128][TITLE_SIZE];
char message[2001];
int fontfeed = 0, fontitem = 0, fontmessage = 0, cellheight = 0;

AppContextRef app;
int pslot, pcommand;

void request_command(int slot, int command) {
  pslot = slot;
  pcommand = command;
  app_timer_send_event(app, 25, 0);
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

void throttle() {
  app_comm_set_sniff_interval(SNIFF_INTERVAL_NORMAL);
}

uint16_t menu_get_num_rows_callback(MenuLayer *me, uint16_t section_index, void *data) {
  switch (currentLevel) {
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
  GRect bounds = cell_layer->bounds;
  switch (currentLevel) {
  case 0:
	graphics_text_draw(ctx, feed_names[cell_index->row], fonts_get_system_font(fontid2resource(fontfeed)), bounds, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
	break;
  case 1:
	graphics_text_draw(ctx, item_names[cell_index->row], fonts_get_system_font(fontid2resource(fontitem)), bounds, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
    break;	
  }
}

void setup_window(Window *me); // ugh

void menu_select_callback(MenuLayer *me, MenuIndex *cell_index, void *data) {
  if (currentLevel == 0 && feed_count == 0) return;
  if (currentLevel == 1 && item_count == 0) return;
   
  currentLevel++;
  
  setup_window(&window[currentLevel]);

  switch (currentLevel) {
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

void message_click(ClickRecognizerRef recognizer, void *context) {
  request_command(1093, selected_item_id);
}

void message_click_config_provider(ClickConfig **config, void* context) {
  config[BUTTON_ID_SELECT]->click.handler = message_click;
}

void window_load(Window *me) {
  if (currentLevel < 2) { // feed or item
	menu_layer_init(&menu_layer[currentLevel], me->layer.bounds);		
    menu_layer_set_callbacks(&menu_layer[currentLevel], NULL, (MenuLayerCallbacks){
	  .get_cell_height = menu_get_cell_height_callback,
      .get_num_rows = menu_get_num_rows_callback,
      .draw_row = menu_draw_row_callback,
      .select_click = menu_select_callback,
    }); 
    menu_layer_set_click_config_onto_window(&menu_layer[currentLevel], me);
    layer_add_child(window_get_root_layer(me), menu_layer_get_layer(&menu_layer[currentLevel]));	
  }
  else { // message
    text_layer_init(&messagetext_layer, GRect(0, 0, 144, 2048));
	text_layer_set_font(&messagetext_layer, fonts_get_system_font(fontid2resource(fontmessage)));
	text_layer_set_text(&messagetext_layer, (const char*)&message);	
	scroll_layer_init(&message_layer, me->layer.bounds);
    scroll_layer_add_child(&message_layer, &messagetext_layer.layer);
	scroll_layer_set_callbacks(&message_layer, (ScrollLayerCallbacks){
	  .click_config_provider = message_click_config_provider
	});
	scroll_layer_set_click_config_onto_window(&message_layer, me);
	layer_add_child(window_get_root_layer(me), &message_layer.layer);
  }
}

void window_unload(Window *me) {
  switch (currentLevel) {
  case 1: // back to feed list
	if (feed_receive_idx != feed_count) request_command(1090, 2); // continue loading
	for (int i = 0; i < 128; i++)
	  item_names[i][0] = 0;
    break;
  case 2: // back to item list
	if (item_receive_idx != item_count) request_command(1090, 3); // continue loading
	message[0] = 0;
    break;      
  }
  currentLevel--;
}

void setup_window(Window *me) {
  window_init(me, MY_NAME);
  window_set_window_handlers(me, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(me, true);
}

void handle_init(AppContextRef ctx) {
  resource_init_current_app(&APP_RESOURCES);
  app = ctx;
  bmp_init_container(RESOURCE_ID_IMAGE_REFRESH, &refresh_image);
  refresh_image.layer.layer.frame.origin.x = 55;
  refresh_image.layer.layer.frame.origin.y = 58;
  setup_window(&window[0]);
  layer_add_child(window_get_root_layer(&window[0]), &refresh_image.layer.layer);
  layer_set_hidden(&refresh_image.layer.layer, true);
  request_command(1090, 0); // hello
}

void handle_deinit(AppContextRef ctx) {
  bmp_deinit_container(&refresh_image);
}

void send_ack() {
  request_command(1090, 1);
}

void msg_in_rcv_handler(DictionaryIterator *received, void *context) {
  Tuple *feed_tuple = dict_find(received, 1001);
  if (feed_tuple) {
    Tuple *total = dict_find(received, 1011);
    Tuple *offset = dict_find(received, 1012);
	feed_count = total->value->uint8;
	
    memcpy(&feed_names[offset->value->uint8], feed_tuple->value->cstring, feed_tuple->length);
	
	if (feed_receive_idx == 0) {
	  layer_set_hidden(&refresh_image.layer.layer, true);
	  feed_count = total->value->uint8;
	  menu_layer_reload_data(&menu_layer[0]);
	}
	
	layer_mark_dirty(menu_layer_get_layer(&menu_layer[0]));	
	
	if (++feed_receive_idx == total->value->uint8 && currentLevel == 0) // received all
	  throttle();
	else if (currentLevel == 0) request_command(1090, 2);
  }  
  
  Tuple *item_tuple = dict_find(received, 1002);
  if (item_tuple) { 
	Tuple *total = dict_find(received, 1011);
	Tuple *offset = dict_find(received, 1012);
	
    memcpy(&item_names[offset->value->uint8], item_tuple->value->cstring, item_tuple->length);
	
	if (item_receive_idx == 0) {
      item_count = total->value->uint8;
	  menu_layer_reload_data(&menu_layer[1]);
	}
	
	layer_mark_dirty(menu_layer_get_layer(&menu_layer[1]));	
	
	if (++item_receive_idx == total->value->uint8 && currentLevel == 1) // received all
	  throttle();
	else if (currentLevel == 1) request_command(1090, 3);
  }
  
  Tuple *message_tuple = dict_find(received, 1003);
  if (message_tuple) {
	Tuple *total = dict_find(received, 1011);
	Tuple *offset = dict_find(received, 1012);
	
    memcpy(&message[offset->value->uint16], message_tuple->value->cstring, message_tuple->length);	
	
	if (++message_receive_idx == total->value->uint8) {	// received all
      if (currentLevel == 2) throttle();
	  GSize size = text_layer_get_max_used_size(app_get_current_graphics_context(), &messagetext_layer);
	  size.h += 4;
 	  scroll_layer_set_content_size(&message_layer, size);
	  layer_mark_dirty(&messagetext_layer.layer);
	  layer_mark_dirty(&message_layer.layer);
	}
	else send_ack();	
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
	menu_layer_reload_data(&menu_layer[0]);
	layer_mark_dirty(menu_layer_get_layer(&menu_layer[0]));	
	if (currentLevel > 0) {
		menu_layer_reload_data(&menu_layer[1]);	
	    layer_mark_dirty(menu_layer_get_layer(&menu_layer[1]));		
	}
	if (currentLevel > 1) {
	    text_layer_set_font(&messagetext_layer, fonts_get_system_font(fontid2resource(fontmessage)));
		GSize size = text_layer_get_max_used_size(app_get_current_graphics_context(), &messagetext_layer);
	    size.h += 4;
 	    scroll_layer_set_content_size(&message_layer, size);
		scroll_layer_set_content_offset(&message_layer, GPoint(0, 0), true);
	    layer_mark_dirty(&messagetext_layer.layer);
	    layer_mark_dirty(&message_layer.layer);	
	}
  }
  
  Tuple *refresh_packet = dict_find(received, 1017);
  if (refresh_packet && currentLevel == 0 && (feed_receive_idx == 0)) {
     layer_set_hidden(&refresh_image.layer.layer, false);
  }
}

void handle_timer(AppContextRef app_ctx, AppTimerHandle handle, uint32_t cookie) {
  AppMessageResult result;
  DictionaryIterator *dict;
  result = app_message_out_get(&dict);
  if (result != APP_MSG_OK) {
    app_message_out_release();
    app_timer_send_event(app_ctx, 25, cookie);
    return;
  }
  dict_write_uint8(dict, pslot, pcommand);
  dict_write_end(dict);
  app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);  
  result = app_message_out_send();
  app_message_out_release();
  if (result != APP_MSG_OK) {
    app_timer_send_event(app_ctx, 25, cookie);
    return;
  }
}

void pbl_main(void *params) {
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
	.deinit_handler = &handle_deinit,
	.messaging_info = {
      .buffer_sizes = {
        .inbound = 256,
        .outbound = 256,
      },
	  .default_callbacks.callbacks = {
        .in_received = msg_in_rcv_handler
      }
	},
	.timer_handler = &handle_timer
  };
  app_event_loop(params, &handlers);
}
