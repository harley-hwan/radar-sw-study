//
// @file	TargetSimUIDlg.h
// @brief	트랙 개시 · 관리 결과 주 화면 (과제 5)
// @author	hwan
// @date	2026.10.21.
//
#pragma once

#include "PlotResult.h"
#include "Scenario.h"
#include "TrajectoryPlot.h"

// 그림 보기 (2D, 3D)
#define UI_VIEW_BOTH			0
#define UI_VIEW_2D				1
#define UI_VIEW_3D				2
#define UI_VIEW_NUM				3

// 그림 표시 (True, Plot, Clutter, Track)
#define UI_SHOW_TRUE			0
#define UI_SHOW_PLOT			1
#define UI_SHOW_CLUTTER			2
#define UI_SHOW_TRACK			3
#define UI_SHOW_NUM				4

// 트랙 개시 · 관리 결과 화면 (과제 5).
// 궤적, 기동, 측정 조건은 고정 시나리오 그대로 한 번 계산해 두고, 측정 모의 결과를 플롯 메시지로 보내 데이터 처리 쪽이 처리한 스캔을
// True · Plot · Track 그림 (2D, 3D), 지금 스캔의 트랙 표, 송수신 · 트랙 요약으로 보임.
class CTargetSimUIDlg : public CDialogEx
{
public:
	explicit CTargetSimUIDlg(CWnd *st_Parent = nullptr);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_TARGETSIMUI_DIALOG };
#endif

protected:
	VOID	DoDataExchange(CDataExchange *st_Dx) override;
	BOOL	OnInitDialog(VOID) override;
	VOID	OnOK(VOID) override;
	VOID	OnCancel(VOID) override;

	afx_msg BOOL	OnEraseBkgnd(CDC *st_Dc);
	afx_msg HBRUSH	OnCtlColor(CDC *st_Dc, CWnd *st_Wnd, UINT32 ctlColor);
	afx_msg VOID	OnSize(UINT32 type, INT32 width, INT32 height);
	afx_msg VOID	OnGetMinMaxInfo(MINMAXINFO *st_Info);
	afx_msg VOID	OnClose(VOID);
	afx_msg VOID	OnDestroy(VOID);
	afx_msg VOID	OnTimer(UINT_PTR nEvent);
	afx_msg VOID	OnHScroll(UINT32 code, UINT32 pos, CScrollBar *st_Bar);
	afx_msg BOOL	OnMouseWheel(UINT32 flags, INT16 delta, CPoint st_Point);

	afx_msg VOID	f_OnShowClicked(VOID);
	afx_msg VOID	f_OnViewModeChanged(VOID);
	afx_msg VOID	f_OnTrkGapChanged(VOID);
	afx_msg VOID	f_OnTrkResendClicked(VOID);
	afx_msg VOID	f_OnTrkScoreClicked(VOID);
	afx_msg VOID	f_OnSaveCsvClicked(VOID);
	afx_msg VOID	f_OnTrackGetDispInfo(NMHDR *st_Hdr, LRESULT *pt_Result);

	DECLARE_MESSAGE_MAP()

private:
	// 배치
	VOID	f_SetInitialSize(VOID);
	VOID	f_Layout(VOID);
	VOID	f_DrawCard(CDC *st_Dc, const CRect &st_Card) const;
	VOID	f_SetText(INT32 ctrlId, const CString &st_Text);

	// 실행
	VOID	f_RunScenario(VOID);
	VOID	f_RunTrack(VOID);
	VOID	f_SetupTrackColumns(VOID);

	// 결과 보이기
	VOID	f_ShowScan(INT32 nScan);
	VOID	f_UpdateTrackList(VOID);
	VOID	f_UpdateTrackSummary(VOID);
	INT32	f_GetLastScan(VOID) const;

	CTrajectoryPlot		st_Plot;
	CTrackView3D		st_View3D;
	CSliderCtrl			st_ScanSlider;
	CComboBox			st_ViewMode;
	CComboBox			st_GapCombo;
	CListCtrl			st_TrackList;
	CFont				st_TitleFont;
	CBrush				st_CardBrush;
	CBrush				st_PageBrush;

	CRect				st_TopBar;
	CRect				st_CardPlot;
	CRect				st_CardTrk;
	CRect				st_CardList;

	CSimResult			st_Result;							// 궤적 계산 결과 (참값)
	CPlotResult			st_PlotResult;						// 측정 모의 결과. 궤적 계산 결과 (st_Result) 를 잰 것
	CTrackResult		st_TrackResult;						// 트랙 개시 · 관리 결과. 측정 모의 결과를 UDP 로 보내 받은 쪽이 처리한 것

	INT32				dpi;
	INT32				textHeight;
	INT32				isShow[UI_SHOW_NUM];				// 1 이면 그림에 표시. UI_SHOW_* 순서
	INT32				sendGapMs;							// 플롯 메시지 송신 간격 [ms]
	INT32				nCurScan;							// 그림, 트랙 표, 요약에 보이는 스캔 번호
	INT32				isFollowScan;						// 1 이면 새 스캔을 받을 때마다 그 스캔을 보임
	INT32				nViewMode;							// UI_VIEW_*
};
