/*
 *	motion-filter, an OBS-Studio filter plugin for animating sources using 
 *	transform manipulation on the scene.
 *	Copyright(C) <2018>  <CatxFish>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License along
 *	with this program; if not, write to the Free Software Foundation, Inc.,
 *	51 Franklin Street, Fifth Floor, Boston, MA 02110 - 1301 USA.
 */

#include <obs-module.h>
#include <obs-hotkey.h>
#include <obs-scene.h>
#include <util/dstr.h>
#include "helper.h"

// Define property keys
#define S_PATH_LINEAR       0
#define S_PATH_QUADRATIC    1
#define S_PATH_CUBIC        2
#define S_IS_REVERSED       "is_reversed"
#define S_ORG_X             "org_x"
#define S_ORG_Y             "org_y"
#define S_ORG_W             "org_w"
#define S_ORG_H             "org_h"
#define S_PATH_TYPE         "path_type"
#define S_START_POS         "start_position"
#define S_START_SCALE       "start_scale"
#define S_CTRL_X            "ctrl_x"
#define S_CTRL_Y            "ctrl_y"
#define S_CTRL2_X           "ctrl2_x"
#define S_CTRL2_Y           "ctrl2_y"
#define S_DST_X             "dst_x"
#define S_DST_Y             "dst_y"
#define S_DST_W             "dst_w"
#define S_DST_H             "dst_h"
#define S_USE_DST_SCALE     "dst_use_scale"
#define S_DURATION          "duration"
#define S_SOURCE            "source_id"
#define S_FORWARD           "forward"
#define S_BACKWARD          "backward"
#define S_DEST_GRAB_POS     "use_cur_src_pos"
#define S_MOTION_BEHAVIOUR  "motion_behaviour"
#define S_MOTION_ONE_WAY    0
#define S_MOTION_ROUND_TRIP 1

// Define property localisation tags
#define T_(v)               obs_module_text(v)
#define T_PATH_TYPE         T_("PathType")
#define T_PATH_LINEAR       T_("PathType.Linear")
#define T_PATH_QUADRATIC    T_("PathType.Quadratic")
#define T_PATH_CUBIC        T_("PathType.Cubic")
#define T_START_POS         T_("Start.GivenPosition")
#define T_START_SCALE       T_("Start.GivenScale")
#define T_ORG_X             T_("Start.X")
#define T_ORG_Y             T_("Start.Y")
#define T_ORG_W             T_("Start.W")
#define T_ORG_H             T_("Start.H")
#define T_CTRL_X            T_("ControlPoint.X")
#define T_CTRL_Y            T_("ControlPoint.Y")
#define T_CTRL2_X           T_("ControlPoint2.X")
#define T_CTRL2_Y           T_("ControlPoint2.Y")
#define T_DST_X             T_("Destination.X")
#define T_DST_Y             T_("Destination.Y")
#define T_DST_W             T_("Destination.W")
#define T_DST_H             T_("Destination.H")
#define T_USE_DST_SCALE     T_("ChangeScale")
#define T_DURATION          T_("Duration")
#define T_SOURCE            T_("SourceName")
#define T_FORWARD           T_("Forward")
#define T_BACKWARD          T_("Backward")
#define T_DISABLED          T_("Disabled")
#define T_DEST_GRAB_POS     T_("DestinationGrabPosition")
#define T_MOTION_BEHAVIOUR  T_("Behaviour")
#define T_MOTION_ONE_WAY    T_("Behaviour.OneWay")
#define T_MOTION_ROUND_TRIP T_("Behaviour.RoundTrip")

typedef struct variation_data variation_data_t;
typedef struct motion_filter_data motion_filter_data_t;

struct variation_data {
	float               point_x[3];
	float               point_y[3];
	float               scale_x[2];
	float               scale_y[2];
	struct vec2         scale;
	struct vec2         position;	
	float               elapsed_time;
};

struct motion_filter_data {
	obs_source_t        *context;
	obs_scene_t         *scene;
	obs_sceneitem_t     *item;
	obs_hotkey_id       hotkey_id_f;
	obs_hotkey_id       hotkey_id_b;
	variation_data_t    variation;
	bool                hotkey_init;
	bool                restart_backward;
	bool                motion_start;
	bool                motion_reverse;
	bool                start_position;
	bool                start_scale;
	bool                use_dst_scale;
	int                 motion_behaviour;
	int                 path_type;
	int                 org_width;
	int                 org_height;
	int                 dst_width;
	int                 dst_height;
	struct vec2         org_pos;
	struct vec2         ctrl_pos;
	struct vec2         ctrl2_pos;
	struct vec2         dst_pos;
	float               duration;
	char                *item_name;
	int64_t             item_id;
};

static void update_variation_data(motion_filter_data_t *filter)
{
	variation_data_t *var = &filter->variation;

	if (check_item_basesize(filter->item))
		return ;

	if (!filter->motion_reverse) {
		struct obs_transform_info info;
		obs_sceneitem_get_info(filter->item, &info);
		if (!filter->start_position) {
			var->point_x[0] = info.pos.x;
			var->point_y[0] = info.pos.y;
		}
		if (!filter->start_scale) {
			var->scale_x[0] = info.scale.x;
			var->scale_y[0] = info.scale.y;
		}
	}

	if (filter->start_position){
		var->point_x[0] = filter->org_pos.x;
		var->point_y[0] = filter->org_pos.y;
	}

	if (filter->path_type >= S_PATH_QUADRATIC) {
		var->point_x[1] = filter->ctrl_pos.x;
		var->point_y[1] = filter->ctrl_pos.y;
	}
		
	if (filter->path_type == S_PATH_CUBIC) {
		var->point_x[2] = filter->ctrl2_pos.x;
		var->point_y[2] = filter->ctrl2_pos.y;
	}
		
	var->point_x[filter->path_type + 1] = filter->dst_pos.x;
	var->point_y[filter->path_type + 1] = filter->dst_pos.y;

	if(filter->start_scale) {
		cal_scale(filter->item, &var->scale_x[0],
			&var->scale_y[0], filter->org_width, filter->org_width);
	}

	cal_scale(filter->item, &var->scale_x[1],
		&var->scale_x[1], filter->dst_width, filter->dst_height);

	var->elapsed_time = 0.0f;
	return ;
}

static void reset_source_name(void *data, obs_sceneitem_t *item)
{
	motion_filter_data_t *filter = data;
	if (item) {
		obs_data_t *settings = obs_source_get_settings(filter->context);
		obs_source_t *item_source = obs_sceneitem_get_source(item);
		const char *name = obs_source_get_name(item_source);
		obs_data_set_string(settings, S_SOURCE, name);
		bfree(filter->item_name);
		filter->item_name = bstrdup(name);
		obs_data_release(settings);
	}
}

static void recover_source(motion_filter_data_t *filter)
{
	struct vec2 pos;
	struct vec2 scale;
	variation_data_t *var = &filter->variation;

	if (!filter->motion_reverse)
		return;

	pos.x = var->point_x[0];
	pos.y = var->point_y[0];
	scale.x = var->scale_x[0];
	scale.y = var->scale_y[0];

	obs_sceneitem_set_pos(filter->item, &pos);
	obs_sceneitem_set_scale(filter->item, &scale);
	filter->motion_reverse = false;
}

static bool motion_init(void *data, bool forward)
{
	motion_filter_data_t *filter = data;

	if (filter->motion_start || filter->motion_reverse == forward)
		return false;

	filter->item = get_item(filter->context, filter->item_name);

	if (!filter->item) {
		filter->item = get_item_by_id(filter->context, filter->item_id);
		reset_source_name(data, filter->item);
	}

	if (filter->item) {
		update_variation_data(filter);
		obs_sceneitem_addref(filter->item);
		filter->motion_start = true;
		return true;
	}
	return false;
}

static bool hotkey_forward(void *data, obs_hotkey_pair_id id,
	obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(pressed);
	return motion_init(data, true);
}

static bool hotkey_backward(void *data, obs_hotkey_pair_id id,
	obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(pressed);
	return motion_init(data, false);
}

static void set_reverse_info(struct motion_filter_data *filter)
{
	obs_data_t *settings = obs_source_get_settings(filter->context);
	obs_data_set_bool(settings, S_IS_REVERSED, filter->motion_reverse);
	obs_data_set_int(settings, S_ORG_X, (int)filter->org_pos.x);
	obs_data_set_int(settings, S_ORG_Y, (int)filter->org_pos.y);
	obs_data_set_int(settings, S_ORG_W, filter->org_width);
	obs_data_set_int(settings, S_ORG_H, filter->org_height);
	obs_data_release(settings);
}

static void motion_filter_update(void *data, obs_data_t *settings)
{
	motion_filter_data_t *filter = data;
	int64_t item_id;
	const char *item_name;
	
	filter->motion_behaviour = (int)obs_data_get_int(settings, S_MOTION_BEHAVIOUR);
	filter->start_position = obs_data_get_bool(settings, S_START_POS);
	filter->start_scale = obs_data_get_bool(settings, S_START_SCALE);
	filter->path_type = (int)obs_data_get_int(settings, S_PATH_TYPE);
	filter->org_pos.x = (float)obs_data_get_int(settings, S_ORG_X);
	filter->org_pos.y = (float)obs_data_get_int(settings, S_ORG_Y);
	filter->org_width = (int)obs_data_get_int(settings, S_ORG_W);
	filter->org_height = (int)obs_data_get_int(settings, S_ORG_H);
	filter->ctrl_pos.x = (float)obs_data_get_int(settings, S_CTRL_X);
	filter->ctrl_pos.y = (float)obs_data_get_int(settings, S_CTRL_Y);
	filter->ctrl2_pos.x = (float)obs_data_get_int(settings, S_CTRL2_X);
	filter->ctrl2_pos.y = (float)obs_data_get_int(settings, S_CTRL2_Y);
	filter->duration = (float)obs_data_get_double(settings, S_DURATION);
	filter->use_dst_scale = obs_data_get_bool(settings, S_USE_DST_SCALE);
	filter->dst_pos.x = (float)obs_data_get_int(settings, S_DST_X);
	filter->dst_pos.y = (float)obs_data_get_int(settings, S_DST_Y);
	filter->dst_width = (int)obs_data_get_int(settings, S_DST_W);
	filter->dst_height = (int)obs_data_get_int(settings, S_DST_H);
	item_name = obs_data_get_string(settings, S_SOURCE);
	item_id = get_item_id(filter->context, item_name);

	if (item_id != filter->item_id) 
		recover_source(filter);

	bfree(filter->item_name);
	filter->item_name = bstrdup(item_name);
	filter->item_id = item_id;
}

static bool init_hotkey(void *data)
{
	motion_filter_data_t *filter = data;
	obs_source_t *source = obs_filter_get_parent(filter->context);
	obs_scene_t *scene = obs_scene_from_source(source);
	filter->hotkey_init = true;

	if (!scene)
		return false;


	filter->hotkey_id_f = register_hotkey(filter->context, source, S_FORWARD,
		T_FORWARD, hotkey_forward, data);

	if (filter->motion_behaviour == S_MOTION_ROUND_TRIP) {
		filter->hotkey_id_b = register_hotkey(filter->context, source, 
			S_BACKWARD, T_BACKWARD, hotkey_backward, data);
	}

	return true;
}

static bool deinit_hotkey(void *data)
{
	

}

static bool motion_set_button(obs_properties_t *props, obs_property_t *p,
	bool reversed)
{
	obs_property_t *f = obs_properties_get(props, S_FORWARD);
	obs_property_t *b = obs_properties_get(props, S_BACKWARD);
	obs_property_set_visible(f, !reversed);
	obs_property_set_visible(b, reversed);
	UNUSED_PARAMETER(p);
	return true;
}

static bool forward_clicked(obs_properties_t *props, obs_property_t *p,
	void *data)
{
	motion_filter_data_t *filter = data;
	if (motion_init(data, true) && filter->motion_behaviour == S_MOTION_ROUND_TRIP)
		return motion_set_button(props, p, true);
	else
		return false;
}

static bool backward_clicked(obs_properties_t *props, obs_property_t *p,
	void *data)
{
	if (motion_init(data, false))
		return motion_set_button(props, p, false);
	else
		return false;
}

static bool source_changed(obs_properties_t *props, obs_property_t *p,
	obs_data_t *s)
{
	bool reversed = obs_data_get_bool(s, S_IS_REVERSED);
	obs_property_t *f = obs_properties_get(props, S_FORWARD);
	obs_property_t *b = obs_properties_get(props, S_BACKWARD);
	if (obs_property_visible(f) && obs_property_visible(b))
		return motion_set_button(props, p, reversed);
	else
		return motion_set_button(props, p, false);
}

static bool motion_list_source(obs_scene_t* scene,
	obs_sceneitem_t* item, void* p)
{
	obs_source_t *source = obs_sceneitem_get_source(item);
	const char *name = obs_source_get_name(source);
	obs_property_list_add_string((obs_property_t*)p, name, name);
	UNUSED_PARAMETER(scene);
	return true;
}

/** 
 * Macro: set_visibility
 * ---------------------
 * Sets the visibility of a property field in the config.
 * Our lists have an int backend like an enum,
 *		key:	the property key - e.g. S_EXAMPLE
 *		val:	either 0 or 1 for toggles, 0->N for lists
 *		cmp:	comparison value, either 1 for toggles, or 0->N for lists
 */
#define set_visibility(key, val, cmp) \
		do { \
			p = obs_properties_get(props, key); \
			obs_property_set_visible(p, val >= cmp);\
		} while (false)

/* 
 * Macro: set_visibility_bool
 * --------------------------
 * Shorthand for when we want visibility directly affected by toggle. 
 *		key:	the property key - e.g. S_EXAMPLE
 *		vis:	a bool for whether the property should be shown (true) or hidden (false)
 */
#define set_visibility_bool(key, vis) \
		set_visibility(key, vis ? 1 : 0, 1)

static bool path_type_changed(obs_properties_t *props, obs_property_t *p,
	obs_data_t *s)
{
	int type = (int)obs_data_get_int(s, S_PATH_TYPE);
	set_visibility(S_CTRL_X, type, S_PATH_QUADRATIC);
	set_visibility(S_CTRL_Y, type, S_PATH_QUADRATIC);
	set_visibility(S_CTRL2_X, type, S_PATH_CUBIC);
	set_visibility(S_CTRL2_Y, type, S_PATH_CUBIC);
	return true;
}

static bool motion_behaviour_changed(void *data, obs_properties_t *props, obs_property_t *p,
	obs_data_t *s)
{
	struct motion_filter_data *filter = data;
	int behaviour = (int)obs_data_get_int(s, S_MOTION_BEHAVIOUR);
	if (behaviour != filter->motion_behaviour) {
		// Behaviour has changed! Nuke and reload the hotkey config.
		filter->motion_behaviour = behaviour;
		obs_hotkey_unregister(filter->hotkey_id_f);
		obs_hotkey_unregister(filter->hotkey_id_b);
		filter->hotkey_id_b = OBS_INVALID_HOTKEY_ID;
		filter->hotkey_id_f = OBS_INVALID_HOTKEY_ID;
		filter->hotkey_init = false;
	}

	return false;
}

static bool provide_start_position_toggle_changed(obs_properties_t *props, 
	obs_property_t *p, obs_data_t *s)
{
	bool ticked = obs_data_get_bool(s, S_START_POS);
	set_visibility_bool(S_ORG_X, ticked);
	set_visibility_bool(S_ORG_Y, ticked);
	return true;
}

static bool provide_start_size_toggle_changed(obs_properties_t *props, 
	obs_property_t *p, obs_data_t *s)
{
	bool ticked = obs_data_get_bool(s, S_START_SCALE);
	set_visibility_bool(S_ORG_W, ticked);
	set_visibility_bool(S_ORG_H, ticked);
	return true;
}

static bool provide_custom_size_at_destination_toggle_changed(
	obs_properties_t *props, obs_property_t *p, obs_data_t *s)
{
	bool ticked = obs_data_get_bool(s, S_USE_DST_SCALE);
	set_visibility_bool(S_DST_W, ticked);
	set_visibility_bool(S_DST_H, ticked);
	return true;
}

static bool dest_grab_current_position_clicked(obs_properties_t *props, 
	obs_property_t *p, void *data)
{
	struct motion_filter_data *filter = data;
	obs_sceneitem_t *item = get_item(filter->context, filter->item_name);
	// Find the targetted source item within the scene
	if (!filter->item) {
		item = get_item_by_id(filter->context, filter->item_id);
		reset_source_name(data, filter->item);
	}

	if (item) {
		struct obs_transform_info info;
		obs_sceneitem_get_info(item, &info);
		// Set setting property values to match the source's current position
		obs_data_t *settings = obs_source_get_settings(filter->context);
		obs_data_set_double(settings, S_DST_X, info.pos.x);
		obs_data_set_double(settings, S_DST_Y, info.pos.y);
		obs_data_release(settings);
	}

	return true;
}

#undef set_visibility
#undef set_visibility_bool

/*
 * Filter property layout.
 */
static obs_properties_t *motion_filter_properties(void *data)
{
	motion_filter_data_t *filter = data;
	obs_properties_t *props = obs_properties_create();
	obs_property_t *p;

	obs_source_t *source = obs_filter_get_parent(filter->context);
	obs_scene_t *scene = obs_scene_from_source(source);

	if (!scene)
		return props;
	
	p = obs_properties_add_list(props, S_SOURCE, T_SOURCE,
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

	struct dstr disable_str = { 0 };
	dstr_copy(&disable_str, "--- ");
	dstr_cat(&disable_str, T_DISABLED);
	dstr_cat(&disable_str, " ---");
	obs_property_list_add_string(p, disable_str.array, disable_str.array);
	dstr_free(&disable_str);

	// A list of sources
	obs_scene_enum_items(scene, motion_list_source, (void*)p);
	obs_property_set_modified_callback(p, source_changed);

	// Various motion behaviour types
	p = obs_properties_add_list(props, S_MOTION_BEHAVIOUR, T_MOTION_BEHAVIOUR,
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, T_MOTION_ONE_WAY, S_MOTION_ONE_WAY);
	obs_property_list_add_int(p, T_MOTION_ROUND_TRIP, S_MOTION_ROUND_TRIP);
	// Using modified_callback2 enables us to send along data into the callback
	obs_property_set_modified_callback2(p, motion_behaviour_changed, filter);

	// Toggle for providing a custom start position
	p = obs_properties_add_bool(props, S_START_POS, T_START_POS);
	obs_property_set_modified_callback(p, provide_start_position_toggle_changed);
	// Custom starting X and Y values
	obs_properties_add_int(props, S_ORG_X, T_ORG_X, 0, 8192, 1);
	obs_properties_add_int(props, S_ORG_Y, T_ORG_Y, 0, 8192, 1);

	// Toggle for providing a custom starting size
	p = obs_properties_add_bool(props, S_START_SCALE, T_START_SCALE);
	obs_property_set_modified_callback(p, provide_start_size_toggle_changed);
	// Custom width and height
	obs_properties_add_int(props, S_ORG_W, T_ORG_W, 0, 8192, 1);
	obs_properties_add_int(props, S_ORG_H, T_ORG_H, 0, 8192, 1);

	// Various animation types
	p = obs_properties_add_list(props, S_PATH_TYPE, T_PATH_TYPE,
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, T_PATH_LINEAR, S_PATH_LINEAR);
	obs_property_list_add_int(p, T_PATH_QUADRATIC, S_PATH_QUADRATIC);
	obs_property_list_add_int(p, T_PATH_CUBIC, S_PATH_CUBIC);
	obs_property_set_modified_callback(p, path_type_changed);

	// Button that pre-populates destination position with the source's current position
	obs_properties_add_button(props, S_DEST_GRAB_POS, T_DEST_GRAB_POS, 
		dest_grab_current_position_clicked);
	// Destination X and Y values
	obs_properties_add_int(props, S_DST_X, T_DST_X, -8192, 8192, 1);
	obs_properties_add_int(props, S_DST_Y, T_DST_Y, -8192, 8192, 1);
	// Other control point fields for other types
	obs_properties_add_int(props, S_CTRL_X, T_CTRL_X, -8192, 8192, 1);
	obs_properties_add_int(props, S_CTRL_Y, T_CTRL_Y, -8192, 8192, 1);
	obs_properties_add_int(props, S_CTRL2_X, T_CTRL2_X, -8192, 8192, 1);
	obs_properties_add_int(props, S_CTRL2_Y, T_CTRL2_Y, -8192, 8192, 1);

	// Toggle for providing a custom size for the source at its destination
	p = obs_properties_add_bool(props, S_USE_DST_SCALE, T_USE_DST_SCALE);
	obs_property_set_modified_callback(p, 
		provide_custom_size_at_destination_toggle_changed);
	// Custom width and height
	obs_properties_add_int(props, S_DST_W, T_DST_W, 0, 8192, 1);
	obs_properties_add_int(props, S_DST_H, T_DST_H, 0, 8192, 1);

	// Animation duration slider
	obs_properties_add_float_slider(props, S_DURATION, T_DURATION, 0, 5, 
		0.1);

	// Forwards / Backwards button(s)
	obs_properties_add_button(props, S_FORWARD, T_FORWARD, forward_clicked);
	obs_properties_add_button(props, S_BACKWARD, T_BACKWARD, backward_clicked);

	return props;
}

static void cal_variation(motion_filter_data_t *filter)
{
	variation_data_t *data = &filter->variation;

	float elapsed_time = min(filter->duration, data->elapsed_time);
	float percent;
	int order;

	if (filter->duration <= 0)
		percent = 1.0f;
	else if (filter->motion_reverse) 
		percent = 1.0f - (elapsed_time / filter->duration);
	else 
		percent = elapsed_time / filter->duration;


	if (filter->path_type == S_PATH_QUADRATIC)
		order = 2;
	else if (filter->path_type == S_PATH_CUBIC)
		order = 3;
	else
		order = 1;

	data->position.x = bezier(data->point_x, percent, order);
	data->position.y = bezier(data->point_y, percent, order);


	if (filter->use_dst_scale) {
		data->scale.x = bezier(data->scale_x, percent, 1);
		data->scale.y = bezier(data->scale_y, percent, 1);
	} else {
		data->scale.x = data->scale_x[0];
		data->scale.y = data->scale_y[0];
	}
}

static void motion_filter_tick(void *data, float seconds)
{
	motion_filter_data_t *filter = data;
	variation_data_t *var = &filter->variation;

	if (filter->motion_start) {

		cal_variation(filter);
		obs_sceneitem_set_pos(filter->item, &var->position);
		obs_sceneitem_set_scale(filter->item, &var->scale);

		if (var->elapsed_time >= filter->duration) {
			filter->motion_start = false;
			var->elapsed_time = 0.0f;
			obs_sceneitem_release(filter->item);
			if (filter->motion_behaviour == S_MOTION_ROUND_TRIP) {
				filter->motion_reverse = !filter->motion_reverse;
				set_reverse_info(filter);
			}
		} else
			var->elapsed_time += seconds;
	}

	if (!filter->hotkey_init)
		init_hotkey(data);
}

static void motion_filter_save(void *data, obs_data_t *settings)
{
	motion_filter_data_t *filter = data;
	save_hotkey_config(filter->hotkey_id_f, settings, S_FORWARD);
	save_hotkey_config(filter->hotkey_id_b, settings, S_BACKWARD);
}

static void *motion_filter_create(obs_data_t *settings, obs_source_t *context)
{
	motion_filter_data_t *filter = bzalloc(sizeof(*filter));
	
	filter->context = context;
	filter->motion_start = false;
	filter->hotkey_init = false;
	filter->motion_behaviour = S_MOTION_ROUND_TRIP;
	filter->path_type = S_PATH_LINEAR;
	filter->motion_reverse = obs_data_get_bool(settings, S_IS_REVERSED);
	filter->restart_backward = filter->motion_reverse;
	obs_source_update(context, settings);
	return filter;
}

static void motion_filter_remove(void *data, obs_source_t *source)
{
	motion_filter_data_t *filter = data;
	recover_source(filter);
	UNUSED_PARAMETER(source);
}

static void motion_filter_destroy(void *data)
{
	motion_filter_data_t *filter = data;
	if (filter->hotkey_id_f)
		obs_hotkey_unregister(filter->hotkey_id_f);

	if (filter->hotkey_id_b && filter->motion_behaviour == S_MOTION_ROUND_TRIP)
		obs_hotkey_unregister(filter->hotkey_id_b);

	bfree(filter->item_name);
	bfree(filter);
}

static void motion_filter_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, S_IS_REVERSED, false);
	obs_data_set_default_int(settings, S_SOURCE, -1);
	obs_data_set_default_int(settings, S_MOTION_BEHAVIOUR, S_MOTION_ROUND_TRIP);
	obs_data_set_default_int(settings, S_ORG_W, 300);
	obs_data_set_default_int(settings, S_ORG_H, 300);
	obs_data_set_default_int(settings, S_DST_W, 300);
	obs_data_set_default_int(settings, S_DST_H, 300);
	obs_data_set_default_double(settings, S_DURATION, 1.0);
}


static const char *motion_filter_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return T_("Motion");
}

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("motion-filter", "en-US")

struct obs_source_info motion_filter = {
	.id = "motion-filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = motion_filter_get_name,
	.create = motion_filter_create,
	.destroy = motion_filter_destroy,
	.update = motion_filter_update,
	.get_properties = motion_filter_properties,
	.get_defaults = motion_filter_defaults,
	.video_tick = motion_filter_tick,
	.save = motion_filter_save,
	.filter_remove = motion_filter_remove
};

bool obs_module_load(void) {
	obs_register_source(&motion_filter);
	return true;
}