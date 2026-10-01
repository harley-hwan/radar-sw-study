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
#define PLOT_PANEL_TITLE_HEIGHT	22
#define PLOT_PANEL_MIN_HEIGHT	112
#define PLOT_PANEL_MAX_HEIGHT	190
#define PLOT_PANEL_RATIO		0.30
#define PLOT_WIDE_RATIO			1.40					// 너비/높이가 이보다 크면 시각 그래프를 평면 오른쪽에 배치
#define PLOT_PANEL_SIDE_RATIO	0.38
#define PLOT_PANEL_SIDE_MIN		250
#define PLOT_MIN_SPAN			1000.0					// [m]
#define PLOT_SPAN_SCALE			1.18
#define PLOT_MIN_ALT_SPAN		10.0					// [m]
#define PLOT_MIN_SPEED_SPAN		10.0					// [m/s]
#define PLOT_MAX_GRID_NUM		8.0
#define PLOT_LON_LABEL_PX		78						// [px @96dpi] 경도 라벨 하나의 폭
#define PLOT_LAT_LABEL_PX		34						// [px @96dpi] 위도 라벨 하나의 높이
#define PLOT_TIME_LABEL_PX		22						// [px @96dpi] 시각 라벨 하나의 폭
#define PLOT_FONT_PX			12.0F
#define PLOT_LABEL_GAP_PX		8.0						// [px @96dpi] 시작점과 이름표 사이
#define PLOT_LABEL_SIDE_MIN		0.3
#define PLOT_COMPARE_DARK		60						// [%] 비교 결과 선 밝기 (같은 색을 어둡게)

// COLORREF + 알파 -> GDI+ 색.
static Gdiplus::Color f_Plot_Color(COLORREF color, INT32 alpha)
{
	return Gdiplus::Color(static_cast<UINT8>(alpha), GetRValue(color), GetGValue(color), GetBValue(color));
}

// 궤적 선 색. 비교 결과는 같은 색을 어둡게.
static Gdiplus::Color f_Plot_TrackColor(INT32 nObject, INT32 isCompare)
{
	const COLORREF	color = f_Ui_ObjectColor(nObject);
	const INT32		percent = (isCompare != 0) ? PLOT_COMPARE_DARK : 100;

	return f_Plot_Color(RGB((GetRValue(color) * percent) / 100, (GetGValue(color) * percent) / 100, (GetBValue(color) * percent) / 100), 255);
}

// 궤적 선 모양. 꺾이는 곳은 둥글게, 비교 결과는 점선.
static VOID f_Plot_SetTrackStyle(Gdiplus::Pen *st_Pen, INT32 isCompare)
{
	static const Gdiplus::REAL s_Dash[2] = { 4.0F, 3.0F };					// 선, 빈칸 길이 (선 굵기 배)

	(VOID)st_Pen->SetLineJoin(Gdiplus::LineJoinRound);

	if (isCompare != 0)
	{
		(VOID)st_Pen->SetDashPattern(s_Dash, 2);
	}
}

// 좌표 -> 정수 픽셀 반올림.
static INT32 f_Plot_Round(FLOAT64 value)
{
	return static_cast<INT32>(floor(value + 0.5));
}

// 가수 x 10^k 를 작은 것부터 올려, 눈금이 maxTickNum 개 이하가 되는 첫 간격 선택.
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

// {1, 2, 5} x 10^k. 시각 그래프 축용.
static FLOAT64 f_Plot_NiceStep(FLOAT64 span, FLOAT64 maxTickNum)
{
	static const FLOAT64 s_Mantissa[3] = { 1.0, 2.0, 5.0 };

	return f_Plot_NiceStepFrom(span, maxTickNum, s_Mantissa, 3);
}

// {1, 2, 4, 5} x 10^k. 위경도 격자용.
static FLOAT64 f_Plot_NiceStepGeo(FLOAT64 span, FLOAT64 maxTickNum)
{
	static const FLOAT64 s_Mantissa[4] = { 1.0, 2.0, 4.0, 5.0 };

	return f_Plot_NiceStepFrom(span, maxTickNum, s_Mantissa, 4);
}

// 눈금 간격에 맞는 소수 자릿수.
static INT32 f_Plot_DecimalFor(FLOAT64 step)
{
	return (step < 1.0) ? static_cast<INT32>(ceil(-log10(step))) : 0;
}

// 눈금 값 -> 고정 소수 자릿수 글자.
static CString f_Plot_Number(FLOAT64 value, INT32 nDecimal)
{
	CString st_Text;

	st_Text.Format(_T("%.*f"), nDecimal, value);

	return st_Text;
}

// 기준점 정렬 글자 그리기. Near = 오른쪽(아래), Far = 왼쪽(위).
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

// 궤적 그림. 결과 버퍼는 대화상자 소유, 여기서는 읽기만.
CTrajectoryPlot::CTrajectoryPlot() noexcept
	: st_Result(nullptr)
	, st_Compare(nullptr)
	, dpi(UI_BASE_DPI)
	, pixelPerMeter(1.0)
	, centerEast(0.0)
	, centerNorth(0.0)
	, centerLat(0.0)
	, centerLon(0.0)
	, gridLatDeg(0.01)
	, gridLonDeg(0.01)
	, timeGrid(1.0)
	, st_MapArea(0, 0, 0, 0)
{
	st_Panel[PLOT_PANEL_ALT].pt_Title		= _T("고도 (m) / 시각 (s)");
	st_Panel[PLOT_PANEL_ALT].minSpan		= PLOT_MIN_ALT_SPAN;
	st_Panel[PLOT_PANEL_SPEED].pt_Title		= _T("속력 (m/s) / 시각 (s)");
	st_Panel[PLOT_PANEL_SPEED].minSpan		= PLOT_MIN_SPEED_SPAN;
}

// 화면 배율 설정.
VOID CTrajectoryPlot::f_SetDpi(INT32 newDpi)
{
	dpi = newDpi;
}

// 그릴 결과 교체 후 다시 그리기. 재계산, 비교 켜기 / 끄기마다 호출. 비교 결과는 크기가 같을 때만 씀.
VOID CTrajectoryPlot::f_SetResult(const CSimResult *st_NewResult, const CSimResult *st_NewCompare)
{
	st_Result	= st_NewResult;
	st_Compare	= nullptr;

	if ((st_NewCompare != nullptr) && (st_NewCompare->f_GetSampleNum() == st_NewResult->f_GetSampleNum())
		&& (st_NewCompare->f_GetObjectNum() == st_NewResult->f_GetObjectNum()))
	{
		st_Compare = st_NewCompare;
	}

	Invalidate(FALSE);
}

// 배경 지우기 생략. OnPaint 에서 전부 그림.
BOOL CTrajectoryPlot::OnEraseBkgnd(CDC *st_Dc)
{
	UNREFERENCED_PARAMETER(st_Dc);

	return TRUE;
}

// 크기 변경 시 전체 다시 그리기.
VOID CTrajectoryPlot::OnSize(UINT32 type, INT32 width, INT32 height)
{
	CStatic::OnSize(type, width, height);
	Invalidate(FALSE);
}

// 메모리 DC 에 그린 뒤 한 번에 복사 (깜박임 방지).
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
		f_DrawPanel(&st_Graphics, PLOT_PANEL_ALT);
		f_DrawPanel(&st_Graphics, PLOT_PANEL_SPEED);
	}

	(VOID)st_Dc.BitBlt(0, 0, st_Client.Width(), st_Client.Height(), &st_MemDc, 0, 0, SRCCOPY);
	(VOID)st_MemDc.SelectObject(st_OldBitmap);
}

// 평면, 시각 그래프 배치. 가로가 넓으면 평면 오른쪽에 고도, 속력을 위아래로, 좁으면 평면 아래에 좌우로.
VOID CTrajectoryPlot::f_ComputeLayout(const CRect &st_Client)
{
	const INT32	marginLeft = f_Ui_Scale(PLOT_MARGIN_LEFT, dpi);
	const INT32	marginRight = f_Ui_Scale(PLOT_MARGIN_RIGHT, dpi);
	const INT32	axisHeight = f_Ui_Scale(PLOT_AXIS_HEIGHT, dpi);
	const INT32	panelTitle = f_Ui_Scale(PLOT_PANEL_TITLE_HEIGHT, dpi);
	const INT32	left = st_Client.left + marginLeft;
	const INT32	right = st_Client.right - marginRight;
	const INT32	top = st_Client.top + f_Ui_Scale(PLOT_TITLE_HEIGHT, dpi);
	const INT32	bottom = st_Client.bottom - axisHeight;
	INT32		block;
	INT32		split;

	if (static_cast<FLOAT64>(st_Client.Width()) >= (PLOT_WIDE_RATIO * static_cast<FLOAT64>(st_Client.Height())))
	{
		block = f_Plot_Round(static_cast<FLOAT64>(st_Client.Width()) * PLOT_PANEL_SIDE_RATIO);

		if (block < f_Ui_Scale(PLOT_PANEL_SIDE_MIN, dpi))
		{
			block = f_Ui_Scale(PLOT_PANEL_SIDE_MIN, dpi);
		}

		// 오른쪽 칸을 위아래로 나눔. 사이는 위 칸 시각 눈금과 아래 칸 제목 자리.
		split = (top + bottom - axisHeight - panelTitle) / 2;
		st_MapArea.SetRect(left, top, st_Client.right - block - marginRight, bottom);
		st_Panel[PLOT_PANEL_ALT].st_Area.SetRect(st_Client.right - block + marginLeft, top, right, split);
		st_Panel[PLOT_PANEL_SPEED].st_Area.SetRect(st_Client.right - block + marginLeft, split + axisHeight + panelTitle, right, bottom);
	}
	else
	{
		block = f_Plot_Round(static_cast<FLOAT64>(st_Client.Height()) * PLOT_PANEL_RATIO);

		if (block < f_Ui_Scale(PLOT_PANEL_MIN_HEIGHT, dpi))
		{
			block = f_Ui_Scale(PLOT_PANEL_MIN_HEIGHT, dpi);
		}
		else if (block > f_Ui_Scale(PLOT_PANEL_MAX_HEIGHT, dpi))
		{
			block = f_Ui_Scale(PLOT_PANEL_MAX_HEIGHT, dpi);
		}
		else
		{
			// 비율대로
		}

		// 아래 칸을 좌우로 나눔.
		split = (st_Client.left + st_Client.right) / 2;
		st_MapArea.SetRect(left, top, right, st_Client.bottom - block - axisHeight);
		st_Panel[PLOT_PANEL_ALT].st_Area.SetRect(left, st_Client.bottom - block + panelTitle, split - marginRight, bottom);
		st_Panel[PLOT_PANEL_SPEED].st_Area.SetRect(split + marginLeft, st_Client.bottom - block + panelTitle, right, bottom);
	}
}

// 모든 점이 들어가는 축척, 중심, 눈금 간격 계산.
VOID CTrajectoryPlot::f_ComputeView(VOID)
{
	const INT32				nSampleNum = st_Result->f_GetSampleNum();
	const INT32				nObjectNum = st_Result->f_GetObjectNum();
	const FLOAT64			width = static_cast<FLOAT64>(st_MapArea.Width());
	const FLOAT64			height = static_cast<FLOAT64>(st_MapArea.Height());
	const CSimResult		*st_Source[2] = { st_Result, st_Compare };
	const INT32				nSourceNum = (st_Compare != nullptr) ? 2 : 1;
	const ST_PlotPoint		*st_Point;
	FLOAT64					minEast = 0.0;
	FLOAT64					maxEast = 0.0;
	FLOAT64					minNorth = 0.0;
	FLOAT64					maxNorth = 0.0;
	FLOAT64					lowValue[PLOT_PANEL_NUM];
	FLOAT64					highValue[PLOT_PANEL_NUM];
	FLOAT64					value;
	FLOAT64					span;
	FLOAT64					halfEast;
	FLOAT64					halfNorth;
	STRUCT_Coord_Lla		st_Center;
	STRUCT_Coord_Lla		st_Low;
	STRUCT_Coord_Lla		st_High;
	INT32					nSource;
	INT32					nStep;
	INT32					nObject;
	INT32					nPanel;

	for (nPanel = 0; nPanel < PLOT_PANEL_NUM; nPanel++)
	{
		lowValue[nPanel]	= HUGE_VAL;
		highValue[nPanel]	= -HUGE_VAL;
	}

	// 원점(플랫폼 초기 위치)은 항상 포함. 비교 결과가 있으면 함께 포함.
	for (nSource = 0; nSource < nSourceNum; nSource++)
	{
		for (nStep = 0; nStep < nSampleNum; nStep++)
		{
			for (nObject = 0; nObject < nObjectNum; nObject++)
			{
				st_Point	= st_Source[nSource]->f_GetPoint(nStep, nObject);
				minEast		= fmin(minEast, st_Point->east);
				maxEast		= fmax(maxEast, st_Point->east);
				minNorth	= fmin(minNorth, st_Point->north);
				maxNorth	= fmax(maxNorth, st_Point->north);

				for (nPanel = 0; nPanel < PLOT_PANEL_NUM; nPanel++)
				{
					value				= f_PanelValue(st_Source[nSource], nPanel, nStep, nObject);
					lowValue[nPanel]	= fmin(lowValue[nPanel], value);
					highValue[nPanel]	= fmax(highValue[nPanel], value);
				}
			}
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

	// 위경도 격자 간격 (축별).
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

	// 시각 그래프 세로축 위아래 10 % 여유. 값 변화가 작으면 최소 폭 적용.
	for (nPanel = 0; nPanel < PLOT_PANEL_NUM; nPanel++)
	{
		ST_PlotPanel *st_Info = &st_Panel[nPanel];

		span = highValue[nPanel] - lowValue[nPanel];

		if (span < st_Info->minSpan)
		{
			lowValue[nPanel]	= lowValue[nPanel] - (0.5 * (st_Info->minSpan - span));
			span				= st_Info->minSpan;
		}

		st_Info->valueMin	= lowValue[nPanel] - (0.10 * span);
		st_Info->valueMax	= lowValue[nPanel] + (1.10 * span);
		st_Info->valueGrid	= f_Plot_NiceStep(st_Info->valueMax - st_Info->valueMin, 4.0);
	}

	// 시각 눈금은 칸 폭에 라벨이 겹치지 않을 만큼.
	timeGrid = f_Plot_NiceStep(st_Result->f_GetSample(nSampleNum - 1)->simTime,
		fmax(2.0, fmin(10.0, static_cast<FLOAT64>(st_Panel[PLOT_PANEL_ALT].st_Area.Width()) / static_cast<FLOAT64>(f_Ui_Scale(PLOT_TIME_LABEL_PX, dpi)))));
}

// 동-북 좌표 -> 평면 픽셀.
VOID CTrajectoryPlot::f_MapToPixel(const ST_PlotPoint *st_Point, FLOAT64 *pt_X, FLOAT64 *pt_Y) const
{
	*pt_X = static_cast<FLOAT64>(st_MapArea.left) + (0.5 * static_cast<FLOAT64>(st_MapArea.Width())) + ((st_Point->east - centerEast) * pixelPerMeter);
	*pt_Y = static_cast<FLOAT64>(st_MapArea.top) + (0.5 * static_cast<FLOAT64>(st_MapArea.Height())) - ((st_Point->north - centerNorth) * pixelPerMeter);
}

// 평면 좌표(플랫폼 기준 동-북) -> 위경도.
STRUCT_Coord_Lla CTrajectoryPlot::f_PlotToLla(FLOAT64 east, FLOAT64 north) const
{
	const ST_TargetState	*st_Origin = st_Result->f_GetState(0, 0);
	const STRUCT_Coord_Rect	st_Ned = { north, east, 0.0 };

	return f_Tgt_NedToLla(&st_Ned, &st_Origin->st_PosEcef, &st_Origin->st_Lla);
}

// 위경도 -> 평면 좌표. 격자선 위치 계산용.
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

// 시각 그래프 값: 고도 [m] 또는 속력 [m/s] (ECEF 속도 크기).
FLOAT64 CTrajectoryPlot::f_PanelValue(const CSimResult *st_Source, INT32 nPanel, INT32 nStep, INT32 nObject) const
{
	const ST_TargetState	*st_State = st_Source->f_GetState(nStep, nObject);
	FLOAT64					value;

	if (nPanel == PLOT_PANEL_ALT)
	{
		value = st_State->st_Lla.Alt;
	}
	else
	{
		value = sqrt((st_State->st_VelEcef.x * st_State->st_VelEcef.x) + (st_State->st_VelEcef.y * st_State->st_VelEcef.y)
			+ (st_State->st_VelEcef.z * st_State->st_VelEcef.z));
	}

	return value;
}

// 스텝, 값 -> 시각 그래프 픽셀.
VOID CTrajectoryPlot::f_PanelToPixel(INT32 nPanel, INT32 nStep, FLOAT64 value, FLOAT64 *pt_X, FLOAT64 *pt_Y) const
{
	const ST_PlotPanel	*st_Info = &st_Panel[nPanel];
	const FLOAT64		ratioX = static_cast<FLOAT64>(nStep) / static_cast<FLOAT64>(st_Result->f_GetSampleNum() - 1);
	const FLOAT64		ratioY = (value - st_Info->valueMin) / (st_Info->valueMax - st_Info->valueMin);

	*pt_X = static_cast<FLOAT64>(st_Info->st_Area.left) + (ratioX * static_cast<FLOAT64>(st_Info->st_Area.Width()));
	*pt_Y = static_cast<FLOAT64>(st_Info->st_Area.bottom) - (ratioY * static_cast<FLOAT64>(st_Info->st_Area.Height()));
}

// 동-북 평면: 위경도 격자, 궤적, 시작점 그리기.
VOID CTrajectoryPlot::f_DrawMap(Gdiplus::Graphics *st_Graphics) const
{
	const FLOAT32				scale = static_cast<FLOAT32>(f_Ui_Scale(100, dpi)) / 100.0F;
	const Gdiplus::FontFamily	st_Family(L"Malgun Gothic");
	const Gdiplus::Font			st_Font(&st_Family, scale * PLOT_FONT_PX, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	const Gdiplus::Font			st_Bold(&st_Family, scale * PLOT_FONT_PX, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
	Gdiplus::Pen				st_GridPen(f_Plot_Color(UI_COLOR_GRID_LINE, 255), 1.0F);
	Gdiplus::Pen				st_FramePen(f_Plot_Color(UI_COLOR_BORDER, 255), 1.0F);
	const Gdiplus::Rect			st_Clip(st_MapArea.left, st_MapArea.top, st_MapArea.Width(), st_MapArea.Height());
	const INT32					nObjectNum = st_Result->f_GetObjectNum();
	const FLOAT64				halfEast = (0.5 * static_cast<FLOAT64>(st_MapArea.Width())) / pixelPerMeter;
	const FLOAT64				halfNorth = (0.5 * static_cast<FLOAT64>(st_MapArea.Height())) / pixelPerMeter;
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

	// 격자, 눈금은 선명하게, 궤적은 부드럽게.
	(VOID)st_Graphics->SetSmoothingMode(Gdiplus::SmoothingModeNone);

	// 세로선 = 경도. 보이는 좌우 끝 경도 사이의 눈금마다 긋기.
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

	if (st_Compare != nullptr)
	{
		f_Plot_Text(st_Graphics, &st_Font, CString(_T("점선 = 플랫폼 기준")), static_cast<FLOAT64>(st_MapArea.right), static_cast<FLOAT64>(st_MapArea.top) - 5.0,
			Gdiplus::StringAlignmentFar, Gdiplus::StringAlignmentFar, UI_COLOR_TEXT_SUB);
	}

	(VOID)st_Graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	(VOID)st_Graphics->SetClip(st_Clip);

	// 궤적. 비교 결과는 점선으로 위에 겹침 (평면에서는 거의 같은 자리).
	f_DrawMapTrack(st_Graphics, st_Result, 0);

	if (st_Compare != nullptr)
	{
		f_DrawMapTrack(st_Graphics, st_Compare, 1);
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

// 평면에 결과 하나의 궤적 그리기.
VOID CTrajectoryPlot::f_DrawMapTrack(Gdiplus::Graphics *st_Graphics, const CSimResult *st_Source, INT32 isCompare) const
{
	const FLOAT32					scale = static_cast<FLOAT32>(f_Ui_Scale(100, dpi)) / 100.0F;
	const INT32						nSampleNum = st_Source->f_GetSampleNum();
	std::vector<Gdiplus::PointF>	st_Line(static_cast<UINT64>(nSampleNum));
	FLOAT64							x = 0.0;
	FLOAT64							y = 0.0;
	INT32							nObject;
	INT32							nSample;

	for (nObject = 0; nObject < st_Source->f_GetObjectNum(); nObject++)
	{
		Gdiplus::Pen st_TrackPen(f_Plot_TrackColor(nObject, isCompare), ((isCompare != 0) ? 1.4F : 2.0F) * scale);

		f_Plot_SetTrackStyle(&st_TrackPen, isCompare);

		for (nSample = 0; nSample < nSampleNum; nSample++)
		{
			f_MapToPixel(st_Source->f_GetPoint(nSample, nObject), &x, &y);
			st_Line[static_cast<UINT64>(nSample)] = Gdiplus::PointF(static_cast<FLOAT32>(x), static_cast<FLOAT32>(y));
		}

		(VOID)st_Graphics->DrawLines(&st_TrackPen, st_Line.data(), nSampleNum);
	}
}

// 시작점 이름표. 출발 방향 반대쪽에 표시.
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

// 시각 그래프 칸 그리기 (고도-시각 또는 속력-시각).
VOID CTrajectoryPlot::f_DrawPanel(Gdiplus::Graphics *st_Graphics, INT32 nPanel) const
{
	const ST_PlotPanel			*st_Info = &st_Panel[nPanel];
	const FLOAT32				scale = static_cast<FLOAT32>(f_Ui_Scale(100, dpi)) / 100.0F;
	const Gdiplus::FontFamily	st_Family(L"Malgun Gothic");
	const Gdiplus::Font			st_Font(&st_Family, scale * PLOT_FONT_PX, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	const Gdiplus::Font			st_Bold(&st_Family, scale * PLOT_FONT_PX, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
	Gdiplus::Pen				st_GridPen(f_Plot_Color(UI_COLOR_GRID_LINE, 255), 1.0F);
	Gdiplus::Pen				st_FramePen(f_Plot_Color(UI_COLOR_BORDER, 255), 1.0F);
	const Gdiplus::Rect			st_Clip(st_Info->st_Area.left, st_Info->st_Area.top, st_Info->st_Area.Width(), st_Info->st_Area.Height());
	const INT32					nSampleNum = st_Result->f_GetSampleNum();
	const FLOAT64				endTime = st_Result->f_GetSample(nSampleNum - 1)->simTime;
	FLOAT64						x = 0.0;
	FLOAT64						y = 0.0;
	FLOAT64						firstLine;
	FLOAT64						lineNum;
	FLOAT64						value;
	INT32						nLine;

	(VOID)st_Graphics->SetSmoothingMode(Gdiplus::SmoothingModeNone);

	// 가로선 = 고도 또는 속력
	firstLine	= ceil(st_Info->valueMin / st_Info->valueGrid);
	lineNum		= floor(st_Info->valueMax / st_Info->valueGrid) - firstLine;

	for (nLine = 0; static_cast<FLOAT64>(nLine) <= lineNum; nLine++)
	{
		value = (firstLine + static_cast<FLOAT64>(nLine)) * st_Info->valueGrid;
		f_PanelToPixel(nPanel, 0, value, &x, &y);
		(VOID)st_Graphics->DrawLine(&st_GridPen, st_Info->st_Area.left, f_Plot_Round(y), st_Info->st_Area.right, f_Plot_Round(y));
		f_Plot_Text(st_Graphics, &st_Font, f_Plot_Number(value, f_Plot_DecimalFor(st_Info->valueGrid)), static_cast<FLOAT64>(st_Info->st_Area.left) - 6.0, y,
			Gdiplus::StringAlignmentFar, Gdiplus::StringAlignmentCenter, UI_COLOR_TEXT_SUB);
	}

	// 세로선 = 시각
	lineNum = floor(endTime / timeGrid);

	for (nLine = 0; static_cast<FLOAT64>(nLine) <= lineNum; nLine++)
	{
		value	= static_cast<FLOAT64>(nLine) * timeGrid;
		x		= static_cast<FLOAT64>(st_Info->st_Area.left) + ((value / endTime) * static_cast<FLOAT64>(st_Info->st_Area.Width()));
		(VOID)st_Graphics->DrawLine(&st_GridPen, f_Plot_Round(x), st_Info->st_Area.top, f_Plot_Round(x), st_Info->st_Area.bottom);
		f_Plot_Text(st_Graphics, &st_Font, f_Plot_Number(value, f_Plot_DecimalFor(timeGrid)), x, static_cast<FLOAT64>(st_Info->st_Area.bottom) + 4.0,
			Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentNear, UI_COLOR_TEXT_SUB);
	}

	(VOID)st_Graphics->DrawRectangle(&st_FramePen, st_Info->st_Area.left, st_Info->st_Area.top, st_Info->st_Area.Width(), st_Info->st_Area.Height());
	f_Plot_Text(st_Graphics, &st_Bold, CString(st_Info->pt_Title), static_cast<FLOAT64>(st_Info->st_Area.left), static_cast<FLOAT64>(st_Info->st_Area.top) - 5.0,
		Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentFar, UI_COLOR_TEXT);

	(VOID)st_Graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	(VOID)st_Graphics->SetClip(st_Clip);

	// 값 선. 비교 결과는 점선으로 위에 겹침.
	f_DrawPanelTrack(st_Graphics, nPanel, st_Result, 0);

	if (st_Compare != nullptr)
	{
		f_DrawPanelTrack(st_Graphics, nPanel, st_Compare, 1);
	}

	(VOID)st_Graphics->ResetClip();
}

// 시각 그래프 칸에 결과 하나의 값 선 그리기.
VOID CTrajectoryPlot::f_DrawPanelTrack(Gdiplus::Graphics *st_Graphics, INT32 nPanel, const CSimResult *st_Source, INT32 isCompare) const
{
	const FLOAT32					scale = static_cast<FLOAT32>(f_Ui_Scale(100, dpi)) / 100.0F;
	const INT32						nSampleNum = st_Source->f_GetSampleNum();
	std::vector<Gdiplus::PointF>	st_Line(static_cast<UINT64>(nSampleNum));
	FLOAT64							x = 0.0;
	FLOAT64							y = 0.0;
	INT32							nObject;
	INT32							nSample;

	for (nObject = 0; nObject < st_Source->f_GetObjectNum(); nObject++)
	{
		Gdiplus::Pen st_Pen(f_Plot_TrackColor(nObject, isCompare), ((isCompare != 0) ? 1.4F : 1.6F) * scale);

		f_Plot_SetTrackStyle(&st_Pen, isCompare);

		for (nSample = 0; nSample < nSampleNum; nSample++)
		{
			f_PanelToPixel(nPanel, nSample, f_PanelValue(st_Source, nPanel, nSample, nObject), &x, &y);
			st_Line[static_cast<UINT64>(nSample)] = Gdiplus::PointF(static_cast<FLOAT32>(x), static_cast<FLOAT32>(y));
		}

		(VOID)st_Graphics->DrawLines(&st_Pen, st_Line.data(), nSampleNum);
	}
}
