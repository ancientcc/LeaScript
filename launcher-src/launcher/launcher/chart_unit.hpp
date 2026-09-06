#ifndef CHART_UNIT_HPP_INCLUDED
#define CHART_UNIT_HPP_INCLUDED

#include "base_unit.hpp"

class chart_controller;
class chart_display;
class chart_unit_map;

#define CAF_SAMPLES_RATE		8000
#define MDA_SAMPLES_RATE		10

#define AUDIO_BYTES_PER_SAMPLE	2
#define MAX_AUDIO_RANGE		32768

class chart_unit: public base_unit
{
public:
	static const int alert_radius;
	static const int top_margin;
	static const int initial_zoom;

	struct talert
	{
		talert(int x, int y)
			: x(x)
			, y(y)
		{}
	
		int x;
		int y;
	};

	chart_unit(chart_controller& controller, chart_display& disp, chart_unit_map& units, const SDL_Rect& rect, int samples, int pixels_per_sample, int number, const uint8_t* chart_data);
	~chart_unit();

	// bool require_sort() const { return true; }
	bool sort_compare(const base_unit& that) const;
	void app_draw_unit(const int xsrc, const int ysrc) override;

	void set_samples(int samples, int pixels_per_sample, const uint8_t* chart_data);
	void clear_samples();
	void draw_minimap(surface& screen, const SDL_Rect& minimap_location, double xscaling, double yscaling, const uint32_t mapped_col) const;

	void insert_alert(int pixel, int value, bool caf);
	std::vector<talert>& alerts() { return alerts_; }
	const talert* point_in_alert(int x, int y) const;

	int number() const { return number_; }
	int samples() const { return samples_; }
	int pixels_per_sample() const { return pixels_per_sample_; }
	const uint8_t* chart_data() const { return chart_data_; }

	void get_current_value(int at, int& audio, int& motion) const;

private:
	void redraw_unit2(bool show_nagtive, surface& canvas, int samples, int pixels_per_sample, const uint8_t* chart_data, bool audio, bool half);
	void draw_minimap2(surface& screen, const SDL_Rect& minimap_location, double xscaling, double yscaling, const uint32_t mapped_col, int samples, int pixels_per_sample, const uint8_t* chart_data, bool audio) const;

	void resize_data(int size);
	SDL_Point calculate_text_xy_offset(bool show_nagtive, const int xsrc, const int ysrc, const surface& text_surf, int volume);

protected:
	chart_controller& controller_;
	chart_display& disp_;
	chart_unit_map& units_;

	const int number_;
	int samples_;
	int pixels_per_sample_;

	int chart_data_size_;
	int chart_data_vsize_;
	uint8_t* chart_data_;

	std::vector<talert> alerts_;
};

#endif
