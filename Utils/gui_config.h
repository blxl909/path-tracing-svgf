#ifndef GUI_CONFIG_H
#define GUI_CONFIG_H

#include "Utils/camera.h"


enum debug_view_type
{
	path_tracing_pic_1spp,
	svgf_reprojected_pic,
	svgf_variance_pic,
	svgf_atrous_pic,
	svgf_modulate_pic,
	taa_pic,
	final_pic,
	accumulate_color
};

class parameter_config {
public:
	float sigma_z = 1.0f;
	float sigma_n = 128.0f;
	float sigma_l = 4.0f;

	float reproj_normal_threshold = 16.0f;
	float reproj_depth_threshold = 10.0f;

	float clamp_threshold =10.0f;
	int max_tracing_depth = 2;

	int num_atrous_iterations = 5;

	bool accumulate_color = true;
	bool use_normal_texture = false;


	void update_debug_pic_state(Camera& camera, debug_view_type type) {
		if (type == debug_view_type::accumulate_color) {
			accumulate_color = true;
		}
		else {
			accumulate_color = false;
		}
		camera.frameCounter = 0;
	}
};






#endif
