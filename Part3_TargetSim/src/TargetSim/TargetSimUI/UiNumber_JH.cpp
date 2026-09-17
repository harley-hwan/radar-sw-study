#include "pch.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "UiNumber_JH.h"

EN_NumParse f_Num_Parse(const CString &st_Text, FLOAT64 *pt_Value)
{
	EN_NumParse	enResult;
	CString		st_Trim = st_Text;
	CHAR		*pt_End = nullptr;
	FLOAT64		value;

	(VOID)st_Trim.Trim();

	if (st_Trim.IsEmpty())
	{
		enResult = NUM_EMPTY;
	}
	// 숫자 문자만 받으면 inf, nan, 16진수, '32,125' 같은 표기가 여기서 걸린다.
	else if (st_Trim.SpanIncluding(_T("0123456789+-.eE")).GetLength() != st_Trim.GetLength())
	{
		enResult = NUM_SYNTAX;
	}
	else
	{
		CStringA st_Ascii(st_Trim);

		value = strtod(st_Ascii.GetString(), &pt_End);

		if ((pt_End == st_Ascii.GetString()) || (*pt_End != '\0'))
		{
			enResult = NUM_SYNTAX;
		}
		else if (!isfinite(value))
		{
			enResult = NUM_NOT_FINITE;
		}
		else
		{
			*pt_Value	= value;
			enResult	= NUM_OK;
		}
	}

	return enResult;
}

INT32 f_Num_FormatFixed(CHAR *pt_Buf, INT32 bufSize, FLOAT64 value, INT32 nDecimal)
{
	INT32	nLength = -1;
	INT32	isZero;
	INT32	nIndex;

	if ((pt_Buf != nullptr) && (bufSize > 1))
	{
		nLength = _snprintf_s(pt_Buf, static_cast<UINT64>(bufSize), _TRUNCATE, "%.*f", nDecimal, value);

		if ((nLength > 1) && (pt_Buf[0] == '-'))
		{
			// -9.3e-10 을 %.4f 로 쓰면 -0.0000 이 되므로 표와 CSV 에서 부호를 없앤다.
			isZero = 1;

			for (nIndex = 1; nIndex < nLength; nIndex++)
			{
				if ((pt_Buf[nIndex] != '0') && (pt_Buf[nIndex] != '.'))
				{
					isZero = 0;
				}
			}

			if (isZero != 0)
			{
				(VOID)memmove(pt_Buf, &pt_Buf[1], static_cast<UINT64>(nLength));
				nLength = nLength - 1;
			}
		}
	}

	return nLength;
}

CString f_Num_ToText(FLOAT64 value, INT32 nDecimal)
{
	CHAR	pt_Buf[NUM_TEXT_SIZE];
	CString	st_Text;

	if (f_Num_FormatFixed(pt_Buf, NUM_TEXT_SIZE, value, nDecimal) > 0)
	{
		st_Text = CString(pt_Buf);
	}

	return st_Text;
}

// 값을 그대로 적는 데 필요한 소수 자릿수 (0 ~ NUM_MAX_DECIMAL)
static INT32 f_Num_NeededDecimal(FLOAT64 value)
{
	FLOAT64	scaled = value;
	INT32	nDecimal = 0;
	INT32	isDone = 0;

	while ((nDecimal < NUM_MAX_DECIMAL) && (isDone == 0))
	{
		if (fabs(scaled - floor(scaled + 0.5)) <= (NUM_DECIMAL_TOL * fmax(1.0, fabs(scaled))))
		{
			isDone = 1;
		}
		else
		{
			scaled		= scaled * 10.0;
			nDecimal	= nDecimal + 1;
		}
	}

	return nDecimal;
}

INT32 f_Num_CountDecimal(const CString &st_Text)
{
	CString	st_Trim = st_Text;
	FLOAT64	value = 0.0;
	INT32	nDecimal = 0;
	INT32	nDot;

	(VOID)st_Trim.Trim();
	nDot = st_Trim.Find(_T('.'));

	if (st_Trim.FindOneOf(_T("eE")) >= 0)
	{
		// 지수 표기(1.2345e2)는 글자 수로 셀 수 없으므로 값에서 구한다.
		if (f_Num_Parse(st_Trim, &value) == NUM_OK)
		{
			nDecimal = f_Num_NeededDecimal(value);
		}
	}
	else if (nDot >= 0)
	{
		nDecimal = st_Trim.GetLength() - nDot - 1;
	}
	else
	{
		nDecimal = 0;
	}

	if (nDecimal > NUM_MAX_DECIMAL)
	{
		nDecimal = NUM_MAX_DECIMAL;
	}

	return nDecimal;
}

INT32 f_Num_Nudge(const CString &st_Text, FLOAT64 delta, INT32 nMinDecimal, FLOAT64 minValue, FLOAT64 maxValue, CString *st_Out)
{
	FLOAT64	value = 0.0;
	INT32	nDecimal;
	INT32	isClamped = 1;
	INT32	isOk = 0;

	if ((st_Out != nullptr) && (f_Num_Parse(st_Text, &value) == NUM_OK))
	{
		// 쓰던 자릿수를 지켜야 값을 굴리는 동안 칸 너비와 끝자리가 흔들리지 않는다.
		nDecimal = f_Num_CountDecimal(st_Text);

		if (nDecimal < nMinDecimal)
		{
			nDecimal = nMinDecimal;
		}

		value = value + delta;

		if (value < minValue)
		{
			value = minValue;
		}
		else if (value > maxValue)
		{
			value = maxValue;
		}
		else
		{
			isClamped = 0;
		}

		// 범위 끝(예: 0.1, 89.9)에 닿으면 그 값을 적을 만큼은 자릿수를 늘린다. 아니면 1 → 0.1 이 "0" 으로 적힌다.
		if ((isClamped != 0) && (nDecimal < f_Num_NeededDecimal(value)))
		{
			nDecimal = f_Num_NeededDecimal(value);
		}

		*st_Out	= f_Num_ToText(value, nDecimal);
		isOk	= (st_Out->IsEmpty()) ? 0 : 1;
	}

	return isOk;
}
