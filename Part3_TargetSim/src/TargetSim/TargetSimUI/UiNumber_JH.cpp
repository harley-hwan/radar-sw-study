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
