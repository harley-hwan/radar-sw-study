#pragma once

#include "UiCommon.h"

namespace Gdiplus
{
	class Graphics;
	class RectF;
}

#define CHART_VIEW_RANDOM		0									// 난수 히스토그램
#define CHART_VIEW_MAP			1									// LLA 전시

// 결과 그림. 자료는 대화상자 것을 읽기만 함.
class CChartView : public CStatic
{
public:
	CChartView() noexcept;

	VOID	f_SetDpi(INT32 newDpi);
	VOID	f_SetData(const ST_ChartData *st_NewData);
	VOID	f_SetView(INT32 nNewView);

protected:
	afx_msg VOID	OnPaint(VOID);
	afx_msg BOOL	OnEraseBkgnd(CDC *st_Dc);
	afx_msg VOID	OnSize(UINT32 type, INT32 width, INT32 height);

	DECLARE_MESSAGE_MAP()

private:
	VOID	f_DrawRandom(Gdiplus::Graphics *st_Graphics, const Gdiplus::RectF &st_Area) const;
	VOID	f_DrawHist(Gdiplus::Graphics *st_Graphics, const Gdiplus::RectF &st_Area, const ST_PcsHist *st_Hist, LPCTSTR pt_Title,
				INT32 isGauss) const;
	VOID	f_DrawMap(Gdiplus::Graphics *st_Graphics, const Gdiplus::RectF &st_Area) const;
	VOID	f_DrawMessage(Gdiplus::Graphics *st_Graphics, const Gdiplus::RectF &st_Area, LPCTSTR pt_Text) const;
	FLOAT32	f_Px(FLOAT32 pixel) const;

	const ST_ChartData	*st_Data;
	INT32				dpi;
	INT32				nView;
};
