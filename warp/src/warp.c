#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

#include "tinymt32.h"
#include "math_helper.h"

#define MY_NAME "Warp"
#define MY_UUID { 0x5B, 0x41, 0x73, 0x2E, 0x21, 0xA8, 0x41, 0xF0, 0xB0, 0xE2, 0xCA, 0x28, 0x83, 0x71, 0x4B, 0xE3 }
PBL_APP_INFO(MY_UUID, MY_NAME, "sWp", 1, 0, RESOURCE_ID_IMAGE_MENU_ICON, APP_INFO_WATCH_FACE);

#define DISPLAY_SLEEP 1000 / 15
#define STAR_COUNT 500
#define STAR_SIZE 5
#define EXTENT 50

typedef struct {
	Vec3 position;
	float velocity;
} GParticle;

Window window;
Layer star_layer;

GParticle stars[STAR_COUNT];
tinymt32_t random;
Mat4 vp;

float random_in_range(float min, float max) {
	return min + (tinymt32_generate_float01(&random) * (max - min));
}

void init_particle(GParticle *particle) {
	particle->position.x = random_in_range(-EXTENT * 2, EXTENT * 2);
	particle->position.y = random_in_range(-EXTENT * 2, EXTENT * 2);
	particle->position.z = -EXTENT + random_in_range(0, EXTENT * 2);
	particle->velocity = random_in_range(0.1f, 1);
}

void update_particle(GParticle *particle) {
	particle->position.z += particle->velocity;
	if (particle->position.z > EXTENT)
		init_particle(particle);
}

void init_stars() {
	int i;
	for (i = 0; i < STAR_COUNT; i++)
		init_particle(&stars[i]);
}

void update_stars() {
	int i;
	for (i = 0; i < STAR_COUNT; i++)
		update_particle(&stars[i]);
}

void update_star_layer(Layer *me, GContext *ctx) {
	(void)ctx;

	int i;
	Vec3 screen_space;
	float radius;

	graphics_context_set_fill_color(ctx, GColorWhite);
	for (i = 0; i < STAR_COUNT; i++) {
		mat4_multiply_vec3(&screen_space, &vp, &stars[i].position);
		radius = (screen_space.z / EXTENT * 2) * STAR_SIZE;

		if (screen_space.z > 0)
			graphics_fill_circle(ctx, GPoint(72 + screen_space.x, 84 + screen_space.y), radius);
	}
}

void handle_init(AppContextRef ctx) {
	(void)ctx;

	Vec3 eye = Vec3(0, 0, -1);
	Vec3 pos = Vec3(0, 0, 50);
	Vec3 up = Vec3(0, 1, 0);
	Mat4 view, projection;
	Layer *root;

	resource_init_current_app(&APP_RESOURCES);
	tinymt32_init(&random, (uint32_t)112);
	init_stars();

	MatrixLookAtRH(&view, &eye, &pos, &up);
	MatrixProjectionRH(&projection, EXTENT, -EXTENT, -EXTENT, EXTENT, -EXTENT, EXTENT);
	mat4_multiply(&vp, &view, &projection);

	window_init(&window, MY_NAME);
	window_set_background_color(&window, GColorBlack);

	root = window_get_root_layer(&window);
	layer_init(&star_layer, root->frame);
	star_layer.update_proc = &update_star_layer;
	layer_add_child(root, &star_layer);

	window_stack_push(&window, true);
	app_timer_send_event(ctx, DISPLAY_SLEEP, 0);
}

void handle_timer(AppContextRef ctx, AppTimerHandle handle, uint32_t cookie) {
	(void)ctx;
	(void)handle;

	update_stars();
	layer_mark_dirty(&star_layer);

	app_timer_send_event(ctx, DISPLAY_SLEEP, 0);
}

void pbl_main(void *params) {
	PebbleAppHandlers handlers = {
		.init_handler = &handle_init,
		.timer_handler = &handle_timer,
	};
	app_event_loop(params, &handlers);
}
