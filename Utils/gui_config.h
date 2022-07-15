#ifndef GUI_CONFIG_H
#define GUI_CONFIG_H

#include <vector>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"


class guiConfig {
public:
	bool path_tracing_pic_1spp = false;
	bool svgf_reprojected_pic = false;
	bool final_pic = false;
	bool accumulate_color = true;
	bool use_normal_texture = false;

	std::vector<bool> config_container;
public:
	guiConfig() {
		
	}

	void show_pass_img() {

	}
};

class debug_gui {
public:
	bool use_debug_view = false;
	//最好是枚举类型
	bool path_tracing_pic_1spp = false;
	bool svgf_reprojected_pic = false;
	bool svgf_variance_pic = false;
	bool svg_atrous_pic = false;
	bool taa_pic = false;
	bool final_pic = false;
	bool accumulate_color = true;

	std::vector<bool> debug_view_container;

	debug_gui() {
		debug_view_container.push_back(path_tracing_pic_1spp);
		debug_view_container.push_back(svgf_reprojected_pic);
		debug_view_container.push_back(svgf_variance_pic);
		debug_view_container.push_back(svg_atrous_pic);
		debug_view_container.push_back(taa_pic);
		debug_view_container.push_back(final_pic);
		debug_view_container.push_back(accumulate_color);
	}

	void show_debug_view() {

	}
};

#endif
