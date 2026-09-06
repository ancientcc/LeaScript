#define GETTEXT_DOMAIN "rose-lib"

#include "cairo2.hpp"
#include <iomanip>
#include "rose_config.hpp"
#include "font.hpp"
#include "gettext.hpp"
#include "rose_string_utils_dll.hpp"
#include "gui/widgets/widget.hpp"

#include <opencv2/imgproc.hpp>

using namespace std::placeholders;

namespace cairo {

void draw_rounded_rectangle(cairo_t* cr, double x, double y, double width, double height, double radius,
    bool ltop, bool rtop, bool lbottom, bool rbottom)
{
    // Ensure the radius does not exceed half the rectangle size.
    radius = std::min(radius, std::min(width / 2, height / 2));
    
    // Starting from the top-left corner, draw clockwise.
    if (ltop) {
        cairo_move_to(cr, x + radius, y);                          // Move to the starting point of the arc at the top-left corner.
    } else {
        cairo_move_to(cr, x, y);                          // Move to the starting point of the arc at the top-left corner.
    }
    if (rtop) {
        cairo_line_to(cr, x + width - radius, y);                  // Top edge line
        cairo_arc(cr, x + width - radius, y + radius, radius, -M_PI/2, 0);  // Top-right corner arc
    } else {
        cairo_line_to(cr, x + width, y);                  // Top edge line
    }
    if (rbottom) {
        cairo_line_to(cr, x + width, y + height - radius);         // Right edge line
        cairo_arc(cr, x + width - radius, y + height - radius, radius, 0, M_PI/2);  // Bottom-right corner arc
    } else {
        cairo_line_to(cr, x + width, y + height);         // Right edge line
    }
    if (lbottom) {
        cairo_line_to(cr, x + radius, y + height);                 // Bottom edge line
        cairo_arc(cr, x + radius, y + height - radius, radius, M_PI/2, M_PI);  // Bottom-left corner arc
    } else {
        cairo_line_to(cr, x, y + height);                 // Bottom edge line
    }
    
    if (rtop) {
        cairo_line_to(cr, x, y + radius);                          // Left edge line
        cairo_arc(cr, x + radius, y + radius, radius, M_PI, 3*M_PI/2);  // Top-left corner arc
    } else {
        cairo_line_to(cr, x, y);                          // Left edge line
    }
    cairo_close_path(cr);  // Close path
}

void draw_rounded_rectangle2(const SDL_DColor* fill_color, const SDL_DColor* line_color, double line_width,  cairo_t* cr, double x, double y, double width, double height, double radius,
    bool ltop, bool rtop, bool lbottom, bool rbottom)
{
    VALIDATE(fill_color != nullptr || line_color != nullptr, null_str);
    draw_rounded_rectangle(cr, x, y, width, height, radius, ltop, rtop, lbottom, rbottom);
    if (fill_color != nullptr) {
        cairo_set_source_rgba(cr, fill_color->r, fill_color->g, fill_color->b, fill_color->a);
        if (line_color != nullptr) {
            cairo_fill_preserve(cr);
        } else {
            cairo_fill(cr);
        }
    }
    if (line_color != nullptr) {
        cairo_set_source_rgba(cr, line_color->r, line_color->g, line_color->b, line_color->a);
        cairo_set_line_width(cr, line_width);
        cairo_stroke(cr);
    }    
}

void ticon::set_cairo_shape(int _shape_type, const SDL_DColor& _color, const SDL_DSize& _size, double _border_width, double _padding)
{
	VALIDATE(_shape_type >= 0 && _shape_type < shapetype_count, null_str);
    VALIDATE(_shape_type != shapetype_surf, null_str);
	if (shape_type == shapetype_circle_solid || shape_type == shapetype_circle_hollow) {
		VALIDATE(KDL_Equal(size.w, size.h), null_str);
	}

	shape_type = _shape_type;
	color = _color;
	size = _size;
	border_width = _border_width;
	padding = _padding;
}

void ticon::set_surf(const surface& _surf, double _padding)
{
    VALIDATE(_surf.get() != nullptr, null_str);

    shape_type = shapetype_surf;
    surf = _surf;
    size.w = _surf->w;
    size.h = _surf->h;
    border_width = 0;
    padding = _padding;
}

void ticon::draw(cairo_t *cr) const
{
    if (shape_type == shapetype_circle_solid) {
        cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
        // cairo_arc(cr, thumbX, progressY + progressHeight/2, 3, 0, 2 * M_PI);
        double radius = size.w / 2;
        cairo_arc(cr, x + radius, y + radius, radius, 0, 2 * M_PI);
        cairo_fill(cr);

    } else {
        VALIDATE(false, null_str);
    }
}

void draw_rounded_rectangle3(const ticon* icon, const SDL_DColor* fill_color, const SDL_DColor* line_color, double line_width,  cairo_t* cr, double x, double y, double width, double height, double radius,
    bool ltop, bool rtop, bool lbottom, bool rbottom)
{
    draw_rounded_rectangle2(fill_color, line_color, line_width, cr, x, y, width, height, radius,
        ltop, rtop, lbottom, rbottom);

    if (icon != nullptr && icon->shape_type != shapetype_surf) {
        icon->draw(cr);
    }
}

// 计算两点之间的距离
double distance(const SDL_DPoint& p1, const SDL_DPoint& p2)
{
    double dx = p2.x - p1.x;
    double dy = p2.y - p1.y;
    return sqrt(dx * dx + dy * dy);
}

// Draw a line with an arrow (arrow at the end point)
// cr: Cairo contex
// start: start point
// end: end point
// arrow_size: Arrow size (in pixels)
// line_width:
void draw_line_with_arrow(cairo_t* cr, const SDL_DPoint& start, const SDL_DPoint& end, const SDL_DColor& color,
                          double arrow_size, double line_width, double arrow_width_ratio)
{
    cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
    
    double dx = end.x - start.x;
    double dy = end.y - start.y;
    double len = sqrt(dx * dx + dy * dy);
    
    if (len < 0.001) return;
    
    // 单位方向向量
    double udx = dx / len;
    double udy = dy / len;
    
    // 计算垂直方向向量（确保长度一致，解决水平/垂直线条变细问题）
    double perp_x = -udy;
    double perp_y = udx;
    
    // 箭头底边宽度系数（0.4 表示箭头宽度为箭头长度的 0.8 倍）
    double arrow_base_width = arrow_size * arrow_width_ratio;
    
    // 箭头底边两个点的坐标
    double arrow_base1_x = end.x - arrow_size * udx + arrow_base_width * perp_x;
    double arrow_base1_y = end.y - arrow_size * udy + arrow_base_width * perp_y;
    double arrow_base2_x = end.x - arrow_size * udx - arrow_base_width * perp_x;
    double arrow_base2_y = end.y - arrow_size * udy - arrow_base_width * perp_y;
    
    // 主线终点（连接到箭头底边中心，不是箭头顶点）
    double line_end_x = end.x - arrow_size * udx * 0.3;
    double line_end_y = end.y - arrow_size * udy * 0.3;
    
    // 保存当前状态
    cairo_save(cr);
    
    // 设置线条样式
    cairo_set_line_width(cr, line_width);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    
    // 绘制主线（确保使用足够的长度）
    cairo_move_to(cr, start.x, start.y);
    cairo_line_to(cr, line_end_x, line_end_y);
    cairo_stroke(cr);
    
    // 绘制实心三角形箭头
    cairo_move_to(cr, end.x, end.y);
    cairo_line_to(cr, arrow_base1_x, arrow_base1_y);
    cairo_line_to(cr, arrow_base2_x, arrow_base2_y);
    cairo_close_path(cr);
    cairo_fill(cr);
    
    // 恢复状态
    cairo_restore(cr);
}

void draw_rounded_rectangle_8points(const SDL_Size& bg_size, int margin, bool sel, const SDL_DColor* fill_color, const SDL_DColor* line_color, double line_width, cairo_t* cr, double radius,
    bool ltop, bool rtop, bool lbottom, bool rbottom)
{
    double x = margin;
    double y = margin;
    double width = bg_size.w - 2 * margin;
    double height = bg_size.h - 2 * margin;
    VALIDATE(line_color != nullptr, null_str);

    draw_rounded_rectangle2(fill_color, line_color, line_width, cr, x, y, width, height, radius,
        ltop, rtop, lbottom, rbottom);

    if (!sel) {
        return;
    }
    // Rectangle boundary (with margins)
    // double rect_left = margin;
    // double rect_right = width - margin;
    // double rect_top = margin;
    // double rect_bottom = height - margin;

    double rect_left = x;
    double rect_right = bg_size.w - margin;
    double rect_top = y;
    double rect_bottom = bg_size.h - margin;
    
    // Rectangle center
    double center_x = (rect_left + rect_right) / 2;
    double center_y = (rect_top + rect_bottom) / 2;

    // Draw the rectangle border
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);  // black
    cairo_set_line_width(cr, line_width);
    
    cairo_move_to(cr, rect_left, rect_bottom);
    cairo_line_to(cr, rect_right, rect_bottom);
    cairo_line_to(cr, rect_right, rect_top);
    cairo_line_to(cr, rect_left, rect_top);
    cairo_close_path(cr);
    cairo_stroke(cr);

    const SDL_DPoint points[] = {
        // 4 vertices
        {rect_left, rect_bottom},   // lbottom
        {rect_right, rect_bottom},  // rbottom
        {rect_right, rect_top},     // rtop
        {rect_left, rect_top},      // ltop
        // Midpoints of the 4 sides
        {center_x, rect_bottom},    // bottom-mid
        {rect_right, center_y},     // right-mid
        {center_x, rect_top},       // top-mid
        {rect_left, center_y},      // left-mid
    };

    // Draw 8 key points (large dots)
    const int NUM = sizeof(points) / sizeof(points[0]);
    VALIDATE(NUM == 8, null_str);
    for (size_t i = 0; i < NUM; i++) {
        SDL_DPoint p = points[i];
        
        // Outer white glow
        cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
        cairo_arc(cr, p.x, p.y, margin, 0, 2 * M_PI);
        cairo_fill(cr);
        
        // Inner red dot
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_arc(cr, p.x, p.y, margin - 1.5, 0, 2 * M_PI);
        cairo_fill(cr);
/*        
        // Inner white point
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_arc(cr, p.x, p.y, 3, 0, 2 * M_PI);
        cairo_fill(cr);
*/
    }
}

void draw_curved_line(cairo_t* cr, double x1, double y1, double x2, double y2)
{
    // Calculate direction
    double dx = x2 - x1;
    double dy = y2 - y1;

    // Generate random control points - each segment has its own unique curvature
    double random_factor1 = 0.2 + (rand() % 60) / 100.0;  // 0.2 ~ 0.8
    double random_factor2 = 0.2 + (rand() % 60) / 100.0;
        
    // Control point 1 (near the start point)
    double ctrl1_x = x1 + dx * random_factor1;
    double ctrl1_y = y1 + dy * random_factor1 + (rand() % 60) - 30;  // Vertical random offset
        
    // Control point 2 (near the end point)
    double ctrl2_x = x2 - dx * random_factor2;
    double ctrl2_y = y2 - dy * random_factor2 + (rand() % 60) - 30;
        
    // if (i == 0) {
    //    cairo_move_to(cr, x1, y1);
    // }

    cairo_curve_to(cr, ctrl1_x, ctrl1_y, ctrl2_x, ctrl2_y, x2, y2);
}

// Compared to the usual Catmull-Rom, when the y-values of the preceding and following points are equal, 
// make it as straight as possible.
// chinese remark version: 
//    draw_smooth_curve_optimized(...) in <apps>/external/dbg-chinese-remark/cairo.cpp
void draw_smooth_curve_optimized(cairo_t* cr, const SDL_DColor& color, double line_width,
                                 const std::vector<SDL_DPoint>& points,
                                 double tension = 0.5) {
    
    int n = points.size();
    if (n < 2) return;
    
    cairo_set_line_width(cr, line_width);
    cairo_set_source_rgb(cr, color.r, color.g, color.b);  // yellow
    
    // Move to the first point
    cairo_move_to(cr, points[0].x, points[0].y);
    
    // Horizontal segment threshold
    const double HORIZONTAL_THRESHOLD = 0.001;
    
    for (int i = 0; i < n - 1; i++) {
        double x1 = points[i].x;
        double y1 = points[i].y;
        double x2 = points[i+1].x;
        double y2 = points[i+1].y;
        
        // Check if the current segment is horizontal
        bool is_horizontal = (fabs(y2 - y1) < HORIZONTAL_THRESHOLD);
        
        if (is_horizontal) {
            // Horizontal segment: draw a straight line directly
            cairo_line_to(cr, x2, y2);
        } else {
            // Non-horizontal segment: use Catmull-Rom curve
            // Get previous and next points
            double x0, y0, x3, y3;
            
            if (i == 0) {
                x0 = x1;
                y0 = y1;
            } else {
                x0 = points[i-1].x;
                y0 = points[i-1].y;
            }
            
            if (i == n - 2) {
                x3 = x2;
                y3 = y2;
            } else {
                x3 = points[i+2].x;
                y3 = points[i+2].y;
            }
            
            // Check if further reduction in curvature is needed (previous and next points are also nearly horizontal)
            bool prev_horizontal = (i > 0) ? (fabs(y1 - points[i-1].y) < HORIZONTAL_THRESHOLD) : false;
            bool next_horizontal = (i < n - 2) ? (fabs(points[i+2].y - y2) < HORIZONTAL_THRESHOLD) : false;
            
            double t = tension;
/*            
            // If the current segment is slanted but the preceding and following segments are horizontal, 
            // reduce tension to make the transition smoother
            if (prev_horizontal || next_horizontal) {
                t *= 0.4;  // Reduce tension to make the curve closer to a straight line
            }
            
            // Additional check: if the y-value change is very small, also reduce tension
            double y_change = fabs(y2 - y1);
            if (y_change < 10.0) {  // Y change is less than 10 pixels
                t *= (y_change / 10.0) * 0.8;
                t = std::max(t, 0.1);
            }
*/            
            // Calculate Catmull-Rom control points
            double ctrl1_x = x1 + (x2 - x0) * t / 3.0;
            double ctrl1_y = y1 + (y2 - y0) * t / 3.0;
            double ctrl2_x = x2 - (x3 - x1) * t / 3.0;
            double ctrl2_y = y2 - (y3 - y1) * t / 3.0;
/*            
            // For slanted segments with small Y changes, 
            // ensure that the Y values of the control points do not deviate too much
            if (y_change < 20.0) {
                ctrl1_y = std::max(std::min(ctrl1_y, std::max(y1, y2) + 5), std::min(y1, y2) - 5);
                ctrl2_y = std::max(std::min(ctrl2_y, std::max(y1, y2) + 5), std::min(y1, y2) - 5);
            }
*/            
            cairo_curve_to(cr, ctrl1_x, ctrl1_y, ctrl2_x, ctrl2_y, x2, y2);
        }
    }
    
    cairo_stroke(cr);
}

// Draw a straight line
void draw_line(cairo_t* cr, double x1, double y1, double x2, double y2, const SDL_DColor& color, double line_width)
{
    // Set the line color.
    cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
    
    // Set the line width.
    cairo_set_line_width(cr, line_width);
    
    // Set the line cap style (optional: CAIRO_LINE_CAP_ROUND, CAIRO_LINE_CAP_SQUARE, CAIRO_LINE_CAP_BUTT)
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    
    // from (x1, y1) to (x2, y2)
    cairo_move_to(cr, x1, y1);
    cairo_line_to(cr, x2, y2);
    
    // Execute drawing.
    cairo_stroke(cr);
}

void did_draw_canvas_gradient(cairo_t* cr, int width, int height, double red, double green, double blue,
    bool vertical, const SDL_DColor& to)
{
    // Draw the canvas background (gradient)
    cairo_pattern_t* gradient = nullptr;
    if (vertical) {
        gradient = cairo_pattern_create_linear(0, 0, 0, 0 + height);
    } else {
        gradient = cairo_pattern_create_linear(0, 0, 0 + width, 0);
    }
    cairo_pattern_add_color_stop_rgb(gradient, 0, red, green, blue);
    cairo_pattern_add_color_stop_rgb(gradient, 1, to.r, to.g, to.b);
    cairo_set_source(cr, gradient);
    cairo_paint(cr);
    cairo_pattern_destroy(gradient);
}

void draw_gradient_use_path(cairo_t* cr, int width, int height, bool vertical, const SDL_DColor& from, 
    const SDL_DColor& to, const SDL_Rect* _clip)
{
    SDL_Rect clip{0, 0, width, height};
    if (_clip != nullptr) {
        clip = *_clip;
    }
    // ============================================
    // Draw the inner gradient rectangle rect{10,10,1260,700}
    // ============================================
    cairo_pattern_t* gradient = nullptr;
    if (vertical) {
        // Create a vertical gradient
        gradient = cairo_pattern_create_linear(clip.x, clip.y, clip.x, clip.y + clip.h);
    } else {
        // Create a vertical gradient
        gradient = cairo_pattern_create_linear(clip.x, clip.y, clip.x + clip.w, clip.y);
    }
    cairo_pattern_add_color_stop_rgb(gradient, 0, from.r, from.g, from.b);
    cairo_pattern_add_color_stop_rgb(gradient, 1, to.r, to.g, to.b);
    cairo_set_source(cr, gradient);
    
    // draw the inner rectangle
    cairo_rectangle(cr, clip.x, clip.y, clip.w, clip.h);
    cairo_fill(cr);
    
    cairo_pattern_destroy(gradient);
}

void draw_canvas(cairo_t* cr, int width, int height, double radius, const SDL_DColor& color,
    const std::function<void (cairo_t* cr, int width, int height, double red, double green, double blue)>& did_draw,
    bool ltop, bool rtop, bool lbottom, bool rbottom)
{
    if (radius != float_nposm) {
        // Transparent background
        // cairo_set_source_rgba(cr, 0, 0, 0, 0);
        // cairo_paint(cr);

        // ===== Set rounded rectangle clipping area =====
        cairo_save(cr);  // Save state for later restoration

        // Disable antialiasing
        cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);

        // Create a rounded rectangle path
        draw_rounded_rectangle(cr, 0, 0, width, height, radius, ltop, rtop, lbottom, rbottom);
    
        // Set as clipping area
        cairo_clip(cr);
    }

    if (did_draw == NULL) {
        // Fill with a single color
        cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
        cairo_paint(cr);

    } else {
        did_draw(cr, width, height, color.r, color.g, color.b);
    }

    if (radius != float_nposm) {
        // Restore state (remove clipping area)
        cairo_restore(cr);

        // Restore antialiasing (optional)
        cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);

    }
}

void drawPlayIcon(cairo_t* cr, double cx, double cy, double size)
{
    double h = size * 0.65;
    double w = size * 0.55;
    cairo_move_to(cr, cx - w/2, cy - h/2);
    cairo_line_to(cr, cx - w/2, cy + h/2);
    cairo_line_to(cr, cx + w/2, cy);
    cairo_close_path(cr);
    cairo_fill(cr);
}

void drawPauseIcon(cairo_t* cr, double cx, double cy, double size)
{
    double barW = size * 0.25;
    double barH = size * 0.65;
    double gap = size * 0.15;
    cairo_rectangle(cr, cx - barW - gap/2, cy - barH/2, barW, barH);
    cairo_fill(cr);
    cairo_rectangle(cr, cx + gap/2, cy - barH/2, barW, barH);
    cairo_fill(cr);
}

void drawRestartIcon(cairo_t* cr, double cx, double cy, double size)
{
    cairo_save(cr);
    
    double radius = size * 0.45;
    double stroke_width = size * 0.12;
    
    // Set line style
    cairo_set_line_width(cr, stroke_width);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    
    // Draw a circular arrow (counterclockwise)
    double start_angle = -M_PI / 3;      // -60 degrees (top right)
    double end_angle = start_angle + 5 * M_PI / 3;  // 300 degrees = 5*pi/3
    
    cairo_arc(cr, cx, cy, radius, start_angle, end_angle);
    cairo_stroke(cr);
    
    // Draw arrowhead (small triangle)
    double arrow_x = cx + radius * cos(end_angle);
    double arrow_y = cy + radius * sin(end_angle);
    double arrow_angle = end_angle + M_PI / 2;  // Tangent direction
    
    double arrow_size = size * 0.25; // 0.18
    double arrow_width = size * 0.2; // 0.1
    
    // Arrow triangle
    double tip_x = arrow_x + arrow_size * cos(arrow_angle);
    double tip_y = arrow_y + arrow_size * sin(arrow_angle);
    
    double left_x = arrow_x + arrow_width * cos(arrow_angle + M_PI * 0.65);
    double left_y = arrow_y + arrow_width * sin(arrow_angle + M_PI * 0.65);
    
    double right_x = arrow_x + arrow_width * cos(arrow_angle - M_PI * 0.65);
    double right_y = arrow_y + arrow_width * sin(arrow_angle - M_PI * 0.65);
    
    cairo_move_to(cr, tip_x, tip_y);
    cairo_line_to(cr, left_x, left_y);
    cairo_line_to(cr, right_x, right_y);
    cairo_close_path(cr);
    cairo_fill(cr);
    
    cairo_restore(cr);
}

// Single frame backward - double vertical line + left arrow
void drawStepbackwardIcon(cairo_t* cr, double cx, double cy, double size)
{
    double bar_w = size * 0.1;        // Vertical line width
    double bar_h = size * 0.7;       // Vertical line height
    double arrow_w = size * 0.35;     // Arrow width
    double arrow_h = size * 0.45;     // Arrow height
    double line_w = size * 0.45;      // Horizontal line length
    double line_h = size * 0.08;      // Horizontal line thickness
    double gap = size * 0.04;         // Gap between the vertical line and the arrow
    
    // Total icon width = vertical line + gap + arrow + horizontal line
    double total_w = bar_w + gap + arrow_w + line_w;
    double start_x = cx - total_w / 2;
    
    // 1. vertical line
    cairo_rectangle(cr, start_x, cy - bar_h/2, bar_w, bar_h);
    cairo_fill(cr);
    
    // 2. Left arrow (arrow on the right side of the vertical line, pointing left)
    double arrow_x = start_x + bar_w + gap;
    cairo_move_to(cr, arrow_x + arrow_w, cy - arrow_h/2);
    cairo_line_to(cr, arrow_x, cy);
    cairo_line_to(cr, arrow_x + arrow_w, cy + arrow_h/2);
    cairo_close_path(cr);
    cairo_fill(cr);
    
    // 3. Horizontal line (part of the arrow, to the right of the arrowhead)
    double line_x = arrow_x + arrow_w;
    cairo_rectangle(cr, line_x, cy - line_h/2, line_w, line_h);
    cairo_fill(cr);
}

// Single frame forward - right arrow + double vertical line
void drawStepforwardIcon(cairo_t* cr, double cx, double cy, double size)
{
    double bar_w = size * 0.1;        // Vertical line width
    double bar_h = size * 0.7;       // Vertical line height
    double arrow_w = size * 0.35;     // Arrow width
    double arrow_h = size * 0.45;     // Arrow height
    double line_w = size * 0.45;      // Horizontal line length
    double line_h = size * 0.08;      // Horizontal line thickness
    double gap = size * 0.04;         // Gap between the vertical line and the arrow
    
    // Total icon width = horizontal line + arrow + gap + vertical line
    double total_w = line_w + arrow_w + gap + bar_w;
    double start_x = cx - total_w / 2;
    
    // 1. Horizontal line (far left)
    double line_x = start_x;
    cairo_rectangle(cr, line_x, cy - line_h/2, line_w, line_h);
    cairo_fill(cr);
    
    // 2. Right arrow (arrow on the right side of the horizontal line, pointing right)
    double arrow_x = line_x + line_w;
    cairo_move_to(cr, arrow_x, cy - arrow_h/2);
    cairo_line_to(cr, arrow_x + arrow_w, cy);
    cairo_line_to(cr, arrow_x, cy + arrow_h/2);
    cairo_close_path(cr);
    cairo_fill(cr);
    
    // 3. Vertical line (far right, with a gap to the right of the arrow)
    double bar_x = arrow_x + arrow_w + gap;
    cairo_rectangle(cr, bar_x, cy - bar_h/2, bar_w, bar_h);
    cairo_fill(cr);
}

void drawStopIcon(cairo_t* cr, double cx, double cy, double size)
{
    double s = size * 0.65;
    cairo_rectangle(cr, cx - s/2, cy - s/2, s, s);
    cairo_fill(cr);
}

#ifndef M_PI_2
#define M_PI_2     1.57079632679489661923   // pi/2
#endif

void draw_check_icon(cairo_t* cr, int width, int height, double board_line_width, const SDL_DPoint& margin, double radius, double x, double y, double checkmark_line_width)
{
    // 圆角矩形参数 (使用传入的 margin_x 和 margin_y)
    double box_width = width - (margin.x * 2);
    double box_height = height - (margin.y * 2);

    // ==========================================
    // 绘制灰色圆角矩形边框
    // ==========================================
    cairo_set_source_rgb(cr, 0.78, 0.78, 0.78);
    cairo_set_line_width(cr, board_line_width);

    // 圆角矩形路径 (所有坐标 +x, +y 偏移，分别使用 margin.x 和 margin.y)
    cairo_move_to(cr, x + margin.x + radius, y + margin.y);
    cairo_line_to(cr, x + margin.x + box_width - radius, y + margin.y);
    cairo_arc(cr, x + margin.x + box_width - radius, y + margin.y + radius, radius, -M_PI_2, 0);
    cairo_line_to(cr, x + margin.x + box_width, y + margin.y + box_height - radius);
    cairo_arc(cr, x + margin.x + box_width - radius, y + margin.y + box_height - radius, radius, 0, M_PI_2);
    cairo_line_to(cr, x + margin.x + radius, y + margin.y + box_height);
    cairo_arc(cr, x + margin.x + radius, y + margin.y + box_height - radius, radius, M_PI_2, M_PI);
    cairo_line_to(cr, x + margin.x, y + margin.y + radius);
    cairo_arc(cr, x + margin.x + radius, y + margin.y + radius, radius, M_PI, M_PI_2 * 3);
    cairo_close_path(cr);
    cairo_stroke(cr);

    // ==========================================
    // 绘制绿色的对钩 (Checkmark)
    // ==========================================
    if (!is_float_nposm(checkmark_line_width)) {
        cairo_set_source_rgb(cr, 0.16, 0.71, 0.58);
        cairo_set_line_width(cr, checkmark_line_width);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);

        // 计算相对比例 (以 256x256 为基准)
        double scale_w = width / 256.0;
        double scale_h = height / 256.0;

        // ==========================================
        // 进一步放大对钩坐标，使其占据更大空间
        // 原始坐标 (256x256 时的位置) 被放大，更靠近边框
        // ==========================================
    
        // 左臂起点 - 更靠左靠上
        double base_x1 = 55.0, base_y1 = 100.0;
        // 底部转折点 - 更靠下
        double base_x2 = 110.0, base_y2 = 185.0;
        // 右臂终点 - 更靠右靠上
        double base_x3 = 195.0, base_y3 = 55.0;

        // 应用比例和偏移
        cairo_move_to(cr, x + base_x1 * scale_w, y + base_y1 * scale_h);
        cairo_line_to(cr, x + base_x2 * scale_w, y + base_y2 * scale_h);
        cairo_line_to(cr, x + base_x3 * scale_w, y + base_y3 * scale_h);

        cairo_stroke(cr);
    }
}

// 标签结构体
struct ttab_C
{
    double x;
    double y;
    double width;
    double height;
    bool selected;
};

// Draw a single label
void draw_tab(cairo_t* cr, const ttab_C& tab)
{
    SDL_DColor fill_color{0.75, 0.75, 0.75, 1.0};
    SDL_DColor line_color{0.5, 0.5, 0.5, 1.0};
    double line_width = 1.0;
    double radius = 8;
    if (tab.selected) {
        fill_color = SDL_DColor{1.0, 1.0, 1.0, 1.0};
    }
    draw_rounded_rectangle2(&fill_color, nullptr, line_width, cr, tab.x, tab.y, tab.width, tab.height, radius);
}

struct Button
{
    double x, y, width, height;
    bool hover;
    bool pressed;
    
    Button(double x_, double y_, double w_, double h_)
        : x(x_), y(y_), width(w_), height(h_), hover(false), pressed(false) {}
    
    bool contains(double px, double py) const {
        return (px >= x && px <= x + width && py >= y && py <= y + height);
    }

    SDL_Rect get_rect() const { return SDL_Rect{(int)x, (int)y, (int)width, (int)height}; }
};

void drawButton(cairo_t* cr, const Button& btn, int type, double inner_size = 16 * gui2::twidget::hdpi_scale, const SDL_DColor* bg_color = nullptr)
{
    VALIDATE(type >= 0 && type < pl_btn_count, null_str);
    // Button background - semi-transparent
    if (bg_color == nullptr) {
        if (btn.hover) {
            cairo_set_source_rgba(cr, 0.4, 0.6, 0.9, 0.85);
        } else {
            cairo_set_source_rgba(cr, 0.1, 0.15, 0.25, 0.8);
            {
                // cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 1.0);
                int ii = 0;
            }
        }
    } else {
        cairo_set_source_rgba(cr, bg_color->r, bg_color->g, bg_color->b, bg_color->a);
    }
        
    double radius = 6;
    cairo_move_to(cr, btn.x + radius, btn.y);
    cairo_line_to(cr, btn.x + btn.width - radius, btn.y);
    cairo_arc(cr, btn.x + btn.width - radius, btn.y + radius, radius, -M_PI/2, 0);
    cairo_line_to(cr, btn.x + btn.width, btn.y + btn.height - radius);
    cairo_arc(cr, btn.x + btn.width - radius, btn.y + btn.height - radius, radius, 0, M_PI/2);
    cairo_line_to(cr, btn.x + radius, btn.y + btn.height);
    cairo_arc(cr, btn.x + radius, btn.y + btn.height - radius, radius, M_PI/2, M_PI);
    cairo_line_to(cr, btn.x, btn.y + radius);
    cairo_arc(cr, btn.x + radius, btn.y + radius, radius, M_PI, 3*M_PI/2);
    cairo_close_path(cr);
    cairo_fill(cr);
        
    double cx = btn.x + btn.width/2;
    double cy = btn.y + btn.height/2;
    cairo_set_source_rgb(cr, 1, 1, 1);
        
    double size = inner_size;
    switch (type) {
        case pl_btn_play: drawPlayIcon(cr, cx, cy, size); break;
        case pl_btn_pause: drawPauseIcon(cr, cx, cy, size); break;
        case pl_btn_restart: drawRestartIcon(cr, cx, cy, size); break;
        case pl_btn_step_backward: drawStepbackwardIcon(cr, cx, cy, size); break;
        case pl_btn_step_forward: drawStepforwardIcon(cr, cx, cy, size); break;
        case pl_btn_stop: drawStopIcon(cr, cx, cy, size); break;
    }
}

cv::Mat argb_mat_from_CAIRO_FORMAT_ARGB32(cairo_surface_t* cairo_surf, uint8_t* data, int width, int height)
{
    VALIDATE(cairo_surf != nullptr && data != nullptr && width > 0 && height > 0, null_str);

    bool use_flush = false;
    if (use_flush) {
        cairo_surface_flush(cairo_surf);
    }

    cv::Mat tmp = cv::Mat(height, width, CV_8UC4, data);
    cv::Mat result = tmp.clone();
    // cv::cvtColor(tmp, result, cv::COLOR_BGRA2RGBA);

    return result;
/*
    // SDL_Surface* surf = SDL_CreateRGBSurfaceFrom(buffer.data(), width, height, 
    //    32, stride, 0xFF0000, 0x00FF00, 0xFF, 0xFF000000);
    surface surf = SDL_CreateRGBSurfaceFrom(buffer.data(), width, height, 
        32, stride, 0x0000FF, 0x00FF00, 0xFF0000, 0xFF000000);
*/
}

surface draw_obj(int width, int height, const SDL_DColor& canvas_color,
    const std::function<void (cairo_t* cr, int width, int height)>& did_draw_obj)
{
    surface result;
    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
    std::vector<unsigned char> buffer(height * stride);
    
    // ===== 2. Create a Cairo surface and context =====
    cairo_surface_t* cairo_surf = cairo_image_surface_create_for_data(
        buffer.data(),
        CAIRO_FORMAT_ARGB32,
        width,
        height,
        stride
    );
    
    if (cairo_surface_status(cairo_surf) != CAIRO_STATUS_SUCCESS) {
        SDL_Log("Failed to create surface");
        return result;
    }
    
    cairo_t* cr = cairo_create(cairo_surf);

    // ===== 4. Set white background =====
    // draw_canvas(cr, width, height, radius, canvas_color, NULL);
    cairo_set_source_rgba(cr, canvas_color.r, canvas_color.g, canvas_color.b, canvas_color.a);
    cairo_paint(cr);

    if (did_draw_obj != NULL) {
        did_draw_obj(cr, width, height);
    }

    // ===== 14. Save image and clean up resources =====
    // result = argb_mat_from_CAIRO_FORMAT_ARGB32(surface, buffer.data(), width, height);

    // buffer.data() will be free after this function, here must use 'independent_pixels = true'.
    // Keep 'use_independent_pixels', just to say that if you want to use independent and shared memory, 
    // you can use the 'use_independent_pixels=false' logic.
    const bool use_independent_pixels = true;
    if (use_independent_pixels) {
        int channels = 4;
        SDL_Surface* photo = SDL_CreateRGBSurface(0, width, height, channels * 8,
			    0xFF0000, 0xFF00, 0xFF, 0xFF000000); // SDL_PIXELFORMAT_ARGB8888
	    uint32_t* pixels = (uint32_t*)photo->pixels;
        memcpy(pixels, buffer.data(), width * height * 4);
        result = photo;

    } else {
        result = SDL_CreateRGBSurfaceFrom(buffer.data(), width, height, 
            32, stride, 0xFF0000, 0x00FF00, 0xFF, 0xFF000000);
    }

    // cairo_surface_write_to_png(surface, (game_config::preferences_dir + "/line_chart.png").c_str());
    // SDL_Log("Line chart saved to line_chart.png");
    
    cairo_destroy(cr);
    cairo_surface_destroy(cairo_surf);

    return result;
}

surface draw_rounded_rectangle_for_shape(const SDL_Size& bg_size, int margin, bool sel, const SDL_DColor& canvas_color, const SDL_DColor* fill_color, const SDL_DColor* line_color, double line_width, double radius,
    bool ltop, bool rtop, bool lbottom, bool rbottom)
{
    return cairo::draw_obj(bg_size.w, bg_size.h, canvas_color, 
        std::bind(&cairo::draw_rounded_rectangle_8points, bg_size, margin, sel, fill_color, line_color, line_width, _1, radius, ltop, rtop, lbottom, rbottom));
}

SDL_Rect get_arrow_bounding_rect_sdl(const SDL_Point& start, const SDL_Point& end,
            double arrow_size, double line_width, double arrow_width_ratio = 0.45) {
    
    double dx = end.x - start.x;
    double dy = end.y - start.y;
    double len = sqrt(dx * dx + dy * dy);
    
    if (len < 0.001) {
        SDL_Rect empty = {(int)start.x, (int)start.y, 0, 0};
        return empty;
    }
    
    // Unit direction vector
    double udx = dx / len;
    double udy = dy / len;
    double perp_x = -udy;
    double perp_y = udx;
    
    double arrow_base_width = arrow_size * arrow_width_ratio;
    
    // Calculate the coordinates of the two points at the base of the arrow
    double arrow_base1_x = end.x - arrow_size * udx + arrow_base_width * perp_x;
    double arrow_base1_y = end.y - arrow_size * udy + arrow_base_width * perp_y;
    double arrow_base2_x = end.x - arrow_size * udx - arrow_base_width * perp_x;
    double arrow_base2_y = end.y - arrow_size * udy - arrow_base_width * perp_y;
    
    // End point of the main line
    double line_end_x = end.x - arrow_size * udx * 0.3;
    double line_end_y = end.y - arrow_size * udy * 0.3;
    
    // Collect all coordinates (double)
    double xs[] = {(double)start.x, line_end_x, (double)end.x, arrow_base1_x, arrow_base2_x};
    double ys[] = {(double)start.y, line_end_y, (double)end.y, arrow_base1_y, arrow_base2_y};
    
    double half_width = line_width / 2.0;
    
    // The points type of SDL_EnclosePoints(...) is SDL_Point, 
    // which does not support SDL_DPoint. 
    // However, to improve precision here, SDL_DPoint must be used.
    double min_x = xs[0], max_x = xs[0];
    double min_y = ys[0], max_y = ys[0];    
    for (int i = 1; i < 5; i++) {
        if (xs[i] < min_x) {
            min_x = xs[i];
        } else if (xs[i] > max_x) {
            max_x = xs[i];
        }
        if (ys[i] < min_y) {
            min_y = ys[i];
        } else if (ys[i] > max_y) {
            max_y = ys[i];
        }
    }
    
    // Expand line width
    min_x -= half_width;
    max_x += half_width;
    min_y -= half_width;
    max_y += half_width;
    
    SDL_Rect result;
    result.x = (int)floor(min_x);
    result.y = (int)floor(min_y);
    result.w = (int)ceil(max_x - min_x);
    result.h = (int)ceil(max_y - min_y);
    
    return result;
}

surface draw_line_width_arrow_for_shape(const SDL_Point& start, const SDL_Point& end, const SDL_DColor& color, double arrow_size, double line_width, double arrow_width_ratio, SDL_Rect* _enclose_rect)
{
    // SDL_Point points[] = {start, end};
    SDL_Rect enclose_rect;
    // SDL_Rect enclose_rect2;
    // SDL_EnclosePoints(points, sizeof(points) / sizeof(points[0]), nullptr, &enclose_rect2);
    enclose_rect = get_arrow_bounding_rect_sdl(start, end, arrow_size, line_width, arrow_width_ratio);
    if (_enclose_rect != nullptr) {
        *_enclose_rect = enclose_rect;
    }

    SDL_DColor canvas_color{0.0, 0.0, 0.0, 0.0};
    SDL_DPoint start2{(double)(start.x - enclose_rect.x), (double)(start.y - enclose_rect.y)};
    SDL_DPoint end2{(double)(end.x - enclose_rect.x), (double)(end.y - enclose_rect.y)};

    return cairo::draw_obj(enclose_rect.w, enclose_rect.h, canvas_color, 
        std::bind(&cairo::draw_line_with_arrow, _1, start2, end2, color, arrow_size, line_width, arrow_width_ratio));
}

surface draw_rounded_rectangle_for_text_surf(const surface& text_surf, const SDL_Size& margin, ticon* icon, const SDL_DColor& fill_color, const SDL_DColor& line_color, double line_width, double radius,
    bool ltop, bool rtop, bool lbottom, bool rbottom)
{
    const SDL_DColor canvas_color{1.0, 1.0, 1.0, 0.0};
    SDL_Size bg_size {margin.w * 2 + text_surf->w, margin.h * 2 + text_surf->h};
    double x = 0;
    double y = 0;
    if (icon != nullptr) {
        bg_size.w += icon->size.w + icon->padding;
        icon->set_xy(x + margin.w, y + (bg_size.h - icon->size.h) / 2);
    }
    surface bg_surf = cairo::draw_obj(bg_size.w, bg_size.h, canvas_color, 
        std::bind(&cairo::draw_rounded_rectangle3, icon, &fill_color, &line_color, line_width, _1, 
            x, y, bg_size.w, bg_size.h, radius, ltop, rtop, lbottom, rbottom));

    SDL_Rect dst_rect;
    if (icon != nullptr && icon->shape_type == shapetype_surf) {
        dst_rect= SDL_Rect{(int)icon->x, (int)icon->y, icon->surf->w, icon->surf->h};
        sdl_blit(icon->surf, nullptr, bg_surf, &dst_rect);
    }

    dst_rect = SDL_Rect{margin.w, margin.h, text_surf->w, text_surf->h};
    if (icon != nullptr) {
        dst_rect.x += icon->size.w + icon->padding;
    }
    sdl_blit(text_surf, nullptr, bg_surf, &dst_rect);


    return bg_surf;
}

// Helper function: Normalize data to the height of the drawing area
double normalize(double value, double min_val, double max_val, double chart_height)
{
    if (max_val == min_val) {
        return chart_height / 2;
    }
    return ((value - min_val) / (max_val - min_val)) * chart_height;
}

double normalize2(double value, double max_value, double chart_height)
{
    return (value / max_value) * chart_height;
}

void tsdl_field::set(const std::string& _icon, const std::string& _name, int _name_font_size, 
    const std::string& _val, int _val_font_size, const SDL_Point& _margin, const SDL_Point& _gap)
{
	VALIDATE(!_name.empty(), null_str);
	VALIDATE(_name_font_size != 0, null_str);

    icon = _icon;

	name = _name;
	name_font_size = _name_font_size;
	name_text_size = font::get_rendered_text_size(name, INT32_MAX, name_font_size);

	val = _val;
	if (!val.empty()) {
		VALIDATE(_val_font_size != 0, null_str);
		val_font_size = _val_font_size;
		val_text_size = font::get_rendered_text_size(val, INT32_MAX, val_font_size);

	} else {
		VALIDATE(_val_font_size == 0, null_str);
		val_font_size = 0;
		val_text_size = tpoint(0, 0);
	}

	margin = _margin;
	gap = _gap;
}

void draw_check_icon2(cairo_t* cr, bool check_marker, SDL_Rect& btn_share_rect)
{
    double icon_size = 51.2 * gui2::twidget::hdpi_scale; // 64(51.2 * 1.25)
    // const SDL_Rect icon = create_rect(margin.x, height - margin.y - icon_size, icon_size, icon_size);;
    const SDL_Rect icon = create_rect(9, 0, icon_size, icon_size);;
    btn_share_rect = icon;
    double board_line_width = 6.0; // 6(4.8 * 1.25)
    double mark_line_width = 8.0 * gui2::twidget::hdpi_scale; // 10(8 * 1.25)
    draw_check_icon(cr, icon.w, icon.h, board_line_width, SDL_DPoint{2, 2}, 10.0, icon.x, icon.y, check_marker? mark_line_width: float_nposm);
}

cv::Mat draw_header_mat(bool to_image, int width, int height, double radius, const SDL_Point& margin, theader_fields& fields)
{
    VALIDATE(fields.pl_btn_rects != nullptr, null_str);
    memset(fields.pl_btn_rects, 0, sizeof(SDL_Rect) * pl_btn_count);

    cv::Mat result;

    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
    std::vector<unsigned char> buffer(height * stride);
    
    // ===== 3. Create a Cairo surface and context =====
    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        buffer.data(),
        CAIRO_FORMAT_ARGB32,
        width,
        height,
        stride
    );
    
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        SDL_Log("Failed to create surface");
        return result;
    }
    
    cairo_t* cr = cairo_create(surface);

    // ===== 4. Set white background =====
    draw_canvas(cr, width, height, radius, SDL_DColor{1.0, 1.0, 1.0, 1.0}, NULL, !to_image, !to_image, false, false);

    // ===== 5. Draw my image =====
    // int margin_left = margin.x;
    // int margin_right = 20;
    // int margin_top = margin.y;
    // int margin_bottom = 15;

    const int chart_width = width - margin.x * 2;

    const int TOTAL_BARS = fields.fid_count;
    double gap_ratio = 0.1;

    // (TOTAL_BARS - 1) * column_width + (column_width * (1 - 0.1)) = chart_width
    // x = column_width
    // => (TOTAL_BARS - 1) * x + x * 0.9 = chart_width
    //    (TOTAL_BARS - 0.1) * x = chart_width
    //    => x = chart_width / (TOTAL_BARS - 0.1)
    double column_width = chart_width / (TOTAL_BARS - gap_ratio);
    double gap = column_width * gap_ratio;
    double bar_width = column_width - gap;
    double bar_height = height - margin.y * 2;

    SDL_DColor line_color{237.0 / 255, 237.0 / 255, 237.0 / 255, 1.0};
    for (int at = 0; at < TOTAL_BARS; at ++) {
        int x = margin.x + column_width * at;
        int y = margin.y;
        draw_rounded_rectangle2(nullptr, &line_color, 1.5, cr, x, y, bar_width, bar_height, radius);

        fields.arrays[at]->offset.x = x;
        fields.arrays[at]->offset.y = y;
    }

    if (fields.share != bool_set_none) {
        draw_check_icon2(cr, fields.share == bool_set_true, fields.pl_btn_rects[pl_btn_share]);
    }
    // ===== 14. Save image and clean up resources =====
    result = argb_mat_from_CAIRO_FORMAT_ARGB32(surface, buffer.data(), width, height);
    
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    return result;
}

// Draw a line chart
cv::Mat draw_sit_dual_axis_stacked_bar_chart(bool to_image, int width, int height, double radius, const SDL_Point& margin,
    tsit_fields& fields)
{
    VALIDATE(fields.pl_btn_rects != nullptr, null_str);
    memset(fields.pl_btn_rects, 0, sizeof(SDL_Rect) * pl_btn_count);

    cv::Mat result;
    // ===== 1. Prepare data =====
    const int TOTAL_COLUMNS = ONE_DAY_HOURS;
    char buf[32];

    const int cairo_font_size = 16;

    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
    std::vector<unsigned char> buffer(height * stride);
    
    // ===== 2. Create a Cairo surface and context =====
    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        buffer.data(),
        CAIRO_FORMAT_ARGB32,
        width,
        height,
        stride
    );
    
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        SDL_Log("Failed to create surface");
        return result;
    }
    
    cairo_t* cr = cairo_create(surface);
    
    // ===== 3. Set canvas parameters =====
    const int Y_axis_label_chart_gap = 20;
    cairo_text_extents_t extents;
    cairo_set_font_size(cr, cairo_font_size);
    cairo_text_extents(cr, "55.5", &extents);
    // int chart_margin_left = 80;
    int chart_margin_left = margin.x + extents.width + Y_axis_label_chart_gap;
    // int chart_margin_right = 80;
    int chart_margin_right = chart_margin_left;
    int chart_margin_top = margin.y + fields.title_height + fields.legend_height;
    int chart_margin_bottom = fields.time_labels_height + margin.y;
    // int chart_margin_bottom = 60;
    
    int chart_width = width - chart_margin_left - chart_margin_right;
    int chart_height = height - chart_margin_top - chart_margin_bottom;

    // ===== 4. Set white background =====
    draw_canvas(cr, width, height, radius, SDL_DColor{1.0, 1.0, 1.0, 1.0}, NULL, false, false, !to_image, !to_image);
    
    // ===== 5. Draw grid and axes =====
    cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);
    cairo_set_line_width(cr, 0.5);
    
    // Find the maximum max_duration of all data for normalization
    double max_sit_duration = 0;
    for (int i = 0; i < TOTAL_COLUMNS; i++) {
        max_sit_duration = SDL_max(max_sit_duration, fields.sit_durations[i] / 60.0);
    }
    // max_sit_duration = posix_align_ceil2((int)max_sit_duration + 9, 10);

    double max_type_improper_duration = 0;
    for (int i = 0; i < TOTAL_COLUMNS; i++) {
        double column_total = 0;
        for (std::vector<tsit_fields::ttype_improper>::const_iterator it = fields.type_impropers.begin(); it != fields.type_impropers.end(); ++ it) {
            const tsit_fields::ttype_improper& improper = *it;
            column_total += improper.durations[i];
        }
        max_type_improper_duration = SDL_max(max_type_improper_duration, column_total / 60.0);

        // both type_improper_duration and improper_alert use same y-axis range.
        max_type_improper_duration = SDL_max(max_type_improper_duration, fields.improper_alerts[i]);
    }
/*
    const SDL_DColor sit_duration_color{78 / 255.0, 175 / 255.0, 80 / 255.0, 1.0};
    // const SDL_DColor improper_duration_color{252 / 255.0, 84 / 255.0, 84 / 255.0, 1.0};
    const SDL_DColor improper_duration_color{1.0, 0.0, 0.0, 1.0};
    const SDL_DColor improper_alert_color{1.0, 195 / 255.0, 4 / 255.0, 1.0};
*/
    const SDL_DColor& sit_duration_color = fields.legend_sit_duration.cairo_color;
    const SDL_DColor& improper_duration_color = fields.legend_improper_duration.cairo_color;
    const SDL_DColor& improper_alert_color = fields.legend_improper_alert.cairo_color;

    // Draw horizontal grid lines
    const double min_sit_duration = 0;
    const double min_type_improper_duration = 0;
    int grid_lines = 6;
    for (int i = 0; i <= grid_lines; i++) {
        double y = chart_margin_top + (chart_height * i) / grid_lines;
        cairo_move_to(cr, chart_margin_left, y);
        cairo_line_to(cr, width - chart_margin_right, y);
        cairo_stroke(cr);

        // Add Y-axis labels
        double sit_value = max_sit_duration - (max_sit_duration - min_sit_duration) * i / grid_lines;
        std::stringstream ss;
        
        ss.str("");
        ss << std::fixed << std::setprecision(1) << sit_value;

        // left. labels are right-align.
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_set_font_size(cr, cairo_font_size);
        cairo_text_extents(cr, ss.str().c_str(), &extents);
        cairo_move_to(cr, chart_margin_left - Y_axis_label_chart_gap - extents.width / 2, y + 5); // y - 5
        cairo_show_text(cr, ss.str().c_str());

        // right. labels are left-align.
        double type_improper_value = max_type_improper_duration - (max_type_improper_duration - min_type_improper_duration) * i / grid_lines;
        ss.str("");
        ss << std::fixed << std::setprecision(1) << type_improper_value;
        // ss << (int)type_improper_value;
        // cairo_set_source_rgb(cr, 250.0 / 255, 198.0 / 255, 52.0 / 255);
        cairo_set_source_rgb(cr, improper_duration_color.r, improper_duration_color.g, improper_duration_color.b);
        cairo_move_to(cr, chart_margin_left + chart_width + 5, y + 5);
        // cairo_move_to(cr, chart_margin_left + chart_width + Y_axis_label_chart_gap, y + 5);
        cairo_show_text(cr, ss.str().c_str());

        cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);  // Restore grid color
    }
    
    // Draw vertical grid lines
    double column_width = (double)chart_width / TOTAL_COLUMNS;
    double gap = column_width * 0.2;
    double bar_width = column_width - gap;
    double bar_width_by_3 = bar_width / 3;

    for (int i = 0; i < TOTAL_COLUMNS; i++) {
        double x = chart_margin_left + i * column_width + gap / 2;
        double current_y = height - chart_margin_bottom;  // Stack from the bottom

        double segment_height = normalize2(fields.sit_durations[i] / 60.0, max_sit_duration, chart_height);
        double y = current_y - segment_height;
        draw_rounded_rectangle2(&sit_duration_color, nullptr, float_nposm, cr, x, y, bar_width_by_3, segment_height, 
                0, true, true, false, false);
        x += bar_width_by_3;

        segment_height = normalize2(fields.improper_durations[i] / 60.0, max_sit_duration, chart_height);
        y = current_y - segment_height;
        draw_rounded_rectangle2(&improper_duration_color, nullptr, float_nposm, cr, x, y, bar_width_by_3, segment_height, 
                0, true, true, false, false);
        x += bar_width_by_3;

        for (std::vector<tsit_fields::ttype_improper>::const_iterator it = fields.type_impropers.begin(); it != fields.type_impropers.end(); ++ it) {
            const tsit_fields::ttype_improper& type_improper = *it;
            segment_height = normalize2(type_improper.durations[i] / 60.0, max_type_improper_duration, chart_height);
            y = current_y - segment_height;

            draw_rounded_rectangle2(&type_improper.color, nullptr, float_nposm, cr, x, y, bar_width_by_3, segment_height, 
                5, true, true, false, false);
            
            current_y = y;  // Update Y position for stacking the next segment
        }
    }
    
    // ===== 6. Draw axes (bold black) =====
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, 2.0);
 /*   
    // y-axis
    cairo_move_to(cr, chart_margin_left, chart_margin_top);
    cairo_line_to(cr, chart_margin_left, height - chart_margin_bottom);
    cairo_stroke(cr);
*/    
    // x-axis
    cairo_move_to(cr, chart_margin_left, height - chart_margin_bottom);
    cairo_line_to(cr, width - chart_margin_right, height - chart_margin_bottom);
    cairo_stroke(cr);
    
    // ===== 7. Calculate point coordinates =====
    std::vector<SDL_DPoint> points(TOTAL_COLUMNS);
    double x_step = (double)chart_width / (TOTAL_COLUMNS - 1);
    
    for (int i = 0; i < TOTAL_COLUMNS; i++) {
        double x = chart_margin_left + i * column_width + column_width / 2;
        double y = height - chart_margin_bottom - normalize(fields.improper_alerts[i], 
            min_type_improper_duration, max_type_improper_duration, chart_height);
        points[i] = {x, y};
    }
    
    // ===== 8. Draw polyline =====
    bool use_catmull_rom = true;
    if (use_catmull_rom) {
        draw_smooth_curve_optimized(cr, improper_alert_color, 2.5, points);

    } else {
        cairo_set_source_rgb(cr, improper_alert_color.r, improper_alert_color.g, improper_alert_color.b);  // blue color
        cairo_set_line_width(cr, 2.5);
    
        // Move to the first point
        cairo_move_to(cr, points[0].x, points[0].y);
    
        // Connect to all subsequent points
        for (int i = 1; i < TOTAL_COLUMNS; i++) {
            const SDL_DPoint& from = points[i - 1];
            const SDL_DPoint& to = points[i];
            if (from.y == to.y) {
                cairo_line_to(cr, to.x, to.y);
            } else {
                draw_curved_line(cr, from.x, from.y, to.x, to.y);
            }
        }

        // Stroke the polyline
        cairo_stroke(cr);
    }
    
    // ===== 9. Draw data point markers =====
    for (int i = 0; i < TOTAL_COLUMNS; i++) {
        double x = points[i].x;
        double y = points[i].y;
        
        double point_radius = 5;

        // Draw white filled circles
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_arc(cr, x, y, point_radius, 0, 2 * M_PI);
        cairo_fill(cr);
        
        // Draw blue border
        cairo_set_source_rgb(cr, improper_alert_color.r, improper_alert_color.g, improper_alert_color.b);
        cairo_set_line_width(cr, 2.0);
        cairo_arc(cr, x, y, point_radius, 0, 2 * M_PI);
        cairo_stroke(cr);
/*
        // Add value labels above the points
        const std::string label = str_cast(fields.improper_alerts[i]);
        
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_set_font_size(cr, 9);
        cairo_move_to(cr, x - 15, y - 15);
        cairo_show_text(cr, label.c_str());
*/
    }
    
    // ===== 10. Add X-axis labels =====
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_font_size(cr, cairo_font_size);

    std::vector<std::string> time_labels;
    for (int i = 0; i < TOTAL_COLUMNS; i++) {
        // Generate labels
        SDL_snprintf(buf, sizeof(buf), "%02i:00", i);
        time_labels.push_back(buf);
    }

    cairo_text_extents(cr, time_labels[TOTAL_COLUMNS / 2].c_str(), &extents);
    bool use_rotate = column_width < extents.width + 15;
    // SDL_Log("{dbg}column_width: %.5f, extents.width: %.5f", column_width, extents.width);

    // cairo_save(cr);
    for (int i = 0; i < TOTAL_COLUMNS; i++) {
        double x = chart_margin_left + i * column_width + column_width / 2;
        
        if (!use_rotate) {
            // Get text dimensions for centering
            cairo_text_extents(cr, time_labels[i].c_str(), &extents);
            // Center the text at (0,0)
            cairo_move_to(cr, x - extents.width/2, height - chart_margin_bottom + 12 + extents.height/2);

            // cairo_move_to(cr, 0, 0);

            cairo_show_text(cr, time_labels[i].c_str());

        } else {
            cairo_save(cr);
            cairo_translate(cr, x, height - chart_margin_bottom + 15);
            cairo_rotate(cr, -M_PI / 4);

            // Get text dimensions for centering
            cairo_text_extents(cr, time_labels[i].c_str(), &extents);
            // Center the text at (0,0)
            cairo_move_to(cr, -extents.width/2, extents.height/2);

            // cairo_move_to(cr, 0, 0);

            cairo_show_text(cr, time_labels[i].c_str());
            cairo_restore(cr);
        }
    }
    // cairo_restore(cr);
    
    // ===== 11. Add title and axis labels =====
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    
    // Main title
    fields.title.offset = SDL_Point{margin.x, margin.y};

    // Today
    tsdl_field* sdl_field = &fields.today;
    int today_gap_x = 24;
    SDL_Rect today_rect{0, margin.y, 2 * today_gap_x + sdl_field->name_text_size.x, fields.title.name_text_size.y};
    today_rect.x = width - margin.x - today_rect.w;
    draw_rounded_rectangle2(&sdl_field->cairo_color, nullptr, float_nposm, cr, today_rect.x, today_rect.y,
        today_rect.w, today_rect.h, radius);
    sdl_field->offset = SDL_Point{today_rect.x + today_gap_x, 
        margin.y + (fields.title.name_text_size.y - sdl_field->name_text_size.y) / 2};
    
    // X-axis title
    sdl_field = &fields.chart_remark;
    sdl_field->offset = SDL_Point{margin.x, height - margin.y - sdl_field->name_text_size.y};
/*
    cairo_set_font_size(cr, 14);
    cairo_move_to(cr, width/2 - 30, height - 10);
    cairo_show_text(cr, "Data Points");
*/    
    // Y-axis title (rotated)
/*
    cairo_save(cr);
    cairo_translate(cr, 20, height/2);
    cairo_rotate(cr, -M_PI/2);
    cairo_set_font_size(cr, 14);
    cairo_move_to(cr, 0, 0);
    cairo_show_text(cr, "Values");
    cairo_restore(cr);
*/    
    // ===== 12. Add legend =====
    const int legend_x = margin.x;
    const int legend_y = margin.y + fields.title_height;
    const int legend_icon_text_gap_x = 3;
    SDL_Point legend_2legend_gap{15, fields.legend_2legend_gap_y};
    SDL_Point legend_size{30, posix_align_ceil2(fields.legend_improper_alert.name_text_size.y - 4, 2)};

    // The legend is split into two lines, both left-aligned. 
    // The first line contains three fixed items, and the second line contains various reasons for improper posture.
    int fixed_legends[] = {fields.fid_legend_sit_duration, 
        fields.fid_legend_improper_duration, 
        fields.fid_legend_improper_alert};
    int fixed_legend_count = sizeof(fixed_legends) / sizeof(fixed_legends[0]);
    int tmp_legend_x = legend_x;
    int tmp_legend_y = legend_y;
    int max_1th_line_height = 0;
    for (int at = 0; at < fixed_legend_count; at ++) {
        int fid = fixed_legends[at];
        sdl_field = fields.arrays[fid];

        int x = tmp_legend_x;
        int y = tmp_legend_y;
        if (fid != fields.fid_legend_improper_alert) {
            draw_rounded_rectangle2(&sdl_field->cairo_color, nullptr, float_nposm, cr, x, 
                y + (sdl_field->name_text_size.y - legend_size.y) / 2, legend_size.x, legend_size.y, 4);
        } else {
            int mid_offset = fields.legend_improper_alert.name_text_size.y / 2;
            // Draw legend line
            cairo_set_source_rgb(cr, improper_alert_color.r, improper_alert_color.g, improper_alert_color.b);
            cairo_set_line_width(cr, 2.5);
            cairo_move_to(cr, x, y + mid_offset);
            cairo_line_to(cr, x + legend_size.x, y + mid_offset);
            cairo_stroke(cr);
    
            // Draw legend point
            cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
            cairo_arc(cr, x + legend_size.x / 2, y + mid_offset, 5, 0, 2 * M_PI);
            cairo_fill(cr);
            // cairo_fill_preserve(cr);
    
            cairo_set_source_rgb(cr, improper_alert_color.r, improper_alert_color.g, improper_alert_color.b);
            cairo_arc(cr, x + legend_size.x / 2, y + mid_offset, 5, 0, 2 * M_PI);
            cairo_stroke(cr);
        }

        sdl_field->offset.x = tmp_legend_x + legend_size.x + legend_icon_text_gap_x;
        sdl_field->offset.y = tmp_legend_y; // (sdl_field->name_text_size.y / 2);

        tmp_legend_x = sdl_field->offset.x + sdl_field->name_text_size.x + legend_2legend_gap.x;
        max_1th_line_height = SDL_max(max_1th_line_height, sdl_field->name_text_size.y);
    }

    const int bonus_gap_x = legend_2legend_gap.x;
    const int non_fixed_1th_legend_x = tmp_legend_x + bonus_gap_x;
    bool switched_to_1th = false;

    // 2th line
    tmp_legend_x = legend_x;
    tmp_legend_y = legend_y + max_1th_line_height + legend_2legend_gap.y;
    for (std::vector<tsit_fields::ttype_improper>::iterator it = fields.type_impropers.begin(); it != fields.type_impropers.end(); ++ it) {
        tsit_fields::ttype_improper& improper = *it;

        SDL_DColor fill_color = improper.color;
        int x = tmp_legend_x;
        int y = tmp_legend_y;

        const int this_label_offset_x = x + legend_size.x + legend_icon_text_gap_x;
        if (!switched_to_1th && (this_label_offset_x + improper.label.name_text_size.x > width - margin.x)) {
            // 2th line can’t fit, so move it to 1th line.
            switched_to_1th = true;
            x = non_fixed_1th_legend_x;
            y = legend_y;
            tmp_legend_y = y;
        }

        draw_rounded_rectangle2(&fill_color, nullptr, float_nposm, cr, x, 
            y + (improper.label.name_text_size.y - legend_size.y) / 2, legend_size.x, legend_size.y, 4);

        improper.label.offset.x = x + legend_size.x + legend_icon_text_gap_x;
        improper.label.offset.y = y;

        tmp_legend_x = improper.label.offset.x + improper.label.name_text_size.x + legend_2legend_gap.x;
    }
   
    // ===== 13. Add statistical information =====
    sdl_field = &fields.left_y_axis;
    sdl_field->offset.x = chart_margin_left - sdl_field->name_font_size * 3;
    sdl_field->offset.y = chart_margin_top - fields.y_axis_title_gap_y - sdl_field->name_text_size.y;

    sdl_field = &fields.right_y_axis;
    sdl_field->offset.x = chart_margin_left + chart_width - sdl_field->name_text_size.x + sdl_field->name_font_size * 3;
    sdl_field->offset.y = chart_margin_top - fields.y_axis_title_gap_y - sdl_field->name_text_size.y;

    if (fields.share != bool_set_none) {
        draw_check_icon2(cr, fields.share == bool_set_true, fields.pl_btn_rects[pl_btn_share]);
    }

    // ===== 14. Save image and clean up resources =====
    result = argb_mat_from_CAIRO_FORMAT_ARGB32(surface, buffer.data(), width, height);

    // cairo_surface_write_to_png(surface, (game_config::preferences_dir + "/line_chart.png").c_str());
    // SDL_Log("Line chart saved to line_chart.png");
    
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    
    return result;
}

cv::Mat draw_days_posture_dual_axis_stacked_bar_chart(bool to_image, int width, int height, double radius, const SDL_Point& margin,
    int days, tdays_posture_fields& fields)
{
    VALIDATE_POSTURE_DAYS(days);

    cv::Mat result;
    // ===== 1. Prepare data =====
    const int TOTAL_COLUMNS = days;
    VALIDATE((int)fields.bar_4labels.size() == days, null_str);
    char buf[32];

    const int cairo_font_size = 16;

    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
    std::vector<unsigned char> buffer(height * stride);
    
    // ===== 2. Create a Cairo surface and context =====
    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        buffer.data(),
        CAIRO_FORMAT_ARGB32,
        width,
        height,
        stride
    );
    
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        SDL_Log("Failed to create surface");
        return result;
    }
    
    cairo_t* cr = cairo_create(surface);
    
    // ===== 3. Set canvas parameters =====
    cairo_text_extents_t extents;
    cairo_set_font_size(cr, cairo_font_size);
    cairo_text_extents(cr, "55.5", &extents);
    // int chart_margin_left = 80;
    // int chart_margin_left = margin.x + extents.width + Y_axis_label_chart_gap;
    int chart_margin_left = margin.x + fields.Y_axis_label_width + fields.Y_axis_label_chart_gap;
    // int chart_margin_right = 80;
    int chart_margin_right = chart_margin_left;
    int chart_margin_top = margin.y + fields.title_height + fields.legend_height;
    int chart_margin_bottom = fields.day_labels_height + margin.y;
    // int chart_margin_bottom = 60;
    
    int chart_width = width - chart_margin_left - chart_margin_right;
    int chart_height = height - chart_margin_top - chart_margin_bottom;

    // ===== 4. Set white background =====
    draw_canvas(cr, width, height, radius, SDL_DColor{1.0, 1.0, 1.0, 1.0}, NULL, !to_image, !to_image, !to_image, !to_image);
    
    // ===== 5. Draw grid and axes =====
    cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);
    cairo_set_line_width(cr, 0.5);
    
    // Find the maximum max_duration of all data for normalization
    double max_sit_duration = 0;
    for (int i = 0; i < TOTAL_COLUMNS; i++) {
        // max_sit_duration = SDL_max(max_sit_duration, fields.sit_durations[i] / 60.0);
        max_sit_duration = SDL_max(max_sit_duration, fields.sit_durations[i]);
    }
    // max_sit_duration = posix_align_ceil2((int)max_sit_duration + 9, 10);

    double max_type_improper_duration = 0;
    for (int i = 0; i < TOTAL_COLUMNS; i++) {
        // both type_improper_duration and improper_alert use same y-axis range.
        max_type_improper_duration = SDL_max(max_type_improper_duration, fields.improper_alerts[i]);
    }
/*
    const SDL_DColor sit_duration_color{78 / 255.0, 175 / 255.0, 80 / 255.0, 1.0};
    // const SDL_DColor improper_duration_color{252 / 255.0, 84 / 255.0, 84 / 255.0, 1.0};
    const SDL_DColor improper_duration_color{1.0, 0.0, 0.0, 1.0};
    const SDL_DColor improper_alert_color{1.0, 195 / 255.0, 4 / 255.0, 1.0};
*/
    const SDL_DColor& sit_duration_color = fields.legend_sit_duration.cairo_color;
    const SDL_DColor& improper_duration_color = fields.legend_improper_duration.cairo_color;
    const SDL_DColor& improper_alert_color = fields.legend_improper_alert.cairo_color;

    // Draw horizontal grid lines
    const double min_sit_duration = 0;
    const double min_type_improper_duration = 0;
    int grid_lines = 6;
    for (int i = 0; i <= grid_lines; i++) {
        double y = chart_margin_top + (chart_height * i) / grid_lines;
        cairo_move_to(cr, chart_margin_left, y);
        cairo_line_to(cr, width - chart_margin_right, y);
        cairo_stroke(cr);

        // Add Y-axis labels
        double sit_value = max_sit_duration - (max_sit_duration - min_sit_duration) * i / grid_lines;
        std::stringstream ss;
        
        ss.str("");
        // ss << std::fixed << std::setprecision(1) << sit_value;
        ss << utils::format_elapse_hm_or_ms(sit_value, false, utils::timesep_unit);

        // left. labels are right-align.
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_set_font_size(cr, cairo_font_size);
        cairo_text_extents(cr, ss.str().c_str(), &extents);
        cairo_move_to(cr, chart_margin_left - fields.Y_axis_label_chart_gap - extents.width / 2, y + 5); // y - 5
        cairo_show_text(cr, ss.str().c_str());

        // right. labels are left-align.
        double type_improper_value = max_type_improper_duration - (max_type_improper_duration - min_type_improper_duration) * i / grid_lines;
        ss.str("");
        ss << std::fixed << std::setprecision(1) << type_improper_value;
        // ss << (int)type_improper_value;
        // cairo_set_source_rgb(cr, 250.0 / 255, 198.0 / 255, 52.0 / 255);
        cairo_set_source_rgb(cr, improper_alert_color.r, improper_alert_color.g, improper_alert_color.b);
        cairo_move_to(cr, chart_margin_left + chart_width + 5, y + 5);
        // cairo_move_to(cr, chart_margin_left + chart_width + Y_axis_label_chart_gap, y + 5);
        cairo_show_text(cr, ss.str().c_str());

        cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);  // Restore grid color
    }
    
    // Draw vertical grid lines
    double column_width = (double)chart_width / TOTAL_COLUMNS;
    double gap = column_width * 0.2;
    double bar_width = column_width - gap;
    double bar_width_by_2 = bar_width / 2;

    for (int i = 0; i < TOTAL_COLUMNS; i++) {
        double x = chart_margin_left + i * column_width + gap / 2;
        double current_y = height - chart_margin_bottom;  // Stack from the bottom
        // sit duration
        double segment_height = normalize2(fields.sit_durations[i], max_sit_duration, chart_height);
        double y = current_y - segment_height;
        draw_rounded_rectangle2(&sit_duration_color, nullptr, float_nposm, cr, x, y, bar_width_by_2, segment_height, 
                0, true, true, false, false);
        tsdl_field* field = &fields.bar_4labels[i].sit;
        field->offset.x = x + bar_width_by_2 / 2 - field->name_text_size.x / 2;
        field->offset.x -= SDL_min(gap / 2, 15);
        field->offset.y = y - field->name_text_size.y;

        x += bar_width_by_2;

        // improper duration
        segment_height = normalize2(fields.improper_durations[i], max_sit_duration, chart_height);
        y = current_y - segment_height;
        draw_rounded_rectangle2(&improper_duration_color, nullptr, float_nposm, cr, x, y, bar_width_by_2, segment_height, 
                0, true, true, false, false);
        field = &fields.bar_4labels[i].improper;
        field->offset.x = x + bar_width_by_2 / 2 - field->name_text_size.x / 2;
        field->offset.x += SDL_min(gap / 2, 15);
        field->offset.y = y - field->name_text_size.y;

        x += bar_width_by_2;
    }
    
    // ===== 6. Draw axes (bold black) =====
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, 2.0);
 /*   
    // y-axis
    cairo_move_to(cr, chart_margin_left, chart_margin_top);
    cairo_line_to(cr, chart_margin_left, height - chart_margin_bottom);
    cairo_stroke(cr);
*/    
    // x-axis
    cairo_move_to(cr, chart_margin_left, height - chart_margin_bottom);
    cairo_line_to(cr, width - chart_margin_right, height - chart_margin_bottom);
    cairo_stroke(cr);
    
    // ===== 7. Calculate point coordinates =====
    std::vector<SDL_DPoint> points(TOTAL_COLUMNS);
    double x_step = (double)chart_width / (TOTAL_COLUMNS - 1);
    
    for (int i = 0; i < TOTAL_COLUMNS; i++) {
        double x = chart_margin_left + i * column_width + column_width / 2;
        double y = height - chart_margin_bottom - normalize(fields.improper_alerts[i], 
            min_type_improper_duration, max_type_improper_duration, chart_height);
        points[i] = {x, y};
    }
    
    // ===== 8. Draw polyline =====
    // SDL_Log("{dbg-days}8. Draw polyline");
    bool use_catmull_rom = true;
    if (use_catmull_rom) {
        draw_smooth_curve_optimized(cr, improper_alert_color, 2.5, points);
/*
        int at = 0;
        SDL_Log("---points, size: %i---", (int)points.size());
        for (std::vector<SDL_DPoint>::const_iterator it = points.begin(); it != points.end(); ++ it, at ++) {
            const SDL_DPoint& point = *it;
            SDL_Log("[%i](%.3f, %.3f)", at, point.x, point.y);
        }
*/
    } else {
        cairo_set_source_rgb(cr, improper_alert_color.r, improper_alert_color.g, improper_alert_color.b);  // blue color
        cairo_set_line_width(cr, 2.5);
    
        // Move to the first point
        cairo_move_to(cr, points[0].x, points[0].y);
    
        // Connect to all subsequent points
        for (int i = 1; i < TOTAL_COLUMNS; i++) {
            const SDL_DPoint& from = points[i - 1];
            const SDL_DPoint& to = points[i];
            if (from.y == to.y) {
                cairo_line_to(cr, to.x, to.y);
            } else {
                draw_curved_line(cr, from.x, from.y, to.x, to.y);
            }
        }
    
        // Stroke the polyline
        cairo_stroke(cr);
    }
    
    // ===== 9. Draw data point markers =====
    // SDL_Log("{dbg-days}9. Draw data point markers");
    tsdl_field* sdl_field = nullptr;
    for (int i = 0; i < TOTAL_COLUMNS; i++) {
        double x = points[i].x;
        double y = points[i].y;
        
        double point_radius = 5;

        // Draw white filled circles
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_arc(cr, x, y, point_radius, 0, 2 * M_PI);
        cairo_fill(cr);
        
        // Draw blue border
        cairo_set_source_rgb(cr, improper_alert_color.r, improper_alert_color.g, improper_alert_color.b);
        cairo_set_line_width(cr, 2.0);
        cairo_arc(cr, x, y, point_radius, 0, 2 * M_PI);
        cairo_stroke(cr);

        // Add value labels above the points
        sdl_field = &fields.bar_4labels[i].improper_alert;
        sdl_field->offset.x = x - sdl_field->name_text_size.x / 2;
        sdl_field->offset.y = y - sdl_field->name_text_size.y;
/*
        if (fields.improper_alerts[i] != 0) {
            const std::string label = str_cast(fields.improper_alerts[i]);
        
            cairo_set_source_rgb(cr, improper_alert_color.r, improper_alert_color.g, improper_alert_color.b);
            cairo_set_font_size(cr, cairo_font_size);
            cairo_move_to(cr, x - 15, y - 15);
            cairo_show_text(cr, label.c_str());
        }
*/
    }
    
    // ===== 10. Add X-axis labels =====
    // SDL_Log("{dbg-days}10. Add X-axis labels");
    bool use_sdl_labels = true;
    if (use_sdl_labels) {
        double y = height - chart_margin_bottom;
        for (int i = 0; i < TOTAL_COLUMNS; i++) {
            tsdl_field* field = &fields.bar_4labels[i].day;
            double x = chart_margin_left + i * column_width + column_width / 2;
            field->offset = SDL_Point{(int)x, (int)y};
        }

    } else {
        VALIDATE(false, null_str);
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_set_font_size(cr, cairo_font_size);

        std::vector<std::string> time_labels;
        for (int i = 0; i < TOTAL_COLUMNS; i++) {
            // Generate labels
            SDL_snprintf(buf, sizeof(buf), "%02i:00", i);
            time_labels.push_back(buf);
        }

        cairo_text_extents(cr, time_labels[TOTAL_COLUMNS / 2].c_str(), &extents);
        bool use_rotate = column_width < extents.width + 15;
        // SDL_Log("{dbg}column_width: %.5f, extents.width: %.5f", column_width, extents.width);

        for (int i = 0; i < TOTAL_COLUMNS; i++) {
            double x = chart_margin_left + i * column_width + column_width / 2;
        
            if (!use_rotate) {
                // Get text dimensions for centering
                cairo_text_extents(cr, time_labels[i].c_str(), &extents);
                // Center the text at (0,0)
                cairo_move_to(cr, x - extents.width/2, height - chart_margin_bottom + 12 + extents.height/2);

                // cairo_move_to(cr, 0, 0);

                cairo_show_text(cr, time_labels[i].c_str());

            } else {
                cairo_save(cr);
                cairo_translate(cr, x, height - chart_margin_bottom + 15);
                cairo_rotate(cr, -M_PI / 4);

                // Get text dimensions for centering
                cairo_text_extents(cr, time_labels[i].c_str(), &extents);
                // Center the text at (0,0)
                cairo_move_to(cr, -extents.width/2, extents.height/2);

                // cairo_move_to(cr, 0, 0);

                cairo_show_text(cr, time_labels[i].c_str());
                cairo_restore(cr);
            }
        }
    }
    
    // ===== 11. Add title and axis labels =====
    // SDL_Log("{dbg-days}11. Add title and axis labels");
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    
    // Main title
    fields.title.offset = SDL_Point{margin.x, margin.y};

    // Today
    sdl_field = &fields.this_days;
    int today_gap_x = 24;
    SDL_Rect today_rect{0, margin.y, 2 * today_gap_x + sdl_field->name_text_size.x, fields.title.name_text_size.y};
    today_rect.x = width - margin.x - today_rect.w;
    draw_rounded_rectangle2(&sdl_field->cairo_color, nullptr, float_nposm, cr, today_rect.x, today_rect.y,
        today_rect.w, today_rect.h, radius);
    sdl_field->offset = SDL_Point{today_rect.x + today_gap_x, 
        margin.y + (fields.title.name_text_size.y - sdl_field->name_text_size.y) / 2};
    
    // X-axis title
/*
    cairo_set_font_size(cr, 14);
    cairo_move_to(cr, width/2 - 30, height - 10);
    cairo_show_text(cr, "Data Points");
*/   
    // Y-axis title (rotated)
/*
    cairo_save(cr);
    cairo_translate(cr, 20, height/2);
    cairo_rotate(cr, -M_PI/2);
    cairo_set_font_size(cr, 14);
    cairo_move_to(cr, 0, 0);
    cairo_show_text(cr, "Values");
    cairo_restore(cr);
*/    
    // ===== 12. Add legend =====
    // SDL_Log("{dbg-days}12. Add legend");
    int legend_icon_text_gap_x = 3;
    SDL_Point legend_2legend_gap{15, fields.legend_2legend_gap_y};
    SDL_Point legend_size{30, posix_align_ceil2(fields.legend_improper_alert.name_text_size.y - 4, 2)};

    // The legend is split into two lines, both left-aligned. 
    // The first line contains three fixed items, and the second line contains various reasons for improper posture.
    int fixed_legends[] = {fields.fid_legend_sit_duration, 
        fields.fid_legend_improper_duration, 
        fields.fid_legend_improper_alert};
    int fixed_legend_count = sizeof(fixed_legends) / sizeof(fixed_legends[0]);
    int best_fixed_legends_width = 0;
    int max_1th_line_height = 0;
    for (int at = 0; at < fixed_legend_count; at ++) {
        int fid = fixed_legends[at];
        sdl_field = fields.arrays[fid];
        // SDL_Log("{dbg-days}12.2, [%i/%i], fid: %i, sdl_field: %p", at, fixed_legend_count, fid, sdl_field);
        VALIDATE(sdl_field != nullptr, null_str);

        if (at != 0) {
            best_fixed_legends_width += legend_2legend_gap.x;
        }
        best_fixed_legends_width += legend_size.x + legend_icon_text_gap_x + sdl_field->name_text_size.x;
        max_1th_line_height = SDL_max(max_1th_line_height, sdl_field->name_text_size.y);
    }

    const int legend_x = (width - best_fixed_legends_width) / 2;
    const int legend_y = margin.y + fields.title_height;

    int tmp_legend_x = legend_x;
    int tmp_legend_y = legend_y;
    for (int at = 0; at < fixed_legend_count; at ++) {
        int fid = fixed_legends[at];
        sdl_field = fields.arrays[fid];
        VALIDATE(sdl_field != nullptr, null_str);

        int x = tmp_legend_x;
        int y = tmp_legend_y;
        if (fid != fields.fid_legend_improper_alert) {
            draw_rounded_rectangle2(&sdl_field->cairo_color, nullptr, float_nposm, cr, x, 
                y + (sdl_field->name_text_size.y - legend_size.y) / 2, legend_size.x, legend_size.y, 4);
        } else {
            int mid_offset = fields.legend_improper_alert.name_text_size.y / 2;
            // Draw legend line
            cairo_set_source_rgb(cr, improper_alert_color.r, improper_alert_color.g, improper_alert_color.b);
            cairo_set_line_width(cr, 2.5);
            cairo_move_to(cr, x, y + mid_offset);
            cairo_line_to(cr, x + legend_size.x, y + mid_offset);
            cairo_stroke(cr);
    
            // Draw legend point
            cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
            cairo_arc(cr, x + legend_size.x / 2, y + mid_offset, 5, 0, 2 * M_PI);
            cairo_fill(cr);
            // cairo_fill_preserve(cr);
    
            cairo_set_source_rgb(cr, improper_alert_color.r, improper_alert_color.g, improper_alert_color.b);
            cairo_arc(cr, x + legend_size.x / 2, y + mid_offset, 5, 0, 2 * M_PI);
            cairo_stroke(cr);
        }

        sdl_field->offset.x = tmp_legend_x + legend_size.x + legend_icon_text_gap_x;
        sdl_field->offset.y = tmp_legend_y; // (sdl_field->name_text_size.y / 2);

        tmp_legend_x = sdl_field->offset.x + sdl_field->name_text_size.x + legend_2legend_gap.x;
        max_1th_line_height = SDL_max(max_1th_line_height, sdl_field->name_text_size.y);
    }

    // 2th line
   
    // ===== 13. Add statistical information =====
    // SDL_Log("{dbg-days}13. Add statistical information");
    sdl_field = &fields.left_y_axis;
    sdl_field->offset.x = chart_margin_left - sdl_field->name_font_size * 3;
    sdl_field->offset.y = chart_margin_top - fields.y_axis_title_gap_y - sdl_field->name_text_size.y;

    sdl_field = &fields.right_y_axis;
    sdl_field->offset.x = chart_margin_left + chart_width - sdl_field->name_text_size.x + sdl_field->name_font_size * 3;
    sdl_field->offset.y = chart_margin_top - fields.y_axis_title_gap_y - sdl_field->name_text_size.y;
    
    // ===== 14. Save image and clean up resources =====
    result = argb_mat_from_CAIRO_FORMAT_ARGB32(surface, buffer.data(), width, height);

    // cairo_surface_write_to_png(surface, (game_config::preferences_dir + "/line_chart.png").c_str());
    // SDL_Log("Line chart saved to line_chart.png");
    
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    
    return result;
}

static void fill_tip_points(double x, double y, double w, double h, int& point_count, SDL_Point* points)
{
    // max 3 bar per column.
    VALIDATE(point_count < 6, null_str);
    points[point_count ++] = SDL_Point{(int)x, (int)y};
    points[point_count ++] = SDL_Point{(int)(x + w - 1), (int)(y + h - 1)};
}

cv::Mat draw_workout_bar_chart(int mat_type, int width, int height, double radius, const SDL_Point& margin,
    tworkout_fields& fields)
{
    VALIDATE(mat_type >= 0 && mat_type < wkomattype_count, null_str);
    VALIDATE(fields.pl_btn_rects != nullptr, null_str);
    memset(fields.pl_btn_rects, 0, sizeof(SDL_Rect) * pl_btn_count);

    VALIDATE(fields.tip_rects != nullptr, null_str);
    memset(fields.tip_rects, 0, sizeof(SDL_Rect) * fields.col_count);

    cv::Mat result;
    // ===== 1. Prepare data =====
    const int TOTAL_COLUMNS = fields.col_count;
    bool has_seg = false;
    bool has_rep = false;
    int col_at = 0;
    for (int at = 0; at < fields.flow_state_count; at ++) {
        const aplt::tflow_state_C& state = fields.flow_states[at];
        col_at ++;

        if (state.seg_count != 0) {
            has_seg = true;
            col_at += state.seg_count;
        }

        if (state.rep_count != 0) {
            has_rep = true;
            col_at += aplt::calc_flow_state_rep_cols(state);   
        }
    }
    VALIDATE(fields.col_count == col_at, null_str);
    VALIDATE((int)fields.bar_4labels.size() == fields.col_count, null_str);
    VALIDATE(has_seg == fields.has_seg, null_str);
    VALIDATE(has_rep == fields.has_rep, null_str);
    if (has_seg || has_rep) {
        VALIDATE(fields.has_pose_state, null_str);
    }
    bool is_3duration_exceeds_max_height = fields.has_seg || fields.has_rep;

    char buf[32];

    const int cairo_font_size = 16;

    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
    std::vector<unsigned char> buffer(height * stride);
    
    // ===== 2. Create a Cairo surface and context =====
    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        buffer.data(),
        CAIRO_FORMAT_ARGB32,
        width,
        height,
        stride
    );
    
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        SDL_Log("Failed to create surface");
        return result;
    }
    
    cairo_t* cr = cairo_create(surface);
    
    // ===== 3. Set canvas parameters =====
    cairo_text_extents_t extents;
    cairo_set_font_size(cr, cairo_font_size);
    cairo_text_extents(cr, "55.5", &extents);
    // int chart_margin_left = 80;
    // int chart_margin_left = margin.x + extents.width + Y_axis_label_chart_gap;
    int chart_margin_left = margin.x + fields.Y_axis_label_width + fields.Y_axis_label_chart_gap;
    // int chart_margin_right = 80;
    int chart_margin_right = chart_margin_left;
    int chart_margin_top = margin.y + fields.title_height + fields.legend_height;
    int chart_margin_bottom = fields.day_labels_height + margin.y;
    // int chart_margin_bottom = 60;
    
    int chart_width = width - chart_margin_left - chart_margin_right;
    int chart_height = height - chart_margin_top - chart_margin_bottom;

    // ===== 4. Set white background =====
    bool round_corner = mat_type == wkomattype_health || mat_type == wkomattype_vlog;
    draw_canvas(cr, width, height, radius, SDL_DColor{1.0, 1.0, 1.0, 1.0}, NULL, 
        round_corner, round_corner, round_corner, round_corner);
    
    // ===== 5. Draw grid and axes =====
    cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);
    cairo_set_line_width(cr, 0.5);
    
    // Find the maximum max_duration of all data for normalization
    double max_sit_duration = 0;
    double max_type_improper_duration = 0;
    col_at = 0;
    for (int state_at = 0; state_at < fields.flow_state_count; state_at ++) {
        const aplt::tflow_state_C& state = fields.flow_states[state_at];
        if (!is_3duration_exceeds_max_height) {
            max_sit_duration = SDL_max(max_sit_duration, fields.cols[col_at].state_duration_ms / 1000.0);
        }
        int alerts = fields.cols[col_at].improper_alert.x;
        if (fields.cols[col_at].improper_alert.y != nposm) {
            alerts += fields.cols[col_at].improper_alert.y;
        }
        max_type_improper_duration = SDL_max(max_type_improper_duration, alerts);
        col_at ++;

        if (state.rep_count != 0) {
            for (int rep_at = 0; rep_at < state.rep_count; rep_at += state.rep_step) {
                max_sit_duration = SDL_max(max_sit_duration, fields.cols[col_at].rep_duration_ms / 1000.0);
                max_sit_duration = SDL_max(max_sit_duration, fields.cols[col_at].phase2_duration_ms / 1000.0);

                int alerts = fields.cols[col_at].improper_alert.x;
                if (fields.cols[col_at].improper_alert.y != nposm) {
                    alerts += fields.cols[col_at].improper_alert.y;
                }
                max_type_improper_duration = SDL_max(max_type_improper_duration, alerts);

                col_at ++;
            }
        } // end if (state.rep_count != 0)

        if (state.seg_count != 0) {
            for (int seg_at = 0; seg_at < state.seg_count; seg_at ++) {
                const aplt::tsegment_C& seg = state.segs[seg_at];
                if (seg.start_ms >= fields.range_ms.min) {
                    max_sit_duration = SDL_max(max_sit_duration, fields.cols[col_at].state_duration_ms / 1000.0);
                }
                max_sit_duration = SDL_max(max_sit_duration, fields.cols[col_at].satisfied_duration_ms / 1000.0);
                max_sit_duration = SDL_max(max_sit_duration, fields.cols[col_at].unsatisfied_duration_ms / 1000.0);

                int alerts = fields.cols[col_at].improper_alert.x;
                if (fields.cols[col_at].improper_alert.y != nposm) {
                    alerts += fields.cols[col_at].improper_alert.y;
                }
                max_type_improper_duration = SDL_max(max_type_improper_duration, alerts);

                col_at ++;
            }
        } // end if (state.rep_count != 0)
    }
    VALIDATE(col_at == TOTAL_COLUMNS, null_str);
    if (max_type_improper_duration == 0) {
        max_type_improper_duration = 1;
    }

    const SDL_DColor& nonpose_state_duration_color = fields.legend_nonpose_state_duration.cairo_color;
    const SDL_DColor& pose_state_duration_color = fields.legend_pose_state_duration.cairo_color;
    const SDL_DColor& satisfied_duration_color = fields.legend_satisfied_duration.cairo_color;
    const SDL_DColor& unsatisfied_duration_color = fields.legend_unsatisfied_duration.cairo_color;
    const SDL_DColor& improper_alert_color = fields.legend_improper_alert.cairo_color;
    const SDL_DColor& seg_duration_color = fields.legend_seg_duration.cairo_color;
    const SDL_DColor& rep_duration_color = fields.legend_rep_duration.cairo_color;
    const SDL_DColor& active_period_color = fields.legend_active_period.cairo_color;
    const SDL_DColor& cooldown_period_color = fields.legend_cooldown_period.cairo_color;


    // Draw horizontal grid lines
    const double min_sit_duration = 0;
    const double min_type_improper_duration = 0;
    int grid_lines = 6;
    for (int i = 0; i <= grid_lines; i++) {
        double y = chart_margin_top + (chart_height * i) / grid_lines;
        cairo_move_to(cr, chart_margin_left, y);
        cairo_line_to(cr, width - chart_margin_right, y);
        cairo_stroke(cr);

        // Add Y-axis labels
        double sit_value = max_sit_duration - (max_sit_duration - min_sit_duration) * i / grid_lines;
        std::stringstream ss;
        
        ss.str("");
        ss << utils::format_mselapse_hm_or_ms_or_dotms(sit_value * 1000, true, utils::timesep_unit, false);

        // left. labels are right-align.
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_set_font_size(cr, cairo_font_size);
        cairo_text_extents(cr, ss.str().c_str(), &extents);
        cairo_move_to(cr, chart_margin_left - fields.Y_axis_label_chart_gap - extents.width / 2, y + 5); // y - 5
        cairo_show_text(cr, ss.str().c_str());

        // right. labels are left-align.
        double type_improper_value = max_type_improper_duration - (max_type_improper_duration - min_type_improper_duration) * i / grid_lines;
        ss.str("");
        ss << std::fixed << std::setprecision(1) << type_improper_value;
        // ss << (int)type_improper_value;
        // cairo_set_source_rgb(cr, 250.0 / 255, 198.0 / 255, 52.0 / 255);
        cairo_set_source_rgb(cr, improper_alert_color.r, improper_alert_color.g, improper_alert_color.b);
        cairo_move_to(cr, chart_margin_left + chart_width + 5, y + 5);
        // cairo_move_to(cr, chart_margin_left + chart_width + Y_axis_label_chart_gap, y + 5);
        cairo_show_text(cr, ss.str().c_str());

        cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);  // Restore grid color
    }   

    // Draw vertical grid lines
    double column_width = (double)chart_width / TOTAL_COLUMNS;
    double gap = column_width * 0.2;
    double bar_width = column_width - gap;
    double bar_width_by_3 = bar_width / 3;
    // double bar_width_by_2 = bar_width / 2;

    int tip_rect_count = 0;
    SDL_Point tip_points[6];
    int tip_point_count;

    col_at = 0;
    for (int state_at1 = 0; state_at1 < fields.flow_state_count; state_at1 ++) {
        const aplt::tflow_state_C& state = fields.flow_states[state_at1];

        tsdl_field* field = nullptr;
        SDL_Rect* tip_rect = nullptr;
        {
            const bool is_pose_state = fields.cols[col_at].type == coltype_pose_state;

            double x = chart_margin_left + col_at * column_width + gap / 2;
            double current_y = height - chart_margin_bottom;  // Stack from the bottom
            // state duration
            double segment_height = normalize2(fields.cols[col_at].state_duration_ms / 1000.0, max_sit_duration, chart_height);
            if (is_3duration_exceeds_max_height && segment_height > chart_height) {
                // At this point, the time scale uses a 'repetition' maximum duration. 
                // Timing or voice states often require a duration longer than this. 
                // In this case, the height is set to the maximum, ensuring only the correct time value is displayed.
                segment_height = chart_height;
            }
            double y = current_y - segment_height;
            if (!is_pose_state || state.seg_count > 0 || state.rep_count > 0) {
                x += bar_width_by_3;
            }
            const SDL_DColor& fill_color = is_pose_state? pose_state_duration_color: nonpose_state_duration_color;
            draw_rounded_rectangle2(&fill_color, nullptr, float_nposm, cr, x, y, bar_width_by_3, segment_height, 
                    5, true, true, false, false);

            tip_point_count = 0;
            fill_tip_points(x, y, bar_width_by_3, segment_height, tip_point_count, tip_points);

            field = &fields.bar_4labels[col_at].state;
            if (is_pose_state) {
                field->offset.x = x + bar_width_by_3 / 2 - field->name_text_size.x / 2;
                field->offset.x -= SDL_min(gap / 2, 15);

            } else {
                field->offset.x = x + bar_width_by_3 / 2 - field->name_text_size.x / 2;
            }
            field->offset.y = y - field->name_text_size.y;

            if (is_pose_state && state.seg_count == 0 && state.rep_count == 0) {
                x += bar_width_by_3;

                // satisfied duration
                segment_height = normalize2(fields.cols[col_at].satisfied_duration_ms / 1000.0, max_sit_duration, chart_height);
                if (is_3duration_exceeds_max_height && segment_height > chart_height) {
                    segment_height = chart_height;
                }
                y = current_y - segment_height;
                draw_rounded_rectangle2(&satisfied_duration_color, nullptr, float_nposm, cr, x, y, bar_width_by_3, segment_height, 
                        0, true, true, false, false);

                fill_tip_points(x, y, bar_width_by_3, segment_height, tip_point_count, tip_points);

                field = &fields.bar_4labels[col_at].satisfied;
                field->offset.x = x + bar_width_by_3 / 2 - field->name_text_size.x / 2;
                field->offset.x += SDL_min(gap / 2, 15);
                field->offset.y = y - field->name_text_size.y;

                x += bar_width_by_3;

                // unsatisfied duration
                segment_height = normalize2(fields.cols[col_at].unsatisfied_duration_ms / 1000.0, max_sit_duration, chart_height);
                if (is_3duration_exceeds_max_height && segment_height > chart_height) {
                    segment_height = chart_height;
                }
                y = current_y - segment_height;
                draw_rounded_rectangle2(&unsatisfied_duration_color, nullptr, float_nposm, cr, x, y, bar_width_by_3, segment_height, 
                        0, true, true, false, false);

                fill_tip_points(x, y, bar_width_by_3, segment_height, tip_point_count, tip_points);

                field = &fields.bar_4labels[col_at].unsatisfied;
                field->offset.x = x + bar_width_by_3 / 2 - field->name_text_size.x / 2;
                field->offset.x += SDL_min(gap / 2, 15);
                field->offset.y = y - field->name_text_size.y;

                x += bar_width_by_3;
            }

            col_at ++;

            tip_rect = &fields.tip_rects[tip_rect_count ++];
            SDL_EnclosePoints(tip_points, tip_point_count, nullptr, tip_rect);
        }

        if (state.rep_count != 0) {
            for (int rep_at = 0; rep_at < state.rep_count; ) {
                double x = chart_margin_left + col_at * column_width + gap / 2;
                const double current_y = height - chart_margin_bottom;  // Stack from the bottom

                x += bar_width_by_3 / 2;

                double segment_height = normalize2(fields.cols[col_at].rep_duration_ms / 1000.0, max_sit_duration, chart_height);
                double y = current_y - segment_height;
                draw_rounded_rectangle2(&rep_duration_color, nullptr, float_nposm, cr, x, y, bar_width_by_3, segment_height, 
                    0, true, true, false, false);

                tip_point_count = 0;
                fill_tip_points(x, y, bar_width_by_3, segment_height, tip_point_count, tip_points);

                field = &fields.bar_4labels[col_at].rep_duration;
                field->offset.x = x + bar_width_by_3 / 2 - field->name_text_size.x / 2;
                field->offset.x -= SDL_min(gap / 2, 15);
                field->offset.y = y - field->name_text_size.y;

                x += bar_width_by_3;

                double current_y2 = current_y;
                for (int step_at = 0; step_at < state.rep_step && rep_at < state.rep_count; step_at ++, rep_at ++) {
					const aplt::trepetition_C& rep = state.reps[rep_at];
                    for (int phase_at = 0; phase_at < rep.phase_count; phase_at ++) {
                        double active_duration_ms = rep.cooldown_start_ms[phase_at] - rep.active_start_ms[phase_at];
                        if (active_duration_ms < 1) {
                            active_duration_ms = 1;
                        }
                        segment_height = normalize2(active_duration_ms / 1000.0, max_sit_duration, chart_height);

                        y = current_y2 - segment_height;
                        draw_rounded_rectangle2(&active_period_color, nullptr, float_nposm, cr, x, y, bar_width_by_3, segment_height, 
                            5, true, true, false, false);
            
                        current_y2 = y;  // Update Y position for stacking the next segment

                        double cooldown_duration_ms = 0;
                        if (phase_at != rep.phase_count - 1 || rep_at != state.rep_count - 1) {
                            if (phase_at != rep.phase_count - 1) {
                                cooldown_duration_ms = rep.active_start_ms[phase_at + 1] - rep.cooldown_start_ms[phase_at];
                            } else {
                                cooldown_duration_ms = state.reps[rep_at + 1].active_start_ms[0] - rep.cooldown_start_ms[phase_at];
                            }
                            if (cooldown_duration_ms < 1) {
                                cooldown_duration_ms = 1;
                            }
                    
                            segment_height = normalize2(cooldown_duration_ms / 1000.0, max_sit_duration, chart_height);
                            y = current_y2 - segment_height;

                            draw_rounded_rectangle2(&cooldown_period_color, nullptr, float_nposm, cr, x, y, bar_width_by_3, segment_height, 
                                5, true, true, false, false);
            
                            current_y2 = y;  // Update Y position for stacking the next segment
                        }
                    } // for (..., phase_at < rep.phase_count, ...)

                } // for (..., step_at < state.rep_step, ...)
                col_at ++;

                // fill_tip_points(x, y, bar_width_by_3, segment_height, tip_point_count, tip_points);
                fill_tip_points(x, y, bar_width_by_3, current_y - y, tip_point_count, tip_points);

                tip_rect = &fields.tip_rects[tip_rect_count ++];
                SDL_EnclosePoints(tip_points, tip_point_count, nullptr, tip_rect);

            } // for (..., rep_at < state.rep_count, ...)

        }

        if (state.seg_count != 0) {
            for (int seg_at = 0; seg_at < state.seg_count; seg_at ++) {
                double x = chart_margin_left + col_at * column_width + gap / 2;
                double current_y = height - chart_margin_bottom;  // Stack from the bottom
                // state duration
                double segment_height = normalize2(fields.cols[col_at].state_duration_ms / 1000.0, max_sit_duration, chart_height);
                if (fields.has_seg && segment_height > chart_height) {
                    // At this point, the time scale uses a 'repetition' maximum duration. 
                    // Timing or voice states often require a duration longer than this. 
                    // In this case, the height is set to the maximum, ensuring only the correct time value is displayed.
                    segment_height = chart_height;
                }
                double y = current_y - segment_height;
                // if (!is_pose_state || state.rep_count > 0) {
                //    x += bar_width_by_3;
                // }
                const SDL_DColor& fill_color = seg_duration_color;
                draw_rounded_rectangle2(&fill_color, nullptr, float_nposm, cr, x, y, bar_width_by_3, segment_height, 
                        0, true, true, false, false);

                tip_point_count = 0;
                fill_tip_points(x, y, bar_width_by_3, segment_height, tip_point_count, tip_points);

                field = &fields.bar_4labels[col_at].state;
                // if (is_pose_state) {
                    field->offset.x = x + bar_width_by_3 / 2 - field->name_text_size.x / 2;
                    field->offset.x -= SDL_min(gap / 2, 15);

                // } else {
                    // field->offset.x = x + bar_width_by_3 / 2 - field->name_text_size.x / 2;
                // }
                field->offset.y = y - field->name_text_size.y;

                x += bar_width_by_3;

                // satisfied duration
                segment_height = normalize2(fields.cols[col_at].satisfied_duration_ms / 1000.0, max_sit_duration, chart_height);
                if (fields.has_rep && segment_height > chart_height) {
                    segment_height = chart_height;
                }
                y = current_y - segment_height;
                draw_rounded_rectangle2(&satisfied_duration_color, nullptr, float_nposm, cr, x, y, bar_width_by_3, segment_height, 
                        0, true, true, false, false);

                fill_tip_points(x, y, bar_width_by_3, segment_height, tip_point_count, tip_points);

                field = &fields.bar_4labels[col_at].satisfied;
                field->offset.x = x + bar_width_by_3 / 2 - field->name_text_size.x / 2;
                field->offset.x += SDL_min(gap / 2, 15);
                field->offset.y = y - field->name_text_size.y;

                x += bar_width_by_3;

                // unsatisfied duration
                segment_height = normalize2(fields.cols[col_at].unsatisfied_duration_ms / 1000.0, max_sit_duration, chart_height);
                if (fields.has_rep && segment_height > chart_height) {
                    segment_height = chart_height;
                }
                y = current_y - segment_height;
                draw_rounded_rectangle2(&unsatisfied_duration_color, nullptr, float_nposm, cr, x, y, bar_width_by_3, segment_height, 
                        0, true, true, false, false);

                fill_tip_points(x, y, bar_width_by_3, segment_height, tip_point_count, tip_points);

                field = &fields.bar_4labels[col_at].unsatisfied;
                field->offset.x = x + bar_width_by_3 / 2 - field->name_text_size.x / 2;
                field->offset.x += SDL_min(gap / 2, 15);
                field->offset.y = y - field->name_text_size.y;

                x += bar_width_by_3;

                col_at ++;

                tip_rect = &fields.tip_rects[tip_rect_count ++];
                SDL_EnclosePoints(tip_points, tip_point_count, nullptr, tip_rect);
            } // for (..., seg_at < state.seg_count, ...)

        }
    }
    VALIDATE(col_at == TOTAL_COLUMNS, null_str);
    VALIDATE(tip_rect_count == TOTAL_COLUMNS, null_str);

    // ===== 6. Draw axes (bold black) =====
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, 2.0);
 /*   
    // y-axis
    cairo_move_to(cr, chart_margin_left, chart_margin_top);
    cairo_line_to(cr, chart_margin_left, height - chart_margin_bottom);
    cairo_stroke(cr);
*/    
    // x-axis
    cairo_move_to(cr, chart_margin_left, height - chart_margin_bottom);
    cairo_line_to(cr, width - chart_margin_right, height - chart_margin_bottom);
    cairo_stroke(cr);
    
    // ===== 7. Calculate point coordinates =====
    std::vector<SDL_DPoint> points(TOTAL_COLUMNS);
    double x_step = (double)chart_width / (TOTAL_COLUMNS - 1);
    
    // col_at = 0;
    for (int i = 0; i < TOTAL_COLUMNS; i ++) {
        double x = chart_margin_left + i * column_width + column_width / 2;
        int alerts = fields.cols[i].improper_alert.x;
        if (fields.cols[i].improper_alert.y != nposm) {
            alerts += fields.cols[i].improper_alert.y;
        }
        double y = height - chart_margin_bottom - normalize(alerts, 
            min_type_improper_duration, max_type_improper_duration, chart_height);
        points[i] = {x, y};

        col_at ++;
    }
    // VALIDATE(col_at == TOTAL_WKO_COLUMNS, null_str);
    
    // ===== 8. Draw polyline =====
    // SDL_Log("{dbg-days}8. Draw polyline");
    bool use_catmull_rom = true;
    if (use_catmull_rom) {
        draw_smooth_curve_optimized(cr, improper_alert_color, 2.5, points);
/*
        int at = 0;
        SDL_Log("---points, size: %i---", (int)points.size());
        for (std::vector<SDL_DPoint>::const_iterator it = points.begin(); it != points.end(); ++ it, at ++) {
            const SDL_DPoint& point = *it;
            SDL_Log("[%i](%.3f, %.3f)", at, point.x, point.y);
        }
*/
    } else {
        cairo_set_source_rgb(cr, improper_alert_color.r, improper_alert_color.g, improper_alert_color.b);  // blue color
        cairo_set_line_width(cr, 2.5);
    
        // Move to the first point
        cairo_move_to(cr, points[0].x, points[0].y);
    
        // Connect to all subsequent points
        for (int i = 1; i < TOTAL_COLUMNS; i++) {
            const SDL_DPoint& from = points[i - 1];
            const SDL_DPoint& to = points[i];
            if (from.y == to.y) {
                cairo_line_to(cr, to.x, to.y);
            } else {
                draw_curved_line(cr, from.x, from.y, to.x, to.y);
            }
        }
    
        // Stroke the polyline
        cairo_stroke(cr);
    }

    // ===== 9. Draw data point markers =====
    // SDL_Log("{dbg-days}9. Draw data point markers");
    tsdl_field* sdl_field = nullptr;
    for (int i = 0; i < TOTAL_COLUMNS; i++) {
        double x = points[i].x;
        double y = points[i].y;
        
        double point_radius = 5;

        // Draw white filled circles
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_arc(cr, x, y, point_radius, 0, 2 * M_PI);
        cairo_fill(cr);
        
        // Draw blue border
        cairo_set_source_rgb(cr, improper_alert_color.r, improper_alert_color.g, improper_alert_color.b);
        cairo_set_line_width(cr, 2.0);
        cairo_arc(cr, x, y, point_radius, 0, 2 * M_PI);
        cairo_stroke(cr);

        // Add value labels above the points
        sdl_field = &fields.bar_4labels[i].improper_alert;
        sdl_field->offset.x = x - sdl_field->name_text_size.x / 2;
        sdl_field->offset.y = y - sdl_field->name_text_size.y;
/*
        if (fields.improper_alerts[i] != 0) {
            const std::string label = str_cast(fields.improper_alerts[i]);
        
            cairo_set_source_rgb(cr, improper_alert_color.r, improper_alert_color.g, improper_alert_color.b);
            cairo_set_font_size(cr, cairo_font_size);
            cairo_move_to(cr, x - 15, y - 15);
            cairo_show_text(cr, label.c_str());
        }
*/
    }

    // ===== 10. Add X-axis labels =====
    // SDL_Log("{dbg-days}10. Add X-axis labels");
    bool use_sdl_labels = true;
    if (use_sdl_labels) {
        double y = height - chart_margin_bottom;
        for (int i = 0; i < TOTAL_COLUMNS; i++) {
            tsdl_field* field = &fields.bar_4labels[i].day;
            double x = chart_margin_left + i * column_width + column_width / 2;
            field->offset = SDL_Point{(int)x, (int)y};
        }

    } else {
        VALIDATE(false, null_str);
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_set_font_size(cr, cairo_font_size);

        std::vector<std::string> time_labels;
        for (int i = 0; i < TOTAL_COLUMNS; i++) {
            // Generate labels
            SDL_snprintf(buf, sizeof(buf), "%02i:00", i);
            time_labels.push_back(buf);
        }

        cairo_text_extents(cr, time_labels[TOTAL_COLUMNS / 2].c_str(), &extents);
        bool use_rotate = column_width < extents.width + 15;
        // SDL_Log("{dbg}column_width: %.5f, extents.width: %.5f", column_width, extents.width);

        for (int i = 0; i < TOTAL_COLUMNS; i++) {
            double x = chart_margin_left + i * column_width + column_width / 2;
        
            if (!use_rotate) {
                // Get text dimensions for centering
                cairo_text_extents(cr, time_labels[i].c_str(), &extents);
                // Center the text at (0,0)
                cairo_move_to(cr, x - extents.width/2, height - chart_margin_bottom + 12 + extents.height/2);

                // cairo_move_to(cr, 0, 0);

                cairo_show_text(cr, time_labels[i].c_str());

            } else {
                cairo_save(cr);
                cairo_translate(cr, x, height - chart_margin_bottom + 15);
                cairo_rotate(cr, -M_PI / 4);

                // Get text dimensions for centering
                cairo_text_extents(cr, time_labels[i].c_str(), &extents);
                // Center the text at (0,0)
                cairo_move_to(cr, -extents.width/2, extents.height/2);

                // cairo_move_to(cr, 0, 0);

                cairo_show_text(cr, time_labels[i].c_str());
                cairo_restore(cr);
            }
        }
    }
    
    // ===== 11. Add title and axis labels =====
    // SDL_Log("{dbg-days}11. Add title and axis labels");
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    
    // Main title
    fields.title.offset = SDL_Point{margin.x, margin.y};

    // Today
    sdl_field = &fields.this_days;
    int today_gap_x = 24;
    SDL_Rect today_rect{0, margin.y, 2 * today_gap_x + sdl_field->name_text_size.x, fields.title.name_text_size.y};
    today_rect.x = width - margin.x - today_rect.w;
    draw_rounded_rectangle2(&sdl_field->cairo_color, nullptr, float_nposm, cr, today_rect.x, today_rect.y,
        today_rect.w, today_rect.h, radius);
    sdl_field->offset = SDL_Point{today_rect.x + today_gap_x, 
        margin.y + (fields.title.name_text_size.y - sdl_field->name_text_size.y) / 2};

    // history
    sdl_field = &fields.history;
    sdl_field->offset = SDL_Point{margin.x, margin.y + fields.title.name_text_size.y};
    
    // X-axis title
    sdl_field = &fields.unsatisfied_msg;
    sdl_field->offset = SDL_Point{margin.x,
        height - margin.y - fields.chart_remark.name_text_size.y - fields.unsatisfied_msg_to_chart_remark_gap_y - sdl_field->name_text_size.y};

    sdl_field = &fields.chart_remark;
    sdl_field->offset = SDL_Point{margin.x, height - margin.y - sdl_field->name_text_size.y};
/*
    cairo_set_font_size(cr, 14);
    cairo_move_to(cr, width/2 - 30, height - 10);
    cairo_show_text(cr, "Data Points");
*/   
    // Y-axis title (rotated)
/*
    cairo_save(cr);
    cairo_translate(cr, 20, height/2);
    cairo_rotate(cr, -M_PI/2);
    cairo_set_font_size(cr, 14);
    cairo_move_to(cr, 0, 0);
    cairo_show_text(cr, "Values");
    cairo_restore(cr);
*/  

    // ===== 12. Add legend =====
    // SDL_Log("{dbg-days}12. Add legend");
    int legend_icon_text_gap_x = 3;
    SDL_Point legend_2legend_gap{15, fields.legend_2legend_gap_y};
    SDL_Point legend_size{30, posix_align_ceil2(fields.legend_improper_alert.name_text_size.y - 4, 2)};

    // The legend is split into two lines, both left-aligned. 
    // The first line contains three fixed items, and the second line contains various reasons for improper posture.
    int fixed_legends[10] = {fields.fid_legend_nonpose_state_duration};
    int fixed_legend_count = 1;
    if (fields.has_pose_state) {
        fixed_legends[fixed_legend_count ++] = fields.fid_legend_pose_state_duration;
    }
    if (fields.has_seg) {
        fixed_legends[fixed_legend_count ++] = fields.fid_legend_seg_duration;
        fixed_legends[fixed_legend_count ++] = fields.fid_legend_satisfied_duration;
        fixed_legends[fixed_legend_count ++] = fields.fid_legend_unsatisfied_duration;
    }
    if (fields.has_rep) {
        fixed_legends[fixed_legend_count ++] = fields.fid_legend_rep_duration;
        fixed_legends[fixed_legend_count ++] = fields.fid_legend_active_period;
        fixed_legends[fixed_legend_count ++] = fields.fid_legend_cooldown_period;
    }
    if (fields.has_pose_state) {
        fixed_legends[fixed_legend_count ++] = fields.fid_legend_improper_alert;
    }

    int best_fixed_legends_width = 0;
    int max_1th_line_height = 0;
    for (int at = 0; at < fixed_legend_count; at ++) {
        int fid = fixed_legends[at];
        sdl_field = fields.arrays[fid];
        // SDL_Log("{dbg-days}12.2, [%i/%i], fid: %i, sdl_field: %p", at, fixed_legend_count, fid, sdl_field);
        VALIDATE(sdl_field != nullptr, null_str);

        if (at != 0) {
            best_fixed_legends_width += legend_2legend_gap.x;
        }
        best_fixed_legends_width += legend_size.x + legend_icon_text_gap_x + sdl_field->name_text_size.x;
        max_1th_line_height = SDL_max(max_1th_line_height, sdl_field->name_text_size.y);
    }

    const int legend_x = (width - best_fixed_legends_width) / 2;
    const int legend_y = margin.y + fields.title_height;

    int tmp_legend_x = legend_x;
    int tmp_legend_y = legend_y;
    for (int at = 0; at < fixed_legend_count; at ++) {
        int fid = fixed_legends[at];
        sdl_field = fields.arrays[fid];
        VALIDATE(sdl_field != nullptr, null_str);

        int x = tmp_legend_x;
        int y = tmp_legend_y;
        if (fid != fields.fid_legend_improper_alert) {
            draw_rounded_rectangle2(&sdl_field->cairo_color, nullptr, float_nposm, cr, x, 
                y + (sdl_field->name_text_size.y - legend_size.y) / 2, legend_size.x, legend_size.y, 4);
        } else {
            int mid_offset = fields.legend_improper_alert.name_text_size.y / 2;
            // Draw legend line
            cairo_set_source_rgb(cr, improper_alert_color.r, improper_alert_color.g, improper_alert_color.b);
            cairo_set_line_width(cr, 2.5);
            cairo_move_to(cr, x, y + mid_offset);
            cairo_line_to(cr, x + legend_size.x, y + mid_offset);
            cairo_stroke(cr);
    
            // Draw legend point
            cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
            cairo_arc(cr, x + legend_size.x / 2, y + mid_offset, 5, 0, 2 * M_PI);
            cairo_fill(cr);
            // cairo_fill_preserve(cr);
    
            cairo_set_source_rgb(cr, improper_alert_color.r, improper_alert_color.g, improper_alert_color.b);
            cairo_arc(cr, x + legend_size.x / 2, y + mid_offset, 5, 0, 2 * M_PI);
            cairo_stroke(cr);
        }

        sdl_field->offset.x = tmp_legend_x + legend_size.x + legend_icon_text_gap_x;
        sdl_field->offset.y = tmp_legend_y; // (sdl_field->name_text_size.y / 2);

        tmp_legend_x = sdl_field->offset.x + sdl_field->name_text_size.x + legend_2legend_gap.x;
        max_1th_line_height = SDL_max(max_1th_line_height, sdl_field->name_text_size.y);
    }

    // 2th line
   
    // ===== 13. Add statistical information =====
    // SDL_Log("{dbg-days}13. Add statistical information");
    sdl_field = &fields.left_y_axis;
    sdl_field->offset.x = chart_margin_left - sdl_field->name_font_size * 3;
    sdl_field->offset.y = chart_margin_top - fields.y_axis_title_gap_y - sdl_field->name_text_size.y;

    sdl_field = &fields.right_y_axis;
    sdl_field->offset.x = chart_margin_left + chart_width - sdl_field->name_text_size.x + sdl_field->name_font_size * 3;
    sdl_field->offset.y = chart_margin_top - fields.y_axis_title_gap_y - sdl_field->name_text_size.y;
    
    // ==== 14. draw play icon ====
    if (fields.share != bool_set_none) {
        draw_check_icon2(cr, fields.share == bool_set_true, fields.pl_btn_rects[pl_btn_share]);
    }
    
    double icon_size = 57.6 * gui2::twidget::hdpi_scale;  // 72(57.6*1.25)
    if (fields.cairo_draw_start_btn) {
        fields.pl_btn_rects[pl_btn_play] = create_rect(margin.x, height - margin.y - icon_size, icon_size, icon_size);

        const SDL_Rect& icon = fields.pl_btn_rects[pl_btn_play];
        Button play2Btn(icon.x, icon.y, icon.w, icon.h);
        play2Btn.hover = true;
        // SDL_Color bg_color{0.1, 0.15, 0.25, 0.8};
        // SDL_DColor bg_color{135 / 255.0, 206 / 255.0, 235 / 255.0, 0.8};
        SDL_DColor bg_color{211 / 255.0, 211 / 255.0, 211 / 255.0, 0.8};
        double inner_size = 28.8 * gui2::twidget::hdpi_scale; // 36(28.8*1.25)
        drawButton(cr, play2Btn, pl_btn_play, inner_size, &bg_color);

        fields.pl_btn_rects[misc_btn_open_url] = create_rect(
            fields.title.offset.x + fields.title.name_text_size.y, // 'fields.title.name_text_size.y' is iocn width.
            fields.title.offset.y,
            fields.title.name_text_size.x, fields.title.name_text_size.y);
/*
        double cx = icon.x + icon_size / 2;
        double cy = icon.y + icon_size / 2;
        // cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_set_source_rgb(cr, 1, 0, 0);
        drawStopIcon(cr, cx, cy, icon_size);
*/
    }

    // ===== 15. Save image and clean up resources =====
    result = argb_mat_from_CAIRO_FORMAT_ARGB32(surface, buffer.data(), width, height);

    // cairo_surface_write_to_png(surface, (game_config::preferences_dir + "/line_chart.png").c_str());
    // SDL_Log("Line chart saved to line_chart.png");
    
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    
    return result;
}

static int compute_code(int x, int y, int width, int height)
{
    const int INSIDE = 0;
    const int LEFT = 1;
    const int RIGHT = 2;
    const int BOTTOM = 4;
    const int TOP = 8;
    
    int code = INSIDE;
    if (x < 0) code |= LEFT;
    else if (x >= width) code |= RIGHT;
    if (y < 0) code |= BOTTOM;
    else if (y >= height) code |= TOP;
    return code;
}

// Helper function: Calculate the intersection of a line segment with a rectangle boundary
static bool clip_line_to_rect(int x1, int y1, int x2, int y2, 
                               int width, int height,
                               int& out_x1, int& out_y1, 
                               int& out_x2, int& out_y2) {
    const int INSIDE = 0;
    const int LEFT = 1;
    const int RIGHT = 2;
    const int BOTTOM = 4;
    const int TOP = 8;
    
    out_x1 = x1; out_y1 = y1; out_x2 = x2; out_y2 = y2;
    
    int code1 = compute_code(out_x1, out_y1, width, height);
    int code2 = compute_code(out_x2, out_y2, width, height);
    
    while (true) {
        if ((code1 == INSIDE) && (code2 == INSIDE)) {
            // Completely inside the area
            return true;
        }
        if (code1 & code2) {
            // Completely outside the area
            return false;
        }
        
        int code_out = code1 != INSIDE ? code1 : code2;
        int x, y;
        
        if (code_out & LEFT) {
            y = out_y1 + (out_y2 - out_y1) * (0 - out_x1) / (out_x2 - out_x1);
            x = 0;
        } else if (code_out & RIGHT) {
            y = out_y1 + (out_y2 - out_y1) * (width - out_x1) / (out_x2 - out_x1);
            x = width - 1;
        } else if (code_out & BOTTOM) {
            x = out_x1 + (out_x2 - out_x1) * (0 - out_y1) / (out_y2 - out_y1);
            y = 0;
        } else if (code_out & TOP) {
            x = out_x1 + (out_x2 - out_x1) * (height - out_y1) / (out_y2 - out_y1);
            y = height - 1;
        } else {
            break;
        }
        
        if (code_out == code1) {
            out_x1 = x; out_y1 = y;
            code1 = compute_code(out_x1, out_y1, width, height);
        } else {
            out_x2 = x; out_y2 = y;
            code2 = compute_code(out_x2, out_y2, width, height);
        }
    }
    return false;
}

int lmk33_which_side(int lmk_at)
{
    int result = nposm;
    if (lmk_at == 0) {
        // nose
        result = lmkside_center;

    } else if (lmk_at < 7) {
        result = (lmk_at <= 3? lmkside_left: lmkside_right);

    } else if ((lmk_at & 1) == 1) {
        result = lmkside_left;

    } else {
        result = lmkside_right;
    }

    VALIDATE(result >= 0 && result < lmkside_count, null_str);

    return result;
}

void overlay_pose_landmarks(const SDL_FPoint* norm_xy_landmarks, int count, bool flip_h, int lmk33_mode,
    cairo_surface_t* output_surface, cairo_t* cr, const SDL_Rect* clip, bool trim_to_region, 
    const SDL_DColor* colors, const bool* sel_landmarks, SDL_Point* points)
{
	VALIDATE(count == mediapipe::kNumPoseLandmarks, null_str);
    VALIDATE(lmk33_mode >= 0 && lmkmode_count, null_str);

	const SDL_Point POSE_CONNECTIONS[] = {
		{0, 2}, {0, 5}, {2, 7}, {5, 8}, {9, 10},
		{11, 12}, {23, 24},
		{11, 13}, {13, 15}, {15, 17}, {15, 21}, {15, 19}, {17, 19},
		{12, 14}, {14, 16}, {16, 18}, {16, 22}, {16, 20}, {18, 20},
		{11, 23}, {23, 25}, {25, 27}, {27, 29}, {27, 31}, {29, 31},
		{12, 24}, {24, 26}, {26, 28}, {28, 30}, {28, 32}, {30, 32},
	};

    // Get surface dimensions
    int width = cairo_image_surface_get_width(output_surface);
    int height = cairo_image_surface_get_height(output_surface);

    SDL_Point margin{0, 0};
    if (clip != nullptr) {
        VALIDATE(clip->x + clip->w <= width, null_str);
        VALIDATE(clip->y + clip->h <= height, null_str);
        margin.x = clip->x;
        margin.y = clip->y;
        width = clip->w;
        height = clip->h;
    }

	SDL_Point xy_landmarks[mediapipe::kNumPoseLandmarks];
	for (int at = 0; at < count; at ++) {
		float x = flip_h? 1.0 - norm_xy_landmarks[at].x: norm_xy_landmarks[at].x;
		xy_landmarks[at].x = static_cast<int>(x * width);
		xy_landmarks[at].y = static_cast<int>(norm_xy_landmarks[at].y * height);
	}

    // Set line style
    const bool thin = sel_landmarks == nullptr;
    int line_width = thin? 3 : 4;
    int circle_radius = thin? 5 : 8;
    
    // Draw skeleton connections
    int lines = sizeof(POSE_CONNECTIONS) / sizeof(POSE_CONNECTIONS[0]);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);  // white
    cairo_set_line_width(cr, line_width);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);    

    for (int at = 0; at < lines; at ++) {
        const SDL_Point& connection = POSE_CONNECTIONS[at];

		if (std::isnan(norm_xy_landmarks[connection.x].x) || std::isnan(norm_xy_landmarks[connection.y].x)) {
			continue;
		}

        const SDL_Point& start = xy_landmarks[connection.x];
        const SDL_Point& end = xy_landmarks[connection.y];

        bool start_in = start.x >= 0 && start.x < width && start.y >= 0 && start.y < height;
        bool end_in = end.x >= 0 && end.x < width && end.y >= 0 && end.y < height;
        if (!trim_to_region || (start_in && end_in)) {
            // no trim or Completely inside the area
            cairo_move_to(cr, margin.x + start.x, margin.y + start.y);
            cairo_line_to(cr, margin.x + end.x, margin.y + end.y);
            cairo_stroke(cr);

        } else if (start_in || end_in) {
            // Partially inside the area
            int clip_x1, clip_y1, clip_x2, clip_y2;
            if (clip_line_to_rect(start.x, start.y, end.x, end.y, 
                                  width, height,
                                  clip_x1, clip_y1, clip_x2, clip_y2)) {
                cairo_move_to(cr, margin.x + clip_x1, margin.y + clip_y1);
                cairo_line_to(cr, margin.x + clip_x2, margin.y + clip_y2);
                cairo_stroke(cr);
            }
        }
    }

    // Draw keypoints (dots)
    SDL_DColor lkm_def_color{0.0, 1.0, 0.0, 1.0};
	for (int at = 0; at < count; ++ at) {
		const int& x = xy_landmarks[at].x;
		const int& y = xy_landmarks[at].y;

        if (std::isnan(norm_xy_landmarks[at].x)) {
			continue;
		}
        const int side = lmk33_which_side(at);
        bool allow_sel = true;

        if (lmk33_mode != lmkmode_both) {
            if (lmk33_mode == lmkmode_left_only) {
                if (side == lmkside_right) {
                    allow_sel = false;
                }

            } else if (lmk33_mode == lmkmode_right_only) {
                if (side == lmkside_left) {
                    allow_sel = false;
                }
            }
        }

        if (points != nullptr && allow_sel) {
            points[at].x = margin.x + x;
            points[at].y = margin.y + y;
        }

        if (x >= 0 && x < width && y >= 0 && y < height) {
            const SDL_DColor* color = &lkm_def_color;
            if (colors != 0) {
                color = colors + side;
            }

            double alpha = color->a;
            int circle_radius2 = circle_radius;
            if (sel_landmarks != nullptr && sel_landmarks[at]) {
                alpha = 0.5;
                circle_radius2 = 2 * circle_radius;
            }

            if (allow_sel) {
                cairo_set_source_rgba(cr, color->r, color->g, color->b, alpha);  // Green dot
                cairo_arc(cr, margin.x + x, margin.y + y, circle_radius2, 0, 2 * M_PI);
                cairo_fill(cr);
            }
        }
    }
}

class VideoPlayer
{
public:
    const int BTN_WIDTH = 32 * gui2::twidget::hdpi_scale;  // 45
    const int BTN_HEIGHT = 28 * gui2::twidget::hdpi_scale;  // 35
    const int GAP_EDGE = 12 * gui2::twidget::hdpi_scale;  // 15
    const int GAP_BETWEEN_BUTTONS = 6.4 * gui2::twidget::hdpi_scale;  // 8
    const int GAP_BETWEEN_PROGRESSBAR = 12 * gui2::twidget::hdpi_scale;  // 15

    // enum {btntype_play, btntype_pause, btntype_restart, btntype_stop, btn_types};
    VideoPlayer(cairo_t* _cr, int margin_x, int margin_y, int w, int h, int played_ms, int duration_ms, int playstyle)
        : cr(_cr)
        , played_ms(played_ms)
        , duration_ms(duration_ms)
        , playstyle(playstyle)
        , progress(duration_ms == 0? 0.0: 1.0 * played_ms / duration_ms)
        , cairo_font_size_(18)
        , playBtn(0, 0, BTN_WIDTH, BTN_HEIGHT)
        , pauseBtn(0, 0, BTN_WIDTH, BTN_HEIGHT)
        , restartBtn(0, 0, BTN_WIDTH, BTN_HEIGHT)
        , stepbackwardBtn(0, 0, BTN_WIDTH, BTN_HEIGHT)
        , stepforwardBtn(0, 0, BTN_WIDTH, BTN_HEIGHT)
        , stopBtn(0, 0, BTN_WIDTH, BTN_HEIGHT) 
    {
        videoX = margin_x;
        videoY = margin_y;
        videoWidth = w;
        videoHeight = h;
        
        double progress_area_height = 30 * gui2::twidget::hdpi_scale;
        double min_progress_valid_height = 12 * gui2::twidget::hdpi_scale;
        VALIDATE(progress_area_height > min_progress_valid_height, null_str);

        controlBarHeight = progress_area_height + BTN_HEIGHT;
        controlBarY = videoY + videoHeight - controlBarHeight;
        
        double btnY = controlBarY + progress_area_height;
        playBtn.x = videoX + GAP_EDGE;
        playBtn.y = btnY;
        pauseBtn.x = playBtn.x;
        pauseBtn.y = playBtn.y;
        restartBtn.x = playBtn.x;
        restartBtn.y = playBtn.y;

        stepbackwardBtn.x = playBtn.x + playBtn.width + GAP_BETWEEN_BUTTONS;
        stepbackwardBtn.y = btnY;
        stepforwardBtn.x = stepbackwardBtn.x + stepbackwardBtn.width + GAP_BETWEEN_BUTTONS;
        stepforwardBtn.y = btnY;

        const Button& prevStopBtn = playstyle == pl_btn_play? stepforwardBtn: playBtn;
        stopBtn.x = prevStopBtn.x + prevStopBtn.width + GAP_BETWEEN_BUTTONS; // videoX + 70
        stopBtn.y = btnY;

        char buf[64];
        SDL_snprintf(buf, sizeof(buf), "%s/%s", utils::format_elapse_ms2(played_ms / 1000, false).c_str(),
            utils::format_elapse_ms2(duration_ms / 1000, false).c_str());
        time_label_ = buf;

        bool is_time_and_progress_same_line = false;
        cairo_text_extents_t extents;
        cairo_set_font_size(cr, cairo_font_size_);
        cairo_text_extents(cr, time_label_.c_str(), &extents);

        // progressX = stopBtn.x + stopBtn.width + GAP_BETWEEN_PROGRESSBAR;
        // progressY = controlBarY + controlBarHeight / 2 - 5;
        progressHeight = 4.8 * gui2::twidget::hdpi_scale;

        progressX = playBtn.x;
        progressY = controlBarY + (progress_area_height - progressHeight) / 2;
        if (is_time_and_progress_same_line) {
            progressWidth = videoWidth - progressX - extents.width - GAP_EDGE;
        } else {
            progressWidth = videoX + videoWidth - progressX - GAP_EDGE;
        }

        time_label_xy_.x = videoX + videoWidth - extents.width - GAP_EDGE;
        if (is_time_and_progress_same_line) {
            time_label_xy_.y = progressY + progressHeight / 2 + 4;
        } else {
            time_label_xy_.y = progressY - extents.height;
        }
    }
    
    ~VideoPlayer()
    {}
    
    SDL_Rect get_progressbar_rect() const
    {
        return SDL_Rect{(int)progressX, (int)progressY, (int)progressWidth, (int)progressHeight};
    }

    void drawProgressBar()
    {
        // Progress bar background - semi-transparent
        cairo_set_source_rgba(cr, 0.2, 0.25, 0.35, 0.7);
        cairo_rectangle(cr, progressX, progressY, progressWidth, progressHeight);
        cairo_fill(cr);
        
        // Progress bar foreground
        cairo_set_source_rgb(cr, 0.2, 0.7, 0.9);
        cairo_rectangle(cr, progressX, progressY, progressWidth * progress, progressHeight);
        cairo_fill(cr);
        
        // Progress dot
        double thumbX = progressX + progressWidth * progress;
        cairo_set_source_rgb(cr, 0.2, 0.7, 0.9);
        // cairo_arc(cr, thumbX, progressY + progressHeight/2, 7, 0, 2 * M_PI);
        cairo_arc(cr, thumbX, progressY + progressHeight/2, 5.6 * gui2::twidget::hdpi_scale, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, 1, 1, 1);
        // cairo_arc(cr, thumbX, progressY + progressHeight/2, 3, 0, 2 * M_PI);
        cairo_arc(cr, thumbX, progressY + progressHeight/2, 2.4 * gui2::twidget::hdpi_scale, 0, 2 * M_PI);
        cairo_fill(cr);
        
        // Time display
        cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
        cairo_set_font_size(cr, cairo_font_size_);
        cairo_move_to(cr, time_label_xy_.x, time_label_xy_.y);
        cairo_show_text(cr, time_label_.c_str());
    }
    
    void drawControlBar()
    {
/*
        // Semi-transparent black background
        cairo_set_source_rgba(cr, 0, 0, 0, 0.3);
        cairo_rectangle(cr, videoX, controlBarY, videoWidth, controlBarHeight);
        cairo_fill(cr);
*/
/*        
        // Top thin line
        cairo_set_source_rgba(cr, 0.6, 0.6, 0.7, 0.4);
        cairo_set_line_width(cr, 1);
        cairo_move_to(cr, videoX, controlBarY);
        cairo_line_to(cr, videoX + videoWidth, controlBarY);
        cairo_stroke(cr);
*/        
        // Buttons and progress bar
        if (playstyle == pl_btn_play) {
            drawButton(cr, playBtn, pl_btn_play);
            
        } else if (playstyle == pl_btn_pause) {
            drawButton(cr, pauseBtn, pl_btn_pause);

        } else {
            VALIDATE(playstyle == pl_btn_restart, null_str);
            drawButton(cr, restartBtn, pl_btn_restart);
        }
        if (playstyle == pl_btn_play) {
            drawButton(cr, stepbackwardBtn, pl_btn_step_backward);
            drawButton(cr, stepforwardBtn, pl_btn_step_forward);
        }
        drawButton(cr, stopBtn, pl_btn_stop);
        drawProgressBar();
    }

public:
    cairo_t* cr;
    
    const int played_ms;
    const int duration_ms;
    int playstyle;
    int currentFrame;
    int totalFrames;
    double progress;
    
    double videoX, videoY, videoWidth, videoHeight;
    double cornerRadius;
    double controlBarHeight;
    double controlBarY;
    int cairo_font_size_;
    std::string time_label_;
    SDL_DPoint time_label_xy_;
    
    Button playBtn;
    Button pauseBtn;
    Button restartBtn;
    Button stepbackwardBtn;
    Button stepforwardBtn;
    Button stopBtn;
    double progressX, progressY, progressWidth, progressHeight;
};

tlandmark_fields::tlandmark_fields(int small_font_size, int lmk33_mode, bool is_player, const bool* _sel_landmarks, bool use_surf_landmarks)
	: small_font_size(small_font_size)
	, lmk33_mode(lmk33_mode)
	, is_player(is_player)
	, sel_landmarks(_sel_landmarks)
	, sel_count(0)
	, use_surf_landmarks(use_surf_landmarks)
	, legend_to_right_gap_x(4)
	, legend_2legend_gap_y(5)
	, legend_to_top_gap_y(10)
	, nose_color({1.0, 0.0, 0.0, 1.0})
	, share(bool_set_none)
	, lmk33_modes({
		{lmkmode_both, _("Left+Right")},
		{lmkmode_left_only, _("Left only")},
		{lmkmode_right_only, _("Right only")},
	})
	, phase_count(0)
	, phase_sel(nposm)
{
	VALIDATE(lmk33_mode >= 0 && lmkmode_count, null_str);
	if (is_player) {
		VALIDATE(sel_landmarks == nullptr, null_str);
	}

	clear();
	if (sel_landmarks != nullptr) {
		for (int at = 0; at < mediapipe::kNumPoseLandmarks; at ++) {
			if (sel_landmarks[at]) {
				sel_count ++;
			}
		}
	}

	memset(arrays, 0, sizeof(arrays));

	arrays[fid_phase0] = &phase0;
	arrays[fid_phase1] = &phase1;
	arrays[fid_legend_left] = &legend_left;
	arrays[fid_legend_right] = &legend_right;
	arrays[fid_open_image] = &open_image;
	arrays[fid_fake_landmark] = &fake_landmark;
	arrays[fid_sdl_lmk33_mode] = &sdl_lmk33_mode;
	arrays[fid_btn_clear] = &btn_clear;

	for (int at = 0; at < fid_count; at ++) {
		VALIDATE(arrays[at] != nullptr, null_str);
	}
}

void tlandmark_fields::pre_fill_sdl_fields()
{
    utils::string_map symbols;
    for (int at = 0; at < fid_count; at ++) {
		cairo::tsdl_field* field = arrays[at];
		std::string icon;
		std::string name;
		int name_font_size = 0;
		SDL_DColor cairo_color{0.0, 0.0, 0.0, 0.0};

        if (at == fid_phase0 || at == fid_phase1) {
            if (is_player) {
                continue;
            }
            if (phase_count == 1) {
                if (at == fid_phase1) {
                    continue;
                }
            }
            symbols["number"] = str_cast(at - fid_phase0 + 1);
		    name = vgettext2("Phase $number", symbols);
		    name_font_size = small_font_size;
            // cairo_color = SDL_DColor{0.2, 0.2, 0.2, 1.0};

	    } else if (at == fid_legend_left) {
		    name = _("Left");
		    name_font_size = small_font_size;
            cairo_color = SDL_DColor{78 / 255.0, 175 / 255.0, 80 / 255.0, 1.0};

	    } else if (at == fid_legend_right) {
            name = _("Right");
		    name_font_size = small_font_size;
            cairo_color = SDL_DColor{0.9, 0.7, 0.1, 1.0};

        } else if (at == fid_open_image) {
            if (is_player) {
                continue;
            }
            name = _("Open image");
            name_font_size = small_font_size;
            cairo_color = SDL_DColor{1.0, 1.0, 1.0, 1.0};

        } else if (at == fid_fake_landmark) {
            if (is_player) {
                continue;
            }
            name = _("Fake landmark");
            name_font_size = small_font_size;
            cairo_color = SDL_DColor{1.0, 1.0, 1.0, 1.0};

        } else if (at == fid_sdl_lmk33_mode) {
            if (is_player) {
                continue;
            }
            VALIDATE(lmk33_modes.count(lmk33_mode) != 0, null_str);
            name = lmk33_modes.find(lmk33_mode)->second;
            name_font_size = small_font_size;
            cairo_color = SDL_DColor{1.0, 1.0, 1.0, 1.0};

        } else if (at == fid_btn_clear) {
            if (sel_count == 0) {
                continue;
            }
            name = _("Clear selected");
		    name_font_size = font::SIZE_SMALL;
            // SDL_Color color{0x6c, 0x75, 0x7d, 255};
            SDL_Color color{0xdc, 0x35, 0x45, 255};
            cairo_color = SDL_DColor{color.r / 255.0, color.g / 255.0, color.b / 255.0, 1.0};
        }

		field->set(icon, name, name_font_size, null_str, 0, SDL_Point{0, 0}, SDL_Point{0, 0});
		field->cairo_color = cairo_color;
		field->desire_size.x = field->name_text_size.x;
		field->desire_size.y = field->margin.y * 2 + field->name_text_size.y + field->gap.y + field->val_text_size.y;
	}
}

void tlandmark_fields::post_render_sdl_fields(cv::Mat& mat)
{
	surface surf(mat);

    surface text_surf;
	SDL_Rect dst_rect;

	for (int at = 0; at < fid_count; at ++) {
        const cairo::tsdl_field& field = *arrays[at];

	    int x_start = field.offset.x;
	    int y_start = field.offset.y;
        SDL_Color font_color = font::GRAY_COLOR;

        if (at == fid_phase0 || at == fid_phase1) {
            font_color = font::SDL_DColor_to_SDL_Color(SDL_DColor{0.2, 0.2, 0.2, 1.0});

        } else if (at == fid_legend_left) {
		    font_color = font::BIGMAP_COLOR;

	    } else if (at == fid_legend_right) {
		    font_color = font::BIGMAP_COLOR;

	    } else if (at == fid_open_image) {
		    font_color = font::SDL_DColor_to_SDL_Color(SDL_DColor{0.2, 0.2, 0.2, 1.0});

	    } else if (at == fid_fake_landmark) {
		    font_color = font::SDL_DColor_to_SDL_Color(SDL_DColor{0.2, 0.2, 0.2, 1.0});

	    } else if (at == fid_sdl_lmk33_mode) {
		    font_color = font::SDL_DColor_to_SDL_Color(SDL_DColor{0.2, 0.2, 0.2, 1.0});

	    } else if (at == fid_btn_clear) {
		    font_color = font::BIGMAP_COLOR;
	    }

        if (!field.name.empty()) {
		    text_surf = font::get_rendered_text(field.name, INT_MAX, field.name_font_size, font_color);
		    dst_rect = ::create_rect(x_start, y_start, text_surf->w, text_surf->h);
		    sdl_blit(text_surf, nullptr, surf, &dst_rect);
        }
	}
}

cv::Mat draw_landmarks_mat(bool to_image, int width, int height, double radius, const SDL_Point& margin,
    const SDL_Rect& video_clip, const SDL_FPoint* landmarks, int count, int played_ms, int duration_ms, int playstyle, 
    SDL_Rect* btn_rects, int btn_count, tlandmark_fields& fields)
{
    // from launcher's gui2::pose_state2, width maybe not multiple_of_4.
    // VALIDATE(IS_MULTIPLE_OF_4(width), null_str);

    VALIDATE(fields.phase_count <= WKO_MAX_PHASE_COUNT, null_str);

    VALIDATE(btn_rects != nullptr, null_str);
    memset(btn_rects, 0, sizeof(SDL_Rect) * btn_count);

    fields.pre_fill_sdl_fields();

    const bool is_player = fields.is_player;
    if (is_player) {
        VALIDATE(playstyle >= 0 && playstyle < pl_btn_count, null_str);
    } else {
        VALIDATE(playstyle == nposm, null_str);
    }
    cv::Mat result;

    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
    std::vector<unsigned char> buffer(height * stride);
    
    // ===== 2. Create a Cairo surface and context =====
    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        buffer.data(),
        CAIRO_FORMAT_ARGB32,
        width,
        height,
        stride
    );
    
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        SDL_Log("Failed to create surface");
        return result;
    }
    
    cairo_t* cr = cairo_create(surface);

    tsdl_field* sdl_field = nullptr;
    // ===== 4. Set white background =====
    SDL_DColor from{0.12, 0.15, 0.25, 1.0};
    SDL_DColor to{0.06, 0.08, 0.12, 1.0};
    if (is_player) {
        VALIDATE(!fields.use_surf_landmarks, null_str);
        draw_canvas(cr, width, height, radius, SDL_DColor{1.0, 1.0, 1.0, 1.0}, NULL, !to_image, !to_image, !to_image, !to_image);
        draw_gradient_use_path(cr, width, height, false, from, to, &video_clip);

    } else {
        if (!fields.use_surf_landmarks) {
            draw_gradient_use_path(cr, width, height, false, from, to, nullptr);

        } else {
            SDL_Rect top_rect{0, 0, width, video_clip.y};
            draw_gradient_use_path(cr, width, height, false, from, to, &top_rect);
            // don't overrwirte middle rect. it is reseved by surf.

            SDL_Rect bottom_rect{0, video_clip.y + video_clip.h, width, 0};
            bottom_rect.h = height - bottom_rect.y;
            draw_gradient_use_path(cr, width, height, false, from, to, &bottom_rect);
        }
    }

    bool flip_h = false;
    const SDL_DColor lkm_colors[] = {fields.nose_color, fields.legend_left.cairo_color, fields.legend_right.cairo_color};
    VALIDATE(sizeof(lkm_colors) / sizeof(lkm_colors[0]) == lmkside_count, null_str);
    overlay_pose_landmarks(landmarks, count, flip_h, fields.lmk33_mode, surface, cr, &video_clip, true, lkm_colors, fields.sel_landmarks, fields.points);

    if (is_player) {
        VideoPlayer player(cr, video_clip.x, video_clip.y, video_clip.w, video_clip.h, played_ms, duration_ms, playstyle);
        player.drawControlBar();

        btn_rects[pl_btn_play] = player.playBtn.get_rect();
        if (playstyle == pl_btn_play) {
            btn_rects[pl_btn_step_backward] = player.stepbackwardBtn.get_rect();
            btn_rects[pl_btn_step_forward] = player.stepforwardBtn.get_rect();
        }
        btn_rects[pl_btn_stop] = player.stopBtn.get_rect();
        btn_rects[pl_btn_progressbar] = player.get_progressbar_rect();
    }

    // ===== 12. Add legend =====
    int legend_icon_text_gap_x = 3;
    SDL_Point legend_2legend_gap{15, fields.legend_2legend_gap_y};
    SDL_Point legend_size{30, posix_align_ceil2(fields.legend_left.name_text_size.y - 4, 2)};

    // The legend is split into two lines, both left-aligned. 
    // The first line contains three fixed items, and the second line contains various reasons for improper posture.
    int fixed_legends[] = {fields.fid_legend_left, 
        fields.fid_legend_right};
    int fixed_legend_count = sizeof(fixed_legends) / sizeof(fixed_legends[0]);
    int best_fixed_legends_width = 0;
    int max_1th_line_height = 0;
    for (int at = 0; at < fixed_legend_count; at ++) {
        int fid = fixed_legends[at];
        sdl_field = fields.arrays[fid];
        VALIDATE(sdl_field != nullptr, null_str);

        if (at != 0) {
            best_fixed_legends_width += legend_2legend_gap.x;
        }
        best_fixed_legends_width += legend_size.x + legend_icon_text_gap_x + sdl_field->name_text_size.x;
        max_1th_line_height = SDL_max(max_1th_line_height, sdl_field->name_text_size.y);
    }

    // const int legend_x = (width - best_fixed_legends_width) / 2;
    int legend_x = is_player? margin.x + video_clip.w: width;
    legend_x -= best_fixed_legends_width + fields.legend_to_right_gap_x;
    const int legend_y = margin.y + fields.legend_to_top_gap_y;

    int tmp_legend_x = legend_x;
    int tmp_legend_y = legend_y;
    for (int at = 0; at < fixed_legend_count; at ++) {
        int fid = fixed_legends[at];
        sdl_field = fields.arrays[fid];
        VALIDATE(sdl_field != nullptr, null_str);

        int x = tmp_legend_x;
        int y = tmp_legend_y;

        draw_rounded_rectangle2(&sdl_field->cairo_color, nullptr, float_nposm, cr, x, 
            y + (sdl_field->name_text_size.y - legend_size.y) / 2, legend_size.x, legend_size.y, 4);

        sdl_field->offset.x = tmp_legend_x + legend_size.x + legend_icon_text_gap_x;
        sdl_field->offset.y = tmp_legend_y; // (sdl_field->name_text_size.y / 2);

        tmp_legend_x = sdl_field->offset.x + sdl_field->name_text_size.x + legend_2legend_gap.x;
        max_1th_line_height = SDL_max(max_1th_line_height, sdl_field->name_text_size.y);
    }

    // ===== 13. button: clear selected =====
    {
        int tabs_x = margin.x + fields.legend_to_right_gap_x;
        const int tabs_y = margin.y + fields.legend_to_top_gap_y;
    
        SDL_Size min_tab_size{(int)(80 * gui2::twidget::hdpi_scale), (int)(32 * gui2::twidget::hdpi_scale)};  // {100, 40}
        ttab_C tabs[WKO_MAX_PHASE_COUNT];
        int tab_offset_x = tabs_x;
        for (int at = 0; at < fields.phase_count; at ++) {
            ttab_C& tab = tabs[at];
            sdl_field = fields.arrays[fields.fid_phase0 + at];
            tab.x = tab_offset_x;
            tab.y = tabs_y;
            tab.width = SDL_max(sdl_field->name_text_size.x, min_tab_size.w);
            tab.height = SDL_max(sdl_field->name_text_size.y, min_tab_size.h);
            if (at == fields.phase_sel) {
                // To reduce the cpu spend, avoid triggering potential click events here.
                tab.selected = true;

            } else {
                tab.selected = false;
                btn_rects[lmk_btn_phase0 + at] = SDL_Rect{(int)tab.x, (int)tab.y, (int)tab.width, (int)tab.height};
            }
            draw_tab(cr, tab);

            sdl_field->offset.x = tab.x + (tab.width - sdl_field->name_text_size.x) / 2;
            sdl_field->offset.y = tab.y + (tab.height - sdl_field->name_text_size.y) / 2;

            tab_offset_x = tab.x + tab.width;
        }
    }

    if (!is_player) {
        // open image
        sdl_field = fields.arrays[fields.fid_open_image];
        const SDL_Point btn_margin{(int)(4 * gui2::twidget::hdpi_scale), (int)(4 * gui2::twidget::hdpi_scale)};
        SDL_Size btn_size{sdl_field->name_text_size.x + btn_margin.x * 2, sdl_field->name_text_size.y + btn_margin.y * 2};

        int x = margin.x + margin.x + fields.legend_to_right_gap_x;
        int y = height - btn_size.h - fields.legend_to_top_gap_y - margin.y;
        
        draw_rounded_rectangle2(&sdl_field->cairo_color, nullptr, float_nposm, cr, x, 
            y, btn_size.w, btn_size.h, 4);

        sdl_field->offset.x = x + btn_margin.x;
        sdl_field->offset.y = y + btn_margin.y;
        btn_rects[lmk_btn_open_image] = SDL_Rect{x, y, btn_size.w, btn_size.h};

        struct tlayout_btn
        {
            int fid;
            int lmk_btn;
        };
        tlayout_btn layout_btns[] = {
            {fields.fid_fake_landmark, lmk_btn_fake_landmark},
            {fields.fid_sdl_lmk33_mode, lmk_btn_lmk33_mode},
        };
        int NUM = sizeof(layout_btns) / sizeof(layout_btns[0]);
        const int btn_gap_x = 8 * gui2::twidget::hdpi_scale;
        for (int at = 0; at < NUM; at ++) {
            const tlayout_btn& layout_btn = layout_btns[at];
            sdl_field = fields.arrays[layout_btn.fid];

            x = x + btn_gap_x + btn_size.w;
            btn_size = SDL_Size{sdl_field->name_text_size.x + btn_margin.x * 2, sdl_field->name_text_size.y + btn_margin.y * 2};

            // y = y;
        
            draw_rounded_rectangle2(&sdl_field->cairo_color, nullptr, float_nposm, cr, x, 
                y, btn_size.w, btn_size.h, 4);

            sdl_field->offset.x = x + btn_margin.x;
            sdl_field->offset.y = y + btn_margin.y;

            btn_rects[layout_btn.lmk_btn] = SDL_Rect{x, y, btn_size.w, btn_size.h};
        }

        if (fields.sel_count != 0) {
            // clear
            sdl_field = fields.arrays[fields.fid_btn_clear];
            btn_size = SDL_Size{sdl_field->name_text_size.x + btn_margin.x * 2, sdl_field->name_text_size.y + btn_margin.y * 2};

            int x = is_player? margin.x + video_clip.w: width;
            x -= btn_size.w + fields.legend_to_right_gap_x;
            int y = height - btn_size.h - fields.legend_to_top_gap_y - margin.y;
        
            draw_rounded_rectangle2(&sdl_field->cairo_color, nullptr, float_nposm, cr, x, 
                y, btn_size.w, btn_size.h, 4);

            sdl_field->offset.x = x + btn_margin.x;
            sdl_field->offset.y = y + btn_margin.y;

            btn_rects[lmk_btn_clear] = SDL_Rect{x, y, btn_size.w, btn_size.h};
        }
    }
    
    if (is_player) {
        if (fields.share != bool_set_none) {
            VALIDATE(btn_rects != nullptr, null_str);
            draw_check_icon2(cr, fields.share == bool_set_true, btn_rects[pl_btn_share]);
        }
    } else {
        VALIDATE(fields.share == bool_set_none, null_str);
    }

    // ===== 14. Save image and clean up resources =====
    result = argb_mat_from_CAIRO_FORMAT_ARGB32(surface, buffer.data(), width, height);

    // cairo_surface_write_to_png(surface, (game_config::preferences_dir + "/line_chart.png").c_str());
    // SDL_Log("Line chart saved to line_chart.png");
    
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    
    fields.post_render_sdl_fields(result);
    return result;
}

cv::Mat draw_days_workout_dual_axis_stacked_bar_chart(bool to_image, int width, int height, double radius, const SDL_Point& margin,
    int days, tdays_workout_fields& fields)
{
    VALIDATE_POSTURE_DAYS(days);

    VALIDATE(fields.pl_btn_rects != nullptr, null_str);
    memset(fields.pl_btn_rects, 0, sizeof(SDL_Rect) * pl_btn_count);

    VALIDATE(fields.tip_rects != nullptr, null_str);
    memset(fields.tip_rects, 0, sizeof(SDL_Rect) * days);

    cv::Mat result;
    // ===== 1. Prepare data =====
    const int TOTAL_COLUMNS = days;
    VALIDATE((int)fields.bar_4labels.size() == days, null_str);
    char buf[32];

    const int cairo_font_size = 16;

    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
    std::vector<unsigned char> buffer(height * stride);
    
    // ===== 2. Create a Cairo surface and context =====
    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        buffer.data(),
        CAIRO_FORMAT_ARGB32,
        width,
        height,
        stride
    );
    
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        SDL_Log("Failed to create surface");
        return result;
    }
    
    cairo_t* cr = cairo_create(surface);
    
    // ===== 3. Set canvas parameters =====
    cairo_text_extents_t extents;
    cairo_set_font_size(cr, cairo_font_size);
    cairo_text_extents(cr, "55.5", &extents);
    // int chart_margin_left = 80;
    // int chart_margin_left = margin.x + extents.width + Y_axis_label_chart_gap;
    int chart_margin_left = margin.x + fields.Y_axis_label_width + fields.Y_axis_label_chart_gap;
    // int chart_margin_right = 80;
    int chart_margin_right = chart_margin_left;
    int chart_margin_top = margin.y + fields.title_height + fields.legend_height;
    int chart_margin_bottom = fields.day_labels_height + margin.y;
    // int chart_margin_bottom = 60;
    
    int chart_width = width - chart_margin_left - chart_margin_right;
    int chart_height = height - chart_margin_top - chart_margin_bottom;

    // ===== 4. Set white background =====
    draw_canvas(cr, width, height, radius, SDL_DColor{1.0, 1.0, 1.0, 1.0}, NULL, !to_image, !to_image, !to_image, !to_image);
    
    // ===== 5. Draw grid and axes =====
    cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);
    cairo_set_line_width(cr, 0.5);
    
    // Find the maximum max_duration of all data for normalization
    double max_sit_duration = 0;
    for (int i = 0; i < TOTAL_COLUMNS; i++) {
        // max_sit_duration = SDL_max(max_sit_duration, fields.sit_durations[i] / 60.0);
        max_sit_duration = SDL_max(max_sit_duration, fields.sit_durations[i]);
    }
    // max_sit_duration = posix_align_ceil2((int)max_sit_duration + 9, 10);

    double max_type_improper_duration = 0;
    for (int i = 0; i < TOTAL_COLUMNS; i++) {
        // both type_improper_duration and improper_alert use same y-axis range.
        max_type_improper_duration = SDL_max(max_type_improper_duration, fields.improper_alerts[i]);
    }
/*
    const SDL_DColor sit_duration_color{78 / 255.0, 175 / 255.0, 80 / 255.0, 1.0};
    // const SDL_DColor improper_duration_color{252 / 255.0, 84 / 255.0, 84 / 255.0, 1.0};
    const SDL_DColor improper_duration_color{1.0, 0.0, 0.0, 1.0};
    const SDL_DColor improper_alert_color{1.0, 195 / 255.0, 4 / 255.0, 1.0};
*/
    const SDL_DColor& sit_duration_color = fields.legend_workout_duration.cairo_color;
    // const SDL_DColor& improper_duration_color = fields.legend_improper_duration.cairo_color;
    const SDL_DColor& improper_alert_color = fields.legend_improper_alert.cairo_color;
    const SDL_DColor& share_alert_color = fields.legend_share_alert.cairo_color;

    // Draw horizontal grid lines
    const double min_sit_duration = 0;
    const double min_type_improper_duration = 0;
    int grid_lines = 6;
    for (int i = 0; i <= grid_lines; i++) {
        double y = chart_margin_top + (chart_height * i) / grid_lines;
        cairo_move_to(cr, chart_margin_left, y);
        cairo_line_to(cr, width - chart_margin_right, y);
        cairo_stroke(cr);

        // Add Y-axis labels
        double sit_value = max_sit_duration - (max_sit_duration - min_sit_duration) * i / grid_lines;
        std::stringstream ss;
        
        ss.str("");
        // ss << std::fixed << std::setprecision(1) << sit_value;
        ss << utils::format_elapse_hm_or_ms(sit_value, false, utils::timesep_unit);

        // left. labels are right-align.
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_set_font_size(cr, cairo_font_size);
        cairo_text_extents(cr, ss.str().c_str(), &extents);
        cairo_move_to(cr, chart_margin_left - fields.Y_axis_label_chart_gap - extents.width / 2, y + 5); // y - 5
        cairo_show_text(cr, ss.str().c_str());

        // right. labels are left-align.
        double type_improper_value = max_type_improper_duration - (max_type_improper_duration - min_type_improper_duration) * i / grid_lines;
        ss.str("");
        ss << std::fixed << std::setprecision(1) << type_improper_value;
        // ss << (int)type_improper_value;
        // cairo_set_source_rgb(cr, 250.0 / 255, 198.0 / 255, 52.0 / 255);
        cairo_set_source_rgb(cr, improper_alert_color.r, improper_alert_color.g, improper_alert_color.b);
        cairo_move_to(cr, chart_margin_left + chart_width + 5, y + 5);
        // cairo_move_to(cr, chart_margin_left + chart_width + Y_axis_label_chart_gap, y + 5);
        cairo_show_text(cr, ss.str().c_str());

        cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);  // Restore grid color
    }
    
    // Draw vertical grid lines
    double column_width = (double)chart_width / TOTAL_COLUMNS;
    double gap = column_width * 0.2;
    double bar_width = column_width - gap;
    double bar_width_by_2 = bar_width / 2;

    int tip_rect_count = 0;
    for (int i = 0; i < TOTAL_COLUMNS; i++) {
        // double x = chart_margin_left + i * column_width + gap / 2;
        double x = chart_margin_left + i * column_width + gap / 2 + bar_width_by_2 / 2;

        double current_y = height - chart_margin_bottom;  // Stack from the bottom
        // workout duration
        double segment_height = normalize2(fields.sit_durations[i], max_sit_duration, chart_height);
        double y = current_y - segment_height;
        draw_rounded_rectangle2(&sit_duration_color, nullptr, float_nposm, cr, x, y, bar_width_by_2, segment_height, 
                0, true, true, false, false);
        tsdl_field* field = &fields.bar_4labels[i].sit;
        field->offset.x = x + bar_width_by_2 / 2 - field->name_text_size.x / 2;
        field->offset.x -= SDL_min(gap / 2, 15);
        field->offset.y = y - field->name_text_size.y;

        SDL_Rect& tip_rect = fields.tip_rects[tip_rect_count ++];
        double bonus_w_by_2 = 0;
        if (bar_width_by_2 < 16 * gui2::twidget::hdpi_scale) {
            bonus_w_by_2 = bar_width / 6;
        }
        // tip_rect = SDL_Rect{(int)(x - bonus_w_by_2), (int)y, 
        //    (int)(bar_width_by_2 + bonus_w_by_2 * 2), (int)segment_height};

        tip_rect = SDL_Rect{round_double(x - bonus_w_by_2), (int)y, 
            round_double(bar_width_by_2 + bonus_w_by_2 * 2), (int)segment_height};


        x += bar_width_by_2;
/*
        // improper duration
        segment_height = normalize2(fields.improper_durations[i], max_sit_duration, chart_height);
        y = current_y - segment_height;
        draw_rounded_rectangle2(&improper_duration_color, nullptr, float_nposm, cr, x, y, bar_width_by_2, segment_height, 
                0, true, true, false, false);
        field = &fields.bar_4labels[i].improper;
        field->offset.x = x + bar_width_by_2 / 2 - field->name_text_size.x / 2;
        field->offset.x += SDL_min(gap / 2, 15);
        field->offset.y = y - field->name_text_size.y;

        x += bar_width_by_2;
*/
    }
    VALIDATE(tip_rect_count == TOTAL_COLUMNS, null_str);
    
    // ===== 6. Draw axes (bold black) =====
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, 2.0);
 /*   
    // y-axis
    cairo_move_to(cr, chart_margin_left, chart_margin_top);
    cairo_line_to(cr, chart_margin_left, height - chart_margin_bottom);
    cairo_stroke(cr);
*/    
    // x-axis
    cairo_move_to(cr, chart_margin_left, height - chart_margin_bottom);
    cairo_line_to(cr, width - chart_margin_right, height - chart_margin_bottom);
    cairo_stroke(cr);
    
    // ===== 7. Calculate point coordinates =====
    std::vector<SDL_DPoint> points(TOTAL_COLUMNS);
    double x_step = (double)chart_width / (TOTAL_COLUMNS - 1);
    
    tsdl_field* sdl_field = nullptr;
    for (int alert_at = 0; alert_at < 2; alert_at ++) {
        if (!fields.is_sharing && alert_at > 0) {
            break;
        }
        const int* alerts = alert_at == 0? fields.improper_alerts: fields.share_alerts;
        const SDL_DColor& alert_color = alert_at == 0? improper_alert_color: share_alert_color;
        for (int i = 0; i < TOTAL_COLUMNS; i++) {
            double x = chart_margin_left + i * column_width + column_width / 2;
            double y = height - chart_margin_bottom - normalize(alerts[i], 
                min_type_improper_duration, max_type_improper_duration, chart_height);
            points[i] = {x, y};
        }
    
        // ===== 8. Draw polyline =====
        // SDL_Log("{dbg-days}8. Draw polyline");
        double line_width = 2.5;
        if (fields.is_sharing) {
            line_width = alert_at == 0? 3.0: 1.0;
        }
        draw_smooth_curve_optimized(cr, alert_color, line_width, points);
    
        // ===== 9. Draw data point markers =====
        // SDL_Log("{dbg-days}9. Draw data point markers");
        for (int i = 0; i < TOTAL_COLUMNS; i++) {
            double x = points[i].x;
            double y = points[i].y;
        
            double point_radius = 5;

            // Draw white filled circles
            cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
            cairo_arc(cr, x, y, point_radius, 0, 2 * M_PI);
            cairo_fill(cr);
        
            // Draw blue border
            cairo_set_source_rgb(cr, alert_color.r, alert_color.g, alert_color.b);
            cairo_set_line_width(cr, 2.0);
            cairo_arc(cr, x, y, point_radius, 0, 2 * M_PI);
            cairo_stroke(cr);

            // Add value labels above the points
            sdl_field = alert_at == 0? &fields.bar_4labels[i].improper_alert: &fields.bar_4labels[i].share_alert;
            sdl_field->offset.x = x - sdl_field->name_text_size.x / 2;
            sdl_field->offset.y = y - sdl_field->name_text_size.y;
        }
    }
    
    // ===== 10. Add X-axis labels =====
    // SDL_Log("{dbg-days}10. Add X-axis labels");
    bool use_sdl_labels = true;
    if (use_sdl_labels) {
        double y = height - chart_margin_bottom;
        for (int i = 0; i < TOTAL_COLUMNS; i++) {
            tsdl_field* field = &fields.bar_4labels[i].day;
            double x = chart_margin_left + i * column_width + column_width / 2;
            field->offset = SDL_Point{(int)x, (int)y};
        }

    } else {
        VALIDATE(false, null_str);
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_set_font_size(cr, cairo_font_size);

        std::vector<std::string> time_labels;
        for (int i = 0; i < TOTAL_COLUMNS; i++) {
            // Generate labels
            SDL_snprintf(buf, sizeof(buf), "%02i:00", i);
            time_labels.push_back(buf);
        }

        cairo_text_extents(cr, time_labels[TOTAL_COLUMNS / 2].c_str(), &extents);
        bool use_rotate = column_width < extents.width + 15;
        // SDL_Log("{dbg}column_width: %.5f, extents.width: %.5f", column_width, extents.width);

        for (int i = 0; i < TOTAL_COLUMNS; i++) {
            double x = chart_margin_left + i * column_width + column_width / 2;
        
            if (!use_rotate) {
                // Get text dimensions for centering
                cairo_text_extents(cr, time_labels[i].c_str(), &extents);
                // Center the text at (0,0)
                cairo_move_to(cr, x - extents.width/2, height - chart_margin_bottom + 12 + extents.height/2);

                // cairo_move_to(cr, 0, 0);

                cairo_show_text(cr, time_labels[i].c_str());

            } else {
                cairo_save(cr);
                cairo_translate(cr, x, height - chart_margin_bottom + 15);
                cairo_rotate(cr, -M_PI / 4);

                // Get text dimensions for centering
                cairo_text_extents(cr, time_labels[i].c_str(), &extents);
                // Center the text at (0,0)
                cairo_move_to(cr, -extents.width/2, extents.height/2);

                // cairo_move_to(cr, 0, 0);

                cairo_show_text(cr, time_labels[i].c_str());
                cairo_restore(cr);
            }
        }
    }
    
    // ===== 11. Add title and axis labels =====
    // SDL_Log("{dbg-days}11. Add title and axis labels");
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    
    // Main title
    fields.title.offset = SDL_Point{margin.x, margin.y};

    // Today
    sdl_field = &fields.this_days;
    int today_gap_x = 24;
    SDL_Rect today_rect{0, margin.y, 2 * today_gap_x + sdl_field->name_text_size.x, fields.title.name_text_size.y};
    today_rect.x = width - margin.x - today_rect.w;
    draw_rounded_rectangle2(&sdl_field->cairo_color, nullptr, float_nposm, cr, today_rect.x, today_rect.y,
        today_rect.w, today_rect.h, radius);
    sdl_field->offset = SDL_Point{today_rect.x + today_gap_x, 
        margin.y + (fields.title.name_text_size.y - sdl_field->name_text_size.y) / 2};
    
    sdl_field = &fields.chart_remark;
    if (!sdl_field->name.empty()) {
        sdl_field->offset = SDL_Point{margin.x, height - margin.y - sdl_field->name_text_size.y};
    }

    // X-axis title
/*
    cairo_set_font_size(cr, 14);
    cairo_move_to(cr, width/2 - 30, height - 10);
    cairo_show_text(cr, "Data Points");
*/   
    // Y-axis title (rotated)
/*
    cairo_save(cr);
    cairo_translate(cr, 20, height/2);
    cairo_rotate(cr, -M_PI/2);
    cairo_set_font_size(cr, 14);
    cairo_move_to(cr, 0, 0);
    cairo_show_text(cr, "Values");
    cairo_restore(cr);
*/    
    // ===== 12. Add legend =====
    // SDL_Log("{dbg-days}12. Add legend");
    int legend_icon_text_gap_x = 3;
    SDL_Point legend_2legend_gap{15, fields.legend_2legend_gap_y};
    SDL_Point legend_size{30, posix_align_ceil2(fields.legend_improper_alert.name_text_size.y - 4, 2)};

    // The legend is split into two lines, both left-aligned. 
    // The first line contains three fixed items, and the second line contains various reasons for improper posture.
    int fixed_legends[10] = {fields.fid_legend_workout_duration,
        fields.fid_legend_improper_alert};
    int fixed_legend_count = 2;
    if (fields.is_sharing) {
        fixed_legends[fixed_legend_count ++] = fields.fid_legend_share_alert;
    }
    int best_fixed_legends_width = 0;
    int max_1th_line_height = 0;
    for (int at = 0; at < fixed_legend_count; at ++) {
        int fid = fixed_legends[at];
        sdl_field = fields.arrays[fid];
        // SDL_Log("{dbg-days}12.2, [%i/%i], fid: %i, sdl_field: %p", at, fixed_legend_count, fid, sdl_field);
        VALIDATE(sdl_field != nullptr, null_str);

        if (at != 0) {
            best_fixed_legends_width += legend_2legend_gap.x;
        }
        best_fixed_legends_width += legend_size.x + legend_icon_text_gap_x + sdl_field->name_text_size.x;
        max_1th_line_height = SDL_max(max_1th_line_height, sdl_field->name_text_size.y);
    }

    const int legend_x = (width - best_fixed_legends_width) / 2;
    const int legend_y = margin.y + fields.title_height;

    int tmp_legend_x = legend_x;
    int tmp_legend_y = legend_y;
    for (int at = 0; at < fixed_legend_count; at ++) {
        int fid = fixed_legends[at];
        sdl_field = fields.arrays[fid];
        VALIDATE(sdl_field != nullptr, null_str);

        int x = tmp_legend_x;
        int y = tmp_legend_y;
        if (fid != fields.fid_legend_improper_alert) {
            draw_rounded_rectangle2(&sdl_field->cairo_color, nullptr, float_nposm, cr, x, 
                y + (sdl_field->name_text_size.y - legend_size.y) / 2, legend_size.x, legend_size.y, 4);
        } else {
            int mid_offset = fields.legend_improper_alert.name_text_size.y / 2;
            // Draw legend line
            cairo_set_source_rgb(cr, improper_alert_color.r, improper_alert_color.g, improper_alert_color.b);
            cairo_set_line_width(cr, 2.5);
            cairo_move_to(cr, x, y + mid_offset);
            cairo_line_to(cr, x + legend_size.x, y + mid_offset);
            cairo_stroke(cr);
    
            // Draw legend point
            cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
            cairo_arc(cr, x + legend_size.x / 2, y + mid_offset, 5, 0, 2 * M_PI);
            cairo_fill(cr);
            // cairo_fill_preserve(cr);
    
            cairo_set_source_rgb(cr, improper_alert_color.r, improper_alert_color.g, improper_alert_color.b);
            cairo_arc(cr, x + legend_size.x / 2, y + mid_offset, 5, 0, 2 * M_PI);
            cairo_stroke(cr);
        }

        sdl_field->offset.x = tmp_legend_x + legend_size.x + legend_icon_text_gap_x;
        sdl_field->offset.y = tmp_legend_y; // (sdl_field->name_text_size.y / 2);

        tmp_legend_x = sdl_field->offset.x + sdl_field->name_text_size.x + legend_2legend_gap.x;
        max_1th_line_height = SDL_max(max_1th_line_height, sdl_field->name_text_size.y);
    }

    // 2th line
   
    // ===== 13. Add statistical information =====
    // SDL_Log("{dbg-days}13. Add statistical information");
    sdl_field = &fields.left_y_axis;
    sdl_field->offset.x = chart_margin_left - sdl_field->name_font_size * 3;
    sdl_field->offset.y = chart_margin_top - fields.y_axis_title_gap_y - sdl_field->name_text_size.y;

    sdl_field = &fields.right_y_axis;
    sdl_field->offset.x = chart_margin_left + chart_width - sdl_field->name_text_size.x + sdl_field->name_font_size * 3;
    sdl_field->offset.y = chart_margin_top - fields.y_axis_title_gap_y - sdl_field->name_text_size.y;
    
    // ==== 14. draw play icon ====
    if (fields.share != bool_set_none) {
        draw_check_icon2(cr, fields.share == bool_set_true, fields.pl_btn_rects[pl_btn_share]);
    }

    // ===== 15. Save image and clean up resources =====
    result = argb_mat_from_CAIRO_FORMAT_ARGB32(surface, buffer.data(), width, height);

    // cairo_surface_write_to_png(surface, (game_config::preferences_dir + "/line_chart.png").c_str());
    // SDL_Log("Line chart saved to line_chart.png");
    
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    
    return result;
}

// 绘制带柔和边缘阴影的矩形背景（类似参考图中的阴影过渡效果）
void draw_rect_with_soft_edge_shadow(cairo_t *cr, 
                                      double rect_x, double rect_y, 
                                      double rect_w, double rect_h, 
                                      double corner_radius,
                                      double shadow_blur_radius,
                                      double shadow_offset_x,
                                      double shadow_offset_y)
{    
    // ============================================================
    // 方法：使用 Cairo 的 group + 模糊滤镜（需要 Cairo 1.10+）
    // 或者通过多层半透明矩形叠加来模拟阴影渐变
    // ============================================================
    
    // 保存状态
    cairo_save(cr);
    
    // ---------- 第1步：绘制阴影（底层）----------
    // 方法：绘制一个比主矩形略大的圆角矩形，填充半透明渐变色，模拟阴影扩散
    
    // 阴影区域比主矩形大
    double shadow_x = rect_x + shadow_offset_x - shadow_blur_radius;
    double shadow_y = rect_y + shadow_offset_y - shadow_blur_radius;
    double shadow_w = rect_w + shadow_blur_radius * 2;
    double shadow_h = rect_h + shadow_blur_radius * 2;
    double shadow_radius = corner_radius + shadow_blur_radius * 0.8;
    
    // 创建径向渐变阴影（从边缘向外淡出）
    // 使用多个半透明层叠来实现软阴影效果
    for (int layer = 0; layer < 8; layer++) {
        double t = (double)layer / 8.0;  // 0 -> 1
        double expand = shadow_blur_radius * t;
        double alpha = 0.12 * (1.0 - t * 0.7);  // 外层更淡
        
        double layer_x = shadow_x + expand * 0.5;
        double layer_y = shadow_y + expand * 0.5;
        double layer_w = shadow_w - expand;
        double layer_h = shadow_h - expand;
        double layer_r = shadow_radius - expand * 0.3;
        if (layer_r < 2) layer_r = 2;
        
        cairo_new_path(cr);
        // 绘制圆角矩形路径
        cairo_move_to(cr, layer_x + layer_r, layer_y);
        cairo_line_to(cr, layer_x + layer_w - layer_r, layer_y);
        cairo_curve_to(cr, layer_x + layer_w, layer_y, 
                           layer_x + layer_w, layer_y,
                           layer_x + layer_w, layer_y + layer_r);
        cairo_line_to(cr, layer_x + layer_w, layer_y + layer_h - layer_r);
        cairo_curve_to(cr, layer_x + layer_w, layer_y + layer_h,
                           layer_x + layer_w, layer_y + layer_h,
                           layer_x + layer_w - layer_r, layer_y + layer_h);
        cairo_line_to(cr, layer_x + layer_r, layer_y + layer_h);
        cairo_curve_to(cr, layer_x, layer_y + layer_h,
                           layer_x, layer_y + layer_h,
                           layer_x, layer_y + layer_h - layer_r);
        cairo_line_to(cr, layer_x, layer_y + layer_r);
        cairo_curve_to(cr, layer_x, layer_y,
                           layer_x, layer_y,
                           layer_x + layer_r, layer_y);
        cairo_close_path(cr);
        
        // 阴影颜色：深灰到半透明
        cairo_set_source_rgba(cr, 0.25, 0.22, 0.20, alpha * (1.0 - t * 0.5));
        cairo_fill(cr);
    }
    
    // 额外添加一层高斯风格的径向阴影（中心暗、边缘淡）
    cairo_push_group(cr);
    cairo_new_path(cr);
    cairo_move_to(cr, shadow_x + shadow_radius, shadow_y);
    cairo_line_to(cr, shadow_x + shadow_w - shadow_radius, shadow_y);
    cairo_curve_to(cr, shadow_x + shadow_w, shadow_y, shadow_x + shadow_w, shadow_y,
                   shadow_x + shadow_w, shadow_y + shadow_radius);
    cairo_line_to(cr, shadow_x + shadow_w, shadow_y + shadow_h - shadow_radius);
    cairo_curve_to(cr, shadow_x + shadow_w, shadow_y + shadow_h,
                   shadow_x + shadow_w, shadow_y + shadow_h,
                   shadow_x + shadow_w - shadow_radius, shadow_y + shadow_h);
    cairo_line_to(cr, shadow_x + shadow_radius, shadow_y + shadow_h);
    cairo_curve_to(cr, shadow_x, shadow_y + shadow_h,
                   shadow_x, shadow_y + shadow_h,
                   shadow_x, shadow_y + shadow_h - shadow_radius);
    cairo_line_to(cr, shadow_x, shadow_y + shadow_radius);
    cairo_curve_to(cr, shadow_x, shadow_y, shadow_x, shadow_y,
                   shadow_x + shadow_radius, shadow_y);
    cairo_close_path(cr);
    
    cairo_pattern_t *shadow_grad = cairo_pattern_create_radial(
        rect_x + rect_w/2, rect_y + rect_h/2, 10,
        rect_x + rect_w/2, rect_y + rect_h/2, rect_w * 0.8
    );
    cairo_pattern_add_color_stop_rgba(shadow_grad, 0.0, 0.2, 0.18, 0.15, 0.25);
    cairo_pattern_add_color_stop_rgba(shadow_grad, 0.5, 0.2, 0.18, 0.15, 0.12);
    cairo_pattern_add_color_stop_rgba(shadow_grad, 1.0, 0.2, 0.18, 0.15, 0.0);
    cairo_set_source(cr, shadow_grad);
    cairo_fill(cr);
    cairo_pattern_destroy(shadow_grad);
    cairo_pop_group_to_source(cr);
    cairo_paint_with_alpha(cr, 0.5);
    
    // ---------- 第2步：绘制主矩形背景 ----------
    cairo_new_path(cr);
    cairo_move_to(cr, rect_x + corner_radius, rect_y);
    cairo_line_to(cr, rect_x + rect_w - corner_radius, rect_y);
    cairo_curve_to(cr, rect_x + rect_w, rect_y, 
                       rect_x + rect_w, rect_y,
                       rect_x + rect_w, rect_y + corner_radius);
    cairo_line_to(cr, rect_x + rect_w, rect_y + rect_h - corner_radius);
    cairo_curve_to(cr, rect_x + rect_w, rect_y + rect_h,
                       rect_x + rect_w, rect_y + rect_h,
                       rect_x + rect_w - corner_radius, rect_y + rect_h);
    cairo_line_to(cr, rect_x + corner_radius, rect_y + rect_h);
    cairo_curve_to(cr, rect_x, rect_y + rect_h,
                       rect_x, rect_y + rect_h,
                       rect_x, rect_y + rect_h - corner_radius);
    cairo_line_to(cr, rect_x, rect_y + corner_radius);
    cairo_curve_to(cr, rect_x, rect_y,
                       rect_x, rect_y,
                       rect_x + corner_radius, rect_y);
    cairo_close_path(cr);
    
    // 主背景渐变（类似参考图中的米白/暖色背景）
    cairo_pattern_t *bg_grad = cairo_pattern_create_linear(rect_x, rect_y, 
                                                            rect_x + rect_w, rect_y + rect_h);
    cairo_pattern_add_color_stop_rgb(bg_grad, 0.0, 0.98, 0.96, 0.91);
    cairo_pattern_add_color_stop_rgb(bg_grad, 0.4, 0.96, 0.93, 0.87);
    cairo_pattern_add_color_stop_rgb(bg_grad, 1.0, 0.92, 0.88, 0.82);
    cairo_set_source(cr, bg_grad);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(bg_grad);
    
    // ---------- 第3步：内阴影/内发光（增加立体感）----------
    cairo_pattern_t *inner_shadow = cairo_pattern_create_linear(rect_x, rect_y,
                                                                 rect_x + rect_w * 0.3,
                                                                 rect_y + rect_h);
    cairo_pattern_add_color_stop_rgba(inner_shadow, 0.0, 1.0, 1.0, 0.95, 0.4);
    cairo_pattern_add_color_stop_rgba(inner_shadow, 0.6, 0.98, 0.95, 0.88, 0.1);
    cairo_pattern_add_color_stop_rgba(inner_shadow, 1.0, 0.85, 0.80, 0.72, 0.25);
    cairo_set_source(cr, inner_shadow);
    cairo_fill(cr);
    cairo_pattern_destroy(inner_shadow);
    
    // ---------- 第4步：边缘高光线（微弱）----------
    cairo_new_path(cr);
    cairo_move_to(cr, rect_x + corner_radius, rect_y);
    cairo_line_to(cr, rect_x + rect_w - corner_radius, rect_y);
    cairo_curve_to(cr, rect_x + rect_w, rect_y, rect_x + rect_w, rect_y,
                   rect_x + rect_w, rect_y + corner_radius);
    cairo_set_source_rgba(cr, 1.0, 1.0, 0.95, 0.35);
    cairo_set_line_width(cr, 1.2);
    cairo_stroke(cr);
    
    cairo_restore(cr);
}

// 添加细微的纹理噪点（可选，增加质感）
void add_subtle_noise(cairo_t *cr, int width, int height, double intensity)
{
    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    
    // 使用随机半透明点模拟细微纸张纹理
    srand(12345);
    for (int i = 0; i < 3000; i++) {
        double x = rand() % width;
        double y = rand() % height;
        double alpha = (rand() % 100) / 10000.0 * intensity;
        cairo_set_source_rgba(cr, 0.4, 0.38, 0.35, alpha);
        cairo_arc(cr, x, y, 0.5, 0, 2 * M_PI);
        cairo_fill(cr);
    }
    cairo_restore(cr);
}

cv::Mat draw_tip_mat(bool to_image, int width, int height, double radius, const SDL_Point& margin, 
    int shadow_blur_radius, ttip_fields& fields)
{
    // VALIDATE(fields.phase_count <= WKO_MAX_PHASE_COUNT, null_str);

    // fields.pre_fill_sdl_fields();

    cv::Mat result;

    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
    std::vector<unsigned char> buffer(height * stride);
    
    // ===== 2. Create a Cairo surface and context =====
    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        buffer.data(),
        CAIRO_FORMAT_ARGB32,
        width,
        height,
        stride
    );
    
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        SDL_Log("Failed to create surface");
        return result;
    }
    
    cairo_t* cr = cairo_create(surface);

    tsdl_field* sdl_field = nullptr;
    // ===== 4. Set white background =====
/*
    cairo_pattern_t *global_bg = cairo_pattern_create_linear(0, 0, width, height);
    cairo_pattern_add_color_stop_rgb(global_bg, 0.0, 0.85, 0.83, 0.80);
    cairo_pattern_add_color_stop_rgb(global_bg, 1.0, 0.75, 0.73, 0.70);
    cairo_set_source(cr, global_bg);
    cairo_paint(cr);
    cairo_pattern_destroy(global_bg);
 */   
    double corner_radius = radius; // 22
    // double shadow_blur_radius = 4; // 18
    // double shadow_offset_x = 2; // 4
    // double shadow_offset_y = 2; // 5
    int shadow_offset_x = shadow_blur_radius > 2? 2: 0; // 4
    int shadow_offset_y = shadow_blur_radius > 2? 2: 0; // 5
    // 
    // 2. 绘制带阴影的主矩形
    // 参数: x, y, 宽, 高, 圆角半径, 阴影模糊半径, 阴影偏移X, 阴影偏移Y
    SDL_Rect rect{shadow_blur_radius - shadow_offset_x, shadow_blur_radius - shadow_offset_y, 0, 0};
    rect.w = width - shadow_blur_radius * 2;
    rect.h = height - shadow_blur_radius * 2;
    draw_rect_with_soft_edge_shadow(cr, 
                                    rect.x, rect.y,      // 矩形位置 // 80, 60
                                    rect.w, rect.h,    // 矩形尺寸
                                    corner_radius,          // 圆角半径
                                    shadow_blur_radius,          // 阴影模糊半径（控制阴影扩散程度）
                                    shadow_offset_x, shadow_offset_y);       // 阴影偏移（右下方向）
    
    // 3. 添加细微纹理（模仿纸张/参考图的质感）
    add_subtle_noise(cr, width, height, 0.5);

    int offset_y = 0;
    // header
    sdl_field = fields.arrays[fields.fid_header];
    sdl_field->offset.x = rect.x + margin.x;
    sdl_field->offset.y = rect.y + margin.y;
    offset_y = sdl_field->offset.y + sdl_field->name_text_size.y;

    // horizontal line
    double line_width = 0.8;
    double y = offset_y + (fields.hdr_label_gap_y - line_width) / 2;
    draw_line(cr, rect.x + margin.x, y, width - shadow_blur_radius - margin.x, y, SDL_DColor{0.0, 0.0, 0.0, 0.8}, line_width);
    offset_y += fields.hdr_label_gap_y;

    // left-right label
    sdl_field = fields.arrays[fields.fid_left_label];
    sdl_field->offset.x = rect.x + margin.x;
    sdl_field->offset.y = offset_y;
    int left_label_width = sdl_field->name_text_size.x;

    sdl_field = fields.arrays[fields.fid_right_label];
    sdl_field->offset.x = rect.x + margin.x + left_label_width + fields.lr_gap_x;
    sdl_field->offset.y = offset_y;


    // ===== 14. Save image and clean up resources =====
    result = argb_mat_from_CAIRO_FORMAT_ARGB32(surface, buffer.data(), width, height);

    // cairo_surface_write_to_png(surface, (game_config::preferences_dir + "/line_chart.png").c_str());
    // SDL_Log("Line chart saved to line_chart.png");
    
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    
    // fields.post_render_sdl_fields(result);
    return result;
}

cv::Mat draw_multcol_mat(double radius, const SDL_DColor& fill_color, const SDL_Point& margin, tmultcol_fields& fields)
{
    cv::Mat result;

    const int width = fields.max_width;

    int max_height = 0;
    for (std::vector<tmultcol_fields::tcol>::const_iterator it = fields.cols.begin(); it != fields.cols.end(); ++ it) {
        const tmultcol_fields::tcol& col = *it;
        int h = col.line1.name_text_size.y + col.line2.name_text_size.y;
        if (h > max_height) {
            max_height = h;
        }
    }
    const int height = margin.y * 2 + max_height;

    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
    std::vector<unsigned char> buffer(height * stride);
    
    // ===== 3. Create a Cairo surface and context =====
    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        buffer.data(),
        CAIRO_FORMAT_ARGB32,
        width,
        height,
        stride
    );
    
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        SDL_Log("Failed to create surface");
        return result;
    }
    
    cairo_t* cr = cairo_create(surface);

    // ===== 4. Set white background =====
    draw_canvas(cr, width, height, radius, fill_color, NULL, true, true, true, true);

    // ===== 5. Draw my image =====
    // int margin_left = margin.x;
    // int margin_right = 20;
    // int margin_top = margin.y;
    // int margin_bottom = 15;

    int col_count = fields.cols.size();
    const int usable_width = width - margin.x * 2;  
    int edge_gap_x = 12 * gui2::twidget::hdpi_scale;
    if (fields.mat_type == multcolmattype_poses) {
        edge_gap_x = 0;
    }
    const int text_usable_width = usable_width - edge_gap_x * 2;

    const tmultcol_fields::tcol& last_col = fields.cols[col_count - 1];
    const int last_col_width = SDL_max(last_col.line1.name_text_size.x, last_col.line2.name_text_size.x);
    int non_last_col_width = (text_usable_width - last_col_width) / (col_count - 1);
    int x_offset = margin.x + edge_gap_x;
    for (int at = 0; at < col_count; at ++) {
        tmultcol_fields::tcol& col = fields.cols[at];

        col.line1.offset.x = x_offset;
        col.line1.offset.y = margin.y;
            
        col.line2.offset.x = x_offset;
        col.line2.offset.y = margin.y + col.line1.name_text_size.y;

        x_offset += non_last_col_width;
    }

    // ===== 14. Save image and clean up resources =====
    result = argb_mat_from_CAIRO_FORMAT_ARGB32(surface, buffer.data(), width, height);
    
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    return result;
}

}
