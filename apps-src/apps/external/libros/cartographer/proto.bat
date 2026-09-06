set SCRIPTS=c:\ddksample\apps-src\scripts
set CARTOGRAPHER=C:\ddksample\apps-src\apps\external\libros\cartographer

set CUR_DIR=C:\ddksample\apps-src\apps\external\libros\cartographer\cartographer\cloud\proto
cd %CUR_DIR%
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. map_builder_server_options.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. map_builder_service.proto

set CUR_DIR=C:\ddksample\apps-src\apps\external\libros\cartographer\cartographer\common\proto
cd %CUR_DIR%
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. ceres_solver_options.proto

set CUR_DIR=C:\ddksample\apps-src\apps\external\libros\cartographer\cartographer\ground_truth\proto
cd %CUR_DIR%
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. relations.proto

set CUR_DIR=C:\ddksample\apps-src\apps\external\libros\cartographer\cartographer\mapping\proto
cd %CUR_DIR%
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. cell_limits_2d.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. connected_components.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. grid_2d.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. grid_2d_options.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. hybrid_grid.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. local_trajectory_builder_options_2d.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. local_trajectory_builder_options_3d.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. map_builder_options.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. map_limits.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. motion_filter_options.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. normal_estimation_options_2d.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. pose_extrapolator_options.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. pose_graph.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. pose_graph_options.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. probability_grid.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. probability_grid_range_data_inserter_options_2d.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. range_data_inserter_options.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. range_data_inserter_options_3d.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. serialization.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. submap.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. submap_visualization.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. submaps_options_2d.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. submaps_options_3d.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. trajectory.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. trajectory_builder_options.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. trajectory_node_data.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. tsdf_2d.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. tsdf_range_data_inserter_options_2d.proto

set CUR_DIR=C:\ddksample\apps-src\apps\external\libros\cartographer\cartographer\mapping\proto\pose_graph
cd %CUR_DIR%
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. constraint_builder_options.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. optimization_problem_options.proto

set CUR_DIR=C:\ddksample\apps-src\apps\external\libros\cartographer\cartographer\mapping\proto\scan_matching
cd %CUR_DIR%
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. ceres_scan_matcher_options_2d.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. ceres_scan_matcher_options_3d.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. fast_correlative_scan_matcher_options_2d.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. fast_correlative_scan_matcher_options_3d.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. real_time_correlative_scan_matcher_options.proto

set CUR_DIR=C:\ddksample\apps-src\apps\external\libros\cartographer\cartographer\sensor\proto
cd %CUR_DIR%
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. adaptive_voxel_filter_options.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. sensor.proto

set CUR_DIR=C:\ddksample\apps-src\apps\external\libros\cartographer\cartographer\transform\proto
cd %CUR_DIR%
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. timestamped_transform.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. transform.proto