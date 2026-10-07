#pragma once

#include "PlotSim.h"

// 결과 그림 한 칸. LLA 지도 또는 난수 히스토그램을 그림. 자료는 대화상자 것을 읽기만 함.
class CChartView : public CStatic
{
public:
	VOID	f_SetMap(const ST_PcsScan *st_NewScan);
	VOID	f_SetHist(const ST_PcsHist *st_NewHist, INT32 isNewGauss);

protected:
	afx_msg VOID	OnPaint(VOID);
	afx_msg BOOL	OnEraseBkgnd(CDC *st_Dc);

	DECLARE_MESSAGE_MAP()

private:
	VOID	f_DrawMap(CDC *st_Dc, const CRect &st_Area) const;
	VOID	f_DrawHist(CDC *st_Dc, const CRect &st_Area) const;

	const ST_PcsScan	*st_Scan = nullptr;					// PCS_SCAN_NUM 개
	const ST_PcsHist	*st_Hist = nullptr;
	INT32				isGauss = 0;
};
