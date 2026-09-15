#include "pch.h"

#include <math.h>
#include <stdlib.h>

#include "TrajectoryPlot_JH.h"
#include "UiNumber_JH.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define UI_PLOT_MARGIN_LEFT		52						// [px]
#define UI_PLOT_MARGIN_RIGHT	12
#define UI_PLOT_MARGIN_TOP		24
#define UI_PLOT_MARGIN_BOTTOM	36
#define UI_PLOT_MIN_SPAN		1000.0					// [m]
#define UI_PLOT_SPAN_SCALE		1.16
#define UI_PLOT_MAX_GRID_NUM	8.0
#define UI_PLOT_GRID_SEARCH_NUM	45
#define UI_PLOT_MAX_LINE_NUM	40
#define UI_PLOT_PIXEL_LIMIT		1.0e6
#define UI_PLOT_HEADING_PX		16.0
#define UI_PLOT_ARROW_PX		6.0
#define UI_PLOT_ARROW_ANGLE		(PI / 6.0)
#define UI_PLOT_MIN_HEADING		1.0e-3					// [m/s]
#define UI_PLOT_LABEL_SIZE		32
#define UI_PLOT_OBJECT_NUM		(TGT_MAX_TARGET_NUM + 1)

static const COLORREF s_ObjectColor[UI_PLOT_OBJECT_NUM] =
{
	RGB(32, 32, 32),
	RGB(31, 119, 180), RGB(214, 39, 40), RGB(44, 160, 44), RGB(255, 127, 14), RGB(148, 103, 189),
	RGB(140, 86, 75), RGB(227, 119, 194), RGB(23, 190, 207), RGB(188, 189, 34), RGB(127, 127, 127)
};

static const FLOAT64 s_GridMantissa[3] = { 1.0, 2.0, 5.0 };

// 화면 안의 작은 픽셀 오프셋만 반올림한다.
static INT32 f_Plot_Round(FLOAT64 value)
{
	return static_cast<INT32>(floor(value + 0.5));
}

BEGIN_MESSAGE_MAP(CTrajectoryPlot, CStatic)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

CTrajectoryPlot::CTrajectoryPlot() noexcept
	: st_PointBuf(nullptr)
	, st_SampleBuf(nullptr)
	, nSampleNum(0)
	, nObjectNum(0)
	, nCurStep(0)
	, isBackValid(0)
	, pixelPerMeter(1.0)
	, centerEast(0.0)
	, centerNorth(0.0)
	, gridMeter(1000.0)
	, st_Area(0, 0, 0, 0)
	, st_FrameSize(0, 0)
{
}

VOID CTrajectoryPlot::f_SetData(const ST_PlotPoint *st_NewPoint, const ST_SimSample *st_NewSample, INT32 nNewSampleNum, INT32 nNewObjectNum)
{
	if ((st_NewPoint != nullptr) && (st_NewSample != nullptr) && (nNewSampleNum > 0) &&
		(nNewObjectNum > 0) && (nNewObjectNum <= UI_PLOT_OBJECT_NUM))
	{
		st_PointBuf		= st_NewPoint;
		st_SampleBuf	= st_NewSample;
		nSampleNum		= nNewSampleNum;
		nObjectNum		= nNewObjectNum;
		nCurStep		= 0;
		isBackValid		= 0;
	}
	else
	{
		f_Clear();
	}

	if (GetSafeHwnd() != nullptr)
	{
		Invalidate(FALSE);
	}
}

VOID CTrajectoryPlot::f_SetStep(INT32 nStep)
{
	if ((nStep >= 0) && (nStep < nSampleNum))
	{
		nCurStep = nStep;
	}

	if (GetSafeHwnd() != nullptr)
	{
		Invalidate(FALSE);
	}
}

VOID CTrajectoryPlot::f_Clear(VOID)
{
	st_PointBuf		= nullptr;
	st_SampleBuf	= nullptr;
	nSampleNum		= 0;
	nObjectNum		= 0;
	nCurStep		= 0;
	isBackValid		= 0;

	if (GetSafeHwnd() != nullptr)
	{
		Invalidate(FALSE);
	}
}

BOOL CTrajectoryPlot::OnEraseBkgnd(CDC *st_Dc)
{
	// 배경까지 OnPaint 에서 한 번에 그려 깜빡임을 없앤다.
	UNREFERENCED_PARAMETER(st_Dc);

	return TRUE;
}

VOID CTrajectoryPlot::OnPaint(VOID)
{
	CPaintDC	st_Dc(this);
	CDC			st_MemDc;
	CDC			st_BackDc;
	CRect		st_Client;
	CBitmap		*st_OldBitmap;
	CBitmap		*st_OldBackBitmap;
	CFont		*st_OldFont = nullptr;
	CFont		*st_Font = nullptr;
	CWnd		*st_Parent = GetParent();

	GetClientRect(&st_Client);

	if ((st_Client.Width() > 0) && (st_Client.Height() > 0) && (st_MemDc.CreateCompatibleDC(&st_Dc) != FALSE))
	{
		// 페인트 DC 기준으로 만들어야 컬러 비트맵이 된다.
		if ((st_FrameBitmap.GetSafeHandle() == nullptr) || (st_FrameSize != st_Client.Size()))
		{
			(VOID)st_FrameBitmap.DeleteObject();
			(VOID)st_FrameBitmap.CreateCompatibleBitmap(&st_Dc, st_Client.Width(), st_Client.Height());
			st_FrameSize	= st_Client.Size();
			isBackValid		= 0;
		}

		st_OldBitmap = st_MemDc.SelectObject(&st_FrameBitmap);

		if (st_Parent != nullptr)
		{
			st_Font = st_Parent->GetFont();
		}

		if (st_Font != nullptr)
		{
			st_OldFont = st_MemDc.SelectObject(st_Font);
		}

		if ((nSampleNum > 0) && (st_PointBuf != nullptr) && (st_SampleBuf != nullptr))
		{
			if (isBackValid == 0)
			{
				f_BuildBackground(&st_Dc, st_Client);
			}

			if (st_BackDc.CreateCompatibleDC(&st_Dc) != FALSE)
			{
				st_OldBackBitmap = st_BackDc.SelectObject(&st_BackBitmap);
				(VOID)st_MemDc.BitBlt(0, 0, st_Client.Width(), st_Client.Height(), &st_BackDc, 0, 0, SRCCOPY);
				(VOID)st_BackDc.SelectObject(st_OldBackBitmap);
			}

			f_DrawOverlay(&st_MemDc);
		}
		else
		{
			CBrush st_FrameBrush(RGB(160, 160, 160));

			st_MemDc.FillSolidRect(&st_Client, RGB(255, 255, 255));
			st_MemDc.FrameRect(&st_Client, &st_FrameBrush);
			(VOID)st_MemDc.SetBkMode(TRANSPARENT);
			(VOID)st_MemDc.SetTextColor(RGB(96, 96, 96));
			(VOID)st_MemDc.DrawText(CString(_T("실행 결과 없음")), &st_Client, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		}

		(VOID)st_Dc.BitBlt(0, 0, st_Client.Width(), st_Client.Height(), &st_MemDc, 0, 0, SRCCOPY);

		if (st_OldFont != nullptr)
		{
			(VOID)st_MemDc.SelectObject(st_OldFont);
		}

		(VOID)st_MemDc.SelectObject(st_OldBitmap);
	}
}

VOID CTrajectoryPlot::f_ComputeView(const CRect &st_Client)
{
	const INT32		nPointNum = nSampleNum * nObjectNum;
	FLOAT64			minEast = 0.0;
	FLOAT64			maxEast = 0.0;
	FLOAT64			minNorth = 0.0;
	FLOAT64			maxNorth = 0.0;
	FLOAT64			spanEast;
	FLOAT64			spanNorth;
	FLOAT64			width;
	FLOAT64			height;
	FLOAT64			extent;
	FLOAT64			decade = 1.0;
	FLOAT64			candidate;
	INT32			isFound = 0;
	INT32			nIndex;

	st_Area.SetRect(st_Client.left + UI_PLOT_MARGIN_LEFT, st_Client.top + UI_PLOT_MARGIN_TOP,
		st_Client.right - UI_PLOT_MARGIN_RIGHT, st_Client.bottom - UI_PLOT_MARGIN_BOTTOM);

	// 원점(플랫폼 초기 위치)을 항상 화면에 넣는다.
	for (nIndex = 0; nIndex < nPointNum; nIndex++)
	{
		minEast		= fmin(minEast, st_PointBuf[nIndex].east);
		maxEast		= fmax(maxEast, st_PointBuf[nIndex].east);
		minNorth	= fmin(minNorth, st_PointBuf[nIndex].north);
		maxNorth	= fmax(maxNorth, st_PointBuf[nIndex].north);
	}

	spanEast	= fmax(maxEast - minEast, UI_PLOT_MIN_SPAN) * UI_PLOT_SPAN_SCALE;
	spanNorth	= fmax(maxNorth - minNorth, UI_PLOT_MIN_SPAN) * UI_PLOT_SPAN_SCALE;
	width		= fmax(static_cast<FLOAT64>(st_Area.Width()), 1.0);
	height		= fmax(static_cast<FLOAT64>(st_Area.Height()), 1.0);

	// 등축척이라 선회가 원으로 보인다.
	pixelPerMeter	= fmin(width / spanEast, height / spanNorth);
	centerEast		= 0.5 * (minEast + maxEast);
	centerNorth		= 0.5 * (minNorth + maxNorth);

	// 격자 간격은 {1, 2, 5} x 10^k m 중 한 축에 8 칸 이하가 되는 가장 작은 값
	extent		= fmax(width, height) / pixelPerMeter;
	gridMeter	= extent / UI_PLOT_MAX_GRID_NUM;

	for (nIndex = 0; nIndex < UI_PLOT_GRID_SEARCH_NUM; nIndex++)
	{
		candidate = decade * s_GridMantissa[nIndex % 3];

		if ((isFound == 0) && ((extent / candidate) <= UI_PLOT_MAX_GRID_NUM))
		{
			gridMeter	= candidate;
			isFound		= 1;
		}

		if ((nIndex % 3) == 2)
		{
			decade = decade * 10.0;
		}
	}
}

POINT CTrajectoryPlot::f_ToPixel(const ST_PlotPoint *st_Point) const
{
	POINT	st_Pixel;
	FLOAT64	pixelX;
	FLOAT64	pixelY;

	pixelX = static_cast<FLOAT64>(st_Area.left) + (0.5 * static_cast<FLOAT64>(st_Area.Width())) + ((st_Point->east - centerEast) * pixelPerMeter);
	pixelY = static_cast<FLOAT64>(st_Area.top) + (0.5 * static_cast<FLOAT64>(st_Area.Height())) - ((st_Point->north - centerNorth) * pixelPerMeter);

	// 정수 변환 전에 범위를 자른다.
	pixelX = fmin(fmax(pixelX, -UI_PLOT_PIXEL_LIMIT), UI_PLOT_PIXEL_LIMIT);
	pixelY = fmin(fmax(pixelY, -UI_PLOT_PIXEL_LIMIT), UI_PLOT_PIXEL_LIMIT);

	st_Pixel.x = static_cast<INT32>(floor(pixelX + 0.5));
	st_Pixel.y = static_cast<INT32>(floor(pixelY + 0.5));

	return st_Pixel;
}

VOID CTrajectoryPlot::f_BuildBackground(CDC *st_Dc, const CRect &st_Client)
{
	CDC			st_MemDc;
	CBitmap		*st_OldBitmap;
	CFont		*st_OldFont = nullptr;
	CFont		*st_Font = nullptr;
	CWnd		*st_Parent = GetParent();
	CBrush		st_FrameBrush(RGB(160, 160, 160));

	(VOID)st_BackBitmap.DeleteObject();

	if ((st_BackBitmap.CreateCompatibleBitmap(st_Dc, st_Client.Width(), st_Client.Height()) != FALSE) &&
		(st_MemDc.CreateCompatibleDC(st_Dc) != FALSE))
	{
		st_OldBitmap = st_MemDc.SelectObject(&st_BackBitmap);

		if (st_Parent != nullptr)
		{
			st_Font = st_Parent->GetFont();
		}

		if (st_Font != nullptr)
		{
			st_OldFont = st_MemDc.SelectObject(st_Font);
		}

		f_ComputeView(st_Client);

		st_MemDc.FillSolidRect(&st_Client, RGB(255, 255, 255));
		(VOID)st_MemDc.SetBkMode(TRANSPARENT);

		f_DrawGrid(&st_MemDc);

		(VOID)st_MemDc.IntersectClipRect(&st_Area);
		f_DrawTrack(&st_MemDc);
		(VOID)st_MemDc.SelectClipRgn(nullptr);

		f_DrawLegend(&st_MemDc);
		st_MemDc.FrameRect(&st_Area, &st_FrameBrush);

		if (st_OldFont != nullptr)
		{
			(VOID)st_MemDc.SelectObject(st_OldFont);
		}

		(VOID)st_MemDc.SelectObject(st_OldBitmap);
		isBackValid = 1;
	}
}

VOID CTrajectoryPlot::f_DrawGrid(CDC *st_Dc) const
{
	CPen			st_GridPen(PS_SOLID, 1, RGB(225, 225, 225));
	CPen			st_AxisPen(PS_SOLID, 1, RGB(180, 180, 180));
	CPen			*st_OldPen;
	CHAR			pt_Label[UI_PLOT_LABEL_SIZE];
	ST_PlotPoint	st_Point;
	POINT			st_Pixel;
	const FLOAT64	halfEast = (0.5 * static_cast<FLOAT64>(st_Area.Width())) / pixelPerMeter;
	const FLOAT64	halfNorth = (0.5 * static_cast<FLOAT64>(st_Area.Height())) / pixelPerMeter;
	const FLOAT64	firstEast = ceil((centerEast - halfEast) / gridMeter);
	const FLOAT64	firstNorth = ceil((centerNorth - halfNorth) / gridMeter);
	const FLOAT64	lineEastNum = floor((centerEast + halfEast) / gridMeter) - firstEast;
	const FLOAT64	lineNorthNum = floor((centerNorth + halfNorth) / gridMeter) - firstNorth;
	const INT32		nDecimal = static_cast<INT32>(fmax(0.0, -floor(log10(gridMeter / 1000.0) + 1.0e-9)));
	const INT32		textHeight = st_Dc->GetTextExtent(CString(_T("0"))).cy;
	INT32			nLine;
	FLOAT64			index;

	st_OldPen = st_Dc->SelectObject(&st_GridPen);
	(VOID)st_Dc->SetTextColor(RGB(96, 96, 96));

	// 동쪽 격자선과 아래 눈금
	(VOID)st_Dc->SetTextAlign(TA_CENTER | TA_TOP);

	for (nLine = 0; (nLine <= UI_PLOT_MAX_LINE_NUM) && (static_cast<FLOAT64>(nLine) <= lineEastNum); nLine++)
	{
		index				= firstEast + static_cast<FLOAT64>(nLine);
		st_Point.east		= index * gridMeter;
		st_Point.north		= centerNorth;
		st_Pixel			= f_ToPixel(&st_Point);

		(VOID)st_Dc->SelectObject((fabs(index) < 0.5) ? &st_AxisPen : &st_GridPen);
		(VOID)st_Dc->MoveTo(st_Pixel.x, st_Area.top);
		(VOID)st_Dc->LineTo(st_Pixel.x, st_Area.bottom);

		if (f_Num_FormatFixed(pt_Label, UI_PLOT_LABEL_SIZE, st_Point.east / 1000.0, nDecimal) > 0)
		{
			(VOID)st_Dc->TextOut(st_Pixel.x, st_Area.bottom + 2, CString(pt_Label));
		}
	}

	// 북쪽 격자선과 왼쪽 눈금
	(VOID)st_Dc->SetTextAlign(TA_RIGHT | TA_TOP);

	for (nLine = 0; (nLine <= UI_PLOT_MAX_LINE_NUM) && (static_cast<FLOAT64>(nLine) <= lineNorthNum); nLine++)
	{
		index				= firstNorth + static_cast<FLOAT64>(nLine);
		st_Point.east		= centerEast;
		st_Point.north		= index * gridMeter;
		st_Pixel			= f_ToPixel(&st_Point);

		(VOID)st_Dc->SelectObject((fabs(index) < 0.5) ? &st_AxisPen : &st_GridPen);
		(VOID)st_Dc->MoveTo(st_Area.left, st_Pixel.y);
		(VOID)st_Dc->LineTo(st_Area.right, st_Pixel.y);

		if (f_Num_FormatFixed(pt_Label, UI_PLOT_LABEL_SIZE, st_Point.north / 1000.0, nDecimal) > 0)
		{
			(VOID)st_Dc->TextOut(st_Area.left - 4, st_Pixel.y - (textHeight / 2), CString(pt_Label));
		}
	}

	(VOID)st_Dc->SetTextAlign(TA_CENTER | TA_TOP);
	(VOID)st_Dc->TextOut(st_Area.CenterPoint().x, st_Area.bottom + 2 + textHeight, CString(_T("동 E [km]")));
	(VOID)st_Dc->SetTextAlign(TA_LEFT | TA_TOP);
	(VOID)st_Dc->TextOut(st_Area.left - UI_PLOT_MARGIN_LEFT + 4, 4, CString(_T("북 N [km]")));

	(VOID)st_Dc->SelectObject(st_OldPen);
}

VOID CTrajectoryPlot::f_DrawTrack(CDC *st_Dc) const
{
	POINT	*st_Line = static_cast<POINT *>(malloc(static_cast<UINT64>(nSampleNum) * static_cast<UINT64>(sizeof(POINT))));
	POINT	st_Pixel;
	INT32	nObject;
	INT32	nSample;
	INT32	nCount;

	if (st_Line != nullptr)
	{
		for (nObject = 0; nObject < nObjectNum; nObject++)
		{
			CPen	st_TrackPen(PS_SOLID, 2, s_ObjectColor[nObject]);
			CPen	st_StartPen(PS_SOLID, 1, s_ObjectColor[nObject]);
			CPen	*st_OldPen = st_Dc->SelectObject(&st_TrackPen);
			CGdiObject *st_OldBrush;

			// 같은 픽셀에 겹치는 점은 버려 Polyline 점 수를 줄인다.
			nCount = 0;

			for (nSample = 0; nSample < nSampleNum; nSample++)
			{
				st_Pixel = f_ToPixel(&st_PointBuf[(nSample * nObjectNum) + nObject]);

				if ((nCount == 0) || (st_Pixel.x != st_Line[nCount - 1].x) || (st_Pixel.y != st_Line[nCount - 1].y))
				{
					st_Line[nCount] = st_Pixel;
					nCount = nCount + 1;
				}
			}

			if (nCount >= 2)
			{
				(VOID)st_Dc->Polyline(st_Line, nCount);
			}

			// 시작 위치는 속 빈 원
			(VOID)st_Dc->SelectObject(&st_StartPen);
			st_OldBrush = st_Dc->SelectStockObject(NULL_BRUSH);
			(VOID)st_Dc->Ellipse(st_Line[0].x - 4, st_Line[0].y - 4, st_Line[0].x + 5, st_Line[0].y + 5);
			(VOID)st_Dc->SelectObject(st_OldBrush);
			(VOID)st_Dc->SelectObject(st_OldPen);
		}

		free(st_Line);
	}
}

VOID CTrajectoryPlot::f_DrawLegend(CDC *st_Dc) const
{
	const INT32		textHeight = st_Dc->GetTextExtent(CString(_T("0"))).cy;
	const INT32		rowHeight = textHeight + 2;
	const INT32		lineLength = 20;
	CBrush			st_FrameBrush(RGB(160, 160, 160));
	CString			st_Name;
	CRect			st_Box;
	INT32			textWidth = 0;
	INT32			nObject;
	INT32			rowY;

	for (nObject = 0; nObject < nObjectNum; nObject++)
	{
		if (nObject == 0)
		{
			st_Name = _T("플랫폼");
		}
		else
		{
			st_Name.Format(_T("표적 %d"), nObject);
		}

		if (st_Dc->GetTextExtent(st_Name).cx > textWidth)
		{
			textWidth = st_Dc->GetTextExtent(st_Name).cx;
		}
	}

	st_Box.SetRect(st_Area.left + 6, st_Area.top + 6,
		st_Area.left + 6 + 6 + lineLength + 6 + textWidth + 6, st_Area.top + 6 + 4 + (rowHeight * nObjectNum) + 2);
	st_Dc->FillSolidRect(&st_Box, RGB(255, 255, 255));
	st_Dc->FrameRect(&st_Box, &st_FrameBrush);
	(VOID)st_Dc->SetTextAlign(TA_LEFT | TA_TOP);
	(VOID)st_Dc->SetTextColor(RGB(32, 32, 32));

	for (nObject = 0; nObject < nObjectNum; nObject++)
	{
		CPen	st_Pen(PS_SOLID, 2, s_ObjectColor[nObject]);
		CPen	*st_OldPen = st_Dc->SelectObject(&st_Pen);

		if (nObject == 0)
		{
			st_Name = _T("플랫폼");
		}
		else
		{
			st_Name.Format(_T("표적 %d"), nObject);
		}

		rowY = st_Box.top + 4 + (rowHeight * nObject);
		(VOID)st_Dc->MoveTo(st_Box.left + 6, rowY + (textHeight / 2));
		(VOID)st_Dc->LineTo(st_Box.left + 6 + lineLength, rowY + (textHeight / 2));
		(VOID)st_Dc->TextOut(st_Box.left + 6 + lineLength + 6, rowY, st_Name);
		(VOID)st_Dc->SelectObject(st_OldPen);
	}
}

VOID CTrajectoryPlot::f_DrawOverlay(CDC *st_Dc) const
{
	const ST_SimSample		*st_Sample = &st_SampleBuf[nCurStep];
	const ST_TargetState	*st_State;
	CPen					st_WhitePen(PS_SOLID, 1, RGB(255, 255, 255));
	ST_CoordRect			st_VelNed;
	POINT					st_Pixel;
	CHAR					pt_Label[UI_PLOT_LABEL_SIZE];
	FLOAT64					headingNorm;
	INT32					nObject;

	for (nObject = 0; nObject < nObjectNum; nObject++)
	{
		CPen	st_HeadingPen(PS_SOLID, 2, s_ObjectColor[nObject]);
		CBrush	st_MarkBrush(s_ObjectColor[nObject]);
		CPen	*st_OldPen = st_Dc->SelectObject(&st_HeadingPen);
		CBrush	*st_OldBrush = st_Dc->SelectObject(&st_MarkBrush);

		st_State = (nObject == 0) ? &st_Sample->st_Platform : &st_Sample->st_Target[nObject - 1];
		st_Pixel = f_ToPixel(&st_PointBuf[(nCurStep * nObjectNum) + nObject]);

		// 진행 방향선. 정지한 객체는 방향이 없으므로 생략한다.
		// 궤적 점과 같은 기준(플랫폼 표본 0)의 NED 로 돌려야 화살표가 그림 속 궤적 접선과 맞는다.
		if (f_Trans_EcefVec_To_Ned(&st_VelNed, &st_State->st_VelEcef, st_SampleBuf[0].st_Platform.st_Lla.lat, st_SampleBuf[0].st_Platform.st_Lla.lon) == COORD_OK)
		{
			headingNorm = hypot(st_VelNed.x, st_VelNed.y);

			if (headingNorm >= UI_PLOT_MIN_HEADING)
			{
				// 화면 좌표의 진행 방향 단위벡터 (x 동, y 아래)
				const FLOAT64	dirX = st_VelNed.y / headingNorm;
				const FLOAT64	dirY = -st_VelNed.x / headingNorm;
				POINT			st_Arrow[3];

				st_Arrow[1].x = st_Pixel.x + f_Plot_Round(UI_PLOT_HEADING_PX * dirX);
				st_Arrow[1].y = st_Pixel.y + f_Plot_Round(UI_PLOT_HEADING_PX * dirY);

				// 궤적선과 같은 색·굵기라 화살촉을 달아 구별한다.
				st_Arrow[0].x = st_Arrow[1].x + f_Plot_Round(UI_PLOT_ARROW_PX * ((-dirX * cos(UI_PLOT_ARROW_ANGLE)) + (dirY * sin(UI_PLOT_ARROW_ANGLE))));
				st_Arrow[0].y = st_Arrow[1].y + f_Plot_Round(UI_PLOT_ARROW_PX * ((-dirY * cos(UI_PLOT_ARROW_ANGLE)) - (dirX * sin(UI_PLOT_ARROW_ANGLE))));
				st_Arrow[2].x = st_Arrow[1].x + f_Plot_Round(UI_PLOT_ARROW_PX * ((-dirX * cos(UI_PLOT_ARROW_ANGLE)) - (dirY * sin(UI_PLOT_ARROW_ANGLE))));
				st_Arrow[2].y = st_Arrow[1].y + f_Plot_Round(UI_PLOT_ARROW_PX * ((-dirY * cos(UI_PLOT_ARROW_ANGLE)) + (dirX * sin(UI_PLOT_ARROW_ANGLE))));

				(VOID)st_Dc->MoveTo(st_Pixel);
				(VOID)st_Dc->LineTo(st_Arrow[1]);
				(VOID)st_Dc->Polyline(st_Arrow, 3);
			}
		}

		(VOID)st_Dc->SelectObject(&st_WhitePen);

		if (nObject == 0)
		{
			(VOID)st_Dc->Rectangle(st_Pixel.x - 4, st_Pixel.y - 4, st_Pixel.x + 5, st_Pixel.y + 5);
		}
		else
		{
			(VOID)st_Dc->Ellipse(st_Pixel.x - 5, st_Pixel.y - 5, st_Pixel.x + 6, st_Pixel.y + 6);
		}

		(VOID)st_Dc->SelectObject(st_OldBrush);
		(VOID)st_Dc->SelectObject(st_OldPen);
	}

	if (f_Num_FormatFixed(pt_Label, UI_PLOT_LABEL_SIZE, st_Sample->simTime, 3) > 0)
	{
		(VOID)st_Dc->SetBkMode(TRANSPARENT);
		(VOID)st_Dc->SetTextColor(RGB(32, 32, 32));
		(VOID)st_Dc->SetTextAlign(TA_RIGHT | TA_TOP);
		(VOID)st_Dc->TextOut(st_Area.right, 4, _T("t = ") + CString(pt_Label) + _T(" s"));
	}
}
