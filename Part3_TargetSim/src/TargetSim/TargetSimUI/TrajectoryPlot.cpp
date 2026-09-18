#include "pch.h"

#include <math.h>
#include <stdlib.h>

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

#define PLOT_MARGIN_LEFT		58						// [px @96dpi]
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
#define PLOT_MIN_CLIENT_HEIGHT	280						// 이보다 낮으면 고도 패널 생략
#define PLOT_MIN_SPAN			1000.0					// [m]
#define PLOT_SPAN_SCALE			1.18
#define PLOT_MIN_ALT_SPAN		10.0					// [m]
#define PLOT_MAX_GRID_NUM		8.0
#define PLOT_MAX_LINE_NUM		60
#define PLOT_PIXEL_LIMIT		1.0e6
#define PLOT_HEADING_PX			18.0
#define PLOT_ARROW_PX			7.0
#define PLOT_ARROW_ANGLE		(PI / 6.0)
#define PLOT_MIN_HEADING		1.0e-3					// [m/s]
#define PLOT_HANDLE_PX			36.0
#define PLOT_HIT_PX				10.0
#define PLOT_DRAG_START_PX		4
#define PLOT_FONT_PX			12.0
#define PLOT_LABEL_TRY_NUM		4
#define PLOT_LABEL_SIDE_MIN		0.3
#define PLOT_CAPTION_WIDTH		30.0F					// [px @96dpi] 구석의 축 이름 자리
#define PLOT_CAPTION_HEIGHT		22.0F

#define PLOT_DRAG_NONE			0
#define PLOT_DRAG_PENDING		1						// 눌렀지만 아직 끌지 않음
#define PLOT_DRAG_MOVE			2
#define PLOT_DRAG_TURN			3
#define PLOT_DRAG_SCRUB			4

#define PLOT_HIT_NONE			0
#define PLOT_HIT_START			1
#define PLOT_HIT_HANDLE			2
#define PLOT_HIT_ALT			3

#define PLOT_ALIGN_NEAR			0
#define PLOT_ALIGN_CENTER		1
#define PLOT_ALIGN_FAR			2

static Gdiplus::Color f_Plot_Color(COLORREF color, INT32 alpha)
{
	return Gdiplus::Color(static_cast<UINT8>(alpha), GetRValue(color), GetGValue(color), GetBValue(color));
}

static FLOAT32 f_Plot_Real(FLOAT64 value)
{
	FLOAT64 limited = value;

	if (limited > PLOT_PIXEL_LIMIT)
	{
		limited = PLOT_PIXEL_LIMIT;
	}
	else if (limited < -PLOT_PIXEL_LIMIT)
	{
		limited = -PLOT_PIXEL_LIMIT;
	}
	else
	{
		// 그대로
	}

	return static_cast<FLOAT32>(limited);
}

static INT32 f_Plot_Round(FLOAT64 value)
{
	return static_cast<INT32>(floor(static_cast<FLOAT64>(f_Plot_Real(value)) + 0.5));
}

// {1, 2, 5} x 10^k 가운데 눈금이 maxTickNum 개 이하가 되는 가장 작은 간격
static FLOAT64 f_Plot_NiceStep(FLOAT64 span, FLOAT64 maxTickNum)
{
	static const FLOAT64	s_Mantissa[3] = { 1.0, 2.0, 5.0 };
	FLOAT64					step = 1.0;
	FLOAT64					decade;
	INT32					nIndex;
	INT32					isFound = 0;

	if ((span > 0.0) && (maxTickNum > 0.0))
	{
		decade = pow(10.0, floor(log10(span / maxTickNum)) - 1.0);

		for (nIndex = 0; (nIndex < 12) && (isFound == 0); nIndex++)
		{
			step = decade * s_Mantissa[nIndex % 3];

			if ((span / step) <= maxTickNum)
			{
				isFound = 1;
			}
			else if ((nIndex % 3) == 2)
			{
				decade = decade * 10.0;
			}
			else
			{
				// 다음 가수
			}
		}
	}

	return step;
}

static INT32 f_Plot_DecimalFor(FLOAT64 step)
{
	INT32 nDecimal = 0;

	if ((step > 0.0) && (step < 1.0))
	{
		nDecimal = static_cast<INT32>(ceil(-log10(step) - 1.0e-9));
	}

	return (nDecimal > 6) ? 6 : nDecimal;
}

static VOID f_Plot_Text(Gdiplus::Graphics *st_Graphics, const Gdiplus::Font *st_Font, const CString &st_Text, FLOAT64 x, FLOAT64 y,
	INT32 alignX, INT32 alignY, COLORREF color)
{
	Gdiplus::StringFormat	st_Format;
	Gdiplus::SolidBrush		st_Brush(f_Plot_Color(color, 255));
	const Gdiplus::PointF	st_Origin(f_Plot_Real(x), f_Plot_Real(y));

	(VOID)st_Format.SetAlignment((alignX == PLOT_ALIGN_CENTER) ? Gdiplus::StringAlignmentCenter :
		((alignX == PLOT_ALIGN_FAR) ? Gdiplus::StringAlignmentFar : Gdiplus::StringAlignmentNear));
	(VOID)st_Format.SetLineAlignment((alignY == PLOT_ALIGN_CENTER) ? Gdiplus::StringAlignmentCenter :
		((alignY == PLOT_ALIGN_FAR) ? Gdiplus::StringAlignmentFar : Gdiplus::StringAlignmentNear));
	(VOID)st_Format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
	(VOID)st_Graphics->DrawString(st_Text.GetString(), -1, st_Font, st_Origin, &st_Format, &st_Brush);
}

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
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_CAPTURECHANGED()
	ON_WM_SETCURSOR()
END_MESSAGE_MAP()

CTrajectoryPlot::CTrajectoryPlot() noexcept
	: st_Result(nullptr)
	, dpi(UI_BASE_DPI)
	, nCurStep(0)
	, nSelObject(-1)
	, isBackValid(0)
	, isViewFrozen(0)
	, dragKind(PLOT_DRAG_NONE)
	, nDragObject(-1)
	, pixelPerMeter(1.0)
	, centerEast(0.0)
	, centerNorth(0.0)
	, gridMeter(1000.0)
	, altMin(0.0)
	, altMax(1.0)
	, altGrid(1.0)
	, timeGrid(1.0)
	, st_MapArea(0, 0, 0, 0)
	, st_AltArea(0, 0, 0, 0)
	, st_FrameSize(0, 0)
	, st_DragOffset(0, 0)
	, st_DownPoint(0, 0)
{
}

VOID CTrajectoryPlot::f_SetDpi(INT32 newDpi)
{
	dpi			= (newDpi > 0) ? newDpi : UI_BASE_DPI;
	isBackValid	= 0;
}

VOID CTrajectoryPlot::f_SetResult(const CSimResult *st_NewResult)
{
	st_Result	= st_NewResult;
	isBackValid	= 0;

	if (f_HasData() == 0)
	{
		nCurStep = 0;
	}
	else if (nCurStep >= st_Result->f_GetSampleNum())
	{
		nCurStep = st_Result->f_GetSampleNum() - 1;
	}
	else
	{
		// 같은 스텝
	}

	if (GetSafeHwnd() != nullptr)
	{
		Invalidate(FALSE);
	}
}

VOID CTrajectoryPlot::f_SetStep(INT32 nStep)
{
	if ((f_HasData() != 0) && (nStep >= 0) && (nStep < st_Result->f_GetSampleNum()) && (nStep != nCurStep))
	{
		nCurStep = nStep;

		if (GetSafeHwnd() != nullptr)
		{
			Invalidate(FALSE);
		}
	}
}

VOID CTrajectoryPlot::f_SetSelObject(INT32 nObject)
{
	if (nObject != nSelObject)
	{
		// 고른 객체의 궤적은 굵게 그려 배경에 들어 있다.
		nSelObject	= nObject;
		isBackValid	= 0;

		if (GetSafeHwnd() != nullptr)
		{
			Invalidate(FALSE);
		}
	}
}

INT32 CTrajectoryPlot::f_HasData(VOID) const
{
	return ((st_Result != nullptr) && (st_Result->f_GetSampleNum() > 0) && (st_Result->f_GetObjectNum() > 0)) ? 1 : 0;
}

BOOL CTrajectoryPlot::OnEraseBkgnd(CDC *st_Dc)
{
	UNREFERENCED_PARAMETER(st_Dc);

	return TRUE;
}

VOID CTrajectoryPlot::OnSize(UINT32 type, INT32 width, INT32 height)
{
	CStatic::OnSize(type, width, height);
	isBackValid = 0;
	Invalidate(FALSE);
}

VOID CTrajectoryPlot::OnPaint(VOID)
{
	CPaintDC	st_Dc(this);
	CDC			st_MemDc;
	CDC			st_BackDc;
	CRect		st_Client;
	CBitmap		*st_OldBitmap;
	CBitmap		*st_OldBackBitmap;

	GetClientRect(&st_Client);

	if ((st_Client.Width() > 0) && (st_Client.Height() > 0) && (st_MemDc.CreateCompatibleDC(&st_Dc) != FALSE))
	{
		if ((st_FrameBitmap.GetSafeHandle() == nullptr) || (st_FrameSize != st_Client.Size()))
		{
			(VOID)st_FrameBitmap.DeleteObject();
			(VOID)st_FrameBitmap.CreateCompatibleBitmap(&st_Dc, st_Client.Width(), st_Client.Height());
			st_FrameSize	= st_Client.Size();
			isBackValid		= 0;
		}

		if (st_FrameBitmap.GetSafeHandle() != nullptr)
		{
			st_OldBitmap = st_MemDc.SelectObject(&st_FrameBitmap);

			// 격자, 궤적, 이름표는 배경 비트맵에 두고 재생 중에는 현재 표식만 다시 그린다.
			if ((f_HasData() != 0) && (isBackValid == 0))
			{
				f_BuildBackground(&st_Dc, st_Client);
			}

			if ((f_HasData() != 0) && (isBackValid != 0) && (st_BackDc.CreateCompatibleDC(&st_Dc) != FALSE))
			{
				Gdiplus::Graphics st_Graphics(st_MemDc.GetSafeHdc());

				st_OldBackBitmap = st_BackDc.SelectObject(&st_BackBitmap);
				(VOID)st_MemDc.BitBlt(0, 0, st_Client.Width(), st_Client.Height(), &st_BackDc, 0, 0, SRCCOPY);
				(VOID)st_BackDc.SelectObject(st_OldBackBitmap);

				(VOID)st_Graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
				(VOID)st_Graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
				f_DrawOverlay(&st_Graphics);
			}
			else
			{
				f_DrawEmpty(&st_MemDc, st_Client);
			}

			(VOID)st_Dc.BitBlt(0, 0, st_Client.Width(), st_Client.Height(), &st_MemDc, 0, 0, SRCCOPY);
			(VOID)st_MemDc.SelectObject(st_OldBitmap);
		}
	}
}

VOID CTrajectoryPlot::f_DrawEmpty(CDC *st_Dc, const CRect &st_Client) const
{
	CFont	*st_OldFont = nullptr;
	CWnd	*st_Parent = GetParent();
	CRect	st_Text = st_Client;

	st_Dc->FillSolidRect(&st_Client, UI_COLOR_CARD);

	if ((st_Parent != nullptr) && (st_Parent->GetFont() != nullptr))
	{
		st_OldFont = st_Dc->SelectObject(st_Parent->GetFont());
	}

	(VOID)st_Dc->SetBkMode(TRANSPARENT);
	(VOID)st_Dc->SetTextColor(UI_COLOR_TEXT_SUB);
	(VOID)st_Dc->DrawText(CString(_T("표시할 결과가 없습니다")), &st_Text, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

	if (st_OldFont != nullptr)
	{
		(VOID)st_Dc->SelectObject(st_OldFont);
	}
}

VOID CTrajectoryPlot::f_ComputeLayout(const CRect &st_Client)
{
	const INT32	left = st_Client.left + f_Ui_Scale(PLOT_MARGIN_LEFT, dpi);
	const INT32	right = st_Client.right - f_Ui_Scale(PLOT_MARGIN_RIGHT, dpi);
	const INT32	top = st_Client.top + f_Ui_Scale(PLOT_TITLE_HEIGHT, dpi);
	const INT32	bottom = st_Client.bottom - f_Ui_Scale(PLOT_AXIS_HEIGHT, dpi);
	INT32		altBlock;

	if (st_Client.Height() < f_Ui_Scale(PLOT_MIN_CLIENT_HEIGHT, dpi))
	{
		st_AltArea.SetRectEmpty();
		st_MapArea.SetRect(left, top, right, bottom);
	}
	else if (static_cast<FLOAT64>(st_Client.Width()) >= (PLOT_WIDE_RATIO * static_cast<FLOAT64>(st_Client.Height())))
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

VOID CTrajectoryPlot::f_ComputeView(VOID)
{
	const INT32				nSampleNum = st_Result->f_GetSampleNum();
	const INT32				nObjectNum = st_Result->f_GetObjectNum();
	const ST_PlotPoint		*st_Point;
	const ST_TargetState	*st_State;
	FLOAT64					minEast = 0.0;
	FLOAT64					maxEast = 0.0;
	FLOAT64					minNorth = 0.0;
	FLOAT64					maxNorth = 0.0;
	FLOAT64					lowAlt = HUGE_VAL;
	FLOAT64					highAlt = -HUGE_VAL;
	FLOAT64					spanEast;
	FLOAT64					spanNorth;
	FLOAT64					spanAlt;
	FLOAT64					width;
	FLOAT64					height;
	INT32					nStep;
	INT32					nObject;

	// 원점(플랫폼 초기 위치)은 늘 들어간다.
	for (nStep = 0; nStep < nSampleNum; nStep++)
	{
		for (nObject = 0; nObject < nObjectNum; nObject++)
		{
			st_Point = st_Result->f_GetPoint(nStep, nObject);
			st_State = st_Result->f_GetState(nStep, nObject);

			if ((st_Point != nullptr) && (st_State != nullptr))
			{
				minEast		= fmin(minEast, st_Point->east);
				maxEast		= fmax(maxEast, st_Point->east);
				minNorth	= fmin(minNorth, st_Point->north);
				maxNorth	= fmax(maxNorth, st_Point->north);
				lowAlt		= fmin(lowAlt, st_State->st_Lla.alt);
				highAlt		= fmax(highAlt, st_State->st_Lla.alt);
			}
		}
	}

	if (isViewFrozen == 0)
	{
		spanEast	= fmax(maxEast - minEast, PLOT_MIN_SPAN) * PLOT_SPAN_SCALE;
		spanNorth	= fmax(maxNorth - minNorth, PLOT_MIN_SPAN) * PLOT_SPAN_SCALE;
		width		= fmax(static_cast<FLOAT64>(st_MapArea.Width()), 1.0);
		height		= fmax(static_cast<FLOAT64>(st_MapArea.Height()), 1.0);

		// 등축척
		pixelPerMeter	= fmin(width / spanEast, height / spanNorth);
		centerEast		= 0.5 * (minEast + maxEast);
		centerNorth		= 0.5 * (minNorth + maxNorth);
		gridMeter		= f_Plot_NiceStep(fmax(width, height) / pixelPerMeter, PLOT_MAX_GRID_NUM);
	}

	if (!(lowAlt <= highAlt))
	{
		lowAlt	= 0.0;
		highAlt	= 0.0;
	}

	spanAlt = highAlt - lowAlt;

	if (spanAlt < PLOT_MIN_ALT_SPAN)
	{
		lowAlt	= lowAlt - (0.5 * (PLOT_MIN_ALT_SPAN - spanAlt));
		spanAlt	= PLOT_MIN_ALT_SPAN;
	}

	altMin		= lowAlt - (0.10 * spanAlt);
	altMax		= lowAlt + (1.10 * spanAlt);
	altGrid		= f_Plot_NiceStep(altMax - altMin, 4.0);
	timeGrid	= f_Plot_NiceStep(fmax(static_cast<FLOAT64>(nSampleNum - 1) * st_Result->f_GetStepTime(), 1.0e-3), 10.0);
}

VOID CTrajectoryPlot::f_MapToPixel(const ST_PlotPoint *st_Point, FLOAT64 *pt_X, FLOAT64 *pt_Y) const
{
	*pt_X = static_cast<FLOAT64>(st_MapArea.left) + (0.5 * static_cast<FLOAT64>(st_MapArea.Width())) + ((st_Point->east - centerEast) * pixelPerMeter);
	*pt_Y = static_cast<FLOAT64>(st_MapArea.top) + (0.5 * static_cast<FLOAT64>(st_MapArea.Height())) - ((st_Point->north - centerNorth) * pixelPerMeter);
}

VOID CTrajectoryPlot::f_AltToPixel(INT32 nStep, FLOAT64 alt, FLOAT64 *pt_X, FLOAT64 *pt_Y) const
{
	const INT32		nLastStep = st_Result->f_GetSampleNum() - 1;
	const FLOAT64	ratioX = (nLastStep > 0) ? (static_cast<FLOAT64>(nStep) / static_cast<FLOAT64>(nLastStep)) : 0.0;
	const FLOAT64	ratioY = (altMax > altMin) ? ((alt - altMin) / (altMax - altMin)) : 0.0;

	*pt_X = static_cast<FLOAT64>(st_AltArea.left) + (ratioX * static_cast<FLOAT64>(st_AltArea.Width()));
	*pt_Y = static_cast<FLOAT64>(st_AltArea.bottom) - (ratioY * static_cast<FLOAT64>(st_AltArea.Height()));
}

VOID CTrajectoryPlot::f_BuildBackground(CDC *st_Dc, const CRect &st_Client)
{
	CDC		st_MemDc;
	CBitmap	*st_OldBitmap;

	(VOID)st_BackBitmap.DeleteObject();

	if ((st_BackBitmap.CreateCompatibleBitmap(st_Dc, st_Client.Width(), st_Client.Height()) != FALSE) &&
		(st_MemDc.CreateCompatibleDC(st_Dc) != FALSE))
	{
		st_OldBitmap = st_MemDc.SelectObject(&st_BackBitmap);
		st_MemDc.FillSolidRect(&st_Client, UI_COLOR_CARD);

		f_ComputeLayout(st_Client);
		f_ComputeView();

		{
			Gdiplus::Graphics st_Graphics(st_MemDc.GetSafeHdc());

			(VOID)st_Graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
			f_DrawMap(&st_Graphics);

			if (st_AltArea.IsRectEmpty() == FALSE)
			{
				f_DrawAltitude(&st_Graphics);
			}
		}

		(VOID)st_MemDc.SelectObject(st_OldBitmap);
		isBackValid = 1;
	}
}

VOID CTrajectoryPlot::f_DrawMap(Gdiplus::Graphics *st_Graphics) const
{
	const FLOAT32			fontPx = static_cast<FLOAT32>(f_Ui_Scale(100, dpi)) * static_cast<FLOAT32>(PLOT_FONT_PX) / 100.0F;
	const Gdiplus::FontFamily	st_Family(L"Malgun Gothic");
	const Gdiplus::Font		st_Font(&st_Family, fontPx, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	const Gdiplus::Font		st_Bold(&st_Family, fontPx, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
	Gdiplus::Pen			st_GridPen(f_Plot_Color(UI_COLOR_GRID_LINE, 255), 1.0F);
	Gdiplus::Pen			st_AxisPen(f_Plot_Color(UI_COLOR_TEXT_OFF, 255), 1.0F);
	Gdiplus::Pen			st_FramePen(f_Plot_Color(UI_COLOR_BORDER, 255), 1.0F);
	const Gdiplus::Rect		st_Clip(st_MapArea.left, st_MapArea.top, st_MapArea.Width(), st_MapArea.Height());
	const INT32				nSampleNum = st_Result->f_GetSampleNum();
	const INT32				nObjectNum = st_Result->f_GetObjectNum();
	const FLOAT64			halfEast = (0.5 * static_cast<FLOAT64>(st_MapArea.Width())) / pixelPerMeter;
	const FLOAT64			halfNorth = (0.5 * static_cast<FLOAT64>(st_MapArea.Height())) / pixelPerMeter;
	const FLOAT64			firstEast = ceil((centerEast - halfEast) / gridMeter);
	const FLOAT64			firstNorth = ceil((centerNorth - halfNorth) / gridMeter);
	const FLOAT64			lineEastNum = floor((centerEast + halfEast) / gridMeter) - firstEast;
	const FLOAT64			lineNorthNum = floor((centerNorth + halfNorth) / gridMeter) - firstNorth;
	const INT32				nDecimal = f_Plot_DecimalFor(gridMeter / 1000.0);
	Gdiplus::Point			*st_Line;
	ST_PlotPoint			st_Point;
	FLOAT64					x = 0.0;
	FLOAT64					y = 0.0;
	FLOAT64					index;
	INT32					nLine;
	INT32					nObject;
	INT32					nSample;
	INT32					nCount;

	// 격자와 눈금은 또렷하게, 궤적은 부드럽게
	(VOID)st_Graphics->SetSmoothingMode(Gdiplus::SmoothingModeNone);

	for (nLine = 0; (nLine <= PLOT_MAX_LINE_NUM) && (static_cast<FLOAT64>(nLine) <= lineEastNum); nLine++)
	{
		index			= firstEast + static_cast<FLOAT64>(nLine);
		st_Point.east	= index * gridMeter;
		st_Point.north	= centerNorth;
		f_MapToPixel(&st_Point, &x, &y);

		(VOID)st_Graphics->DrawLine((fabs(index) < 0.5) ? &st_AxisPen : &st_GridPen, f_Plot_Round(x), st_MapArea.top, f_Plot_Round(x), st_MapArea.bottom);
		f_Plot_Text(st_Graphics, &st_Font, f_Num_ToText(st_Point.east / 1000.0, nDecimal), x, static_cast<FLOAT64>(st_MapArea.bottom) + 4.0,
			PLOT_ALIGN_CENTER, PLOT_ALIGN_NEAR, UI_COLOR_TEXT_SUB);
	}

	for (nLine = 0; (nLine <= PLOT_MAX_LINE_NUM) && (static_cast<FLOAT64>(nLine) <= lineNorthNum); nLine++)
	{
		index			= firstNorth + static_cast<FLOAT64>(nLine);
		st_Point.east	= centerEast;
		st_Point.north	= index * gridMeter;
		f_MapToPixel(&st_Point, &x, &y);

		(VOID)st_Graphics->DrawLine((fabs(index) < 0.5) ? &st_AxisPen : &st_GridPen, st_MapArea.left, f_Plot_Round(y), st_MapArea.right, f_Plot_Round(y));
		f_Plot_Text(st_Graphics, &st_Font, f_Num_ToText(st_Point.north / 1000.0, nDecimal), static_cast<FLOAT64>(st_MapArea.left) - 6.0, y,
			PLOT_ALIGN_FAR, PLOT_ALIGN_CENTER, UI_COLOR_TEXT_SUB);
	}

	(VOID)st_Graphics->DrawRectangle(&st_FramePen, st_MapArea.left, st_MapArea.top, st_MapArea.Width(), st_MapArea.Height());

	f_Plot_Text(st_Graphics, &st_Bold, CString(_T("궤적 (플랫폼 기준 동-북, km)")), static_cast<FLOAT64>(st_MapArea.left), static_cast<FLOAT64>(st_MapArea.top) - 5.0,
		PLOT_ALIGN_NEAR, PLOT_ALIGN_FAR, UI_COLOR_TEXT);
	f_Plot_Text(st_Graphics, &st_Font, CString(_T("E")), static_cast<FLOAT64>(st_MapArea.right) - 6.0, static_cast<FLOAT64>(st_MapArea.bottom) - 4.0,
		PLOT_ALIGN_FAR, PLOT_ALIGN_FAR, UI_COLOR_TEXT_OFF);
	f_Plot_Text(st_Graphics, &st_Font, CString(_T("N")), static_cast<FLOAT64>(st_MapArea.left) + 6.0, static_cast<FLOAT64>(st_MapArea.top) + 4.0,
		PLOT_ALIGN_NEAR, PLOT_ALIGN_NEAR, UI_COLOR_TEXT_OFF);

	(VOID)st_Graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	(VOID)st_Graphics->SetClip(st_Clip);

	st_Line = static_cast<Gdiplus::Point *>(malloc(static_cast<UINT64>(nSampleNum) * static_cast<UINT64>(sizeof(Gdiplus::Point))));

	if (st_Line != nullptr)
	{
		for (nObject = 0; nObject < nObjectNum; nObject++)
		{
			Gdiplus::Pen st_TrackPen(f_Plot_Color(f_Ui_ObjectColor(nObject), 255),
				static_cast<FLOAT32>(f_Ui_Scale(100, dpi)) * ((nObject == nSelObject) ? 3.0F : 2.0F) / 100.0F);

			(VOID)st_TrackPen.SetLineJoin(Gdiplus::LineJoinRound);

			// 같은 픽셀의 점은 버린다.
			nCount = 0;

			for (nSample = 0; nSample < nSampleNum; nSample++)
			{
				f_MapToPixel(st_Result->f_GetPoint(nSample, nObject), &x, &y);

				if ((nCount == 0) || (f_Plot_Round(x) != st_Line[nCount - 1].X) || (f_Plot_Round(y) != st_Line[nCount - 1].Y))
				{
					st_Line[nCount].X = f_Plot_Round(x);
					st_Line[nCount].Y = f_Plot_Round(y);
					nCount = nCount + 1;
				}
			}

			if (nCount >= 2)
			{
				(VOID)st_Graphics->DrawLines(&st_TrackPen, st_Line, nCount);
			}
		}

		free(st_Line);
	}

	// 시작점
	for (nObject = 0; nObject < nObjectNum; nObject++)
	{
		const FLOAT32		radius = static_cast<FLOAT32>(f_Ui_Scale(100, dpi)) * 5.0F / 100.0F;
		Gdiplus::SolidBrush	st_Fill(f_Plot_Color(UI_COLOR_CARD, 255));
		Gdiplus::Pen		st_Ring(f_Plot_Color(f_Ui_ObjectColor(nObject), 255), static_cast<FLOAT32>(f_Ui_Scale(100, dpi)) * 2.0F / 100.0F);

		f_MapToPixel(st_Result->f_GetPoint(0, nObject), &x, &y);
		(VOID)st_Graphics->FillEllipse(&st_Fill, f_Plot_Real(x) - radius, f_Plot_Real(y) - radius, 2.0F * radius, 2.0F * radius);
		(VOID)st_Graphics->DrawEllipse(&st_Ring, f_Plot_Real(x) - radius, f_Plot_Real(y) - radius, 2.0F * radius, 2.0F * radius);
	}

	f_DrawStartLabels(st_Graphics, &st_Font);
	(VOID)st_Graphics->ResetClip();
}

// 이름표는 시작점 둘레 네 자리 중 그림 안에 들고 다른 이름표, 시작점, 기수 손잡이, 축 이름과 겹치지 않는 첫 자리에 놓는다.
VOID CTrajectoryPlot::f_DrawStartLabels(Gdiplus::Graphics *st_Graphics, const Gdiplus::Font *st_Font) const
{
	const FLOAT32		scale = static_cast<FLOAT32>(f_Ui_Scale(100, dpi)) / 100.0F;
	const FLOAT32		reach = 8.0F * scale;
	const INT32			nObjectNum = (st_Result->f_GetObjectNum() < UI_OBJECT_NUM) ? st_Result->f_GetObjectNum() : UI_OBJECT_NUM;
	const Gdiplus::RectF	st_Map(static_cast<FLOAT32>(st_MapArea.left), static_cast<FLOAT32>(st_MapArea.top),
							static_cast<FLOAT32>(st_MapArea.Width()), static_cast<FLOAT32>(st_MapArea.Height()));
	Gdiplus::SolidBrush	st_Plate(f_Plot_Color(UI_COLOR_CARD, 215));
	Gdiplus::RectF		st_Placed[UI_OBJECT_NUM];
	Gdiplus::RectF		st_Marker[UI_OBJECT_NUM];
	Gdiplus::RectF		st_Caption[3];
	Gdiplus::RectF		st_Size;
	Gdiplus::RectF		st_Try;
	Gdiplus::RectF		st_Best;
	FLOAT64				x = 0.0;
	FLOAT64				y = 0.0;
	FLOAT64				handleX = 0.0;
	FLOAT64				handleY = 0.0;
	INT32				nObject;
	INT32				nOther;
	INT32				nTry;
	INT32				nBlock;
	INT32				isLeftFirst;
	INT32				isBelowFirst;
	INT32				isLeft;
	INT32				isBelow;
	INT32				isInside;
	INT32				isFree;
	INT32				isKept;
	INT32				isPlaced;

	for (nObject = 0; nObject < nObjectNum; nObject++)
	{
		f_MapToPixel(st_Result->f_GetPoint(0, nObject), &x, &y);
		st_Marker[nObject] = Gdiplus::RectF(f_Plot_Real(x) - reach, f_Plot_Real(y) - reach, 2.0F * reach, 2.0F * reach);
	}

	st_Caption[0] = Gdiplus::RectF(st_Map.X, st_Map.Y, PLOT_CAPTION_WIDTH * scale, PLOT_CAPTION_HEIGHT * scale);
	st_Caption[1] = Gdiplus::RectF(st_Map.GetRight() - (PLOT_CAPTION_WIDTH * scale), st_Map.GetBottom() - (PLOT_CAPTION_HEIGHT * scale),
					PLOT_CAPTION_WIDTH * scale, PLOT_CAPTION_HEIGHT * scale);
	st_Caption[2] = Gdiplus::RectF(0.0F, 0.0F, 0.0F, 0.0F);

	// 고른 표적의 기수 손잡이 (겹쳐 그리는 쪽)
	if (f_GetHandlePixel(nSelObject, &handleX, &handleY) != 0)
	{
		st_Caption[2] = Gdiplus::RectF(f_Plot_Real(handleX) - reach, f_Plot_Real(handleY) - reach, 2.0F * reach, 2.0F * reach);
	}

	for (nObject = 0; nObject < nObjectNum; nObject++)
	{
		const CString	st_Name = f_Plot_ObjectName(nObject);
		const FLOAT64	yaw = st_Result->f_GetState(0, nObject)->st_Att.yaw;

		(VOID)st_Graphics->MeasureString(st_Name.GetString(), -1, st_Font, Gdiplus::PointF(0.0F, 0.0F), &st_Size);

		// 출발 방향 반대쪽부터. 남북 성분이 작으면 이웃끼리 위아래를 엇갈린다.
		isLeftFirst = (sin(yaw) > PLOT_LABEL_SIDE_MIN) ? 1 : 0;

		if (cos(yaw) > PLOT_LABEL_SIDE_MIN)
		{
			isBelowFirst = 1;
		}
		else if (cos(yaw) < -PLOT_LABEL_SIDE_MIN)
		{
			isBelowFirst = 0;
		}
		else
		{
			isBelowFirst = ((nObject % 2) == 0) ? 1 : 0;
		}

		isKept		= 0;
		isPlaced	= 0;

		for (nTry = 0; (nTry < PLOT_LABEL_TRY_NUM) && (isPlaced == 0); nTry++)
		{
			isLeft	= (nTry >= 2) ? (1 - isLeftFirst) : isLeftFirst;
			isBelow	= ((nTry % 2) == 0) ? isBelowFirst : (1 - isBelowFirst);

			st_Try = Gdiplus::RectF(
				(isLeft != 0) ? (st_Marker[nObject].X - st_Size.Width + 2.0F) : (st_Marker[nObject].GetRight() - 2.0F),
				(isBelow != 0) ? (st_Marker[nObject].GetBottom() - 4.0F) : (st_Marker[nObject].Y - st_Size.Height + 4.0F),
				st_Size.Width, st_Size.Height);

			isInside	= (st_Map.Contains(st_Try) != FALSE) ? 1 : 0;
			isFree		= isInside;

			for (nBlock = 0; (nBlock < 3) && (isFree != 0); nBlock++)
			{
				if (st_Try.IntersectsWith(st_Caption[nBlock]) != FALSE)
				{
					isFree = 0;
				}
			}

			for (nOther = 0; (nOther < nObjectNum) && (isFree != 0); nOther++)
			{
				if (((nOther < nObject) && (st_Try.IntersectsWith(st_Placed[nOther]) != FALSE)) ||
					((nOther != nObject) && (st_Try.IntersectsWith(st_Marker[nOther]) != FALSE)))
				{
					isFree = 0;
				}
			}

			if (isFree != 0)
			{
				st_Best		= st_Try;
				isPlaced	= 1;
			}
			else if ((isInside != 0) && (isKept == 0))
			{
				st_Best	= st_Try;
				isKept	= 1;
			}
			else if (nTry == 0)
			{
				st_Best = st_Try;
			}
			else
			{
				// 이미 잡은 자리
			}
		}

		st_Placed[nObject] = st_Best;

		(VOID)st_Graphics->FillRectangle(&st_Plate, st_Best.X + 1.0F, st_Best.Y + 1.0F, st_Best.Width - 2.0F, st_Best.Height - 2.0F);
		f_Plot_Text(st_Graphics, st_Font, st_Name, static_cast<FLOAT64>(st_Best.X), static_cast<FLOAT64>(st_Best.Y),
			PLOT_ALIGN_NEAR, PLOT_ALIGN_NEAR, f_Ui_ObjectColor(nObject));
	}
}

VOID CTrajectoryPlot::f_DrawAltitude(Gdiplus::Graphics *st_Graphics) const
{
	const FLOAT32				fontPx = static_cast<FLOAT32>(f_Ui_Scale(100, dpi)) * static_cast<FLOAT32>(PLOT_FONT_PX) / 100.0F;
	const Gdiplus::FontFamily	st_Family(L"Malgun Gothic");
	const Gdiplus::Font			st_Font(&st_Family, fontPx, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	const Gdiplus::Font			st_Bold(&st_Family, fontPx, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
	Gdiplus::Pen				st_GridPen(f_Plot_Color(UI_COLOR_GRID_LINE, 255), 1.0F);
	Gdiplus::Pen				st_FramePen(f_Plot_Color(UI_COLOR_BORDER, 255), 1.0F);
	const Gdiplus::Rect			st_Clip(st_AltArea.left, st_AltArea.top, st_AltArea.Width(), st_AltArea.Height());
	const INT32					nSampleNum = st_Result->f_GetSampleNum();
	const INT32					nObjectNum = st_Result->f_GetObjectNum();
	const FLOAT64				endTime = static_cast<FLOAT64>(nSampleNum - 1) * st_Result->f_GetStepTime();
	const INT32					nAltDecimal = f_Plot_DecimalFor(altGrid);
	const INT32					nTimeDecimal = f_Plot_DecimalFor(timeGrid);
	Gdiplus::Point				*st_Line;
	FLOAT64						x = 0.0;
	FLOAT64						y = 0.0;
	FLOAT64						value;
	INT32						nLine;
	INT32						nObject;
	INT32						nSample;
	INT32						nCount;

	(VOID)st_Graphics->SetSmoothingMode(Gdiplus::SmoothingModeNone);

	value = ceil(altMin / altGrid) * altGrid;

	for (nLine = 0; (nLine <= PLOT_MAX_LINE_NUM) && (value <= altMax); nLine++)
	{
		f_AltToPixel(0, value, &x, &y);
		(VOID)st_Graphics->DrawLine(&st_GridPen, st_AltArea.left, f_Plot_Round(y), st_AltArea.right, f_Plot_Round(y));
		f_Plot_Text(st_Graphics, &st_Font, f_Num_ToText(value, nAltDecimal), static_cast<FLOAT64>(st_AltArea.left) - 6.0, y,
			PLOT_ALIGN_FAR, PLOT_ALIGN_CENTER, UI_COLOR_TEXT_SUB);
		value = value + altGrid;
	}

	value = 0.0;

	for (nLine = 0; (nLine <= PLOT_MAX_LINE_NUM) && (value <= (endTime + (1.0e-9 * timeGrid))) && (endTime > 0.0); nLine++)
	{
		x = static_cast<FLOAT64>(st_AltArea.left) + ((value / endTime) * static_cast<FLOAT64>(st_AltArea.Width()));
		(VOID)st_Graphics->DrawLine(&st_GridPen, f_Plot_Round(x), st_AltArea.top, f_Plot_Round(x), st_AltArea.bottom);
		f_Plot_Text(st_Graphics, &st_Font, f_Num_ToText(value, nTimeDecimal), x, static_cast<FLOAT64>(st_AltArea.bottom) + 4.0,
			PLOT_ALIGN_CENTER, PLOT_ALIGN_NEAR, UI_COLOR_TEXT_SUB);
		value = value + timeGrid;
	}

	(VOID)st_Graphics->DrawRectangle(&st_FramePen, st_AltArea.left, st_AltArea.top, st_AltArea.Width(), st_AltArea.Height());
	f_Plot_Text(st_Graphics, &st_Bold, CString(_T("고도 (m) / 시각 (s)")), static_cast<FLOAT64>(st_AltArea.left), static_cast<FLOAT64>(st_AltArea.top) - 5.0,
		PLOT_ALIGN_NEAR, PLOT_ALIGN_FAR, UI_COLOR_TEXT);

	(VOID)st_Graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	(VOID)st_Graphics->SetClip(st_Clip);

	st_Line = static_cast<Gdiplus::Point *>(malloc(static_cast<UINT64>(nSampleNum) * static_cast<UINT64>(sizeof(Gdiplus::Point))));

	if (st_Line != nullptr)
	{
		for (nObject = 0; nObject < nObjectNum; nObject++)
		{
			Gdiplus::Pen st_Pen(f_Plot_Color(f_Ui_ObjectColor(nObject), 255),
				static_cast<FLOAT32>(f_Ui_Scale(100, dpi)) * ((nObject == nSelObject) ? 2.5F : 1.6F) / 100.0F);

			(VOID)st_Pen.SetLineJoin(Gdiplus::LineJoinRound);
			nCount = 0;

			for (nSample = 0; nSample < nSampleNum; nSample++)
			{
				f_AltToPixel(nSample, st_Result->f_GetState(nSample, nObject)->st_Lla.alt, &x, &y);

				if ((nCount == 0) || (f_Plot_Round(x) != st_Line[nCount - 1].X) || (f_Plot_Round(y) != st_Line[nCount - 1].Y))
				{
					st_Line[nCount].X = f_Plot_Round(x);
					st_Line[nCount].Y = f_Plot_Round(y);
					nCount = nCount + 1;
				}
			}

			if (nCount >= 2)
			{
				(VOID)st_Graphics->DrawLines(&st_Pen, st_Line, nCount);
			}
		}

		free(st_Line);
	}

	(VOID)st_Graphics->ResetClip();
}

INT32 CTrajectoryPlot::f_GetHandlePixel(INT32 nObject, FLOAT64 *pt_X, FLOAT64 *pt_Y) const
{
	const ST_TargetState	*st_State;
	const FLOAT64			length = static_cast<FLOAT64>(f_Ui_Scale(100, dpi)) * PLOT_HANDLE_PX / 100.0;
	FLOAT64					x = 0.0;
	FLOAT64					y = 0.0;
	INT32					isOk = 0;

	// 기수 손잡이는 표적에만
	if ((f_HasData() != 0) && (nObject >= 1) && (nObject < st_Result->f_GetObjectNum()))
	{
		st_State = st_Result->f_GetState(0, nObject);
		f_MapToPixel(st_Result->f_GetPoint(0, nObject), &x, &y);

		*pt_X = x + (length * sin(st_State->st_Att.yaw));
		*pt_Y = y - (length * cos(st_State->st_Att.yaw));
		isOk = 1;
	}

	return isOk;
}

VOID CTrajectoryPlot::f_DrawOverlay(Gdiplus::Graphics *st_Graphics) const
{
	const FLOAT32				scale = static_cast<FLOAT32>(f_Ui_Scale(100, dpi)) / 100.0F;
	const FLOAT32				fontPx = scale * static_cast<FLOAT32>(PLOT_FONT_PX);
	const Gdiplus::FontFamily	st_Family(L"Malgun Gothic");
	const Gdiplus::Font			st_Font(&st_Family, fontPx, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	const INT32					nObjectNum = st_Result->f_GetObjectNum();
	const ST_SimSample			*st_Origin = st_Result->f_GetSample(0);
	const ST_SimSample			*st_Sample = st_Result->f_GetSample(nCurStep);
	const ST_TargetState		*st_State;
	const Gdiplus::Rect			st_MapClip(st_MapArea.left, st_MapArea.top, st_MapArea.Width() + 1, st_MapArea.Height() + 1);
	ST_CoordRect				st_VelNed;
	FLOAT64						x = 0.0;
	FLOAT64						y = 0.0;
	FLOAT64						handleX = 0.0;
	FLOAT64						handleY = 0.0;
	FLOAT64						headingNorm;
	INT32						nObject;

	if ((st_Origin != nullptr) && (st_Sample != nullptr))
	{
		(VOID)st_Graphics->SetClip(st_MapClip);

		// 고른 객체의 시작점 강조, 표적이면 기수 손잡이
		if ((nSelObject >= 0) && (nSelObject < nObjectNum))
		{
			const FLOAT32	ringRadius = 9.0F * scale;
			Gdiplus::Pen	st_Halo(f_Plot_Color(f_Ui_ObjectColor(nSelObject), 90), 3.0F * scale);

			f_MapToPixel(st_Result->f_GetPoint(0, nSelObject), &x, &y);
			(VOID)st_Graphics->DrawEllipse(&st_Halo, f_Plot_Real(x) - ringRadius, f_Plot_Real(y) - ringRadius, 2.0F * ringRadius, 2.0F * ringRadius);

			if (f_GetHandlePixel(nSelObject, &handleX, &handleY) != 0)
			{
				const FLOAT32		knob = 5.0F * scale;
				Gdiplus::Pen		st_Stem(f_Plot_Color(f_Ui_ObjectColor(nSelObject), 200), 1.5F * scale);
				Gdiplus::SolidBrush	st_KnobFill(f_Plot_Color(UI_COLOR_CARD, 255));
				Gdiplus::Pen		st_KnobRing(f_Plot_Color(f_Ui_ObjectColor(nSelObject), 255), 2.0F * scale);

				(VOID)st_Stem.SetDashStyle(Gdiplus::DashStyleDash);
				(VOID)st_Graphics->DrawLine(&st_Stem, f_Plot_Real(x), f_Plot_Real(y), f_Plot_Real(handleX), f_Plot_Real(handleY));
				(VOID)st_Graphics->FillEllipse(&st_KnobFill, f_Plot_Real(handleX) - knob, f_Plot_Real(handleY) - knob, 2.0F * knob, 2.0F * knob);
				(VOID)st_Graphics->DrawEllipse(&st_KnobRing, f_Plot_Real(handleX) - knob, f_Plot_Real(handleY) - knob, 2.0F * knob, 2.0F * knob);
			}
		}

		for (nObject = 0; nObject < nObjectNum; nObject++)
		{
			const COLORREF		color = f_Ui_ObjectColor(nObject);
			const FLOAT32		radius = 5.5F * scale;
			Gdiplus::SolidBrush	st_Fill(f_Plot_Color(color, 255));
			Gdiplus::Pen		st_Outline(f_Plot_Color(UI_COLOR_CARD, 255), 1.5F * scale);
			Gdiplus::Pen		st_Heading(f_Plot_Color(color, 255), 2.0F * scale);

			st_State = st_Result->f_GetState(nCurStep, nObject);
			f_MapToPixel(st_Result->f_GetPoint(nCurStep, nObject), &x, &y);

			// 진행 방향 화살표. 궤적 점과 같은 기준(플랫폼 표본 0)의 NED 로 돌린다.
			if (f_Trans_EcefVec_To_Ned(&st_VelNed, &st_State->st_VelEcef, st_Origin->st_Platform.st_Lla.lat, st_Origin->st_Platform.st_Lla.lon) == COORD_OK)
			{
				headingNorm = hypot(st_VelNed.x, st_VelNed.y);

				if (headingNorm >= PLOT_MIN_HEADING)
				{
					const FLOAT64	dirX = st_VelNed.y / headingNorm;
					const FLOAT64	dirY = -st_VelNed.x / headingNorm;
					const FLOAT64	tipX = x + (PLOT_HEADING_PX * static_cast<FLOAT64>(scale) * dirX);
					const FLOAT64	tipY = y + (PLOT_HEADING_PX * static_cast<FLOAT64>(scale) * dirY);
					const FLOAT64	barb = PLOT_ARROW_PX * static_cast<FLOAT64>(scale);

					(VOID)st_Graphics->DrawLine(&st_Heading, f_Plot_Real(x), f_Plot_Real(y), f_Plot_Real(tipX), f_Plot_Real(tipY));
					(VOID)st_Graphics->DrawLine(&st_Heading, f_Plot_Real(tipX), f_Plot_Real(tipY),
						f_Plot_Real(tipX + (barb * ((-dirX * cos(PLOT_ARROW_ANGLE)) + (dirY * sin(PLOT_ARROW_ANGLE))))),
						f_Plot_Real(tipY + (barb * ((-dirY * cos(PLOT_ARROW_ANGLE)) - (dirX * sin(PLOT_ARROW_ANGLE))))));
					(VOID)st_Graphics->DrawLine(&st_Heading, f_Plot_Real(tipX), f_Plot_Real(tipY),
						f_Plot_Real(tipX + (barb * ((-dirX * cos(PLOT_ARROW_ANGLE)) - (dirY * sin(PLOT_ARROW_ANGLE))))),
						f_Plot_Real(tipY + (barb * ((-dirY * cos(PLOT_ARROW_ANGLE)) + (dirX * sin(PLOT_ARROW_ANGLE))))));
				}
			}

			if (nObject == 0)
			{
				(VOID)st_Graphics->FillRectangle(&st_Fill, f_Plot_Real(x) - radius, f_Plot_Real(y) - radius, 2.0F * radius, 2.0F * radius);
				(VOID)st_Graphics->DrawRectangle(&st_Outline, f_Plot_Real(x) - radius, f_Plot_Real(y) - radius, 2.0F * radius, 2.0F * radius);
			}
			else
			{
				(VOID)st_Graphics->FillEllipse(&st_Fill, f_Plot_Real(x) - radius, f_Plot_Real(y) - radius, 2.0F * radius, 2.0F * radius);
				(VOID)st_Graphics->DrawEllipse(&st_Outline, f_Plot_Real(x) - radius, f_Plot_Real(y) - radius, 2.0F * radius, 2.0F * radius);
			}
		}

		(VOID)st_Graphics->ResetClip();

		// 고도 패널의 현재 시각 선과 고도 점
		if (st_AltArea.IsRectEmpty() == FALSE)
		{
			Gdiplus::Pen st_Cursor(f_Plot_Color(UI_COLOR_TEXT, 170), 1.0F * scale);

			f_AltToPixel(nCurStep, altMin, &x, &y);
			(VOID)st_Graphics->DrawLine(&st_Cursor, f_Plot_Real(x), static_cast<FLOAT32>(st_AltArea.top), f_Plot_Real(x), static_cast<FLOAT32>(st_AltArea.bottom));

			for (nObject = 0; nObject < nObjectNum; nObject++)
			{
				const FLOAT32		dot = 3.5F * scale;
				Gdiplus::SolidBrush	st_Dot(f_Plot_Color(f_Ui_ObjectColor(nObject), 255));

				f_AltToPixel(nCurStep, st_Result->f_GetState(nCurStep, nObject)->st_Lla.alt, &x, &y);
				(VOID)st_Graphics->FillEllipse(&st_Dot, f_Plot_Real(x) - dot, f_Plot_Real(y) - dot, 2.0F * dot, 2.0F * dot);
			}
		}

		f_Plot_Text(st_Graphics, &st_Font, CString(_T("t = ")) + f_Num_ToText(st_Sample->simTime, 3) + _T(" s"),
			static_cast<FLOAT64>(st_MapArea.right), static_cast<FLOAT64>(st_MapArea.top) - 5.0, PLOT_ALIGN_FAR, PLOT_ALIGN_FAR, UI_COLOR_TEXT);
	}
}

// 마우스

INT32 CTrajectoryPlot::f_HitTest(CPoint st_Point, INT32 *pt_Object) const
{
	const FLOAT64	reach = static_cast<FLOAT64>(f_Ui_Scale(100, dpi)) * PLOT_HIT_PX / 100.0;
	FLOAT64			x = 0.0;
	FLOAT64			y = 0.0;
	FLOAT64			best = reach;
	INT32			hit = PLOT_HIT_NONE;
	INT32			nObject;

	*pt_Object = -1;

	if (f_HasData() != 0)
	{
		// 손잡이는 평면 안에서만
		if ((st_MapArea.PtInRect(st_Point) != FALSE) && (f_GetHandlePixel(nSelObject, &x, &y) != 0) &&
			(hypot(static_cast<FLOAT64>(st_Point.x) - x, static_cast<FLOAT64>(st_Point.y) - y) <= reach))
		{
			hit			= PLOT_HIT_HANDLE;
			*pt_Object	= nSelObject;
		}
		else if (st_MapArea.PtInRect(st_Point) != FALSE)
		{
			// 겹친 시작점은 가장 가까운 것
			for (nObject = 0; nObject < st_Result->f_GetObjectNum(); nObject++)
			{
				f_MapToPixel(st_Result->f_GetPoint(0, nObject), &x, &y);

				if (hypot(static_cast<FLOAT64>(st_Point.x) - x, static_cast<FLOAT64>(st_Point.y) - y) <= best)
				{
					best		= hypot(static_cast<FLOAT64>(st_Point.x) - x, static_cast<FLOAT64>(st_Point.y) - y);
					hit			= PLOT_HIT_START;
					*pt_Object	= nObject;
				}
			}
		}
		else if ((st_AltArea.IsRectEmpty() == FALSE) && (st_AltArea.PtInRect(st_Point) != FALSE))
		{
			hit = PLOT_HIT_ALT;
		}
		else
		{
			hit = PLOT_HIT_NONE;
		}
	}

	return hit;
}

VOID CTrajectoryPlot::f_Notify(UINT32 code, const ST_PlotNotify *st_Source)
{
	ST_PlotNotify	st_Notify = *st_Source;
	CWnd			*st_Parent = GetParent();

	if (st_Parent != nullptr)
	{
		st_Notify.st_Hdr.hwndFrom	= GetSafeHwnd();
		st_Notify.st_Hdr.idFrom		= static_cast<UINT_PTR>(GetDlgCtrlID());
		st_Notify.st_Hdr.code		= code;
		(VOID)st_Parent->SendMessage(WM_NOTIFY, static_cast<WPARAM>(GetDlgCtrlID()), reinterpret_cast<LPARAM>(&st_Notify));
	}
}

VOID CTrajectoryPlot::f_DragTo(CPoint st_Point)
{
	const ST_SimSample	*st_Origin = (f_HasData() != 0) ? st_Result->f_GetSample(0) : nullptr;
	ST_PlotNotify		st_Notify;
	ST_CoordRect		st_Ned;
	ST_CoordRect		st_Ecef;
	ST_CoordLla			st_Lla;
	FLOAT64				x = 0.0;
	FLOAT64				y = 0.0;
	FLOAT64				yawDeg;
	INT32				nLastStep;

	(VOID)memset(&st_Notify, 0, sizeof(st_Notify));
	st_Notify.nObject = nDragObject;

	if (st_Origin == nullptr)
	{
		// 결과 없음
	}
	else if (dragKind == PLOT_DRAG_MOVE)
	{
		// 화면 점 → 플랫폼 기준 수평면 → 위경도. 고도는 표의 값 그대로
		st_Ned.y = centerEast + ((static_cast<FLOAT64>(st_Point.x + st_DragOffset.x) - (static_cast<FLOAT64>(st_MapArea.left) + (0.5 * static_cast<FLOAT64>(st_MapArea.Width())))) / pixelPerMeter);
		st_Ned.x = centerNorth - ((static_cast<FLOAT64>(st_Point.y + st_DragOffset.y) - (static_cast<FLOAT64>(st_MapArea.top) + (0.5 * static_cast<FLOAT64>(st_MapArea.Height())))) / pixelPerMeter);
		st_Ned.z = 0.0;

		if ((f_Trans_Ned_To_Ecef(&st_Ecef, &st_Ned, &st_Origin->st_Platform.st_PosEcef, st_Origin->st_Platform.st_Lla.lat, st_Origin->st_Platform.st_Lla.lon) == COORD_OK) &&
			(f_Trans_Ecef_To_Lla(&st_Lla, &st_Ecef) == COORD_OK))
		{
			st_Notify.latDeg = f_Rad_To_Deg(st_Lla.lat);
			st_Notify.lonDeg = f_Rad_To_Deg(st_Lla.lon);
			f_Notify(PLOTN_MOVE, &st_Notify);
		}
	}
	else if (dragKind == PLOT_DRAG_TURN)
	{
		f_MapToPixel(st_Result->f_GetPoint(0, nDragObject), &x, &y);

		if (hypot(static_cast<FLOAT64>(st_Point.x) - x, static_cast<FLOAT64>(st_Point.y) - y) >= 2.0)
		{
			// 북쪽 0 도, 시계 방향 (+), 1 도 단위
			yawDeg = floor(f_Rad_To_Deg(atan2(static_cast<FLOAT64>(st_Point.x) - x, y - static_cast<FLOAT64>(st_Point.y))) + 0.5);

			if (yawDeg < 0.0)
			{
				yawDeg = yawDeg + 360.0;
			}

			st_Notify.yawDeg = yawDeg;
			f_Notify(PLOTN_TURN, &st_Notify);
		}
	}
	else if (dragKind == PLOT_DRAG_SCRUB)
	{
		nLastStep = st_Result->f_GetSampleNum() - 1;

		if ((st_AltArea.Width() > 0) && (nLastStep > 0))
		{
			st_Notify.nStep = f_Plot_Round((static_cast<FLOAT64>(st_Point.x - st_AltArea.left) / static_cast<FLOAT64>(st_AltArea.Width())) * static_cast<FLOAT64>(nLastStep));

			if (st_Notify.nStep < 0)
			{
				st_Notify.nStep = 0;
			}
			else if (st_Notify.nStep > nLastStep)
			{
				st_Notify.nStep = nLastStep;
			}
			else
			{
				// 범위 안
			}

			f_Notify(PLOTN_SCRUB, &st_Notify);
		}
	}
	else
	{
		// 끌고 있지 않음
	}
}

VOID CTrajectoryPlot::OnLButtonDown(UINT32 flags, CPoint st_Point)
{
	ST_PlotNotify	st_Notify;
	FLOAT64			x = 0.0;
	FLOAT64			y = 0.0;
	INT32			nObject = -1;
	INT32			hit;

	UNREFERENCED_PARAMETER(flags);

	// 포커스를 가져와 다른 곳에서 치던 값을 먼저 반영시키고, 그림이 달라졌으면 새로 그린 뒤 판정한다.
	(VOID)SetFocus();

	if (isBackValid == 0)
	{
		UpdateWindow();
	}

	hit = f_HitTest(st_Point, &nObject);

	if (hit == PLOT_HIT_HANDLE)
	{
		dragKind		= PLOT_DRAG_TURN;
		nDragObject		= nObject;
		isViewFrozen	= 1;
		(VOID)SetCapture();
	}
	else if (hit == PLOT_HIT_START)
	{
		(VOID)memset(&st_Notify, 0, sizeof(st_Notify));
		st_Notify.nObject = nObject;
		f_Notify(PLOTN_SELECT, &st_Notify);

		// 표적만 끌 수 있다. 조금 움직이기 전까지는 그냥 누른 것
		if (nObject >= 1)
		{
			f_MapToPixel(st_Result->f_GetPoint(0, nObject), &x, &y);
			st_DragOffset	= CPoint(f_Plot_Round(x) - st_Point.x, f_Plot_Round(y) - st_Point.y);
			st_DownPoint	= st_Point;
			dragKind		= PLOT_DRAG_PENDING;
			nDragObject		= nObject;
			(VOID)SetCapture();
		}
	}
	else if (hit == PLOT_HIT_ALT)
	{
		dragKind = PLOT_DRAG_SCRUB;
		(VOID)SetCapture();
		f_DragTo(st_Point);
	}
	else
	{
		dragKind = PLOT_DRAG_NONE;
	}
}

VOID CTrajectoryPlot::OnMouseMove(UINT32 flags, CPoint st_Point)
{
	UNREFERENCED_PARAMETER(flags);

	if (dragKind == PLOT_DRAG_PENDING)
	{
		if ((abs(st_Point.x - st_DownPoint.x) >= PLOT_DRAG_START_PX) || (abs(st_Point.y - st_DownPoint.y) >= PLOT_DRAG_START_PX))
		{
			dragKind		= PLOT_DRAG_MOVE;
			isViewFrozen	= 1;
		}
	}

	if ((dragKind == PLOT_DRAG_MOVE) || (dragKind == PLOT_DRAG_TURN) || (dragKind == PLOT_DRAG_SCRUB))
	{
		f_DragTo(st_Point);
	}
}

VOID CTrajectoryPlot::OnLButtonUp(UINT32 flags, CPoint st_Point)
{
	const INT32 endedKind = dragKind;

	UNREFERENCED_PARAMETER(flags);

	if (endedKind != PLOT_DRAG_NONE)
	{
		if ((endedKind == PLOT_DRAG_MOVE) || (endedKind == PLOT_DRAG_TURN))
		{
			f_DragTo(st_Point);
		}

		dragKind		= PLOT_DRAG_NONE;
		nDragObject		= -1;
		isViewFrozen	= 0;
		isBackValid		= 0;
		(VOID)ReleaseCapture();
		Invalidate(FALSE);
	}
}

VOID CTrajectoryPlot::OnCaptureChanged(CWnd *st_NewWnd)
{
	if ((st_NewWnd != this) && (dragKind != PLOT_DRAG_NONE))
	{
		dragKind		= PLOT_DRAG_NONE;
		nDragObject		= -1;
		isViewFrozen	= 0;
		isBackValid		= 0;
		Invalidate(FALSE);
	}

	CStatic::OnCaptureChanged(st_NewWnd);
}

BOOL CTrajectoryPlot::OnSetCursor(CWnd *st_Wnd, UINT32 hitTest, UINT32 message)
{
	CPoint	st_Point;
	INT32	nObject = -1;
	INT32	hit;
	BOOL	isHandled = FALSE;

	if (::GetCursorPos(&st_Point) != FALSE)
	{
		ScreenToClient(&st_Point);
		hit = f_HitTest(st_Point, &nObject);

		if ((hit == PLOT_HIT_HANDLE) || ((hit == PLOT_HIT_START) && (nObject >= 1)))
		{
			(VOID)::SetCursor(::LoadCursor(nullptr, (hit == PLOT_HIT_HANDLE) ? IDC_HAND : IDC_SIZEALL));
			isHandled = TRUE;
		}
		else if (hit == PLOT_HIT_ALT)
		{
			(VOID)::SetCursor(::LoadCursor(nullptr, IDC_SIZEWE));
			isHandled = TRUE;
		}
		else
		{
			isHandled = FALSE;
		}
	}

	if (isHandled == FALSE)
	{
		isHandled = CStatic::OnSetCursor(st_Wnd, hitTest, message);
	}

	return isHandled;
}
