@echo off
REM Clean build cache and force recompilation
echo Cleaning build cache...

cd build

REM Delete intermediate files for nova3d project
if exist nova3d.dir (
    echo Deleting nova3d.dir...
    rd /S /Q nova3d.dir
)

REM Delete CMake cache
if exist CMakeCache.txt (
    echo Deleting CMakeCache.txt...
    del /F CMakeCache.txt
)

REM Delete CMakeFiles
if exist CMakeFiles (
    echo Deleting CMakeFiles...
    rd /S /Q CMakeFiles
)

echo Clean complete!
echo.
echo Reconfiguring with CMake...
cmake ..

echo.
echo Build cache cleaned and reconfigured.
echo Now run: cmake --build . --config Debug --target nova3d
