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
#define PLOT_MIN_SPAN			1000.0					// [m]
#define PLOT_SPAN_SCALE			1.18
#define PLOT_MAX_GRID_NUM		8.0
#define PLOT_LON_LABEL_PX		78						// [px @96dpi] 경도 라벨 하나의 폭
#define PLOT_LAT_LABEL_PX		34						// [px @96dpi] 위도 라벨 하나의 높이
#define PLOT_FONT_PX			12.0F
#define PLOT_LABEL_GAP_PX		8.0						// [px @96dpi] 시작점과 이름표 사이
#define PLOT_LABEL_ROOM_X_PX	56.0					// [px @96dpi] 가장자리 시작점의 이름표 자리 (간격 + 글자 폭)
#define PLOT_LABEL_ROOM_Y_PX	26.0					// [px @96dpi] 가장자리 시작점의 이름표 자리 (간격 + 글자 높이)
#define PLOT_LABEL_SIDE_MIN		0.3
#define PLOT_COLOR_PLOT			RGB(31, 41, 55)			// 플롯 점. 표적 색 선과 겹쳐도 보이게 검정에 가깝게
#define PLOT_COLOR_CLUTTER		RGB(156, 163, 175)		// 클러터 점
#define PLOT_PLOT_RADIUS		1.6F					// [px @96dpi]
#define PLOT_CLUTTER_RADIUS		1.4F
#define PLOT_LEGEND_PAD			8.0F					// [px @96dpi]

// 지금 스캔 강조, 트랙 심볼
#define PLOT_HISTORY_ALPHA		90						// 지난 스캔 플롯, 클러터의 불투명도
#define PLOT_FUTURE_ALPHA		60						// 지금 스캔 뒤 참값 궤적 (앞으로 갈 길) 의 불투명도
#define PLOT_SCAN_RADIUS		3.2F					// [px @96dpi] 지금 스캔에 받은 플롯
#define PLOT_TRUE_RADIUS		4.0F					// [px @96dpi] 지금 스캔의 참값 위치
#define PLOT_TRACK_HALF			6.0F					// [px @96dpi] 트랙 사각형 반 변
#define PLOT_COLOR_TRK_TENT		RGB(234, 88, 12)		// 임시 트랙
#define PLOT_COLOR_TRK_CONF		RGB(22, 163, 74)		// 확정 트랙
#define PLOT_COLOR_TRK_MEM		RGB(147, 51, 234)		// 메모리 트랙
#define PLOT_COLOR_TRK_DEL		RGB(107, 114, 128)		// 삭제 트랙
#define PLOT_ZOOM_MAX			200.0
#define PLOT_ZOOM_STEP			1.25					// 휠 한 칸 배율

// 3D 그림
#define TRK3D_UP_SCALE_MAX		5.0						// 고도를 늘려 그리는 최대 배율
#define TRK3D_UP_RATIO			0.4						// 늘린 고도가 바닥 폭의 이 비율쯤 되게 배율을 고름
#define TRK3D_YAW_DEG			(-25.0)					// 처음 보기 방위 [deg]
#define TRK3D_PITCH_DEG			35.0					// 처음 보기 내려다보는 각 [deg]
#define TRK3D_TITLE_HEIGHT		26						// [px @96dpi]
#define TRK3D_MARGIN			12
#define TRK3D_FIT_SCALE			1.08					// 장면 둘레 여백 비율
#define TRK3D_DRAG_DEG_PER_PX	0.4						// 끌기 1 px 당 회전 [deg]
#define TRK3D_MIN_SPAN			2000.0					// [m]
#define TRK3D_MIN_UP			1000.0					// [m] 고도 축 최소 길이
#define TRK3D_SHADOW_ALPHA		170						// 참값 궤적 바닥 그림자 (회색) 의 불투명도

// 범례 줄 종류
#define PLOT_LEG_LINE			0						// 참값 궤적 · 위치
#define PLOT_LEG_DOT			1						// 점 (플롯, 클러터)
#define PLOT_LEG_TRACKS			2						// 트랙 상태 네 가지
#define PLOT_LEG_MAX			4

typedef struct
{
	INT32		nKind;
	COLORREF	color;
	CString		st_Name;
} ST_LegendRow;

// COLORREF + 알파 -> GDI+ 색.
static Gdiplus::Color f_Plot_Color(COLORREF color, INT32 alpha)
{
	return Gdiplus::Color(static_cast<UINT8>(alpha), GetRValue(color), GetGValue(color), GetBValue(color));
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

// {1, 2, 4, 5} x 10^k. 위경도 격자용.
static FLOAT64 f_Plot_NiceStepGeo(FLOAT64 span, FLOAT64 maxTickNum)
{
	static const FLOAT64 s_Mantissa[4] = { 1.0, 2.0, 4.0, 5.0 };

	return f_Plot_NiceStepFrom(span, maxTickNum, s_Mantissa, 4);
}

// {1, 2, 5} x 10^k. 3D 바닥 격자, 고도 눈금용.
static FLOAT64 f_Plot_NiceStepMeter(FLOAT64 span, FLOAT64 maxTickNum)
{
	static const FLOAT64 s_Mantissa[3] = { 1.0, 2.0, 5.0 };

	return f_Plot_NiceStepFrom(span, maxTickNum, s_Mantissa, 3);
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

// 기준점 정렬 글자를 옅은 흰 바탕 위에 그리기. 클러터 점이 깔린 곳에서도 읽히게 함.
static VOID f_Plot_LabelText(Gdiplus::Graphics *st_Graphics, const Gdiplus::Font *st_Font, const CString &st_Text, FLOAT64 x, FLOAT64 y,
	Gdiplus::StringAlignment alignX, Gdiplus::StringAlignment alignY, COLORREF color)
{
	const Gdiplus::PointF	st_Zero(0.0F, 0.0F);
	Gdiplus::RectF			st_Bound;
	Gdiplus::SolidBrush		st_Back(f_Plot_Color(UI_COLOR_CARD, 215));
	FLOAT32					left;
	FLOAT32					top;

	// 글자 크기만 재고 위치는 정렬에 맞춰 직접 계산 (Near = 기준점 오른쪽 / 아래, Far = 왼쪽 / 위)
	(VOID)st_Graphics->MeasureString(st_Text.GetString(), -1, st_Font, st_Zero, &st_Bound);
	left	= static_cast<FLOAT32>(x) - ((alignX == Gdiplus::StringAlignmentFar) ? st_Bound.Width : ((alignX == Gdiplus::StringAlignmentCenter) ? (0.5F * st_Bound.Width) : 0.0F));
	top		= static_cast<FLOAT32>(y) - ((alignY == Gdiplus::StringAlignmentFar) ? st_Bound.Height : ((alignY == Gdiplus::StringAlignmentCenter) ? (0.5F * st_Bound.Height) : 0.0F));
	(VOID)st_Graphics->FillRectangle(&st_Back, left, top, st_Bound.Width, st_Bound.Height);
	f_Plot_Text(st_Graphics, st_Font, st_Text, x, y, alignX, alignY, color);
}

// 점 하나가 들어가게 동-북 범위 넓히기.
static VOID f_Plot_Include(const ST_PlotPoint *st_Point, FLOAT64 *pt_MinEast, FLOAT64 *pt_MaxEast, FLOAT64 *pt_MinNorth, FLOAT64 *pt_MaxNorth)
{
	*pt_MinEast		= fmin(*pt_MinEast, st_Point->east);
	*pt_MaxEast		= fmax(*pt_MaxEast, st_Point->east);
	*pt_MinNorth	= fmin(*pt_MinNorth, st_Point->north);
	*pt_MaxNorth	= fmax(*pt_MaxNorth, st_Point->north);
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

// 평면 제목. 보이는 것 (참값, 플롯, 클러터, 트랙) 만 적음.
static CString f_Plot_MapTitle(INT32 isTrue, INT32 isPlot, INT32 isClutter, INT32 isTrack)
{
	static const LPCTSTR	s_Part[4] = { _T("참값"), _T("플롯"), _T("클러터"), _T("트랙") };
	const INT32				isShown[4] = { isTrue, isPlot, isClutter, isTrack };
	CString					st_Title;
	INT32					nPart;

	for (nPart = 0; nPart < 4; nPart++)
	{
		if (isShown[nPart] != 0)
		{
			if (st_Title.IsEmpty() == FALSE)
			{
				st_Title += _T(" · ");
			}

			st_Title += s_Part[nPart];
		}
	}

	st_Title += (st_Title.IsEmpty() == FALSE) ? _T(" (위도-경도)") : _T("위도-경도");

	return st_Title;
}

// 트랙 상태 색.
static COLORREF f_Plot_TrackStateColor(INT32 nState)
{
	COLORREF color = PLOT_COLOR_TRK_DEL;

	if (nState == TRK_STATE_TENTATIVE)
	{
		color = PLOT_COLOR_TRK_TENT;
	}
	else if (nState == TRK_STATE_CONFIRMED)
	{
		color = PLOT_COLOR_TRK_CONF;
	}
	else if (nState == TRK_STATE_MEMORY)
	{
		color = PLOT_COLOR_TRK_MEM;
	}
	else
	{
		// 삭제
	}

	return color;
}

// 트랙 사각형 하나. 임시 = 빈 사각형, 확정 = 채운 사각형, 메모리 = 점선 사각형, 삭제 = 회색 사각형에 X.
static VOID f_Plot_TrackBox(Gdiplus::Graphics *st_Graphics, FLOAT32 x, FLOAT32 y, FLOAT32 half, INT32 nState, FLOAT32 scale)
{
	static const Gdiplus::REAL	s_Dash[2] = { 2.0F, 1.5F };
	const Gdiplus::Color		st_Color = f_Plot_Color(f_Plot_TrackStateColor(nState), 255);
	Gdiplus::Pen				st_Pen(st_Color, 2.0F * scale);
	Gdiplus::SolidBrush			st_Fill(f_Plot_Color(UI_COLOR_CARD, 200));

	if (nState == TRK_STATE_CONFIRMED)
	{
		Gdiplus::SolidBrush st_Solid(st_Color);

		(VOID)st_Graphics->FillRectangle(&st_Solid, x - half, y - half, 2.0F * half, 2.0F * half);
	}
	else
	{
		(VOID)st_Graphics->FillRectangle(&st_Fill, x - half, y - half, 2.0F * half, 2.0F * half);

		if (nState == TRK_STATE_MEMORY)
		{
			(VOID)st_Pen.SetDashPattern(s_Dash, 2);
		}

		(VOID)st_Graphics->DrawRectangle(&st_Pen, x - half, y - half, 2.0F * half, 2.0F * half);

		if (nState == TRK_STATE_DELETED)
		{
			Gdiplus::Pen st_Cross(st_Color, 1.4F * scale);

			(VOID)st_Graphics->DrawLine(&st_Cross, x - half, y - half, x + half, y + half);
			(VOID)st_Graphics->DrawLine(&st_Cross, x - half, y + half, x + half, y - half);
		}
	}
}

// 트랙 심볼: 속도 선 (TRK_LEAD_TIME 동안 가는 거리), 상태 사각형, ID 글자.
static VOID f_Plot_TrackSymbol(Gdiplus::Graphics *st_Graphics, const Gdiplus::Font *st_Font, const ST_TrkView *st_View, FLOAT64 x, FLOAT64 y,
	FLOAT64 leadX, FLOAT64 leadY, FLOAT32 scale)
{
	const FLOAT32	half = PLOT_TRACK_HALF * scale;
	Gdiplus::Pen	st_Lead(f_Plot_Color(f_Plot_TrackStateColor(st_View->TrackState), 220), 1.6F * scale);
	CString			st_Id;

	(VOID)st_Graphics->DrawLine(&st_Lead, static_cast<FLOAT32>(x), static_cast<FLOAT32>(y), static_cast<FLOAT32>(leadX), static_cast<FLOAT32>(leadY));
	f_Plot_TrackBox(st_Graphics, static_cast<FLOAT32>(x), static_cast<FLOAT32>(y), half, st_View->TrackState, scale);

	st_Id.Format(_T("%u"), st_View->TrackId);

	// 삭제 트랙은 같은 표적에 새로 만든 트랙과 자리가 거의 같아 ID 를 왼쪽 아래에 씀 (새 트랙 ID 는 오른쪽 위)
	if (st_View->TrackState == TRK_STATE_DELETED)
	{
		f_Plot_LabelText(st_Graphics, st_Font, st_Id, x - static_cast<FLOAT64>(half) - 2.0, y + static_cast<FLOAT64>(half),
			Gdiplus::StringAlignmentFar, Gdiplus::StringAlignmentNear, f_Plot_TrackStateColor(st_View->TrackState));
	}
	else
	{
		f_Plot_LabelText(st_Graphics, st_Font, st_Id, x + static_cast<FLOAT64>(half) + 2.0, y - static_cast<FLOAT64>(half),
			Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentFar, f_Plot_TrackStateColor(st_View->TrackState));
	}
}

// 지난 스캔 점 여러 개를 한 번에 그림. 스캔마다 쌓여 수천 개가 되므로 하나씩 안티앨리어싱 원으로 그리지 않고 작은 사각형으로 모아 그림
// (원으로 그리면 그리는 시간이 스캔 수에 비례해 늘어 다시 보내기 중 화면이 스캔을 건너뜀).
static VOID f_Plot_FillDots(Gdiplus::Graphics *st_Graphics, COLORREF color, const std::vector<Gdiplus::RectF> &st_Dot)
{
	const Gdiplus::SmoothingMode	mode = st_Graphics->GetSmoothingMode();
	Gdiplus::SolidBrush				st_Brush(f_Plot_Color(color, PLOT_HISTORY_ALPHA));

	if (st_Dot.empty() == false)
	{
		(VOID)st_Graphics->SetSmoothingMode(Gdiplus::SmoothingModeNone);
		(VOID)st_Graphics->FillRectangles(&st_Brush, st_Dot.data(), static_cast<INT32>(st_Dot.size()));
		(VOID)st_Graphics->SetSmoothingMode(mode);
	}
}

// 지금 스캔에 받은 플롯: 흰 테를 두른 점 (표적 플롯은 검정, 클러터는 회색).
static VOID f_Plot_ScanDot(Gdiplus::Graphics *st_Graphics, FLOAT64 x, FLOAT64 y, INT32 nSource, FLOAT32 scale)
{
	const FLOAT32		radius = PLOT_SCAN_RADIUS * scale;
	Gdiplus::SolidBrush	st_Dot(f_Plot_Color((nSource == 0) ? PLOT_COLOR_CLUTTER : PLOT_COLOR_PLOT, 255));
	Gdiplus::Pen		st_Ring(f_Plot_Color(UI_COLOR_CARD, 255), 1.2F * scale);

	(VOID)st_Graphics->FillEllipse(&st_Dot, static_cast<FLOAT32>(x) - radius, static_cast<FLOAT32>(y) - radius, 2.0F * radius, 2.0F * radius);
	(VOID)st_Graphics->DrawEllipse(&st_Ring, static_cast<FLOAT32>(x) - radius, static_cast<FLOAT32>(y) - radius, 2.0F * radius, 2.0F * radius);
}

// 지금 스캔의 참값 위치: 객체 색 마름모.
static VOID f_Plot_TrueMark(Gdiplus::Graphics *st_Graphics, FLOAT64 x, FLOAT64 y, INT32 nObject, FLOAT32 scale)
{
	const FLOAT32			radius = PLOT_TRUE_RADIUS * scale;
	const Gdiplus::PointF	st_Corner[4] =
	{
		Gdiplus::PointF(static_cast<FLOAT32>(x), static_cast<FLOAT32>(y) - radius), Gdiplus::PointF(static_cast<FLOAT32>(x) + radius, static_cast<FLOAT32>(y)),
		Gdiplus::PointF(static_cast<FLOAT32>(x), static_cast<FLOAT32>(y) + radius), Gdiplus::PointF(static_cast<FLOAT32>(x) - radius, static_cast<FLOAT32>(y))
	};
	Gdiplus::SolidBrush		st_Fill(f_Plot_Color(f_Ui_ObjectColor(nObject), 255));
	Gdiplus::Pen			st_Ring(f_Plot_Color(UI_COLOR_CARD, 255), 1.2F * scale);

	(VOID)st_Graphics->FillPolygon(&st_Fill, st_Corner, 4);
	(VOID)st_Graphics->DrawPolygon(&st_Ring, st_Corner, 4);
}

// 범례 (영역 오른쪽 위 안쪽, isBottom 이면 오른쪽 아래 안쪽).
static VOID f_Plot_Legend(Gdiplus::Graphics *st_Graphics, const Gdiplus::Font *st_Font, const ST_LegendRow *st_Row, INT32 nRowNum, const CRect &st_Area,
	INT32 isBottom, FLOAT32 scale)
{
	static const INT32		s_State[4] = { TRK_STATE_TENTATIVE, TRK_STATE_CONFIRMED, TRK_STATE_MEMORY, TRK_STATE_DELETED };
	static const LPCTSTR	s_StateName[4] = { _T("임시"), _T("확정"), _T("메모리"), _T("삭제") };
	const FLOAT32			pad = PLOT_LEGEND_PAD * scale;
	const FLOAT32			markWidth = 18.0F * scale;
	const FLOAT32			half = 0.8F * PLOT_TRACK_HALF * scale;
	const Gdiplus::PointF	st_Zero(0.0F, 0.0F);
	Gdiplus::RectF			st_Bound;
	Gdiplus::SolidBrush		st_Back(f_Plot_Color(UI_COLOR_CARD, 235));
	Gdiplus::Pen			st_Frame(f_Plot_Color(UI_COLOR_BORDER, 255), 1.0F);
	FLOAT32					stateWidth[4] = {};
	FLOAT32					rowWidth;
	FLOAT32					textWidth = 0.0F;
	FLOAT32					rowHeight = 0.0F;
	FLOAT32					boxWidth;
	FLOAT32					boxHeight;
	FLOAT32					left;
	FLOAT32					top;
	FLOAT32					x;
	FLOAT32					y;
	INT32					nRow;
	INT32					nState;

	for (nState = 0; nState < 4; nState++)
	{
		(VOID)st_Graphics->MeasureString(s_StateName[nState], -1, st_Font, st_Zero, &st_Bound);
		stateWidth[nState] = st_Bound.Width;
	}

	// 줄 폭: 기호 + 이름. 트랙 줄은 이름 뒤에 상태 네 가지
	for (nRow = 0; nRow < nRowNum; nRow++)
	{
		(VOID)st_Graphics->MeasureString(st_Row[nRow].st_Name.GetString(), -1, st_Font, st_Zero, &st_Bound);
		rowHeight = (st_Bound.Height > rowHeight) ? st_Bound.Height : rowHeight;

		if (st_Row[nRow].nKind == PLOT_LEG_TRACKS)
		{
			rowWidth = st_Bound.Width + (0.5F * pad);

			for (nState = 0; nState < 4; nState++)
			{
				rowWidth = rowWidth + (2.0F * half) + (0.3F * pad) + stateWidth[nState] + (0.8F * pad);
			}
		}
		else
		{
			rowWidth = markWidth + (0.5F * pad) + st_Bound.Width;
		}

		textWidth = (rowWidth > textWidth) ? rowWidth : textWidth;
	}

	boxWidth	= pad + textWidth + pad;
	boxHeight	= pad + (static_cast<FLOAT32>(nRowNum) * rowHeight) + pad;
	left		= static_cast<FLOAT32>(st_Area.right) - pad - boxWidth;
	top			= (isBottom != 0) ? (static_cast<FLOAT32>(st_Area.bottom) - pad - boxHeight) : (static_cast<FLOAT32>(st_Area.top) + pad);
	(VOID)st_Graphics->FillRectangle(&st_Back, left, top, boxWidth, boxHeight);
	(VOID)st_Graphics->DrawRectangle(&st_Frame, left, top, boxWidth, boxHeight);

	y = top + pad + (0.5F * rowHeight);

	for (nRow = 0; nRow < nRowNum; nRow++)
	{
		x = left + pad;

		if (st_Row[nRow].nKind == PLOT_LEG_LINE)
		{
			Gdiplus::Pen st_Mark(f_Plot_Color(st_Row[nRow].color, 255), 2.0F * scale);

			(VOID)st_Graphics->DrawLine(&st_Mark, x, y, x + markWidth, y);
			f_Plot_Text(st_Graphics, st_Font, st_Row[nRow].st_Name, static_cast<FLOAT64>(x + markWidth + (0.5F * pad)), static_cast<FLOAT64>(y),
				Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter, UI_COLOR_TEXT);
		}
		else if (st_Row[nRow].nKind == PLOT_LEG_DOT)
		{
			Gdiplus::SolidBrush	st_Mark(f_Plot_Color(st_Row[nRow].color, 255));
			const FLOAT32		radius = 3.0F * scale;

			(VOID)st_Graphics->FillEllipse(&st_Mark, x + (0.5F * markWidth) - radius, y - radius, 2.0F * radius, 2.0F * radius);
			f_Plot_Text(st_Graphics, st_Font, st_Row[nRow].st_Name, static_cast<FLOAT64>(x + markWidth + (0.5F * pad)), static_cast<FLOAT64>(y),
				Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter, UI_COLOR_TEXT);
		}
		else
		{
			// 트랙 줄: 이름을 앞에 쓰고 상태 네 가지를 뒤에 늘어놓음
			f_Plot_Text(st_Graphics, st_Font, st_Row[nRow].st_Name, static_cast<FLOAT64>(x), static_cast<FLOAT64>(y), Gdiplus::StringAlignmentNear,
				Gdiplus::StringAlignmentCenter, UI_COLOR_TEXT);
			(VOID)st_Graphics->MeasureString(st_Row[nRow].st_Name.GetString(), -1, st_Font, st_Zero, &st_Bound);
			x = x + st_Bound.Width + (0.5F * pad);

			for (nState = 0; nState < 4; nState++)
			{
				f_Plot_TrackBox(st_Graphics, x + half, y, half, s_State[nState], scale);
				x = x + (2.0F * half) + (0.3F * pad);
				f_Plot_Text(st_Graphics, st_Font, CString(s_StateName[nState]), static_cast<FLOAT64>(x), static_cast<FLOAT64>(y), Gdiplus::StringAlignmentNear,
					Gdiplus::StringAlignmentCenter, f_Plot_TrackStateColor(s_State[nState]));
				x = x + stateWidth[nState] + (0.8F * pad);
			}
		}

		y = y + rowHeight;
	}
}

// 보이는 것에 맞춘 범례 줄. 줄 수 반환.
static INT32 f_Plot_LegendRows(ST_LegendRow *st_Row, INT32 isShowTrue, INT32 isShowPlot, INT32 isShowClutter, INT32 isShowTrack)
{
	INT32 nRowNum = 0;

	if (isShowTrue != 0)
	{
		st_Row[nRowNum].nKind	= PLOT_LEG_LINE;
		st_Row[nRowNum].color	= UI_COLOR_TEXT_SUB;
		st_Row[nRowNum].st_Name	= _T("참값 궤적, 지금 위치 (표적 색)");
		nRowNum++;
	}

	if (isShowPlot != 0)
	{
		st_Row[nRowNum].nKind	= PLOT_LEG_DOT;
		st_Row[nRowNum].color	= PLOT_COLOR_PLOT;
		st_Row[nRowNum].st_Name	= _T("플롯 (큰 점은 지금 스캔)");
		nRowNum++;
	}

	if (isShowClutter != 0)
	{
		st_Row[nRowNum].nKind	= PLOT_LEG_DOT;
		st_Row[nRowNum].color	= PLOT_COLOR_CLUTTER;
		st_Row[nRowNum].st_Name	= _T("클러터");
		nRowNum++;
	}

	if (isShowTrack != 0)
	{
		st_Row[nRowNum].nKind	= PLOT_LEG_TRACKS;
		st_Row[nRowNum].color	= UI_COLOR_TEXT;
		st_Row[nRowNum].st_Name	= _T("트랙");
		nRowNum++;
	}

	return nRowNum;
}

BEGIN_MESSAGE_MAP(CTrajectoryPlot, CStatic)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_MOUSEMOVE()
	ON_WM_CAPTURECHANGED()
END_MESSAGE_MAP()

// 2D 그림. 결과 버퍼는 대화상자 소유, 여기서는 읽기만.
CTrajectoryPlot::CTrajectoryPlot() noexcept
	: st_Result(nullptr)
	, st_Measure(nullptr)
	, st_Track(nullptr)
	, isShowTrue(1)
	, isShowPlot(1)
	, isShowClutter(1)
	, isShowTrack(1)
	, nCurScan(0)
	, dpi(UI_BASE_DPI)
	, pixelPerMeter(1.0)
	, centerEast(0.0)
	, centerNorth(0.0)
	, centerLat(0.0)
	, centerLon(0.0)
	, gridLatDeg(0.01)
	, gridLonDeg(0.01)
	, zoom(1.0)
	, panEast(0.0)
	, panNorth(0.0)
	, isDragging(0)
	, st_DragStart(0, 0)
	, dragPanEast(0.0)
	, dragPanNorth(0.0)
	, st_MapArea(0, 0, 0, 0)
{
}

// 화면 배율 설정.
VOID CTrajectoryPlot::f_SetDpi(INT32 newDpi)
{
	dpi = newDpi;
}

// 그릴 결과 교체 후 다시 그리기. 측정 모의, 트랙 결과는 st_NewResult 와 같은 실행의 것이어야 함.
VOID CTrajectoryPlot::f_SetData(const CSimResult *st_NewResult, const CPlotResult *st_NewMeasure, const CTrackResult *st_NewTrack)
{
	st_Result	= st_NewResult;
	st_Measure	= st_NewMeasure;
	st_Track	= st_NewTrack;

	Invalidate(FALSE);
}

// 표시 여부 (참값, 플롯, 클러터, 트랙) 교체 후 다시 그리기.
VOID CTrajectoryPlot::f_SetShow(INT32 isNewShowTrue, INT32 isNewShowPlot, INT32 isNewShowClutter, INT32 isNewShowTrack)
{
	isShowTrue		= isNewShowTrue;
	isShowPlot		= isNewShowPlot;
	isShowClutter	= isNewShowClutter;
	isShowTrack		= isNewShowTrack;

	Invalidate(FALSE);
}

// 보일 스캔 번호 바꾼 뒤 다시 그리기.
VOID CTrajectoryPlot::f_SetScan(INT32 nNewScan)
{
	nCurScan = nNewScan;

	Invalidate(FALSE);
}

// 휠 확대 · 축소. 커서 아래 지점이 제자리에 있도록 중심을 옮김. 커서가 평면 밖이면 무시.
VOID CTrajectoryPlot::f_Zoom(INT32 delta, CPoint st_ScreenPoint)
{
	CPoint	st_Point = st_ScreenPoint;
	FLOAT64	offsetX;
	FLOAT64	offsetY;
	FLOAT64	cursorEast;
	FLOAT64	cursorNorth;
	FLOAT64	newZoom;
	FLOAT64	newPixelPerMeter;

	ScreenToClient(&st_Point);

	if ((st_MapArea.PtInRect(st_Point) != FALSE) && (pixelPerMeter > 0.0))
	{
		offsetX				= static_cast<FLOAT64>(st_Point.x) - (static_cast<FLOAT64>(st_MapArea.left) + (0.5 * static_cast<FLOAT64>(st_MapArea.Width())));
		offsetY				= static_cast<FLOAT64>(st_Point.y) - (static_cast<FLOAT64>(st_MapArea.top) + (0.5 * static_cast<FLOAT64>(st_MapArea.Height())));
		cursorEast			= centerEast + (offsetX / pixelPerMeter);
		cursorNorth			= centerNorth - (offsetY / pixelPerMeter);
		newZoom				= fmin(PLOT_ZOOM_MAX, fmax(1.0, zoom * pow(PLOT_ZOOM_STEP, static_cast<FLOAT64>(delta) / static_cast<FLOAT64>(WHEEL_DELTA))));
		newPixelPerMeter	= pixelPerMeter * (newZoom / zoom);

		// 새 중심 = 커서 지점 - 커서까지 픽셀 / 새 축척
		panEast		= panEast + ((cursorEast - (offsetX / newPixelPerMeter)) - centerEast);
		panNorth	= panNorth + ((cursorNorth + (offsetY / newPixelPerMeter)) - centerNorth);
		zoom		= newZoom;

		if (zoom <= 1.0)
		{
			panEast		= 0.0;
			panNorth	= 0.0;
		}

		Invalidate(FALSE);
	}
}

// 끌기 시작.
VOID CTrajectoryPlot::OnLButtonDown(UINT32 flags, CPoint st_Point)
{
	isDragging		= 1;
	st_DragStart	= st_Point;
	dragPanEast		= panEast;
	dragPanNorth	= panNorth;
	(VOID)SetCapture();

	CStatic::OnLButtonDown(flags, st_Point);
}

// 끌기 끝.
VOID CTrajectoryPlot::OnLButtonUp(UINT32 flags, CPoint st_Point)
{
	isDragging = 0;

	if (GetCapture() == this)
	{
		(VOID)ReleaseCapture();
	}

	CStatic::OnLButtonUp(flags, st_Point);
}

// 두 번 클릭하면 처음 보기 (확대 1 배, 이동 없음).
VOID CTrajectoryPlot::OnLButtonDblClk(UINT32 flags, CPoint st_Point)
{
	zoom		= 1.0;
	panEast		= 0.0;
	panNorth	= 0.0;
	Invalidate(FALSE);

	CStatic::OnLButtonDblClk(flags, st_Point);
}

// 끄는 동안 평면 이동.
VOID CTrajectoryPlot::OnMouseMove(UINT32 flags, CPoint st_Point)
{
	if ((isDragging != 0) && (pixelPerMeter > 0.0))
	{
		panEast		= dragPanEast - (static_cast<FLOAT64>(st_Point.x - st_DragStart.x) / pixelPerMeter);
		panNorth	= dragPanNorth + (static_cast<FLOAT64>(st_Point.y - st_DragStart.y) / pixelPerMeter);
		Invalidate(FALSE);
	}

	CStatic::OnMouseMove(flags, st_Point);
}

// 다른 창이 마우스를 가져가면 끌기 끝.
VOID CTrajectoryPlot::OnCaptureChanged(CWnd *st_Wnd)
{
	isDragging = 0;

	CStatic::OnCaptureChanged(st_Wnd);
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

	if ((st_Result != nullptr) && (st_Measure != nullptr) && (st_Track != nullptr) && (st_Client.Width() > 0) && (st_Client.Height() > 0))
	{
		f_ComputeLayout(st_Client);
		f_ComputeView();

		{
			Gdiplus::Graphics st_Graphics(st_MemDc.GetSafeHdc());

			(VOID)st_Graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
			f_DrawMap(&st_Graphics);
		}
	}

	(VOID)st_Dc.BitBlt(0, 0, st_Client.Width(), st_Client.Height(), &st_MemDc, 0, 0, SRCCOPY);
	(VOID)st_MemDc.SelectObject(st_OldBitmap);
}

// 평면 배치. 컨트롤 전체를 쓰고 왼쪽은 위도 라벨, 위는 제목, 아래는 경도 라벨 자리.
VOID CTrajectoryPlot::f_ComputeLayout(const CRect &st_Client)
{
	st_MapArea.SetRect(st_Client.left + f_Ui_Scale(PLOT_MARGIN_LEFT, dpi), st_Client.top + f_Ui_Scale(PLOT_TITLE_HEIGHT, dpi),
		st_Client.right - f_Ui_Scale(PLOT_MARGIN_RIGHT, dpi), st_Client.bottom - f_Ui_Scale(PLOT_AXIS_HEIGHT, dpi));
}

// 모든 점이 들어가는 축척, 중심, 눈금 간격 계산. 참값 궤적은 숨겨도 범위에 넣어 켜고 끌 때 축척이 바뀌지 않게 하고,
// 플롯, 클러터는 지금 스캔이 아니라 전체 스캔을 넣어 스캔을 바꿔도 축척이 그대로이게 함.
// 휠로 확대한 배율, 끌어 옮긴 양은 이 처음 보기에 곱하고 더함.
VOID CTrajectoryPlot::f_ComputeView(VOID)
{
	const INT32				nSampleNum = st_Result->f_GetSampleNum();
	const INT32				nObjectNum = st_Result->f_GetObjectNum();
	const FLOAT64			width = static_cast<FLOAT64>(st_MapArea.Width());
	const FLOAT64			height = static_cast<FLOAT64>(st_MapArea.Height());
	const FLOAT64			scale = static_cast<FLOAT64>(f_Ui_Scale(100, dpi)) / 100.0;
	FLOAT64					minEast = 0.0;
	FLOAT64					maxEast = 0.0;
	FLOAT64					minNorth = 0.0;
	FLOAT64					maxNorth = 0.0;
	FLOAT64					spanEast;
	FLOAT64					spanNorth;
	FLOAT64					halfEast;
	FLOAT64					halfNorth;
	STRUCT_Coord_Lla		st_Center;
	STRUCT_Coord_Lla		st_Low;
	STRUCT_Coord_Lla		st_High;
	INT32					nStep;
	INT32					nObject;
	INT32					nScan;
	INT32					nIndex;

	// 원점(플랫폼 초기 위치)은 항상 포함.
	for (nStep = 0; nStep < nSampleNum; nStep++)
	{
		for (nObject = 0; nObject < nObjectNum; nObject++)
		{
			f_Plot_Include(st_Result->f_GetPoint(nStep, nObject), &minEast, &maxEast, &minNorth, &maxNorth);
		}
	}

	// 보이는 플롯, 클러터도 포함. 클러터를 보이면 FOV 부채꼴이 다 들어옴.
	for (nScan = 0; nScan < st_Measure->f_GetScanNum(); nScan++)
	{
		for (nIndex = 0; (isShowPlot != 0) && (nIndex < st_Measure->f_GetTargetNum()); nIndex++)
		{
			f_Plot_Include(st_Measure->f_GetPlotPoint(nScan, nIndex), &minEast, &maxEast, &minNorth, &maxNorth);
		}

		for (nIndex = 0; (isShowClutter != 0) && (nIndex < st_Measure->f_GetScan(nScan)->nClutterNum); nIndex++)
		{
			f_Plot_Include(st_Measure->f_GetClutterPoint(nScan, nIndex), &minEast, &maxEast, &minNorth, &maxNorth);
		}
	}

	// 등축척. 여백은 범위 비율 (PLOT_SPAN_SCALE) 과 가장자리 시작점의 이름표 자리 중 큰 쪽 (이름표가 평면 밖으로 잘리지 않게).
	spanEast		= fmax(maxEast - minEast, PLOT_MIN_SPAN);
	spanNorth		= fmax(maxNorth - minNorth, PLOT_MIN_SPAN);
	pixelPerMeter	= fmin(fmin(width / (spanEast * PLOT_SPAN_SCALE), fmax(width - (2.0 * PLOT_LABEL_ROOM_X_PX * scale), 1.0) / spanEast),
						fmin(height / (spanNorth * PLOT_SPAN_SCALE), fmax(height - (2.0 * PLOT_LABEL_ROOM_Y_PX * scale), 1.0) / spanNorth));
	pixelPerMeter	= pixelPerMeter * zoom;
	centerEast		= (0.5 * (minEast + maxEast)) + panEast;
	centerNorth		= (0.5 * (minNorth + maxNorth)) + panNorth;
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
	st_Point.up		= -st_Ned.z;

	return st_Point;
}

// 동-북 평면: 위경도 격자, 시작점 이름표, 지난 스캔 플롯 · 클러터, 참값 궤적 (지금 스캔 뒤는 옅게), 시작점을 깔고 맨 위에 지금 스캔의 참값 위치, 받은 플롯, 트랙을 그림.
// 참값을 숨기면 시작점은 플랫폼 (레이다) 만 그림.
VOID CTrajectoryPlot::f_DrawMap(Gdiplus::Graphics *st_Graphics) const
{
	const FLOAT32				scale = static_cast<FLOAT32>(f_Ui_Scale(100, dpi)) / 100.0F;
	const Gdiplus::FontFamily	st_Family(L"Malgun Gothic");
	const Gdiplus::Font			st_Font(&st_Family, scale * PLOT_FONT_PX, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	const Gdiplus::Font			st_Bold(&st_Family, scale * PLOT_FONT_PX, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
	Gdiplus::Pen				st_GridPen(f_Plot_Color(UI_COLOR_GRID_LINE, 255), 1.0F);
	Gdiplus::Pen				st_FramePen(f_Plot_Color(UI_COLOR_BORDER, 255), 1.0F);
	const Gdiplus::Rect			st_Clip(st_MapArea.left, st_MapArea.top, st_MapArea.Width(), st_MapArea.Height());
	const INT32					nStartNum = (isShowTrue != 0) ? st_Result->f_GetObjectNum() : 1;	// 시작점을 그릴 객체 수 (0 = 플랫폼)
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
	CString						st_Zoom;
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

	f_Plot_Text(st_Graphics, &st_Bold, f_Plot_MapTitle(isShowTrue, isShowPlot, isShowClutter, isShowTrack),
		static_cast<FLOAT64>(st_MapArea.left), static_cast<FLOAT64>(st_MapArea.top) - 5.0, Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentFar, UI_COLOR_TEXT);
	f_Plot_Text(st_Graphics, &st_Font, CString(_T("Lon [deg]")), static_cast<FLOAT64>(st_MapArea.right) - 6.0, static_cast<FLOAT64>(st_MapArea.bottom) - 4.0,
		Gdiplus::StringAlignmentFar, Gdiplus::StringAlignmentFar, UI_COLOR_TEXT_OFF);
	f_Plot_Text(st_Graphics, &st_Font, CString(_T("Lat [deg]")), static_cast<FLOAT64>(st_MapArea.left) + 6.0, static_cast<FLOAT64>(st_MapArea.top) + 4.0,
		Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentNear, UI_COLOR_TEXT_OFF);

	// 확대 중이면 평면 왼쪽 위 안쪽 (Lat 글자 아래) 에 배율. 제목, 범례와 겹치지 않는 자리
	if (zoom > 1.0)
	{
		st_Zoom.Format(_T("%.1f배, 두 번 클릭하면 처음 보기"), zoom);
		f_Plot_Text(st_Graphics, &st_Font, st_Zoom, static_cast<FLOAT64>(st_MapArea.left) + 6.0,
			static_cast<FLOAT64>(st_MapArea.top) + 6.0 + static_cast<FLOAT64>(scale * PLOT_FONT_PX * 1.4F), Gdiplus::StringAlignmentNear,
			Gdiplus::StringAlignmentNear, UI_COLOR_TEXT_SUB);
	}

	(VOID)st_Graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	(VOID)st_Graphics->SetClip(st_Clip);

	// 시작점 이름표는 맨 먼저 깔아, 흰 바탕이 시작점 원이나 그 근처를 지나는 다른 표적의 궤적, 플롯, 트랙을 가리지 않게 함.
	f_DrawStartLabels(st_Graphics, &st_Font, nStartNum);

	// 지난 스캔 플롯, 클러터는 궤적 아래에 깔아 참값 선이 가려지지 않게 함.
	f_DrawMapHistory(st_Graphics);

	if (isShowTrue != 0)
	{
		f_DrawMapTruth(st_Graphics);
	}

	// 시작점 원
	for (nObject = 0; nObject < nStartNum; nObject++)
	{
		const FLOAT32		radius = 5.0F * scale;
		Gdiplus::SolidBrush	st_Fill(f_Plot_Color(UI_COLOR_CARD, 255));
		Gdiplus::Pen		st_Ring(f_Plot_Color(f_Ui_ObjectColor(nObject), 255), 2.0F * scale);

		f_MapToPixel(st_Result->f_GetPoint(0, nObject), &x, &y);
		(VOID)st_Graphics->FillEllipse(&st_Fill, static_cast<FLOAT32>(x) - radius, static_cast<FLOAT32>(y) - radius, 2.0F * radius, 2.0F * radius);
		(VOID)st_Graphics->DrawEllipse(&st_Ring, static_cast<FLOAT32>(x) - radius, static_cast<FLOAT32>(y) - radius, 2.0F * radius, 2.0F * radius);
	}

	f_DrawMapScan(st_Graphics, &st_Font);
	f_DrawMapLegend(st_Graphics, &st_Font);

	(VOID)st_Graphics->ResetClip();
}

// 평면에 참값 궤적 그리기. 객체 색 선, 꺾이는 곳은 둥글게. 지금 스캔까지는 진하게, 그 뒤 (앞으로 갈 길) 는 옅게.
VOID CTrajectoryPlot::f_DrawMapTruth(Gdiplus::Graphics *st_Graphics) const
{
	const FLOAT32					scale = static_cast<FLOAT32>(f_Ui_Scale(100, dpi)) / 100.0F;
	const INT32						nSampleNum = st_Result->f_GetSampleNum();
	const INT32						nNow = std::min(std::max(nCurScan, 0), nSampleNum - 1);		// 지금 스캔의 표본
	std::vector<Gdiplus::PointF>	st_Line(static_cast<UINT64>(nSampleNum));
	FLOAT64							x = 0.0;
	FLOAT64							y = 0.0;
	INT32							nObject;
	INT32							nSample;

	for (nObject = 0; nObject < st_Result->f_GetObjectNum(); nObject++)
	{
		Gdiplus::Pen st_TruthPen(f_Plot_Color(f_Ui_ObjectColor(nObject), 255), 2.0F * scale);
		Gdiplus::Pen st_FuturePen(f_Plot_Color(f_Ui_ObjectColor(nObject), PLOT_FUTURE_ALPHA), 1.4F * scale);

		(VOID)st_TruthPen.SetLineJoin(Gdiplus::LineJoinRound);
		(VOID)st_FuturePen.SetLineJoin(Gdiplus::LineJoinRound);

		for (nSample = 0; nSample < nSampleNum; nSample++)
		{
			f_MapToPixel(st_Result->f_GetPoint(nSample, nObject), &x, &y);
			st_Line[static_cast<UINT64>(nSample)] = Gdiplus::PointF(static_cast<FLOAT32>(x), static_cast<FLOAT32>(y));
		}

		if (nNow < (nSampleNum - 1))
		{
			(VOID)st_Graphics->DrawLines(&st_FuturePen, st_Line.data() + nNow, nSampleNum - nNow);
		}

		if (nNow > 0)
		{
			(VOID)st_Graphics->DrawLines(&st_TruthPen, st_Line.data(), nNow + 1);
		}
	}
}

// 평면에 지금 스캔까지의 플롯, 클러터를 옅게 그림. 클러터를 먼저, 플롯을 그 위에.
// 지금 스캔은 f_DrawMapScan 이 데이터 처리 쪽이 받은 플롯으로 따로 강조함.
VOID CTrajectoryPlot::f_DrawMapHistory(Gdiplus::Graphics *st_Graphics) const
{
	const FLOAT32				scale = static_cast<FLOAT32>(f_Ui_Scale(100, dpi)) / 100.0F;
	const FLOAT32				clutterRadius = PLOT_CLUTTER_RADIUS * scale;
	const FLOAT32				plotRadius = PLOT_PLOT_RADIUS * scale;
	const INT32					nScanEnd = std::min(nCurScan + 1, st_Measure->f_GetScanNum());
	std::vector<Gdiplus::RectF>	st_Dot;
	FLOAT64						x = 0.0;
	FLOAT64						y = 0.0;
	INT32						nScan;
	INT32						nIndex;

	for (nScan = 0; (isShowClutter != 0) && (nScan < nScanEnd); nScan++)
	{
		for (nIndex = 0; nIndex < st_Measure->f_GetScan(nScan)->nClutterNum; nIndex++)
		{
			f_MapToPixel(st_Measure->f_GetClutterPoint(nScan, nIndex), &x, &y);
			st_Dot.push_back(Gdiplus::RectF(static_cast<FLOAT32>(x) - clutterRadius, static_cast<FLOAT32>(y) - clutterRadius, 2.0F * clutterRadius,
				2.0F * clutterRadius));
		}
	}

	f_Plot_FillDots(st_Graphics, PLOT_COLOR_CLUTTER, st_Dot);
	st_Dot.clear();

	for (nScan = 0; (isShowPlot != 0) && (nScan < nScanEnd); nScan++)
	{
		for (nIndex = 0; nIndex < st_Measure->f_GetTargetNum(); nIndex++)
		{
			f_MapToPixel(st_Measure->f_GetPlotPoint(nScan, nIndex), &x, &y);
			st_Dot.push_back(Gdiplus::RectF(static_cast<FLOAT32>(x) - plotRadius, static_cast<FLOAT32>(y) - plotRadius, 2.0F * plotRadius, 2.0F * plotRadius));
		}
	}

	f_Plot_FillDots(st_Graphics, PLOT_COLOR_PLOT, st_Dot);
}

// 지금 스캔: 데이터 처리 쓰레드가 받은 플롯 (큰 점), 참값 위치 (객체 색 마름모), 트랙 (상태 사각형 + 속도 선 + ID).
VOID CTrajectoryPlot::f_DrawMapScan(Gdiplus::Graphics *st_Graphics, const Gdiplus::Font *st_Font) const
{
	const FLOAT32		scale = static_cast<FLOAT32>(f_Ui_Scale(100, dpi)) / 100.0F;
	const INT32			nIndex = st_Track->f_FindScan(nCurScan);
	const ST_TrackItem	*st_Item;
	const ST_RecvPlot	*st_Plot;
	FLOAT64				x = 0.0;
	FLOAT64				y = 0.0;
	FLOAT64				leadX = 0.0;
	FLOAT64				leadY = 0.0;
	INT32				nObject;
	INT32				nItem;

	for (nItem = 0; (nIndex >= 0) && (nItem < st_Track->f_GetPlotNum(nIndex)); nItem++)
	{
		st_Plot = st_Track->f_GetPlot(nIndex, nItem);

		if (((st_Plot->nSource == 0) && (isShowClutter != 0)) || ((st_Plot->nSource != 0) && (isShowPlot != 0)))
		{
			f_MapToPixel(&st_Plot->st_Point, &x, &y);
			f_Plot_ScanDot(st_Graphics, x, y, st_Plot->nSource, scale);
		}
	}

	for (nObject = 1; (isShowTrue != 0) && (nObject < st_Result->f_GetObjectNum()) && (nCurScan < st_Result->f_GetSampleNum()); nObject++)
	{
		f_MapToPixel(st_Result->f_GetPoint(nCurScan, nObject), &x, &y);
		f_Plot_TrueMark(st_Graphics, x, y, nObject, scale);
	}

	for (nItem = 0; (nIndex >= 0) && (isShowTrack != 0) && (nItem < st_Track->f_GetTrackNum(nIndex)); nItem++)
	{
		st_Item = st_Track->f_GetTrack(nIndex, nItem);
		f_MapToPixel(&st_Item->st_Point, &x, &y);
		f_MapToPixel(&st_Item->st_Lead, &leadX, &leadY);
		f_Plot_TrackSymbol(st_Graphics, st_Font, &st_Item->st_View, x, y, leadX, leadY, scale);
	}
}

// 범례. 참값은 객체 색 선, 플롯과 클러터는 점, 트랙은 상태별 사각형. 보이는 것만 적음.
// 표적이 많은 북쪽을 가리지 않게 평면 오른쪽 아래 안쪽에 둠.
VOID CTrajectoryPlot::f_DrawMapLegend(Gdiplus::Graphics *st_Graphics, const Gdiplus::Font *st_Font) const
{
	const FLOAT32	scale = static_cast<FLOAT32>(f_Ui_Scale(100, dpi)) / 100.0F;
	ST_LegendRow	st_Row[PLOT_LEG_MAX];
	INT32			nRowNum;

	nRowNum = f_Plot_LegendRows(st_Row, isShowTrue, isShowPlot, isShowClutter, isShowTrack);

	if (nRowNum > 0)
	{
		f_Plot_Legend(st_Graphics, st_Font, st_Row, nRowNum, st_MapArea, 1, scale);
	}
}

// 시작점 이름표 (앞 nStartNum 객체). 출발 방향 반대쪽에 표시. 플롯, 클러터를 보일 때는 흰 바탕을 깔아 읽히게 함.
VOID CTrajectoryPlot::f_DrawStartLabels(Gdiplus::Graphics *st_Graphics, const Gdiplus::Font *st_Font, INT32 nStartNum) const
{
	const FLOAT64				gap = PLOT_LABEL_GAP_PX * static_cast<FLOAT64>(f_Ui_Scale(100, dpi)) / 100.0;
	const INT32					isBacked = ((isShowPlot != 0) || (isShowClutter != 0)) ? 1 : 0;
	Gdiplus::StringAlignment	alignX;
	Gdiplus::StringAlignment	alignY;
	FLOAT64						x = 0.0;
	FLOAT64						y = 0.0;
	FLOAT64						yaw;
	INT32						nObject;

	for (nObject = 0; nObject < nStartNum; nObject++)
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

		if (isBacked != 0)
		{
			f_Plot_LabelText(st_Graphics, st_Font, f_Plot_ObjectName(nObject),
				(alignX == Gdiplus::StringAlignmentFar) ? (x - gap) : (x + gap), (alignY == Gdiplus::StringAlignmentFar) ? (y - gap) : (y + gap),
				alignX, alignY, f_Ui_ObjectColor(nObject));
		}
		else
		{
			f_Plot_Text(st_Graphics, st_Font, f_Plot_ObjectName(nObject),
				(alignX == Gdiplus::StringAlignmentFar) ? (x - gap) : (x + gap), (alignY == Gdiplus::StringAlignmentFar) ? (y - gap) : (y + gap),
				alignX, alignY, f_Ui_ObjectColor(nObject));
		}
	}
}

// CTrackView3D

BEGIN_MESSAGE_MAP(CTrackView3D, CStatic)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_MOUSEMOVE()
	ON_WM_CAPTURECHANGED()
END_MESSAGE_MAP()

// 3D 그림. 결과 버퍼는 대화상자 소유, 여기서는 읽기만.
CTrackView3D::CTrackView3D() noexcept
	: st_Result(nullptr)
	, st_Measure(nullptr)
	, st_Track(nullptr)
	, isShowTrue(1)
	, isShowPlot(1)
	, isShowClutter(1)
	, isShowTrack(1)
	, nCurScan(0)
	, dpi(UI_BASE_DPI)
	, viewYaw(f_Deg_To_Rad(TRK3D_YAW_DEG))
	, viewPitch(f_Deg_To_Rad(TRK3D_PITCH_DEG))
	, zoom(1.0)
	, pixelPerMeter(1.0)
	, centerEast(0.0)
	, centerNorth(0.0)
	, centerUp(0.0)
	, minEast(0.0)
	, maxEast(0.0)
	, minNorth(0.0)
	, maxNorth(0.0)
	, maxUp(0.0)
	, upScale(1.0)
	, gridStep(1000.0)
	, isShowLegend(1)
	, isDragging(0)
	, st_DragStart(0, 0)
	, dragYaw(0.0)
	, dragPitch(0.0)
	, st_Area(0, 0, 0, 0)
{
}

// 화면 배율 설정.
VOID CTrackView3D::f_SetDpi(INT32 newDpi)
{
	dpi = newDpi;
}

// 그릴 결과 교체 후 다시 그리기.
VOID CTrackView3D::f_SetData(const CSimResult *st_NewResult, const CPlotResult *st_NewMeasure, const CTrackResult *st_NewTrack)
{
	st_Result	= st_NewResult;
	st_Measure	= st_NewMeasure;
	st_Track	= st_NewTrack;

	Invalidate(FALSE);
}

// 표시 여부 (참값, 플롯, 클러터, 트랙) 교체 후 다시 그리기.
VOID CTrackView3D::f_SetShow(INT32 isNewShowTrue, INT32 isNewShowPlot, INT32 isNewShowClutter, INT32 isNewShowTrack)
{
	isShowTrue		= isNewShowTrue;
	isShowPlot		= isNewShowPlot;
	isShowClutter	= isNewShowClutter;
	isShowTrack		= isNewShowTrack;

	Invalidate(FALSE);
}

// 보일 스캔 번호 바꾼 뒤 다시 그리기.
VOID CTrackView3D::f_SetScan(INT32 nNewScan)
{
	nCurScan = nNewScan;

	Invalidate(FALSE);
}

// 범례 보이기 (2D 그림을 함께 보일 때는 2D 에만 범례를 둠).
VOID CTrackView3D::f_SetLegend(INT32 isNewShowLegend)
{
	isShowLegend = isNewShowLegend;

	Invalidate(FALSE);
}

// 휠 확대 · 축소 (그림 가운데 기준). 커서가 그림 밖이면 무시.
VOID CTrackView3D::f_Zoom(INT32 delta, CPoint st_ScreenPoint)
{
	CPoint	st_Point = st_ScreenPoint;
	CRect	st_Client;

	ScreenToClient(&st_Point);
	GetClientRect(&st_Client);

	if (st_Client.PtInRect(st_Point) != FALSE)
	{
		zoom = fmin(PLOT_ZOOM_MAX, fmax(0.5, zoom * pow(PLOT_ZOOM_STEP, static_cast<FLOAT64>(delta) / static_cast<FLOAT64>(WHEEL_DELTA))));
		Invalidate(FALSE);
	}
}

// 끌기 시작 (회전).
VOID CTrackView3D::OnLButtonDown(UINT32 flags, CPoint st_Point)
{
	isDragging		= 1;
	st_DragStart	= st_Point;
	dragYaw			= viewYaw;
	dragPitch		= viewPitch;
	(VOID)SetCapture();

	CStatic::OnLButtonDown(flags, st_Point);
}

// 끌기 끝.
VOID CTrackView3D::OnLButtonUp(UINT32 flags, CPoint st_Point)
{
	isDragging = 0;

	if (GetCapture() == this)
	{
		(VOID)ReleaseCapture();
	}

	CStatic::OnLButtonUp(flags, st_Point);
}

// 두 번 클릭하면 처음 보기.
VOID CTrackView3D::OnLButtonDblClk(UINT32 flags, CPoint st_Point)
{
	viewYaw		= f_Deg_To_Rad(TRK3D_YAW_DEG);
	viewPitch	= f_Deg_To_Rad(TRK3D_PITCH_DEG);
	zoom		= 1.0;
	Invalidate(FALSE);

	CStatic::OnLButtonDblClk(flags, st_Point);
}

// 끄는 동안 회전. 좌우 = 방위, 위아래 = 내려다보는 각 (0 ~ 90 도).
VOID CTrackView3D::OnMouseMove(UINT32 flags, CPoint st_Point)
{
	if (isDragging != 0)
	{
		viewYaw		= dragYaw - f_Deg_To_Rad(TRK3D_DRAG_DEG_PER_PX * static_cast<FLOAT64>(st_Point.x - st_DragStart.x));
		viewPitch	= fmin(f_Deg_To_Rad(90.0), fmax(0.0, dragPitch + f_Deg_To_Rad(TRK3D_DRAG_DEG_PER_PX * static_cast<FLOAT64>(st_Point.y - st_DragStart.y))));
		Invalidate(FALSE);
	}

	CStatic::OnMouseMove(flags, st_Point);
}

// 다른 창이 마우스를 가져가면 끌기 끝.
VOID CTrackView3D::OnCaptureChanged(CWnd *st_Wnd)
{
	isDragging = 0;

	CStatic::OnCaptureChanged(st_Wnd);
}

// 배경 지우기 생략. OnPaint 에서 전부 그림.
BOOL CTrackView3D::OnEraseBkgnd(CDC *st_Dc)
{
	UNREFERENCED_PARAMETER(st_Dc);

	return TRUE;
}

// 크기 변경 시 전체 다시 그리기.
VOID CTrackView3D::OnSize(UINT32 type, INT32 width, INT32 height)
{
	CStatic::OnSize(type, width, height);
	Invalidate(FALSE);
}

// 메모리 DC 에 그린 뒤 한 번에 복사.
VOID CTrackView3D::OnPaint(VOID)
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

	if ((st_Result != nullptr) && (st_Measure != nullptr) && (st_Track != nullptr) && (st_Client.Width() > 0) && (st_Client.Height() > 0))
	{
		f_ComputeView(st_Client);

		{
			Gdiplus::Graphics st_Graphics(st_MemDc.GetSafeHdc());

			(VOID)st_Graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
			f_DrawScene(&st_Graphics);
		}
	}

	(VOID)st_Dc.BitBlt(0, 0, st_Client.Width(), st_Client.Height(), &st_MemDc, 0, 0, SRCCOPY);
	(VOID)st_MemDc.SelectObject(st_OldBitmap);
}

// 장면 범위 (바닥은 참값 궤적과 보이는 표적 플롯, 고도는 참값 궤적) 와, 그 상자가 지금 보는 방향에서 그림 영역에 들어가는 축척 계산.
// 클러터는 고각 30 도까지 퍼져 7 km 넘게 올라가므로 범위에 넣지 않음 (넣으면 고도가 늘 1 배가 됨).
VOID CTrackView3D::f_ComputeView(const CRect &st_Client)
{
	const INT32		nSampleNum = st_Result->f_GetSampleNum();
	const INT32		nObjectNum = st_Result->f_GetObjectNum();
	ST_PlotPoint	st_Corner;
	FLOAT64			minX = 0.0;
	FLOAT64			maxX = 0.0;
	FLOAT64			minY = 0.0;
	FLOAT64			maxY = 0.0;
	FLOAT64			x = 0.0;
	FLOAT64			y = 0.0;
	FLOAT64			upMax = 0.0;
	FLOAT64			upStep;
	INT32			nStep;
	INT32			nObject;
	INT32			nScan;
	INT32			nIndex;
	INT32			nCorner;

	st_Area.SetRect(st_Client.left + f_Ui_Scale(TRK3D_MARGIN, dpi), st_Client.top + f_Ui_Scale(TRK3D_TITLE_HEIGHT, dpi),
		st_Client.right - f_Ui_Scale(TRK3D_MARGIN, dpi), st_Client.bottom - f_Ui_Scale(TRK3D_MARGIN, dpi));

	minEast		= 0.0;
	maxEast		= 0.0;
	minNorth	= 0.0;
	maxNorth	= 0.0;

	for (nStep = 0; nStep < nSampleNum; nStep++)
	{
		for (nObject = 0; nObject < nObjectNum; nObject++)
		{
			f_Plot_Include(st_Result->f_GetPoint(nStep, nObject), &minEast, &maxEast, &minNorth, &maxNorth);
			upMax = fmax(upMax, st_Result->f_GetPoint(nStep, nObject)->up);
		}
	}

	for (nScan = 0; (isShowPlot != 0) && (nScan < st_Measure->f_GetScanNum()); nScan++)
	{
		for (nIndex = 0; nIndex < st_Measure->f_GetTargetNum(); nIndex++)
		{
			f_Plot_Include(st_Measure->f_GetPlotPoint(nScan, nIndex), &minEast, &maxEast, &minNorth, &maxNorth);
		}
	}

	// 바닥이 너무 좁으면 넓힘
	if ((maxEast - minEast) < TRK3D_MIN_SPAN)
	{
		minEast = minEast - (0.5 * (TRK3D_MIN_SPAN - (maxEast - minEast)));
		maxEast = minEast + TRK3D_MIN_SPAN;
	}

	if ((maxNorth - minNorth) < TRK3D_MIN_SPAN)
	{
		minNorth = minNorth - (0.5 * (TRK3D_MIN_SPAN - (maxNorth - minNorth)));
		maxNorth = minNorth + TRK3D_MIN_SPAN;
	}

	upStep		= f_Plot_NiceStepMeter(fmax(upMax, TRK3D_MIN_UP), 4.0);
	maxUp		= ceil(fmax(upMax, TRK3D_MIN_UP) / upStep) * upStep;		// 고도 기둥 맨 위 눈금이 가장 높은 참값 위에 오게
	gridStep	= f_Plot_NiceStepMeter(fmax(maxEast - minEast, maxNorth - minNorth), 6.0);
	upScale		= floor(fmax(1.0, fmin(TRK3D_UP_SCALE_MAX, (TRK3D_UP_RATIO * fmax(maxEast - minEast, maxNorth - minNorth)) / maxUp)) + 0.5);
	centerEast	= 0.5 * (minEast + maxEast);
	centerNorth	= 0.5 * (minNorth + maxNorth);
	centerUp	= 0.5 * maxUp;

	// 상자 꼭짓점 8 개를 축척 1 로 투영해 차지하는 폭, 높이를 잼
	pixelPerMeter = 1.0;

	for (nCorner = 0; nCorner < 8; nCorner++)
	{
		st_Corner.east	= ((nCorner & 1) != 0) ? maxEast : minEast;
		st_Corner.north	= ((nCorner & 2) != 0) ? maxNorth : minNorth;
		st_Corner.up	= ((nCorner & 4) != 0) ? maxUp : 0.0;
		f_Project(&st_Corner, &x, &y);
		minX = (nCorner == 0) ? x : fmin(minX, x);
		maxX = (nCorner == 0) ? x : fmax(maxX, x);
		minY = (nCorner == 0) ? y : fmin(minY, y);
		maxY = (nCorner == 0) ? y : fmax(maxY, y);
	}

	pixelPerMeter = zoom * fmin(static_cast<FLOAT64>(st_Area.Width()) / (fmax(maxX - minX, 1.0) * TRK3D_FIT_SCALE),
		static_cast<FLOAT64>(st_Area.Height()) / (fmax(maxY - minY, 1.0) * TRK3D_FIT_SCALE));
}

// 동-북-위 -> 그림 픽셀 (정투영). 바라보는 방위만큼 돌린 뒤 내려다보는 각만큼 기울임. 고도는 upScale 배.
VOID CTrackView3D::f_Project(const ST_PlotPoint *st_Point, FLOAT64 *pt_X, FLOAT64 *pt_Y) const
{
	const FLOAT64	east = st_Point->east - centerEast;
	const FLOAT64	north = st_Point->north - centerNorth;
	const FLOAT64	up = (st_Point->up - centerUp) * upScale;
	const FLOAT64	across = (east * cos(viewYaw)) - (north * sin(viewYaw));		// 화면 가로
	const FLOAT64	depth = (east * sin(viewYaw)) + (north * cos(viewYaw));		// 바라보는 방향 앞쪽

	*pt_X = (static_cast<FLOAT64>(st_Area.left) + (0.5 * static_cast<FLOAT64>(st_Area.Width()))) + (across * pixelPerMeter);
	*pt_Y = (static_cast<FLOAT64>(st_Area.top) + (0.5 * static_cast<FLOAT64>(st_Area.Height()))) - (((up * cos(viewPitch)) + (depth * sin(viewPitch))) * pixelPerMeter);
}

// 점이 장면 상자 (그려진 바닥 격자 범위, 고도 maxUp 아래) 안이면 1. 클러터를 그릴지 고를 때 씀.
INT32 CTrackView3D::f_IsInBox(const ST_PlotPoint *st_Point) const
{
	return ((st_Point->east >= (floor(minEast / gridStep) * gridStep)) && (st_Point->east <= (ceil(maxEast / gridStep) * gridStep)) &&
		(st_Point->north >= (floor(minNorth / gridStep) * gridStep)) && (st_Point->north <= (ceil(maxNorth / gridStep) * gridStep)) &&
		(st_Point->up <= maxUp)) ? 1 : 0;
}

// 바닥 격자 (고도 0), 방위 글자 (N, E), 고도 눈금 기둥 (플랫폼 시작 위치).
VOID CTrackView3D::f_DrawGround(Gdiplus::Graphics *st_Graphics, const Gdiplus::Font *st_Font) const
{
	const FLOAT64	step = gridStep;
	const FLOAT64	upStep = f_Plot_NiceStepMeter(maxUp, 4.0);
	const FLOAT64	east0 = floor(minEast / step) * step;
	const FLOAT64	east1 = ceil(maxEast / step) * step;
	const FLOAT64	north0 = floor(minNorth / step) * step;
	const FLOAT64	north1 = ceil(maxNorth / step) * step;
	Gdiplus::Pen	st_GridPen(f_Plot_Color(UI_COLOR_BORDER, 255), 1.0F);
	Gdiplus::Pen	st_AxisPen(f_Plot_Color(UI_COLOR_TEXT_SUB, 255), 1.2F);
	ST_PlotPoint	st_A;
	ST_PlotPoint	st_B;
	FLOAT64			ax = 0.0;
	FLOAT64			ay = 0.0;
	FLOAT64			bx = 0.0;
	FLOAT64			by = 0.0;
	FLOAT64			value;
	CString			st_Text;

	st_A.up = 0.0;
	st_B.up = 0.0;

	for (value = east0; value <= (east1 + (0.5 * step)); value = value + step)
	{
		st_A.east	= value;
		st_A.north	= north0;
		st_B.east	= value;
		st_B.north	= north1;
		f_Project(&st_A, &ax, &ay);
		f_Project(&st_B, &bx, &by);
		(VOID)st_Graphics->DrawLine(&st_GridPen, static_cast<FLOAT32>(ax), static_cast<FLOAT32>(ay), static_cast<FLOAT32>(bx), static_cast<FLOAT32>(by));
	}

	for (value = north0; value <= (north1 + (0.5 * step)); value = value + step)
	{
		st_A.east	= east0;
		st_A.north	= value;
		st_B.east	= east1;
		st_B.north	= value;
		f_Project(&st_A, &ax, &ay);
		f_Project(&st_B, &bx, &by);
		(VOID)st_Graphics->DrawLine(&st_GridPen, static_cast<FLOAT32>(ax), static_cast<FLOAT32>(ay), static_cast<FLOAT32>(bx), static_cast<FLOAT32>(by));
	}

	// 방위 글자: 북쪽 끝, 동쪽 끝 가운데
	st_A.east	= 0.5 * (east0 + east1);
	st_A.north	= north1;
	f_Project(&st_A, &ax, &ay);
	f_Plot_LabelText(st_Graphics, st_Font, CString(_T("N")), ax, ay, Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentCenter, UI_COLOR_TEXT);
	st_A.east	= east1;
	st_A.north	= 0.5 * (north0 + north1);
	f_Project(&st_A, &ax, &ay);
	f_Plot_LabelText(st_Graphics, st_Font, CString(_T("E")), ax, ay, Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentCenter, UI_COLOR_TEXT);

	// 고도 기둥. 눈금은 실제 고도 [m]
	st_A.east	= 0.0;
	st_A.north	= 0.0;
	st_B		= st_A;
	st_B.up		= floor(maxUp / upStep) * upStep;
	f_Project(&st_A, &ax, &ay);
	f_Project(&st_B, &bx, &by);
	(VOID)st_Graphics->DrawLine(&st_AxisPen, static_cast<FLOAT32>(ax), static_cast<FLOAT32>(ay), static_cast<FLOAT32>(bx), static_cast<FLOAT32>(by));

	for (value = upStep; value <= (st_B.up + (0.5 * upStep)); value = value + upStep)
	{
		st_A.up = value;
		f_Project(&st_A, &ax, &ay);
		(VOID)st_Graphics->DrawLine(&st_AxisPen, static_cast<FLOAT32>(ax) - 3.0F, static_cast<FLOAT32>(ay), static_cast<FLOAT32>(ax) + 3.0F, static_cast<FLOAT32>(ay));
		st_Text.Format(_T("%.0f m"), value);
		f_Plot_Text(st_Graphics, st_Font, st_Text, ax - 6.0, ay, Gdiplus::StringAlignmentFar, Gdiplus::StringAlignmentCenter, UI_COLOR_TEXT_SUB);
	}
}

// 장면: 바닥, 참값 궤적 바닥 그림자, 지난 스캔 플롯 (옅게), 참값 궤적 (지금 스캔 뒤는 옅게), 플랫폼, 지금 스캔의 받은 플롯 · 참값 위치 (바닥까지 점선) · 트랙, 범례.
VOID CTrackView3D::f_DrawScene(Gdiplus::Graphics *st_Graphics) const
{
	static const Gdiplus::REAL		s_Dash[2] = { 3.0F, 3.0F };
	const FLOAT32					scale = static_cast<FLOAT32>(f_Ui_Scale(100, dpi)) / 100.0F;
	const Gdiplus::FontFamily		st_Family(L"Malgun Gothic");
	const Gdiplus::Font				st_Font(&st_Family, scale * PLOT_FONT_PX, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	const Gdiplus::Font				st_Bold(&st_Family, scale * PLOT_FONT_PX, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
	const INT32						nSampleNum = st_Result->f_GetSampleNum();
	const INT32						nIndex = st_Track->f_FindScan(nCurScan);
	const INT32						nScanEnd = std::min(nCurScan + 1, st_Measure->f_GetScanNum());
	const INT32						nNow = std::min(std::max(nCurScan, 0), nSampleNum - 1);		// 지금 스캔의 표본
	const FLOAT32					plotRadius = PLOT_PLOT_RADIUS * scale;
	const FLOAT32					clutterRadius = PLOT_CLUTTER_RADIUS * scale;
	std::vector<Gdiplus::PointF>	st_Line(static_cast<UINT64>(nSampleNum));
	std::vector<Gdiplus::RectF>		st_Dot;
	Gdiplus::Pen					st_DropPen(f_Plot_Color(UI_COLOR_TEXT_OFF, 255), 1.0F);
	ST_LegendRow					st_Row[PLOT_LEG_MAX];
	ST_PlotPoint					st_Ground;
	const ST_TrackItem				*st_Item;
	const ST_RecvPlot				*st_Plot;
	FLOAT64							x = 0.0;
	FLOAT64							y = 0.0;
	FLOAT64							gx = 0.0;
	FLOAT64							gy = 0.0;
	FLOAT64							leadX = 0.0;
	FLOAT64							leadY = 0.0;
	CString							st_Title;
	INT32							nObject;
	INT32							nStep;
	INT32							nScan;
	INT32							nItem;
	INT32							nRowNum;

	(VOID)st_DropPen.SetDashPattern(s_Dash, 2);
	(VOID)st_Graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

	st_Title.Format(_T("3D (동-북-고도, 고도 %.0f배, 바닥 격자 %g km)"), upScale, gridStep / 1000.0);
	f_Plot_Text(st_Graphics, &st_Bold, st_Title, static_cast<FLOAT64>(st_Area.left), static_cast<FLOAT64>(st_Area.top) - 5.0,
		Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentFar, UI_COLOR_TEXT);
	f_Plot_Text(st_Graphics, &st_Font, CString(_T("끌어서 회전, 휠로 확대, 두 번 클릭하면 처음 보기")), static_cast<FLOAT64>(st_Area.left),
		static_cast<FLOAT64>(st_Area.bottom), Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentFar, UI_COLOR_TEXT_OFF);

	f_DrawGround(st_Graphics, &st_Font);

	// 참값 궤적의 바닥 그림자 (지금 스캔까지, 회색). 궤적과 그림자 사이가 고도. 표적 색으로 옅게 그리는 앞으로 갈 길과 헷갈리지 않게 회색
	for (nObject = 1; (isShowTrue != 0) && (nNow > 0) && (nObject < st_Result->f_GetObjectNum()); nObject++)
	{
		Gdiplus::Pen st_ShadowPen(f_Plot_Color(UI_COLOR_TEXT_OFF, TRK3D_SHADOW_ALPHA), 1.2F * scale);

		for (nStep = 0; nStep <= nNow; nStep++)
		{
			st_Ground		= *st_Result->f_GetPoint(nStep, nObject);
			st_Ground.up	= 0.0;
			f_Project(&st_Ground, &x, &y);
			st_Line[static_cast<UINT64>(nStep)] = Gdiplus::PointF(static_cast<FLOAT32>(x), static_cast<FLOAT32>(y));
		}

		(VOID)st_Graphics->DrawLines(&st_ShadowPen, st_Line.data(), nNow + 1);
	}

	// 지난 스캔 플롯, 클러터 (옅게). 클러터는 상자 안 것만
	for (nScan = 0; (isShowClutter != 0) && (nScan < nScanEnd); nScan++)
	{
		for (nItem = 0; nItem < st_Measure->f_GetScan(nScan)->nClutterNum; nItem++)
		{
			if (f_IsInBox(st_Measure->f_GetClutterPoint(nScan, nItem)) != 0)
			{
				f_Project(st_Measure->f_GetClutterPoint(nScan, nItem), &x, &y);
				st_Dot.push_back(Gdiplus::RectF(static_cast<FLOAT32>(x) - clutterRadius, static_cast<FLOAT32>(y) - clutterRadius, 2.0F * clutterRadius,
					2.0F * clutterRadius));
			}
		}
	}

	f_Plot_FillDots(st_Graphics, PLOT_COLOR_CLUTTER, st_Dot);
	st_Dot.clear();

	for (nScan = 0; (isShowPlot != 0) && (nScan < nScanEnd); nScan++)
	{
		for (nItem = 0; nItem < st_Measure->f_GetTargetNum(); nItem++)
		{
			f_Project(st_Measure->f_GetPlotPoint(nScan, nItem), &x, &y);
			st_Dot.push_back(Gdiplus::RectF(static_cast<FLOAT32>(x) - plotRadius, static_cast<FLOAT32>(y) - plotRadius, 2.0F * plotRadius, 2.0F * plotRadius));
		}
	}

	f_Plot_FillDots(st_Graphics, PLOT_COLOR_PLOT, st_Dot);

	// 참값 궤적. 지금 스캔까지는 진하게, 그 뒤 (앞으로 갈 길) 는 옅게
	for (nObject = 0; (isShowTrue != 0) && (nObject < st_Result->f_GetObjectNum()); nObject++)
	{
		Gdiplus::Pen st_TruthPen(f_Plot_Color(f_Ui_ObjectColor(nObject), 255), 1.8F * scale);
		Gdiplus::Pen st_FuturePen(f_Plot_Color(f_Ui_ObjectColor(nObject), PLOT_FUTURE_ALPHA), 1.2F * scale);

		(VOID)st_TruthPen.SetLineJoin(Gdiplus::LineJoinRound);
		(VOID)st_FuturePen.SetLineJoin(Gdiplus::LineJoinRound);

		for (nStep = 0; nStep < nSampleNum; nStep++)
		{
			f_Project(st_Result->f_GetPoint(nStep, nObject), &x, &y);
			st_Line[static_cast<UINT64>(nStep)] = Gdiplus::PointF(static_cast<FLOAT32>(x), static_cast<FLOAT32>(y));
		}

		if (nNow < (nSampleNum - 1))
		{
			(VOID)st_Graphics->DrawLines(&st_FuturePen, st_Line.data() + nNow, nSampleNum - nNow);
		}

		if (nNow > 0)
		{
			(VOID)st_Graphics->DrawLines(&st_TruthPen, st_Line.data(), nNow + 1);
		}
	}

	// 플랫폼 (레이다)
	{
		const FLOAT32		radius = 5.0F * scale;
		Gdiplus::SolidBrush	st_Fill(f_Plot_Color(UI_COLOR_CARD, 255));
		Gdiplus::Pen		st_Ring(f_Plot_Color(f_Ui_ObjectColor(0), 255), 2.0F * scale);

		f_Project(st_Result->f_GetPoint(std::min(nCurScan, nSampleNum - 1), 0), &x, &y);
		(VOID)st_Graphics->FillEllipse(&st_Fill, static_cast<FLOAT32>(x) - radius, static_cast<FLOAT32>(y) - radius, 2.0F * radius, 2.0F * radius);
		(VOID)st_Graphics->DrawEllipse(&st_Ring, static_cast<FLOAT32>(x) - radius, static_cast<FLOAT32>(y) - radius, 2.0F * radius, 2.0F * radius);
	}

	// 지금 스캔에 받은 플롯
	for (nItem = 0; (nIndex >= 0) && (nItem < st_Track->f_GetPlotNum(nIndex)); nItem++)
	{
		st_Plot = st_Track->f_GetPlot(nIndex, nItem);

		if (((st_Plot->nSource == 0) && (isShowClutter != 0) && (f_IsInBox(&st_Plot->st_Point) != 0)) || ((st_Plot->nSource != 0) && (isShowPlot != 0)))
		{
			f_Project(&st_Plot->st_Point, &x, &y);
			f_Plot_ScanDot(st_Graphics, x, y, st_Plot->nSource, scale);
		}
	}

	// 지금 스캔의 참값 위치. 바닥까지 점선을 내려 높이를 읽게 함
	for (nObject = 1; (isShowTrue != 0) && (nObject < st_Result->f_GetObjectNum()) && (nCurScan < nSampleNum); nObject++)
	{
		st_Ground		= *st_Result->f_GetPoint(nCurScan, nObject);
		f_Project(&st_Ground, &x, &y);
		st_Ground.up	= 0.0;
		f_Project(&st_Ground, &gx, &gy);
		(VOID)st_Graphics->DrawLine(&st_DropPen, static_cast<FLOAT32>(x), static_cast<FLOAT32>(y), static_cast<FLOAT32>(gx), static_cast<FLOAT32>(gy));
		f_Plot_TrueMark(st_Graphics, x, y, nObject, scale);
	}

	// 트랙
	for (nItem = 0; (nIndex >= 0) && (isShowTrack != 0) && (nItem < st_Track->f_GetTrackNum(nIndex)); nItem++)
	{
		st_Item = st_Track->f_GetTrack(nIndex, nItem);
		f_Project(&st_Item->st_Point, &x, &y);
		f_Project(&st_Item->st_Lead, &leadX, &leadY);
		f_Plot_TrackSymbol(st_Graphics, &st_Font, &st_Item->st_View, x, y, leadX, leadY, scale);
	}

	nRowNum = f_Plot_LegendRows(st_Row, isShowTrue, isShowPlot, isShowClutter, isShowTrack);

	if ((nRowNum > 0) && (isShowLegend != 0))
	{
		f_Plot_Legend(st_Graphics, &st_Font, st_Row, nRowNum, st_Area, 0, scale);
	}
}
