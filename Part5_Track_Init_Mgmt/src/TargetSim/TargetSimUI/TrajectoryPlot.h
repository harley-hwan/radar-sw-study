// 결과 그림: 2D, 3D, 트랙 수명
#pragma once

#include "PlotResult.h"
#include "Scenario.h"

namespace Gdiplus
{
	class Graphics;
	class Font;
}

// 2D 그림: 동-북 평면 (위경도 격자) 에 참값 · 플롯 · 클러터 · 트랙. 휠 확대, 끌어 이동, 두 번 클릭 = 처음 보기
class CTrajectoryPlot : public CStatic
{
public:
	CTrajectoryPlot() noexcept;

	VOID	f_SetDpi(INT32 newDpi);
	VOID	f_SetData(const CSimResult *st_NewResult, const CPlotResult *st_NewMeasure, const CTrackResult *st_NewTrack);
	VOID	f_SetShow(INT32 isNewShowTrue, INT32 isNewShowPlot, INT32 isNewShowClutter, INT32 isNewShowTrack);
	VOID	f_SetScan(INT32 nNewScan);
	VOID	f_Zoom(INT32 delta, CPoint st_ScreenPoint);

protected:
	afx_msg VOID	OnPaint(VOID);
	afx_msg BOOL	OnEraseBkgnd(CDC *st_Dc);
	afx_msg VOID	OnSize(UINT32 type, INT32 width, INT32 height);
	afx_msg VOID	OnLButtonDown(UINT32 flags, CPoint st_Point);
	afx_msg VOID	OnLButtonUp(UINT32 flags, CPoint st_Point);
	afx_msg VOID	OnLButtonDblClk(UINT32 flags, CPoint st_Point);
	afx_msg VOID	OnMouseMove(UINT32 flags, CPoint st_Point);
	afx_msg VOID	OnCaptureChanged(CWnd *st_Wnd);

	DECLARE_MESSAGE_MAP()

private:
	VOID				f_ComputeLayout(const CRect &st_Client);
	VOID				f_ComputeView(VOID);
	VOID				f_DrawMap(Gdiplus::Graphics *st_Graphics) const;
	VOID				f_DrawStartLabels(Gdiplus::Graphics *st_Graphics, const Gdiplus::Font *st_Font, INT32 nStartNum) const;
	VOID				f_DrawMapTruth(Gdiplus::Graphics *st_Graphics) const;
	VOID				f_DrawMapHistory(Gdiplus::Graphics *st_Graphics) const;
	VOID				f_DrawMapScan(Gdiplus::Graphics *st_Graphics, const Gdiplus::Font *st_Font) const;
	VOID				f_DrawMapLegend(Gdiplus::Graphics *st_Graphics, const Gdiplus::Font *st_Font) const;
	VOID				f_MapToPixel(const ST_PlotPoint *st_Point, FLOAT64 *pt_X, FLOAT64 *pt_Y) const;
	STRUCT_Coord_Lla	f_PlotToLla(FLOAT64 east, FLOAT64 north) const;
	ST_PlotPoint		f_LlaToPlot(FLOAT64 lat, FLOAT64 lon) const;

	const CSimResult	*st_Result;						// 궤적 계산 결과 (참값). 셋 중 하나라도 없으면 그리지 않음
	const CPlotResult	*st_Measure;					// 측정 모의 결과 (플롯, 클러터)
	const CTrackResult	*st_Track;						// 트랙 결과 (받은 플롯, 트랙)
	INT32				isShowTrue;						// 1 이면 참값 궤적, 지금 위치, 표적 시작점 표시
	INT32				isShowPlot;
	INT32				isShowClutter;
	INT32				isShowTrack;
	INT32				nCurScan;						// 지금 보이는 스캔 번호
	INT32				dpi;

	FLOAT64				pixelPerMeter;
	FLOAT64				centerEast;
	FLOAT64				centerNorth;
	FLOAT64				centerLat;						// 보이는 영역 중심 [rad]
	FLOAT64				centerLon;
	FLOAT64				gridLatDeg;						// 격자 간격 [deg]
	FLOAT64				gridLonDeg;
	FLOAT64				zoom;							// 처음 보기 대비 확대 배율
	FLOAT64				panEast;						// 처음 보기 중심에서 옮긴 양 [m]
	FLOAT64				panNorth;
	INT32				isDragging;
	CPoint				st_DragStart;
	FLOAT64				dragPanEast;
	FLOAT64				dragPanNorth;

	CRect				st_MapArea;
};

// 3D 그림: 동-북-위 정투영 (고도는 1 ~ 5 배로 늘림). 끌어 회전, 휠 확대, 두 번 클릭 = 처음 보기
class CTrackView3D : public CStatic
{
public:
	CTrackView3D() noexcept;

	VOID	f_SetDpi(INT32 newDpi);
	VOID	f_SetData(const CSimResult *st_NewResult, const CPlotResult *st_NewMeasure, const CTrackResult *st_NewTrack);
	VOID	f_SetShow(INT32 isNewShowTrue, INT32 isNewShowPlot, INT32 isNewShowClutter, INT32 isNewShowTrack);
	VOID	f_SetScan(INT32 nNewScan);
	VOID	f_SetLegend(INT32 isNewShowLegend);
	VOID	f_Zoom(INT32 delta, CPoint st_ScreenPoint);

protected:
	afx_msg VOID	OnPaint(VOID);
	afx_msg BOOL	OnEraseBkgnd(CDC *st_Dc);
	afx_msg VOID	OnSize(UINT32 type, INT32 width, INT32 height);
	afx_msg VOID	OnLButtonDown(UINT32 flags, CPoint st_Point);
	afx_msg VOID	OnLButtonUp(UINT32 flags, CPoint st_Point);
	afx_msg VOID	OnLButtonDblClk(UINT32 flags, CPoint st_Point);
	afx_msg VOID	OnMouseMove(UINT32 flags, CPoint st_Point);
	afx_msg VOID	OnCaptureChanged(CWnd *st_Wnd);

	DECLARE_MESSAGE_MAP()

private:
	VOID	f_ComputeView(const CRect &st_Client);
	VOID	f_Project(const ST_PlotPoint *st_Point, FLOAT64 *pt_X, FLOAT64 *pt_Y) const;
	VOID	f_DrawScene(Gdiplus::Graphics *st_Graphics) const;
	VOID	f_DrawGround(Gdiplus::Graphics *st_Graphics, const Gdiplus::Font *st_Font) const;
	INT32	f_IsInBox(const ST_PlotPoint *st_Point) const;

	const CSimResult	*st_Result;
	const CPlotResult	*st_Measure;
	const CTrackResult	*st_Track;
	INT32				isShowTrue;
	INT32				isShowPlot;
	INT32				isShowClutter;
	INT32				isShowTrack;
	INT32				nCurScan;
	INT32				dpi;

	FLOAT64				viewYaw;						// 바라보는 방위 [rad]. 0 이면 남쪽에서 북쪽을 봄
	FLOAT64				viewPitch;						// 내려다보는 각 [rad]. 90 도면 위에서 똑바로 봄 (2D 와 같음)
	FLOAT64				zoom;
	FLOAT64				pixelPerMeter;
	FLOAT64				centerEast;
	FLOAT64				centerNorth;
	FLOAT64				centerUp;
	FLOAT64				minEast;						// 바닥 격자 범위 [m]
	FLOAT64				maxEast;
	FLOAT64				minNorth;
	FLOAT64				maxNorth;
	FLOAT64				maxUp;
	FLOAT64				upScale;						// 고도를 늘려 그리는 배율
	FLOAT64				gridStep;						// 바닥 격자 간격 [m]
	INT32				isShowLegend;
	INT32				isDragging;
	CPoint				st_DragStart;
	FLOAT64				dragYaw;
	FLOAT64				dragPitch;
	CRect				st_Area;
};

// 트랙 수명 그림의 줄 하나 (트랙은 개시 스캔 + ID 로 구분)
typedef struct
{
	UINT32				TrackId;
	INT32				nInitScan;
	INT32				nSource;						// 개시 플롯 참값 (ID 글자 색)
	INT32				nFirstScan;						// 가로 구간 안에서 처음 · 마지막으로 트랙 표에 있던 스캔
	INT32				nLastScan;
	INT32				isInTable;						// 1 이면 지금 스캔의 트랙 표에 있음 (ID 를 굵게)
} ST_LifeRow;

// 트랙 수명 그림의 칸 하나 (트랙 하나의 스캔 하나)
typedef struct
{
	INT32				nRow;							// st_RowBuf 자리
	INT32				nScan;
	INT32				nState;							// 그 스캔을 처리한 뒤의 TrackState
} ST_LifeCell;

// 트랙 수명 그림: 트랙마다 한 줄, 스캔마다 한 칸 (칸 색 = TrackState). 가로는 지금 스캔에서 끝나는 nSpan 스캔
// 휠로 구간 길이, 두 번 클릭 = 처음 길이
class CTrackLifeView : public CStatic
{
public:
	CTrackLifeView() noexcept;

	VOID	f_SetDpi(INT32 newDpi);
	VOID	f_SetData(const CTrackResult *st_NewTrack);
	VOID	f_SetScan(INT32 nNewScan);
	VOID	f_Zoom(INT32 delta, CPoint st_ScreenPoint);

protected:
	afx_msg VOID	OnPaint(VOID);
	afx_msg BOOL	OnEraseBkgnd(CDC *st_Dc);
	afx_msg VOID	OnSize(UINT32 type, INT32 width, INT32 height);
	afx_msg VOID	OnLButtonDblClk(UINT32 flags, CPoint st_Point);

	DECLARE_MESSAGE_MAP()

private:
	VOID	f_CollectRows(VOID);
	VOID	f_DrawLife(Gdiplus::Graphics *st_Graphics) const;
	VOID	f_DrawLegend(Gdiplus::Graphics *st_Graphics, const Gdiplus::Font *st_Font, FLOAT64 baseY, FLOAT64 minLeft) const;
	FLOAT64	f_ScanToX(INT32 nScan) const;

	const CTrackResult			*st_Track;
	INT32						nCurScan;
	INT32						dpi;
	INT32						nSpan;							// 가로 구간 길이 [스캔]
	INT32						nFirstScan;						// 가로 구간 첫 스캔
	std::vector<ST_LifeRow>		st_RowBuf;						// 구간에 트랙 표에 있던 트랙. 개시 순서
	std::vector<ST_LifeCell>	st_CellBuf;
	CRect						st_Area;						// 칸을 그리는 영역
};
