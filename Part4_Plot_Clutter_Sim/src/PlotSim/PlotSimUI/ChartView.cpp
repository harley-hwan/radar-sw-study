#include "pch.h"

#include <math.h>

#include "ChartView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define CHART_INK				RGB(40, 40, 40)						// 제목, 레이다
#define CHART_MUTED				RGB(120, 120, 120)					// 눈금 글자
#define CHART_GRID				RGB(230, 230, 226)
#define CHART_FRAME				RGB(180, 180, 175)
#define CHART_TRUE				RGB(42, 120, 214)					// 참값 (파랑)
#define CHART_PLOT				RGB(235, 104, 52)					// 플롯 (주황), 이론 PDF
#define CHART_CLUTTER			RGB(27, 175, 122)					// 클러터 (초록)
#define CHART_BAR				RGB(160, 196, 236)					// 히스토그램 막대

// 96 DPI 기준 픽셀 -> 현재 화면 픽셀
static INT32 f_Chart_Px(const CDC *st_Dc, INT32 pixel)
{
	return ::MulDiv(pixel, st_Dc->GetDeviceCaps(LOGPIXELSX), 96);
}

// 1, 2, 5 x 10^k 중 칸이 nMax 개 이하가 되는 가장 작은 눈금 간격
static FLOAT64 f_Chart_Step(FLOAT64 span, INT32 nMax)
{
	FLOAT64 step = pow(10.0, floor(log10(span / (FLOAT64)nMax)));

	if ((span / step) > (FLOAT64)nMax)
	{
		step = step * 2.0;
	}

	if ((span / step) > (FLOAT64)nMax)
	{
		step = step * 2.5;
	}

	if ((span / step) > (FLOAT64)nMax)
	{
		step = step * 2.0;
	}

	return step;
}

// 눈금 간격에 맞는 소수 자릿수 (0.05 -> 2, 0.2 -> 1, 1 -> 0)
static INT32 f_Chart_Digits(FLOAT64 step)
{
	const INT32 nDigits = -static_cast<INT32>(floor(log10(step) + 1.0e-9));

	return (nDigits > 0) ? nDigits : 0;
}

// (x, y) 기준 글자. nAlign 은 TA_LEFT / TA_CENTER / TA_RIGHT, 세로는 y 가 글자 가운데
static VOID f_Chart_Text(CDC *st_Dc, INT32 x, INT32 y, UINT32 nAlign, COLORREF color, LPCTSTR pt_Text)
{
	const INT32 nLength = static_cast<INT32>(_tcslen(pt_Text));
	const CSize st_Size = st_Dc->GetTextExtent(pt_Text, nLength);

	(VOID)st_Dc->SetTextAlign(nAlign | TA_TOP);
	(VOID)st_Dc->SetTextColor(color);
	(VOID)st_Dc->TextOut(x, y - (st_Size.cy / 2), pt_Text, nLength);
}

// 점 위에서도 읽히게 흰 바탕을 깔고 쓰는 글자. (x, y) 는 글자 가운데
static VOID f_Chart_Label(CDC *st_Dc, INT32 x, INT32 y, COLORREF color, LPCTSTR pt_Text)
{
	const CSize	st_Size = st_Dc->GetTextExtent(pt_Text, static_cast<INT32>(_tcslen(pt_Text)));
	const INT32	pad = f_Chart_Px(st_Dc, 1);

	st_Dc->FillSolidRect(x - (st_Size.cx / 2) - pad, y - (st_Size.cy / 2) - pad, st_Size.cx + (2 * pad), st_Size.cy + (2 * pad), RGB(255, 255, 255));
	f_Chart_Text(st_Dc, x, y, TA_CENTER, color, pt_Text);
}

BEGIN_MESSAGE_MAP(CChartView, CStatic)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

VOID CChartView::f_SetMap(const ST_PcsScan *st_NewScan)
{
	st_Scan = st_NewScan;
	Invalidate(FALSE);
}

VOID CChartView::f_SetHist(const ST_PcsHist *st_NewHist, INT32 isNewGauss)
{
	st_Hist	= st_NewHist;
	isGauss	= isNewGauss;
	Invalidate(FALSE);
}

// 바탕은 OnPaint 에서 칠함.
BOOL CChartView::OnEraseBkgnd(CDC *st_Dc)
{
	UNREFERENCED_PARAMETER(st_Dc);
	return TRUE;
}

// 메모리 DC 에 그린 뒤 한 번에 복사 (깜박임 방지).
VOID CChartView::OnPaint(VOID)
{
	CPaintDC	st_Dc(this);
	CDC			st_MemDc;
	CBitmap		st_Bitmap;
	CRect		st_Area;

	GetClientRect(&st_Area);

	if (st_Area.IsRectEmpty())
	{
		return;
	}

	(VOID)st_MemDc.CreateCompatibleDC(&st_Dc);
	(VOID)st_Bitmap.CreateCompatibleBitmap(&st_Dc, st_Area.Width(), st_Area.Height());

	CBitmap	*st_OldBitmap = st_MemDc.SelectObject(&st_Bitmap);
	CFont	*st_OldFont = st_MemDc.SelectObject(GetParent()->GetFont());

	st_MemDc.FillSolidRect(&st_Area, RGB(255, 255, 255));
	(VOID)st_MemDc.SetBkMode(TRANSPARENT);

	if (st_Scan != nullptr)
	{
		f_DrawMap(&st_MemDc, st_Area);
	}
	else if (st_Hist != nullptr)
	{
		f_DrawHist(&st_MemDc, st_Area);
	}
	else
	{
		// 아직 자료 없음
	}

	st_MemDc.Draw3dRect(&st_Area, CHART_FRAME, CHART_FRAME);
	(VOID)st_Dc.BitBlt(0, 0, st_Area.Width(), st_Area.Height(), &st_MemDc, 0, 0, SRCCOPY);
	(VOID)st_MemDc.SelectObject(st_OldFont);
	(VOID)st_MemDc.SelectObject(st_OldBitmap);
}

// LLA 지도: 클러터 (초록), 플롯 (주황), 참값 궤적 (파란 선), 레이다 (삼각형), 표적 번호 (궤적 시작점).
// 경도 쪽에 cos(위도) 를 곱해 가로, 세로 거리 비율을 맞춤.
VOID CChartView::f_DrawMap(CDC *st_Dc, const CRect &st_Area) const
{
	const CRect	st_Plot(st_Area.left + f_Chart_Px(st_Dc, 64), st_Area.top + f_Chart_Px(st_Dc, 46), st_Area.right - f_Chart_Px(st_Dc, 16),
					st_Area.bottom - f_Chart_Px(st_Dc, 40));
	const INT32	dot = f_Chart_Px(st_Dc, 2);
	FLOAT64		latMin = PCS_RADAR_LAT;
	FLOAT64		latMax = PCS_RADAR_LAT;
	FLOAT64		lonMin = PCS_RADAR_LON;
	FLOAT64		lonMax = PCS_RADAR_LON;
	CString		st_Text;
	INT32		nScan;
	INT32		nTarget;
	INT32		nClutter;
	INT32		nTick;

	// 범위: 레이다, 참값, 플롯, 클러터 전부
	auto f_Extend = [&](const STRUCT_Coord_Lla &st_Lla)
	{
		latMin = fmin(latMin, f_Rad_To_Deg(st_Lla.Lat));
		latMax = fmax(latMax, f_Rad_To_Deg(st_Lla.Lat));
		lonMin = fmin(lonMin, f_Rad_To_Deg(st_Lla.Lon));
		lonMax = fmax(lonMax, f_Rad_To_Deg(st_Lla.Lon));
	};

	for (nScan = 0; nScan < PCS_SCAN_NUM; nScan++)
	{
		for (nTarget = 0; nTarget < PCS_TARGET_NUM; nTarget++)
		{
			f_Extend(st_Scan[nScan].st_TrueLla[nTarget]);
			f_Extend(st_Scan[nScan].st_PlotLla[nTarget]);
		}

		for (nClutter = 0; nClutter < st_Scan[nScan].nClutterNum; nClutter++)
		{
			f_Extend(st_Scan[nScan].st_ClutterLla[nClutter]);
		}
	}

	// 축척 [px / deg]. 둘레에 5 % 여유
	const FLOAT64	latMid = 0.5 * (latMin + latMax);
	const FLOAT64	lonMid = 0.5 * (lonMin + lonMax);
	const FLOAT64	cosLat = cos(f_Deg_To_Rad(latMid));
	const FLOAT64	scale = fmin(st_Plot.Width() / (fmax((lonMax - lonMin) * cosLat, 0.001) * 1.1),
						st_Plot.Height() / (fmax(latMax - latMin, 0.001) * 1.1));
	const CPoint	st_Center = st_Plot.CenterPoint();

	auto f_ToPoint = [&](FLOAT64 latDeg, FLOAT64 lonDeg)
	{
		return CPoint(st_Center.x + static_cast<INT32>(lround((lonDeg - lonMid) * cosLat * scale)),
			st_Center.y - static_cast<INT32>(lround((latDeg - latMid) * scale)));
	};
	auto f_ToPointLla = [&](const STRUCT_Coord_Lla &st_Lla)
	{
		return f_ToPoint(f_Rad_To_Deg(st_Lla.Lat), f_Rad_To_Deg(st_Lla.Lon));
	};

	// 격자와 눈금 글자. 그림 칸에 보이는 범위 기준
	const FLOAT64	latLo = latMid - ((0.5 * st_Plot.Height()) / scale);
	const FLOAT64	latHi = latMid + ((0.5 * st_Plot.Height()) / scale);
	const FLOAT64	lonLo = lonMid - ((0.5 * st_Plot.Width()) / (scale * cosLat));
	const FLOAT64	lonHi = lonMid + ((0.5 * st_Plot.Width()) / (scale * cosLat));
	const FLOAT64	latStep = f_Chart_Step(latHi - latLo, 6);
	const FLOAT64	lonStep = f_Chart_Step(lonHi - lonLo, 6);
	CPen			st_GridPen(PS_SOLID, 1, CHART_GRID);
	CPen			*st_OldPen = st_Dc->SelectObject(&st_GridPen);

	for (nTick = static_cast<INT32>(ceil(latLo / latStep)); (nTick * latStep) <= latHi; nTick++)
	{
		const INT32 y = f_ToPoint(nTick * latStep, lonMid).y;

		(VOID)st_Dc->MoveTo(st_Plot.left, y);
		(VOID)st_Dc->LineTo(st_Plot.right, y);
		st_Text.Format(_T("%.*f"), f_Chart_Digits(latStep), nTick * latStep);
		f_Chart_Text(st_Dc, st_Plot.left - f_Chart_Px(st_Dc, 6), y, TA_RIGHT, CHART_MUTED, st_Text);
	}

	for (nTick = static_cast<INT32>(ceil(lonLo / lonStep)); (nTick * lonStep) <= lonHi; nTick++)
	{
		const INT32 x = f_ToPoint(latMid, nTick * lonStep).x;

		(VOID)st_Dc->MoveTo(x, st_Plot.top);
		(VOID)st_Dc->LineTo(x, st_Plot.bottom);
		st_Text.Format(_T("%.*f"), f_Chart_Digits(lonStep), nTick * lonStep);
		f_Chart_Text(st_Dc, x, st_Plot.bottom + f_Chart_Px(st_Dc, 10), TA_CENTER, CHART_MUTED, st_Text);
	}

	f_Chart_Text(st_Dc, st_Plot.CenterPoint().x, st_Area.bottom - f_Chart_Px(st_Dc, 12), TA_CENTER, CHART_MUTED, _T("경도 [°]"));
	f_Chart_Text(st_Dc, st_Area.left + f_Chart_Px(st_Dc, 10), st_Plot.top - f_Chart_Px(st_Dc, 12), TA_LEFT, CHART_MUTED, _T("위도 [°]"));
	(VOID)st_Dc->SelectObject(st_OldPen);

	// 클러터, 플롯
	CBrush	st_PlotBrush(CHART_PLOT);
	CPen	st_PlotPen(PS_SOLID, 1, CHART_PLOT);
	CBrush	*st_OldBrush = st_Dc->SelectObject(&st_PlotBrush);

	st_OldPen = st_Dc->SelectObject(&st_PlotPen);

	for (nScan = 0; nScan < PCS_SCAN_NUM; nScan++)
	{
		for (nClutter = 0; nClutter < st_Scan[nScan].nClutterNum; nClutter++)
		{
			const CPoint st_Point = f_ToPointLla(st_Scan[nScan].st_ClutterLla[nClutter]);

			st_Dc->FillSolidRect(st_Point.x - (dot / 2), st_Point.y - (dot / 2), dot, dot, CHART_CLUTTER);
		}
	}

	for (nScan = 0; nScan < PCS_SCAN_NUM; nScan++)
	{
		for (nTarget = 0; nTarget < PCS_TARGET_NUM; nTarget++)
		{
			const CPoint st_Point = f_ToPointLla(st_Scan[nScan].st_PlotLla[nTarget]);

			(VOID)st_Dc->Ellipse(st_Point.x - dot, st_Point.y - dot, st_Point.x + dot + 1, st_Point.y + dot + 1);
		}
	}

	// 참값 궤적. 표적마다 선 하나
	CPen st_TruePen(PS_SOLID, f_Chart_Px(st_Dc, 2), CHART_TRUE);

	(VOID)st_Dc->SelectObject(&st_TruePen);

	for (nTarget = 0; nTarget < PCS_TARGET_NUM; nTarget++)
	{
		(VOID)st_Dc->MoveTo(f_ToPointLla(st_Scan[0].st_TrueLla[nTarget]));

		for (nScan = 1; nScan < PCS_SCAN_NUM; nScan++)
		{
			(VOID)st_Dc->LineTo(f_ToPointLla(st_Scan[nScan].st_TrueLla[nTarget]));
		}
	}

	// 레이다
	CBrush			st_RadarBrush(CHART_INK);
	CPen			st_RadarPen(PS_SOLID, 1, CHART_INK);
	const CPoint	st_Radar = f_ToPoint(PCS_RADAR_LAT, PCS_RADAR_LON);
	const INT32		size = f_Chart_Px(st_Dc, 6);
	const POINT		st_Triangle[3] = { { st_Radar.x, st_Radar.y - size }, { st_Radar.x - size, st_Radar.y + size }, { st_Radar.x + size, st_Radar.y + size } };

	(VOID)st_Dc->SelectObject(&st_RadarBrush);
	(VOID)st_Dc->SelectObject(&st_RadarPen);
	(VOID)st_Dc->Polygon(st_Triangle, 3);
	(VOID)st_Dc->SelectObject(st_OldBrush);
	(VOID)st_Dc->SelectObject(st_OldPen);

	// 표적 번호: 궤적 시작점에서 진행 방향 반대쪽에 써서 궤적을 가리지 않게 함 (방향은 처음 5 s 기준)
	for (nTarget = 0; nTarget < PCS_TARGET_NUM; nTarget++)
	{
		const CPoint	st_Start = f_ToPointLla(st_Scan[0].st_TrueLla[nTarget]);
		const CPoint	st_Next = f_ToPointLla(st_Scan[PCS_SCAN_NUM / 12].st_TrueLla[nTarget]);
		const FLOAT64	dx = static_cast<FLOAT64>(st_Next.x - st_Start.x);
		const FLOAT64	dy = static_cast<FLOAT64>(st_Next.y - st_Start.y);
		const FLOAT64	length = sqrt((dx * dx) + (dy * dy));
		const FLOAT64	gap = static_cast<FLOAT64>(f_Chart_Px(st_Dc, 11));
		const INT32		x = st_Start.x - static_cast<INT32>(lround((length > 0.0) ? ((gap * dx) / length) : -gap));
		const INT32		y = st_Start.y - static_cast<INT32>(lround((length > 0.0) ? ((gap * dy) / length) : gap));

		st_Text.Format(_T("%d"), nTarget + 1);
		f_Chart_Label(st_Dc, x, y, CHART_INK, st_Text);
	}

	// 제목, 범례
	static const LPCTSTR	s_LegendText[4] = { _T("참값"), _T("플롯"), _T("클러터"), _T("레이다") };
	static const COLORREF	s_LegendColor[4] = { CHART_TRUE, CHART_PLOT, CHART_CLUTTER, CHART_INK };
	INT32					x = st_Area.right - f_Chart_Px(st_Dc, 16);
	INT32					nItem;

	f_Chart_Text(st_Dc, st_Area.left + f_Chart_Px(st_Dc, 10), st_Area.top + f_Chart_Px(st_Dc, 12), TA_LEFT, CHART_INK, _T("LLA 전시 (60 s, 600 스캔, 표적 5개)"));
	f_Chart_Text(st_Dc, st_Area.right - f_Chart_Px(st_Dc, 16), st_Area.top + f_Chart_Px(st_Dc, 30), TA_RIGHT, CHART_MUTED, _T("숫자: 표적 번호 (시작 위치)"));

	for (nItem = 3; nItem >= 0; nItem--)
	{
		x = x - st_Dc->GetTextExtent(s_LegendText[nItem], static_cast<INT32>(_tcslen(s_LegendText[nItem]))).cx;
		f_Chart_Text(st_Dc, x, st_Area.top + f_Chart_Px(st_Dc, 12), TA_LEFT, CHART_INK, s_LegendText[nItem]);
		x = x - f_Chart_Px(st_Dc, 12);
		st_Dc->FillSolidRect(x, st_Area.top + f_Chart_Px(st_Dc, 8), f_Chart_Px(st_Dc, 8), f_Chart_Px(st_Dc, 8), s_LegendColor[nItem]);
		x = x - f_Chart_Px(st_Dc, 14);
	}
}

// 히스토그램: 막대 = 표본 PDF, 주황 선 = 이론 PDF (균등은 1, 정규는 표준정규 곡선).
VOID CChartView::f_DrawHist(CDC *st_Dc, const CRect &st_Area) const
{
	const CRect		st_Plot(st_Area.left + f_Chart_Px(st_Dc, 46), st_Area.top + f_Chart_Px(st_Dc, 50), st_Area.right - f_Chart_Px(st_Dc, 14),
						st_Area.bottom - f_Chart_Px(st_Dc, 28));
	const FLOAT64	xMin = st_Hist->binMin;
	const FLOAT64	xMax = st_Hist->binMin + (st_Hist->nBin * st_Hist->binWidth);
	FLOAT64			yMax = (isGauss != 0) ? (1.0 / sqrt(2.0 * PI)) : 1.0;
	CString			st_Text;
	INT32			nIndex;

	for (nIndex = 0; nIndex < st_Hist->nBin; nIndex++)
	{
		yMax = fmax(yMax, st_Hist->pdf[nIndex]);
	}

	const FLOAT64 yStep = f_Chart_Step(yMax * 1.15, 5);

	yMax = ceil((yMax * 1.15) / yStep) * yStep;

	auto f_ToX = [&](FLOAT64 x)
	{
		return st_Plot.left + static_cast<INT32>(lround(((x - xMin) / (xMax - xMin)) * st_Plot.Width()));
	};
	auto f_ToY = [&](FLOAT64 y)
	{
		return st_Plot.bottom - static_cast<INT32>(lround((y / yMax) * st_Plot.Height()));
	};

	// 제목과 통계
	if (isGauss != 0)
	{
		f_Chart_Text(st_Dc, st_Area.left + f_Chart_Px(st_Dc, 10), st_Area.top + f_Chart_Px(st_Dc, 12), TA_LEFT, CHART_INK, _T("GAUSS(0, 1) 정규난수 10,000개"));
		st_Text.Format(_T("평균 %+.4f, 표준편차 %.4f  (이론 0, 1)"), st_Hist->mean, st_Hist->std);
	}
	else
	{
		f_Chart_Text(st_Dc, st_Area.left + f_Chart_Px(st_Dc, 10), st_Area.top + f_Chart_Px(st_Dc, 12), TA_LEFT, CHART_INK, _T("UNIRAN 균등난수 10,000개"));
		st_Text.Format(_T("평균 %.4f, 표준편차 %.4f  (이론 0.5, %.4f)"), st_Hist->mean, st_Hist->std, 1.0 / sqrt(12.0));
	}

	f_Chart_Text(st_Dc, st_Area.left + f_Chart_Px(st_Dc, 10), st_Area.top + f_Chart_Px(st_Dc, 30), TA_LEFT, CHART_MUTED, st_Text);

	// 가로 격자, 세로축 글자
	CPen	st_GridPen(PS_SOLID, 1, CHART_GRID);
	CPen	*st_OldPen = st_Dc->SelectObject(&st_GridPen);

	for (nIndex = 0; (nIndex * yStep) <= (yMax + (0.5 * yStep)); nIndex++)
	{
		const INT32 y = f_ToY(nIndex * yStep);

		(VOID)st_Dc->MoveTo(st_Plot.left, y);
		(VOID)st_Dc->LineTo(st_Plot.right, y);
		st_Text.Format(_T("%.*f"), f_Chart_Digits(yStep), nIndex * yStep);
		f_Chart_Text(st_Dc, st_Plot.left - f_Chart_Px(st_Dc, 6), y, TA_RIGHT, CHART_MUTED, st_Text);
	}

	// 막대
	for (nIndex = 0; nIndex < st_Hist->nBin; nIndex++)
	{
		const INT32 left = f_ToX(xMin + (nIndex * st_Hist->binWidth));
		const INT32 right = f_ToX(xMin + ((nIndex + 1) * st_Hist->binWidth));
		const INT32 top = f_ToY(st_Hist->pdf[nIndex]);

		st_Dc->FillSolidRect(left + 1, top, right - left - 1, st_Plot.bottom - top, CHART_BAR);
	}

	// 이론 PDF
	CPen st_TheoryPen(PS_SOLID, f_Chart_Px(st_Dc, 2), CHART_PLOT);

	(VOID)st_Dc->SelectObject(&st_TheoryPen);

	if (isGauss != 0)
	{
		for (nIndex = 0; nIndex <= 160; nIndex++)
		{
			const FLOAT64 x = xMin + ((xMax - xMin) * nIndex / 160.0);
			const CPoint st_Point(f_ToX(x), f_ToY(exp(-0.5 * x * x) / sqrt(2.0 * PI)));

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
	const FLOAT64 xStep = f_Chart_Step(xMax - xMin, 8);

	for (nIndex = static_cast<INT32>(ceil(xMin / xStep)); (nIndex * xStep) <= (xMax + (0.5 * xStep)); nIndex++)
	{
		st_Text.Format(_T("%.*f"), f_Chart_Digits(xStep), nIndex * xStep);
		f_Chart_Text(st_Dc, f_ToX(nIndex * xStep), st_Plot.bottom + f_Chart_Px(st_Dc, 10), TA_CENTER, CHART_MUTED, st_Text);
	}
}
