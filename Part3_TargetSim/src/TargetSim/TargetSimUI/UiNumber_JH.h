#pragma once

#include "Define_JH.h"

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
