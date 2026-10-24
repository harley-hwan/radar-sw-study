// 결과 그림 (평면, 3D, 2D 단면, 추적 성능) 구현 (GDI+)
#include "pch.h"

#include <math.h>

#include <algorithm>
#include <vector>
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

// 공통
#define PLOT_FONT_PX			12.0F
#define PLOT_COLOR_PLOT			RGB(31, 41, 55)			// 플롯 점. 참값 선과 겹쳐도 보이게 검정에 가깝게
#define PLOT_PLOT_RADIUS		1.6F					// [px @96dpi] 지난 스캔 플롯
#define PLOT_HISTORY_ALPHA		110						// 지난 스캔 플롯의 불투명도
#define PLOT_FUTURE_ALPHA		60						// 지금 스캔 뒤 참값 궤적 (앞으로 갈 길) 의 불투명도
#define PLOT_SCAN_RADIUS		3.2F					// [px @96dpi] 지금 스캔에 받은 플롯
#define PLOT_TRUE_RADIUS		4.0F					// [px @96dpi] 지금 스캔의 참값 위치
#define PLOT_TRACK_HALF			6.0F					// [px @96dpi] 지금 트랙 사각형 반 변
#define PLOT_TRACK_DOT_HALF		2.2F					// [px @96dpi] 트랙 이력 사각형 반 변
#define PLOT_TRACK_GAP			9.0						// [px @96dpi] 트랙 이력 사각형 사이 최소 간격
#define PLOT_TRACK_COLOR_NUM	6
#define PLOT_LEGEND_PAD			8.0F					// [px @96dpi]
#define PLOT_ZOOM_MAX			200.0
#define PLOT_ZOOM_STEP			1.25					// 휠 한 칸 배율

// 평면 그림
#define PLOT_MARGIN_LEFT		74						// [px @96dpi] 위도 라벨 자리
#define PLOT_MARGIN_RIGHT		16
#define PLOT_TITLE_HEIGHT		26
#define PLOT_AXIS_HEIGHT		42						// [px @96dpi] 경도 눈금 + 축 이름 두 줄
#define PLOT_MIN_SPAN			1000.0					// [m]
#define PLOT_SPAN_SCALE			1.18
#define PLOT_MAX_GRID_NUM		8.0
#define PLOT_LON_LABEL_PX		78						// [px @96dpi] 경도 라벨 하나의 폭
#define PLOT_LAT_LABEL_PX		34						// [px @96dpi] 위도 라벨 하나의 높이
#define PLOT_LABEL_GAP_PX		8.0						// [px @96dpi] 시작점과 이름표 사이
#define PLOT_LABEL_ROOM_X_PX	56.0					// [px @96dpi] 가장자리 시작점의 이름표 자리
#define PLOT_LABEL_ROOM_Y_PX	26.0
#define PLOT_LABEL_SIDE_MIN		0.3

// 3D 그림
#define TRK3D_UP_SCALE_MAX		5.0						// 고도를 늘려 그리는 최대 배율
#define TRK3D_UP_RATIO			0.35					// 늘린 고도가 바닥 폭의 이 비율쯤 되게 배율을 고름
#define TRK3D_YAW_DEG			(-25.0)					// 처음 보기 방위 [deg]
#define TRK3D_PITCH_DEG			30.0					// 처음 보기 내려다보는 각 [deg]
#define TRK3D_TITLE_HEIGHT		26						// [px @96dpi]
#define TRK3D_MARGIN			16
#define TRK3D_FIT_SCALE			1.16					// 장면 둘레 여백 비율 (눈금 글자 자리)
#define TRK3D_DRAG_DEG_PER_PX	0.4						// 끌기 1 px 당 회전 [deg]
#define TRK3D_MIN_SPAN			2000.0					// [m]
#define TRK3D_MIN_UP			1000.0					// [m] 고도 축 최소 길이
#define TRK3D_GRID_NUM			6.0						// 바닥 격자 선 수 (축마다 대략)
#define TRK3D_SHADOW_ALPHA		170						// 참값 궤적 바닥 그림자 (회색) 의 불투명도
#define TRK3D_SHADOW_MIN_UP		50.0					// [m] 이보다 높이 나는 표적만 그림자

// 2D 단면 그림
#define PROF_MARGIN_LEFT		64						// [px @96dpi] 고도 라벨 자리
#define PROF_MARGIN_RIGHT		14
#define PROF_TITLE_HEIGHT		24
#define PROF_AXIS_HEIGHT		32
#define PROF_MIN_ACROSS			500.0					// [m] 가로 최소 폭
#define PROF_MIN_UP				200.0					// [m] 세로 최소 폭
#define PROF_PAD_RATIO			0.08					// 데이터 범위 둘레 여백 비율
#define PROF_TICK_X_PX			92.0					// [px @96dpi] 가로 눈금 하나의 폭
#define PROF_TICK_Y_PX			34.0

// 추적 성능 그림
#define PERF_MARGIN_LEFT		64
#define PERF_MARGIN_RIGHT		16
#define PERF_TITLE_HEIGHT		26
#define PERF_AXIS_HEIGHT		30
#define PERF_GAP				14						// 두 그래프 사이
#define PERF_TICK_X_PX			64.0
#define PERF_NIS_LOG_MIN		(-2.0)					// NIS 축 아래 끝 10^-2
#define PERF_NIS_LOG_FLOOR		2.0						// NIS 축 위 끝은 적어도 10^2
#define PERF_COLOR_GATE			RGB(220, 38, 38)
#define PERF_COLOR_DIVERGE		RGB(254, 226, 226)		// 발산 판정 띠
#define PERF_COLOR_TURN			RGB(233, 236, 241)		// 선회 띠

// 범례 줄 종류
#define PLOT_LEG_LINE			0						// 참값 궤적
#define PLOT_LEG_DOT			1						// 플롯
#define PLOT_LEG_TRACKLINE		2						// 트랙 이력 선
#define PLOT_LEG_TRACKS			3						// 지금 트랙 상태 네 가지
#define PLOT_LEG_MAX			4

typedef struct
{
	INT32		nKind;
	COLORREF	color;
	CString		st_Name;
} ST_LegendRow;

// 트랙 이력 선 하나: 같은 트랙 (개시 스캔 + ID) 의 위치를 받은 스캔 순서로 이은 픽셀
typedef struct
{
	UINT32							TrackId;
	INT32							nInitScan;
	COLORREF						color;
	std::vector<Gdiplus::PointF>	st_Pixel;
} ST_TrackLine;

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

// {1, 2, 5} x 10^k. 고도, 오차, 시간 눈금용.
static FLOAT64 f_Plot_NiceStepMeter(FLOAT64 span, FLOAT64 maxTickNum)
{
	static const FLOAT64 s_Mantissa[3] = { 1.0, 2.0, 5.0 };

	return f_Plot_NiceStepFrom(span, maxTickNum, s_Mantissa, 3);
}

// 눈금 간격에 맞는 소수 자릿수.
static INT32 f_Plot_DecimalFor(FLOAT64 step)
{
	return (step < 1.0) ? static_cast<INT32>(ceil(-log10(step) - 1.0e-9)) : 0;
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

// 기준점 정렬 글자를 옅은 흰 바탕 위에 그리기. 점, 선이 깔린 곳에서도 읽히게 함.
static VOID f_Plot_LabelText(Gdiplus::Graphics *st_Graphics, const Gdiplus::Font *st_Font, const CString &st_Text, FLOAT64 x, FLOAT64 y,
	Gdiplus::StringAlignment alignX, Gdiplus::StringAlignment alignY, COLORREF color)
{
	const Gdiplus::PointF	st_Zero(0.0F, 0.0F);
	Gdiplus::RectF			st_Bound;
	Gdiplus::SolidBrush		st_Back(f_Plot_Color(UI_COLOR_CARD, 215));
	FLOAT32					left;
	FLOAT32					top;

	// 글자 크기를 재서 정렬에 맞춘 위치에 바탕을 깖
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

// 객체 이름 ("레이다" 또는 "표적").
static CString f_Plot_ObjectName(INT32 nObject)
{
	return CString((nObject == 0) ? _T("레이다") : _T("표적"));
}

// 그림 제목: 머리 + 보이는 것 (참값, 플롯, 트랙)
static CString f_Plot_Title(LPCTSTR pt_Head, INT32 isTrue, INT32 isPlot, INT32 isTrack)
{
	static const LPCTSTR	s_Part[3] = { _T("참값"), _T("플롯"), _T("트랙") };
	const INT32				isShown[3] = { isTrue, isPlot, isTrack };
	CString					st_Title(pt_Head);
	INT32					isFirst = 1;
	INT32					nPart;

	for (nPart = 0; nPart < 3; nPart++)
	{
		if (isShown[nPart] != 0)
		{
			st_Title += (isFirst != 0) ? _T("  ·  ") : _T(", ");
			st_Title += s_Part[nPart];
			isFirst = 0;
		}
	}

	return st_Title;
}

// 트랙 색 (ID 가 나온 순서). 참값 (표적 색 파랑) 과 겹치지 않는 색
static COLORREF f_Plot_TrackColor(INT32 nColor)
{
	static const COLORREF s_Color[PLOT_TRACK_COLOR_NUM] =
	{
		RGB(22, 163, 74), RGB(234, 88, 12), RGB(147, 51, 234), RGB(219, 39, 119), RGB(13, 148, 136), RGB(202, 138, 4)
	};

	return s_Color[((nColor >= 0) ? nColor : 0) % PLOT_TRACK_COLOR_NUM];
}

// 트랙 사각형 (지금 스캔): 임시 빈 칸, 확정 채움, 메모리 점선, 삭제 X
static VOID f_Plot_TrackBox(Gdiplus::Graphics *st_Graphics, FLOAT32 x, FLOAT32 y, FLOAT32 half, INT32 nState, COLORREF color, FLOAT32 scale)
{
	static const Gdiplus::REAL	s_Dash[2] = { 2.0F, 1.5F };
	const Gdiplus::Color		st_Color = f_Plot_Color(color, 255);
	Gdiplus::Pen				st_Pen(st_Color, 2.0F * scale);
	Gdiplus::Pen				st_Ring(f_Plot_Color(UI_COLOR_CARD, 255), 1.0F);
	Gdiplus::SolidBrush			st_Fill(f_Plot_Color(UI_COLOR_CARD, 200));

	if (nState == TRK_STATE_CONFIRMED)
	{
		Gdiplus::SolidBrush st_Solid(st_Color);

		(VOID)st_Graphics->FillRectangle(&st_Solid, x - half, y - half, 2.0F * half, 2.0F * half);
		(VOID)st_Graphics->DrawRectangle(&st_Ring, x - half, y - half, 2.0F * half, 2.0F * half);
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

// 지금 트랙 심볼: 속도 선 (TRK_LEAD_TIME 동안 가는 거리), 상태 사각형, ID 글자.
static VOID f_Plot_TrackSymbol(Gdiplus::Graphics *st_Graphics, const Gdiplus::Font *st_Font, const ST_TrkView *st_View, FLOAT64 x, FLOAT64 y,
	FLOAT64 leadX, FLOAT64 leadY, COLORREF color, FLOAT32 scale)
{
	const FLOAT32	half = PLOT_TRACK_HALF * scale;
	Gdiplus::Pen	st_Lead(f_Plot_Color(color, 220), 1.6F * scale);
	CString			st_Id;

	(VOID)st_Graphics->DrawLine(&st_Lead, static_cast<FLOAT32>(x), static_cast<FLOAT32>(y), static_cast<FLOAT32>(leadX), static_cast<FLOAT32>(leadY));
	f_Plot_TrackBox(st_Graphics, static_cast<FLOAT32>(x), static_cast<FLOAT32>(y), half, st_View->TrackState, color, scale);

	st_Id.Format(_T("%u"), st_View->TrackId);
	f_Plot_LabelText(st_Graphics, st_Font, st_Id, x + static_cast<FLOAT64>(half) + 2.0, y - static_cast<FLOAT64>(half), Gdiplus::StringAlignmentNear,
		Gdiplus::StringAlignmentFar, color);
}

// 지난 스캔 점들을 작은 사각형으로 한 번에 그림 (천 개가 넘어 원으로 그리면 느림)
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

// 지금 스캔에 받은 플롯: 흰 테를 두른 점.
static VOID f_Plot_ScanDot(Gdiplus::Graphics *st_Graphics, FLOAT64 x, FLOAT64 y, FLOAT32 scale)
{
	const FLOAT32		radius = PLOT_SCAN_RADIUS * scale;
	Gdiplus::SolidBrush	st_Dot(f_Plot_Color(PLOT_COLOR_PLOT, 255));
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

// 받은 스캔 처음부터 nLastIndex 까지 트랙 위치를 트랙별 픽셀 선으로 모음. pf_Project: 그림 좌표 -> 픽셀
template <typename T_PROJECT>
static VOID f_Plot_CollectTrackLines(const CTrackResult *st_Track, INT32 nLastIndex, T_PROJECT pf_Project, std::vector<ST_TrackLine> *st_Lines)
{
	const ST_TrackItem	*st_Item;
	ST_TrackLine		st_New;
	FLOAT64				x = 0.0;
	FLOAT64				y = 0.0;
	INT32				nIndex;
	INT32				nTrack;
	INT32				nLine;
	INT32				nFound;

	st_Lines->clear();

	for (nIndex = 0; nIndex <= nLastIndex; nIndex++)
	{
		for (nTrack = 0; nTrack < st_Track->f_GetTrackNum(nIndex); nTrack++)
		{
			st_Item	= st_Track->f_GetTrack(nIndex, nTrack);
			nFound	= -1;

			for (nLine = 0; nLine < static_cast<INT32>(st_Lines->size()); nLine++)
			{
				const ST_TrackLine *st_Line = &(*st_Lines)[static_cast<UINT64>(nLine)];

				nFound = ((st_Line->TrackId == st_Item->st_View.TrackId) && (st_Line->nInitScan == st_Item->st_View.nInitScan)) ? nLine : nFound;
			}

			if (nFound < 0)
			{
				st_New.TrackId		= st_Item->st_View.TrackId;
				st_New.nInitScan	= st_Item->st_View.nInitScan;
				st_New.color		= f_Plot_TrackColor(st_Track->f_GetColorIndex(st_New.TrackId));
				st_Lines->push_back(st_New);
				nFound				= static_cast<INT32>(st_Lines->size()) - 1;
			}

			pf_Project(&st_Item->st_Point, &x, &y);
			(*st_Lines)[static_cast<UINT64>(nFound)].st_Pixel.push_back(Gdiplus::PointF(static_cast<FLOAT32>(x), static_cast<FLOAT32>(y)));
		}
	}
}

// 트랙 이력: 트랙별 색 선 + 작은 사각형 (앞 사각형에서 PLOT_TRACK_GAP 이상 떨어질 때마다). 같은 ID 끼리만 이어짐
static VOID f_Plot_DrawTrackLines(Gdiplus::Graphics *st_Graphics, const std::vector<ST_TrackLine> &st_Lines, FLOAT32 scale)
{
	const FLOAT32	half = PLOT_TRACK_DOT_HALF * scale;
	const FLOAT64	gap = PLOT_TRACK_GAP * static_cast<FLOAT64>(scale);
	Gdiplus::Pen	st_Ring(f_Plot_Color(UI_COLOR_CARD, 255), 1.0F);
	FLOAT64			lastX = 0.0;
	FLOAT64			lastY = 0.0;
	FLOAT64			dx;
	FLOAT64			dy;
	UINT64			nLine;
	UINT64			nPoint;

	for (nLine = 0; nLine < st_Lines.size(); nLine++)
	{
		const std::vector<Gdiplus::PointF>	&st_Pixel = st_Lines[nLine].st_Pixel;
		Gdiplus::Pen						st_LinePen(f_Plot_Color(st_Lines[nLine].color, 230), 1.6F * scale);
		Gdiplus::SolidBrush					st_Mark(f_Plot_Color(st_Lines[nLine].color, 255));

		(VOID)st_LinePen.SetLineJoin(Gdiplus::LineJoinRound);

		if (st_Pixel.size() >= 2U)
		{
			(VOID)st_Graphics->DrawLines(&st_LinePen, st_Pixel.data(), static_cast<INT32>(st_Pixel.size()));
		}

		for (nPoint = 0; nPoint < st_Pixel.size(); nPoint++)
		{
			dx = static_cast<FLOAT64>(st_Pixel[nPoint].X) - lastX;
			dy = static_cast<FLOAT64>(st_Pixel[nPoint].Y) - lastY;

			if ((nPoint == 0U) || (((dx * dx) + (dy * dy)) >= (gap * gap)))
			{
				(VOID)st_Graphics->FillRectangle(&st_Mark, st_Pixel[nPoint].X - half, st_Pixel[nPoint].Y - half, 2.0F * half, 2.0F * half);
				(VOID)st_Graphics->DrawRectangle(&st_Ring, st_Pixel[nPoint].X - half, st_Pixel[nPoint].Y - half, 2.0F * half, 2.0F * half);
				lastX = static_cast<FLOAT64>(st_Pixel[nPoint].X);
				lastY = static_cast<FLOAT64>(st_Pixel[nPoint].Y);
			}
		}
	}
}

// 범례 (영역 오른쪽 위 안쪽, isBottom 이면 오른쪽 아래 안쪽).
static VOID f_Plot_Legend(Gdiplus::Graphics *st_Graphics, const Gdiplus::Font *st_Font, const ST_LegendRow *st_Row, INT32 nRowNum, const CRect &st_Area,
	INT32 isBottom, FLOAT32 scale)
{
	static const INT32		s_State[4] = { TRK_STATE_TENTATIVE, TRK_STATE_CONFIRMED, TRK_STATE_MEMORY, TRK_STATE_DELETED };
	static const LPCTSTR	s_StateName[4] = { _T("임시"), _T("확정"), _T("메모리"), _T("삭제") };
	const FLOAT32			pad = PLOT_LEGEND_PAD * scale;
	const FLOAT32			markWidth = 22.0F * scale;
	const FLOAT32			half = 0.8F * PLOT_TRACK_HALF * scale;
	const FLOAT32			dotHalf = PLOT_TRACK_DOT_HALF * scale;
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

	// 줄 폭: 기호 + 이름. 지금 트랙 줄은 이름 뒤에 상태 네 가지
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
		Gdiplus::Pen		st_Line(f_Plot_Color(st_Row[nRow].color, 255), 2.0F * scale);
		Gdiplus::SolidBrush	st_Mark(f_Plot_Color(st_Row[nRow].color, 255));

		x = left + pad;

		if (st_Row[nRow].nKind == PLOT_LEG_TRACKS)
		{
			// 이름을 앞에 쓰고 상태 네 가지를 뒤에 늘어놓음
			f_Plot_Text(st_Graphics, st_Font, st_Row[nRow].st_Name, static_cast<FLOAT64>(x), static_cast<FLOAT64>(y), Gdiplus::StringAlignmentNear,
				Gdiplus::StringAlignmentCenter, UI_COLOR_TEXT);
			(VOID)st_Graphics->MeasureString(st_Row[nRow].st_Name.GetString(), -1, st_Font, st_Zero, &st_Bound);
			x = x + st_Bound.Width + (0.5F * pad);

			for (nState = 0; nState < 4; nState++)
			{
				f_Plot_TrackBox(st_Graphics, x + half, y, half, s_State[nState], st_Row[nRow].color, scale);
				x = x + (2.0F * half) + (0.3F * pad);
				f_Plot_Text(st_Graphics, st_Font, CString(s_StateName[nState]), static_cast<FLOAT64>(x), static_cast<FLOAT64>(y), Gdiplus::StringAlignmentNear,
					Gdiplus::StringAlignmentCenter, UI_COLOR_TEXT_SUB);
				x = x + stateWidth[nState] + (0.8F * pad);
			}
		}
		else
		{
			if (st_Row[nRow].nKind == PLOT_LEG_DOT)
			{
				(VOID)st_Graphics->FillEllipse(&st_Mark, x + (0.5F * markWidth) - (1.5F * scale), y - (1.5F * scale), 3.0F * scale, 3.0F * scale);
			}
			else
			{
				(VOID)st_Graphics->DrawLine(&st_Line, x, y, x + markWidth, y);

				if (st_Row[nRow].nKind == PLOT_LEG_TRACKLINE)
				{
					(VOID)st_Graphics->FillRectangle(&st_Mark, x + (0.2F * markWidth) - dotHalf, y - dotHalf, 2.0F * dotHalf, 2.0F * dotHalf);
					(VOID)st_Graphics->FillRectangle(&st_Mark, x + (0.8F * markWidth) - dotHalf, y - dotHalf, 2.0F * dotHalf, 2.0F * dotHalf);
				}
			}

			f_Plot_Text(st_Graphics, st_Font, st_Row[nRow].st_Name, static_cast<FLOAT64>(x + markWidth + (0.5F * pad)), static_cast<FLOAT64>(y),
				Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter, UI_COLOR_TEXT);
		}

		y = y + rowHeight;
	}
}

// 보이는 것에 맞춘 범례 줄. 줄 수 반환.
static INT32 f_Plot_LegendRows(ST_LegendRow *st_Row, INT32 isShowTrue, INT32 isShowPlot, INT32 isShowTrack)
{
	INT32 nRowNum = 0;

	if (isShowTrue != 0)
	{
		st_Row[nRowNum].nKind	= PLOT_LEG_LINE;
		st_Row[nRowNum].color	= f_Ui_ObjectColor(1);
		st_Row[nRowNum].st_Name	= _T("참값 궤적, 지금 위치 (마름모)");
		nRowNum++;
	}

	if (isShowPlot != 0)
	{
		st_Row[nRowNum].nKind	= PLOT_LEG_DOT;
		st_Row[nRowNum].color	= PLOT_COLOR_PLOT;
		st_Row[nRowNum].st_Name	= _T("플롯 (큰 점은 지금 스캔)");
		nRowNum++;
	}

	if (isShowTrack != 0)
	{
		st_Row[nRowNum].nKind	= PLOT_LEG_TRACKLINE;
		st_Row[nRowNum].color	= f_Plot_TrackColor(0);
		st_Row[nRowNum].st_Name	= _T("트랙 (같은 ID 를 선으로 이음, 색 = ID)");
		nRowNum++;
		st_Row[nRowNum].nKind	= PLOT_LEG_TRACKS;
		st_Row[nRowNum].color	= f_Plot_TrackColor(0);
		st_Row[nRowNum].st_Name	= _T("지금 트랙");
		nRowNum++;
	}

	return nRowNum;
}

// CTrajectoryPlot

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

// 평면 그림. 결과 버퍼는 대화상자 소유, 여기서는 읽기만.
CTrajectoryPlot::CTrajectoryPlot() noexcept
	: st_Result(nullptr)
	, st_Measure(nullptr)
	, st_Track(nullptr)
	, isShowTrue(1)
	, isShowPlot(1)
	, isShowTrack(1)
	, nCurScan(0)
	, dpi(UI_BASE_DPI)
	, pixelPerMeter(1.0)
	, centerEast(0.0)
	, centerNorth(0.0)
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

// 그릴 결과 교체 후 처음 보기로 다시 그리기
VOID CTrajectoryPlot::f_SetData(const CSimResult *st_NewResult, const CPlotResult *st_NewMeasure, const CTrackResult *st_NewTrack)
{
	st_Result	= st_NewResult;
	st_Measure	= st_NewMeasure;
	st_Track	= st_NewTrack;
	zoom		= 1.0;
	panEast		= 0.0;
	panNorth	= 0.0;

	Invalidate(FALSE);
}

// 표시 여부 (참값, 플롯, 트랙) 교체 후 다시 그리기.
VOID CTrajectoryPlot::f_SetShow(INT32 isNewShowTrue, INT32 isNewShowPlot, INT32 isNewShowTrack)
{
	isShowTrue	= isNewShowTrue;
	isShowPlot	= isNewShowPlot;
	isShowTrack	= isNewShowTrack;

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

	if ((st_Result != nullptr) && (st_Measure != nullptr) && (st_Track != nullptr) && (st_Result->f_GetSampleNum() > 0) && (st_Client.Width() > 0)
		&& (st_Client.Height() > 0))
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

// 축척, 중심, 격자 간격 (켜고 끄거나 스캔을 바꿔도 축척 유지). 레이다, 표적 궤적, 보이는 플롯 · 트랙이 다 들어감
VOID CTrajectoryPlot::f_ComputeView(VOID)
{
	const ST_GeoMap	*st_Map = st_Result->f_GetMap();
	const FLOAT64	width = static_cast<FLOAT64>(st_MapArea.Width());
	const FLOAT64	height = static_cast<FLOAT64>(st_MapArea.Height());
	const FLOAT64	scale = static_cast<FLOAT64>(f_Ui_Scale(100, dpi)) / 100.0;
	FLOAT64			minEast = 0.0;
	FLOAT64			maxEast = 0.0;
	FLOAT64			minNorth = 0.0;
	FLOAT64			maxNorth = 0.0;
	FLOAT64			spanEast;
	FLOAT64			spanNorth;
	FLOAT64			halfEast;
	FLOAT64			halfNorth;
	INT32			nStep;
	INT32			nObject;
	INT32			nScan;
	INT32			nIndex;

	for (nStep = 0; nStep < st_Result->f_GetSampleNum(); nStep++)
	{
		for (nObject = 0; nObject < st_Result->f_GetObjectNum(); nObject++)
		{
			f_Plot_Include(st_Result->f_GetPoint(nStep, nObject), &minEast, &maxEast, &minNorth, &maxNorth);
		}
	}

	for (nScan = 0; (isShowPlot != 0) && (nScan < st_Measure->f_GetScanNum()); nScan++)
	{
		for (nIndex = 0; nIndex < st_Measure->f_GetTargetNum(); nIndex++)
		{
			f_Plot_Include(st_Measure->f_GetPlotPoint(nScan, nIndex), &minEast, &maxEast, &minNorth, &maxNorth);
		}
	}

	for (nScan = 0; (isShowTrack != 0) && (nScan < st_Track->f_GetScanNum()); nScan++)
	{
		for (nIndex = 0; nIndex < st_Track->f_GetTrackNum(nScan); nIndex++)
		{
			f_Plot_Include(&st_Track->f_GetTrack(nScan, nIndex)->st_Point, &minEast, &maxEast, &minNorth, &maxNorth);
		}
	}

	// 등축척. 여백은 이름표가 잘리지 않을 만큼
	spanEast		= fmax(maxEast - minEast, PLOT_MIN_SPAN);
	spanNorth		= fmax(maxNorth - minNorth, PLOT_MIN_SPAN);
	pixelPerMeter	= fmin(fmin(width / (spanEast * PLOT_SPAN_SCALE), fmax(width - (2.0 * PLOT_LABEL_ROOM_X_PX * scale), 1.0) / spanEast),
						fmin(height / (spanNorth * PLOT_SPAN_SCALE), fmax(height - (2.0 * PLOT_LABEL_ROOM_Y_PX * scale), 1.0) / spanNorth));
	pixelPerMeter	= pixelPerMeter * zoom;
	centerEast		= (0.5 * (minEast + maxEast)) + panEast;
	centerNorth		= (0.5 * (minNorth + maxNorth)) + panNorth;

	// 위경도 격자 간격 (축별). 그림 좌표가 위경도에 선형이라 보이는 폭 / 1 도 길이 = 보이는 각도 폭
	halfEast	= (0.5 * width) / pixelPerMeter;
	halfNorth	= (0.5 * height) / pixelPerMeter;
	gridLonDeg	= f_Plot_NiceStepGeo((2.0 * halfEast) / st_Map->meterPerLon,
					fmax(2.0, fmin(PLOT_MAX_GRID_NUM, width / static_cast<FLOAT64>(f_Ui_Scale(PLOT_LON_LABEL_PX, dpi)))));
	gridLatDeg	= f_Plot_NiceStepGeo((2.0 * halfNorth) / st_Map->meterPerLat,
					fmax(2.0, fmin(PLOT_MAX_GRID_NUM, height / static_cast<FLOAT64>(f_Ui_Scale(PLOT_LAT_LABEL_PX, dpi)))));
}

// 그림 좌표 -> 평면 픽셀.
VOID CTrajectoryPlot::f_MapToPixel(const ST_PlotPoint *st_Point, FLOAT64 *pt_X, FLOAT64 *pt_Y) const
{
	*pt_X = static_cast<FLOAT64>(st_MapArea.left) + (0.5 * static_cast<FLOAT64>(st_MapArea.Width())) + ((st_Point->east - centerEast) * pixelPerMeter);
	*pt_Y = static_cast<FLOAT64>(st_MapArea.top) + (0.5 * static_cast<FLOAT64>(st_MapArea.Height())) - ((st_Point->north - centerNorth) * pixelPerMeter);
}

// 평면: 격자, 지난 플롯, 참값 궤적, 트랙 이력 선, 그 위에 지금 스캔의 플롯 · 참값 · 트랙
VOID CTrajectoryPlot::f_DrawMap(Gdiplus::Graphics *st_Graphics) const
{
	const FLOAT32				scale = static_cast<FLOAT32>(f_Ui_Scale(100, dpi)) / 100.0F;
	const Gdiplus::FontFamily	st_Family(L"Malgun Gothic");
	const Gdiplus::Font			st_Font(&st_Family, scale * PLOT_FONT_PX, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	const Gdiplus::Font			st_Bold(&st_Family, scale * PLOT_FONT_PX, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
	const ST_GeoMap				*st_Map = st_Result->f_GetMap();
	const INT32					nSampleNum = st_Result->f_GetSampleNum();
	const INT32					nNow = std::min(std::max(nCurScan, 0), nSampleNum - 1);		// 지금 스캔의 표본
	const INT32					nIndex = st_Track->f_FindScan(nCurScan);
	const INT32					nLastIndex = st_Track->f_FindLastUpTo(nCurScan);
	const INT32					nScanEnd = std::min(nCurScan + 1, st_Measure->f_GetScanNum());
	const FLOAT32				plotRadius = PLOT_PLOT_RADIUS * scale;
	const FLOAT64				halfEast = (0.5 * static_cast<FLOAT64>(st_MapArea.Width())) / pixelPerMeter;
	const FLOAT64				halfNorth = (0.5 * static_cast<FLOAT64>(st_MapArea.Height())) / pixelPerMeter;
	Gdiplus::Pen				st_GridPen(f_Plot_Color(UI_COLOR_GRID_LINE, 255), 1.0F);
	Gdiplus::Pen				st_FramePen(f_Plot_Color(UI_COLOR_BORDER, 255), 1.0F);
	const Gdiplus::Rect			st_Clip(st_MapArea.left, st_MapArea.top, st_MapArea.Width(), st_MapArea.Height());
	std::vector<Gdiplus::PointF>	st_Line(static_cast<UINT64>(nSampleNum));
	std::vector<Gdiplus::RectF>		st_Dot;
	std::vector<ST_TrackLine>		st_TrackLines;
	ST_PlotPoint				st_Point = {};
	ST_LegendRow				st_Row[PLOT_LEG_MAX];
	const ST_TrackItem			*st_Item;
	FLOAT64						x = 0.0;
	FLOAT64						y = 0.0;
	FLOAT64						leadX = 0.0;
	FLOAT64						leadY = 0.0;
	FLOAT64						firstLine;
	FLOAT64						lineNum;
	FLOAT64						degree;
	CString						st_Zoom;
	INT32						nLine;
	INT32						nObject;
	INT32						nScan;
	INT32						nItem;
	INT32						nSample;
	INT32						nRowNum;

	// 격자, 눈금은 선명하게, 궤적은 부드럽게.
	(VOID)st_Graphics->SetSmoothingMode(Gdiplus::SmoothingModeNone);

	// 세로선 = 경도. 보이는 좌우 끝 경도 사이의 눈금마다 긋기.
	firstLine	= ceil(f_Scn_EastToLon(st_Map, centerEast - halfEast) / gridLonDeg);
	lineNum		= floor(f_Scn_EastToLon(st_Map, centerEast + halfEast) / gridLonDeg) - firstLine;

	for (nLine = 0; static_cast<FLOAT64>(nLine) <= lineNum; nLine++)
	{
		degree			= (firstLine + static_cast<FLOAT64>(nLine)) * gridLonDeg;
		st_Point.east	= f_Scn_LonToEast(st_Map, degree);
		st_Point.north	= centerNorth;
		f_MapToPixel(&st_Point, &x, &y);
		(VOID)st_Graphics->DrawLine(&st_GridPen, f_Plot_Round(x), st_MapArea.top, f_Plot_Round(x), st_MapArea.bottom);
		f_Plot_Text(st_Graphics, &st_Font, f_Plot_Number(degree, f_Plot_DecimalFor(gridLonDeg)), x, static_cast<FLOAT64>(st_MapArea.bottom) + 4.0,
			Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentNear, UI_COLOR_TEXT_SUB);
	}

	// 가로선 = 위도
	firstLine	= ceil(f_Scn_NorthToLat(st_Map, centerNorth - halfNorth) / gridLatDeg);
	lineNum		= floor(f_Scn_NorthToLat(st_Map, centerNorth + halfNorth) / gridLatDeg) - firstLine;

	for (nLine = 0; static_cast<FLOAT64>(nLine) <= lineNum; nLine++)
	{
		degree			= (firstLine + static_cast<FLOAT64>(nLine)) * gridLatDeg;
		st_Point.east	= centerEast;
		st_Point.north	= f_Scn_LatToNorth(st_Map, degree);
		f_MapToPixel(&st_Point, &x, &y);
		(VOID)st_Graphics->DrawLine(&st_GridPen, st_MapArea.left, f_Plot_Round(y), st_MapArea.right, f_Plot_Round(y));
		f_Plot_Text(st_Graphics, &st_Font, f_Plot_Number(degree, f_Plot_DecimalFor(gridLatDeg)), static_cast<FLOAT64>(st_MapArea.left) - 6.0, y,
			Gdiplus::StringAlignmentFar, Gdiplus::StringAlignmentCenter, UI_COLOR_TEXT_SUB);
	}

	(VOID)st_Graphics->DrawRectangle(&st_FramePen, st_MapArea.left, st_MapArea.top, st_MapArea.Width(), st_MapArea.Height());

	f_Plot_Text(st_Graphics, &st_Bold, f_Plot_Title(_T("평면 궤적 (위도-경도)"), isShowTrue, isShowPlot, isShowTrack),
		static_cast<FLOAT64>(st_MapArea.left), static_cast<FLOAT64>(st_MapArea.top) - 5.0, Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentFar, UI_COLOR_TEXT);
	f_Plot_Text(st_Graphics, &st_Font, CString(_T("경도 [°]")), static_cast<FLOAT64>(st_MapArea.right), static_cast<FLOAT64>(st_MapArea.bottom) + 4.0 + static_cast<FLOAT64>(scale * PLOT_FONT_PX * 1.4F),
		Gdiplus::StringAlignmentFar, Gdiplus::StringAlignmentNear, UI_COLOR_TEXT_OFF);
	f_Plot_Text(st_Graphics, &st_Font, CString(_T("위도 [°]")), static_cast<FLOAT64>(st_MapArea.left) + 6.0, static_cast<FLOAT64>(st_MapArea.top) + 4.0,
		Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentNear, UI_COLOR_TEXT_OFF);

	// 확대 중이면 평면 왼쪽 위 안쪽 (위도 글자 아래) 에 배율
	if (zoom > 1.0)
	{
		st_Zoom.Format(_T("%.1f배, 두 번 클릭하면 처음 보기"), zoom);
		f_Plot_Text(st_Graphics, &st_Font, st_Zoom, static_cast<FLOAT64>(st_MapArea.left) + 6.0,
			static_cast<FLOAT64>(st_MapArea.top) + 6.0 + static_cast<FLOAT64>(scale * PLOT_FONT_PX * 1.4F), Gdiplus::StringAlignmentNear,
			Gdiplus::StringAlignmentNear, UI_COLOR_TEXT_SUB);
	}

	(VOID)st_Graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	(VOID)st_Graphics->SetClip(st_Clip);

	// 시작점 이름표를 먼저 깔아 흰 바탕이 다른 그림을 가리지 않게 함
	f_DrawStartLabels(st_Graphics, &st_Font);

	// 지난 스캔 플롯은 궤적 아래에 깔아 참값 선이 가려지지 않게 함
	for (nScan = 0; (isShowPlot != 0) && (nScan < nScanEnd); nScan++)
	{
		for (nItem = 0; nItem < st_Measure->f_GetTargetNum(); nItem++)
		{
			f_MapToPixel(st_Measure->f_GetPlotPoint(nScan, nItem), &x, &y);
			st_Dot.push_back(Gdiplus::RectF(static_cast<FLOAT32>(x) - plotRadius, static_cast<FLOAT32>(y) - plotRadius, 2.0F * plotRadius, 2.0F * plotRadius));
		}
	}

	f_Plot_FillDots(st_Graphics, PLOT_COLOR_PLOT, st_Dot);

	// 참값 궤적: 지금 스캔까지 진하게, 그 뒤는 옅게
	for (nObject = 1; (isShowTrue != 0) && (nObject < st_Result->f_GetObjectNum()); nObject++)
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

	// 시작점 원 (레이다, 표적)
	for (nObject = 0; nObject < st_Result->f_GetObjectNum(); nObject++)
	{
		const FLOAT32		radius = 5.0F * scale;
		Gdiplus::SolidBrush	st_Fill(f_Plot_Color(UI_COLOR_CARD, 255));
		Gdiplus::Pen		st_Ring(f_Plot_Color(f_Ui_ObjectColor(nObject), 255), 2.0F * scale);

		if ((nObject == 0) || (isShowTrue != 0))
		{
			f_MapToPixel(st_Result->f_GetPoint(0, nObject), &x, &y);
			(VOID)st_Graphics->FillEllipse(&st_Fill, static_cast<FLOAT32>(x) - radius, static_cast<FLOAT32>(y) - radius, 2.0F * radius, 2.0F * radius);
			(VOID)st_Graphics->DrawEllipse(&st_Ring, static_cast<FLOAT32>(x) - radius, static_cast<FLOAT32>(y) - radius, 2.0F * radius, 2.0F * radius);
		}
	}

	// 트랙 이력 (같은 ID 끼리 선)
	if (isShowTrack != 0)
	{
		f_Plot_CollectTrackLines(st_Track, nLastIndex, [this](const ST_PlotPoint *st_P, FLOAT64 *pt_X, FLOAT64 *pt_Y) { f_MapToPixel(st_P, pt_X, pt_Y); },
			&st_TrackLines);
		f_Plot_DrawTrackLines(st_Graphics, st_TrackLines, scale);
	}

	// 지금 스캔: 받은 플롯, 참값 위치, 트랙 심볼
	for (nItem = 0; (nIndex >= 0) && (isShowPlot != 0) && (nItem < st_Track->f_GetPlotNum(nIndex)); nItem++)
	{
		f_MapToPixel(&st_Track->f_GetPlot(nIndex, nItem)->st_Point, &x, &y);
		f_Plot_ScanDot(st_Graphics, x, y, scale);
	}

	for (nObject = 1; (isShowTrue != 0) && (nObject < st_Result->f_GetObjectNum()) && (nCurScan < nSampleNum); nObject++)
	{
		f_MapToPixel(st_Result->f_GetPoint(nCurScan, nObject), &x, &y);
		f_Plot_TrueMark(st_Graphics, x, y, nObject, scale);
	}

	for (nItem = 0; (nIndex >= 0) && (isShowTrack != 0) && (nItem < st_Track->f_GetTrackNum(nIndex)); nItem++)
	{
		st_Item = st_Track->f_GetTrack(nIndex, nItem);
		f_MapToPixel(&st_Item->st_Point, &x, &y);
		f_MapToPixel(&st_Item->st_Lead, &leadX, &leadY);
		f_Plot_TrackSymbol(st_Graphics, &st_Font, &st_Item->st_View, x, y, leadX, leadY, f_Plot_TrackColor(st_Track->f_GetColorIndex(st_Item->st_View.TrackId)),
			scale);
	}

	nRowNum = f_Plot_LegendRows(st_Row, isShowTrue, isShowPlot, isShowTrack);

	if (nRowNum > 0)
	{
		f_Plot_Legend(st_Graphics, &st_Font, st_Row, nRowNum, st_MapArea, 1, scale);
	}

	(VOID)st_Graphics->ResetClip();
}

// 시작점 이름표 (출발 방향 반대쪽)
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
		if ((nObject == 0) || (isShowTrue != 0))
		{
			yaw = st_Result->f_GetState(0, nObject)->st_Att.Yaw;
			f_MapToPixel(st_Result->f_GetPoint(0, nObject), &x, &y);

			// 동쪽으로 출발하면 왼쪽, 아니면 오른쪽. 북쪽으로 출발하면 아래, 남쪽으로 출발하면 위
			alignX = (sin(yaw) > PLOT_LABEL_SIDE_MIN) ? Gdiplus::StringAlignmentFar : Gdiplus::StringAlignmentNear;

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

			f_Plot_LabelText(st_Graphics, st_Font, f_Plot_ObjectName(nObject),
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
	, gridLatDeg(0.01)
	, gridLonDeg(0.01)
	, minLat(0.0)
	, maxLat(0.0)
	, minLon(0.0)
	, maxLon(0.0)
	, minUp(0.0)
	, maxUp(1.0)
	, upStep(1.0)
	, upScale(1.0)
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

// 표시 여부 (참값, 플롯, 트랙) 교체 후 다시 그리기.
VOID CTrackView3D::f_SetShow(INT32 isNewShowTrue, INT32 isNewShowPlot, INT32 isNewShowTrack)
{
	isShowTrue	= isNewShowTrue;
	isShowPlot	= isNewShowPlot;
	isShowTrack	= isNewShowTrack;

	Invalidate(FALSE);
}

// 보일 스캔 번호 바꾼 뒤 다시 그리기.
VOID CTrackView3D::f_SetScan(INT32 nNewScan)
{
	nCurScan = nNewScan;

	Invalidate(FALSE);
}

// 범례 보이기.
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

	if ((st_Result != nullptr) && (st_Measure != nullptr) && (st_Track != nullptr) && (st_Result->f_GetSampleNum() > 0) && (st_Client.Width() > 0)
		&& (st_Client.Height() > 0))
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

// 장면 범위와 축척: 바닥 = 위경도 격자 (표적 참값, 보이는 플롯 · 트랙이 다 들어감. 레이다는 빼서 궤적이 크게 보이게), 고도 축 = 데이터 범위 + 0 m
VOID CTrackView3D::f_ComputeView(const CRect &st_Client)
{
	const ST_GeoMap	*st_Map = st_Result->f_GetMap();
	ST_PlotPoint	st_Corner;
	const ST_PlotPoint	*st_First = (st_Result->f_GetObjectNum() > 1) ? st_Result->f_GetPoint(0, 1) : st_Result->f_GetPoint(0, 0);
	FLOAT64			minEast = st_First->east;
	FLOAT64			maxEast = st_First->east;
	FLOAT64			minNorth = st_First->north;
	FLOAT64			maxNorth = st_First->north;
	FLOAT64			lowUp = 0.0;
	FLOAT64			highUp = 0.0;
	FLOAT64			span;
	FLOAT64			minX = 0.0;
	FLOAT64			maxX = 0.0;
	FLOAT64			minY = 0.0;
	FLOAT64			maxY = 0.0;
	FLOAT64			x = 0.0;
	FLOAT64			y = 0.0;
	INT32			nStep;
	INT32			nObject;
	INT32			nScan;
	INT32			nIndex;
	INT32			nCorner;

	st_Area.SetRect(st_Client.left + f_Ui_Scale(TRK3D_MARGIN, dpi), st_Client.top + f_Ui_Scale(TRK3D_TITLE_HEIGHT, dpi),
		st_Client.right - f_Ui_Scale(TRK3D_MARGIN, dpi), st_Client.bottom - f_Ui_Scale(TRK3D_MARGIN, dpi));

	for (nStep = 0; nStep < st_Result->f_GetSampleNum(); nStep++)
	{
		for (nObject = 1; nObject < st_Result->f_GetObjectNum(); nObject++)
		{
			f_Plot_Include(st_Result->f_GetPoint(nStep, nObject), &minEast, &maxEast, &minNorth, &maxNorth);
			lowUp	= fmin(lowUp, st_Result->f_GetPoint(nStep, nObject)->up);
			highUp	= fmax(highUp, st_Result->f_GetPoint(nStep, nObject)->up);
		}
	}

	for (nScan = 0; (isShowPlot != 0) && (nScan < st_Measure->f_GetScanNum()); nScan++)
	{
		for (nIndex = 0; nIndex < st_Measure->f_GetTargetNum(); nIndex++)
		{
			f_Plot_Include(st_Measure->f_GetPlotPoint(nScan, nIndex), &minEast, &maxEast, &minNorth, &maxNorth);
			lowUp	= fmin(lowUp, st_Measure->f_GetPlotPoint(nScan, nIndex)->up);
			highUp	= fmax(highUp, st_Measure->f_GetPlotPoint(nScan, nIndex)->up);
		}
	}

	for (nScan = 0; (isShowTrack != 0) && (nScan < st_Track->f_GetScanNum()); nScan++)
	{
		for (nIndex = 0; nIndex < st_Track->f_GetTrackNum(nScan); nIndex++)
		{
			f_Plot_Include(&st_Track->f_GetTrack(nScan, nIndex)->st_Point, &minEast, &maxEast, &minNorth, &maxNorth);
			lowUp	= fmin(lowUp, st_Track->f_GetTrack(nScan, nIndex)->st_Point.up);
			highUp	= fmax(highUp, st_Track->f_GetTrack(nScan, nIndex)->st_Point.up);
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

	// 바닥 격자 = 위경도 눈금에 맞춘 범위
	gridLatDeg	= f_Plot_NiceStepGeo((maxNorth - minNorth) / st_Map->meterPerLat, TRK3D_GRID_NUM);
	gridLonDeg	= f_Plot_NiceStepGeo((maxEast - minEast) / st_Map->meterPerLon, TRK3D_GRID_NUM);
	minLat		= floor(f_Scn_NorthToLat(st_Map, minNorth) / gridLatDeg) * gridLatDeg;
	maxLat		= ceil(f_Scn_NorthToLat(st_Map, maxNorth) / gridLatDeg) * gridLatDeg;
	minLon		= floor(f_Scn_EastToLon(st_Map, minEast) / gridLonDeg) * gridLonDeg;
	maxLon		= ceil(f_Scn_EastToLon(st_Map, maxEast) / gridLonDeg) * gridLonDeg;
	minEast		= f_Scn_LonToEast(st_Map, minLon);
	maxEast		= f_Scn_LonToEast(st_Map, maxLon);
	minNorth	= f_Scn_LatToNorth(st_Map, minLat);
	maxNorth	= f_Scn_LatToNorth(st_Map, maxLat);

	// 고도 축: 0 m (바닥) 을 넣고 TRK3D_MIN_UP 이상
	span = highUp - lowUp;

	if (span < TRK3D_MIN_UP)
	{
		lowUp	= (lowUp < 0.0) ? (0.5 * ((lowUp + highUp) - TRK3D_MIN_UP)) : 0.0;
		lowUp	= fmin(lowUp, 0.0);
		highUp	= lowUp + TRK3D_MIN_UP;
	}

	upStep		= f_Plot_NiceStepMeter(highUp - lowUp, 5.0);
	minUp		= floor(lowUp / upStep) * upStep;
	maxUp		= ceil(highUp / upStep) * upStep;
	upScale		= floor(fmax(1.0, fmin(TRK3D_UP_SCALE_MAX, (TRK3D_UP_RATIO * fmax(maxEast - minEast, maxNorth - minNorth)) / (maxUp - minUp))) + 0.5);
	centerEast	= 0.5 * (minEast + maxEast);
	centerNorth	= 0.5 * (minNorth + maxNorth);
	centerUp	= 0.5 * (minUp + maxUp);

	// 상자 꼭짓점 8 개를 축척 1 로 투영해 차지하는 폭, 높이를 잼
	pixelPerMeter = 1.0;

	for (nCorner = 0; nCorner < 8; nCorner++)
	{
		st_Corner.east	= ((nCorner & 1) != 0) ? maxEast : minEast;
		st_Corner.north	= ((nCorner & 2) != 0) ? maxNorth : minNorth;
		st_Corner.up	= ((nCorner & 4) != 0) ? maxUp : minUp;
		f_Project(&st_Corner, &x, &y);
		minX = (nCorner == 0) ? x : fmin(minX, x);
		maxX = (nCorner == 0) ? x : fmax(maxX, x);
		minY = (nCorner == 0) ? y : fmin(minY, y);
		maxY = (nCorner == 0) ? y : fmax(maxY, y);
	}

	pixelPerMeter = zoom * fmin(static_cast<FLOAT64>(st_Area.Width()) / (fmax(maxX - minX, 1.0) * TRK3D_FIT_SCALE),
		static_cast<FLOAT64>(st_Area.Height()) / (fmax(maxY - minY, 1.0) * TRK3D_FIT_SCALE));
}

// 그림 좌표 (경도, 위도, 고도) -> 그림 픽셀 (정투영, 고도는 upScale 배)
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

// 바닥 (고도 0) 위경도 격자와 눈금, 축 이름, 북쪽 표시, 고도 눈금 기둥 (북서 모서리, 눈금 글자는 오른쪽)
VOID CTrackView3D::f_DrawGround(Gdiplus::Graphics *st_Graphics, const Gdiplus::Font *st_Font) const
{
	const ST_GeoMap	*st_Map = st_Result->f_GetMap();
	const FLOAT64	east0 = f_Scn_LonToEast(st_Map, minLon);
	const FLOAT64	east1 = f_Scn_LonToEast(st_Map, maxLon);
	const FLOAT64	north0 = f_Scn_LatToNorth(st_Map, minLat);
	const FLOAT64	north1 = f_Scn_LatToNorth(st_Map, maxLat);
	const FLOAT64	textHeight = 1.4 * static_cast<FLOAT64>(PLOT_FONT_PX) * static_cast<FLOAT64>(f_Ui_Scale(100, dpi)) / 100.0;
	const INT32		nLonLine = f_Plot_Round((maxLon - minLon) / gridLonDeg);
	const INT32		nLatLine = f_Plot_Round((maxLat - minLat) / gridLatDeg);
	const INT32		nUpTick = f_Plot_Round((maxUp - minUp) / upStep);
	Gdiplus::Pen	st_GridPen(f_Plot_Color(UI_COLOR_BORDER, 255), 1.0F);
	Gdiplus::Pen	st_AxisPen(f_Plot_Color(UI_COLOR_TEXT_SUB, 255), 1.2F);
	const Gdiplus::PointF	st_Zero(0.0F, 0.0F);
	Gdiplus::RectF	st_Bound;
	ST_PlotPoint	st_A = {};
	ST_PlotPoint	st_B = {};
	FLOAT64			ax = 0.0;
	FLOAT64			ay = 0.0;
	FLOAT64			bx = 0.0;
	FLOAT64			by = 0.0;
	FLOAT64			degree;
	FLOAT64			labelWidth = 0.0;
	CString			st_Text;
	INT32			nLine;

	// 경도선 (남쪽 끝에 경도 눈금)
	for (nLine = 0; nLine <= nLonLine; nLine++)
	{
		degree		= minLon + (static_cast<FLOAT64>(nLine) * gridLonDeg);
		st_A.east	= f_Scn_LonToEast(st_Map, degree);
		st_A.north	= north0;
		st_B.east	= st_A.east;
		st_B.north	= north1;
		f_Project(&st_A, &ax, &ay);
		f_Project(&st_B, &bx, &by);
		(VOID)st_Graphics->DrawLine(&st_GridPen, static_cast<FLOAT32>(ax), static_cast<FLOAT32>(ay), static_cast<FLOAT32>(bx), static_cast<FLOAT32>(by));
		f_Plot_Text(st_Graphics, st_Font, f_Plot_Number(degree, f_Plot_DecimalFor(gridLonDeg)), ax, ay + 4.0, Gdiplus::StringAlignmentCenter,
			Gdiplus::StringAlignmentNear, UI_COLOR_TEXT_SUB);
	}

	// 위도선 (서쪽 끝에 위도 눈금)
	for (nLine = 0; nLine <= nLatLine; nLine++)
	{
		degree		= minLat + (static_cast<FLOAT64>(nLine) * gridLatDeg);
		st_A.east	= east0;
		st_A.north	= f_Scn_LatToNorth(st_Map, degree);
		st_B.east	= east1;
		st_B.north	= st_A.north;
		f_Project(&st_A, &ax, &ay);
		f_Project(&st_B, &bx, &by);
		(VOID)st_Graphics->DrawLine(&st_GridPen, static_cast<FLOAT32>(ax), static_cast<FLOAT32>(ay), static_cast<FLOAT32>(bx), static_cast<FLOAT32>(by));
		st_Text = f_Plot_Number(degree, f_Plot_DecimalFor(gridLatDeg));
		f_Plot_Text(st_Graphics, st_Font, st_Text, ax - 6.0, ay, Gdiplus::StringAlignmentFar, Gdiplus::StringAlignmentCenter, UI_COLOR_TEXT_SUB);
		(VOID)st_Graphics->MeasureString(st_Text.GetString(), -1, st_Font, st_Zero, &st_Bound);
		labelWidth = fmax(labelWidth, static_cast<FLOAT64>(st_Bound.Width));
	}

	// 축 이름: 남동 모서리 아래에 경도, 서쪽 끝 가운데 (위도 눈금 왼쪽) 에 위도. 북쪽 끝 가운데에 N
	st_A.east	= east1;
	st_A.north	= north0;
	f_Project(&st_A, &ax, &ay);
	f_Plot_Text(st_Graphics, st_Font, CString(_T("경도 [°]")), ax, ay + 4.0 + textHeight, Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentNear,
		UI_COLOR_TEXT_OFF);
	st_A.east	= east0;
	st_A.north	= 0.5 * (north0 + north1);
	f_Project(&st_A, &ax, &ay);
	f_Plot_Text(st_Graphics, st_Font, CString(_T("위도 [°]")), ax - 10.0 - labelWidth, ay, Gdiplus::StringAlignmentFar, Gdiplus::StringAlignmentCenter,
		UI_COLOR_TEXT_OFF);
	st_A.east	= 0.5 * (east0 + east1);
	st_A.north	= north1;
	f_Project(&st_A, &ax, &ay);
	f_Plot_LabelText(st_Graphics, st_Font, CString(_T("N")), ax, ay, Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentCenter, UI_COLOR_TEXT);

	// 고도 기둥 (북서 모서리). 눈금은 실제 고도 [m]
	st_A.east	= east0;
	st_A.north	= north1;
	st_A.up		= minUp;
	st_B		= st_A;
	st_B.up		= maxUp;
	f_Project(&st_A, &ax, &ay);
	f_Project(&st_B, &bx, &by);
	(VOID)st_Graphics->DrawLine(&st_AxisPen, static_cast<FLOAT32>(ax), static_cast<FLOAT32>(ay), static_cast<FLOAT32>(bx), static_cast<FLOAT32>(by));
	f_Plot_Text(st_Graphics, st_Font, CString(_T("고도 [m]")), bx, by - 4.0, Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentFar, UI_COLOR_TEXT_OFF);

	for (nLine = 0; nLine <= nUpTick; nLine++)
	{
		st_A.up = minUp + (static_cast<FLOAT64>(nLine) * upStep);
		f_Project(&st_A, &ax, &ay);
		(VOID)st_Graphics->DrawLine(&st_AxisPen, static_cast<FLOAT32>(ax) - 3.0F, static_cast<FLOAT32>(ay), static_cast<FLOAT32>(ax) + 3.0F, static_cast<FLOAT32>(ay));
		st_Text.Format(_T("%.0f"), st_A.up);
		f_Plot_Text(st_Graphics, st_Font, st_Text, ax + 6.0, ay, Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentCenter, UI_COLOR_TEXT_SUB);
	}
}

// 장면: 바닥, 궤적 그림자, 지난 플롯, 참값 궤적, 트랙 이력 선, 지금 스캔의 플롯 · 참값 · 트랙, 범례
VOID CTrackView3D::f_DrawScene(Gdiplus::Graphics *st_Graphics) const
{
	static const Gdiplus::REAL		s_Dash[2] = { 3.0F, 3.0F };
	const FLOAT32					scale = static_cast<FLOAT32>(f_Ui_Scale(100, dpi)) / 100.0F;
	const Gdiplus::FontFamily		st_Family(L"Malgun Gothic");
	const Gdiplus::Font				st_Font(&st_Family, scale * PLOT_FONT_PX, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	const Gdiplus::Font				st_Bold(&st_Family, scale * PLOT_FONT_PX, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
	const INT32						nSampleNum = st_Result->f_GetSampleNum();
	const INT32						nIndex = st_Track->f_FindScan(nCurScan);
	const INT32						nLastIndex = st_Track->f_FindLastUpTo(nCurScan);
	const INT32						nScanEnd = std::min(nCurScan + 1, st_Measure->f_GetScanNum());
	const INT32						nNow = std::min(std::max(nCurScan, 0), nSampleNum - 1);		// 지금 스캔의 표본
	const FLOAT32					plotRadius = PLOT_PLOT_RADIUS * scale;
	std::vector<Gdiplus::PointF>	st_Line(static_cast<UINT64>(nSampleNum));
	std::vector<Gdiplus::RectF>		st_Dot;
	std::vector<ST_TrackLine>		st_TrackLines;
	Gdiplus::Pen					st_DropPen(f_Plot_Color(UI_COLOR_TEXT_OFF, 255), 1.0F);
	ST_LegendRow					st_Row[PLOT_LEG_MAX];
	ST_PlotPoint					st_Ground;
	const ST_TrackItem				*st_Item;
	FLOAT64							x = 0.0;
	FLOAT64							y = 0.0;
	FLOAT64							gx = 0.0;
	FLOAT64							gy = 0.0;
	FLOAT64							leadX = 0.0;
	FLOAT64							leadY = 0.0;
	FLOAT64							maxObjectUp;
	CString							st_Head;
	INT32							nObject;
	INT32							nStep;
	INT32							nScan;
	INT32							nItem;
	INT32							nRowNum;

	(VOID)st_DropPen.SetDashPattern(s_Dash, 2);
	(VOID)st_Graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

	st_Head.Format(_T("3D 궤적 (경도-위도-고도, 고도 %.0f배)"), upScale);
	f_Plot_Text(st_Graphics, &st_Bold, f_Plot_Title(st_Head.GetString(), isShowTrue, isShowPlot, isShowTrack), static_cast<FLOAT64>(st_Area.left),
		static_cast<FLOAT64>(st_Area.top) - 5.0, Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentFar, UI_COLOR_TEXT);
	f_Plot_Text(st_Graphics, &st_Font, CString(_T("끌어서 회전, 휠로 확대, 두 번 클릭하면 처음 보기")), static_cast<FLOAT64>(st_Area.left),
		static_cast<FLOAT64>(st_Area.bottom), Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentFar, UI_COLOR_TEXT_OFF);

	f_DrawGround(st_Graphics, &st_Font);

	// 높이 나는 표적은 참값 궤적의 바닥 그림자 (회색, 지금 스캔까지)
	for (nObject = 1; (isShowTrue != 0) && (nNow > 0) && (nObject < st_Result->f_GetObjectNum()); nObject++)
	{
		Gdiplus::Pen st_ShadowPen(f_Plot_Color(UI_COLOR_TEXT_OFF, TRK3D_SHADOW_ALPHA), 1.2F * scale);

		maxObjectUp = 0.0;

		for (nStep = 0; nStep <= nNow; nStep++)
		{
			st_Ground		= *st_Result->f_GetPoint(nStep, nObject);
			maxObjectUp		= fmax(maxObjectUp, st_Ground.up);
			st_Ground.up	= 0.0;
			f_Project(&st_Ground, &x, &y);
			st_Line[static_cast<UINT64>(nStep)] = Gdiplus::PointF(static_cast<FLOAT32>(x), static_cast<FLOAT32>(y));
		}

		if (maxObjectUp > TRK3D_SHADOW_MIN_UP)
		{
			(VOID)st_Graphics->DrawLines(&st_ShadowPen, st_Line.data(), nNow + 1);
		}
	}

	// 지난 스캔 플롯 (옅게)
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
	for (nObject = 1; (isShowTrue != 0) && (nObject < st_Result->f_GetObjectNum()); nObject++)
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

	// 레이다 (바닥 격자 안에 있을 때만)
	if ((st_Result->f_GetPoint(nNow, 0)->east >= f_Scn_LonToEast(st_Result->f_GetMap(), minLon)) && (st_Result->f_GetPoint(nNow, 0)->east <= f_Scn_LonToEast(st_Result->f_GetMap(), maxLon))
		&& (st_Result->f_GetPoint(nNow, 0)->north >= f_Scn_LatToNorth(st_Result->f_GetMap(), minLat)) && (st_Result->f_GetPoint(nNow, 0)->north <= f_Scn_LatToNorth(st_Result->f_GetMap(), maxLat)))
	{
		const FLOAT32		radius = 5.0F * scale;
		Gdiplus::SolidBrush	st_Fill(f_Plot_Color(UI_COLOR_CARD, 255));
		Gdiplus::Pen		st_Ring(f_Plot_Color(f_Ui_ObjectColor(0), 255), 2.0F * scale);

		f_Project(st_Result->f_GetPoint(nNow, 0), &x, &y);
		(VOID)st_Graphics->FillEllipse(&st_Fill, static_cast<FLOAT32>(x) - radius, static_cast<FLOAT32>(y) - radius, 2.0F * radius, 2.0F * radius);
		(VOID)st_Graphics->DrawEllipse(&st_Ring, static_cast<FLOAT32>(x) - radius, static_cast<FLOAT32>(y) - radius, 2.0F * radius, 2.0F * radius);
		f_Plot_LabelText(st_Graphics, &st_Font, f_Plot_ObjectName(0), x + 8.0, y + 4.0, Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentNear,
			f_Ui_ObjectColor(0));
	}

	// 트랙 이력 (같은 ID 끼리 선)
	if (isShowTrack != 0)
	{
		f_Plot_CollectTrackLines(st_Track, nLastIndex, [this](const ST_PlotPoint *st_P, FLOAT64 *pt_X, FLOAT64 *pt_Y) { f_Project(st_P, pt_X, pt_Y); },
			&st_TrackLines);
		f_Plot_DrawTrackLines(st_Graphics, st_TrackLines, scale);
	}

	// 지금 스캔에 받은 플롯
	for (nItem = 0; (nIndex >= 0) && (isShowPlot != 0) && (nItem < st_Track->f_GetPlotNum(nIndex)); nItem++)
	{
		f_Project(&st_Track->f_GetPlot(nIndex, nItem)->st_Point, &x, &y);
		f_Plot_ScanDot(st_Graphics, x, y, scale);
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

	// 지금 트랙
	for (nItem = 0; (nIndex >= 0) && (isShowTrack != 0) && (nItem < st_Track->f_GetTrackNum(nIndex)); nItem++)
	{
		st_Item = st_Track->f_GetTrack(nIndex, nItem);
		f_Project(&st_Item->st_Point, &x, &y);
		f_Project(&st_Item->st_Lead, &leadX, &leadY);
		f_Plot_TrackSymbol(st_Graphics, &st_Font, &st_Item->st_View, x, y, leadX, leadY, f_Plot_TrackColor(st_Track->f_GetColorIndex(st_Item->st_View.TrackId)),
			scale);
	}

	nRowNum = f_Plot_LegendRows(st_Row, isShowTrue, isShowPlot, isShowTrack);

	if ((nRowNum > 0) && (isShowLegend != 0))
	{
		f_Plot_Legend(st_Graphics, &st_Font, st_Row, nRowNum, st_Area, 0, scale);
	}
}

// CProfilePlot

BEGIN_MESSAGE_MAP(CProfilePlot, CStatic)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_MOUSEMOVE()
	ON_WM_CAPTURECHANGED()
END_MESSAGE_MAP()

// 2D 단면 그림. 결과 버퍼는 대화상자 소유, 여기서는 읽기만.
CProfilePlot::CProfilePlot() noexcept
	: st_Result(nullptr)
	, st_Measure(nullptr)
	, st_Track(nullptr)
	, nAxis(PROFILE_LAT)
	, isShowTrue(1)
	, isShowPlot(1)
	, isShowTrack(1)
	, isShowLegend(0)
	, nCurScan(0)
	, dpi(UI_BASE_DPI)
	, homeAcross(0.0)
	, homeUp(0.0)
	, homeSpanAcross(1.0)
	, homeSpanUp(1.0)
	, zoom(1.0)
	, panAcross(0.0)
	, panUp(0.0)
	, centerAcross(0.0)
	, centerUp(0.0)
	, spanAcross(1.0)
	, spanUp(1.0)
	, isDragging(0)
	, st_DragStart(0, 0)
	, dragPanAcross(0.0)
	, dragPanUp(0.0)
	, st_Area(0, 0, 0, 0)
{
}

// 가로축 (PROFILE_LAT: 위도-고도, PROFILE_LON: 경도-고도). 처음 보기로 돌림
VOID CProfilePlot::f_SetAxis(INT32 nNewAxis)
{
	nAxis		= (nNewAxis == PROFILE_LON) ? PROFILE_LON : PROFILE_LAT;
	zoom		= 1.0;
	panAcross	= 0.0;
	panUp		= 0.0;

	Invalidate(FALSE);
}

// 화면 배율 설정.
VOID CProfilePlot::f_SetDpi(INT32 newDpi)
{
	dpi = newDpi;
}

// 그릴 결과 교체 후 처음 보기로 다시 그리기
VOID CProfilePlot::f_SetData(const CSimResult *st_NewResult, const CPlotResult *st_NewMeasure, const CTrackResult *st_NewTrack)
{
	st_Result	= st_NewResult;
	st_Measure	= st_NewMeasure;
	st_Track	= st_NewTrack;
	zoom		= 1.0;
	panAcross	= 0.0;
	panUp		= 0.0;

	Invalidate(FALSE);
}

// 표시 여부 (참값, 플롯, 트랙) 교체 후 다시 그리기.
VOID CProfilePlot::f_SetShow(INT32 isNewShowTrue, INT32 isNewShowPlot, INT32 isNewShowTrack)
{
	isShowTrue	= isNewShowTrue;
	isShowPlot	= isNewShowPlot;
	isShowTrack	= isNewShowTrack;

	Invalidate(FALSE);
}

// 보일 스캔 번호 바꾼 뒤 다시 그리기.
VOID CProfilePlot::f_SetScan(INT32 nNewScan)
{
	nCurScan = nNewScan;

	Invalidate(FALSE);
}

// 범례 보이기.
VOID CProfilePlot::f_SetLegend(INT32 isNewShowLegend)
{
	isShowLegend = isNewShowLegend;

	Invalidate(FALSE);
}

// 휠 확대 · 축소 (두 축 같이). 커서 아래 지점이 제자리에 있도록 중심을 옮김. 커서가 그래프 밖이면 무시.
VOID CProfilePlot::f_Zoom(INT32 delta, CPoint st_ScreenPoint)
{
	CPoint	st_Point = st_ScreenPoint;
	FLOAT64	offsetX;
	FLOAT64	offsetY;
	FLOAT64	cursorAcross;
	FLOAT64	cursorUp;
	FLOAT64	newZoom;

	ScreenToClient(&st_Point);

	if ((st_Area.PtInRect(st_Point) != FALSE) && (st_Area.Width() > 0) && (st_Area.Height() > 0))
	{
		offsetX			= (static_cast<FLOAT64>(st_Point.x) - (static_cast<FLOAT64>(st_Area.left) + (0.5 * static_cast<FLOAT64>(st_Area.Width())))) / static_cast<FLOAT64>(st_Area.Width());
		offsetY			= (static_cast<FLOAT64>(st_Point.y) - (static_cast<FLOAT64>(st_Area.top) + (0.5 * static_cast<FLOAT64>(st_Area.Height())))) / static_cast<FLOAT64>(st_Area.Height());
		cursorAcross	= centerAcross + (offsetX * spanAcross);
		cursorUp		= centerUp - (offsetY * spanUp);
		newZoom			= fmin(PLOT_ZOOM_MAX, fmax(1.0, zoom * pow(PLOT_ZOOM_STEP, static_cast<FLOAT64>(delta) / static_cast<FLOAT64>(WHEEL_DELTA))));

		// 새 중심 = 커서 지점 - 커서까지 비율 x 새 폭
		panAcross	= (cursorAcross - (offsetX * (homeSpanAcross / newZoom))) - homeAcross;
		panUp		= (cursorUp + (offsetY * (homeSpanUp / newZoom))) - homeUp;
		zoom		= newZoom;

		if (zoom <= 1.0)
		{
			panAcross	= 0.0;
			panUp		= 0.0;
		}

		Invalidate(FALSE);
	}
}

// 끌기 시작.
VOID CProfilePlot::OnLButtonDown(UINT32 flags, CPoint st_Point)
{
	isDragging		= 1;
	st_DragStart	= st_Point;
	dragPanAcross	= panAcross;
	dragPanUp		= panUp;
	(VOID)SetCapture();

	CStatic::OnLButtonDown(flags, st_Point);
}

// 끌기 끝.
VOID CProfilePlot::OnLButtonUp(UINT32 flags, CPoint st_Point)
{
	isDragging = 0;

	if (GetCapture() == this)
	{
		(VOID)ReleaseCapture();
	}

	CStatic::OnLButtonUp(flags, st_Point);
}

// 두 번 클릭하면 처음 보기.
VOID CProfilePlot::OnLButtonDblClk(UINT32 flags, CPoint st_Point)
{
	zoom		= 1.0;
	panAcross	= 0.0;
	panUp		= 0.0;
	Invalidate(FALSE);

	CStatic::OnLButtonDblClk(flags, st_Point);
}

// 끄는 동안 이동.
VOID CProfilePlot::OnMouseMove(UINT32 flags, CPoint st_Point)
{
	if ((isDragging != 0) && (st_Area.Width() > 0) && (st_Area.Height() > 0))
	{
		panAcross	= dragPanAcross - ((static_cast<FLOAT64>(st_Point.x - st_DragStart.x) / static_cast<FLOAT64>(st_Area.Width())) * spanAcross);
		panUp		= dragPanUp + ((static_cast<FLOAT64>(st_Point.y - st_DragStart.y) / static_cast<FLOAT64>(st_Area.Height())) * spanUp);
		Invalidate(FALSE);
	}

	CStatic::OnMouseMove(flags, st_Point);
}

// 다른 창이 마우스를 가져가면 끌기 끝.
VOID CProfilePlot::OnCaptureChanged(CWnd *st_Wnd)
{
	isDragging = 0;

	CStatic::OnCaptureChanged(st_Wnd);
}

// 배경 지우기 생략. OnPaint 에서 전부 그림.
BOOL CProfilePlot::OnEraseBkgnd(CDC *st_Dc)
{
	UNREFERENCED_PARAMETER(st_Dc);

	return TRUE;
}

// 크기 변경 시 전체 다시 그리기.
VOID CProfilePlot::OnSize(UINT32 type, INT32 width, INT32 height)
{
	CStatic::OnSize(type, width, height);
	Invalidate(FALSE);
}

// 메모리 DC 에 그린 뒤 한 번에 복사.
VOID CProfilePlot::OnPaint(VOID)
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

	if ((st_Result != nullptr) && (st_Measure != nullptr) && (st_Track != nullptr) && (st_Result->f_GetSampleNum() > 0) && (st_Client.Width() > 0)
		&& (st_Client.Height() > 0))
	{
		f_ComputeView(st_Client);

		{
			Gdiplus::Graphics st_Graphics(st_MemDc.GetSafeHdc());

			(VOID)st_Graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
			f_Draw(&st_Graphics);
		}
	}

	(VOID)st_Dc.BitBlt(0, 0, st_Client.Width(), st_Client.Height(), &st_MemDc, 0, 0, SRCCOPY);
	(VOID)st_MemDc.SelectObject(st_OldBitmap);
}

// 그림 좌표의 가로 성분 (위도면 북, 경도면 동)
FLOAT64 CProfilePlot::f_Across(const ST_PlotPoint *st_Point) const
{
	return (nAxis == PROFILE_LON) ? st_Point->east : st_Point->north;
}

// 처음 보기 범위 = 표적 참값 · 보이는 플롯 · 트랙 (레이다는 빼서 표적 움직임이 크게 보이게), 지금 보기 = 처음 보기 / 배율 + 이동
VOID CProfilePlot::f_ComputeView(const CRect &st_Client)
{
	FLOAT64	minAcross = 0.0;
	FLOAT64	maxAcross = 0.0;
	FLOAT64	lowUp = 0.0;
	FLOAT64	highUp = 0.0;
	INT32	isFirst = 1;
	INT32	nStep;
	INT32	nObject;
	INT32	nScan;
	INT32	nIndex;

	// 한 점을 범위에 넣음 (첫 점이면 범위를 그 점으로)
	auto f_Add = [&](const ST_PlotPoint *st_Point)
	{
		const FLOAT64 across = f_Across(st_Point);

		minAcross	= (isFirst != 0) ? across : fmin(minAcross, across);
		maxAcross	= (isFirst != 0) ? across : fmax(maxAcross, across);
		lowUp		= (isFirst != 0) ? st_Point->up : fmin(lowUp, st_Point->up);
		highUp		= (isFirst != 0) ? st_Point->up : fmax(highUp, st_Point->up);
		isFirst		= 0;
	};

	st_Area.SetRect(st_Client.left + f_Ui_Scale(PROF_MARGIN_LEFT, dpi), st_Client.top + f_Ui_Scale(PROF_TITLE_HEIGHT, dpi),
		st_Client.right - f_Ui_Scale(PROF_MARGIN_RIGHT, dpi), st_Client.bottom - f_Ui_Scale(PROF_AXIS_HEIGHT, dpi));

	for (nStep = 0; nStep < st_Result->f_GetSampleNum(); nStep++)
	{
		for (nObject = 1; nObject < st_Result->f_GetObjectNum(); nObject++)
		{
			f_Add(st_Result->f_GetPoint(nStep, nObject));
		}
	}

	for (nScan = 0; (isShowPlot != 0) && (nScan < st_Measure->f_GetScanNum()); nScan++)
	{
		for (nIndex = 0; nIndex < st_Measure->f_GetTargetNum(); nIndex++)
		{
			f_Add(st_Measure->f_GetPlotPoint(nScan, nIndex));
		}
	}

	for (nScan = 0; (isShowTrack != 0) && (nScan < st_Track->f_GetScanNum()); nScan++)
	{
		for (nIndex = 0; nIndex < st_Track->f_GetTrackNum(nScan); nIndex++)
		{
			f_Add(&st_Track->f_GetTrack(nScan, nIndex)->st_Point);
		}
	}

	homeAcross		= 0.5 * (minAcross + maxAcross);
	homeUp			= 0.5 * (lowUp + highUp);
	homeSpanAcross	= fmax(maxAcross - minAcross, PROF_MIN_ACROSS) * (1.0 + (2.0 * PROF_PAD_RATIO));
	homeSpanUp		= fmax(highUp - lowUp, PROF_MIN_UP) * (1.0 + (2.0 * PROF_PAD_RATIO));
	spanAcross		= homeSpanAcross / zoom;
	spanUp			= homeSpanUp / zoom;
	centerAcross	= homeAcross + panAcross;
	centerUp		= homeUp + panUp;
}

// 그림 좌표 -> 그래프 픽셀
VOID CProfilePlot::f_ToPixel(const ST_PlotPoint *st_Point, FLOAT64 *pt_X, FLOAT64 *pt_Y) const
{
	*pt_X = static_cast<FLOAT64>(st_Area.left) + (static_cast<FLOAT64>(st_Area.Width()) * (0.5 + ((f_Across(st_Point) - centerAcross) / spanAcross)));
	*pt_Y = static_cast<FLOAT64>(st_Area.top) + (static_cast<FLOAT64>(st_Area.Height()) * (0.5 - ((st_Point->up - centerUp) / spanUp)));
}

// 단면: 격자 (가로 위도 또는 경도 [deg], 세로 고도 [m]), 지난 플롯, 참값, 트랙 이력, 지금 스캔의 플롯 · 참값 · 트랙
VOID CProfilePlot::f_Draw(Gdiplus::Graphics *st_Graphics) const
{
	const FLOAT32				scale = static_cast<FLOAT32>(f_Ui_Scale(100, dpi)) / 100.0F;
	const Gdiplus::FontFamily	st_Family(L"Malgun Gothic");
	const Gdiplus::Font			st_Font(&st_Family, scale * PLOT_FONT_PX, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	const Gdiplus::Font			st_Bold(&st_Family, scale * PLOT_FONT_PX, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
	const ST_GeoMap				*st_Map = st_Result->f_GetMap();
	const INT32					isLon = (nAxis == PROFILE_LON) ? 1 : 0;
	const FLOAT64				lowDeg = (isLon != 0) ? f_Scn_EastToLon(st_Map, centerAcross - (0.5 * spanAcross)) : f_Scn_NorthToLat(st_Map, centerAcross - (0.5 * spanAcross));
	const FLOAT64				highDeg = (isLon != 0) ? f_Scn_EastToLon(st_Map, centerAcross + (0.5 * spanAcross)) : f_Scn_NorthToLat(st_Map, centerAcross + (0.5 * spanAcross));
	const FLOAT64				stepDeg = f_Plot_NiceStepGeo(highDeg - lowDeg, fmax(2.0, fmin(PLOT_MAX_GRID_NUM, static_cast<FLOAT64>(st_Area.Width()) / (PROF_TICK_X_PX * static_cast<FLOAT64>(scale)))));
	const FLOAT64				stepUp = f_Plot_NiceStepMeter(spanUp, fmax(2.0, fmin(PLOT_MAX_GRID_NUM, static_cast<FLOAT64>(st_Area.Height()) / (PROF_TICK_Y_PX * static_cast<FLOAT64>(scale)))));
	const INT32					nSampleNum = st_Result->f_GetSampleNum();
	const INT32					nNow = std::min(std::max(nCurScan, 0), nSampleNum - 1);
	const INT32					nIndex = st_Track->f_FindScan(nCurScan);
	const INT32					nLastIndex = st_Track->f_FindLastUpTo(nCurScan);
	const INT32					nScanEnd = std::min(nCurScan + 1, st_Measure->f_GetScanNum());
	const FLOAT32				plotRadius = PLOT_PLOT_RADIUS * scale;
	const Gdiplus::Rect			st_Clip(st_Area.left, st_Area.top, st_Area.Width(), st_Area.Height());
	Gdiplus::Pen				st_GridPen(f_Plot_Color(UI_COLOR_GRID_LINE, 255), 1.0F);
	Gdiplus::Pen				st_FramePen(f_Plot_Color(UI_COLOR_BORDER, 255), 1.0F);
	std::vector<Gdiplus::PointF>	st_Line(static_cast<UINT64>(nSampleNum));
	std::vector<Gdiplus::RectF>		st_Dot;
	std::vector<ST_TrackLine>		st_TrackLines;
	ST_LegendRow				st_Row[PLOT_LEG_MAX];
	ST_PlotPoint				st_Point = {};
	const ST_TrackItem			*st_Item;
	FLOAT64						x = 0.0;
	FLOAT64						y = 0.0;
	FLOAT64						leadX = 0.0;
	FLOAT64						leadY = 0.0;
	FLOAT64						value;
	FLOAT64						first;
	CString						st_Text;
	INT32						nLine;
	INT32						nLineNum;
	INT32						nObject;
	INT32						nStep;
	INT32						nScan;
	INT32						nItem;
	INT32						nRowNum;

	(VOID)st_Graphics->SetSmoothingMode(Gdiplus::SmoothingModeNone);

	// 세로선 = 위도 (또는 경도) 눈금
	first		= ceil(lowDeg / stepDeg);
	nLineNum	= static_cast<INT32>(floor(highDeg / stepDeg) - first);

	for (nLine = 0; nLine <= nLineNum; nLine++)
	{
		value			= (first + static_cast<FLOAT64>(nLine)) * stepDeg;
		st_Point.north	= f_Scn_LatToNorth(st_Map, value);
		st_Point.east	= f_Scn_LonToEast(st_Map, value);
		f_ToPixel(&st_Point, &x, &y);
		(VOID)st_Graphics->DrawLine(&st_GridPen, f_Plot_Round(x), st_Area.top, f_Plot_Round(x), st_Area.bottom);
		f_Plot_Text(st_Graphics, &st_Font, f_Plot_Number(value, f_Plot_DecimalFor(stepDeg)), x, static_cast<FLOAT64>(st_Area.bottom) + 4.0,
			Gdiplus::StringAlignmentCenter, Gdiplus::StringAlignmentNear, UI_COLOR_TEXT_SUB);
	}

	// 가로선 = 고도 눈금
	first		= ceil((centerUp - (0.5 * spanUp)) / stepUp);
	nLineNum	= static_cast<INT32>(floor((centerUp + (0.5 * spanUp)) / stepUp) - first);

	for (nLine = 0; nLine <= nLineNum; nLine++)
	{
		st_Point.up = (first + static_cast<FLOAT64>(nLine)) * stepUp;
		f_ToPixel(&st_Point, &x, &y);
		(VOID)st_Graphics->DrawLine(&st_GridPen, st_Area.left, f_Plot_Round(y), st_Area.right, f_Plot_Round(y));
		st_Text.Format(_T("%.0f"), st_Point.up);
		f_Plot_Text(st_Graphics, &st_Font, st_Text, static_cast<FLOAT64>(st_Area.left) - 6.0, y, Gdiplus::StringAlignmentFar, Gdiplus::StringAlignmentCenter,
			UI_COLOR_TEXT_SUB);
	}

	(VOID)st_Graphics->DrawRectangle(&st_FramePen, st_Area.left, st_Area.top, st_Area.Width(), st_Area.Height());
	f_Plot_Text(st_Graphics, &st_Bold, f_Plot_Title((isLon != 0) ? _T("2D 궤적 (경도-고도)") : _T("2D 궤적 (위도-고도)"), isShowTrue, isShowPlot, isShowTrack),
		static_cast<FLOAT64>(st_Area.left), static_cast<FLOAT64>(st_Area.top) - 5.0, Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentFar, UI_COLOR_TEXT);
	f_Plot_Text(st_Graphics, &st_Font, CString((isLon != 0) ? _T("경도 [°]") : _T("위도 [°]")), static_cast<FLOAT64>(st_Area.right) - 6.0,
		static_cast<FLOAT64>(st_Area.bottom) - 4.0, Gdiplus::StringAlignmentFar, Gdiplus::StringAlignmentFar, UI_COLOR_TEXT_OFF);
	f_Plot_Text(st_Graphics, &st_Font, CString(_T("고도 [m]")), static_cast<FLOAT64>(st_Area.left) + 6.0, static_cast<FLOAT64>(st_Area.top) + 4.0,
		Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentNear, UI_COLOR_TEXT_OFF);

	if (zoom > 1.0)
	{
		st_Text.Format(_T("%.1f배, 두 번 클릭하면 처음 보기"), zoom);
		f_Plot_Text(st_Graphics, &st_Font, st_Text, static_cast<FLOAT64>(st_Area.right), static_cast<FLOAT64>(st_Area.top) - 5.0, Gdiplus::StringAlignmentFar,
			Gdiplus::StringAlignmentFar, UI_COLOR_TEXT_SUB);
	}

	(VOID)st_Graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	(VOID)st_Graphics->SetClip(st_Clip);

	// 지난 스캔 플롯
	for (nScan = 0; (isShowPlot != 0) && (nScan < nScanEnd); nScan++)
	{
		for (nItem = 0; nItem < st_Measure->f_GetTargetNum(); nItem++)
		{
			f_ToPixel(st_Measure->f_GetPlotPoint(nScan, nItem), &x, &y);
			st_Dot.push_back(Gdiplus::RectF(static_cast<FLOAT32>(x) - plotRadius, static_cast<FLOAT32>(y) - plotRadius, 2.0F * plotRadius, 2.0F * plotRadius));
		}
	}

	f_Plot_FillDots(st_Graphics, PLOT_COLOR_PLOT, st_Dot);

	// 참값 궤적: 지금 스캔까지 진하게, 그 뒤는 옅게
	for (nObject = 1; (isShowTrue != 0) && (nObject < st_Result->f_GetObjectNum()); nObject++)
	{
		Gdiplus::Pen st_TruthPen(f_Plot_Color(f_Ui_ObjectColor(nObject), 255), 1.8F * scale);
		Gdiplus::Pen st_FuturePen(f_Plot_Color(f_Ui_ObjectColor(nObject), PLOT_FUTURE_ALPHA), 1.2F * scale);

		(VOID)st_TruthPen.SetLineJoin(Gdiplus::LineJoinRound);
		(VOID)st_FuturePen.SetLineJoin(Gdiplus::LineJoinRound);

		for (nStep = 0; nStep < nSampleNum; nStep++)
		{
			f_ToPixel(st_Result->f_GetPoint(nStep, nObject), &x, &y);
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

	// 트랙 이력 (같은 ID 끼리 선)
	if (isShowTrack != 0)
	{
		f_Plot_CollectTrackLines(st_Track, nLastIndex, [this](const ST_PlotPoint *st_P, FLOAT64 *pt_X, FLOAT64 *pt_Y) { f_ToPixel(st_P, pt_X, pt_Y); },
			&st_TrackLines);
		f_Plot_DrawTrackLines(st_Graphics, st_TrackLines, scale);
	}

	// 지금 스캔: 받은 플롯, 참값 위치, 트랙 심볼
	for (nItem = 0; (nIndex >= 0) && (isShowPlot != 0) && (nItem < st_Track->f_GetPlotNum(nIndex)); nItem++)
	{
		f_ToPixel(&st_Track->f_GetPlot(nIndex, nItem)->st_Point, &x, &y);
		f_Plot_ScanDot(st_Graphics, x, y, scale);
	}

	for (nObject = 1; (isShowTrue != 0) && (nObject < st_Result->f_GetObjectNum()) && (nCurScan < nSampleNum); nObject++)
	{
		f_ToPixel(st_Result->f_GetPoint(nCurScan, nObject), &x, &y);
		f_Plot_TrueMark(st_Graphics, x, y, nObject, scale);
	}

	for (nItem = 0; (nIndex >= 0) && (isShowTrack != 0) && (nItem < st_Track->f_GetTrackNum(nIndex)); nItem++)
	{
		st_Item = st_Track->f_GetTrack(nIndex, nItem);
		f_ToPixel(&st_Item->st_Point, &x, &y);
		f_ToPixel(&st_Item->st_Lead, &leadX, &leadY);
		f_Plot_TrackSymbol(st_Graphics, &st_Font, &st_Item->st_View, x, y, leadX, leadY, f_Plot_TrackColor(st_Track->f_GetColorIndex(st_Item->st_View.TrackId)),
			scale);
	}

	nRowNum = f_Plot_LegendRows(st_Row, isShowTrue, isShowPlot, isShowTrack);

	if ((nRowNum > 0) && (isShowLegend != 0))
	{
		f_Plot_Legend(st_Graphics, &st_Font, st_Row, nRowNum, st_Area, 0, scale);
	}

	(VOID)st_Graphics->ResetClip();
}

// CFilterPlot

BEGIN_MESSAGE_MAP(CFilterPlot, CStatic)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
END_MESSAGE_MAP()

// 추적 성능 그림. 결과 버퍼는 대화상자 소유, 여기서는 읽기만.
CFilterPlot::CFilterPlot() noexcept
	: st_Result(nullptr)
	, st_Track(nullptr)
	, nCurScan(0)
	, dpi(UI_BASE_DPI)
	, timeMax(1.0)
	, errorMax(100.0)
	, nisLogMin(PERF_NIS_LOG_MIN)
	, nisLogMax(PERF_NIS_LOG_FLOOR)
	, st_ErrorArea(0, 0, 0, 0)
	, st_NisArea(0, 0, 0, 0)
{
}

// 화면 배율 설정.
VOID CFilterPlot::f_SetDpi(INT32 newDpi)
{
	dpi = newDpi;
}

// 그릴 결과 교체 후 다시 그리기.
VOID CFilterPlot::f_SetData(const CSimResult *st_NewResult, const CTrackResult *st_NewTrack)
{
	st_Result	= st_NewResult;
	st_Track	= st_NewTrack;

	Invalidate(FALSE);
}

// 보일 스캔 번호 바꾼 뒤 다시 그리기.
VOID CFilterPlot::f_SetScan(INT32 nNewScan)
{
	nCurScan = nNewScan;

	Invalidate(FALSE);
}

// 배경 지우기 생략. OnPaint 에서 전부 그림.
BOOL CFilterPlot::OnEraseBkgnd(CDC *st_Dc)
{
	UNREFERENCED_PARAMETER(st_Dc);

	return TRUE;
}

// 크기 변경 시 전체 다시 그리기.
VOID CFilterPlot::OnSize(UINT32 type, INT32 width, INT32 height)
{
	CStatic::OnSize(type, width, height);
	Invalidate(FALSE);
}

// 메모리 DC 에 그린 뒤 한 번에 복사.
VOID CFilterPlot::OnPaint(VOID)
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

	if ((st_Result != nullptr) && (st_Track != nullptr) && (st_Result->f_GetSampleNum() > 0) && (st_Client.Width() > 0) && (st_Client.Height() > 0))
	{
		f_ComputeView(st_Client);

		{
			Gdiplus::Graphics st_Graphics(st_MemDc.GetSafeHdc());

			(VOID)st_Graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
			f_Draw(&st_Graphics);
		}
	}

	(VOID)st_Dc.BitBlt(0, 0, st_Client.Width(), st_Client.Height(), &st_MemDc, 0, 0, SRCCOPY);
	(VOID)st_MemDc.SelectObject(st_OldBitmap);
}

// 두 그래프 자리, 축 범위: 시간 = 시나리오 길이, 오차 = 받은 전체 트랙 (갱신 TRK_STEADY_UPDATES 번 뒤) 의 최대, NIS = 최대까지 10 의 거듭제곱
VOID CFilterPlot::f_ComputeView(const CRect &st_Client)
{
	const INT32		left = st_Client.left + f_Ui_Scale(PERF_MARGIN_LEFT, dpi);
	const INT32		right = st_Client.right - f_Ui_Scale(PERF_MARGIN_RIGHT, dpi);
	const INT32		top = st_Client.top + f_Ui_Scale(PERF_TITLE_HEIGHT, dpi);
	const INT32		axis = f_Ui_Scale(PERF_AXIS_HEIGHT, dpi);
	const INT32		gap = f_Ui_Scale(PERF_GAP, dpi);
	const INT32		height = ((st_Client.bottom - top) - (2 * axis) - gap) / 2;
	const ST_TrackItem	*st_Item;
	FLOAT64			maxValue = 0.0;
	FLOAT64			maxNis = 0.0;
	FLOAT64			step;
	INT32			nIndex;
	INT32			nTrack;

	st_ErrorArea.SetRect(left, top, right, top + height);
	st_NisArea.SetRect(left, st_ErrorArea.bottom + axis + gap, right, st_ErrorArea.bottom + axis + gap + height);

	timeMax = st_Result->f_GetSample(st_Result->f_GetSampleNum() - 1)->simTime;
	timeMax = (timeMax > 0.0) ? timeMax : 1.0;

	for (nIndex = 0; nIndex < st_Track->f_GetScanNum(); nIndex++)
	{
		for (nTrack = 0; nTrack < st_Track->f_GetTrackNum(nIndex); nTrack++)
		{
			st_Item = st_Track->f_GetTrack(nIndex, nTrack);

			if (st_Item->st_View.nUpdateNum >= TRK_STEADY_UPDATES)
			{
				maxValue = fmax(maxValue, fmax(st_Item->errNorm, sqrt((st_Item->st_View.posStd[0] * st_Item->st_View.posStd[0]) +
					(st_Item->st_View.posStd[1] * st_Item->st_View.posStd[1]) + (st_Item->st_View.posStd[2] * st_Item->st_View.posStd[2]))));
			}

			maxNis = fmax(maxNis, st_Item->st_View.nis);
		}
	}

	step		= f_Plot_NiceStepMeter(fmax(maxValue * 1.1, 10.0), 5.0);
	errorMax	= ceil(fmax(maxValue * 1.1, 10.0) / step) * step;
	nisLogMin	= PERF_NIS_LOG_MIN;
	nisLogMax	= fmax(PERF_NIS_LOG_FLOOR, ceil(log10(fmax(maxNis, 1.0)) + 1.0e-9));
}

// 시간 [s] -> 가로 픽셀
FLOAT64 CFilterPlot::f_TimeToX(FLOAT64 time) const
{
	return static_cast<FLOAT64>(st_ErrorArea.left) + ((time / timeMax) * static_cast<FLOAT64>(st_ErrorArea.Width()));
}

// 위치 오차 [m] -> 위 그래프 세로 픽셀
FLOAT64 CFilterPlot::f_ErrorToY(FLOAT64 error) const
{
	return static_cast<FLOAT64>(st_ErrorArea.bottom) - ((error / errorMax) * static_cast<FLOAT64>(st_ErrorArea.Height()));
}

// NIS -> 아래 그래프 세로 픽셀 (로그 눈금, 아래 끝보다 작으면 아래 끝)
FLOAT64 CFilterPlot::f_NisToY(FLOAT64 nis) const
{
	const FLOAT64 value = fmax(log10(fmax(nis, 1.0e-300)), nisLogMin);

	return static_cast<FLOAT64>(st_NisArea.bottom) - (((value - nisLogMin) / (nisLogMax - nisLogMin)) * static_cast<FLOAT64>(st_NisArea.Height()));
}

// 띠: 선회 구간 (회색, 시나리오 기동) 과 발산 판정 구간 (빨강, 게이트 밖이 TRK_DIVERGE_NUM 번 이상 이어진 스캔). 두 그래프 모두
VOID CFilterPlot::f_DrawBands(Gdiplus::Graphics *st_Graphics, INT32 nLastIndex) const
{
	const ST_SimConfig	*st_Config = st_Result->f_GetConfig();
	const FLOAT64		halfStep = 0.5 * st_Config->stepTime;
	Gdiplus::SolidBrush	st_Turn(f_Plot_Color(PERF_COLOR_TURN, 255));
	Gdiplus::SolidBrush	st_Diverge(f_Plot_Color(PERF_COLOR_DIVERGE, 255));
	const CRect			*st_Panel[2] = { &st_ErrorArea, &st_NisArea };
	const ST_LnkScan	*st_Scan;
	FLOAT64				startTime = 0.0;
	FLOAT64				endTime = 0.0;
	FLOAT64				x0;
	FLOAT64				x1;
	INT32				isDiverged;
	INT32				isOpen = 0;
	INT32				nTarget;
	INT32				nManeuver;
	INT32				nIndex;
	INT32				nTrack;
	INT32				nPanel;

	for (nTarget = 0; nTarget < st_Config->nTargetNum; nTarget++)
	{
		for (nManeuver = 0; nManeuver < st_Config->st_Target[nTarget].nManeuverNum; nManeuver++)
		{
			x0 = f_TimeToX(st_Config->st_Target[nTarget].st_Maneuver[nManeuver].startTime);
			x1 = f_TimeToX(fmin(st_Config->st_Target[nTarget].st_Maneuver[nManeuver].endTime, timeMax));

			for (nPanel = 0; nPanel < 2; nPanel++)
			{
				(VOID)st_Graphics->FillRectangle(&st_Turn, static_cast<FLOAT32>(x0), static_cast<FLOAT32>(st_Panel[nPanel]->top), static_cast<FLOAT32>(x1 - x0),
					static_cast<FLOAT32>(st_Panel[nPanel]->Height()));
			}
		}
	}

	// 발산 판정이 이어진 스캔을 한 띠로 (마지막 스캔 뒤에 한 번 더 돌아 열린 띠를 닫음)
	for (nIndex = 0; nIndex <= (nLastIndex + 1); nIndex++)
	{
		isDiverged = 0;

		for (nTrack = 0; (nIndex <= nLastIndex) && (nTrack < st_Track->f_GetTrackNum(nIndex)); nTrack++)
		{
			isDiverged = (st_Track->f_GetTrack(nIndex, nTrack)->st_View.nGateOutRun >= TRK_DIVERGE_NUM) ? 1 : isDiverged;
		}

		if ((isDiverged != 0) && (isOpen == 0))
		{
			st_Scan		= st_Track->f_GetScan(nIndex);
			startTime	= st_Scan->time - halfStep;
			isOpen		= 1;
		}

		if (isDiverged != 0)
		{
			endTime = st_Track->f_GetScan(nIndex)->time + halfStep;
		}

		if ((isDiverged == 0) && (isOpen != 0))
		{
			x0		= f_TimeToX(startTime);
			x1		= f_TimeToX(endTime);
			isOpen	= 0;

			for (nPanel = 0; nPanel < 2; nPanel++)
			{
				(VOID)st_Graphics->FillRectangle(&st_Diverge, static_cast<FLOAT32>(x0), static_cast<FLOAT32>(st_Panel[nPanel]->top), static_cast<FLOAT32>(x1 - x0),
					static_cast<FLOAT32>(st_Panel[nPanel]->Height()));
			}
		}
	}
}

// 위: 위치 오차 (실선) 와 σ위치 (점선), 아래: NIS (점) 와 게이트 (빨간 점선). 지금 스캔까지, 지금 시각에 세로선
VOID CFilterPlot::f_Draw(Gdiplus::Graphics *st_Graphics) const
{
	static const Gdiplus::REAL	s_Dash[2] = { 4.0F, 3.0F };
	const FLOAT32				scale = static_cast<FLOAT32>(f_Ui_Scale(100, dpi)) / 100.0F;
	const Gdiplus::FontFamily	st_Family(L"Malgun Gothic");
	const Gdiplus::Font			st_Font(&st_Family, scale * PLOT_FONT_PX, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	const Gdiplus::Font			st_Bold(&st_Family, scale * PLOT_FONT_PX, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
	const INT32					nLastIndex = st_Track->f_FindLastUpTo(nCurScan);
	const FLOAT64				timeStep = f_Plot_NiceStepMeter(timeMax, fmax(2.0, fmin(12.0, static_cast<FLOAT64>(st_ErrorArea.Width()) / (PERF_TICK_X_PX * static_cast<FLOAT64>(scale)))));
	const FLOAT64				errorStep = f_Plot_NiceStepMeter(errorMax, 5.0);
	const FLOAT64				nowTime = (static_cast<FLOAT64>(std::min(std::max(nCurScan, 0), st_Result->f_GetSampleNum() - 1)) * st_Result->f_GetConfig()->stepTime);
	const FLOAT32				dotHalf = 1.3F * scale;
	const CRect					*st_Panel[2] = { &st_ErrorArea, &st_NisArea };
	Gdiplus::Pen				st_GridPen(f_Plot_Color(UI_COLOR_GRID_LINE, 255), 1.0F);
	Gdiplus::Pen				st_FramePen(f_Plot_Color(UI_COLOR_BORDER, 255), 1.0F);
	Gdiplus::Pen				st_GatePen(f_Plot_Color(PERF_COLOR_GATE, 255), 1.4F * scale);
	Gdiplus::Pen				st_NowPen(f_Plot_Color(UI_COLOR_TEXT_SUB, 200), 1.2F * scale);
	std::vector<ST_TrackLine>	st_ErrorLines;
	std::vector<ST_TrackLine>	st_StdLines;
	std::vector<Gdiplus::RectF>	st_NisDot;
	const ST_TrackItem			*st_Item;
	ST_TrackLine				st_New;
	FLOAT64						value;
	FLOAT64						x;
	FLOAT64						y;
	FLOAT64						sigma;
	CString						st_Text;
	INT32						nLine;
	INT32						nLineNum;
	INT32						nIndex;
	INT32						nTrack;
	INT32						nFound;
	INT32						nPanel;
	UINT64						nPick;

	(VOID)st_GatePen.SetDashPattern(s_Dash, 2);
	(VOID)st_Graphics->SetSmoothingMode(Gdiplus::SmoothingModeNone);

	f_Plot_Text(st_Graphics, &st_Bold, CString(_T("추적 성능 (참값 대비)  ·  회색 띠 = 선회, 빨간 띠 = 발산 판정 (게이트 밖 5 번 이상 이어짐)")),
		static_cast<FLOAT64>(st_ErrorArea.left), static_cast<FLOAT64>(st_ErrorArea.top) - 6.0, Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentFar, UI_COLOR_TEXT);

	f_DrawBands(st_Graphics, nLastIndex);

	// 시간 눈금 (두 그래프 모두), 글자는 각 그래프 아래
	nLineNum = static_cast<INT32>(floor((timeMax / timeStep) + 1.0e-9));

	for (nLine = 0; nLine <= nLineNum; nLine++)
	{
		value	= static_cast<FLOAT64>(nLine) * timeStep;
		x		= f_TimeToX(value);

		for (nPanel = 0; nPanel < 2; nPanel++)
		{
			(VOID)st_Graphics->DrawLine(&st_GridPen, f_Plot_Round(x), st_Panel[nPanel]->top, f_Plot_Round(x), st_Panel[nPanel]->bottom);
			st_Text.Format(_T("%.0f"), value);
			f_Plot_Text(st_Graphics, &st_Font, st_Text, x, static_cast<FLOAT64>(st_Panel[nPanel]->bottom) + 3.0, Gdiplus::StringAlignmentCenter,
				Gdiplus::StringAlignmentNear, UI_COLOR_TEXT_SUB);
		}
	}

	// 오차 눈금
	nLineNum = static_cast<INT32>(floor((errorMax / errorStep) + 1.0e-9));

	for (nLine = 0; nLine <= nLineNum; nLine++)
	{
		value	= static_cast<FLOAT64>(nLine) * errorStep;
		y		= f_ErrorToY(value);
		(VOID)st_Graphics->DrawLine(&st_GridPen, st_ErrorArea.left, f_Plot_Round(y), st_ErrorArea.right, f_Plot_Round(y));
		st_Text.Format(_T("%.0f"), value);
		f_Plot_Text(st_Graphics, &st_Font, st_Text, static_cast<FLOAT64>(st_ErrorArea.left) - 6.0, y, Gdiplus::StringAlignmentFar, Gdiplus::StringAlignmentCenter,
			UI_COLOR_TEXT_SUB);
	}

	// NIS 눈금 (10 의 거듭제곱)
	for (value = nisLogMin; value <= (nisLogMax + 1.0e-9); value = value + 1.0)
	{
		y = f_NisToY(pow(10.0, value));
		(VOID)st_Graphics->DrawLine(&st_GridPen, st_NisArea.left, f_Plot_Round(y), st_NisArea.right, f_Plot_Round(y));
		st_Text.Format(_T("%g"), pow(10.0, value));
		f_Plot_Text(st_Graphics, &st_Font, st_Text, static_cast<FLOAT64>(st_NisArea.left) - 6.0, y, Gdiplus::StringAlignmentFar, Gdiplus::StringAlignmentCenter,
			UI_COLOR_TEXT_SUB);
	}

	for (nPanel = 0; nPanel < 2; nPanel++)
	{
		(VOID)st_Graphics->DrawRectangle(&st_FramePen, st_Panel[nPanel]->left, st_Panel[nPanel]->top, st_Panel[nPanel]->Width(), st_Panel[nPanel]->Height());
		f_Plot_Text(st_Graphics, &st_Font, CString(_T("시간 [s]")), static_cast<FLOAT64>(st_Panel[nPanel]->right) - 6.0,
			static_cast<FLOAT64>(st_Panel[nPanel]->bottom) - 4.0, Gdiplus::StringAlignmentFar, Gdiplus::StringAlignmentFar, UI_COLOR_TEXT_OFF);
	}

	f_Plot_LabelText(st_Graphics, &st_Font, CString(_T("위치 오차 [m]: 실선 = |추정 - 참값|, 점선 = σ위치 (P 위치 대각 RSS)")),
		static_cast<FLOAT64>(st_ErrorArea.left) + 6.0, static_cast<FLOAT64>(st_ErrorArea.top) + 4.0, Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentNear,
		UI_COLOR_TEXT_SUB);
	f_Plot_LabelText(st_Graphics, &st_Font, CString(_T("NIS (로그 눈금): 점 = 잔차^T S^-1 잔차, 빨간 점선 = 게이트 11.34 (χ² 자유도 3, 99 %)")),
		static_cast<FLOAT64>(st_NisArea.left) + 6.0, static_cast<FLOAT64>(st_NisArea.top) + 4.0, Gdiplus::StringAlignmentNear, Gdiplus::StringAlignmentNear,
		UI_COLOR_TEXT_SUB);

	(VOID)st_Graphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

	// 게이트 선
	y = f_NisToY(TRK_GATE_CHI2);
	(VOID)st_Graphics->DrawLine(&st_GatePen, static_cast<FLOAT32>(st_NisArea.left), static_cast<FLOAT32>(y), static_cast<FLOAT32>(st_NisArea.right),
		static_cast<FLOAT32>(y));

	// 트랙마다 오차 · σ 선, NIS 점 (지금 스캔까지)
	for (nIndex = 0; nIndex <= nLastIndex; nIndex++)
	{
		for (nTrack = 0; nTrack < st_Track->f_GetTrackNum(nIndex); nTrack++)
		{
			st_Item	= st_Track->f_GetTrack(nIndex, nTrack);
			x		= f_TimeToX(st_Track->f_GetScan(nIndex)->time);
			sigma	= sqrt((st_Item->st_View.posStd[0] * st_Item->st_View.posStd[0]) + (st_Item->st_View.posStd[1] * st_Item->st_View.posStd[1]) +
						(st_Item->st_View.posStd[2] * st_Item->st_View.posStd[2]));
			nFound	= -1;

			for (nPick = 0; nPick < st_ErrorLines.size(); nPick++)
			{
				nFound = ((st_ErrorLines[nPick].TrackId == st_Item->st_View.TrackId) && (st_ErrorLines[nPick].nInitScan == st_Item->st_View.nInitScan))
					? static_cast<INT32>(nPick) : nFound;
			}

			if (nFound < 0)
			{
				st_New.TrackId		= st_Item->st_View.TrackId;
				st_New.nInitScan	= st_Item->st_View.nInitScan;
				st_New.color		= f_Plot_TrackColor(st_Track->f_GetColorIndex(st_New.TrackId));
				st_ErrorLines.push_back(st_New);
				st_StdLines.push_back(st_New);
				nFound				= static_cast<INT32>(st_ErrorLines.size()) - 1;
			}

			st_ErrorLines[static_cast<UINT64>(nFound)].st_Pixel.push_back(Gdiplus::PointF(static_cast<FLOAT32>(x), static_cast<FLOAT32>(f_ErrorToY(st_Item->errNorm))));
			st_StdLines[static_cast<UINT64>(nFound)].st_Pixel.push_back(Gdiplus::PointF(static_cast<FLOAT32>(x), static_cast<FLOAT32>(f_ErrorToY(sigma))));

			if (st_Item->st_View.nis >= 0.0)
			{
				y = f_NisToY(st_Item->st_View.nis);
				st_NisDot.push_back(Gdiplus::RectF(static_cast<FLOAT32>(x) - dotHalf, static_cast<FLOAT32>(y) - dotHalf, 2.0F * dotHalf, 2.0F * dotHalf));
			}
		}
	}

	{
		const Gdiplus::Rect st_Clip(st_ErrorArea.left, st_ErrorArea.top, st_ErrorArea.Width(), st_ErrorArea.Height());

		(VOID)st_Graphics->SetClip(st_Clip);

		for (nPick = 0; nPick < st_ErrorLines.size(); nPick++)
		{
			Gdiplus::Pen st_ErrorPen(f_Plot_Color(st_ErrorLines[nPick].color, 255), 1.4F * scale);
			Gdiplus::Pen st_StdPen(f_Plot_Color(st_ErrorLines[nPick].color, 170), 1.2F * scale);

			(VOID)st_StdPen.SetDashPattern(s_Dash, 2);

			if (st_ErrorLines[nPick].st_Pixel.size() >= 2U)
			{
				(VOID)st_Graphics->DrawLines(&st_StdPen, st_StdLines[nPick].st_Pixel.data(), static_cast<INT32>(st_StdLines[nPick].st_Pixel.size()));
				(VOID)st_Graphics->DrawLines(&st_ErrorPen, st_ErrorLines[nPick].st_Pixel.data(), static_cast<INT32>(st_ErrorLines[nPick].st_Pixel.size()));
			}
		}

		(VOID)st_Graphics->ResetClip();
	}

	f_Plot_FillDots(st_Graphics, (st_ErrorLines.empty() == false) ? st_ErrorLines[0].color : f_Plot_TrackColor(0), st_NisDot);

	// 지금 시각
	x = f_TimeToX(nowTime);

	for (nPanel = 0; nPanel < 2; nPanel++)
	{
		(VOID)st_Graphics->DrawLine(&st_NowPen, static_cast<FLOAT32>(x), static_cast<FLOAT32>(st_Panel[nPanel]->top), static_cast<FLOAT32>(x),
			static_cast<FLOAT32>(st_Panel[nPanel]->bottom));
	}
}
