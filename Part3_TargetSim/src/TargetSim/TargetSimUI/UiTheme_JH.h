#pragma once

#include "TargetSim_JH.h"

// 화면 색. 밝은 바탕에 흰 카드를 놓고 강조색은 하나만 쓴다.
#define UI_COLOR_PAGE			RGB(243, 244, 246)
#define UI_COLOR_CARD			RGB(255, 255, 255)
#define UI_COLOR_BORDER			RGB(223, 226, 231)
#define UI_COLOR_GRID_LINE		RGB(236, 238, 241)
#define UI_COLOR_HEADER			RGB(249, 250, 251)
#define UI_COLOR_TEXT			RGB(31, 41, 55)
#define UI_COLOR_TEXT_SUB		RGB(107, 114, 128)
#define UI_COLOR_TEXT_OFF		RGB(176, 182, 191)
#define UI_COLOR_ACCENT			RGB(37, 99, 235)
#define UI_COLOR_ACCENT_SOFT	RGB(232, 240, 254)
#define UI_COLOR_ERROR			RGB(220, 38, 38)
#define UI_COLOR_ERROR_SOFT		RGB(254, 226, 226)
#define UI_COLOR_OK				RGB(5, 150, 105)
#define UI_COLOR_TRACK			RGB(229, 231, 235)

#define UI_OBJECT_NUM			(TGT_MAX_TARGET_NUM + 1)			// 플랫폼 + 표적
#define UI_BASE_DPI				96

// 객체 색. 표의 색 표식과 그림의 궤적·범례가 같은 색을 써서 표와 그림을 눈으로 바로 잇는다. 객체 0 은 플랫폼.
static inline COLORREF f_Ui_ObjectColor(INT32 nObject)
{
	static const COLORREF s_Color[UI_OBJECT_NUM] =
	{
		RGB(55, 65, 81),
		RGB(37, 99, 235), RGB(220, 38, 38), RGB(5, 150, 105), RGB(217, 119, 6), RGB(124, 58, 237),
		RGB(8, 145, 178), RGB(219, 39, 119), RGB(101, 163, 13), RGB(146, 64, 14), RGB(100, 116, 139)
	};
	COLORREF color = UI_COLOR_TEXT_SUB;

	if ((nObject >= 0) && (nObject < UI_OBJECT_NUM))
	{
		color = s_Color[nObject];
	}

	return color;
}

// 기동 축 색. 타임라인 막대에 쓴다.
static inline COLORREF f_Ui_TurnColor(EN_TurnType enTurnType)
{
	COLORREF color;

	switch (enTurnType)
	{
	case TGT_TURN_ROLL:
		color = RGB(139, 92, 246);
		break;

	case TGT_TURN_YAW:
		color = RGB(37, 99, 235);
		break;

	case TGT_TURN_PITCH:
		color = RGB(245, 158, 11);
		break;

	default:
		color = RGB(156, 163, 175);
		break;
	}

	return color;
}

// 96 DPI 기준 픽셀을 현재 DPI 의 픽셀로 바꾼다.
static inline INT32 f_Ui_Scale(INT32 pixel, INT32 dpi)
{
	return ::MulDiv(pixel, dpi, UI_BASE_DPI);
}
