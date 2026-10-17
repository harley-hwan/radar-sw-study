#pragma once

#include "TargetSim.h"

// 색
#define UI_COLOR_PAGE			RGB(243, 244, 246)
#define UI_COLOR_CARD			RGB(255, 255, 255)
#define UI_COLOR_BORDER			RGB(223, 226, 231)
#define UI_COLOR_GRID_LINE		RGB(236, 238, 241)
#define UI_COLOR_TEXT			RGB(31, 41, 55)
#define UI_COLOR_TEXT_SUB		RGB(107, 114, 128)
#define UI_COLOR_TEXT_OFF		RGB(176, 182, 191)

#define UI_OBJECT_NUM			(TGT_MAX_TARGET_NUM + 1)			// 플랫폼 + 표적
#define UI_BASE_DPI				96

// 객체 색 (0 = 플랫폼). 궤적, 시작점, 이름표 공용.
static inline COLORREF f_Ui_ObjectColor(INT32 nObject)
{
	static const COLORREF s_Color[UI_OBJECT_NUM] =
	{
		RGB(55, 65, 81),
		RGB(37, 99, 235), RGB(220, 38, 38), RGB(5, 150, 105), RGB(217, 119, 6), RGB(124, 58, 237),
		RGB(8, 145, 178), RGB(219, 39, 119), RGB(101, 163, 13), RGB(146, 64, 14), RGB(100, 116, 139)
	};

	return s_Color[nObject];
}

// 96 DPI 기준 픽셀 -> 현재 DPI 픽셀
static inline INT32 f_Ui_Scale(INT32 pixel, INT32 dpi)
{
	return ::MulDiv(pixel, dpi, UI_BASE_DPI);
}
