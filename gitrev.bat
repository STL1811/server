rem @ECHO OFF
rem for /f "tokens=*" %%a in ('git rev-parse --verify --short HEAD') do (
rem     set TEMPRESPONSE=%%a
rem )
rem COPY "%~dp0\version.tmpl" "%~dp0\version.h" /Y
rem ECHO #define CASPAR_REV "%TEMPRESPONSE%" >> "%~dp0\version.h"
rem ECHO gitrev.bat: %TEMPRESPONSE%
rem SET TEMPRESPONSE=
rem exit /b 0