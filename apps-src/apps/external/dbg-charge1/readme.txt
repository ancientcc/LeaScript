cv::HoughLinesP(dilate, plines, 1, CV_PI / 180, 25, 0, 10); // 40/35

LaserScan-error-nolonely.msg
200时，即使中间没有孤悬点，依旧把前、后错误地认为是一条直线。


------------------------------
为什么不把精度设为400？——设到400时，意味着每像素2.5mm，可雷达精度跟不到，致使雷达各点之间在图像的距离较大，会让cv::HoughLinesP认为充电桩平面不是一条直线。
LaserScan-notfind-400.msg()
400时，即精度达到了每像素2.5毫米。此时雷达点云不到这个精度，在laser_scan-notfind-400.png，可看到出来的点云可没我连续的两点了。由于间隔太大，本来是充电平面的(17.5cm)，cv::HoughLinesP不认为它能生成一条直线。
1)gray-dilate-1st-notfind-400.png。膨胀半径用了上3。
2)HoughLinesP-notfind-400.png。
cv::HoughLinesP(dilate, plines, 1, CV_PI / 180, 20, 0, 10);
累加平面的阈值参数threshold已用了20，cv::HoughLinesP依旧不认为它能生成一条直线。
---------------------------------