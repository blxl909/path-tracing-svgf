#ifndef GUI_CONFIG_H
#define GUI_CONFIG_H

#include <vector>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"


class parameter_config {
public:
	float sigma_z = 1.0f;
	float sigma_n = 128.0f;
	float sigma_l = 4.0f;

	float reproj_normal_threshold = 0.8f;
	float reproj_depth_threshold = 0.2f;

	float clamp_threshold =10.0f;
	int max_tracing_depth = 2;

	int num_atrous_iterations = 5;
};


enum debug_view_type
{
	path_tracing_pic_1spp, 
	svgf_reprojected_pic,
	svgf_variance_pic,
	svg_atrous_pic,
	taa_pic,
	final_pic,
	accumulate_color
};

class debug_gui {
public:

	bool path_tracing_pic_1spp = false;
	bool svgf_reprojected_pic = false;
	bool svgf_variance_pic = false;
	bool svg_atrous_pic = false;
	bool taa_pic = false;
	bool final_pic = false;
	bool accumulate_color = true;

	bool use_normal_texture = false;


	debug_view_type pre_type = debug_view_type::accumulate_color;



	void update_debug_state(debug_view_type type) {
		switch (pre_type)
		{
		case debug_view_type::path_tracing_pic_1spp:
			path_tracing_pic_1spp = false;
			break;
		case debug_view_type::svgf_reprojected_pic:
			svgf_reprojected_pic = false;
			break;
		case debug_view_type::svgf_variance_pic:
			svgf_variance_pic = false;
			break;
		case debug_view_type::svg_atrous_pic:
			svg_atrous_pic = false;
			break;
		case debug_view_type::taa_pic:
			taa_pic = false;
			break;
		case debug_view_type::final_pic:
			final_pic = false;
			break;
		case debug_view_type::accumulate_color:
			accumulate_color = false;
			break;
		default:
			accumulate_color = false;
			break;
		}


		switch (type)
		{
		case debug_view_type::path_tracing_pic_1spp:
			path_tracing_pic_1spp = true;
			break;
		case debug_view_type::svgf_reprojected_pic:
			svgf_reprojected_pic = true;
			break;
		case debug_view_type::svgf_variance_pic:
			svgf_variance_pic = true;
			break;
		case debug_view_type::svg_atrous_pic:
			svg_atrous_pic = true;
			break;
		case debug_view_type::taa_pic:
			taa_pic = true;
			break;
		case debug_view_type::final_pic:
			final_pic = true;
			break;
		case debug_view_type::accumulate_color:
			accumulate_color = true;
			break;
		default:
			accumulate_color = true;
			break;
		}

		pre_type = type;
		
	}
};

#endif
