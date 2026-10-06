#include "pch.h"

#include <math.h>

#include <algorithm>
#include <cmath>
namespace Gdiplus
{
	using std::min;
	using std::max;
}
#pragma warning(push, 3)
#include <gdiplus.h>
#pragma warning(pop)
#pragma comment(lib, "gdiplus.lib")

#include "ChartView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// 크기는 96 DPI 기준 px
#define CHART_FONT_PX			12.0F
#define CHART_TITLE_PX			13.0F
#define CHART_PAD				12.0F
#define CHART_TITLE_HEIGHT		30.0F
#define CHART_AXIS_LEFT			48.0F								// 히스토그램 세로축 글자 자리
#define CHART_AXIS_BOTTOM		30.0F
#define CHART_MAP_LEFT			78.0F								// 위도 글자 자리
#define CHART_DOT_PLOT			2.6F								// 플롯 점 반지름
#define CHART_DOT_CLUTTER		1.9F								// 클러터 점 반지름
#define CHART_MAP_MIN_SPAN		0.002								// [deg] 지도 최소 폭
#define CHART_MAP_PAD_RATIO		0.08								// 지도 바깥 여유
#define CHART_MAP_MAX_LAT		89.0								// [deg] cos(위도) 계산용 한계
#define CHART_CURVE_POINT_NUM	201									// 표준정규 곡선 점 수
#define CHART_TICK_MAX_NUM		64									// 축 하나 눈금 최대
#define CHART_TICK_EPS			1.0e-6								// 끝 눈금 반올림 여유 (간격 대비)

// COLORREF + 알파 -> GDI+ 색.
static Gdiplus::Color f_Chart_Color(COLORREF color, INT32 alpha)
{
	return Gdiplus::Color(static_cast<BYTE>(alpha), GetRValue(color), GetGValue(color), GetBValue(color));
}

// {1, 2, 5} x 10^k 중 눈금이 maxTickNum 개 이하가 되는 가장 작은 간격.
static FLOAT64 f_Chart_NiceStep(FLOAT64 span, FLOAT64 maxTickNum)
{
	static const FLOAT64	s_Mantissa[3] = { 1.0, 2.0, 5.0 };
	FLOAT64					decade;
	FLOAT64					step;
	INT32					nIndex = 0;

	if (!(span > 0.0) || !std::isfinite(span))
	{
		span = 1.0;
	}

	if (!(maxTickNum >= 1.0))
	{
		maxTickNum = 1.0;
	}

	decade = pow(10.0, floor(log10(span / maxTickNum)) - 1.0);

	// 값이 너무 작거나 커서 10^k 를 못 구하면 구간 전체를 한 칸으로
	if (!(decade > 0.0) || !std::isfinite(decade))
	{
		return span;
	}

	while ((span / (decade * s_Mantissa[nIndex])) > maxTickNum)
	{
		nIndex = nIndex + 1;

		if (nIndex == 3)
		{
			nIndex	= 0;
			decade	= decade * 10.0;
		}
	}

	step = decade * s_Mantissa[nIndex];

	return std::isfinite(step) ? step : span;
}

// [lo, hi] 안의 step 배수 눈금 개수. 첫 눈금은 *pt_First, i 번째는 *pt_First + i * step.
// 누적 덧셈을 안 하므로 값이 커도 반복이 끝남.
static INT32 f_Chart_TickNum(FLOAT64 lo, FLOAT64 hi, FLOAT64 step, FLOAT64 *pt_First)
{
	FLOAT64 first;
	FLOAT64 count;

	*pt_First = 0.0;

	if (!std::isfinite(lo) || !std::isfinite(hi) || !std::isfinite(step) || !(step > 0.0) || !(hi >= lo))
	{
		return 0;
	}

	first = ceil((lo / step) - CHART_TICK_EPS) * step;
	count = floor(((hi - first) / step) + CHART_TICK_EPS) + 1.0;

	if (!std::isfinite(first) || !std::isfinite(count) || !(count >= 1.0))
	{
		return 0;
	}

	*pt_First = first;

	return static_cast<INT32>(fmin(count, static_cast<FLOAT64>(CHART_TICK_MAX_NUM)));
}

// 눈금 글자. 간격에 맞춘 소수 자릿수. 아주 크거나 작은 값은 지수 표기.
static CString f_Chart_TickText(FLOAT64 value, FLOAT64 step)
{
	CString	st_Text;
	INT32	nDecimal = 0;

	// -0.0 방지
	if (fabs(value) < (0.001 * step))
	{
		value = 0.0;
	}

	if ((step >= 1.0e-6) && (fabs(value) < 1.0e9))
	{
		if (step < 1.0)
		{
			nDecimal = static_cast<INT32>(ceil(-log10(step) - 1.0e-9));
		}

		st_Text.Format(_T("%.*f"), nDecimal, value);
	}
	else
	{
		st_Text.Format(_T("%.6g"), value);
	}

	return st_Text;
}

// 기준점 정렬 글자. Near = 왼쪽(위), Center = 가운데, Far = 오른쪽(아래).
static VOID f_Chart_Text(Gdiplus::Graphics *st_Graphics, const Gdiplus::Font *st_Font, LPCTSTR pt_Text, Gdiplus::REAL x, Gdiplus::REAL y,
	Gdiplus::StringAlignment alignX, Gdiplus::StringAlignment alignY, COLORREF color)
{
	Gdiplus::StringFormat	st_Format;
	Gdiplus::SolidBrush		st_Brush(f_Chart_Color(color, 255));

	(VOID)st_Format.SetAlignment(alignX);
	(VOID)st_Format.SetLineAlignment(alignY);
	(VOID)st_Format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
	(VOID)st_Graphics->DrawString(pt_Text, -1, st_Font, Gdiplus::PointF(x, y), &st_Format, &st_Brush);
}

// 바탕 상자 있는 글자 (왼쪽 위 기준).
static VOID f_Chart_BoxText(Gdiplus::Graphics *st_Graphics, const Gdiplus::Font *st_Font, LPCTSTR pt_Text, Gdiplus::REAL x, Gdiplus::REAL y,
	Gdiplus::REAL pad, COLORREF color)
{
	Gdiplus::StringFormat	st_Format;
	Gdiplus::RectF			st_Box;
	Gdiplus::SolidBrush		st_Back(f_Chart_Color(UI_COLOR_SURFACE, 235));
	Gdiplus::Pen			st_Border(f_Chart_Color(UI_COLOR_GRID, 255), 1.0F);

	(VOID)st_Format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
	(VOID)st_Graphics->MeasureString(pt_Text, -1, st_Font, Gdiplus::PointF(x + pad, y + pad), &st_Format, &st_Box);
	(VOID)st_Graphics->FillRectangle(&st_Back, x, y, st_Box.Width + (2.0F * pad), st_Box.Height + (2.0F * pad));
	(VOID)st_Graphics->DrawRectangle(&st_Border, x, y, st_Box.Width + (2.0F * pad), st_Box.Height + (2.0F * pad));
	f_Chart_Text(st_Graphics, st_Font, pt_Text, x + pad, y + pad, Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentNear, color);
}

static FLOAT64 f_Chart_NormalPdf(FLOAT64 x)
{
	return exp(-0.5 * x * x) / sqrt(2.0 * PI);
}

// 점의 위도, 경도 [deg]. 값이 유한하지 않으면 0 (안 그림).
static INT32 f_Chart_PointDeg(const ST_PcsPoint *st_Point, FLOAT64 *pt_LatDeg, FLOAT64 *pt_LonDeg)
{
	*pt_LatDeg = f_Rad_To_Deg(st_Point->st_Lla.Lat);
	*pt_LonDeg = f_Rad_To_Deg(st_Point->st_Lla.Lon);

	return (std::isfinite(*pt_LatDeg) && std::isfinite(*pt_LonDeg)) ? 1 : 0;
}

BEGIN_MESSAGE_MAP(CChartView, CStatic)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
END_MESSAGE_MAP()

CChartView::CChartView() noexcept
	: st_Data(nullptr)
	, dpi(UI_BASE_DPI)
	, nView(CHART_VIEW_MAP)
{
}

VOID CChartView::f_SetDpi(INT32 newDpi)
{
	dpi = newDpi;
}

// 자료 교체 후 다시 그림.
VOID CChartView::f_SetData(const ST_ChartData *st_NewData)
{
	st_Data = st_NewData;
	Invalidate(FALSE);
}

VOID CChartView::f_SetView(INT32 nNewView)
{
	nView = nNewView;
	Invalidate(FALSE);
}

// 96 DPI 기준 px -> 현재 px.
FLOAT32 CChartView::f_Px(FLOAT32 pixel) const
{
	return (pixel * static_cast<FLOAT32>(dpi)) / static_cast<FLOAT32>(UI_BASE_DPI);
}

// 바탕은 OnPaint 에서 칠함.
BOOL CChartView::OnEraseBkgnd(CDC *st_Dc)
{
	UNREFERENCED_PARAMETER(st_Dc);

	return TRUE;
}

VOID CChartView::OnSize(UINT32 type, INT32 width, INT32 height)
{
	CStatic::OnSize(type, width, height);
	Invalidate(FALSE);
}

// 메모리 DC 에 그린 뒤 한 번에 복사 (깜박임 방지).
VOID CChartView::OnPaint(VOID)
{
	CPaintDC	st_Dc(this);
	CDC			st_MemDc;
	CBitmap		st_Bitmap;
	CBitmap		*st_OldBitmap;
	CRect		st_Client;

	GetClientRect(&st_Client);

	if ((st_Client.Width() > 0) && (st_Client.Height() > 0))
	{
		(VOID)st_MemDc.CreateCompatibleDC(&st_Dc);
		(VOID)st_Bitmap.CreateCompatibleBitmap(&st_Dc, st_Client.Width(), st_Client.Height());
		st_OldBitmap = st_MemDc.SelectObject(&st_Bitmap);
		st_MemDc.FillSolidRect(&st_Client, UI_COLOR_SURFACE);

		{
			Gdiplus::Graphics		st_Graphics(st_MemDc.GetSafeHdc());
			const Gdiplus::RectF	st_Area(0.0F, 0.0F, static_cast<Gdiplus::REAL>(st_Client.Width()), static_cast<Gdiplus::REAL>(st_Client.Height()));

			(VOID)st_Graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

			if (st_Data == nullptr)
			{
				f_DrawMessage(&st_Graphics, st_Area, _T("실행 결과 없음"));
			}
			else if (nView == CHART_VIEW_RANDOM)
			{
				f_DrawRandom(&st_Graphics, st_Area);
			}
			else
			{
				f_DrawMap(&st_Graphics, st_Area);
			}
		}

		(VOID)st_Dc.BitBlt(0, 0, st_Client.Width(), st_Client.Height(), &st_MemDc, 0, 0, SRCCOPY);
		(VOID)st_MemDc.SelectObject(st_OldBitmap);
	}
}

// 가운데 안내 글자. 폭을 넘으면 줄바꿈.
VOID CChartView::f_DrawMessage(Gdiplus::Graphics *st_Graphics, const Gdiplus::RectF &st_Area, LPCTSTR pt_Text) const
{
	const Gdiplus::FontFamily	st_Family(L"Malgun Gothic");
	const Gdiplus::Font			st_Font(&st_Family, f_Px(CHART_FONT_PX), Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	const Gdiplus::RectF		st_Layout(st_Area.X + f_Px(16.0F), st_Area.Y, st_Area.Width - f_Px(32.0F), st_Area.Height);
	Gdiplus::StringFormat		st_Format;
	Gdiplus::SolidBrush			st_Brush(f_Chart_Color(UI_COLOR_MUTED, 255));

	(VOID)st_Format.SetAlignment(Gdiplus::StringAlignmentCenter);
	(VOID)st_Format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
	(VOID)st_Graphics->DrawString(pt_Text, -1, &st_Font, st_Layout, &st_Format, &st_Brush);
}

// 난수 검증 보기: 왼쪽 UNIRAN, 오른쪽 GAUSS. 세로가 더 길면 위아래.
VOID CChartView::f_DrawRandom(Gdiplus::Graphics *st_Graphics, const Gdiplus::RectF &st_Area) const
{
	Gdiplus::RectF	st_First(st_Area);
	Gdiplus::RectF	st_Second(st_Area);
	CString			st_Title;

	if (st_Area.Width >= st_Area.Height)
	{
		st_First.Width	= 0.5F * st_Area.Width;
		st_Second.X		= st_Area.X + st_First.Width;
		st_Second.Width	= st_Area.Width - st_First.Width;
	}
	else
	{
		st_First.Height		= 0.5F * st_Area.Height;
		st_Second.Y			= st_Area.Y + st_First.Height;
		st_Second.Height	= st_Area.Height - st_First.Height;
	}

	st_Title.Format(_T("UNIRAN 균등난수 %d개"), st_Data->st_Uniform.nSample);
	f_DrawHist(st_Graphics, st_First, &st_Data->st_Uniform, st_Title, 0);
	st_Title.Format(_T("GAUSS(0, 1) 정규난수 %d개"), st_Data->st_Gauss.nSample);
	f_DrawHist(st_Graphics, st_Second, &st_Data->st_Gauss, st_Title, 1);
}

// 히스토그램 한 칸. 막대 = 표본 PDF, 선 = 이론 PDF (균등은 1, 정규는 표준정규 곡선).
VOID CChartView::f_DrawHist(Gdiplus::Graphics *st_Graphics, const Gdiplus::RectF &st_Area, const ST_PcsHist *st_Hist, LPCTSTR pt_Title,
	INT32 isGauss) const
{
	const Gdiplus::FontFamily	st_Family(L"Malgun Gothic");
	const Gdiplus::Font			st_Font(&st_Family, f_Px(CHART_FONT_PX), Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	const Gdiplus::Font			st_Bold(&st_Family, f_Px(CHART_TITLE_PX), Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
	Gdiplus::Pen				st_GridPen(f_Chart_Color(UI_COLOR_GRID, 255), 1.0F);
	Gdiplus::Pen				st_FramePen(f_Chart_Color(UI_COLOR_AXIS, 255), 1.0F);
	Gdiplus::Pen				st_RefPen(f_Chart_Color(UI_COLOR_INK_SUB, 255), f_Px(1.5F));
	Gdiplus::Pen				st_CurvePen(f_Chart_Color(UI_COLOR_PLOT, 255), f_Px(2.0F));
	Gdiplus::SolidBrush			st_BarBrush(f_Chart_Color(UI_COLOR_TRUE, 255));
	const Gdiplus::RectF		st_Plot(st_Area.X + f_Px(CHART_AXIS_LEFT), st_Area.Y + f_Px(CHART_TITLE_HEIGHT),
									st_Area.Width - f_Px(CHART_AXIS_LEFT) - f_Px(CHART_PAD), st_Area.Height - f_Px(CHART_TITLE_HEIGHT) - f_Px(CHART_AXIS_BOTTOM));
	std::vector<Gdiplus::PointF>	st_Curve;
	CString						st_Text;
	FLOAT64						yMax;
	FLOAT64						yStep;
	FLOAT64						yTop;
	FLOAT64						xStep;
	FLOAT64						first;
	FLOAT64						value;
	FLOAT64						x0;
	FLOAT64						x1;
	Gdiplus::REAL				px;
	Gdiplus::REAL				py;
	Gdiplus::REAL				gap;
	INT32						nTickNum;
	INT32						nIndex;

	f_Chart_Text(st_Graphics, &st_Bold, pt_Title, st_Area.X + f_Px(CHART_PAD), st_Area.Y + f_Px(8.0F), Gdiplus::StringAlignmentNear,
		Gdiplus::StringAlignmentNear, UI_COLOR_INK);

	if ((st_Plot.Width < f_Px(60.0F)) || (st_Plot.Height < f_Px(60.0F)))
	{
		return;
	}

	(VOID)st_Graphics->SetSmoothingMode(Gdiplus::SmoothingModeNone);

	if ((st_Hist->nBin < 1) || (st_Hist->nBin > PCS_HIST_MAX_BIN) || !(st_Hist->binMax > st_Hist->binMin)
		|| !std::isfinite(st_Hist->binMax - st_Hist->binMin) || !std::isfinite(st_Hist->binWidth) || !(st_Hist->binWidth > 0.0))
	{
		(VOID)st_Graphics->DrawRectangle(&st_FramePen, st_Plot);
		f_DrawMessage(st_Graphics, st_Plot, _T("결과 없음"));
		return;
	}

	// 세로축: 이론 PDF 최대와 막대 최대 중 큰 쪽 + 15 %
	yMax = (isGauss != 0) ? f_Chart_NormalPdf(0.0) : 1.0;

	for (nIndex = 0; nIndex < st_Hist->nBin; nIndex++)
	{
		if (std::isfinite(st_Hist->pdf[nIndex]) && (st_Hist->pdf[nIndex] > yMax))
		{
			yMax = st_Hist->pdf[nIndex];
		}
	}

	yMax	= yMax * 1.15;
	yStep	= f_Chart_NiceStep(yMax, 5.0);
	yTop	= ceil(yMax / yStep) * yStep;
	xStep	= f_Chart_NiceStep(st_Hist->binMax - st_Hist->binMin, 8.0);

	if (!std::isfinite(yTop) || !(yTop > 0.0))
	{
		yStep	= 0.2;
		yTop	= 1.0;
	}

	auto f_MapX = [&](FLOAT64 x) -> Gdiplus::REAL
	{
		return st_Plot.X + static_cast<Gdiplus::REAL>(((x - st_Hist->binMin) / (st_Hist->binMax - st_Hist->binMin)) * static_cast<FLOAT64>(st_Plot.Width));
	};
	auto f_MapY = [&](FLOAT64 y) -> Gdiplus::REAL
	{
		return st_Plot.GetBottom() - static_cast<Gdiplus::REAL>((y / yTop) * static_cast<FLOAT64>(st_Plot.Height));
	};

	// 가로 격자, 세로축 눈금
	nTickNum = f_Chart_TickNum(0.0, yTop, yStep, &first);

	for (nIndex = 0; nIndex < nTickNum; nIndex++)
	{
		value	= first + (static_cast<FLOAT64>(nIndex) * yStep);
		py		= f_MapY(value);
		(VOID)st_Graphics->DrawLine(&st_GridPen, st_Plot.X, py, st_Plot.GetRight(), py);
		f_Chart_Text(st_Graphics, &st_Font, f_Chart_TickText(value, yStep), st_Plot.X - f_Px(6.0F), py, Gdiplus::StringAlignmentFar,
			Gdiplus::StringAlignmentCenter, UI_COLOR_MUTED);
	}

	// 가로축 눈금
	nTickNum = f_Chart_TickNum(st_Hist->binMin, st_Hist->binMax, xStep, &first);

	for (nIndex = 0; nIndex < nTickNum; nIndex++)
	{
		value	= first + (static_cast<FLOAT64>(nIndex) * xStep);
		px		= f_MapX(value);
		(VOID)st_Graphics->DrawLine(&st_GridPen, px, st_Plot.Y, px, st_Plot.GetBottom());
		f_Chart_Text(st_Graphics, &st_Font, f_Chart_TickText(value, xStep), px, st_Plot.GetBottom() + f_Px(5.0F), Gdiplus::StringAlignmentCenter,
			Gdiplus::StringAlignmentNear, UI_COLOR_MUTED);
	}

	// 막대 (bin 사이 1 px 틈)
	gap = (st_Plot.Width / static_cast<Gdiplus::REAL>(st_Hist->nBin) > f_Px(6.0F)) ? f_Px(1.0F) : 0.0F;
	(VOID)st_Graphics->SetClip(st_Plot);

	for (nIndex = 0; nIndex < st_Hist->nBin; nIndex++)
	{
		value	= st_Hist->pdf[nIndex];
		x0		= st_Hist->binMin + (st_Hist->binWidth * static_cast<FLOAT64>(nIndex));
		x1		= x0 + st_Hist->binWidth;

		if (std::isfinite(value) && (value > 0.0) && (x1 > st_Hist->binMin) && (x0 < st_Hist->binMax))
		{
			x0 = fmax(x0, st_Hist->binMin);
			x1 = fmin(x1, st_Hist->binMax);
			px = f_MapX(x0) + gap;
			py = f_MapY(fmin(value, yTop));
			(VOID)st_Graphics->FillRectangle(&st_BarBrush, px, py, (std::max)(f_MapX(x1) - gap - px, 1.0F), st_Plot.GetBottom() - py);
		}
	}

	(VOID)st_Graphics->ResetClip();
	(VOID)st_Graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

	// 이론 PDF
	if (isGauss == 0)
	{
		py = f_MapY(1.0);
		(VOID)st_Graphics->DrawLine(&st_RefPen, st_Plot.X, py, st_Plot.GetRight(), py);
	}
	else
	{
		for (nIndex = 0; nIndex < CHART_CURVE_POINT_NUM; nIndex++)
		{
			value = st_Hist->binMin + (((st_Hist->binMax - st_Hist->binMin) * static_cast<FLOAT64>(nIndex)) / static_cast<FLOAT64>(CHART_CURVE_POINT_NUM - 1));
			st_Curve.push_back(Gdiplus::PointF(f_MapX(value), f_MapY(f_Chart_NormalPdf(value))));
		}

		(VOID)st_Graphics->DrawLines(&st_CurvePen, st_Curve.data(), static_cast<INT32>(st_Curve.size()));
	}

	(VOID)st_Graphics->SetSmoothingMode(Gdiplus::SmoothingModeNone);
	(VOID)st_Graphics->DrawRectangle(&st_FramePen, st_Plot);

	// 통계 (왼쪽 위)
	if (isGauss == 0)
	{
		st_Text.Format(_T("평균 %.4f   표준편차 %.4f\n최솟값 %.6f   최댓값 %.6f\n막대: 표본 PDF,  검은 선: 이론 PDF 1"), st_Hist->mean, st_Hist->std,
			st_Hist->minValue, st_Hist->maxValue);
	}
	else
	{
		st_Text.Format(_T("평균 %+.4f   표준편차 %.4f\n막대: 표본 PDF,  주황 곡선: 표준정규 PDF"), st_Hist->mean, st_Hist->std);
	}

	f_Chart_BoxText(st_Graphics, &st_Font, st_Text, st_Plot.X + f_Px(6.0F), st_Plot.Y + f_Px(6.0F), f_Px(4.0F), UI_COLOR_INK);
}

// LLA 전시: 위도-경도 평면에 참값(선), 플롯(주황 점), 클러터(초록 작은 점), 레이다(삼각형).
VOID CChartView::f_DrawMap(Gdiplus::Graphics *st_Graphics, const Gdiplus::RectF &st_Area) const
{
	const Gdiplus::FontFamily	st_Family(L"Malgun Gothic");
	const Gdiplus::Font			st_Font(&st_Family, f_Px(CHART_FONT_PX), Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	const Gdiplus::Font			st_Bold(&st_Family, f_Px(CHART_TITLE_PX), Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
	Gdiplus::Pen				st_GridPen(f_Chart_Color(UI_COLOR_GRID, 255), 1.0F);
	Gdiplus::Pen				st_FramePen(f_Chart_Color(UI_COLOR_AXIS, 255), 1.0F);
	Gdiplus::Pen				st_TruePen(f_Chart_Color(UI_COLOR_TRUE, 255), f_Px(2.0F));
	Gdiplus::Pen				st_RingPen(f_Chart_Color(UI_COLOR_SURFACE, 255), f_Px(1.5F));
	Gdiplus::Pen				st_CasingPen(f_Chart_Color(UI_COLOR_SURFACE, 255), f_Px(5.0F));
	Gdiplus::SolidBrush			st_PlotBrush(f_Chart_Color(UI_COLOR_PLOT, 230));
	Gdiplus::SolidBrush			st_ClutterBrush(f_Chart_Color(UI_COLOR_CLUTTER, 170));
	Gdiplus::SolidBrush			st_TrueBrush(f_Chart_Color(UI_COLOR_TRUE, 255));
	Gdiplus::SolidBrush			st_InkBrush(f_Chart_Color(UI_COLOR_INK, 255));
	const Gdiplus::RectF		st_Plot(st_Area.X + f_Px(CHART_MAP_LEFT), st_Area.Y + f_Px(CHART_TITLE_HEIGHT),
									st_Area.Width - f_Px(CHART_MAP_LEFT) - f_Px(CHART_PAD), st_Area.Height - f_Px(CHART_TITLE_HEIGHT) - f_Px(CHART_AXIS_BOTTOM));
	const FLOAT64				radarLat = f_Rad_To_Deg(st_Data->st_RadarLla.Lat);
	const FLOAT64				radarLon = f_Rad_To_Deg(st_Data->st_RadarLla.Lon);
	const Gdiplus::REAL			plotRadius = f_Px(CHART_DOT_PLOT);
	const Gdiplus::REAL			clutterRadius = f_Px(CHART_DOT_CLUTTER);
	std::vector<Gdiplus::PointF>	st_Track;
	Gdiplus::PointF				st_Triangle[3];
	CString						st_Text;
	FLOAT64						minLat = radarLat;
	FLOAT64						maxLat = radarLat;
	FLOAT64						minLon = radarLon;
	FLOAT64						maxLon = radarLon;
	FLOAT64						lat0;
	FLOAT64						lon0;
	FLOAT64						cosLat;
	FLOAT64						spanX;
	FLOAT64						spanY;
	FLOAT64						scale;
	FLOAT64						latHalf;
	FLOAT64						lonHalf;
	FLOAT64						latStep;
	FLOAT64						lonStep;
	FLOAT64						first;
	FLOAT64						value;
	FLOAT64						lat;
	FLOAT64						lon;
	Gdiplus::REAL				px;
	Gdiplus::REAL				py;
	Gdiplus::REAL				legendY;
	INT32						nTickNum;
	INT32						nIndex;
	INT32						nTarget;

	f_Chart_Text(st_Graphics, &st_Bold, _T("LLA 전시   참값 / 플롯 / 클러터 (전체 스캔)"), st_Area.X + f_Px(CHART_PAD), st_Area.Y + f_Px(8.0F),
		Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentNear, UI_COLOR_INK);

	if ((st_Plot.Width < f_Px(80.0F)) || (st_Plot.Height < f_Px(80.0F)))
	{
		return;
	}

	// 보이는 범위: 레이다와 모든 점
	for (const ST_UiPoint &st_Ui : st_Data->st_Points)
	{
		if (f_Chart_PointDeg(&st_Ui.st_Point, &lat, &lon) != 0)
		{
			minLat	= fmin(minLat, lat);
			maxLat	= fmax(maxLat, lat);
			minLon	= fmin(minLon, lon);
			maxLon	= fmax(maxLon, lon);
		}
	}

	// 등축척: 경도 1도 길이 = 위도 1도 길이 * cos(위도)
	lat0	= 0.5 * (minLat + maxLat);
	lon0	= 0.5 * (minLon + maxLon);
	cosLat	= cos(f_Deg_To_Rad(fmin(fmax(lat0, -CHART_MAP_MAX_LAT), CHART_MAP_MAX_LAT)));
	spanY	= fmax(maxLat - minLat, CHART_MAP_MIN_SPAN) * (1.0 + (2.0 * CHART_MAP_PAD_RATIO));
	spanX	= fmax((maxLon - minLon) * cosLat, CHART_MAP_MIN_SPAN) * (1.0 + (2.0 * CHART_MAP_PAD_RATIO));
	scale	= fmin(static_cast<FLOAT64>(st_Plot.Width) / spanX, static_cast<FLOAT64>(st_Plot.Height) / spanY);		// [px / 위도 1도]

	if (!std::isfinite(lat0) || !std::isfinite(lon0) || !std::isfinite(scale) || !(scale > 0.0))
	{
		(VOID)st_Graphics->DrawRectangle(&st_FramePen, st_Plot);
		f_DrawMessage(st_Graphics, st_Plot, _T("위도 / 경도 범위가 너무 커서 그릴 수 없음"));
		return;
	}

	latHalf	= (0.5 * static_cast<FLOAT64>(st_Plot.Height)) / scale;
	lonHalf	= (0.5 * static_cast<FLOAT64>(st_Plot.Width)) / (scale * cosLat);
	latStep	= f_Chart_NiceStep(2.0 * latHalf, fmax(2.0, static_cast<FLOAT64>(st_Plot.Height) / static_cast<FLOAT64>(f_Px(40.0F))));
	lonStep	= f_Chart_NiceStep(2.0 * lonHalf, fmax(2.0, static_cast<FLOAT64>(st_Plot.Width) / static_cast<FLOAT64>(f_Px(90.0F))));

	auto f_MapX = [&](FLOAT64 lonDeg) -> Gdiplus::REAL
	{
		return st_Plot.X + (0.5F * st_Plot.Width) + static_cast<Gdiplus::REAL>((lonDeg - lon0) * cosLat * scale);
	};
	auto f_MapY = [&](FLOAT64 latDeg) -> Gdiplus::REAL
	{
		return st_Plot.Y + (0.5F * st_Plot.Height) - static_cast<Gdiplus::REAL>((latDeg - lat0) * scale);
	};

	(VOID)st_Graphics->SetSmoothingMode(Gdiplus::SmoothingModeNone);

	// 세로 격자 = 경도
	nTickNum = f_Chart_TickNum(lon0 - lonHalf, lon0 + lonHalf, lonStep, &first);

	for (nIndex = 0; nIndex < nTickNum; nIndex++)
	{
		value	= first + (static_cast<FLOAT64>(nIndex) * lonStep);
		px		= f_MapX(value);
		(VOID)st_Graphics->DrawLine(&st_GridPen, px, st_Plot.Y, px, st_Plot.GetBottom());

		// 오른쪽 끝에서 글자가 잘리면 생략
		if ((px + f_Px(28.0F)) <= (st_Area.X + st_Area.Width))
		{
			f_Chart_Text(st_Graphics, &st_Font, f_Chart_TickText(value, lonStep), px, st_Plot.GetBottom() + f_Px(5.0F), Gdiplus::StringAlignmentCenter,
				Gdiplus::StringAlignmentNear, UI_COLOR_MUTED);
		}
	}

	// 가로 격자 = 위도
	nTickNum = f_Chart_TickNum(lat0 - latHalf, lat0 + latHalf, latStep, &first);

	for (nIndex = 0; nIndex < nTickNum; nIndex++)
	{
		value	= first + (static_cast<FLOAT64>(nIndex) * latStep);
		py		= f_MapY(value);
		(VOID)st_Graphics->DrawLine(&st_GridPen, st_Plot.X, py, st_Plot.GetRight(), py);
		f_Chart_Text(st_Graphics, &st_Font, f_Chart_TickText(value, latStep), st_Plot.X - f_Px(6.0F), py, Gdiplus::StringAlignmentFar,
			Gdiplus::StringAlignmentCenter, UI_COLOR_MUTED);
	}

	(VOID)st_Graphics->DrawRectangle(&st_FramePen, st_Plot);
	f_Chart_Text(st_Graphics, &st_Font, _T("경도 [°]"), st_Plot.GetRight() - f_Px(6.0F), st_Plot.GetBottom() - f_Px(4.0F), Gdiplus::StringAlignmentFar,
		Gdiplus::StringAlignmentFar, UI_COLOR_MUTED);
	f_Chart_Text(st_Graphics, &st_Font, _T("위도 [°]"), st_Plot.X + f_Px(6.0F), st_Plot.Y + f_Px(4.0F), Gdiplus::StringAlignmentNear,
		Gdiplus::StringAlignmentNear, UI_COLOR_MUTED);

	(VOID)st_Graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	(VOID)st_Graphics->SetClip(st_Plot);

	// 클러터 (맨 아래)
	for (const ST_UiPoint &st_Ui : st_Data->st_Points)
	{
		if ((st_Ui.st_Point.enKind == PCS_KIND_CLUTTER) && (f_Chart_PointDeg(&st_Ui.st_Point, &lat, &lon) != 0))
		{
			px = f_MapX(lon);
			py = f_MapY(lat);
			(VOID)st_Graphics->FillEllipse(&st_ClutterBrush, px - clutterRadius, py - clutterRadius, 2.0F * clutterRadius, 2.0F * clutterRadius);
		}
	}

	// 플롯
	for (const ST_UiPoint &st_Ui : st_Data->st_Points)
	{
		if ((st_Ui.st_Point.enKind == PCS_KIND_PLOT) && (f_Chart_PointDeg(&st_Ui.st_Point, &lat, &lon) != 0))
		{
			px = f_MapX(lon);
			py = f_MapY(lat);
			(VOID)st_Graphics->FillEllipse(&st_PlotBrush, px - plotRadius, py - plotRadius, 2.0F * plotRadius, 2.0F * plotRadius);
		}
	}

	// 참값: 표적마다 선, 시작점. 바탕색 테두리를 먼저 그려 플롯 위에서도 보이게 함
	for (nTarget = 1; nTarget <= st_Data->nTargetNum; nTarget++)
	{
		st_Track.clear();

		for (const ST_UiPoint &st_Ui : st_Data->st_Points)
		{
			if ((st_Ui.st_Point.enKind == PCS_KIND_TRUE) && (st_Ui.st_Point.nId == nTarget) && (f_Chart_PointDeg(&st_Ui.st_Point, &lat, &lon) != 0))
			{
				st_Track.push_back(Gdiplus::PointF(f_MapX(lon), f_MapY(lat)));
			}
		}

		if (st_Track.size() >= 2U)
		{
			(VOID)st_Graphics->DrawLines(&st_CasingPen, st_Track.data(), static_cast<INT32>(st_Track.size()));
			(VOID)st_Graphics->DrawLines(&st_TruePen, st_Track.data(), static_cast<INT32>(st_Track.size()));
		}

		if (!st_Track.empty())
		{
			(VOID)st_Graphics->FillEllipse(&st_TrueBrush, st_Track[0].X - f_Px(4.0F), st_Track[0].Y - f_Px(4.0F), f_Px(8.0F), f_Px(8.0F));
			(VOID)st_Graphics->DrawEllipse(&st_RingPen, st_Track[0].X - f_Px(4.0F), st_Track[0].Y - f_Px(4.0F), f_Px(8.0F), f_Px(8.0F));
			st_Text.Format(_T("표적 %d"), nTarget);
			f_Chart_Text(st_Graphics, &st_Font, st_Text, st_Track[0].X + f_Px(8.0F), st_Track[0].Y, Gdiplus::StringAlignmentNear,
				Gdiplus::StringAlignmentCenter, UI_COLOR_INK_SUB);
		}
	}

	// 레이다 (삼각형)
	px = f_MapX(radarLon);
	py = f_MapY(radarLat);
	st_Triangle[0] = Gdiplus::PointF(px, py - f_Px(7.0F));
	st_Triangle[1] = Gdiplus::PointF(px - f_Px(6.0F), py + f_Px(5.0F));
	st_Triangle[2] = Gdiplus::PointF(px + f_Px(6.0F), py + f_Px(5.0F));
	(VOID)st_Graphics->FillPolygon(&st_InkBrush, st_Triangle, 3);
	(VOID)st_Graphics->DrawPolygon(&st_RingPen, st_Triangle, 3);
	(VOID)st_Graphics->ResetClip();

	// 범례 (오른쪽 위)
	{
		const Gdiplus::REAL		rowHeight = f_Px(18.0F);
		const Gdiplus::REAL		boxWidth = f_Px(150.0F);
		const Gdiplus::REAL		boxX = st_Plot.GetRight() - boxWidth - f_Px(8.0F);
		const Gdiplus::REAL		markX = boxX + f_Px(16.0F);
		const Gdiplus::REAL		textX = boxX + f_Px(30.0F);
		Gdiplus::SolidBrush		st_Back(f_Chart_Color(UI_COLOR_SURFACE, 235));
		Gdiplus::Pen			st_Border(f_Chart_Color(UI_COLOR_GRID, 255), 1.0F);

		legendY = st_Plot.Y + f_Px(8.0F);
		(VOID)st_Graphics->FillRectangle(&st_Back, boxX, legendY, boxWidth, (4.0F * rowHeight) + f_Px(10.0F));
		(VOID)st_Graphics->DrawRectangle(&st_Border, boxX, legendY, boxWidth, (4.0F * rowHeight) + f_Px(10.0F));
		legendY = legendY + f_Px(5.0F) + (0.5F * rowHeight);

		(VOID)st_Graphics->DrawLine(&st_TruePen, markX - f_Px(8.0F), legendY, markX + f_Px(8.0F), legendY);
		st_Text.Format(_T("참값 (표적 %d개)"), st_Data->nTargetNum);
		f_Chart_Text(st_Graphics, &st_Font, st_Text, textX, legendY, Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter, UI_COLOR_INK);
		legendY = legendY + rowHeight;

		(VOID)st_Graphics->FillEllipse(&st_PlotBrush, markX - plotRadius, legendY - plotRadius, 2.0F * plotRadius, 2.0F * plotRadius);
		st_Text.Format(_T("플롯 %d개"), st_Data->nPlotNum);
		f_Chart_Text(st_Graphics, &st_Font, st_Text, textX, legendY, Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter, UI_COLOR_INK);
		legendY = legendY + rowHeight;

		(VOID)st_Graphics->FillEllipse(&st_ClutterBrush, markX - clutterRadius, legendY - clutterRadius, 2.0F * clutterRadius, 2.0F * clutterRadius);
		st_Text.Format(_T("클러터 %d개"), st_Data->nClutterNum);
		f_Chart_Text(st_Graphics, &st_Font, st_Text, textX, legendY, Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter, UI_COLOR_INK);
		legendY = legendY + rowHeight;

		st_Triangle[0] = Gdiplus::PointF(markX, legendY - f_Px(6.0F));
		st_Triangle[1] = Gdiplus::PointF(markX - f_Px(5.0F), legendY + f_Px(4.0F));
		st_Triangle[2] = Gdiplus::PointF(markX + f_Px(5.0F), legendY + f_Px(4.0F));
		(VOID)st_Graphics->FillPolygon(&st_InkBrush, st_Triangle, 3);
		f_Chart_Text(st_Graphics, &st_Font, _T("레이다"), textX, legendY, Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter, UI_COLOR_INK);
	}
}
