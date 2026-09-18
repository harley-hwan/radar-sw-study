#pragma once

#include "Scenario.h"

// 부모에게 보내는 WM_NOTIFY 코드
#define PLOTN_SELECT			2101U					// 시작점을 눌렀다 (nObject)
#define PLOTN_MOVE				2102U					// 시작점을 끌고 있다 (nObject, latDeg, lonDeg)
#define PLOTN_TURN				2103U					// 기수 손잡이를 끌고 있다 (nObject, yawDeg)
#define PLOTN_SCRUB				2104U					// 고도 패널에서 시각을 짚었다 (nStep)

typedef struct
{
	NMHDR				st_Hdr;
	INT32				nObject;
	INT32				nStep;
	FLOAT64				latDeg;
	FLOAT64				lonDeg;
	FLOAT64				yawDeg;
} ST_PlotNotify;

namespace Gdiplus
{
	class Graphics;
	class Font;
}

// 궤적 그림: 플랫폼 기준 동-북 평면 + 고도-시각. 결과는 대화상자가 소유한다.
class CTrajectoryPlot : public CStatic
{
public:
	CTrajectoryPlot() noexcept;

	VOID	f_SetDpi(INT32 newDpi);
	VOID	f_SetResult(const CSimResult *st_NewResult);
	VOID	f_SetStep(INT32 nStep);
	VOID	f_SetSelObject(INT32 nObject);

protected:
	afx_msg VOID	OnPaint(VOID);
	afx_msg BOOL	OnEraseBkgnd(CDC *st_Dc);
	afx_msg VOID	OnSize(UINT32 type, INT32 width, INT32 height);
	afx_msg VOID	OnLButtonDown(UINT32 flags, CPoint st_Point);
	afx_msg VOID	OnLButtonUp(UINT32 flags, CPoint st_Point);
	afx_msg VOID	OnMouseMove(UINT32 flags, CPoint st_Point);
	afx_msg VOID	OnCaptureChanged(CWnd *st_NewWnd);
	afx_msg BOOL	OnSetCursor(CWnd *st_Wnd, UINT32 hitTest, UINT32 message);

	DECLARE_MESSAGE_MAP()

private:
	INT32	f_HasData(VOID) const;
	VOID	f_ComputeLayout(const CRect &st_Client);
	VOID	f_ComputeView(VOID);
	VOID	f_BuildBackground(CDC *st_Dc, const CRect &st_Client);
	VOID	f_DrawMap(Gdiplus::Graphics *st_Graphics) const;
	VOID	f_DrawStartLabels(Gdiplus::Graphics *st_Graphics, const Gdiplus::Font *st_Font) const;
	VOID	f_DrawAltitude(Gdiplus::Graphics *st_Graphics) const;
	VOID	f_DrawOverlay(Gdiplus::Graphics *st_Graphics) const;
	VOID	f_DrawEmpty(CDC *st_Dc, const CRect &st_Client) const;
	VOID	f_MapToPixel(const ST_PlotPoint *st_Point, FLOAT64 *pt_X, FLOAT64 *pt_Y) const;
	VOID	f_AltToPixel(INT32 nStep, FLOAT64 alt, FLOAT64 *pt_X, FLOAT64 *pt_Y) const;
	INT32	f_GetHandlePixel(INT32 nObject, FLOAT64 *pt_X, FLOAT64 *pt_Y) const;
	INT32	f_HitTest(CPoint st_Point, INT32 *pt_Object) const;
	VOID	f_DragTo(CPoint st_Point);
	VOID	f_Notify(UINT32 code, const ST_PlotNotify *st_Source);

	const CSimResult	*st_Result;
	INT32				dpi;
	INT32				nCurStep;
	INT32				nSelObject;
	INT32				isBackValid;
	INT32				isViewFrozen;					// 끄는 동안 축척 고정
	INT32				dragKind;
	INT32				nDragObject;

	FLOAT64				pixelPerMeter;
	FLOAT64				centerEast;
	FLOAT64				centerNorth;
	FLOAT64				gridMeter;
	FLOAT64				altMin;
	FLOAT64				altMax;
	FLOAT64				altGrid;
	FLOAT64				timeGrid;

	CRect				st_MapArea;
	CRect				st_AltArea;
	CSize				st_FrameSize;
	CPoint				st_DragOffset;					// 누른 점과 표식 중심의 차
	CPoint				st_DownPoint;
	CBitmap				st_BackBitmap;
	CBitmap				st_FrameBitmap;
};
