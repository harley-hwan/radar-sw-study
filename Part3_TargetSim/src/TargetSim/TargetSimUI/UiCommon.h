#pragma once

#include "TargetSim.h"

// 색
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

// 숫자 문자열
#define NUM_TEXT_SIZE			48
#define NUM_MAX_DECIMAL			9
#define NUM_DECIMAL_TOL			1.0e-9

typedef enum
{
	NUM_OK = 0,
	NUM_EMPTY,
	NUM_SYNTAX,
	NUM_NOT_FINITE
} EN_NumParse;

// 객체 색 (0 = 플랫폼). 표의 색 표식과 그림의 궤적이 같은 색을 쓴다.
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

// 기동 축 색
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

// 96 DPI 기준 픽셀 → 현재 DPI 픽셀
static inline INT32 f_Ui_Scale(INT32 pixel, INT32 dpi)
{
	return ::MulDiv(pixel, dpi, UI_BASE_DPI);
}

// 실패하면 *pt_Value 는 그대로
EN_NumParse	f_Num_Parse(const CString &st_Text, FLOAT64 *pt_Value);

// 고정 소수 서식. -0.0000 은 부호를 뗀다. 길이를 돌려주고 실패하면 -1
INT32		f_Num_FormatFixed(CHAR *pt_Buf, INT32 bufSize, FLOAT64 value, INT32 nDecimal);
CString		f_Num_ToText(FLOAT64 value, INT32 nDecimal);

// 문자열의 소수 자릿수
INT32		f_Num_CountDecimal(const CString &st_Text);

// 문자열 값에 delta 를 더해 자릿수를 지켜 다시 쓴다. [minValue, maxValue] 로 자른다. 읽을 수 없으면 0
INT32		f_Num_Nudge(const CString &st_Text, FLOAT64 delta, INT32 nMinDecimal, FLOAT64 minValue, FLOAT64 maxValue, CString *st_Out);
