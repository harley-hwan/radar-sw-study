// 결과 그림: 평면 (위도-경도), 3D (경도-위도-고도), 2D 단면 (위도-고도, 경도-고도), 추적 성능 (오차 · NIS)
#pragma once

#include "PlotResult.h"
#include "Scenario.h"

namespace Gdiplus
{
	class Graphics;
	class Font;
}

#define PROFILE_LAT				0						// 2D 단면 가로축: 위도
#define PROFILE_LON				1						// 경도

// 평면 그림: 위도-경도 (격자 = 위경도) 에 참값 · 플롯 · 트랙. 휠 확대, 끌어 이동, 두 번 클릭 = 처음 보기
class CTrajectoryPlot : public CStatic
{
public:
	CTrajectoryPlot() noexcept;

	VOID	f_SetDpi(INT32 newDpi);
	VOID	f_SetData(const CSimResult *st_NewResult, const CPlotResult *st_NewMeasure, const CTrackResult *st_NewTrack);
	VOID	f_SetShow(INT32 isNewShowTrue, INT32 isNewShowPlot, INT32 isNewShowTrack);
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
	VOID	f_ComputeLayout(const CRect &st_Client);
	VOID	f_ComputeView(VOID);
	VOID	f_DrawMap(Gdiplus::Graphics *st_Graphics) const;
	VOID	f_DrawStartLabels(Gdiplus::Graphics *st_Graphics, const Gdiplus::Font *st_Font) const;
	VOID	f_MapToPixel(const ST_PlotPoint *st_Point, FLOAT64 *pt_X, FLOAT64 *pt_Y) const;

	const CSimResult	*st_Result;						// 궤적 계산 결과 (참값). 셋 중 하나라도 없으면 그리지 않음
	const CPlotResult	*st_Measure;					// 측정 모의 결과 (플롯)
	const CTrackResult	*st_Track;						// 추적 결과 (받은 플롯, 트랙)
	INT32				isShowTrue;						// 1 이면 참값 궤적, 지금 위치, 시작점 표시
	INT32				isShowPlot;
	INT32				isShowTrack;
	INT32				nCurScan;						// 지금 보이는 스캔 번호
	INT32				dpi;

	FLOAT64				pixelPerMeter;
	FLOAT64				centerEast;
	FLOAT64				centerNorth;
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

// 3D 그림: 경도-위도-고도 정투영 (바닥 격자 = 위경도, 고도는 1 ~ 5 배로 늘림). 끌어 회전, 휠 확대, 두 번 클릭 = 처음 보기
class CTrackView3D : public CStatic
{
public:
	CTrackView3D() noexcept;

	VOID	f_SetDpi(INT32 newDpi);
	VOID	f_SetData(const CSimResult *st_NewResult, const CPlotResult *st_NewMeasure, const CTrackResult *st_NewTrack);
	VOID	f_SetShow(INT32 isNewShowTrue, INT32 isNewShowPlot, INT32 isNewShowTrack);
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

	const CSimResult	*st_Result;
	const CPlotResult	*st_Measure;
	const CTrackResult	*st_Track;
	INT32				isShowTrue;
	INT32				isShowPlot;
	INT32				isShowTrack;
	INT32				nCurScan;
	INT32				dpi;

	FLOAT64				viewYaw;						// 바라보는 방위 [rad]. 0 이면 남쪽에서 북쪽을 봄
	FLOAT64				viewPitch;						// 내려다보는 각 [rad]. 90 도면 위에서 똑바로 봄
	FLOAT64				zoom;
	FLOAT64				pixelPerMeter;
	FLOAT64				centerEast;
	FLOAT64				centerNorth;
	FLOAT64				centerUp;
	FLOAT64				gridLatDeg;						// 바닥 격자 간격 [deg]
	FLOAT64				gridLonDeg;
	FLOAT64				minLat;							// 바닥 격자 범위 [deg] (격자 간격에 맞춤)
	FLOAT64				maxLat;
	FLOAT64				minLon;
	FLOAT64				maxLon;
	FLOAT64				minUp;							// 고도 축 범위 [m] (0 포함)
	FLOAT64				maxUp;
	FLOAT64				upStep;							// 고도 눈금 간격 [m]
	FLOAT64				upScale;						// 고도를 늘려 그리는 배율
	INT32				isShowLegend;
	INT32				isDragging;
	CPoint				st_DragStart;
	FLOAT64				dragYaw;
	FLOAT64				dragPitch;
	CRect				st_Area;
};

// 2D 단면 그림: 가로 = 위도 (또는 경도) [deg], 세로 = 고도 [m]. 휠 확대, 끌어 이동, 두 번 클릭 = 처음 보기
class CProfilePlot : public CStatic
{
public:
	CProfilePlot() noexcept;

	VOID	f_SetAxis(INT32 nNewAxis);
	VOID	f_SetDpi(INT32 newDpi);
	VOID	f_SetData(const CSimResult *st_NewResult, const CPlotResult *st_NewMeasure, const CTrackResult *st_NewTrack);
	VOID	f_SetShow(INT32 isNewShowTrue, INT32 isNewShowPlot, INT32 isNewShowTrack);
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
	FLOAT64	f_Across(const ST_PlotPoint *st_Point) const;
	VOID	f_ToPixel(const ST_PlotPoint *st_Point, FLOAT64 *pt_X, FLOAT64 *pt_Y) const;
	VOID	f_Draw(Gdiplus::Graphics *st_Graphics) const;

	const CSimResult	*st_Result;
	const CPlotResult	*st_Measure;
	const CTrackResult	*st_Track;
	INT32				nAxis;							// PROFILE_LAT, PROFILE_LON
	INT32				isShowTrue;
	INT32				isShowPlot;
	INT32				isShowTrack;
	INT32				isShowLegend;
	INT32				nCurScan;
	INT32				dpi;

	FLOAT64				homeAcross;						// 처음 보기 가로 중심 [m] (그림 좌표 북 또는 동)
	FLOAT64				homeUp;
	FLOAT64				homeSpanAcross;					// 처음 보기 가로 폭 [m]
	FLOAT64				homeSpanUp;
	FLOAT64				zoom;
	FLOAT64				panAcross;						// 처음 보기 중심에서 옮긴 양 [m]
	FLOAT64				panUp;
	FLOAT64				centerAcross;					// 지금 보기
	FLOAT64				centerUp;
	FLOAT64				spanAcross;
	FLOAT64				spanUp;
	INT32				isDragging;
	CPoint				st_DragStart;
	FLOAT64				dragPanAcross;
	FLOAT64				dragPanUp;
	CRect				st_Area;
};

// 추적 성능 그림 (가로 = 시간): 위 = 참값 대비 위치 오차와 σ위치, 아래 = NIS (로그 눈금) 와 게이트. 선회 · 발산 판정 구간을 띠로 표시
class CFilterPlot : public CStatic
{
public:
	CFilterPlot() noexcept;

	VOID	f_SetDpi(INT32 newDpi);
	VOID	f_SetData(const CSimResult *st_NewResult, const CTrackResult *st_NewTrack);
	VOID	f_SetScan(INT32 nNewScan);

protected:
	afx_msg VOID	OnPaint(VOID);
	afx_msg BOOL	OnEraseBkgnd(CDC *st_Dc);
	afx_msg VOID	OnSize(UINT32 type, INT32 width, INT32 height);

	DECLARE_MESSAGE_MAP()

private:
	VOID	f_ComputeView(const CRect &st_Client);
	FLOAT64	f_TimeToX(FLOAT64 time) const;
	FLOAT64	f_ErrorToY(FLOAT64 error) const;
	FLOAT64	f_NisToY(FLOAT64 nis) const;
	VOID	f_Draw(Gdiplus::Graphics *st_Graphics) const;
	VOID	f_DrawBands(Gdiplus::Graphics *st_Graphics, INT32 nLastIndex) const;

	const CSimResult	*st_Result;
	const CTrackResult	*st_Track;
	INT32				nCurScan;
	INT32				dpi;

	FLOAT64				timeMax;						// 가로축 끝 [s]
	FLOAT64				errorMax;						// 오차 축 끝 [m]
	FLOAT64				nisLogMin;						// NIS 축 (log10)
	FLOAT64				nisLogMax;
	CRect				st_ErrorArea;
	CRect				st_NisArea;
};
