set SCRIPTS=c:\ddksample\apps-src\scripts
set GOOGLE=C:\ddksample\apps-src\apps\external\protobuf\src
set CARTOGRAPHER=C:\ddksample\apps-src\apps\external\mediapipe
set TENSORFLOW=C:\ddksample\apps-src\apps\external\tensorflow


set CUR_DIR=C:\ddksample\apps-src\apps\external\mediapipe\mediapipe\util\tracking
cd %CUR_DIR%
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. box_detector.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. box_tracker.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. camera_motion.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. flow_packager.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. frame_selection.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. frame_selection_solution_evaluator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. motion_analysis.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. motion_estimation.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. motion_models.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. motion_saliency.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. push_pull_filtering.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. region_flow.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. region_flow_computation.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. tone_estimation.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. tone_models.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. tracked_detection_manager_config.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. tracking.proto


