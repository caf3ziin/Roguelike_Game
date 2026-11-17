@echo off
setlocal enabledelayedexpansion

set GPP=C:\w64devkit\bin\g++.exe
set RAYLIB=C:\raylib

if /i "%1"=="release" (
  set FLAGS=-O2 -static -mwindows
  set MODO=release
) else (
  set FLAGS=-g -Wall
  set MODO=debug
)

echo Pasta atual: %CD%
echo Modo: !MODO!

if not exist build mkdir build

set SRC=
for %%f in (src\*.cpp) do set SRC=!SRC! %%f

if "!SRC!"=="" (
  echo *** NENHUM .cpp ENCONTRADO EM src\ ***
  exit /b 1
)

echo Compilando:!SRC!
%GPP% !SRC! !FLAGS! -o build\jogo.exe -I%RAYLIB%\include -L%RAYLIB%\lib -lraylib -lopengl32 -lgdi32 -lwinmm
if errorlevel 1 (
  echo *** ERRO NA COMPILACAO ***
  exit /b 1
)

if exist resources xcopy /E /I /Y /Q resources build\resources >nul

if /i "%1"=="release" (
  echo Build de entrega pronta em build\
  exit /b 0
)

echo OK, rodando...
build\jogo.exe
