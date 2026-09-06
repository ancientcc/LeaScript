#ifndef UNIT_HPP_INCLUDED
#define UNIT_HPP_INCLUDED

#include "base_unit.hpp"

class mkscript_controller;
class mkscript_display;
class mkscript_unit_map;

#define MKSCRIPT_UNIT_LOCS	2  // 2

// m(middle)
enum {rect8p_ltop, rect8p_rtop, rect8p_rbottom, rect8p_lbottom,
    rect8p_mtop, rect8p_mright, rect8p_mbottom, rect8p_mleft, rect8p_count};

namespace visio {
enum {shape_voice_state, shape_pose_state, shape_pose_state_rep, shape_edge, shape_text, shape_count};

#define shape_type_is_state(type) \
	((type) == visio::shape_voice_state || (type) == visio::shape_pose_state || (type) == visio::shape_pose_state_rep)

#define shape_type_is_can_click(type) \
	(shape_type_is_state(type) || (type) == visio::shape_text)

enum {textid_id, textid_name, textid_author, textid_reference, textid_sys_count};

#define SHAPE_STRETCH_RECT_COUNT	rect8p_count
extern int SHAPE_MARGIN; // This value should vary with different hdpi_scale.
#define MIN_SHAPE_WIDTH		128
#define MIN_SHAPE_HEIGHT	128

void calculate_stretch_rects(const SDL_Rect& obj_rect, SDL_Rect* rects, SDL_Point* points);

class tshape
{
public:
	tshape(int type, const std::string& stem_png, const std::string& name)
		: type(type)
		, stem_png(stem_png)
		, name(name)
		, line_color({0.0, 0.0, 0.0, 0.0})
	{
		if (type == shape_voice_state) {
			line_color = SDL_DColor{128.0 / 255, 128.0 / 255, 128.0 / 255, 1.0};

		} else if (type == shape_pose_state || type == shape_pose_state_rep) {
			line_color = SDL_DColor{1.0, 59.0 / 255, 48.0 / 255, 1.0};

		} else if (type == shape_edge) {

		} else if (type == shape_text) {

		} else {
			VALIDATE(false, null_str);
		}
	}
	virtual ~tshape() {}

	bool operator<(const tshape& that) const { return type < that.type; }

	surface get_surf(int width, int height) const;
	surface get_sel_surf(int width, int height) const;

public:
	int type;
	std::string stem_png;
	std::string name;
	SDL_DColor line_color;

private:
	mutable surface surf_;
	mutable surface sel_surf_;
};

}

class mkscript_unit: public base_unit
{
public:
	mkscript_unit(mkscript_controller& controller, mkscript_display& disp, mkscript_unit_map& units);

private:
	void app_draw_unit(const int xsrc, const int ysrc) override;

protected:
	mkscript_controller& controller_;
	mkscript_display& disp_;
	mkscript_unit_map& units_;
};

#endif
