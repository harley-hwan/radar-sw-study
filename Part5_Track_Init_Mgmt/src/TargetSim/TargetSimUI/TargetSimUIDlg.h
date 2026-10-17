#pragma once

#include "CalcLog.h"
#include "GridCtrl.h"
#include "PlotResult.h"
#include "Scenario.h"
#include "TrajectoryPlot.h"

// 객체 표의 열: 이름, 초기값 7
#define UI_OBJ_COL_NAME			0
#define UI_OBJ_COL_FIELD		1
#define UI_OBJ_COL_NUM			(UI_OBJ_COL_FIELD + SCN_OBJ_FIELD_NUM)

// 기동 표의 열: 번호, 축, G/시작/종료
#define UI_MNV_COL_INDEX		0
#define UI_MNV_COL_TYPE			1
#define UI_MNV_COL_FIELD		2
#define UI_MNV_COL_NUM			(UI_MNV_COL_FIELD + SCN_MNV_FIELD_NUM)

// 결과 표 보기. -1 = 전체, 0 = 플랫폼, k = 표적 k, UI_RESULT_TRACK = 지금 스캔의 트랙 (과제 5)
#define UI_RESULT_TRACK			(-2)

// 그림 보기 (2D, 3D)
#define UI_VIEW_BOTH			0
#define UI_VIEW_2D				1
#define UI_VIEW_3D				2
#define UI_VIEW_NUM				3

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

	afx_msg VOID	f_OnScenarioMenuClicked(VOID);
	afx_msg VOID	f_OnScenarioPreset(UINT32 command);
	afx_msg VOID	f_OnCalcLogClicked(VOID);
	afx_msg VOID	f_OnTargetAddClicked(VOID);
	afx_msg VOID	f_OnTargetDeleteClicked(VOID);
	afx_msg VOID	f_OnManeuverAddClicked(VOID);
	afx_msg VOID	f_OnManeuverDeleteClicked(VOID);
	afx_msg VOID	f_OnSaveCsvClicked(VOID);
	afx_msg VOID	f_OnResultViewChanged(VOID);
	afx_msg VOID	f_OnResultCompareClicked(VOID);
	afx_msg VOID	f_OnObjCellChanged(NMHDR *st_Hdr, LRESULT *pt_Result);
	afx_msg VOID	f_OnObjRowChanged(NMHDR *st_Hdr, LRESULT *pt_Result);
	afx_msg VOID	f_OnMnvCellChanged(NMHDR *st_Hdr, LRESULT *pt_Result);
	afx_msg VOID	f_OnResultGetDispInfo(NMHDR *st_Hdr, LRESULT *pt_Result);
	afx_msg VOID	f_OnPcsInputKillFocus(VOID);
	afx_msg VOID	f_OnPcsShowClicked(VOID);
	afx_msg VOID	f_OnPcsRandClicked(VOID);
	afx_msg VOID	f_OnTrkGapChanged(VOID);
	afx_msg VOID	f_OnTrkShowClicked(VOID);
	afx_msg VOID	f_OnTrkScoreClicked(VOID);
	afx_msg VOID	f_OnViewModeChanged(VOID);

	DECLARE_MESSAGE_MAP()

private:
	// 배치
	VOID	f_SetupGrids(VOID);
	VOID	f_SetInitialSize(VOID);
	VOID	f_Layout(VOID);
	VOID	f_DrawCard(CDC *st_Dc, const CRect &st_Card) const;

	// 편집
	VOID	f_LoadPreset(INT32 nPreset);
	VOID	f_RefreshObjGrid(VOID);
	VOID	f_RefreshMnvGrid(VOID);
	VOID	f_UpdateTitles(VOID);
	VOID	f_SelectObject(INT32 nObject);
	ST_ObjectText *	f_GetObject(INT32 nObject);

	// 실행
	VOID	f_RunScenario(VOID);
	VOID	f_SetupResultList(VOID);
	VOID	f_SetupResultView(VOID);
	VOID	f_SetupResultColumns(VOID);
	VOID	f_SetupTrackColumns(VOID);
	INT32	f_GetResultMethod(LPCTSTR pt_Name[], const CSimResult *st_Source[]) const;
	VOID	f_WriteCalcLog(VOID);

	// 측정 모의 (과제 4)
	VOID	f_RunMeasure(VOID);
	VOID	f_ApplyMeasureInput(VOID);
	VOID	f_ShowMeasureInput(VOID);

	// 트랙 개시 · 관리 (과제 5)
	VOID	f_RunTrack(VOID);
	VOID	f_ShowScan(INT32 nScan);
	VOID	f_UpdateTrackSummary(VOID);
	INT32	f_GetLastScan(VOID) const;

	CGridCtrl			st_ObjGrid;
	CGridCtrl			st_MnvGrid;
	CTrajectoryPlot		st_Plot;
	CTrackView3D		st_View3D;
	CSliderCtrl			st_ScanSlider;
	CComboBox			st_ViewMode;
	CComboBox			st_GapCombo;
	CListCtrl			st_ResultList;
	CComboBox			st_ResultView;
	CFont				st_TitleFont;
	CBrush				st_CardBrush;
	CBrush				st_PageBrush;

	CRect				st_TopBar;
	CRect				st_CardObj;
	CRect				st_CardMnv;
	CRect				st_CardPlot;
	CRect				st_CardResult;
	CRect				st_CardPcs;
	CRect				st_CardTrk;

	CScenario			st_Scenario;
	CSimResult			st_Result;
	CSimResult			st_NoMidResult;						// 같은 설정의 비중점법 결과 (비교용)
	CSimResult			st_PlatformResult;					// 같은 설정의 플랫폼 기준 결과 (비교용)
	CCalcLog			st_CalcLog;							// 기본 계산의 과정을 적는 콘솔
	CPlotResult			st_PlotResult;						// 측정 모의 결과. 기본 계산 (st_Result) 을 잰 것
	CTrackResult		st_TrackResult;						// 트랙 개시 · 관리 결과. 측정 모의 결과를 UDP 로 보내 받은 쪽이 처리한 것

	INT32				dpi;
	INT32				textHeight;
	INT32				nCurObject;							// 0 = 플랫폼, k = 표적 k
	INT32				nResultObject;						// 결과 표에 보일 객체. -1 = 전체, UI_RESULT_TRACK = 트랙
	INT32				nResultObjectNum;					// 보기 목록과 열을 만들 때의 객체 수
	INT32				isResultCompare;					// 1 이면 결과 표에 비중점법 열도 표시
	INT32				isPlatformCompare;					// 1 이면 결과 표에 플랫폼 기준 열도 표시
	FLOAT64				snr;								// 측정 모의 SNR (선형)
	INT32				nClutterNum;						// 스캔당 클러터 수
	INT32				isShowTrue;							// 1 이면 지도에 참값 궤적 표시
	INT32				isShowPlot;							// 1 이면 지도에 플롯 표시
	INT32				isShowClutter;						// 1 이면 지도에 클러터 표시
	INT32				isShowTrack;						// 1 이면 지도에 트랙 표시
	INT32				sendGapMs;							// 플롯 메시지 송신 간격 [ms]
	INT32				nCurScan;							// 그림, 트랙 표에 보이는 스캔 번호
	INT32				isFollowScan;						// 1 이면 새 스캔을 받을 때마다 그 스캔을 보임
	INT32				nViewMode;							// UI_VIEW_*
};
