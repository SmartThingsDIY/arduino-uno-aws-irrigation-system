@echo off
REM Batch script to copy TensorFlow Lite Micro source into PlatformIO project

REM Set the source path to the downloaded tflite-micro repo
set SRC=C:\Users\Thoma\Downloads\tflite-micro-main\tensorflow
REM Set the destination path in your PlatformIO project
set DST=lib\tflite-micro\tensorflow

REM Remove any existing tensorflow folder in the destination
if exist "%DST%" (
    rmdir /s /q "%DST%"
)

REM Create the destination directory if it doesn't exist
if not exist "lib\tflite-micro" (
    mkdir "lib\tflite-micro"
)

REM Copy the tensorflow folder
xcopy /E /I /Y "%SRC%" "%DST%"

echo TensorFlow Lite Micro source copied to %DST%
pause