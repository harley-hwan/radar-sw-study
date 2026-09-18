#pragma once

#include "GridCtrl.h"
#include "Scenario.h"
#include "TrajectoryPlot.h"

#define UI_PLAY_TIMER_ID		1
#define UI_RUN_TIMER_ID			2
#define UI_PLAY_TICK_MS			100
#define UI_RUN_DELAY_MS			250						// 오래 걸리는 시나리오는 입력이 멎은 뒤 실행
#define UI_FAST_RUN_MS			40.0					// 이보다 빨리 끝나면 고칠 때마다 바로 실행
#define UI_PLAY_SPEED_NUM		6

#define UI_SYNC_NONE			0
#define UI_SYNC_SLIDER			1
#define UI_SYNC_TABLE			2
#define UI_SYNC_PLAY			3

#define UI_STATUS_INFO			0
#define UI_STATUS_OK			1
#define UI_STATUS_WARN			2
#define UI_STATUS_ERROR			3

// 객체 표의 열: 이름, 초기값 7, 기동 구간
#define UI_OBJ_COL_NAME			0
#define UI_OBJ_COL_FIELD		1
#define UI_OBJ_COL_TIMELINE		(UI_OBJ_COL_FIELD + SCN_OBJ_FIELD_NUM)
#define UI_OBJ_COL_NUM			(UI_OBJ_COL_TIMELINE + 1)

// 기동 표의 열: 번호, 축, G/시작/종료, 회전각, 선회 반경
#define UI_MNV_COL_INDEX		0
#define UI_MNV_COL_TYPE			1
#define UI_MNV_COL_FIELD		2
#define UI_MNV_COL_ANGLE		(UI_MNV_COL_FIELD + SCN_MNV_FIELD_NUM)
#define UI_MNV_COL_RADIUS		(UI_MNV_COL_ANGLE + 1)
#define UI_MNV_COL_NUM			(UI_MNV_COL_RADIUS + 1)

class CTargetSimUIDlg : public CDialogEx
{
public:
	explicit CTargetSimUIDlg(CWnd *st_Parent = nullptr);
	~CTargetSimUIDlg() override;

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_TARGETSIMUI_DIALOG };
#endif

protected:
	VOID	DoDataExchange(CDataExchange *st_Dx) override;
	BOOL	OnInitDialog(VOID) override;
	BOOL	PreTranslateMessage(MSG *st_Msg) override;
	VOID	OnOK(VOID) override;
	VOID	OnCancel(VOID) override;

	afx_msg VOID	OnSysCommand(UINT32 commandId, LPARAM lParam);
	afx_msg VOID	OnPaint(VOID);
	afx_msg BOOL	OnEraseBkgnd(CDC *st_Dc);
	afx_msg HBRUSH	OnCtlColor(CDC *st_Dc, CWnd *st_Wnd, UINT32 ctlColor);
	afx_msg HCURSOR	OnQueryDragIcon(VOID);
	afx_msg VOID	OnSize(UINT32 type, INT32 width, INT32 height);
	afx_msg VOID	OnGetMinMaxInfo(MINMAXINFO *st_Info);
	afx_msg VOID	OnTimer(UINT_PTR timerId);
	afx_msg VOID	OnHScroll(UINT32 scrollCode, UINT32 thumbPos, CScrollBar *st_ScrollBar);
	afx_msg VOID	OnClose(VOID);

	afx_msg VOID	f_OnScenarioMenuClicked(VOID);
	afx_msg VOID	f_OnScenarioPreset(UINT32 command);
	afx_msg VOID	f_OpenScenario(VOID);
	afx_msg VOID	f_SaveScenario(VOID);
	afx_msg VOID	f_OnTargetAddClicked(VOID);
	afx_msg VOID	f_OnTargetCopyClicked(VOID);
	afx_msg VOID	f_OnTargetDeleteClicked(VOID);
	afx_msg VOID	f_OnManeuverAddClicked(VOID);
	afx_msg VOID	f_OnManeuverDeleteClicked(VOID);
	afx_msg VOID	f_OnPlayClicked(VOID);
	afx_msg VOID	f_OnSaveCsvClicked(VOID);
	afx_msg VOID	f_OnSpeedSelChange(VOID);
	afx_msg VOID	f_OnStepSelChange(VOID);
	afx_msg VOID	f_OnSimFieldKillFocus(VOID);
	afx_msg VOID	f_OnDurationSpin(NMHDR *st_Hdr, LRESULT *pt_Result);
	afx_msg VOID	f_OnObjCellChanged(NMHDR *st_Hdr, LRESULT *pt_Result);
	afx_msg VOID	f_OnObjRowChanged(NMHDR *st_Hdr, LRESULT *pt_Result);
	afx_msg VOID	f_OnObjDrawCell(NMHDR *st_Hdr, LRESULT *pt_Result);
	afx_msg VOID	f_OnMnvCellChanged(NMHDR *st_Hdr, LRESULT *pt_Result);
	afx_msg VOID	f_OnPlotSelect(NMHDR *st_Hdr, LRESULT *pt_Result);
	afx_msg VOID	f_OnPlotMove(NMHDR *st_Hdr, LRESULT *pt_Result);
	afx_msg VOID	f_OnPlotTurn(NMHDR *st_Hdr, LRESULT *pt_Result);
	afx_msg VOID	f_OnPlotScrub(NMHDR *st_Hdr, LRESULT *pt_Result);
	afx_msg VOID	f_OnResultGetDispInfo(NMHDR *st_Hdr, LRESULT *pt_Result);
	afx_msg VOID	f_OnResultFindItem(NMHDR *st_Hdr, LRESULT *pt_Result);
	afx_msg VOID	f_OnResultItemChanged(NMHDR *st_Hdr, LRESULT *pt_Result);
	afx_msg VOID	f_OnResultCustomDraw(NMHDR *st_Hdr, LRESULT *pt_Result);

	DECLARE_MESSAGE_MAP()

private:
	// 배치
	VOID	f_SetupGrids(VOID);
	VOID	f_SetInitialSize(VOID);
	VOID	f_Layout(VOID);
	VOID	f_DrawCard(CDC *st_Dc, const CRect &st_Card) const;

	// 편집
	VOID	f_RefreshAll(VOID);
	VOID	f_ShowScenarioName(VOID);
	VOID	f_RefreshObjGrid(VOID);
	VOID	f_RefreshMnvGrid(INT32 isNewTarget);
	VOID	f_UpdateMnvComputed(INT32 nManeuver);
	VOID	f_UpdateTitles(VOID);
	VOID	f_UpdateTimeNudge(VOID);
	VOID	f_CommitSimFields(VOID);
	VOID	f_SelectObject(INT32 nObject);
	ST_ObjectText *	f_GetObject(INT32 nObject);

	// 실행
	VOID	f_PrepareModal(VOID);
	VOID	f_ScheduleRun(VOID);
	VOID	f_RunScenario(VOID);
	VOID	f_RunScenarioOnce(VOID);
	VOID	f_ShowIssue(const ST_ScnIssue *st_Issue);
	VOID	f_MarkIssue(VOID);
	VOID	f_ClearIssueMarks(VOID);
	VOID	f_SetupResultList(VOID);

	// 재생
	VOID	f_ShowStep(INT32 nStep, INT32 syncSource);
	VOID	f_StartPlay(VOID);
	VOID	f_StopPlay(VOID);
	VOID	f_EnableResultControls(INT32 isEnabled);
	VOID	f_SetStatus(INT32 kind, const CString &st_Text);

	HICON				iconHandle;
	CGridCtrl			st_ObjGrid;
	CGridCtrl			st_MnvGrid;
	CTrajectoryPlot		st_Plot;
	CListCtrl			st_ResultList;
	CSliderCtrl			st_TimeSlider;
	CComboBox			st_SpeedCombo;
	CComboBox			st_StepCombo;
	CSpinButtonCtrl		st_DurationSpin;
	CFont				st_TitleFont;
	CBrush				st_CardBrush;
	CBrush				st_PageBrush;

	CRect				st_TopBar;
	CRect				st_StatusBar;
	CRect				st_CardObj;
	CRect				st_CardMnv;
	CRect				st_CardPlot;
	CRect				st_CardResult;

	CScenario			st_Scenario;
	CSimResult			st_Result;
	ST_SimConfig		st_RunConfig;

	INT32				dpi;
	INT32				textHeight;
	INT32				nCurObject;							// 0 = 플랫폼, k = 표적 k
	INT32				isSyncing;							// 코드가 표 선택을 바꾸는 중
	INT32				isResultStale;						// 화면의 결과가 지금 입력과 다름
	INT32				nResultColumnNum;
	INT32				statusKind;

	INT32				nCurStep;
	INT32				isPlaying;
	INT32				nPlayOriginStep;
	INT32				nPlaySpeed;
	INT64				nPlayTick;
	CString				st_CsvFolder;
	CString				st_ScenarioName;					// 프리셋 이름 또는 파일 이름
	ST_ScnIssue			st_LastIssue;						// isIssueActive 가 1 일 때 유효
	INT32				isModified;
	INT32				isIssueActive;
	INT32				isRunPending;						// 자동 실행을 타이머로 미룸
	INT32				isRunning;
	INT32				isClosing;
};
