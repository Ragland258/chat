@echo off
setlocal

set PROTOC=D:\vcpkg\installed\x64-windows\tools\protobuf\protoc.exe
set GRPC_CPP_PLUGIN=D:\vcpkg\installed\x64-windows\tools\grpc\grpc_cpp_plugin.exe
set PROTO_DIR=%~dp0proto
set OUT_DIR=%~dp0generated

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

"%PROTOC%" -I "%PROTO_DIR%" ^
  --cpp_out="%OUT_DIR%" ^
  --grpc_out="%OUT_DIR%" ^
  --plugin=protoc-gen-grpc="%GRPC_CPP_PLUGIN%" ^
  "%PROTO_DIR%\message.proto"

if errorlevel 1 (
  echo Failed to generate gRPC files.
  exit /b 1
)

echo Generated gRPC files in "%OUT_DIR%".
