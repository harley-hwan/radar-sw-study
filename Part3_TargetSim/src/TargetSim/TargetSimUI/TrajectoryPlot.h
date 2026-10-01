#pragma once

#include "Scenario.h"

namespace Gdiplus
{
	class Graphics;
	class Font;
}

// 시각 그래프 칸
#define PLOT_PANEL_ALT			0						// 고도-시각
#define PLOT_PANEL_SPEED		1						// 속력-시각
#define PLOT_PANEL_NUM			2

// 시각 그래프 칸 하나.
struct ST_PlotPanel
{
	LPCTSTR				pt_Title = _T("");
	FLOAT64				minSpan = 1.0;						// 세로축 최소 폭
	FLOAT64				valueMin = 0.0;						// 세로축 범위
	FLOAT64				valueMax = 1.0;
	FLOAT64				valueGrid = 1.0;					// 세로축 눈금 간격
	CRect				st_Area = CRect(0, 0, 0, 0);
};

// 궤적 그림: 플랫폼 기준 동-북 평면 + 고도-시각, 속력-시각. 결과는 대화상자 소유.
// 비교 결과 (플랫폼 기준 계산) 를 주면 같은 색을 어둡게 한 점선으로 겹쳐 그림.
class CTrajectoryPlot : public CStatic
{
public:
	CTrajectoryPlot() noexcept;

	VOID	f_SetDpi(INT32 newDpi);
	VOID	f_SetResult(const CSimResult *st_NewResult, const CSimResult *st_NewCompare);

protected:
	afx_msg VOID	OnPaint(VOID);
	afx_msg BOOL	OnEraseBkgnd(CDC *st_Dc);
	afx_msg VOID	OnSize(UINT32 type, INT32 width, INT32 height);

	DECLARE_MESSAGE_MAP()

private:
	VOID				f_ComputeLayout(const CRect &st_Client);
	VOID				f_ComputeView(VOID);
	VOID				f_DrawMap(Gdiplus::Graphics *st_Graphics) const;
	VOID				f_DrawStartLabels(Gdiplus::Graphics *st_Graphics, const Gdiplus::Font *st_Font) const;
	VOID				f_DrawPanel(Gdiplus::Graphics *st_Graphics, INT32 nPanel) const;
	VOID				f_DrawMapTrack(Gdiplus::Graphics *st_Graphics, const CSimResult *st_Source, INT32 isCompare) const;
	VOID				f_DrawPanelTrack(Gdiplus::Graphics *st_Graphics, INT32 nPanel, const CSimResult *st_Source, INT32 isCompare) const;
	VOID				f_MapToPixel(const ST_PlotPoint *st_Point, FLOAT64 *pt_X, FLOAT64 *pt_Y) const;
	STRUCT_Coord_Lla	f_PlotToLla(FLOAT64 east, FLOAT64 north) const;
	ST_PlotPoint		f_LlaToPlot(FLOAT64 lat, FLOAT64 lon) const;
	FLOAT64				f_PanelValue(const CSimResult *st_Source, INT32 nPanel, INT32 nStep, INT32 nObject) const;
	VOID				f_PanelToPixel(INT32 nPanel, INT32 nStep, FLOAT64 value, FLOAT64 *pt_X, FLOAT64 *pt_Y) const;

	const CSimResult	*st_Result;
	const CSimResult	*st_Compare;					// 비교 결과 (플랫폼 기준). 없으면 nullptr
	INT32				dpi;

	FLOAT64				pixelPerMeter;
	FLOAT64				centerEast;
	FLOAT64				centerNorth;
	FLOAT64				centerLat;						// 보이는 영역 중심 [rad]
	FLOAT64				centerLon;
	FLOAT64				gridLatDeg;						// 격자 간격 [deg]
	FLOAT64				gridLonDeg;
	FLOAT64				timeGrid;

	CRect				st_MapArea;
	ST_PlotPanel		st_Panel[PLOT_PANEL_NUM];
};
