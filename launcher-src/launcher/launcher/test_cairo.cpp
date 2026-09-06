#include <cairo/cairo.h>
#include <vector>
#include <math.h>
#include <stdio.h>
#include <iomanip>
#include "sdl_utils.hpp"
#include <SDL_image.h>
#include "rose_config.hpp"
#include "cairo2.hpp"
#include <opencv2/imgproc.hpp>
#include "rose_mediapipe_api.hpp"
#include "font.hpp"
#include "filesystem.hpp"


// #include <cairo.h>
// #include <cairo-svg.h>
#include <math.h>
#include <stdio.h>

// 绘制带柔和边缘阴影的矩形背景（类似参考图中的阴影过渡效果）
void draw_rect_with_soft_edge_shadow(cairo_t *cr, 
                                      double rect_x, double rect_y, 
                                      double rect_w, double rect_h, 
                                      double corner_radius,
                                      double shadow_blur_radius,
                                      double shadow_offset_x,
                                      double shadow_offset_y) {
    
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
void add_subtle_noise(cairo_t *cr, int width, int height, double intensity) {
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

int test_cairo() {
    // 创建画布
    int width = 720;
    int height = 500;
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t *cr = cairo_create(surface);
    
    // 1. 绘制整体背景（浅灰色，衬托阴影）

    cairo_pattern_t *global_bg = cairo_pattern_create_linear(0, 0, width, height);
    cairo_pattern_add_color_stop_rgb(global_bg, 0.0, 0.85, 0.83, 0.80);
    cairo_pattern_add_color_stop_rgb(global_bg, 1.0, 0.75, 0.73, 0.70);
    cairo_set_source(cr, global_bg);
    cairo_paint(cr);
    cairo_pattern_destroy(global_bg);
    
    // 2. 绘制带阴影的主矩形
    // 参数: x, y, 宽, 高, 圆角半径, 阴影模糊半径, 阴影偏移X, 阴影偏移Y
    draw_rect_with_soft_edge_shadow(cr, 
                                    80, 60,      // 矩形位置
                                    560, 360,    // 矩形尺寸
                                    22,          // 圆角半径
                                    18,          // 阴影模糊半径（控制阴影扩散程度）
                                    4, 5);       // 阴影偏移（右下方向）
    
    // 3. 添加细微纹理（模仿纸张/参考图的质感）
    add_subtle_noise(cr, width, height, 0.5);
/*    
    // 4. 添加一些装饰性文字/标记（类似参考图中的时间数据样式）
    cairo_select_font_face(cr, "Monaco", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12);
    cairo_set_source_rgb(cr, 0.45, 0.42, 0.38);
    
    // 模拟参考图中的数据标记
    const char *markers[] = {"13:00", "16.7", ">", "14:00", "16.7", "<", 
                             "15:00", "16.7", "~", "16:00", "16.7", "+",
                             "17:00", "16.7", "±", "18:00", "16.7", "·"};
    
    double start_x = 110;
    double start_y = 120;
    for (int i = 0; i < 18; i++) {
        cairo_move_to(cr, start_x + (i % 6) * 85, start_y + (i / 6) * 28);
        cairo_show_text(cr, markers[i]);
    }
    
    // 添加底部说明文字
    cairo_set_font_size(cr, 11);
    cairo_move_to(cr, 100, 440);
    cairo_show_text(cr, "边缘阴影: 多层渐变叠加 + 径向模糊模拟 | Cairo 渲染");
*/    
    // 输出 PNG
    cairo_surface_write_to_png(surface, "soft_edge_shadow_rect.png");
    
    // 清理
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
       
    return 0;
}