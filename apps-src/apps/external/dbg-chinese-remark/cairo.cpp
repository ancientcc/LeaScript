void draw_smooth_curve_optimized(cairo_t* cr,
                                 const std::vector<std::pair<double, double>>& points,
                                 double tension = 0.5) {
    
    int n = points.size();
    if (n < 2) return;
    
    cairo_set_line_width(cr, 2.5);
    cairo_set_source_rgb(cr, 0.9, 0.7, 0.1);  // 黄色
    
    // 移动到第一个点
    cairo_move_to(cr, points[0].first, points[0].second);
    
    // 水平段阈值
    const double HORIZONTAL_THRESHOLD = 0.001;
    
    for (int i = 0; i < n - 1; i++) {
        double x1 = points[i].first;
        double y1 = points[i].second;
        double x2 = points[i+1].first;
        double y2 = points[i+1].second;
        
        // 检查当前段是否水平
        bool is_horizontal = (fabs(y2 - y1) < HORIZONTAL_THRESHOLD);
        
        if (is_horizontal) {
            // 水平段：直接画直线
            cairo_line_to(cr, x2, y2);
        } else {
            // 非水平段：使用 Catmull-Rom 曲线
            // 获取前后点
            double x0, y0, x3, y3;
            
            if (i == 0) {
                x0 = x1;
                y0 = y1;
            } else {
                x0 = points[i-1].first;
                y0 = points[i-1].second;
            }
            
            if (i == n - 2) {
                x3 = x2;
                y3 = y2;
            } else {
                x3 = points[i+2].first;
                y3 = points[i+2].second;
            }
            
            // 检查是否需要进一步减小弯曲度（前后点也接近水平）
            bool prev_horizontal = (i > 0) ? (fabs(y1 - points[i-1].second) < HORIZONTAL_THRESHOLD) : false;
            bool next_horizontal = (i < n - 2) ? (fabs(points[i+2].second - y2) < HORIZONTAL_THRESHOLD) : false;
            
            double t = tension;
            
            // 如果当前段是倾斜的，但前后段是水平的，减少张力使过渡更平滑
            if (prev_horizontal || next_horizontal) {
                t *= 0.4;  // 减少张力，让曲线更接近直线
            }
            
            // 额外检查：如果 y 值变化很小，也减小张力
            double y_change = fabs(y2 - y1);
            if (y_change < 10.0) {  // Y变化小于10像素
                t *= (y_change / 10.0) * 0.8;
                t = std::max(t, 0.1);
            }
            
            // 计算 Catmull-Rom 控制点
            double ctrl1_x = x1 + (x2 - x0) * t / 3.0;
            double ctrl1_y = y1 + (y2 - y0) * t / 3.0;
            double ctrl2_x = x2 - (x3 - x1) * t / 3.0;
            double ctrl2_y = y2 - (y3 - y1) * t / 3.0;
            
            // 对于倾斜段，但 Y 变化小，确保控制点 Y 值不会偏离太大
            if (y_change < 20.0) {
                ctrl1_y = std::max(std::min(ctrl1_y, std::max(y1, y2) + 5), std::min(y1, y2) - 5);
                ctrl2_y = std::max(std::min(ctrl2_y, std::max(y1, y2) + 5), std::min(y1, y2) - 5);
            }
            
            cairo_curve_to(cr, ctrl1_x, ctrl1_y, ctrl2_x, ctrl2_y, x2, y2);
        }
    }
    
    cairo_stroke(cr);
}