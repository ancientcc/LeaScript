#7 [36/52]ms: 21, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.42933(208, 227), end_dist: 0.44949(210, 166), theta: 88.122
#7 [40/52]ms: 21, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.45400(209, 166), end_dist: 0.41484(211, 227), theta: -88.122
#7 [49/52]ms: 21, charge_width: 0.270, line.width_m: 0.30565, line.y_diff_m: -0.04000, start_dist: 0.46760(206, 166), end_dist: 0.41967(210, 227), theta: -86.248
#7 ms: 21, charge_width: 0.270, line.width_m: 0.28504(0.30565), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45436(0.46760), end_dist: 0.42201(0.41967), theta: -88.994(-86.248)
18318 y0_2th_forward, line(width_m: 0.28379, y_diff_m: -0.04937, theta: -88.990(to_vert: 1.009)), dist: 0.42201 ==> vel: (0.07880, 0, 10.000)



#43 [53/62]ms: 24, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.45853(208, 166), end_dist: 0.41967(210, 227), theta: -88.122
#43 ms: 24, charge_width: 0.270, line.width_m: 0.28504(0.30516), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45436(0.45853), end_dist: 0.42201(0.41967), theta: -88.994(-88.122)
#44 [38/53]ms: 16, charge_width: 0.270, line.width_m: 0.30504, line.y_diff_m: -0.04000, start_dist: 0.45853(208, 164), end_dist: 0.42449(209, 225), theta: -89.060
#44 ms: 16, charge_width: 0.270, line.width_m: 0.28504(0.30504), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45436(0.45853), end_dist: 0.42201(0.42449), theta: -88.994(-89.060)
#45 [46/53]ms: 14, charge_width: 0.270, line.width_m: 0.30504, line.y_diff_m: -0.04000, start_dist: 0.46306(207, 168), end_dist: 0.42933(208, 229), theta: -89.060
#45 ms: 14, charge_width: 0.270, line.width_m: 0.28504(0.30504), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45436(0.46306), end_dist: 0.42201(0.42933), theta: -88.994(-89.060)
#46 [39/57]ms: 14, charge_width: 0.270, line.width_m: 0.29516, line.y_diff_m: -0.03500, start_dist: 0.44980(209, 170), end_dist: 0.41484(211, 229), theta: -88.058
#46 ms: 14, charge_width: 0.270, line.width_m: 0.27504(0.29516), line.y_diff_m: -0.03500(-0.03500)(error: 0.01000), start_dist: 0.44119(0.44980), end_dist: 0.42201(0.41484), theta: 88.958(-88.058)
22210 y0_1th_largetheta, line(width_m: 0.28254, y_diff_m: -0.04875, theta: -44.506(to_vert: 45.493)), dist: 0.42201 ==> vel: (0.01000, 0, 65.493)

43(-88.994), 44(-88.994), 45(-88.994), 46(88.958) --> 取4个的平均值，出来-44.506。

在绝对值近乎90度时，会出现正、负角度摇摆，此时若计算角度平均值，就出错了。
-----{vel} next_sample_index: 2---
[0/4]start: (226, 192), end(227, 249), width_m(0.285043), y_diff_m: 0.16000, theta: -1.553(-88.994)
[1/4]start: (226, 192), end(227, 249), width_m(0.285043), y_diff_m: 0.16000, theta: -1.553(-88.994)
[2/4]start: (226, 189), end(226, 246), width_m(0.284999), y_diff_m: 0.16000, theta: 1.570(90.000)
[3/4]start: (223, 193), end(226, 250), width_m(0.285394), y_diff_m: 0.16000, theta: -1.518(-86.987)
--------
is_sample_stable: [0] - [1]: start(0, 0), end(0, 0)
is_sample_stable: [0] - [2]: start(0, 3), end(1, 3)
is_sample_stable: [0] - [3]: start(3, 1), end(1, 1)
is_sample_stable: [1] - [2]: start(0, 3), end(1, 3)
is_sample_stable: [1] - [3]: start(3, 1), end(1, 1)
is_sample_stable: [2] - [3]: start(3, 4), end(0, 4)




因为start.x总小于end.x，导致2、3的start、end互换了，一旦出现，此时误差基本就会大于6。
[0/4]start: (192, 178), end(196, 233), width_m(0.275726), y_diff_m: 0, theta: -1.498(-85.840)
[1/4]start: (189, 176), end(193, 232), width_m(0.280713), y_diff_m: 0.00250, theta: -1.499(-85.914)
[2/4]start: (193, 182), end(197, 237), width_m(0.275726), y_diff_m: 0, theta: -1.498(-85.840)
[3/4]start: (196, 234), end(192, 179), width_m(0.275726), y_diff_m: 0, theta: -1.498(-85.840)
--------
is_sample_stable: [0] - [1]: start(3, 2), end(3, 1)
is_sample_stable: [0] - [2]: start(1, 4), end(1, 4)
is_sample_stable: [0] - [3]: start(4, 56), end(4, 54)
is_sample_stable: [0] - [3]: start(4, 56), end(4, 54) > threshold(6), return false

当abs_theta小于45度时，总是把x小的那个当start，否则把y小的那个当start。避免放入line_samples_时，出现start、end互换。