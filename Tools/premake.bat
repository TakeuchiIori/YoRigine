@echo off
rem Runs premake from Tools/. Paths in the lua are anchored to the repo root,
rem so the .sln is generated at root and build outputs go to ../generated.
"%~dp0premake5.exe" --file="%~dp0premake5.lua" vs2022
