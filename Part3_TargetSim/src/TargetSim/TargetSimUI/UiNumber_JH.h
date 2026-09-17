#pragma once

#include "Define_JH.h"

#define NUM_TEXT_SIZE			48
#define NUM_MAX_DECIMAL			9
#define NUM_DECIMAL_TOL			1.0e-9					// 자릿수를 셀 때 이진 표현 오차로 보는 상대 크기

typedef enum
{
	NUM_OK = 0,
	NUM_EMPTY,
	NUM_SYNTAX,
	NUM_NOT_FINITE
} EN_NumParse;

// 편집칸 문자열을 실수로 읽는다. 실패하면 *pt_Value 는 바꾸지 않는다.
EN_NumParse	f_Num_Parse(const CString &st_Text, FLOAT64 *pt_Value);

// 고정 소수 서식. 0 으로 반올림된 음수의 '-' 는 뗀다. 길이를 돌려주고 실패하면 -1.
INT32		f_Num_FormatFixed(CHAR *pt_Buf, INT32 bufSize, FLOAT64 value, INT32 nDecimal);

// 고정 소수 서식을 CString 으로 돌려준다. 실패하면 빈 문자열.
CString		f_Num_ToText(FLOAT64 value, INT32 nDecimal);

// 문자열의 소수 자릿수. 지수 표기이거나 소수점이 없으면 0.
INT32		f_Num_CountDecimal(const CString &st_Text);

// 숫자 문자열에 delta 를 더해 원래 자릿수(최소 nMinDecimal)로 다시 쓴다. 범위를 벗어나면 끝값으로 자른다.
// 읽을 수 없는 문자열이면 0 을 돌려주고 *st_Out 은 바꾸지 않는다.
INT32		f_Num_Nudge(const CString &st_Text, FLOAT64 delta, INT32 nMinDecimal, FLOAT64 minValue, FLOAT64 maxValue, CString *st_Out);
