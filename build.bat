@echo off
setlocal enabledelayedexpansion

set GPP=C:\w64devkit\bin\g++.exe
set RAYLIB=C:\raylib

echo Pasta atual: %CD%

if not exist build mkdir build

set SRC=
for %%f in (src\*.cpp) do set SRC=!SRC! %%f

if "!SRC!"=="" (
  echo *** NENHUM .cpp ENCONTRADO EM src\ ***
  exit /b 1
)

echo Compilando:!SRC!
%GPP% !SRC! -o build\jogo.exe -I%RAYLIB%\include -L%RAYLIB%\lib -lraylib -lopengl32 -lgdi32 -lwinmm
if errorlevel 1 (
  echo *** ERRO NA COMPILACAO ***
  exit /b 1
)

echo OK, rodando...
build\jogo.exe