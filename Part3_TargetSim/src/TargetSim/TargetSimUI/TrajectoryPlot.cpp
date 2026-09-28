#include "pch.h"

#include <math.h>

#include <algorithm>
namespace Gdiplus
{
	using std::min;
	using std::max;
}
#pragma warning(push, 3)
#include <gdiplus.h>
#pragma warning(pop)
#pragma comment(lib, "gdiplus.lib")

#include "TrajectoryPlot.h"
#include "UiCommon.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define PLOT_MARGIN_LEFT		74						// [px @96dpi] 위도 라벨 자리
#define PLOT_MARGIN_RIGHT		16
#define PLOT_TITLE_HEIGHT		26
#define PLOT_AXIS_HEIGHT		36
#define PLOT_ALT_TITLE_HEIGHT	22
#define PLOT_ALT_MIN_HEIGHT		112
#define PLOT_ALT_MAX_HEIGHT		190
#define PLOT_ALT_RATIO			0.30
#define PLOT_WIDE_RATIO			1.40					// 너비/높이가 이보다 크면 평면과 고도를 나란히
#define PLOT_ALT_SIDE_RATIO		0.38
#define PLOT_ALT_SIDE_MIN		250
#define PLOT_MIN_SPAN			1000.0					// [m]
#define PLOT_SPAN_SCALE			1.18
#define PLOT_MIN_ALT_SPAN		10.0					// [m]
#define PLOT_MAX_GRID_NUM		8.0
#define PLOT_LON_LABEL_PX		78						// [px @96dpi] 경도 라벨 하나가 차지하는 폭
#define PLOT_LAT_LABEL_PX		34						// [px @96dpi] 위도 라벨 하나가 차지하는 높이
#define PLOT_FONT_PX			12.0F
#define PLOT_LABEL_GAP_PX		8.0						// [px @96dpi] 시작점과 이름표 사이
#define PLOT_LABEL_SIDE_MIN		0.3

// COLORREF 를 알파를 붙인 GDI+ 색으로 바꾼다.
static Gdiplus::Color f_Plot_Color(COLORREF color, INT32 alpha)
{
	return Gdiplus::Color(static_cast<UINT8>(alpha), GetRValue(color), GetGValue(color), GetBValue(color));
}

// 그리기 좌표를 정수 픽셀로 반올림한다. 격자선을 또렷하게 긋는 데 쓴다.
static INT32 f_Plot_Round(FLOAT64 value)
{
	return static_cast<INT32>(floor(value + 0.5));
}

// 가수 x 10^k 를 작은 것부터 올려 가며, 눈금이 maxTickNum 개 이하가 되는 첫 간격을 고른다.
static FLOAT64 f_Plot_NiceStepFrom(FLOAT64 span, FLOAT64 maxTickNum, const FLOAT64 *pt_Mantissa, INT32 nMantissaNum)
{
	FLOAT64	decade = pow(10.0, floor(log10(span / maxTickNum)) - 1.0);
	INT32	nIndex = 0;

	while ((span / (decade * pt_Mantissa[nIndex])) > maxTickNum)
	{
		nIndex = nIndex + 1;

		if (nIndex == nMantissaNum)
		{
			nIndex	= 0;
			decade	= decade * 10.0;
		}
	}

	return decade * pt_Mantissa[nIndex];
}

// {1, 2, 5} x 10^k. 고도와 시각 축이 쓴다.
static FLOAT64 f_Plot_NiceStep(FLOAT64 span, FLOAT64 maxTickNum)
{
	static const FLOAT64 s_Mantissa[3] = { 1.0, 2.0, 5.0 };

	return f_Plot_NiceStepFrom(span, maxTickNum, s_Mantissa, 3);
}

// {1, 2, 4, 5} x 10^k. 위경도 격자는 0.04 도 같은 간격도 쓴다.
static FLOAT64 f_Plot_NiceStepGeo(FLOAT64 span, FLOAT64 maxTickNum)
{
	static const FLOAT64 s_Mantissa[4] = { 1.0, 2.0, 4.0, 5.0 };

	return f_Plot_NiceStepFrom(span, maxTickNum, s_Mantissa, 4);
}

// 눈금 간격에 맞는 소수 자릿수.
static INT32 f_Plot_DecimalFor(FLOAT64 step)
{
	return (step < 1.0) ? static_cast<INT32>(ceil(-log10(step) - 1.0e-9)) : 0;
}

// 눈금 값을 고정 소수 자릿수로 적는다.
static CString f_Plot_Number(FLOAT64 value, INT32 nDecimal)
{
	CString st_Text;

	st_Text.Format(_T("%.*f"), nDecimal, value);

	return st_Text;
}

// 정렬 기준점에 글자를 그린다. Near 는 기준점에서 오른쪽(아래)으로, Far 는 왼쪽(위)으로 쓴다.
static VOID f_Plot_Text(Gdiplus::Graphics *st_Graphics, const Gdiplus::Font *st_Font, const CString &st_Text, FLOAT64 x, FLOAT64 y,
	Gdiplus::StringAlignment alignX, Gdiplus::StringAlignment alignY, COLORREF color)
{
	Gdiplus::StringFormat	st_Format;
	Gdiplus::SolidBrush		st_Brush(f_Plot_Color(color, 255));
	const Gdiplus::PointF	st_Origin(static_cast<FLOAT32>(x), static_cast<FLOAT32>(y));

	(VOID)st_Format.SetAlignment(alignX);
	(VOID)st_Format.SetLineAlignment(alignY);
	(VOID)st_Format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
	(VOID)st_Graphics->DrawString(st_Text.GetString(), -1, st_Font, st_Origin, &st_Format, &st_Brush);
}

// 객체 이름 ("플랫폼" 또는 "표적 k").
static CString f_Plot_ObjectName(INT32 nObject)
{
	CString st_Name;

	if (nObject == 0)
	{
		st_Name = _T("플랫폼");
	}
	else
	{
		st_Name.Format(_T("표적 %d"), nObject);
	}

	return st_Name;
}

BEGIN_MESSAGE_MAP(CTrajectoryPlot, CStatic)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
END_MESSAGE_MAP()

// 궤적 그림. 결과 버퍼는 대화상자가 소유하고 여기서는 읽기만 한다.
CTrajectoryPlot::CTrajectoryPlot() noexcept
	: st_Result(nullptr)
	, dpi(UI_BASE_DPI)
	, pixelPerMeter(1.0)
	, centerEast(0.0)
	, centerNorth(0.0)
	, centerLat(0.0)
	, centerLon(0.0)
	, gridLatDeg(0.01)
	, gridLonDeg(0.01)
	, altMin(0.0)
	, altMax(1.0)
	, altGrid(1.0)
	, timeGrid(1.0)
	, st_MapArea(0, 0, 0, 0)
	, st_AltArea(0, 0, 0, 0)
{
}

// 화면 배율을 정한다.
VOID CTrajectoryPlot::f_SetDpi(INT32 newDpi)
{
	dpi = newDpi;
}

// 그릴 결과를 바꾸고 다시 그린다. 결과가 새로 계산될 때마다 부른다.
VOID CTrajectoryPlot::f_SetResult(const CSimResult *st_NewResult)
{
	st_Result = st_NewResult;
	Invalidate(FALSE);
}

// 배경을 지우지 않는다. OnPaint 가 전부 그린다.
BOOL CTrajectoryPlot::OnEraseBkgnd(CDC *st_Dc)
{
	UNREFERENCED_PARAMETER(st_Dc);

	return TRUE;
}

// 크기가 바뀌면 배치가 달라지므로 전부 다시 그린다.
VOID CTrajectoryPlot::OnSize(UINT32 type, INT32 width, INT32 height)
{
	CStatic::OnSize(type, width, height);
	Invalidate(FALSE);
}

// 메모리 DC 에 모두 그린 뒤 한 번에 옮겨 깜박이지 않게 한다.
VOID CTrajectoryPlot::OnPaint(VOID)
{
	CPaintDC	st_Dc(this);
	CDC			st_MemDc;
	CBitmap		st_Bitmap;
	CBitmap		*st_OldBitmap;
	CRect		st_Client;

	GetClientRect(&st_Client);
	(VOID)st_MemDc.CreateCompatibleDC(&st_Dc);
	(VOID)st_Bitmap.CreateCompatibleBitmap(&st_Dc, st_Client.Width(), st_Client.Height());
	st_OldBitmap = st_MemDc.SelectObject(&st_Bitmap);
	st_MemDc.FillSolidRect(&st_Client, UI_COLOR_CARD);

	f_ComputeLayout(st_Client);
	f_ComputeView();

	{
		Gdiplus::Graphics st_Graphics(st_MemDc.GetSafeHdc());

		(VOID)st_Graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
		f_DrawMap(&st_Graphics);
		f_DrawAltitude(&st_Graphics);
	}

	(VOID)st_Dc.BitBlt(0, 0, st_Client.Width(), st_Client.Height(), &st_MemDc, 0, 0, SRCCOPY);
	(VOID)st_MemDc.SelectObject(st_OldBitmap);
}

// 평면과 고도 패널의 자리를 정한다. 가로가 넉넉하면 나란히, 좁으면 위아래로 둔다.
VOID CTrajectoryPlot::f_ComputeLayout(const CRect &st_Client)
{
	const INT32	left = st_Client.left + f_Ui_Scale(PLOT_MARGIN_LEFT, dpi);
	const INT32	right = st_Client.right - f_Ui_Scale(PLOT_MARGIN_RIGHT, dpi);
	const INT32	top = st_Client.top + f_Ui_Scale(PLOT_TITLE_HEIGHT, dpi);
	const INT32	bottom = st_Client.bottom - f_Ui_Scale(PLOT_AXIS_HEIGHT, dpi);
	INT32		altBlock;

	if (static_cast<FLOAT64>(st_Client.Width()) >= (PLOT_WIDE_RATIO * static_cast<FLOAT64>(st_Client.Height())))
	{
		altBlock = f_Plot_Round(static_cast<FLOAT64>(st_Client.Width()) * PLOT_ALT_SIDE_RATIO);

		if (altBlock < f_Ui_Scale(PLOT_ALT_SIDE_MIN, dpi))
		{
			altBlock = f_Ui_Scale(PLOT_ALT_SIDE_MIN, dpi);
		}

		st_AltArea.SetRect(st_Client.right - altBlock + f_Ui_Scale(PLOT_MARGIN_LEFT, dpi), top, right, bottom);
		st_MapArea.SetRect(left, top, st_Client.right - altBlock - f_Ui_Scale(PLOT_MARGIN_RIGHT, dpi), bottom);
	}
	else
	{
		altBlock = f_Plot_Round(static_cast<FLOAT64>(st_Client.Height()) * PLOT_ALT_RATIO);

		if (altBlock < f_Ui_Scale(PLOT_ALT_MIN_HEIGHT, dpi))
		{
			altBlock = f_Ui_Scale(PLOT_ALT_MIN_HEIGHT, dpi);
		}
		else if (altBlock > f_Ui_Scale(PLOT_ALT_MAX_HEIGHT, dpi))
		{
			altBlock = f_Ui_Scale(PLOT_ALT_MAX_HEIGHT, dpi);
		}
		else
		{
			// 비율대로
		}

		st_AltArea.SetRect(left, st_Client.bottom - altBlock + f_Ui_Scale(PLOT_ALT_TITLE_HEIGHT, dpi), right, bottom);
		st_MapArea.SetRect(left, top, right, st_Client.bottom - altBlock - f_Ui_Scale(PLOT_AXIS_HEIGHT, dpi));
	}
}

// 모든 점이 들어가는 축척과 중심, 눈금 간격을 정한다. 평면은 가로 세로 같은 축척을 쓴다.
VOID CTrajectoryPlot::f_ComputeView(VOID)
{
	const INT32				nSampleNum = st_Result->f_GetSampleNum();
	const INT32				nObjectNum = st_Result->f_GetObjectNum();
	const FLOAT64			width = static_cast<FLOAT64>(st_MapArea.Width());
	const FLOAT64			height = static_cast<FLOAT64>(st_MapArea.Height());
	const ST_PlotPoint		*st_Point;
	FLOAT64					minEast = 0.0;
	FLOAT64					maxEast = 0.0;
	FLOAT64					minNorth = 0.0;
	FLOAT64					maxNorth = 0.0;
	FLOAT64					lowAlt = HUGE_VAL;
	FLOAT64					highAlt = -HUGE_VAL;
	FLOAT64					spanAlt;
	FLOAT64					halfEast;
	FLOAT64					halfNorth;
	STRUCT_Coord_Lla		st_Center;
	STRUCT_Coord_Lla		st_Low;
	STRUCT_Coord_Lla		st_High;
	INT32					nStep;
	INT32					nObject;

	// 원점(플랫폼 초기 위치)은 늘 들어간다.
	for (nStep = 0; nStep < nSampleNum; nStep++)
	{
		for (nObject = 0; nObject < nObjectNum; nObject++)
		{
			st_Point	= st_Result->f_GetPoint(nStep, nObject);
			minEast		= fmin(minEast, st_Point->east);
			maxEast		= fmax(maxEast, st_Point->east);
			minNorth	= fmin(minNorth, st_Point->north);
			maxNorth	= fmax(maxNorth, st_Point->north);
			lowAlt		= fmin(lowAlt, st_Result->f_GetState(nStep, nObject)->st_Lla.Alt);
			highAlt		= fmax(highAlt, st_Result->f_GetState(nStep, nObject)->st_Lla.Alt);
		}
	}

	// 등축척
	pixelPerMeter	= fmin(width / (fmax(maxEast - minEast, PLOT_MIN_SPAN) * PLOT_SPAN_SCALE),
						height / (fmax(maxNorth - minNorth, PLOT_MIN_SPAN) * PLOT_SPAN_SCALE));
	centerEast		= 0.5 * (minEast + maxEast);
	centerNorth		= 0.5 * (minNorth + maxNorth);
	st_Center		= f_PlotToLla(centerEast, centerNorth);
	centerLat		= st_Center.Lat;
	centerLon		= st_Center.Lon;

	// 격자는 위경도 눈금이라 축마다 따로 잡는다. 라벨이 길어 눈금 수를 라벨 폭으로도 제한한다.
	halfEast	= (0.5 * width) / pixelPerMeter;
	halfNorth	= (0.5 * height) / pixelPerMeter;

	st_Low		= f_PlotToLla(centerEast - halfEast, centerNorth);
	st_High		= f_PlotToLla(centerEast + halfEast, centerNorth);
	gridLonDeg	= f_Plot_NiceStepGeo(f_Rad_To_Deg(st_High.Lon - st_Low.Lon),
					fmax(2.0, fmin(PLOT_MAX_GRID_NUM, width / static_cast<FLOAT64>(f_Ui_Scale(PLOT_LON_LABEL_PX, dpi)))));

	st_Low		= f_PlotToLla(centerEast, centerNorth - halfNorth);
	st_High		= f_PlotToLla(centerEast, centerNorth + halfNorth);
	gridLatDeg	= f_Plot_NiceStepGeo(f_Rad_To_Deg(st_High.Lat - st_Low.Lat),
					fmax(2.0, fmin(PLOT_MAX_GRID_NUM, height / static_cast<FLOAT64>(f_Ui_Scale(PLOT_LAT_LABEL_PX, dpi)))));

	// 고도 축은 위아래로 10 % 여유를 둔다. 고도가 거의 그대로면 최소 폭을 준다.
	spanAlt = highAlt - lowAlt;

	if (spanAlt < PLOT_MIN_ALT_SPAN)
	{
		lowAlt	= lowAlt - (0.5 * (PLOT_MIN_ALT_SPAN - spanAlt));
		spanAlt	= PLOT_MIN_ALT_SPAN;
	}

	altMin		= lowAlt - (0.10 * spanAlt);
	altMax		= lowAlt + (1.10 * spanAlt);
	altGrid		= f_Plot_NiceStep(altMax - altMin, 4.0);
	timeGrid	= f_Plot_NiceStep(st_Result->f_GetSample(nSampleNum - 1)->simTime, 10.0);
}

// 동-북 좌표를 평면 픽셀로 바꾼다.
VOID CTrajectoryPlot::f_MapToPixel(const ST_PlotPoint *st_Point, FLOAT64 *pt_X, FLOAT64 *pt_Y) const
{
	*pt_X = static_cast<FLOAT64>(st_MapArea.left) + (0.5 * static_cast<FLOAT64>(st_MapArea.Width())) + ((st_Point->east - centerEast) * pixelPerMeter);
	*pt_Y = static_cast<FLOAT64>(st_MapArea.top) + (0.5 * static_cast<FLOAT64>(st_MapArea.Height())) - ((st_Point->north - centerNorth) * pixelPerMeter);
}

// 평면 좌표(플랫폼 기준 동-북)를 위경도로 바꾼다.
STRUCT_Coord_Lla CTrajectoryPlot::f_PlotToLla(FLOAT64 east, FLOAT64 north) const
{
	const ST_TargetState	*st_Origin = st_Result->f_GetState(0, 0);
	const STRUCT_Coord_Rect	st_Ned = { north, east, 0.0 };

	return f_Tgt_NedToLla(&st_Ned, &st_Origin->st_PosEcef, &st_Origin->st_Lla);
}

// 위경도를 평면 좌표로 바꾼다. f_PlotToLla 의 역이며 격자선 자리를 잡는 데 쓴다.
ST_PlotPoint CTrajectoryPlot::f_LlaToPlot(FLOAT64 lat, FLOAT64 lon) const
{
	const ST_TargetState	*st_Origin = st_Result->f_GetState(0, 0);
	const STRUCT_Coord_Lla	st_Lla = { lat, lon, st_Origin->st_Lla.Alt };
	STRUCT_Coord_Rect		st_Ned;
	ST_PlotPoint			st_Point;

	st_Ned			= f_Tgt_LlaToNed(&st_Lla, &st_Origin->st_PosEcef, &st_Origin->st_Lla);
	st_Point.east	= st_Ned.y;
	st_Point.north	= st_Ned.x;

	return st_Point;
}

// 스텝과 고도를 고도 패널 픽셀로 바꾼다.
VOID CTrajectoryPlot::f_AltToPixel(INT32 nStep, FLOAT64 alt, FLOAT64 *pt_X, FLOAT64 *pt_Y) const
{
	const FLOAT64 ratioX = static_cast<FLOAT64>(nStep) / static_cast<FLOAT64>(st_Result->f_GetSampleNum() - 1);
	const FLOAT64 ratioY = (alt - altMin) / (altMax - altMin);

	*pt_X = static_cast<FLOAT64>(st_AltArea.left) + (ratioX * static_cast<FLOAT64>(st_AltArea.Width()));
	*pt_Y = static_cast<FLOAT64>(st_AltArea.bottom) - (ratioY * static_cast<FLOAT64>(st_AltArea.Height()));
}

// 동-북 평면의 위경도 격자, 궤적, 시작점을 그린다.
VOID CTrajectoryPlot::f_DrawMap(Gdiplus::Graphics *st_Graphics) const
{
	const FLOAT32				scale = static_cast<FLOAT32>(f_Ui_Scale(100, dpi)) / 100.0F;
	const Gdiplus::FontFamily	st_Family(L"Malgun Gothic");
	const Gdiplus::Font			st_Font(&st_Family, scale * PLOT_FONT_PX, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	const Gdiplus::Font			st_Bold(&st_Family, scale * PLOT_FONT_PX, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
	Gdiplus::Pen				st_GridPen(f_Plot_Color(UI_COLOR_GRID_LINE, 255), 1.0F);
	Gdiplus::Pen				st_FramePen(f_Plot_Color(UI_COLOR_BORDER, 255), 1.0F);
	const Gdiplus::Rect			st_Clip(st_MapArea.left, st_MapArea.top, st_MapArea.Width(), st_MapArea.Height());
	const INT32					nSampleNum = st_Result->f_GetSampleNum();
	const INT32					nObjectNum = st_Result->f_GetObjectNum();
	const FLOAT64				halfEast = (0.5 * static_cast<FLOAT64>(st_MapArea.Width())) / pixelPerMeter;
	const FLOAT64				halfNorth = (0.5 * static_cast<FLOAT64>(st_MapArea.Height())) / pixelPerMeter;
	std::vector<Gdiplus::PointF>	st_Line(static_cast<UINT64>(nSampleNum));
	ST_PlotPoint				st_Point;
	STRUCT_Coord_Lla			st_Low;
	STRUCT_Coord_Lla			st_High;
	FLOAT64						x = 0.0;
	FLOAT64						y = 0.0;
	FLOAT64						firstLine;
	FLOAT64						lineNum;
	FLOAT64						degree;
	INT32						nLine;
	INT32						nObject;
	INT32						nSample;

	// 격자와 눈금은 또렷하게, 궤적은 부드럽게
	(VOID)st_Graphics->SetSmoothingMode(Gdiplus::SmoothingModeNone);

	// 세로선 = 경도. 보이는 좌우 끝의 경도를 구해 그 사이의 눈금 값마다 긋는다.
	st_Low		= f_PlotToLla(centerEast - halfEast, centerNorth);
	st_High		= f_PlotToLla(centerEast + halfEast, centerNorth);
	firstLine	= ceil(f_Rad_To_Deg(st_Low.Lon) / gridLonDeg);
	lineNum		= floor(f_Rad_To_Deg(st_High.Lon) / gridLonDeg) - firstLine;

	for (nLine = 0; static_cast<FLOAT64>(nLine) <= lineNum; nLine++)
	{
		degree		= (firstLine + static_cast<FLOAT64>(nLine)) * gridLonDeg;
		st_Point	= f_LlaToPlot(centerLat, f_Deg_To_Rad(degree));
		f_MapToPixel(&st_Point, &x, &y);
		(VOID)st_Graphics->DrawLine(&st_GridPen, f_Plot_Round(x), st_MapArea.top, f_Plot_Round(x), st_MapArea.bottom);
		f_Plot_Text(st_Graphics, &st_Font, f_Plot_Number(degree, f_Plot_DecimalFor(gridLonDeg)), x, static_cast<FLOAT64>(st_MapArea.bottom) + 4.0,
			Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentNear, UI_COLOR_TEXT_SUB);
	}

	// 가로선 = 위도
	st_Low		= f_PlotToLla(centerEast, centerNorth - halfNorth);
	st_High		= f_PlotToLla(centerEast, centerNorth + halfNorth);
	firstLine	= ceil(f_Rad_To_Deg(st_Low.Lat) / gridLatDeg);
	lineNum		= floor(f_Rad_To_Deg(st_High.Lat) / gridLatDeg) - firstLine;

	for (nLine = 0; static_cast<FLOAT64>(nLine) <= lineNum; nLine++)
	{
		degree		= (firstLine + static_cast<FLOAT64>(nLine)) * gridLatDeg;
		st_Point	= f_LlaToPlot(f_Deg_To_Rad(degree), centerLon);
		f_MapToPixel(&st_Point, &x, &y);
		(VOID)st_Graphics->DrawLine(&st_GridPen, st_MapArea.left, f_Plot_Round(y), st_MapArea.right, f_Plot_Round(y));
		f_Plot_Text(st_Graphics, &st_Font, f_Plot_Number(degree, f_Plot_DecimalFor(gridLatDeg)), static_cast<FLOAT64>(st_MapArea.left) - 6.0, y,
			Gdiplus::StringAlignmentFar, Gdiplus::StringAlignmentCenter, UI_COLOR_TEXT_SUB);
	}

	(VOID)st_Graphics->DrawRectangle(&st_FramePen, st_MapArea.left, st_MapArea.top, st_MapArea.Width(), st_MapArea.Height());

	f_Plot_Text(st_Graphics, &st_Bold, CString(_T("궤적 (위도-경도)")), static_cast<FLOAT64>(st_MapArea.left), static_cast<FLOAT64>(st_MapArea.top) - 5.0,
		Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentFar, UI_COLOR_TEXT);
	f_Plot_Text(st_Graphics, &st_Font, CString(_T("Lon [deg]")), static_cast<FLOAT64>(st_MapArea.right) - 6.0, static_cast<FLOAT64>(st_MapArea.bottom) - 4.0,
		Gdiplus::StringAlignmentFar, Gdiplus::StringAlignmentFar, UI_COLOR_TEXT_OFF);
	f_Plot_Text(st_Graphics, &st_Font, CString(_T("Lat [deg]")), static_cast<FLOAT64>(st_MapArea.left) + 6.0, static_cast<FLOAT64>(st_MapArea.top) + 4.0,
		Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentNear, UI_COLOR_TEXT_OFF);

	(VOID)st_Graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	(VOID)st_Graphics->SetClip(st_Clip);

	// 궤적
	for (nObject = 0; nObject < nObjectNum; nObject++)
	{
		Gdiplus::Pen st_TrackPen(f_Plot_Color(f_Ui_ObjectColor(nObject), 255), 2.0F * scale);

		(VOID)st_TrackPen.SetLineJoin(Gdiplus::LineJoinRound);

		for (nSample = 0; nSample < nSampleNum; nSample++)
		{
			f_MapToPixel(st_Result->f_GetPoint(nSample, nObject), &x, &y);
			st_Line[static_cast<UINT64>(nSample)] = Gdiplus::PointF(static_cast<FLOAT32>(x), static_cast<FLOAT32>(y));
		}

		(VOID)st_Graphics->DrawLines(&st_TrackPen, st_Line.data(), nSampleNum);
	}

	// 시작점
	for (nObject = 0; nObject < nObjectNum; nObject++)
	{
		const FLOAT32		radius = 5.0F * scale;
		Gdiplus::SolidBrush	st_Fill(f_Plot_Color(UI_COLOR_CARD, 255));
		Gdiplus::Pen		st_Ring(f_Plot_Color(f_Ui_ObjectColor(nObject), 255), 2.0F * scale);

		f_MapToPixel(st_Result->f_GetPoint(0, nObject), &x, &y);
		(VOID)st_Graphics->FillEllipse(&st_Fill, static_cast<FLOAT32>(x) - radius, static_cast<FLOAT32>(y) - radius, 2.0F * radius, 2.0F * radius);
		(VOID)st_Graphics->DrawEllipse(&st_Ring, static_cast<FLOAT32>(x) - radius, static_cast<FLOAT32>(y) - radius, 2.0F * radius, 2.0F * radius);
	}

	f_DrawStartLabels(st_Graphics, &st_Font);
	(VOID)st_Graphics->ResetClip();
}

// 시작점 이름표. 출발 방향의 반대쪽에 두어 궤적을 가리지 않게 한다. 동서로만 가는 객체끼리는 위아래를 엇갈린다.
VOID CTrajectoryPlot::f_DrawStartLabels(Gdiplus::Graphics *st_Graphics, const Gdiplus::Font *st_Font) const
{
	const FLOAT64				gap = PLOT_LABEL_GAP_PX * static_cast<FLOAT64>(f_Ui_Scale(100, dpi)) / 100.0;
	Gdiplus::StringAlignment	alignX;
	Gdiplus::StringAlignment	alignY;
	FLOAT64						x = 0.0;
	FLOAT64						y = 0.0;
	FLOAT64						yaw;
	INT32						nObject;

	for (nObject = 0; nObject < st_Result->f_GetObjectNum(); nObject++)
	{
		yaw = st_Result->f_GetState(0, nObject)->st_Att.Yaw;
		f_MapToPixel(st_Result->f_GetPoint(0, nObject), &x, &y);

		// 동쪽으로 출발하면 왼쪽, 아니면 오른쪽
		alignX = (sin(yaw) > PLOT_LABEL_SIDE_MIN) ? Gdiplus::StringAlignmentFar : Gdiplus::StringAlignmentNear;

		// 북쪽으로 출발하면 아래, 남쪽으로 출발하면 위
		if (cos(yaw) > PLOT_LABEL_SIDE_MIN)
		{
			alignY = Gdiplus::StringAlignmentNear;
		}
		else if (cos(yaw) < -PLOT_LABEL_SIDE_MIN)
		{
			alignY = Gdiplus::StringAlignmentFar;
		}
		else
		{
			alignY = ((nObject % 2) == 0) ? Gdiplus::StringAlignmentNear : Gdiplus::StringAlignmentFar;
		}

		f_Plot_Text(st_Graphics, st_Font, f_Plot_ObjectName(nObject),
			(alignX == Gdiplus::StringAlignmentFar) ? (x - gap) : (x + gap), (alignY == Gdiplus::StringAlignmentFar) ? (y - gap) : (y + gap),
			alignX, alignY, f_Ui_ObjectColor(nObject));
	}
}

// 고도-시각 패널을 그린다.
VOID CTrajectoryPlot::f_DrawAltitude(Gdiplus::Graphics *st_Graphics) const
{
	const FLOAT32				scale = static_cast<FLOAT32>(f_Ui_Scale(100, dpi)) / 100.0F;
	const Gdiplus::FontFamily	st_Family(L"Malgun Gothic");
	const Gdiplus::Font			st_Font(&st_Family, scale * PLOT_FONT_PX, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	const Gdiplus::Font			st_Bold(&st_Family, scale * PLOT_FONT_PX, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
	Gdiplus::Pen				st_GridPen(f_Plot_Color(UI_COLOR_GRID_LINE, 255), 1.0F);
	Gdiplus::Pen				st_FramePen(f_Plot_Color(UI_COLOR_BORDER, 255), 1.0F);
	const Gdiplus::Rect			st_Clip(st_AltArea.left, st_AltArea.top, st_AltArea.Width(), st_AltArea.Height());
	const INT32					nSampleNum = st_Result->f_GetSampleNum();
	const INT32					nObjectNum = st_Result->f_GetObjectNum();
	const FLOAT64				endTime = st_Result->f_GetSample(nSampleNum - 1)->simTime;
	std::vector<Gdiplus::PointF>	st_Line(static_cast<UINT64>(nSampleNum));
	FLOAT64						x = 0.0;
	FLOAT64						y = 0.0;
	FLOAT64						firstLine;
	FLOAT64						lineNum;
	FLOAT64						value;
	INT32						nLine;
	INT32						nObject;
	INT32						nSample;

	(VOID)st_Graphics->SetSmoothingMode(Gdiplus::SmoothingModeNone);

	// 가로선 = 고도
	firstLine	= ceil(altMin / altGrid);
	lineNum		= floor(altMax / altGrid) - firstLine;

	for (nLine = 0; static_cast<FLOAT64>(nLine) <= lineNum; nLine++)
	{
		value = (firstLine + static_cast<FLOAT64>(nLine)) * altGrid;
		f_AltToPixel(0, value, &x, &y);
		(VOID)st_Graphics->DrawLine(&st_GridPen, st_AltArea.left, f_Plot_Round(y), st_AltArea.right, f_Plot_Round(y));
		f_Plot_Text(st_Graphics, &st_Font, f_Plot_Number(value, f_Plot_DecimalFor(altGrid)), static_cast<FLOAT64>(st_AltArea.left) - 6.0, y,
			Gdiplus::StringAlignmentFar, Gdiplus::StringAlignmentCenter, UI_COLOR_TEXT_SUB);
	}

	// 세로선 = 시각. 끝 시각이 눈금과 딱 맞아도 반올림 때문에 빠지지 않게 조금 여유를 둔다.
	lineNum = floor((endTime / timeGrid) + 1.0e-9);

	for (nLine = 0; static_cast<FLOAT64>(nLine) <= lineNum; nLine++)
	{
		value	= static_cast<FLOAT64>(nLine) * timeGrid;
		x		= static_cast<FLOAT64>(st_AltArea.left) + ((value / endTime) * static_cast<FLOAT64>(st_AltArea.Width()));
		(VOID)st_Graphics->DrawLine(&st_GridPen, f_Plot_Round(x), st_AltArea.top, f_Plot_Round(x), st_AltArea.bottom);
		f_Plot_Text(st_Graphics, &st_Font, f_Plot_Number(value, f_Plot_DecimalFor(timeGrid)), x, static_cast<FLOAT64>(st_AltArea.bottom) + 4.0,
			Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentNear, UI_COLOR_TEXT_SUB);
	}

	(VOID)st_Graphics->DrawRectangle(&st_FramePen, st_AltArea.left, st_AltArea.top, st_AltArea.Width(), st_AltArea.Height());
	f_Plot_Text(st_Graphics, &st_Bold, CString(_T("고도 (m) / 시각 (s)")), static_cast<FLOAT64>(st_AltArea.left), static_cast<FLOAT64>(st_AltArea.top) - 5.0,
		Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentFar, UI_COLOR_TEXT);

	(VOID)st_Graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	(VOID)st_Graphics->SetClip(st_Clip);

	for (nObject = 0; nObject < nObjectNum; nObject++)
	{
		Gdiplus::Pen st_Pen(f_Plot_Color(f_Ui_ObjectColor(nObject), 255), 1.6F * scale);

		(VOID)st_Pen.SetLineJoin(Gdiplus::LineJoinRound);

		for (nSample = 0; nSample < nSampleNum; nSample++)
		{
			f_AltToPixel(nSample, st_Result->f_GetState(nSample, nObject)->st_Lla.Alt, &x, &y);
			st_Line[static_cast<UINT64>(nSample)] = Gdiplus::PointF(static_cast<FLOAT32>(x), static_cast<FLOAT32>(y));
		}

		(VOID)st_Graphics->DrawLines(&st_Pen, st_Line.data(), nSampleNum);
	}

	(VOID)st_Graphics->ResetClip();
}
