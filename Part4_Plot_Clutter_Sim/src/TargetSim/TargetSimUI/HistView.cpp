#include "pch.h"

#include <math.h>

#include "HistView.h"
#include "UiCommon.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define HIST_COLOR_BAR			RGB(160, 196, 236)					// 표본 PDF 막대
#define HIST_COLOR_THEORY		RGB(235, 104, 52)					// 이론 PDF 선

// 96 DPI 기준 픽셀 -> 현재 화면 픽셀.
static INT32 f_Hist_Px(const CDC *st_Dc, INT32 pixel)
{
	return ::MulDiv(pixel, st_Dc->GetDeviceCaps(LOGPIXELSX), UI_BASE_DPI);
}

// 1, 2, 5 x 10^k 중 칸이 nMax 개 이하가 되는 가장 작은 눈금 간격.
static FLOAT64 f_Hist_Step(FLOAT64 span, INT32 nMax)
{
	FLOAT64 step = pow(10.0, floor(log10(span / static_cast<FLOAT64>(nMax))));

	if ((span / step) > static_cast<FLOAT64>(nMax))
	{
		step = step * 2.0;
	}

	if ((span / step) > static_cast<FLOAT64>(nMax))
	{
		step = step * 2.5;
	}

	if ((span / step) > static_cast<FLOAT64>(nMax))
	{
		step = step * 2.0;
	}

	return step;
}

// 눈금 간격에 맞는 소수 자릿수 (0.05 -> 2, 0.2 -> 1, 1 -> 0).
static INT32 f_Hist_Digits(FLOAT64 step)
{
	const INT32 nDigits = -static_cast<INT32>(floor(log10(step) + 1.0e-9));

	return (nDigits > 0) ? nDigits : 0;
}

// 글자. y 는 글자 세로 가운데.
static VOID f_Hist_Text(CDC *st_Dc, INT32 x, INT32 y, UINT32 nAlign, COLORREF color, LPCTSTR pt_Text)
{
	const INT32 nLength = static_cast<INT32>(_tcslen(pt_Text));
	const CSize st_Size = st_Dc->GetTextExtent(pt_Text, nLength);

	(VOID)st_Dc->SetTextAlign(nAlign | TA_TOP);
	(VOID)st_Dc->SetTextColor(color);
	(VOID)st_Dc->TextOut(x, y - (st_Size.cy / 2), pt_Text, nLength);
}

BEGIN_MESSAGE_MAP(CHistView, CStatic)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

// 그릴 히스토그램 교체 후 다시 그리기.
VOID CHistView::f_SetHist(const ST_PcsHist *st_NewHist, INT32 isNewGauss)
{
	st_Hist	= st_NewHist;
	isGauss	= isNewGauss;
	Invalidate(FALSE);
}

// 배경 지우기 생략. OnPaint 에서 전부 그림.
BOOL CHistView::OnEraseBkgnd(CDC *st_Dc)
{
	UNREFERENCED_PARAMETER(st_Dc);

	return TRUE;
}

// 메모리 DC 에 그린 뒤 한 번에 복사 (깜박임 방지).
VOID CHistView::OnPaint(VOID)
{
	CPaintDC	st_Dc(this);
	CDC			st_MemDc;
	CBitmap		st_Bitmap;
	CBitmap		*st_OldBitmap;
	CFont		*st_OldFont;
	CRect		st_Area;

	GetClientRect(&st_Area);

	if (st_Area.IsRectEmpty() == FALSE)
	{
		(VOID)st_MemDc.CreateCompatibleDC(&st_Dc);
		(VOID)st_Bitmap.CreateCompatibleBitmap(&st_Dc, st_Area.Width(), st_Area.Height());
		st_OldBitmap	= st_MemDc.SelectObject(&st_Bitmap);
		st_OldFont		= st_MemDc.SelectObject(GetParent()->GetFont());

		st_MemDc.FillSolidRect(&st_Area, UI_COLOR_CARD);
		(VOID)st_MemDc.SetBkMode(TRANSPARENT);

		if (st_Hist != nullptr)
		{
			f_Draw(&st_MemDc, st_Area);
		}

		st_MemDc.Draw3dRect(&st_Area, UI_COLOR_BORDER, UI_COLOR_BORDER);
		(VOID)st_Dc.BitBlt(0, 0, st_Area.Width(), st_Area.Height(), &st_MemDc, 0, 0, SRCCOPY);
		(VOID)st_MemDc.SelectObject(st_OldFont);
		(VOID)st_MemDc.SelectObject(st_OldBitmap);
	}
}

// 제목, 통계, 격자, 막대, 이론 PDF, 가로축 글자.
VOID CHistView::f_Draw(CDC *st_Dc, const CRect &st_Area) const
{
	const CRect		st_Plot(st_Area.left + f_Hist_Px(st_Dc, 46), st_Area.top + f_Hist_Px(st_Dc, 50), st_Area.right - f_Hist_Px(st_Dc, 14),
						st_Area.bottom - f_Hist_Px(st_Dc, 28));
	const FLOAT64	xMin = st_Hist->binMin;
	const FLOAT64	xMax = st_Hist->binMin + (static_cast<FLOAT64>(st_Hist->nBin) * st_Hist->binWidth);
	FLOAT64			yMax = (isGauss != 0) ? (1.0 / sqrt(2.0 * PI)) : 1.0;
	CString			st_Text;
	INT32			nIndex;

	for (nIndex = 0; nIndex < st_Hist->nBin; nIndex++)
	{
		yMax = fmax(yMax, st_Hist->pdf[nIndex]);
	}

	const FLOAT64 yStep = f_Hist_Step(yMax * 1.15, 5);

	yMax = ceil((yMax * 1.15) / yStep) * yStep;

	auto f_ToX = [&](FLOAT64 x)
	{
		return st_Plot.left + static_cast<INT32>(lround(((x - xMin) / (xMax - xMin)) * static_cast<FLOAT64>(st_Plot.Width())));
	};
	auto f_ToY = [&](FLOAT64 y)
	{
		return st_Plot.bottom - static_cast<INT32>(lround((y / yMax) * static_cast<FLOAT64>(st_Plot.Height())));
	};

	// 제목과 통계
	if (isGauss != 0)
	{
		f_Hist_Text(st_Dc, st_Area.left + f_Hist_Px(st_Dc, 10), st_Area.top + f_Hist_Px(st_Dc, 12), TA_LEFT, UI_COLOR_TEXT, _T("GAUSS(0, 1) 정규난수 10,000개"));
		st_Text.Format(_T("평균 %+.4f, 표준편차 %.4f  (이론 0, 1)"), st_Hist->mean, st_Hist->std);
	}
	else
	{
		f_Hist_Text(st_Dc, st_Area.left + f_Hist_Px(st_Dc, 10), st_Area.top + f_Hist_Px(st_Dc, 12), TA_LEFT, UI_COLOR_TEXT, _T("UNIRAN 균등난수 10,000개"));
		st_Text.Format(_T("평균 %.4f, 표준편차 %.4f  (이론 0.5, %.4f)"), st_Hist->mean, st_Hist->std, 1.0 / sqrt(12.0));
	}

	f_Hist_Text(st_Dc, st_Area.left + f_Hist_Px(st_Dc, 10), st_Area.top + f_Hist_Px(st_Dc, 30), TA_LEFT, UI_COLOR_TEXT_SUB, st_Text);

	// 가로 격자, 세로축 글자
	CPen	st_GridPen(PS_SOLID, 1, UI_COLOR_GRID_LINE);
	CPen	*st_OldPen = st_Dc->SelectObject(&st_GridPen);

	for (nIndex = 0; (static_cast<FLOAT64>(nIndex) * yStep) <= (yMax + (0.5 * yStep)); nIndex++)
	{
		const INT32 y = f_ToY(static_cast<FLOAT64>(nIndex) * yStep);

		(VOID)st_Dc->MoveTo(st_Plot.left, y);
		(VOID)st_Dc->LineTo(st_Plot.right, y);
		st_Text.Format(_T("%.*f"), f_Hist_Digits(yStep), static_cast<FLOAT64>(nIndex) * yStep);
		f_Hist_Text(st_Dc, st_Plot.left - f_Hist_Px(st_Dc, 6), y, TA_RIGHT, UI_COLOR_TEXT_SUB, st_Text);
	}

	// 막대
	for (nIndex = 0; nIndex < st_Hist->nBin; nIndex++)
	{
		const INT32 left = f_ToX(xMin + (static_cast<FLOAT64>(nIndex) * st_Hist->binWidth));
		const INT32 right = f_ToX(xMin + (static_cast<FLOAT64>(nIndex + 1) * st_Hist->binWidth));
		const INT32 top = f_ToY(st_Hist->pdf[nIndex]);

		st_Dc->FillSolidRect(left + 1, top, right - left - 1, st_Plot.bottom - top, HIST_COLOR_BAR);
	}

	// 이론 PDF
	CPen st_TheoryPen(PS_SOLID, f_Hist_Px(st_Dc, 2), HIST_COLOR_THEORY);

	(VOID)st_Dc->SelectObject(&st_TheoryPen);

	if (isGauss != 0)
	{
		for (nIndex = 0; nIndex <= 160; nIndex++)
		{
			const FLOAT64	x = xMin + (((xMax - xMin) * static_cast<FLOAT64>(nIndex)) / 160.0);
			const CPoint	st_Point(f_ToX(x), f_ToY(exp(-0.5 * x * x) / sqrt(2.0 * PI)));

			if (nIndex == 0)
			{
				(VOID)st_Dc->MoveTo(st_Point);
			}
			else
			{
				(VOID)st_Dc->LineTo(st_Point);
			}
		}
	}
	else
	{
		(VOID)st_Dc->MoveTo(f_ToX(xMin), f_ToY(1.0));
		(VOID)st_Dc->LineTo(f_ToX(xMax), f_ToY(1.0));
	}

	(VOID)st_Dc->SelectObject(st_OldPen);

	// 가로축 글자
	const FLOAT64 xStep = f_Hist_Step(xMax - xMin, 8);

	for (nIndex = static_cast<INT32>(ceil(xMin / xStep)); (static_cast<FLOAT64>(nIndex) * xStep) <= (xMax + (0.5 * xStep)); nIndex++)
	{
		st_Text.Format(_T("%.*f"), f_Hist_Digits(xStep), static_cast<FLOAT64>(nIndex) * xStep);
		f_Hist_Text(st_Dc, f_ToX(static_cast<FLOAT64>(nIndex) * xStep), st_Plot.bottom + f_Hist_Px(st_Dc, 10), TA_CENTER, UI_COLOR_TEXT_SUB, st_Text);
	}
}
