#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

#include "resource_ids.auto.h"

#define MY_NAME "SimpleState"
#define MY_UUID { 0x19, 0x41, 0xE6, 0x14, 0x91, 0x63, 0x49, 0xBD, 0xBA, 0x01, 0x6D, 0x7F, 0xA7, 0x1E, 0xED, 0xAB }
PBL_APP_INFO(MY_UUID,
             MY_NAME, "sWp",
             1, 0, /* App version */
             DEFAULT_MENU_ICON,
             APP_INFO_WATCH_FACE);

#define REACTOR_COMMAND     810401000
#define REACTOR_PHONEBATTERYLEVEL 810401001
#define REACTOR_PHONERADIOSTATE   810401002
#define REACTOR_PHONENETWORKTYPE  810401003
#define REACTOR_PHONESIGNALLEVEL  810401004
#define REACTOR_NUMBERMISSEDCALLS 810401005
#define REACTOR_NUMBERUNREADSMS   810401006
#define REACTOR_NUMBERCELLRX      810401007
#define REACTOR_NUMBERCELLTX      810401008
#define REACTOR_COMMAND_REFRESH   0

Window window;
GFont ubuntu_time, ubuntu_date, ubuntu_text;
TextLayer layer_time, layer_date, layer_text, layer_numbercall, layer_numbermessage;
BitmapLayer layer_battery, layer_signal;
HeapBitmap bitmap_battery, bitmap_signal; // dynamic images
BmpContainer layer_call, layer_message; // static images
char string_time[6], string_date[11], string_call[8], string_message[8], *format_time, *format_date;
int level_battery = 0, level_signal = 0, number_call = 0, number_message = 0;

void update_time() {
  PblTm time;
  
  get_time(&time);
  string_format_time(string_time, sizeof(string_time), format_time, &time);
  string_format_time(string_date, sizeof(string_date), format_date, &time);
}

void request_command(int command) {
  DictionaryIterator *dict;
  if (app_message_out_get(&dict) == APP_MSG_OK) {
	dict_write_uint8(dict, REACTOR_COMMAND, command);
	app_message_out_send();
	app_message_out_release();
  }
}

void update_stats() {
  snprintf(string_call, sizeof(string_call), "%d", number_call);
  layer_mark_dirty(&layer_numbercall.layer);

  snprintf(string_message, sizeof(string_message), "%d", number_message);
  layer_mark_dirty(&layer_numbermessage.layer);
  
  int battery_id;
  if (level_battery > 0)
    battery_id = RESOURCE_ID_PNG_BATTERY_20;	
  if (level_battery > 20)
    battery_id = RESOURCE_ID_PNG_BATTERY_40;
  if (level_battery > 40)
    battery_id = RESOURCE_ID_PNG_BATTERY_60;
  if (level_battery > 60)
    battery_id = RESOURCE_ID_PNG_BATTERY_80;
  if (level_battery > 80)
    battery_id = RESOURCE_ID_PNG_BATTERY_100;  
  if (&bitmap_battery.bmp)
    heap_bitmap_deinit(&bitmap_battery);
  heap_bitmap_init(&bitmap_battery, battery_id);
  layer_mark_dirty(&layer_battery.layer);
  
  int signal_id;
  if (level_signal > 0)
    signal_id = RESOURCE_ID_PNG_SIGNAL_20;	
  if (level_signal > 20)
    signal_id = RESOURCE_ID_PNG_SIGNAL_40;
  if (level_signal > 40)
    signal_id = RESOURCE_ID_PNG_SIGNAL_60;
  if (level_signal > 60)
    signal_id = RESOURCE_ID_PNG_SIGNAL_80;
  if (level_signal > 80)
    signal_id = RESOURCE_ID_PNG_SIGNAL_100;
  if (&bitmap_battery.bmp)
    heap_bitmap_deinit(&bitmap_signal);
  heap_bitmap_init(&bitmap_signal, signal_id);
  layer_mark_dirty(&layer_signal.layer);  
}

void handle_init(AppContextRef ctx) { 
  if (clock_is_24h_style()) {
    format_time = "%H:%M";
    format_date = "%d-%m-%Y";
  }
  else {
    format_time = "%I:%M";
    format_date = "%m/%d/%Y";
  }
  
  update_time();

  resource_init_current_app(&APP_RESOURCES);
  ubuntu_time = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_UBUNTU_BOLD_50));
  ubuntu_date = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_UBUNTU_16));
  ubuntu_text = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_UBUNTU_10));

  window_init(&window, MY_NAME);
  window_stack_push(&window, true);

  text_layer_init(&layer_time, GRect(0, 20, 144, 54));
  text_layer_set_font(&layer_time, ubuntu_time);
  text_layer_set_text(&layer_time, (const char*)&string_time);
  text_layer_set_text_alignment(&layer_time, GTextAlignmentCenter);
  layer_add_child(window_get_root_layer(&window), &layer_time.layer);

  text_layer_init(&layer_date, GRect(0, 74, 144, 20));
  text_layer_set_font(&layer_date, ubuntu_date);
  text_layer_set_text(&layer_date, (const char*)&string_date);
  text_layer_set_text_alignment(&layer_date, GTextAlignmentCenter);
  layer_add_child(window_get_root_layer(&window), &layer_date.layer);
    
  bitmap_layer_init(&layer_battery, GRect(8, 140, 14, 8));
  bitmap_layer_set_bitmap(&layer_battery, &bitmap_battery.bmp);
  layer_add_child(window_get_root_layer(&window), &layer_battery.layer);

  bitmap_layer_init(&layer_signal, GRect(8, 150, 14, 8));
  bitmap_layer_set_bitmap(&layer_signal, &bitmap_signal.bmp);
  layer_add_child(window_get_root_layer(&window), &layer_signal.layer);
  
  bmp_init_container(RESOURCE_ID_PNG_CALL, &layer_call);
  layer_call.layer.layer.frame.origin.x = 142 - 8 - 9;
  layer_call.layer.layer.frame.origin.y = 140;
  layer_add_child(window_get_root_layer(&window), &layer_call.layer.layer);

  bmp_init_container(RESOURCE_ID_PNG_MESSAGE, &layer_message);
  layer_message.layer.layer.frame.origin.x = 142 - 8 - 9;
  layer_message.layer.layer.frame.origin.y = 150;
  layer_add_child(window_get_root_layer(&window), &layer_message.layer.layer);
  
  text_layer_init(&layer_numbercall, GRect(101, 138, 22, 10));
  text_layer_set_font(&layer_numbercall, ubuntu_text);
  text_layer_set_text(&layer_numbercall, (const char*)&string_call);
  text_layer_set_text_alignment(&layer_numbercall, GTextAlignmentRight);
  layer_add_child(window_get_root_layer(&window), &layer_numbercall.layer);

  text_layer_init(&layer_numbermessage, GRect(101, 148, 22, 10));
  text_layer_set_font(&layer_numbermessage, ubuntu_text);
  text_layer_set_text(&layer_numbermessage, (const char*)&string_message);
  text_layer_set_text_alignment(&layer_numbermessage, GTextAlignmentRight);
  layer_add_child(window_get_root_layer(&window), &layer_numbermessage.layer);  
  
  update_stats();
 
  request_command(REACTOR_COMMAND_REFRESH);
}

void handle_deinit(AppContextRef ctx) {
  //text_layer_deinit(&layer_time);
  //text_layer_deinit(&layer_date);
  
  heap_bitmap_deinit(&bitmap_battery);
  heap_bitmap_deinit(&bitmap_signal);
  
  bmp_deinit_container(&layer_call);  
  bmp_deinit_container(&layer_message);

  fonts_unload_custom_font(ubuntu_time);
  fonts_unload_custom_font(ubuntu_date);
  fonts_unload_custom_font(ubuntu_text);
}

void handle_tick(AppContextRef ctx, PebbleTickEvent *event) {
  update_time();
  layer_mark_dirty(&layer_time.layer);
  layer_mark_dirty(&layer_date.layer);
}

void msg_in_rcv_handler(DictionaryIterator *received, void *context) {
  Tuple *battery = dict_find(received, REACTOR_PHONEBATTERYLEVEL);
  if (battery) {
	level_battery = battery->value->uint8;
  }
  Tuple *signal = dict_find(received, REACTOR_PHONESIGNALLEVEL);
  if (signal) {
    level_signal = signal->value->uint8;
  }
  Tuple *calls = dict_find(received, REACTOR_NUMBERMISSEDCALLS);
  if (calls) {
    number_call = calls->value->uint8;
  }
  Tuple *sms = dict_find(received, REACTOR_NUMBERUNREADSMS);
  if (sms) {
    number_message = sms->value->uint8;
  }
  
  update_stats();
}

void msg_in_drp_handler(void *context, AppMessageResult reason) {
}

void pbl_main(void *params) {
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
	.deinit_handler = &handle_deinit,
	.tick_info = {
     .tick_handler = &handle_tick,
     .tick_units = MINUTE_UNIT
    },
	.messaging_info = {
      .buffer_sizes = {
        .inbound = 124,
        .outbound = 124,
      },
	  .default_callbacks.callbacks = {
        .in_received = msg_in_rcv_handler,
        .in_dropped = msg_in_drp_handler
      }
	}
  };
  app_event_loop(params, &handlers);
}
