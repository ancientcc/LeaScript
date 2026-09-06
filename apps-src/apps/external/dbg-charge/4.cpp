对准充电桩时，随机出现很不相同两个值


17141 {dbg_data}HandleWorkQueue, data_.constraints.size: 2(pre.size) + 0(result.size) = 2
17141 {dbg_data}RunOptimization[1/2], submap_data.size: 1, data_.constraints: 2
17149(use 8 ms) {dbg_data}RunOptimization[2/2], landmark_data.size: 0, global_submap_poses_2d.size: from 1 to 1
17149 {dbg_data}PoseGraph2D::RunFinalOptimization() second WorkItem, max_num_iterations: 50
17149 {dbg_stop_slow}PoseGraph2D::WaitForAllComputations pre constraint_builder_.WhenDone, work_queue_: 0x0
17149 {dbg_stop_slow}PoseGraph2D::WaitForAllComputations pre mutex_.AwaitWithTimeout, num_trajectory_nodes: 2, constraint_builder_.GetNumFinishedNodes(): 2
{dbg_data}WaitForAllComputations, data_.constraints.size: 2(pre.size) + 0(result.size) = 2
17149 {dbg_stop_slow}PoseGraph2D::WaitForAllComputations [KOptimizing: Done.
17149 {dbg_stop_slow}Node::RunFinalOptimization(2/2), post map_builder_bridge_.RunFinalOptimization()
17149 {dbg_stop_slow}pre return
17150 {dbg_stop_slow}Node::FinishAllTrajectories(1/2), map_builder_bridge_.GetTrajectoryStates(): 1
17150 {dbg_stop_slow}Node::FinishAllTrajectories(2/2), map_builder_bridge_.GetTrajectoryStates(): 1
[ INFO] [1733832722.382104600]: {dbg_stop_slow}PoseExtrapolator::~PoseExtrapolator enter.
The thread 5276 has exited with code 0 (0x0).
The thread 16068 has exited with code 0 (0x0).
The thread 18956 has exited with code 0 (0x0).
The thread 17944 has exited with code 0 (0x0).
The thread 20064 has exited with code 0 (0x0).
The thread 19856 has exited with code 0 (0x0).
[ INFO] [1733832722.389783500]: {dbg_stop_slow}MapBuilder::~MapBuilder enter.
[ INFO] [1733832722.390070800]: {dbg_stop_slow}CollatedTrajectoryBuilder::~CollatedTrajectoryBuilder enter.
[ INFO] [1733832722.390307500]: {dbg_stop_slow}PoseExtrapolator::~PoseExtrapolator enter.
17158 {dbg_stop_slow}PoseGraph2D::WaitForAllComputations enter
17158 {dbg_stop_slow}PoseGraph2D::WaitForAllComputations pre predicate, work_queue_: 0x0, num_trajectory_nodes: 2 num_finished_nodes_at_start: 2
17159 {dbg_stop_slow}PoseGraph2D::WaitForAllComputations pre constraint_builder_.WhenDone, work_queue_: 0x0
17159 {dbg_stop_slow}PoseGraph2D::WaitForAllComputations pre mutex_.AwaitWithTimeout, num_trajectory_nodes: 2, constraint_builder_.GetNumFinishedNodes(): 2
{dbg_data}WaitForAllComputations, data_.constraints.size: 2(pre.size) + 0(result.size) = 2
17159 {dbg_stop_slow}PoseGraph2D::WaitForAllComputations [KOptimizing: Done.
The thread 17920 has exited with code 0 (0x0).
The thread 18624 has exited with code 0 (0x0).
The thread 16920 has exited with code 0 (0x0).
The thread 16096 has exited with code 0 (0x0).
The thread 'TransformListener' (10148) has exited with code 0 (0x0).
17172 {dbg_stop_slow}cartographer_ros__cartographer_node exit
The thread 'cartographer_node' (18608) has exited with code 0 (0x0).
The thread 'ActionServerThread' (8168) has exited with code 0 (0x0).
The thread 'MoveBase_planThread' (19124) has exited with code 0 (0x0).
The thread 'updateMap' (15028) has exited with code 0 (0x0).
The thread 'updateMap' (19808) has exited with code 0 (0x0).
The thread 'TransformListener' (10536) has exited with code 0 (0x0).
The thread 'move_base_node' (18668) has exited with code 0 (0x0).
did_move_base_result(3.2), post luafunc and keep_task is true, don't call erase_task()
[ INFO] [1733832722.829723600]: [ros_do_move_client]downCb, Yay! The dishes are now clean
#0 [22/49]ms: 20, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.42933(208, 229), end_dist: 0.45853(208, 168), theta: 90.000
#0 [37/49]ms: 20, charge_width: 0.270, line.width_m: 0.30504, line.y_diff_m: -0.04000, start_dist: 0.42449(209, 229), end_dist: 0.44949(210, 168), theta: 89.060
#0 [47/49]ms: 20, charge_width: 0.270, line.width_m: 0.30504, line.y_diff_m: -0.04000, start_dist: 0.45400(209, 168), end_dist: 0.41967(210, 229), theta: -89.060
#0 ms: 20, charge_width: 0.270, line.width_m: 0.28504(0.30504), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45436(0.45400), end_dist: 0.42201(0.41967), theta: -88.994(-89.060)
#1 [34/55]ms: 16, charge_width: 0.270, line.width_m: 0.28517, line.y_diff_m: -0.03000, start_dist: 0.42449(209, 230), end_dist: 0.43660(211, 173), theta: 87.990
#1 [41/55]ms: 16, charge_width: 0.270, line.width_m: 0.30000, line.y_diff_m: -0.03749, start_dist: 0.42933(208, 230), end_dist: 0.45642(208, 170), theta: 90.000
#1 ms: 16, charge_width: 0.270, line.width_m: 0.27518(0.30000), line.y_diff_m: -0.03500(-0.03749)(error: 0.01000), start_dist: 0.42201(0.42933), end_dist: 0.45499(0.45642), theta: -87.917(90.000)
#2 [34/48]ms: 16, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.45400(209, 166), end_dist: 0.41484(211, 227), theta: -88.122
#2 [38/48]ms: 16, charge_width: 0.270, line.width_m: 0.30536, line.y_diff_m: -0.04000, start_dist: 0.46306(207, 166), end_dist: 0.41967(210, 227), theta: -87.184
#2 ms: 16, charge_width: 0.270, line.width_m: 0.28504(0.30536), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45436(0.46306), end_dist: 0.42201(0.41967), theta: -88.994(-87.184)
#3 [27/57]ms: 32, charge_width: 0.270, line.width_m: 0.29068, line.y_diff_m: -0.03250, start_dist: 0.42933(208, 228), end_dist: 0.43407(212, 170), theta: 86.054
#3 [40/57]ms: 32, charge_width: 0.270, line.width_m: 0.30016, line.y_diff_m: -0.03749, start_dist: 0.43416(207, 228), end_dist: 0.45188(209, 168), theta: 88.090
#3 ms: 32, charge_width: 0.270, line.width_m: 0.27004(0.30016), line.y_diff_m: -0.03250(-0.03749)(error: 0.01000), start_dist: 0.42201(0.43416), end_dist: 0.43923(0.45188), theta: 88.939(88.090)
#4 [32/56]ms: 16, charge_width: 0.270, line.width_m: 0.30000, line.y_diff_m: -0.03749, start_dist: 0.42933(206, 228), end_dist: 0.45642(206, 168), theta: 90.000
#4 [43/56]ms: 16, charge_width: 0.270, line.width_m: 0.30004, line.y_diff_m: -0.03749, start_dist: 0.44735(208, 168), end_dist: 0.41484(209, 228), theta: -89.045
#4 ms: 16, charge_width: 0.270, line.width_m: 0.28004(0.30004), line.y_diff_m: -0.03749(-0.03749)(error: 0.01000), start_dist: 0.45235(0.44735), end_dist: 0.42201(0.41484), theta: -88.976(-89.045)
#5 [38/55]ms: 17, charge_width: 0.270, line.width_m: 0.30536, line.y_diff_m: -0.04000, start_dist: 0.46760(206, 166), end_dist: 0.42449(209, 227), theta: -87.184
#5 ms: 17, charge_width: 0.270, line.width_m: 0.28504(0.30536), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45436(0.46760), end_dist: 0.42201(0.42449), theta: -88.994(-87.184)
#6 [45/60]ms: 18, charge_width: 0.270, line.width_m: 0.30565, line.y_diff_m: -0.04000, start_dist: 0.46306(207, 168), end_dist: 0.41484(211, 229), theta: -86.248
#6 ms: 18, charge_width: 0.270, line.width_m: 0.28504(0.30565), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45436(0.46306), end_dist: 0.42201(0.41484), theta: -88.994(-86.248)
#7 [36/52]ms: 21, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.42933(208, 227), end_dist: 0.44949(210, 166), theta: 88.122
#7 [40/52]ms: 21, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.45400(209, 166), end_dist: 0.41484(211, 227), theta: -88.122
#7 [49/52]ms: 21, charge_width: 0.270, line.width_m: 0.30565, line.y_diff_m: -0.04000, start_dist: 0.46760(206, 166), end_dist: 0.41967(210, 227), theta: -86.248
#7 ms: 21, charge_width: 0.270, line.width_m: 0.28504(0.30565), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45436(0.46760), end_dist: 0.42201(0.41967), theta: -88.994(-86.248)
18318 y0_2th_forward, line(width_m: 0.28379, y_diff_m: -0.04937, theta: -88.990(to_vert: 1.009)), dist: 0.42201 ==> vel: (0.07880, 0, 10.000)
#8 [37/47]ms: 18, charge_width: 0.270, line.width_m: 0.30037, line.y_diff_m: -0.04250, start_dist: 0.43292(206, 230), end_dist: 0.44949(209, 170), theta: 87.137
#8 ms: 18, charge_width: 0.270, line.width_m: 0.28004(0.30037), line.y_diff_m: -0.04250(-0.04250)(error: 0.01000), start_dist: 0.42086(0.43292), end_dist: 0.45436(0.44949), theta: -88.976(87.137)
#9 [35/59]ms: 20, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.42933(205, 225), end_dist: 0.44949(207, 164), theta: 88.122
#9 ms: 20, charge_width: 0.270, line.width_m: 0.28517(0.30516), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.43660(0.42933), end_dist: 0.45436(0.44949), theta: 87.990(88.122)
#10 [43/54]ms: 16, charge_width: 0.270, line.width_m: 0.29538, line.y_diff_m: -0.04000, start_dist: 0.45642(206, 170), end_dist: 0.41355(209, 229), theta: -87.089
#10 ms: 16, charge_width: 0.270, line.width_m: 0.28004(0.29538), line.y_diff_m: -0.03749(-0.04000)(error: 0.01000), start_dist: 0.45235(0.45642), end_dist: 0.42201(0.41355), theta: -88.976(-87.089)
#11 [32/56]ms: 18, charge_width: 0.270, line.width_m: 0.30066, line.y_diff_m: -0.04250, start_dist: 0.46306(207, 164), end_dist: 0.41355(211, 224), theta: -86.185
#11 ms: 18, charge_width: 0.270, line.width_m: 0.28004(0.30066), line.y_diff_m: -0.04250(-0.04250)(error: 0.01000), start_dist: 0.45436(0.46306), end_dist: 0.42086(0.41355), theta: -88.976(-86.185)
#12 [38/53]ms: 17, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.42449(206, 227), end_dist: 0.45400(206, 166), theta: 90.000
#12 [40/53]ms: 17, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.42933(205, 227), end_dist: 0.45853(205, 166), theta: 90.000
#12 [41/53]ms: 17, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.41967(207, 227), end_dist: 0.44949(207, 166), theta: 90.000
#12 ms: 17, charge_width: 0.270, line.width_m: 0.28504(0.30500), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.42201(0.41967), end_dist: 0.45436(0.44949), theta: -88.994(90.000)
#13 [18/49]ms: 15, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.42933(208, 227), end_dist: 0.45853(208, 166), theta: 90.000
#13 ms: 15, charge_width: 0.270, line.width_m: 0.28517(0.30500), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.43660(0.42933), end_dist: 0.45436(0.45853), theta: 87.990(90.000)
#14 [32/61]ms: 16, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.45853(208, 166), end_dist: 0.41967(210, 227), theta: -88.122
#14 [53/61]ms: 16, charge_width: 0.270, line.width_m: 0.30536, line.y_diff_m: -0.04000, start_dist: 0.46760(206, 166), end_dist: 0.42449(209, 227), theta: -87.184
#14 ms: 16, charge_width: 0.270, line.width_m: 0.28504(0.30536), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45436(0.46760), end_dist: 0.42201(0.42449), theta: -88.994(-87.184)
#15 [34/64]ms: 25, charge_width: 0.270, line.width_m: 0.30016, line.y_diff_m: -0.04250, start_dist: 0.45853(208, 163), end_dist: 0.41838(210, 223), theta: -88.090
#15 [52/64]ms: 25, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.46760(206, 163), end_dist: 0.42933(208, 224), theta: -88.122
#15 ms: 25, charge_width: 0.270, line.width_m: 0.28017(0.30516), line.y_diff_m: -0.04250(-0.04000)(error: 0.01000), start_dist: 0.45893(0.46760), end_dist: 0.42086(0.42933), theta: -87.954(-88.122)
#16 [36/53]ms: 25, charge_width: 0.270, line.width_m: 0.30504, line.y_diff_m: -0.04000, start_dist: 0.42933(206, 229), end_dist: 0.45400(207, 168), theta: 89.060
#16 [42/53]ms: 25, charge_width: 0.270, line.width_m: 0.30504, line.y_diff_m: -0.04000, start_dist: 0.44949(208, 168), end_dist: 0.41484(209, 229), theta: -89.060
#16 ms: 25, charge_width: 0.270, line.width_m: 0.28504(0.30504), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45436(0.44949), end_dist: 0.42201(0.41484), theta: -88.994(-89.060)
#17 [36/51]ms: 19, charge_width: 0.270, line.width_m: 0.30004, line.y_diff_m: -0.04250, start_dist: 0.45400(209, 166), end_dist: 0.41838(210, 226), theta: -89.045
#17 [41/51]ms: 19, charge_width: 0.270, line.width_m: 0.30004, line.y_diff_m: -0.04250, start_dist: 0.45853(208, 166), end_dist: 0.42323(209, 226), theta: -89.045
#17 ms: 19, charge_width: 0.270, line.width_m: 0.28004(0.30004), line.y_diff_m: -0.04250(-0.04250)(error: 0.01000), start_dist: 0.45436(0.45853), end_dist: 0.42086(0.42323), theta: -88.976(-89.045)
#18 [27/51]ms: 17, charge_width: 0.270, line.width_m: 0.30000, line.y_diff_m: -0.04250, start_dist: 0.41838(208, 226), end_dist: 0.44949(208, 166), theta: 90.000
#18 [36/51]ms: 17, charge_width: 0.270, line.width_m: 0.30004, line.y_diff_m: -0.04250, start_dist: 0.45853(206, 166), end_dist: 0.42323(207, 226), theta: -89.045
#18 [46/51]ms: 17, charge_width: 0.270, line.width_m: 0.30004, line.y_diff_m: -0.04250, start_dist: 0.46306(205, 166), end_dist: 0.42807(206, 226), theta: -89.045
#18 ms: 17, charge_width: 0.270, line.width_m: 0.28004(0.30004), line.y_diff_m: -0.04250(-0.04250)(error: 0.01000), start_dist: 0.45436(0.46306), end_dist: 0.42086(0.42807), theta: -88.976(-89.045)
#19 [27/59]ms: 16, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.42449(207, 229), end_dist: 0.45400(207, 168), theta: 90.000
#19 [46/59]ms: 16, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.42933(206, 229), end_dist: 0.45853(206, 168), theta: 90.000
#19 [54/59]ms: 16, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.41967(208, 229), end_dist: 0.44949(208, 168), theta: 90.000
#19 ms: 16, charge_width: 0.270, line.width_m: 0.28504(0.30500), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.42201(0.41967), end_dist: 0.45436(0.44949), theta: -88.994(90.000)
#20 [43/65]ms: 16, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.42449(208, 227), end_dist: 0.45400(208, 166), theta: 90.000
#20 [51/65]ms: 16, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.42933(207, 227), end_dist: 0.45853(207, 166), theta: 90.000
#20 ms: 16, charge_width: 0.270, line.width_m: 0.28504(0.30500), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.42201(0.42933), end_dist: 0.45436(0.45853), theta: -88.994(90.000)
#21 [37/51]ms: 15, charge_width: 0.270, line.width_m: 0.30004, line.y_diff_m: -0.04250, start_dist: 0.45853(208, 162), end_dist: 0.42323(209, 222), theta: -89.045
#21 [43/51]ms: 15, charge_width: 0.270, line.width_m: 0.30004, line.y_diff_m: -0.04250, start_dist: 0.45400(209, 162), end_dist: 0.41838(210, 222), theta: -89.045
#21 ms: 15, charge_width: 0.270, line.width_m: 0.28004(0.30004), line.y_diff_m: -0.04250(-0.04250)(error: 0.01000), start_dist: 0.45436(0.45400), end_dist: 0.42086(0.41838), theta: -88.976(-89.045)
#22 [46/59]ms: 15, charge_width: 0.270, line.width_m: 0.28499, line.y_diff_m: -0.03000, start_dist: 0.41967(210, 229), end_dist: 0.44119(210, 172), theta: 90.000
#22 [48/59]ms: 15, charge_width: 0.270, line.width_m: 0.29538, line.y_diff_m: -0.03500, start_dist: 0.46351(206, 170), end_dist: 0.42449(209, 229), theta: -87.089
#22 ms: 15, charge_width: 0.270, line.width_m: 0.27518(0.29538), line.y_diff_m: -0.03500(-0.03500)(error: 0.01000), start_dist: 0.45499(0.46351), end_dist: 0.42201(0.42449), theta: -87.917(-87.089)
#23 [39/53]ms: 18, charge_width: 0.270, line.width_m: 0.30016, line.y_diff_m: -0.04250, start_dist: 0.46306(207, 166), end_dist: 0.42323(209, 226), theta: -88.090
#23 ms: 18, charge_width: 0.270, line.width_m: 0.28004(0.30016), line.y_diff_m: -0.04250(-0.04250)(error: 0.01000), start_dist: 0.45436(0.46306), end_dist: 0.42086(0.42323), theta: -88.976(-88.090)
#24 [29/58]ms: 15, charge_width: 0.270, line.width_m: 0.30536, line.y_diff_m: -0.04000, start_dist: 0.46306(206, 165), end_dist: 0.41967(209, 226), theta: -87.184
#24 ms: 15, charge_width: 0.270, line.width_m: 0.28504(0.30536), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45436(0.46306), end_dist: 0.42201(0.41967), theta: -88.994(-87.184)
#25 [31/57]ms: 15, charge_width: 0.270, line.width_m: 0.28609, line.y_diff_m: -0.05000, start_dist: 0.46306(207, 164), end_dist: 0.40512(212, 221), theta: -84.986
#25 [43/57]ms: 15, charge_width: 0.270, line.width_m: 0.30066, line.y_diff_m: -0.03749, start_dist: 0.46553(206, 165), end_dist: 0.41967(210, 225), theta: -86.185
#25 ms: 15, charge_width: 0.270, line.width_m: 0.28504(0.30066), line.y_diff_m: -0.04000(-0.03749)(error: 0.01000), start_dist: 0.45436(0.46553), end_dist: 0.42201(0.41967), theta: -88.994(-86.185)
#26 [43/61]ms: 16, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.42449(212, 225), end_dist: 0.45400(212, 164), theta: 90.000
#26 [47/61]ms: 16, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.42933(211, 225), end_dist: 0.45853(211, 164), theta: 90.000
#26 ms: 16, charge_width: 0.270, line.width_m: 0.28517(0.30500), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.42201(0.42933), end_dist: 0.45893(0.45853), theta: -87.990(90.000)
#27 [21/48]ms: 15, charge_width: 0.270, line.width_m: 0.30565, line.y_diff_m: -0.04000, start_dist: 0.46760(206, 167), end_dist: 0.41967(210, 228), theta: -86.248
#27 ms: 15, charge_width: 0.270, line.width_m: 0.28517(0.30565), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.46760), end_dist: 0.42201(0.41967), theta: -87.990(-86.248)
#28 [28/54]ms: 16, charge_width: 0.270, line.width_m: 0.30016, line.y_diff_m: -0.04250, start_dist: 0.45400(209, 168), end_dist: 0.41355(211, 228), theta: -88.090
#28 [39/54]ms: 16, charge_width: 0.270, line.width_m: 0.30016, line.y_diff_m: -0.04250, start_dist: 0.42807(208, 228), end_dist: 0.44949(210, 168), theta: 88.090
#28 ms: 16, charge_width: 0.270, line.width_m: 0.28004(0.30016), line.y_diff_m: -0.04250(-0.04250)(error: 0.01000), start_dist: 0.42086(0.42807), end_dist: 0.45436(0.44949), theta: -88.976(88.090)
#29 [39/54]ms: 15, charge_width: 0.270, line.width_m: 0.30000, line.y_diff_m: -0.04250, start_dist: 0.41838(207, 224), end_dist: 0.44949(207, 164), theta: 90.000
#29 ms: 15, charge_width: 0.270, line.width_m: 0.28004(0.30000), line.y_diff_m: -0.04250(-0.04250)(error: 0.01000), start_dist: 0.42086(0.41838), end_dist: 0.45436(0.44949), theta: -88.976(90.000)
#30 [28/54]ms: 20, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.42449(206, 225), end_dist: 0.45400(206, 164), theta: 90.000
#30 [40/54]ms: 20, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.41967(207, 225), end_dist: 0.44949(207, 164), theta: 90.000
#30 [41/54]ms: 20, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.42933(205, 225), end_dist: 0.45853(205, 164), theta: 90.000
#30 ms: 20, charge_width: 0.270, line.width_m: 0.28504(0.30500), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.42201(0.42933), end_dist: 0.45436(0.45853), theta: -88.994(90.000)
#31 [27/57]ms: 14, charge_width: 0.270, line.width_m: 0.28017, line.y_diff_m: -0.04749, start_dist: 0.45188(209, 171), end_dist: 0.40999(211, 227), theta: -87.954
#31 [35/57]ms: 14, charge_width: 0.270, line.width_m: 0.30000, line.y_diff_m: -0.03749, start_dist: 0.42933(208, 231), end_dist: 0.45642(208, 171), theta: 90.000
#31 ms: 14, charge_width: 0.270, line.width_m: 0.28004(0.30000), line.y_diff_m: -0.03749(-0.03749)(error: 0.01000), start_dist: 0.43660(0.42933), end_dist: 0.45694(0.45642), theta: 88.976(90.000)
#32 [26/48]ms: 14, charge_width: 0.270, line.width_m: 0.30504, line.y_diff_m: -0.04000, start_dist: 0.42933(206, 230), end_dist: 0.45400(207, 169), theta: 89.060
#32 ms: 14, charge_width: 0.270, line.width_m: 0.28504(0.30504), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.43660(0.42933), end_dist: 0.45893(0.45400), theta: 88.994(89.060)
#33 [31/45]ms: 16, charge_width: 0.270, line.width_m: 0.30536, line.y_diff_m: -0.04000, start_dist: 0.45853(206, 166), end_dist: 0.41484(209, 227), theta: -87.184
#33 ms: 16, charge_width: 0.270, line.width_m: 0.28504(0.30536), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45436(0.45853), end_dist: 0.42201(0.41484), theta: -88.994(-87.184)
#34 [22/49]ms: 14, charge_width: 0.270, line.width_m: 0.30016, line.y_diff_m: -0.03749, start_dist: 0.45188(209, 166), end_dist: 0.41484(211, 226), theta: -88.090
#34 ms: 14, charge_width: 0.270, line.width_m: 0.28017(0.30016), line.y_diff_m: -0.03749(-0.03749)(error: 0.01000), start_dist: 0.45694(0.45188), end_dist: 0.42201(0.41484), theta: -87.954(-88.090)
#35 [32/48]ms: 15, charge_width: 0.270, line.width_m: 0.30536, line.y_diff_m: -0.04000, start_dist: 0.45853(208, 166), end_dist: 0.41484(211, 227), theta: -87.184
#35 ms: 15, charge_width: 0.270, line.width_m: 0.28504(0.30536), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45436(0.45853), end_dist: 0.42201(0.41484), theta: -88.994(-87.184)
#36 [19/40]ms: 15, charge_width: 0.270, line.width_m: 0.30504, line.y_diff_m: -0.04000, start_dist: 0.45400(208, 165), end_dist: 0.41967(209, 226), theta: -89.060
#36 [29/40]ms: 15, charge_width: 0.270, line.width_m: 0.30504, line.y_diff_m: -0.04000, start_dist: 0.45853(207, 165), end_dist: 0.42449(208, 226), theta: -89.060
#36 ms: 15, charge_width: 0.270, line.width_m: 0.28504(0.30504), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45436(0.45853), end_dist: 0.42201(0.42449), theta: -88.994(-89.060)
#37 [34/53]ms: 15, charge_width: 0.270, line.width_m: 0.30000, line.y_diff_m: -0.04250, start_dist: 0.42323(209, 226), end_dist: 0.45400(209, 166), theta: 90.000
#37 [38/53]ms: 15, charge_width: 0.270, line.width_m: 0.30004, line.y_diff_m: -0.04250, start_dist: 0.44949(210, 166), end_dist: 0.41355(211, 226), theta: -89.045
#37 [44/53]ms: 15, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.42933(208, 227), end_dist: 0.45853(208, 166), theta: 90.000
#37 ms: 15, charge_width: 0.270, line.width_m: 0.28517(0.30500), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.43660(0.42933), end_dist: 0.45436(0.45853), theta: 87.990(90.000)
#38 [36/46]ms: 15, charge_width: 0.270, line.width_m: 0.30004, line.y_diff_m: -0.04250, start_dist: 0.45400(207, 166), end_dist: 0.41838(208, 226), theta: -89.045
#38 [40/46]ms: 15, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.42933(206, 227), end_dist: 0.45853(206, 166), theta: 90.000
#38 ms: 15, charge_width: 0.270, line.width_m: 0.28517(0.30500), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.43660(0.42933), end_dist: 0.45436(0.45853), theta: 87.990(90.000)
#39 [41/49]ms: 16, charge_width: 0.270, line.width_m: 0.29004, line.y_diff_m: -0.03749, start_dist: 0.44980(209, 173), end_dist: 0.41838(210, 231), theta: -89.012
#39 ms: 16, charge_width: 0.270, line.width_m: 0.27018(0.29004), line.y_diff_m: -0.03749(-0.03749)(error: 0.01000), start_dist: 0.45499(0.44980), end_dist: 0.42086(0.41838), theta: -87.878(-89.012)
#40 [38/53]ms: 16, charge_width: 0.270, line.width_m: 0.30000, line.y_diff_m: -0.04250, start_dist: 0.41838(209, 223), end_dist: 0.44949(209, 163), theta: 90.000
#40 [44/53]ms: 16, charge_width: 0.270, line.width_m: 0.30004, line.y_diff_m: -0.04250, start_dist: 0.42807(207, 223), end_dist: 0.45400(208, 163), theta: 89.045
#40 ms: 16, charge_width: 0.270, line.width_m: 0.28004(0.30004), line.y_diff_m: -0.04250(-0.04250)(error: 0.01000), start_dist: 0.42086(0.42807), end_dist: 0.45436(0.45400), theta: -88.976(89.045)
#41 [36/56]ms: 16, charge_width: 0.270, line.width_m: 0.29538, line.y_diff_m: -0.03500, start_dist: 0.45436(208, 170), end_dist: 0.41484(211, 229), theta: -87.089
#41 [48/56]ms: 16, charge_width: 0.270, line.width_m: 0.29567, line.y_diff_m: -0.03500, start_dist: 0.46351(206, 170), end_dist: 0.41967(210, 229), theta: -86.121
#41 ms: 16, charge_width: 0.270, line.width_m: 0.27518(0.29567), line.y_diff_m: -0.03500(-0.03500)(error: 0.01000), start_dist: 0.45499(0.46351), end_dist: 0.42201(0.41967), theta: -87.917(-86.121)
#42 [26/54]ms: 14, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.43416(205, 227), end_dist: 0.45400(207, 166), theta: 88.122
#42 ms: 14, charge_width: 0.270, line.width_m: 0.28504(0.30516), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.42201(0.43416), end_dist: 0.45436(0.45400), theta: -88.994(88.122)
#43 [30/62]ms: 24, charge_width: 0.270, line.width_m: 0.30504, line.y_diff_m: -0.04000, start_dist: 0.44949(210, 166), end_dist: 0.41484(211, 227), theta: -89.060
#43 [44/62]ms: 24, charge_width: 0.270, line.width_m: 0.30504, line.y_diff_m: -0.04000, start_dist: 0.42933(208, 227), end_dist: 0.45400(209, 166), theta: 89.060
#43 [53/62]ms: 24, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.45853(208, 166), end_dist: 0.41967(210, 227), theta: -88.122
#43 ms: 24, charge_width: 0.270, line.width_m: 0.28504(0.30516), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45436(0.45853), end_dist: 0.42201(0.41967), theta: -88.994(-88.122)
#44 [38/53]ms: 16, charge_width: 0.270, line.width_m: 0.30504, line.y_diff_m: -0.04000, start_dist: 0.45853(208, 164), end_dist: 0.42449(209, 225), theta: -89.060
#44 ms: 16, charge_width: 0.270, line.width_m: 0.28504(0.30504), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45436(0.45853), end_dist: 0.42201(0.42449), theta: -88.994(-89.060)
#45 [27/53]ms: 14, charge_width: 0.270, line.width_m: 0.27540, line.y_diff_m: -0.05499, start_dist: 0.45853(208, 168), end_dist: 0.40792(211, 223), theta: -86.877
#45 [46/53]ms: 14, charge_width: 0.270, line.width_m: 0.30504, line.y_diff_m: -0.04000, start_dist: 0.46306(207, 168), end_dist: 0.42933(208, 229), theta: -89.060
#45 ms: 14, charge_width: 0.270, line.width_m: 0.28504(0.30504), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45436(0.46306), end_dist: 0.42201(0.42933), theta: -88.994(-89.060)
#46 [39/57]ms: 14, charge_width: 0.270, line.width_m: 0.29516, line.y_diff_m: -0.03500, start_dist: 0.44980(209, 170), end_dist: 0.41484(211, 229), theta: -88.058
#46 ms: 14, charge_width: 0.270, line.width_m: 0.27504(0.29516), line.y_diff_m: -0.03500(-0.03500)(error: 0.01000), start_dist: 0.44119(0.44980), end_dist: 0.42201(0.41484), theta: 88.958(-88.058)
22210 y0_1th_largetheta, line(width_m: 0.28254, y_diff_m: -0.04875, theta: -44.506(to_vert: 45.493)), dist: 0.42201 ==> vel: (0.01000, 0, 65.493)
#47 [34/53]ms: 14, charge_width: 0.270, line.width_m: 0.30504, line.y_diff_m: -0.04000, start_dist: 0.44949(210, 162), end_dist: 0.41484(211, 223), theta: -89.060
#47 [40/53]ms: 14, charge_width: 0.270, line.width_m: 0.30504, line.y_diff_m: -0.04000, start_dist: 0.45400(209, 162), end_dist: 0.41967(210, 223), theta: -89.060
#47 ms: 14, charge_width: 0.270, line.width_m: 0.28504(0.30504), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45436(0.45400), end_dist: 0.42201(0.41967), theta: -88.994(-89.060)
#48 [39/56]ms: 15, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.45853(208, 168), end_dist: 0.41967(210, 229), theta: -88.122
#48 ms: 15, charge_width: 0.270, line.width_m: 0.28504(0.30516), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45436(0.45853), end_dist: 0.42201(0.41967), theta: -88.994(-88.122)
#49 [28/53]ms: 15, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.45400(208, 167), end_dist: 0.41484(210, 228), theta: -88.122
#49 ms: 15, charge_width: 0.270, line.width_m: 0.28504(0.30516), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45436(0.45400), end_dist: 0.42201(0.41484), theta: -88.994(-88.122)
#50 [34/54]ms: 14, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.42933(208, 231), end_dist: 0.45853(208, 170), theta: 90.000
#50 [41/54]ms: 14, charge_width: 0.270, line.width_m: 0.30504, line.y_diff_m: -0.04000, start_dist: 0.45400(209, 170), end_dist: 0.41967(210, 231), theta: -89.060
#50 ms: 14, charge_width: 0.270, line.width_m: 0.28517(0.30504), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.45400), end_dist: 0.42201(0.41967), theta: -87.990(-89.060)
#51 [36/47]ms: 14, charge_width: 0.270, line.width_m: 0.30504, line.y_diff_m: -0.04000, start_dist: 0.45400(209, 164), end_dist: 0.41967(210, 225), theta: -89.060
#51 ms: 14, charge_width: 0.270, line.width_m: 0.28517(0.30504), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.45400), end_dist: 0.42201(0.41967), theta: -87.990(-89.060)
#52 [34/51]ms: 17, charge_width: 0.270, line.width_m: 0.30066, line.y_diff_m: -0.04250, start_dist: 0.46306(207, 166), end_dist: 0.41355(211, 226), theta: -86.185
#52 ms: 17, charge_width: 0.270, line.width_m: 0.28017(0.30066), line.y_diff_m: -0.04250(-0.04250)(error: 0.01000), start_dist: 0.45893(0.46306), end_dist: 0.42086(0.41355), theta: -87.954(-86.185)
#53 [25/51]ms: 16, charge_width: 0.270, line.width_m: 0.30536, line.y_diff_m: -0.04000, start_dist: 0.46306(207, 168), end_dist: 0.41967(210, 229), theta: -87.184
#53 ms: 16, charge_width: 0.270, line.width_m: 0.28517(0.30536), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.46306), end_dist: 0.42201(0.41967), theta: -87.990(-87.184)
#54 [34/52]ms: 15, charge_width: 0.270, line.width_m: 0.30504, line.y_diff_m: -0.04000, start_dist: 0.42933(208, 225), end_dist: 0.45400(209, 164), theta: 89.060
#54 ms: 15, charge_width: 0.270, line.width_m: 0.28504(0.30504), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.43660(0.42933), end_dist: 0.45893(0.45400), theta: 88.994(89.060)
#55 [24/51]ms: 15, charge_width: 0.270, line.width_m: 0.30536, line.y_diff_m: -0.04000, start_dist: 0.45853(206, 166), end_dist: 0.41484(209, 227), theta: -87.184
#55 ms: 15, charge_width: 0.270, line.width_m: 0.28517(0.30536), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.45853), end_dist: 0.42201(0.41484), theta: -87.990(-87.184)
#56 [29/61]ms: 15, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.45853(208, 164), end_dist: 0.41967(210, 225), theta: -88.122
#56 ms: 15, charge_width: 0.270, line.width_m: 0.28517(0.30516), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.45853), end_dist: 0.42201(0.41967), theta: -87.990(-88.122)
{dbg_lslidar}23282 #100[lslidar]pubish scan, ranges.size: 450 range(0.10, 12.00) scan_time: 0.098 angle(-180.000, 0.801, 180.000)
23291 maybe voice, but voice_len2(16) is < min_voice_len(6400), think not, reset
#57 [41/52]ms: 15, charge_width: 0.270, line.width_m: 0.30647, line.y_diff_m: -0.04000, start_dist: 0.47214(205, 166), end_dist: 0.41484(211, 227), theta: -84.382
#57 ms: 15, charge_width: 0.270, line.width_m: 0.28517(0.30647), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.47214), end_dist: 0.42201(0.41484), theta: -87.990(-84.382)
#58 [30/53]ms: 15, charge_width: 0.270, line.width_m: 0.30504, line.y_diff_m: -0.04000, start_dist: 0.45853(208, 162), end_dist: 0.42449(209, 223), theta: -89.060
#58 [32/53]ms: 15, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.45400(209, 162), end_dist: 0.41484(211, 223), theta: -88.122
#58 ms: 15, charge_width: 0.270, line.width_m: 0.28504(0.30516), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45436(0.45400), end_dist: 0.42201(0.41484), theta: -88.994(-88.122)
#59 [51/52]ms: 14, charge_width: 0.270, line.width_m: 0.29504, line.y_diff_m: -0.03500, start_dist: 0.44525(210, 166), end_dist: 0.41484(211, 225), theta: -89.029
#59 ms: 14, charge_width: 0.270, line.width_m: 0.27504(0.29504), line.y_diff_m: -0.03500(-0.03500)(error: 0.01000), start_dist: 0.44119(0.44525), end_dist: 0.42201(0.41484), theta: 88.958(-89.029)
#60 [33/51]ms: 16, charge_width: 0.270, line.width_m: 0.30016, line.y_diff_m: -0.04250, start_dist: 0.45853(208, 168), end_dist: 0.41838(210, 228), theta: -88.090
#60 [44/51]ms: 16, charge_width: 0.270, line.width_m: 0.30016, line.y_diff_m: -0.04250, start_dist: 0.45400(209, 168), end_dist: 0.41355(211, 228), theta: -88.090
#60 ms: 16, charge_width: 0.270, line.width_m: 0.28017(0.30016), line.y_diff_m: -0.04250(-0.04250)(error: 0.01000), start_dist: 0.45893(0.45400), end_dist: 0.42086(0.41355), theta: -87.954(-88.090)
#61 [25/46]ms: 14, charge_width: 0.270, line.width_m: 0.30565, line.y_diff_m: -0.04000, start_dist: 0.46306(205, 166), end_dist: 0.41484(209, 227), theta: -86.248
#61 ms: 14, charge_width: 0.270, line.width_m: 0.28517(0.30565), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.46306), end_dist: 0.42201(0.41484), theta: -87.990(-86.248)
#62 [28/60]ms: 17, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.45400(208, 165), end_dist: 0.41484(210, 226), theta: -88.122
#62 [34/60]ms: 17, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.45853(207, 165), end_dist: 0.41967(209, 226), theta: -88.122
#62 ms: 17, charge_width: 0.270, line.width_m: 0.28517(0.30516), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.45853), end_dist: 0.42201(0.41967), theta: -87.990(-88.122)
#63 [25/49]ms: 14, charge_width: 0.270, line.width_m: 0.30504, line.y_diff_m: -0.04000, start_dist: 0.45400(209, 166), end_dist: 0.41967(210, 227), theta: -89.060
#63 ms: 14, charge_width: 0.270, line.width_m: 0.28517(0.30504), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.45400), end_dist: 0.42201(0.41967), theta: -87.990(-89.060)
#64 [44/56]ms: 19, charge_width: 0.270, line.width_m: 0.29017, line.y_diff_m: -0.03250, start_dist: 0.42933(208, 226), end_dist: 0.44319(210, 168), theta: 88.025
#64 [50/56]ms: 19, charge_width: 0.270, line.width_m: 0.30565, line.y_diff_m: -0.04000, start_dist: 0.46760(206, 165), end_dist: 0.41967(210, 226), theta: -86.248
#64 ms: 19, charge_width: 0.270, line.width_m: 0.28517(0.30565), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.46760), end_dist: 0.42201(0.41967), theta: -87.990(-86.248)
#65 [34/50]ms: 16, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.42933(206, 230), end_dist: 0.45853(206, 169), theta: 90.000
#65 ms: 16, charge_width: 0.270, line.width_m: 0.28504(0.30500), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.43660(0.42933), end_dist: 0.45893(0.45853), theta: 88.994(90.000)
#66 [24/44]ms: 25, charge_width: 0.270, line.width_m: 0.30037, line.y_diff_m: -0.04250, start_dist: 0.46306(205, 166), end_dist: 0.41838(208, 226), theta: -87.137
#66 [42/44]ms: 25, charge_width: 0.270, line.width_m: 0.30066, line.y_diff_m: -0.04250, start_dist: 0.47214(203, 166), end_dist: 0.42323(207, 226), theta: -86.185
#66 ms: 25, charge_width: 0.270, line.width_m: 0.28017(0.30066), line.y_diff_m: -0.04250(-0.04250)(error: 0.01000), start_dist: 0.45893(0.47214), end_dist: 0.42086(0.42323), theta: -87.954(-86.185)
#67 [38/59]ms: 16, charge_width: 0.270, line.width_m: 0.29516, line.y_diff_m: -0.03500, start_dist: 0.44980(207, 168), end_dist: 0.41484(209, 227), theta: -88.058
#67 ms: 16, charge_width: 0.270, line.width_m: 0.27504(0.29516), line.y_diff_m: -0.03500(-0.03500)(error: 0.01000), start_dist: 0.44119(0.44980), end_dist: 0.42201(0.41484), theta: 88.958(-88.058)
#68 [38/62]ms: 20, charge_width: 0.270, line.width_m: 0.30037, line.y_diff_m: -0.04250, start_dist: 0.46306(207, 164), end_dist: 0.41838(210, 224), theta: -87.137
#68 ms: 20, charge_width: 0.270, line.width_m: 0.28017(0.30037), line.y_diff_m: -0.04250(-0.04250)(error: 0.01000), start_dist: 0.45893(0.46306), end_dist: 0.42086(0.41838), theta: -87.954(-87.137)
#69 [33/50]ms: 16, charge_width: 0.270, line.width_m: 0.29038, line.y_diff_m: -0.03749, start_dist: 0.42807(206, 226), end_dist: 0.44070(209, 168), theta: 87.039
#69 ms: 16, charge_width: 0.270, line.width_m: 0.27004(0.29038), line.y_diff_m: -0.03749(-0.03749)(error: 0.01000), start_dist: 0.42086(0.42807), end_dist: 0.44119(0.44070), theta: 88.939(87.039)
#70 [41/52]ms: 16, charge_width: 0.270, line.width_m: 0.29068, line.y_diff_m: -0.03749, start_dist: 0.43292(205, 226), end_dist: 0.44070(209, 168), theta: 86.054
#70 ms: 16, charge_width: 0.270, line.width_m: 0.27004(0.29068), line.y_diff_m: -0.03749(-0.03749)(error: 0.01000), start_dist: 0.42086(0.43292), end_dist: 0.44119(0.44070), theta: 88.939(86.054)
#71 [39/58]ms: 15, charge_width: 0.270, line.width_m: 0.31000, line.y_diff_m: -0.03749, start_dist: 0.43063(208, 233), end_dist: 0.45853(208, 171), theta: 90.000
#71 ms: 15, charge_width: 0.270, line.width_m: 0.29004(0.31000), line.y_diff_m: -0.03749(-0.03749)(error: 0.01000), start_dist: 0.43777(0.43063), end_dist: 0.45893(0.45853), theta: 89.012(90.000)
#72 [35/58]ms: 18, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.45400(209, 164), end_dist: 0.41484(211, 225), theta: -88.122
#72 [41/58]ms: 18, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.45853(208, 164), end_dist: 0.41967(210, 225), theta: -88.122
#72 [57/58]ms: 18, charge_width: 0.270, line.width_m: 0.30565, line.y_diff_m: -0.04000, start_dist: 0.47214(205, 164), end_dist: 0.42449(209, 225), theta: -86.248
#72 ms: 18, charge_width: 0.270, line.width_m: 0.28517(0.30565), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.47214), end_dist: 0.42201(0.42449), theta: -87.990(-86.248)
#73 [32/49]ms: 15, charge_width: 0.270, line.width_m: 0.29038, line.y_diff_m: -0.04749, start_dist: 0.45400(209, 171), end_dist: 0.40626(212, 229), theta: -87.039
#73 [34/49]ms: 15, charge_width: 0.270, line.width_m: 0.30504, line.y_diff_m: -0.04000, start_dist: 0.45853(208, 171), end_dist: 0.42449(209, 232), theta: -89.060
#73 ms: 15, charge_width: 0.270, line.width_m: 0.28517(0.30504), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.45853), end_dist: 0.42201(0.42449), theta: -87.990(-89.060)
#74 [35/52]ms: 14, charge_width: 0.270, line.width_m: 0.30565, line.y_diff_m: -0.04000, start_dist: 0.46306(207, 166), end_dist: 0.41484(211, 227), theta: -86.248
#74 ms: 14, charge_width: 0.270, line.width_m: 0.28517(0.30565), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.46306), end_dist: 0.42201(0.41484), theta: -87.990(-86.248)
#75 [29/52]ms: 14, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.45853(208, 164), end_dist: 0.41967(210, 225), theta: -88.122
#75 [37/52]ms: 14, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.46306(207, 164), end_dist: 0.42449(209, 225), theta: -88.122
#75 ms: 14, charge_width: 0.270, line.width_m: 0.28517(0.30516), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.46306), end_dist: 0.42201(0.42449), theta: -87.990(-88.122)
#76 [41/54]ms: 14, charge_width: 0.270, line.width_m: 0.30536, line.y_diff_m: -0.04000, start_dist: 0.45853(205, 163), end_dist: 0.41484(208, 224), theta: -87.184
#76 ms: 14, charge_width: 0.270, line.width_m: 0.28517(0.30536), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.45853), end_dist: 0.42201(0.41484), theta: -87.990(-87.184)
#77 [35/58]ms: 20, charge_width: 0.270, line.width_m: 0.29004, line.y_diff_m: -0.03250, start_dist: 0.42933(208, 225), end_dist: 0.44777(209, 167), theta: 89.012
#77 [39/58]ms: 20, charge_width: 0.270, line.width_m: 0.29499, line.y_diff_m: -0.04499, start_dist: 0.42687(208, 223), end_dist: 0.45853(208, 164), theta: 90.000
#77 ms: 20, charge_width: 0.270, line.width_m: 0.28517(0.29499), line.y_diff_m: -0.04000(-0.04499)(error: 0.01000), start_dist: 0.42201(0.42687), end_dist: 0.45893(0.45853), theta: -87.990(90.000)
#78 [29/51]ms: 14, charge_width: 0.270, line.width_m: 0.30066, line.y_diff_m: -0.04250, start_dist: 0.46306(207, 167), end_dist: 0.41355(211, 227), theta: -86.185
#78 ms: 14, charge_width: 0.270, line.width_m: 0.28017(0.30066), line.y_diff_m: -0.04250(-0.04250)(error: 0.01000), start_dist: 0.45893(0.46306), end_dist: 0.42086(0.41355), theta: -87.954(-86.185)
#79 [42/54]ms: 15, charge_width: 0.270, line.width_m: 0.29068, line.y_diff_m: -0.03749, start_dist: 0.46351(206, 171), end_dist: 0.41838(210, 229), theta: -86.054
#79 ms: 15, charge_width: 0.270, line.width_m: 0.27004(0.29068), line.y_diff_m: -0.03749(-0.03749)(error: 0.01000), start_dist: 0.45499(0.46351), end_dist: 0.42573(0.41838), theta: -88.939(-86.054)
#80 [36/53]ms: 24, charge_width: 0.270, line.width_m: 0.29516, line.y_diff_m: -0.03500, start_dist: 0.44980(207, 173), end_dist: 0.41484(209, 232), theta: -88.058
#80 ms: 24, charge_width: 0.270, line.width_m: 0.27504(0.29516), line.y_diff_m: -0.03500(-0.03500)(error: 0.01000), start_dist: 0.44119(0.44980), end_dist: 0.42201(0.41484), theta: 88.958(-88.058)
#81 [35/57]ms: 15, charge_width: 0.270, line.width_m: 0.30004, line.y_diff_m: -0.03749, start_dist: 0.42933(208, 229), end_dist: 0.45188(209, 169), theta: 89.045
#81 ms: 15, charge_width: 0.270, line.width_m: 0.28517(0.30004), line.y_diff_m: -0.04000(-0.03749)(error: 0.01000), start_dist: 0.42201(0.42933), end_dist: 0.45893(0.45188), theta: -87.990(89.045)
#82 [34/53]ms: 14, charge_width: 0.270, line.width_m: 0.30000, line.y_diff_m: -0.04250, start_dist: 0.42323(209, 224), end_dist: 0.45400(209, 164), theta: 90.000
#82 [45/53]ms: 14, charge_width: 0.270, line.width_m: 0.30000, line.y_diff_m: -0.04250, start_dist: 0.42807(208, 224), end_dist: 0.45853(208, 164), theta: 90.000
#82 ms: 14, charge_width: 0.270, line.width_m: 0.28004(0.30000), line.y_diff_m: -0.04250(-0.04250)(error: 0.01000), start_dist: 0.42573(0.42807), end_dist: 0.45893(0.45853), theta: -88.976(90.000)
#83 [28/57]ms: 14, charge_width: 0.270, line.width_m: 0.30536, line.y_diff_m: -0.04000, start_dist: 0.46306(205, 166), end_dist: 0.41967(208, 227), theta: -87.184
#83 ms: 14, charge_width: 0.270, line.width_m: 0.28517(0.30536), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.46306), end_dist: 0.42201(0.41967), theta: -87.990(-87.184)
#84 [36/51]ms: 17, charge_width: 0.270, line.width_m: 0.29567, line.y_diff_m: -0.04499, start_dist: 0.45853(206, 168), end_dist: 0.40746(210, 227), theta: -86.121
#84 [46/51]ms: 17, charge_width: 0.270, line.width_m: 0.30536, line.y_diff_m: -0.04000, start_dist: 0.46306(205, 168), end_dist: 0.41967(208, 229), theta: -87.184
#84 [49/51]ms: 17, charge_width: 0.270, line.width_m: 0.30536, line.y_diff_m: -0.04000, start_dist: 0.46760(204, 168), end_dist: 0.42449(207, 229), theta: -87.184
#84 ms: 17, charge_width: 0.270, line.width_m: 0.28517(0.30536), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.46760), end_dist: 0.42201(0.42449), theta: -87.990(-87.184)
#85 [40/59]ms: 14, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.46306(207, 163), end_dist: 0.42449(209, 224), theta: -88.122
#85 [42/59]ms: 14, charge_width: 0.270, line.width_m: 0.30536, line.y_diff_m: -0.04000, start_dist: 0.45853(208, 163), end_dist: 0.41484(211, 224), theta: -87.184
#85 ms: 14, charge_width: 0.270, line.width_m: 0.28517(0.30536), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.45853), end_dist: 0.42201(0.41484), theta: -87.990(-87.184)
#86 [35/52]ms: 15, charge_width: 0.270, line.width_m: 0.29017, line.y_diff_m: -0.03250, start_dist: 0.42449(209, 229), end_dist: 0.43863(211, 171), theta: 88.025
#86 [41/52]ms: 15, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.42933(208, 229), end_dist: 0.45853(208, 168), theta: 90.000
#86 ms: 15, charge_width: 0.270, line.width_m: 0.28517(0.30500), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.42201(0.42933), end_dist: 0.45893(0.45853), theta: -87.990(90.000)
#87 [50/62]ms: 14, charge_width: 0.270, line.width_m: 0.29538, line.y_diff_m: -0.03500, start_dist: 0.42933(208, 228), end_dist: 0.44070(211, 169), theta: 87.089
#87 ms: 14, charge_width: 0.270, line.width_m: 0.27504(0.29538), line.y_diff_m: -0.03500(-0.03500)(error: 0.01000), start_dist: 0.42201(0.42933), end_dist: 0.44119(0.44070), theta: 88.958(87.089)
#88 [26/60]ms: 15, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.42449(209, 231), end_dist: 0.45400(209, 170), theta: 90.000
#88 [40/60]ms: 15, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.42933(208, 231), end_dist: 0.45853(208, 170), theta: 90.000
#88 ms: 15, charge_width: 0.270, line.width_m: 0.28517(0.30500), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.42201(0.42933), end_dist: 0.45893(0.45853), theta: -87.990(90.000)
#89 [37/54]ms: 15, charge_width: 0.270, line.width_m: 0.30504, line.y_diff_m: -0.04000, start_dist: 0.42933(208, 229), end_dist: 0.45400(209, 168), theta: 89.060
#89 [40/54]ms: 15, charge_width: 0.270, line.width_m: 0.30536, line.y_diff_m: -0.04000, start_dist: 0.46760(206, 168), end_dist: 0.42449(209, 229), theta: -87.184
#89 ms: 15, charge_width: 0.270, line.width_m: 0.28517(0.30536), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.46760), end_dist: 0.42201(0.42449), theta: -87.990(-87.184)
#90 [25/45]ms: 14, charge_width: 0.270, line.width_m: 0.30504, line.y_diff_m: -0.04000, start_dist: 0.45400(207, 167), end_dist: 0.41967(208, 228), theta: -89.060
#90 [38/45]ms: 14, charge_width: 0.270, line.width_m: 0.30504, line.y_diff_m: -0.04000, start_dist: 0.45853(206, 167), end_dist: 0.42449(207, 228), theta: -89.060
#90 ms: 14, charge_width: 0.270, line.width_m: 0.28517(0.30504), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.45853), end_dist: 0.42201(0.42449), theta: -87.990(-89.060)
#91 [35/61]ms: 14, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.42933(206, 229), end_dist: 0.45853(206, 168), theta: 90.000
#91 [43/61]ms: 14, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.42449(207, 229), end_dist: 0.45400(207, 168), theta: 90.000
#91 ms: 14, charge_width: 0.270, line.width_m: 0.28517(0.30500), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.42201(0.42449), end_dist: 0.45893(0.45400), theta: -87.990(90.000)
#92 [32/55]ms: 15, charge_width: 0.270, line.width_m: 0.29004, line.y_diff_m: -0.03250, start_dist: 0.44319(210, 171), end_dist: 0.41484(211, 229), theta: -89.012
#92 [38/55]ms: 15, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.42933(208, 229), end_dist: 0.45853(208, 168), theta: 90.000
#92 [47/55]ms: 15, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.42449(209, 229), end_dist: 0.45400(209, 168), theta: 90.000
#92 ms: 15, charge_width: 0.270, line.width_m: 0.28517(0.30500), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.42201(0.42449), end_dist: 0.45893(0.45400), theta: -87.990(90.000)
#93 [53/58]ms: 14, charge_width: 0.270, line.width_m: 0.30536, line.y_diff_m: -0.04000, start_dist: 0.45853(208, 163), end_dist: 0.41484(211, 224), theta: -87.184
#93 [54/58]ms: 14, charge_width: 0.270, line.width_m: 0.30565, line.y_diff_m: -0.04000, start_dist: 0.46760(206, 163), end_dist: 0.41967(210, 224), theta: -86.248
#93 ms: 14, charge_width: 0.270, line.width_m: 0.28517(0.30565), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.46760), end_dist: 0.42201(0.41967), theta: -87.990(-86.248)
#94 [31/50]ms: 14, charge_width: 0.270, line.width_m: 0.30565, line.y_diff_m: -0.04000, start_dist: 0.46760(206, 168), end_dist: 0.41967(210, 229), theta: -86.248
#94 ms: 14, charge_width: 0.270, line.width_m: 0.27613(0.30565), line.y_diff_m: -0.04499(-0.04000)(error: 0.01000), start_dist: 0.45893(0.46760), end_dist: 0.40512(0.41967), theta: -84.805(-86.248)
#95 [25/48]ms: 14, charge_width: 0.270, line.width_m: 0.29068, line.y_diff_m: -0.04749, start_dist: 0.45853(208, 168), end_dist: 0.40626(212, 226), theta: -86.054
#95 [31/48]ms: 14, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.46306(207, 168), end_dist: 0.42449(209, 229), theta: -88.122
#95 ms: 14, charge_width: 0.270, line.width_m: 0.28517(0.30516), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.46306), end_dist: 0.42201(0.42449), theta: -87.990(-88.122)
#96 [30/54]ms: 14, charge_width: 0.270, line.width_m: 0.29068, line.y_diff_m: -0.04749, start_dist: 0.45853(206, 166), end_dist: 0.40626(210, 224), theta: -86.054
#96 [45/54]ms: 14, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.46306(205, 166), end_dist: 0.42449(207, 227), theta: -88.122
#96 ms: 14, charge_width: 0.270, line.width_m: 0.28517(0.30516), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.46306), end_dist: 0.42201(0.42449), theta: -87.990(-88.122)
27203 y0_2th_forward, line(width_m: 0.28291, y_diff_m: -0.05125, theta: -87.194(to_vert: 2.805)), dist: 0.41779 ==> vel: (0.07989, 0, 10.000)
#97 [35/49]ms: 14, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.42449(209, 227), end_dist: 0.45400(209, 166), theta: 90.000
#97 [45/49]ms: 14, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.42933(208, 227), end_dist: 0.45853(208, 166), theta: 90.000
#97 ms: 14, charge_width: 0.270, line.width_m: 0.28517(0.30500), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.42201(0.42933), end_dist: 0.45893(0.45853), theta: -87.990(90.000)
#98 [21/55]ms: 14, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.42449(207, 231), end_dist: 0.45400(207, 170), theta: 90.000
#98 [42/55]ms: 14, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.42933(206, 231), end_dist: 0.45853(206, 170), theta: 90.000
#98 [53/55]ms: 14, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.43416(205, 231), end_dist: 0.46306(205, 170), theta: 90.000
#98 ms: 14, charge_width: 0.270, line.width_m: 0.28517(0.30500), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.42201(0.43416), end_dist: 0.45893(0.46306), theta: -87.990(90.000)
#99 [30/57]ms: 14, charge_width: 0.270, line.width_m: 0.30536, line.y_diff_m: -0.04000, start_dist: 0.46306(207, 164), end_dist: 0.41967(210, 225), theta: -87.184
#99 ms: 14, charge_width: 0.270, line.width_m: 0.28517(0.30536), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.46306), end_dist: 0.42201(0.41967), theta: -87.990(-87.184)
#100 [33/44]ms: 15, charge_width: 0.270, line.width_m: 0.28539, line.y_diff_m: -0.03000, start_dist: 0.45499(207, 172), end_dist: 0.41967(210, 229), theta: -86.987
#100 ms: 15, charge_width: 0.270, line.width_m: 0.26500(0.28539), line.y_diff_m: -0.03000(-0.03000)(error: 0.01000), start_dist: 0.44195(0.45499), end_dist: 0.42201(0.41967), theta: 90.000(-86.987)
#101 [29/55]ms: 18, charge_width: 0.270, line.width_m: 0.30536, line.y_diff_m: -0.04000, start_dist: 0.46760(204, 169), end_dist: 0.42449(207, 230), theta: -87.184
#101 ms: 18, charge_width: 0.270, line.width_m: 0.28517(0.30536), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.46760), end_dist: 0.42201(0.42449), theta: -87.990(-87.184)
#102 [35/50]ms: 15, charge_width: 0.270, line.width_m: 0.29017, line.y_diff_m: -0.03749, start_dist: 0.45436(206, 168), end_dist: 0.41838(208, 226), theta: -88.025
#102 ms: 15, charge_width: 0.270, line.width_m: 0.26004(0.29017), line.y_diff_m: -0.04250(-0.03749)(error: 0.01000), start_dist: 0.44119(0.45436), end_dist: 0.40893(0.41838), theta: -88.898(-88.025)
#103 [40/54]ms: 16, charge_width: 0.270, line.width_m: 0.28714, line.y_diff_m: -0.05000, start_dist: 0.47214(203, 166), end_dist: 0.40512(210, 223), theta: -82.998
#103 ms: 16, charge_width: 0.270, line.width_m: 0.26575(0.28714), line.y_diff_m: -0.05000(-0.05000)(error: 0.01000), start_dist: 0.45893(0.47214), end_dist: 0.40792(0.40512), theta: -85.683(-82.998)
#104 [39/52]ms: 15, charge_width: 0.270, line.width_m: 0.31000, line.y_diff_m: -0.03749, start_dist: 0.43063(206, 231), end_dist: 0.45853(206, 169), theta: 90.000
#104 [50/52]ms: 15, charge_width: 0.270, line.width_m: 0.31000, line.y_diff_m: -0.03749, start_dist: 0.43545(205, 231), end_dist: 0.46306(205, 169), theta: 90.000
#104 ms: 15, charge_width: 0.270, line.width_m: 0.29004(0.31000), line.y_diff_m: -0.03749(-0.03749)(error: 0.01000), start_dist: 0.43777(0.43545), end_dist: 0.45893(0.46306), theta: 89.012(90.000)
#105 [41/52]ms: 16, charge_width: 0.270, line.width_m: 0.28539, line.y_diff_m: -0.03000, start_dist: 0.45499(205, 172), end_dist: 0.41967(208, 229), theta: -86.987
#105 ms: 16, charge_width: 0.270, line.width_m: 0.26500(0.28539), line.y_diff_m: -0.03000(-0.03000)(error: 0.01000), start_dist: 0.44195(0.45499), end_dist: 0.42201(0.41967), theta: 90.000(-86.987)
#106 [35/59]ms: 14, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.45853(208, 164), end_dist: 0.41967(210, 225), theta: -88.122
#106 [43/59]ms: 14, charge_width: 0.270, line.width_m: 0.30536, line.y_diff_m: -0.04000, start_dist: 0.46760(206, 164), end_dist: 0.42449(209, 225), theta: -87.184
#106 ms: 14, charge_width: 0.270, line.width_m: 0.28517(0.30536), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.46760), end_dist: 0.42201(0.42449), theta: -87.990(-87.184)
#107 [46/60]ms: 15, charge_width: 0.270, line.width_m: 0.30016, line.y_diff_m: -0.04250, start_dist: 0.46760(204, 166), end_dist: 0.42807(206, 226), theta: -88.090
#107 ms: 15, charge_width: 0.270, line.width_m: 0.28004(0.30016), line.y_diff_m: -0.04250(-0.04250)(error: 0.01000), start_dist: 0.45893(0.46760), end_dist: 0.42573(0.42807), theta: -88.976(-88.090)
#108 [30/49]ms: 14, charge_width: 0.270, line.width_m: 0.30504, line.y_diff_m: -0.04000, start_dist: 0.45400(207, 166), end_dist: 0.41967(208, 227), theta: -89.060
#108 ms: 14, charge_width: 0.270, line.width_m: 0.28517(0.30504), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.45400), end_dist: 0.42201(0.41967), theta: -87.990(-89.060)
#109 [32/51]ms: 16, charge_width: 0.270, line.width_m: 0.30037, line.y_diff_m: -0.04250, start_dist: 0.46306(207, 167), end_dist: 0.41838(210, 227), theta: -87.137
#109 ms: 16, charge_width: 0.270, line.width_m: 0.28004(0.30037), line.y_diff_m: -0.04250(-0.04250)(error: 0.01000), start_dist: 0.45893(0.46306), end_dist: 0.42573(0.41838), theta: -88.976(-87.137)
#111 [44/52]ms: 14, charge_width: 0.270, line.width_m: 0.30016, line.y_diff_m: -0.03250, start_dist: 0.43063(206, 231), end_dist: 0.44525(208, 171), theta: 88.090
#111 ms: 14, charge_width: 0.270, line.width_m: 0.28071(0.30016), line.y_diff_m: -0.03250(-0.03250)(error: 0.01000), start_dist: 0.43777(0.43063), end_dist: 0.44119(0.44525), theta: 85.914(88.090)
#112 [47/58]ms: 14, charge_width: 0.270, line.width_m: 0.28999, line.y_diff_m: -0.03749, start_dist: 0.41838(208, 228), end_dist: 0.44525(208, 170), theta: 90.000
#112 [48/58]ms: 14, charge_width: 0.270, line.width_m: 0.28999, line.y_diff_m: -0.03749, start_dist: 0.42323(207, 228), end_dist: 0.44980(207, 170), theta: 90.000
#112 [52/58]ms: 14, charge_width: 0.270, line.width_m: 0.28999, line.y_diff_m: -0.03749, start_dist: 0.42807(206, 228), end_dist: 0.45436(206, 170), theta: 90.000
#112 ms: 14, charge_width: 0.270, line.width_m: 0.27018(0.28999), line.y_diff_m: -0.03749(-0.03749)(error: 0.01000), start_dist: 0.42573(0.42807), end_dist: 0.44119(0.45436), theta: 87.878(90.000)
#113 [40/49]ms: 13, charge_width: 0.270, line.width_m: 0.30037, line.y_diff_m: -0.03749, start_dist: 0.46097(207, 167), end_dist: 0.41967(210, 227), theta: -87.137
#113 [48/49]ms: 13, charge_width: 0.270, line.width_m: 0.30037, line.y_diff_m: -0.03749, start_dist: 0.46553(206, 167), end_dist: 0.42449(209, 227), theta: -87.137
#113 ms: 13, charge_width: 0.270, line.width_m: 0.28017(0.30037), line.y_diff_m: -0.03749(-0.03749)(error: 0.01000), start_dist: 0.45694(0.46553), end_dist: 0.42201(0.42449), theta: -87.954(-87.137)
#114 [40/54]ms: 17, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.45853(208, 167), end_dist: 0.41967(210, 228), theta: -88.122
#114 [47/54]ms: 17, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.45400(209, 167), end_dist: 0.41484(211, 228), theta: -88.122
#114 ms: 17, charge_width: 0.270, line.width_m: 0.28517(0.30516), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.45400), end_dist: 0.42201(0.41484), theta: -87.990(-88.122)
#115 [37/50]ms: 20, charge_width: 0.270, line.width_m: 0.30504, line.y_diff_m: -0.04000, start_dist: 0.45853(208, 166), end_dist: 0.42449(209, 227), theta: -89.060
#115 [47/50]ms: 20, charge_width: 0.270, line.width_m: 0.30504, line.y_diff_m: -0.04000, start_dist: 0.46306(207, 166), end_dist: 0.42933(208, 227), theta: -89.060
#115 ms: 20, charge_width: 0.270, line.width_m: 0.28517(0.30504), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.46306), end_dist: 0.42201(0.42933), theta: -87.990(-89.060)
#116 [40/51]ms: 17, charge_width: 0.270, line.width_m: 0.30016, line.y_diff_m: -0.03749, start_dist: 0.45188(209, 162), end_dist: 0.41484(211, 222), theta: -88.090
#116 [47/51]ms: 17, charge_width: 0.270, line.width_m: 0.30016, line.y_diff_m: -0.03749, start_dist: 0.45642(208, 162), end_dist: 0.41967(210, 222), theta: -88.090
#116 ms: 17, charge_width: 0.270, line.width_m: 0.28004(0.30016), line.y_diff_m: -0.03749(-0.03749)(error: 0.01000), start_dist: 0.44319(0.45642), end_dist: 0.42201(0.41967), theta: 88.976(-88.090)
#117 [38/56]ms: 16, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.46306(207, 167), end_dist: 0.42449(209, 228), theta: -88.122
#117 ms: 16, charge_width: 0.270, line.width_m: 0.28517(0.30516), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.46306), end_dist: 0.42201(0.42449), theta: -87.990(-88.122)
#118 [36/54]ms: 17, charge_width: 0.270, line.width_m: 0.30016, line.y_diff_m: -0.04250, start_dist: 0.45853(208, 168), end_dist: 0.41838(210, 228), theta: -88.090
#118 [44/54]ms: 17, charge_width: 0.270, line.width_m: 0.30037, line.y_diff_m: -0.04250, start_dist: 0.43777(206, 228), end_dist: 0.45400(209, 168), theta: 87.137
#118 ms: 17, charge_width: 0.270, line.width_m: 0.26518(0.30037), line.y_diff_m: -0.03500(-0.04250)(error: 0.01000), start_dist: 0.42573(0.43777), end_dist: 0.43923(0.45400), theta: 87.838(87.137)
#119 [32/48]ms: 18, charge_width: 0.270, line.width_m: 0.30016, line.y_diff_m: -0.03749, start_dist: 0.42449(207, 226), end_dist: 0.44283(209, 166), theta: 88.090
#119 [37/48]ms: 18, charge_width: 0.270, line.width_m: 0.30016, line.y_diff_m: -0.03749, start_dist: 0.43416(205, 226), end_dist: 0.45188(207, 166), theta: 88.090
#119 ms: 18, charge_width: 0.270, line.width_m: 0.28004(0.30016), line.y_diff_m: -0.03749(-0.03749)(error: 0.01000), start_dist: 0.42201(0.43416), end_dist: 0.44319(0.45188), theta: 88.976(88.090)
#120 [44/49]ms: 16, charge_width: 0.270, line.width_m: 0.28999, line.y_diff_m: -0.03749, start_dist: 0.41838(210, 226), end_dist: 0.44525(210, 168), theta: 90.000
#120 [45/49]ms: 16, charge_width: 0.270, line.width_m: 0.29004, line.y_diff_m: -0.03749, start_dist: 0.42807(208, 226), end_dist: 0.44980(209, 168), theta: 89.012
#120 ms: 16, charge_width: 0.270, line.width_m: 0.27018(0.29004), line.y_diff_m: -0.03749(-0.03749)(error: 0.01000), start_dist: 0.42573(0.42807), end_dist: 0.44119(0.44980), theta: 87.878(89.012)
#121 [28/51]ms: 15, charge_width: 0.270, line.width_m: 0.30000, line.y_diff_m: -0.03749, start_dist: 0.42449(209, 226), end_dist: 0.45188(209, 166), theta: 90.000
#121 [44/51]ms: 15, charge_width: 0.270, line.width_m: 0.30000, line.y_diff_m: -0.03749, start_dist: 0.41967(210, 226), end_dist: 0.44735(210, 166), theta: 90.000
#121 [50/51]ms: 15, charge_width: 0.270, line.width_m: 0.30000, line.y_diff_m: -0.03749, start_dist: 0.42933(208, 226), end_dist: 0.45642(208, 166), theta: 90.000
#121 ms: 15, charge_width: 0.270, line.width_m: 0.28004(0.30000), line.y_diff_m: -0.03749(-0.03749)(error: 0.01000), start_dist: 0.42201(0.42933), end_dist: 0.44319(0.45642), theta: 88.976(90.000)
#122 [35/54]ms: 15, charge_width: 0.270, line.width_m: 0.30536, line.y_diff_m: -0.04000, start_dist: 0.45853(206, 167), end_dist: 0.41484(209, 228), theta: -87.184
#122 ms: 15, charge_width: 0.270, line.width_m: 0.28517(0.30536), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.45853), end_dist: 0.42201(0.41484), theta: -87.990(-87.184)
#123 [45/56]ms: 16, charge_width: 0.270, line.width_m: 0.29504, line.y_diff_m: -0.04000, start_dist: 0.42807(206, 228), end_dist: 0.45188(207, 169), theta: 89.029
#123 [53/56]ms: 16, charge_width: 0.270, line.width_m: 0.29504, line.y_diff_m: -0.04000, start_dist: 0.45642(206, 169), end_dist: 0.42323(207, 228), theta: -89.029
#123 ms: 16, charge_width: 0.270, line.width_m: 0.27518(0.29504), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.44319(0.45642), end_dist: 0.42573(0.42323), theta: 87.917(-89.029)
#124 [30/51]ms: 18, charge_width: 0.270, line.width_m: 0.29038, line.y_diff_m: -0.03250, start_dist: 0.45235(208, 168), end_dist: 0.41484(211, 226), theta: -87.039
#124 ms: 18, charge_width: 0.270, line.width_m: 0.26500(0.29038), line.y_diff_m: -0.03000(-0.03250)(error: 0.01000), start_dist: 0.44195(0.45235), end_dist: 0.42201(0.41484), theta: 90.000(-87.039)
#125 [38/51]ms: 20, charge_width: 0.270, line.width_m: 0.30004, line.y_diff_m: -0.04250, start_dist: 0.45853(208, 166), end_dist: 0.42323(209, 226), theta: -89.045
#125 [39/51]ms: 20, charge_width: 0.270, line.width_m: 0.30004, line.y_diff_m: -0.04250, start_dist: 0.45400(209, 166), end_dist: 0.41838(210, 226), theta: -89.045
#125 ms: 20, charge_width: 0.270, line.width_m: 0.28004(0.30004), line.y_diff_m: -0.04250(-0.04250)(error: 0.01000), start_dist: 0.45893(0.45400), end_dist: 0.42573(0.41838), theta: -88.976(-89.045)
#127 [51/55]ms: 16, charge_width: 0.270, line.width_m: 0.29605, line.y_diff_m: -0.04499, start_dist: 0.45853(206, 169), end_dist: 0.40261(211, 228), theta: -85.156
#127 ms: 16, charge_width: 0.270, line.width_m: 0.27572(0.29605), line.y_diff_m: -0.04499(-0.04499)(error: 0.01000), start_dist: 0.45893(0.45853), end_dist: 0.40999(0.40261), theta: -85.840(-85.156)
30307 y0_1th_largetheta, line(width_m: 0.27398, y_diff_m: -0.04937, theta: 0.775(to_vert: 89.224)), dist: 0.42087 ==> vel: (0.01000, 0, -90.000)
#128 [36/54]ms: 15, charge_width: 0.270, line.width_m: 0.30504, line.y_diff_m: -0.04000, start_dist: 0.46306(207, 167), end_dist: 0.42933(208, 228), theta: -89.060
#128 [40/54]ms: 15, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.45853(208, 167), end_dist: 0.41967(210, 228), theta: -88.122
#128 [51/54]ms: 15, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.45400(209, 167), end_dist: 0.41484(211, 228), theta: -88.122
#128 ms: 15, charge_width: 0.270, line.width_m: 0.28517(0.30516), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.45400), end_dist: 0.42201(0.41484), theta: -87.990(-88.122)
#129 [29/56]ms: 14, charge_width: 0.270, line.width_m: 0.29068, line.y_diff_m: -0.04749, start_dist: 0.45853(206, 166), end_dist: 0.40626(210, 224), theta: -86.054
#129 [49/56]ms: 14, charge_width: 0.270, line.width_m: 0.30037, line.y_diff_m: -0.04250, start_dist: 0.46306(205, 166), end_dist: 0.41838(208, 226), theta: -87.137
#129 ms: 14, charge_width: 0.270, line.width_m: 0.27073(0.30037), line.y_diff_m: -0.04749(-0.04250)(error: 0.01000), start_dist: 0.45893(0.46306), end_dist: 0.40893(0.41838), theta: -85.763(-87.137)
#130 [44/55]ms: 15, charge_width: 0.270, line.width_m: 0.28999, line.y_diff_m: -0.03250, start_dist: 0.41967(210, 226), end_dist: 0.44319(210, 168), theta: 90.000
#130 [54/55]ms: 15, charge_width: 0.270, line.width_m: 0.28999, line.y_diff_m: -0.03250, start_dist: 0.42449(209, 226), end_dist: 0.44777(209, 168), theta: 90.000
#130 ms: 15, charge_width: 0.270, line.width_m: 0.27004(0.28999), line.y_diff_m: -0.03250(-0.03250)(error: 0.01000), start_dist: 0.42201(0.42449), end_dist: 0.43923(0.44777), theta: 88.939(90.000)
#131 [34/46]ms: 15, charge_width: 0.270, line.width_m: 0.29538, line.y_diff_m: -0.04000, start_dist: 0.43292(207, 225), end_dist: 0.44735(210, 166), theta: 87.089
#131 ms: 15, charge_width: 0.270, line.width_m: 0.27518(0.29538), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.42573(0.43292), end_dist: 0.44319(0.44735), theta: 87.917(87.089)
#132 [50/61]ms: 15, charge_width: 0.270, line.width_m: 0.30536, line.y_diff_m: -0.04000, start_dist: 0.45853(206, 165), end_dist: 0.41484(209, 226), theta: -87.184
#132 ms: 15, charge_width: 0.270, line.width_m: 0.28517(0.30536), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.45853), end_dist: 0.42201(0.41484), theta: -87.990(-87.184)
#133 [37/50]ms: 15, charge_width: 0.270, line.width_m: 0.30565, line.y_diff_m: -0.04000, start_dist: 0.46306(207, 168), end_dist: 0.41484(211, 229), theta: -86.248
#133 ms: 15, charge_width: 0.270, line.width_m: 0.28517(0.30565), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.46306), end_dist: 0.42201(0.41484), theta: -87.990(-86.248)
#134 [50/60]ms: 15, charge_width: 0.270, line.width_m: 0.29038, line.y_diff_m: -0.03749, start_dist: 0.42323(207, 226), end_dist: 0.43617(210, 168), theta: 87.039
#134 [52/60]ms: 15, charge_width: 0.270, line.width_m: 0.29038, line.y_diff_m: -0.03749, start_dist: 0.42807(206, 226), end_dist: 0.44070(209, 168), theta: 87.039
#134 ms: 15, charge_width: 0.270, line.width_m: 0.27018(0.29038), line.y_diff_m: -0.03749(-0.03749)(error: 0.01000), start_dist: 0.42573(0.42807), end_dist: 0.44119(0.44070), theta: 87.878(87.039)
#136 [31/49]ms: 23, charge_width: 0.270, line.width_m: 0.29516, line.y_diff_m: -0.03500, start_dist: 0.44980(209, 170), end_dist: 0.41484(211, 229), theta: -88.058
#136 ms: 23, charge_width: 0.270, line.width_m: 0.27504(0.29516), line.y_diff_m: -0.03500(-0.03500)(error: 0.01000), start_dist: 0.44119(0.44980), end_dist: 0.42201(0.41484), theta: 88.958(-88.058)
#137 [32/48]ms: 14, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.45853(208, 168), end_dist: 0.41967(210, 229), theta: -88.122
#137 [44/48]ms: 14, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.45400(209, 168), end_dist: 0.41484(211, 229), theta: -88.122
#137 [45/48]ms: 14, charge_width: 0.270, line.width_m: 0.30536, line.y_diff_m: -0.04000, start_dist: 0.46760(206, 168), end_dist: 0.42449(209, 229), theta: -87.184
#137 ms: 14, charge_width: 0.270, line.width_m: 0.28517(0.30536), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.46760), end_dist: 0.42201(0.42449), theta: -87.990(-87.184)
#138 [37/49]ms: 14, charge_width: 0.270, line.width_m: 0.29516, line.y_diff_m: -0.03500, start_dist: 0.44980(209, 168), end_dist: 0.41484(211, 227), theta: -88.058
#138 ms: 14, charge_width: 0.270, line.width_m: 0.27504(0.29516), line.y_diff_m: -0.03500(-0.03500)(error: 0.01000), start_dist: 0.44119(0.44980), end_dist: 0.42201(0.41484), theta: 88.958(-88.058)
#139 [30/51]ms: 15, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.42449(209, 225), end_dist: 0.45400(209, 164), theta: 90.000
#139 [47/51]ms: 15, charge_width: 0.270, line.width_m: 0.30500, line.y_diff_m: -0.04000, start_dist: 0.42933(208, 225), end_dist: 0.45853(208, 164), theta: 90.000
#139 ms: 15, charge_width: 0.270, line.width_m: 0.28517(0.30500), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.42201(0.42933), end_dist: 0.45893(0.45853), theta: -87.990(90.000)
#140 [45/51]ms: 19, charge_width: 0.270, line.width_m: 0.29499, line.y_diff_m: -0.03500, start_dist: 0.42933(206, 225), end_dist: 0.45436(206, 166), theta: 90.000
#140 [47/51]ms: 19, charge_width: 0.270, line.width_m: 0.29516, line.y_diff_m: -0.03500, start_dist: 0.44980(207, 166), end_dist: 0.41484(209, 225), theta: -88.058
#140 ms: 19, charge_width: 0.270, line.width_m: 0.27504(0.29516), line.y_diff_m: -0.03500(-0.03500)(error: 0.01000), start_dist: 0.44119(0.44980), end_dist: 0.42201(0.41484), theta: 88.958(-88.058)
#141 [44/56]ms: 16, charge_width: 0.270, line.width_m: 0.30037, line.y_diff_m: -0.04250, start_dist: 0.46760(206, 165), end_dist: 0.42323(209, 225), theta: -87.137
#141 ms: 16, charge_width: 0.270, line.width_m: 0.27073(0.30037), line.y_diff_m: -0.04749(-0.04250)(error: 0.01000), start_dist: 0.45893(0.46760), end_dist: 0.40893(0.42323), theta: -85.763(-87.137)
#142 [43/50]ms: 15, charge_width: 0.270, line.width_m: 0.28499, line.y_diff_m: -0.03500, start_dist: 0.41838(210, 229), end_dist: 0.44319(210, 172), theta: 90.000
#142 [44/50]ms: 15, charge_width: 0.270, line.width_m: 0.28499, line.y_diff_m: -0.03500, start_dist: 0.42323(209, 229), end_dist: 0.44777(209, 172), theta: 90.000
#142 [47/50]ms: 15, charge_width: 0.270, line.width_m: 0.28499, line.y_diff_m: -0.03500, start_dist: 0.42807(208, 229), end_dist: 0.45235(208, 172), theta: 90.000
#142 ms: 15, charge_width: 0.270, line.width_m: 0.26518(0.28499), line.y_diff_m: -0.03500(-0.03500)(error: 0.01000), start_dist: 0.42573(0.42807), end_dist: 0.43923(0.45235), theta: 87.838(90.000)
#143 [33/52]ms: 15, charge_width: 0.270, line.width_m: 0.29004, line.y_diff_m: -0.03250, start_dist: 0.45235(208, 171), end_dist: 0.42449(209, 229), theta: -89.012
#143 [45/52]ms: 15, charge_width: 0.270, line.width_m: 0.29004, line.y_diff_m: -0.03250, start_dist: 0.44777(209, 171), end_dist: 0.41967(210, 229), theta: -89.012
#143 ms: 15, charge_width: 0.270, line.width_m: 0.26500(0.29004), line.y_diff_m: -0.03000(-0.03250)(error: 0.01000), start_dist: 0.44195(0.44777), end_dist: 0.42201(0.41967), theta: 90.000(-89.012)
#144 [39/54]ms: 19, charge_width: 0.270, line.width_m: 0.29038, line.y_diff_m: -0.04749, start_dist: 0.45400(209, 166), end_dist: 0.40626(212, 224), theta: -87.039
#144 [45/54]ms: 19, charge_width: 0.270, line.width_m: 0.31000, line.y_diff_m: -0.03749, start_dist: 0.43063(208, 228), end_dist: 0.45853(208, 166), theta: 90.000
#144 ms: 19, charge_width: 0.270, line.width_m: 0.29004(0.31000), line.y_diff_m: -0.03749(-0.03749)(error: 0.01000), start_dist: 0.43777(0.43063), end_dist: 0.45893(0.45853), theta: 89.012(90.000)
#145 [28/50]ms: 17, charge_width: 0.270, line.width_m: 0.28539, line.y_diff_m: -0.03000, start_dist: 0.45038(208, 166), end_dist: 0.41484(211, 223), theta: -86.987
#145 [42/50]ms: 17, charge_width: 0.270, line.width_m: 0.28539, line.y_diff_m: -0.04499, start_dist: 0.45188(209, 163), end_dist: 0.40626(212, 220), theta: -86.987
#145 ms: 17, charge_width: 0.270, line.width_m: 0.26504(0.28539), line.y_diff_m: -0.04499(-0.04499)(error: 0.01000), start_dist: 0.44319(0.45188), end_dist: 0.40893(0.40626), theta: -88.919(-86.987)
32155 {dbg_camera}twindow::~twindow()[1/2] id: launcher__center
32160 {dbg_camera}twindow::~twindow()[2/2] id: launcher__center
32163 {dbg_scroll}twindow::invalidate_layout(widget: nullptr)
#146 [33/60]ms: 24, charge_width: 0.270, line.width_m: 0.30516, line.y_diff_m: -0.04000, start_dist: 0.45853(208, 167), end_dist: 0.41967(210, 228), theta: -88.122
#146 ms: 24, charge_width: 0.270, line.width_m: 0.28517(0.30516), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.45853), end_dist: 0.42201(0.41967), theta: -87.990(-88.122)
#147 [33/55]ms: 15, charge_width: 0.270, line.width_m: 0.30016, line.y_diff_m: -0.04250, start_dist: 0.45853(206, 166), end_dist: 0.41838(208, 226), theta: -88.090
#147 ms: 15, charge_width: 0.270, line.width_m: 0.27073(0.30016), line.y_diff_m: -0.04749(-0.04250)(error: 0.01000), start_dist: 0.45893(0.45853), end_dist: 0.40893(0.41838), theta: -85.763(-88.090)
#149 [53/61]ms: 14, charge_width: 0.270, line.width_m: 0.29004, line.y_diff_m: -0.03749, start_dist: 0.45436(206, 169), end_dist: 0.42323(207, 227), theta: -89.012
#149 ms: 14, charge_width: 0.270, line.width_m: 0.26004(0.29004), line.y_diff_m: -0.04250(-0.03749)(error: 0.01000), start_dist: 0.44119(0.45436), end_dist: 0.40893(0.42323), theta: -88.898(-89.012)
#150 [38/47]ms: 14, charge_width: 0.270, line.width_m: 0.28517, line.y_diff_m: -0.03500, start_dist: 0.45235(208, 168), end_dist: 0.41838(210, 225), theta: -87.990
#150 ms: 14, charge_width: 0.270, line.width_m: 0.25019(0.28517), line.y_diff_m: -0.03749(-0.03500)(error: 0.01000), start_dist: 0.44195(0.45235), end_dist: 0.40893(0.41838), theta: -87.709(-87.990)
#151 [28/55]ms: 14, charge_width: 0.270, line.width_m: 0.29567, line.y_diff_m: -0.03500, start_dist: 0.43416(205, 229), end_dist: 0.44070(209, 170), theta: 86.121
#151 ms: 14, charge_width: 0.270, line.width_m: 0.27504(0.29567), line.y_diff_m: -0.03500(-0.03500)(error: 0.01000), start_dist: 0.42201(0.43416), end_dist: 0.44119(0.44070), theta: 88.958(86.121)
#152 [31/51]ms: 18, charge_width: 0.270, line.width_m: 0.30000, line.y_diff_m: -0.04250, start_dist: 0.42323(209, 224), end_dist: 0.45400(209, 164), theta: 90.000
#152 [45/51]ms: 18, charge_width: 0.270, line.width_m: 0.30000, line.y_diff_m: -0.04250, start_dist: 0.42807(208, 224), end_dist: 0.45853(208, 164), theta: 90.000
#152 ms: 18, charge_width: 0.270, line.width_m: 0.28004(0.30000), line.y_diff_m: -0.04250(-0.04250)(error: 0.01000), start_dist: 0.42573(0.42807), end_dist: 0.45893(0.45853), theta: -88.976(90.000)
#153 [28/56]ms: 16, charge_width: 0.270, line.width_m: 0.30536, line.y_diff_m: -0.04000, start_dist: 0.46306(207, 169), end_dist: 0.41967(210, 230), theta: -87.184
#153 ms: 16, charge_width: 0.270, line.width_m: 0.28517(0.30536), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.45893(0.46306), end_dist: 0.42201(0.41967), theta: -87.990(-87.184)
#154 [37/54]ms: 19, charge_width: 0.270, line.width_m: 0.30000, line.y_diff_m: -0.03749, start_dist: 0.42449(207, 226), end_dist: 0.45188(207, 166), theta: 90.000
#154 [38/54]ms: 19, charge_width: 0.270, line.width_m: 0.30000, line.y_diff_m: -0.03749, start_dist: 0.42933(206, 226), end_dist: 0.45642(206, 166), theta: 90.000
#154 [44/54]ms: 19, charge_width: 0.270, line.width_m: 0.30000, line.y_diff_m: -0.03749, start_dist: 0.41967(208, 226), end_dist: 0.44735(208, 166), theta: 90.000
#154 [53/54]ms: 19, charge_width: 0.270, line.width_m: 0.30000, line.y_diff_m: -0.03749, start_dist: 0.41484(209, 226), end_dist: 0.44283(209, 166), theta: 90.000
#154 ms: 19, charge_width: 0.270, line.width_m: 0.28004(0.30000), line.y_diff_m: -0.03749(-0.03749)(error: 0.01000), start_dist: 0.42201(0.41484), end_dist: 0.44319(0.44283), theta: 88.976(90.000)
#155 [33/52]ms: 15, charge_width: 0.270, line.width_m: 0.29516, line.y_diff_m: -0.04000, start_dist: 0.45642(208, 164), end_dist: 0.41838(210, 223), theta: -88.058
#155 ms: 15, charge_width: 0.270, line.width_m: 0.27518(0.29516), line.y_diff_m: -0.04000(-0.04000)(error: 0.01000), start_dist: 0.44319(0.45642), end_dist: 0.42573(0.41838), theta: 87.917(-88.058)
33122 {dbg_camera}twindow::~twindow()[1/2] id: launcher__home
33123 {dbg_camera}twindow::~twindow()[2/2] id: launcher__home
{dbg_timingpb}write_sha1pb, filepath(C:/Users/ancientcc/我的文档/RoseApp/launcher/logs.pb) --> fsize(22332)
{dbg_timingpb}write_sha1pb, filepath2(C:/Users/ancientcc/我的文档/RoseApp/launcher/logs.pb.bak) --> fsize(22514)
ttiming::stop_aplt_task(fail_retry: false)
{dbg_timingpb}write_sha1pb, filepath(C:/Users/ancientcc/我的文档/RoseApp/launcher/logs.pb) --> fsize(22398)
{dbg_timingpb}write_sha1pb, filepath2(C:/Users/ancientcc/我的文档/RoseApp/launcher/logs.pb.bak) --> fsize(22514)
finished aplt.leagor.basic(studio)-charge, result: ok
call trunning.clear
{dbg_timingpb}write_sha1pb, filepath(C:/Users/ancientcc/我的文档/RoseApp/launcher/logs.pb) --> fsize(22467)
{dbg_timingpb}write_sha1pb, filepath2(C:/Users/ancientcc/我的文档/RoseApp/launcher/logs.pb.bak) --> fsize(22514)
{tros_instance::erase_task} called
33134 [stop_node]pre base_footprint_2_laser_.reset()
The thread 'base_footprint_2_laser' (20216) has exited with code 0 (0x0).
33171 [stop_node]pre base_footprint_2_camera_.reset()
33172 [stop_node]pre stop_laser()
The thread 'laser_driver_node' (15796) has exited with code 0 (0x0).
33185 [stop_node]pre cartographer_occupancy_grid_node_.reset()
33185 [stop_node]pre cartographer_node_.reset()
33185 [stop_node]pre map_2_laser_.reset()
33185 [stop_node]pre global_rrt_detector_.reset()
33185 [stop_node]pre map_server_.reset()
The thread 'map_server' (10028) has exited with code 0 (0x0).
33282 [stop_node]pre move_base_node_.reset()
33282 [stop_node]pre shutdown_movebase_topics
33285 [stop_node]rosbag_record_.reset()
33285 [stop_node]rosbag_play_.reset()
{set_mode}stop_navigation_node, nposm --> prev(1)
<xwml.cpp>::wml_config_from_file------fname: C:/ddksample/apps-res/aplt_leagor_basic/xwml/aplt.bin
starting to read from C:/ddksample/apps-res/aplt_leagor_basic/lua/main.lua
starting to read from C:/ddksample/apps-res/aplt_leagor_basic/lua/home.lua
33311 {dbg_scroll}twindow::invalidate_layout(widget: nullptr)
