#pragma once

#include "PlotSim.h"

// 난수 히스토그램 한 칸. 막대는 표본 PDF, 선은 이론 PDF (균등 1, 정규 표준정규 곡선). 자료는 대화상자 것을 가리킴.
// 계산 예로 든 칸 (nMarkBin) 은 막대 색을 달리해 표의 색칠한 줄과 이어 보이게 함.
class CHistView : public CStatic
{
public:
	VOID	f_SetHist(const ST_PcsHist *st_NewHist, INT32 isNewGauss, INT32 nNewMarkBin);

protected:
	afx_msg VOID	OnPaint(VOID);
	afx_msg BOOL	OnEraseBkgnd(CDC *st_Dc);

	DECLARE_MESSAGE_MAP()

private:
	VOID	f_Draw(CDC *st_Dc, const CRect &st_Area) const;

	const ST_PcsHist	*st_Hist = nullptr;
	INT32				isGauss = 0;
	INT32				nMarkBin = -1;					// 색을 달리할 칸. 없으면 -1
};
