#!/bin/sh
# uiview 빌드 (리눅스 gcc / g++): Core C 소스 + 실제 TargetSimUI 소스 + MFC 흉내 (afx_stub.h)
# usage: sh build.sh <목적 파일 폴더>      -> <폴더>/uiview
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SRC="$HERE/../../../../../src/TargetSim"
CORE="$SRC/TargetSimCore"
UI="$SRC/TargetSimUI"
O=${1:?usage: sh build.sh <object dir>}
mkdir -p "$O"

for f in PlotLink TrackMgr TargetSim PlotSim CoordinateTransform matrixCalcLib NoiseGeneration; do
	gcc -O2 -std=gnu11 -w -I "$HERE/../winstub" -I "$CORE/include" -c "$CORE/src/$f.c" -o "$O/c_$f.o"
done

CXX_FLAGS="-std=c++17 -O1 -Wall -Wextra -Wno-unknown-pragmas -I $HERE/shim -I $HERE -I $UI -I $CORE/include"
for f in Scenario PlotResult TrajectoryPlot TargetSimUIDlg ScoreDlg TargetSimUI; do
	g++ $CXX_FLAGS -c "$UI/$f.cpp" -o "$O/u_$f.o"
done
g++ $CXX_FLAGS -c "$HERE/stub_impl.cpp" -o "$O/stub_impl.o"
g++ $CXX_FLAGS -c "$HERE/harness_main.cpp" -o "$O/harness_main.o"
g++ -o "$O/uiview" "$O"/c_*.o "$O"/u_*.o "$O/stub_impl.o" "$O/harness_main.o" -lpthread
echo "built $O/uiview"
