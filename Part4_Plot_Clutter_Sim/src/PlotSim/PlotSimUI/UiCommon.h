#pragma once

#include <vector>

#include "PlotSim.h"

// 색
#define UI_COLOR_SURFACE		RGB(252, 252, 251)					// 바탕
#define UI_COLOR_INK			RGB(11, 11, 11)						// 제목, 레이다
#define UI_COLOR_INK_SUB		RGB(82, 81, 78)						// 보조 글자, 기준선
#define UI_COLOR_MUTED			RGB(137, 135, 129)					// 눈금 글자
#define UI_COLOR_GRID			RGB(225, 224, 217)
#define UI_COLOR_AXIS			RGB(195, 194, 183)
#define UI_COLOR_TRUE			RGB(42, 120, 214)					// 참값 (파랑)
#define UI_COLOR_PLOT			RGB(235, 104, 52)					// 플롯 (주황)
#define UI_COLOR_CLUTTER		RGB(27, 175, 122)					// 클러터 (초록)

#define UI_BASE_DPI				96

// 96 DPI 기준 픽셀 -> 현재 DPI 픽셀
static inline INT32 f_Ui_Scale(INT32 pixel, INT32 dpi)
{
	return ::MulDiv(pixel, dpi, UI_BASE_DPI);
}

// 결과 점 하나 + 스캔 번호.
struct ST_UiPoint
{
	INT32				nScan = 0;
	ST_PcsPoint			st_Point = {};
};

// 차트 자료. 대화상자가 갖고 있고 차트는 읽기만 함.
struct ST_ChartData
{
	ST_PcsHist				st_Uniform = {};						// UNIRAN 히스토그램
	ST_PcsHist				st_Gauss = {};							// GAUSS 히스토그램
	STRUCT_Coord_Lla		st_RadarLla = {};
	INT32					nTargetNum = 0;
	INT32					nPlotNum = 0;
	INT32					nClutterNum = 0;
	std::vector<ST_UiPoint>	st_Points;								// 참값, 플롯, 클러터 (스캔 순서)
};
