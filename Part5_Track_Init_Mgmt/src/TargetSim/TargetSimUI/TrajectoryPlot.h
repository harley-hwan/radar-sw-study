//
// @file	TrajectoryPlot.h
// @brief	결과 그림: 2D (동-북 평면) 와 3D (동-북-위 정투영) 에 True · Plot · Clutter · Track 심볼을 함께 그림, 트랙 수명 (트랙마다 한 줄, 스캔마다 한 칸)
// @author	hwan
// @date	2026.10.21.
//
#pragma once

#include "PlotResult.h"
#include "Scenario.h"

namespace Gdiplus
{
	class Graphics;
	class Font;
}

// 2D 그림: 플랫폼 초기 위치 기준 동-북 평면 (위경도 격자) 에 True · Plot · Track 심볼을 함께 그림. 결과는 대화상자 소유.
// 지금 스캔까지의 플롯 (검은 점) · 클러터 (회색 점) 를 옅게 깔고 참값 궤적을 그린 뒤, 지금 스캔의 참값 위치, 받은 플롯, 트랙 심볼을 위에 그림.
// 참값을 숨기면 플랫폼 (레이다) 시작점만 기준으로 표시. 축척은 그대로 둬 켜고 끈 그림을 바로 비교할 수 있게 함.
// 휠로 확대 · 축소, 끌어서 이동, 두 번 클릭하면 처음 보기.
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

// 3D 그림: 플랫폼 초기 위치 기준 동-북-위 공간을 비스듬히 내려다본 정투영. 고도는 바닥 폭에 맞춰 1 ~ 5 배로 늘려 그림.
// 장면 상자는 바닥을 참값 궤적과 보이는 표적 플롯, 고도를 가장 높은 참값의 두 배로 잡고 (대공 표적이 가운데 높이), 클러터는 상자 안 것만 그림.
// 참값 궤적 (바닥 그림자 포함), 지금 스캔까지의 플롯 (옅게), 지금 스캔의 참값 위치 · 받은 플롯 · 트랙 심볼을 2D 와 같은 기호로 그림.
// 끌면 회전 (좌우 = 방위, 위아래 = 내려다보는 각), 휠로 확대 · 축소, 두 번 클릭하면 처음 보기.
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

// 트랙 수명 그림의 줄 하나 (트랙 하나). 트랙은 (개시 스캔, ID) 로 구분함 (ID 는 10000 뒤 100 부터 다시 씀)
typedef struct
{
	UINT32				TrackId;
	INT32				nInitScan;
	INT32				nSource;						// 개시 플롯 두 개의 참값 (ST_TrackItem 과 같음). ID 글자 색
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

// 트랙 수명 그림: 트랙마다 한 줄, 스캔마다 한 칸 (칸 색 = 그 스캔을 처리한 뒤의 TrackState). 결과는 대화상자 소유, 받은 트랙 표만 읽음.
// 가로는 지금 스캔에서 끝나는 nSpan 스캔 (지금 스캔이 그보다 앞이면 스캔 0 부터 채워 나감), 세로는 그 구간에 트랙 표에 있던 트랙을
// 개시 순서대로 위에서 아래로. ID 는 첫 칸 왼쪽에 개시 플롯의 참값 색으로 씀 (표적 색, 클러터끼리 회색, 섞임 검정).
// 지금 스캔은 세로 띠로, 지금 트랙 표에 있는 트랙의 ID 는 굵게 보임. 휠로 구간 길이 바꾸기, 두 번 클릭하면 처음 길이.
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
