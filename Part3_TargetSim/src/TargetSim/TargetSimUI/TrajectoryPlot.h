#pragma once

#include "Scenario.h"

namespace Gdiplus
{
	class Graphics;
	class Font;
}

// 궤적 그림: 플랫폼 기준 동-북 평면 + 고도-시각. 결과는 대화상자 소유.
class CTrajectoryPlot : public CStatic
{
public:
	CTrajectoryPlot() noexcept;

	VOID	f_SetDpi(INT32 newDpi);
	VOID	f_SetResult(const CSimResult *st_NewResult);

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
	VOID				f_DrawAltitude(Gdiplus::Graphics *st_Graphics) const;
	VOID				f_MapToPixel(const ST_PlotPoint *st_Point, FLOAT64 *pt_X, FLOAT64 *pt_Y) const;
	STRUCT_Coord_Lla	f_PlotToLla(FLOAT64 east, FLOAT64 north) const;
	ST_PlotPoint		f_LlaToPlot(FLOAT64 lat, FLOAT64 lon) const;
	VOID				f_AltToPixel(INT32 nStep, FLOAT64 alt, FLOAT64 *pt_X, FLOAT64 *pt_Y) const;

	const CSimResult	*st_Result;
	INT32				dpi;

	FLOAT64				pixelPerMeter;
	FLOAT64				centerEast;
	FLOAT64				centerNorth;
	FLOAT64				centerLat;						// 보이는 영역 중심 [rad]
	FLOAT64				centerLon;
	FLOAT64				gridLatDeg;						// 격자 간격 [deg]
	FLOAT64				gridLonDeg;
	FLOAT64				altMin;
	FLOAT64				altMax;
	FLOAT64				altGrid;
	FLOAT64				timeGrid;

	CRect				st_MapArea;
	CRect				st_AltArea;
};
