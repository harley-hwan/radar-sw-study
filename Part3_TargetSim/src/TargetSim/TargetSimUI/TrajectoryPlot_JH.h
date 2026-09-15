#pragma once

#include "TargetSim_JH.h"

// 플랫폼 표본 0 을 원점으로 한 국지 ENU 수평 좌표 [m]
typedef struct
{
	FLOAT64		east;
	FLOAT64		north;
} ST_PlotPoint;

// 궤적 그림. 표본·점 버퍼는 대화상자가 소유하고 여기서는 읽기만 한다.
class CTrajectoryPlot : public CStatic
{
public:
	CTrajectoryPlot() noexcept;

	// st_NewPoint 는 [표본 * 객체 수 + 객체] 순서, 객체 0 은 플랫폼
	VOID	f_SetData(const ST_PlotPoint *st_NewPoint, const ST_SimSample *st_NewSample, INT32 nNewSampleNum, INT32 nNewObjectNum);
	VOID	f_SetStep(INT32 nStep);
	VOID	f_Clear(VOID);

protected:
	afx_msg VOID	OnPaint(VOID);
	afx_msg BOOL	OnEraseBkgnd(CDC *st_Dc);

	DECLARE_MESSAGE_MAP()

private:
	VOID	f_ComputeView(const CRect &st_Client);
	VOID	f_BuildBackground(CDC *st_Dc, const CRect &st_Client);
	VOID	f_DrawGrid(CDC *st_Dc) const;
	VOID	f_DrawTrack(CDC *st_Dc) const;
	VOID	f_DrawLegend(CDC *st_Dc) const;
	VOID	f_DrawOverlay(CDC *st_Dc) const;
	POINT	f_ToPixel(const ST_PlotPoint *st_Point) const;

	const ST_PlotPoint	*st_PointBuf;
	const ST_SimSample	*st_SampleBuf;
	INT32				nSampleNum;
	INT32				nObjectNum;
	INT32				nCurStep;
	INT32				isBackValid;
	FLOAT64				pixelPerMeter;
	FLOAT64				centerEast;
	FLOAT64				centerNorth;
	FLOAT64				gridMeter;
	CRect				st_Area;
	CSize				st_FrameSize;
	CBitmap				st_BackBitmap;
	CBitmap				st_FrameBitmap;
};
