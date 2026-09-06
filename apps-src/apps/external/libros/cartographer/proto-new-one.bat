set SCRIPTS=c:\ddksample\apps-src\scripts
set CARTOGRAPHER=C:\ddksample\apps-src\apps\external\libros\cartographer

set CUR_DIR=C:\ddksample\apps-src\apps\external\libros\cartographer\cartographer\ground_truth\proto
cd %CUR_DIR%
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. relations.proto
