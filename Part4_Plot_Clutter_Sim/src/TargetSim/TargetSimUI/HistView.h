#pragma once

#include "PlotSim.h"

// 난수 히스토그램 한 칸. 막대는 표본 PDF, 선은 이론 PDF (균등 1, 정규 표준정규 곡선). 자료는 대화상자 것을 가리킴.
class CHistView : public CStatic
{
public:
	VOID	f_SetHist(const ST_PcsHist *st_NewHist, INT32 isNewGauss);

protected:
	afx_msg VOID	OnPaint(VOID);
	afx_msg BOOL	OnEraseBkgnd(CDC *st_Dc);

	DECLARE_MESSAGE_MAP()

private:
	VOID	f_Draw(CDC *st_Dc, const CRect &st_Area) const;

	const ST_PcsHist	*st_Hist = nullptr;
	INT32				isGauss = 0;
};
