set SCRIPTS=c:\ddksample\apps-src\scripts
set PROTOC_EXE=%SCRIPTS%\protoc-3.21.8.exe
set GOOGLE=C:\ddksample\apps-src\apps\external\protobuf\src
set TENSORFLOW=C:\ddksample\apps-src\apps\external\tensorflow

set CUR_DIR=C:\ddksample\apps-src\apps\external\tensorflow\tensorflow\core\example
cd %CUR_DIR%
%PROTOC_EXE% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. example.proto
%PROTOC_EXE% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. feature.proto

set CUR_DIR=C:\ddksample\apps-src\apps\external\tensorflow\tensorflow\core\framework
cd %CUR_DIR%
%PROTOC_EXE% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. allocation_description.proto
%PROTOC_EXE% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. attr_value.proto
%PROTOC_EXE% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. cost_graph.proto
%PROTOC_EXE% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. device_attributes.proto
%PROTOC_EXE% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. full_type.proto
%PROTOC_EXE% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. function.proto
%PROTOC_EXE% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. graph.proto
%PROTOC_EXE% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. node_def.proto
%PROTOC_EXE% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. op_def.proto
%PROTOC_EXE% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. resource_handle.proto
%PROTOC_EXE% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. step_stats.proto
%PROTOC_EXE% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. tensor.proto
%PROTOC_EXE% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. tensor_description.proto
%PROTOC_EXE% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. tensor_shape.proto
%PROTOC_EXE% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. types.proto
%PROTOC_EXE% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. versions.proto


set CUR_DIR=C:\ddksample\apps-src\apps\external\tensorflow\tensorflow\core\protobuf
cd %CUR_DIR%
%PROTOC_EXE% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. config.proto
%PROTOC_EXE% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. cluster.proto
%PROTOC_EXE% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. debug.proto
%PROTOC_EXE% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. rewriter_config.proto
%PROTOC_EXE% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. rpc_options.proto
%PROTOC_EXE% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. verifier_config.proto

set CUR_DIR=C:\ddksample\apps-src\apps\external\tensorflow\tensorflow\tsl\protobuf
cd %CUR_DIR%
%PROTOC_EXE% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. error_codes.proto
%PROTOC_EXE% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. coordination_config.proto
%PROTOC_EXE% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. rpc_options.proto
