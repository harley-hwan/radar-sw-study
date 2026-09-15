#pragma once

#include "TargetSim_JH.h"
#include "TrajectoryPlot_JH.h"

#define UI_OBJ_FIELD_NUM		7						// 위도, 경도, 고도, 속력, Roll, Pitch, Yaw (IDC_ 연속 번호 순서)
#define UI_MNV_FIELD_NUM		3						// G, 시작, 종료
#define UI_FIELD_LAT			0
#define UI_FIELD_LON			1
#define UI_FIELD_ALT			2
#define UI_FIELD_SPEED			3
#define UI_FIELD_ROLL			4
#define UI_FIELD_PITCH			5
#define UI_FIELD_YAW			6
#define UI_FIELD_GRAVITY		0
#define UI_FIELD_START			1
#define UI_FIELD_END			2

#define UI_PLAY_TIMER_ID		1
#define UI_PLAY_TICK_MS			100
#define UI_PLAY_SPEED_NUM		6
#define UI_TEXT_LIMIT			32
#define UI_CELL_SIZE			48

#define UI_SYNC_NONE			0
#define UI_SYNC_SLIDER			1
#define UI_SYNC_TABLE			2
#define UI_SYNC_PLAY			3

// 입력 허용 범위. Core 는 비유한 값과 비물리 값을 걸러내지 않으므로 여기서 막는다.
#define UI_LAT_LIMIT			89.9					// [deg]
#define UI_LON_LIMIT			180.0					// [deg]
#define UI_ALT_MIN				-1000.0					// [m]
#define UI_ALT_MAX				100000.0				// [m]
#define UI_SPEED_MAX			3000.0					// [m/s]
#define UI_TGT_SPEED_MIN		0.1						// [m/s]
#define UI_ROLL_LIMIT			180.0					// [deg]
#define UI_PITCH_LIMIT			90.0					// [deg]
#define UI_YAW_LIMIT			360.0					// [deg]
#define UI_G_LIMIT				50.0					// [G]
#define UI_STEP_MAX				1.0						// [s]
#define UI_DURATION_MAX			100000.0				// [s]
#define UI_TIME_RES				1.0e-3					// [s]
#define UI_GRID_TOL				1.0e-6
#define UI_TURN_STEP_MAX		(PI / 6.0)				// [rad] 스텝당 회전각

typedef struct
{
	EN_TurnType			enTurnType;
	CString				st_FieldText[UI_MNV_FIELD_NUM];
} ST_ManeuverText;

// 표적 편집값의 원본. CString 이 있으므로 memset 하지 말고 대입으로 비운다.
typedef struct
{
	CString				st_FieldText[UI_OBJ_FIELD_NUM];
	INT32				nManeuverNum;
	ST_ManeuverText		st_Maneuver[TGT_MAX_MANEUVER_NUM];
} ST_TargetText;

// 입력 오류 위치와 문구. 해당 없는 칸은 -1.
typedef struct
{
	INT32				ctrlId;
	INT32				nTarget;
	INT32				nManeuver;
	CString				st_Message;
} ST_UiIssue;

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
	VOID	OnOK(VOID) override;
	VOID	OnCancel(VOID) override;

	afx_msg VOID	OnSysCommand(UINT32 commandId, LPARAM lParam);
	afx_msg VOID	OnPaint(VOID);
	afx_msg HCURSOR	OnQueryDragIcon(VOID);
	afx_msg VOID	OnTimer(UINT_PTR timerId);
	afx_msg VOID	OnHScroll(UINT32 scrollCode, UINT32 thumbPos, CScrollBar *st_ScrollBar);
	afx_msg VOID	OnClose(VOID);

	afx_msg VOID	f_OnLoadSpecClicked(VOID);
	afx_msg VOID	f_OnTargetAddClicked(VOID);
	afx_msg VOID	f_OnTargetDeleteClicked(VOID);
	afx_msg VOID	f_OnManeuverAddClicked(VOID);
	afx_msg VOID	f_OnManeuverDeleteClicked(VOID);
	afx_msg VOID	f_OnRunClicked(VOID);
	afx_msg VOID	f_OnPlayClicked(VOID);
	afx_msg VOID	f_OnCsvBrowseClicked(VOID);
	afx_msg VOID	f_OnSaveCsvClicked(VOID);
	afx_msg VOID	f_OnTargetSelChange(VOID);
	afx_msg VOID	f_OnManeuverTypeChange(VOID);
	afx_msg VOID	f_OnSpeedSelChange(VOID);
	afx_msg VOID	f_OnConfigFieldChange(UINT32 ctrlId);
	afx_msg VOID	f_OnTargetFieldChange(UINT32 ctrlId);
	afx_msg VOID	f_OnManeuverFieldChange(UINT32 ctrlId);
	afx_msg VOID	f_OnManeuverItemChanged(NMHDR *st_Hdr, LRESULT *pt_Result);
	afx_msg VOID	f_OnResultGetDispInfo(NMHDR *st_Hdr, LRESULT *pt_Result);
	afx_msg VOID	f_OnResultFindItem(NMHDR *st_Hdr, LRESULT *pt_Result);
	afx_msg VOID	f_OnResultItemChanged(NMHDR *st_Hdr, LRESULT *pt_Result);

	DECLARE_MESSAGE_MAP()

private:
	// 편집
	VOID	f_LoadSpecScenario(VOID);
	VOID	f_SelectTarget(INT32 nTarget);
	VOID	f_RefreshTargetCombo(VOID);
	VOID	f_LoadTargetEdits(VOID);
	VOID	f_RefreshManeuverList(VOID);
	VOID	f_UpdateManeuverRow(INT32 nManeuver);
	VOID	f_SelectManeuver(INT32 nManeuver);
	VOID	f_LoadManeuverEdits(VOID);
	VOID	f_UpdateEditButtons(VOID);
	VOID	f_MarkConfigChanged(VOID);

	// 검증
	INT32	f_ReadNumber(const CString &st_Text, INT32 ctrlId, INT32 nTarget, INT32 nManeuver, FLOAT64 minValue, FLOAT64 maxValue,
				LPCTSTR pt_Name, FLOAT64 *pt_Value, ST_UiIssue *st_Issue) const;
	INT32	f_ReadObject(const CString *st_FieldText, INT32 baseCtrlId, INT32 nTarget, FLOAT64 minSpeed,
				ST_CoordLla *st_Lla, ST_CoordAtt *st_Att, FLOAT64 *pt_Speed, ST_UiIssue *st_Issue) const;
	INT32	f_BuildConfig(ST_SimConfig *st_Config, ST_UiIssue *st_Issue) const;
	VOID	f_LocateCoreError(EN_TgtStatus enStatus, ST_UiIssue *st_Issue);
	VOID	f_ReportIssue(const ST_UiIssue *st_Issue);

	// 실행·출력
	VOID	f_RunScenario(VOID);
	INT32	f_ComputePlotPoints(const ST_SimSample *st_Sample, ST_PlotPoint *st_Point, INT32 nNewSampleNum, INT32 nNewObjectNum,
				ST_UiIssue *st_Issue) const;
	VOID	f_SetupResultList(VOID);
	INT32	f_FormatCell(INT32 nRow, INT32 nColumn, CHAR *pt_Buf, INT32 bufSize) const;
	INT32	f_WriteCsv(const CString &st_Path, INT32 *pt_LineNum) const;

	// 재생·표시
	VOID	f_ShowStep(INT32 nStep, INT32 syncSource);
	VOID	f_StartPlay(VOID);
	VOID	f_StopPlay(VOID);
	VOID	f_EnableResultControls(INT32 isEnabled);
	VOID	f_SetStatus(const CString &st_Text);
	VOID	f_RefreshStatus(VOID);

	HICON				iconHandle;
	CComboBox			st_TargetCombo;
	CComboBox			st_TypeCombo;
	CComboBox			st_SpeedCombo;
	CListCtrl			st_ManeuverList;
	CListCtrl			st_ResultList;
	CSliderCtrl			st_TimeSlider;
	CTrajectoryPlot		st_Plot;

	ST_TargetText		st_TargetText[TGT_MAX_TARGET_NUM];
	INT32				nTargetNum;
	INT32				nCurTarget;
	INT32				nCurManeuver;
	INT32				isLoading;							// 코드가 칸을 채우는 중이면 EN_CHANGE 를 무시한다
	INT32				isSyncing;							// 코드가 표 선택을 바꾸는 중이면 LVN_ITEMCHANGED 를 무시한다
	INT32				isConfigChanged;

	ST_SimConfig		st_RunConfig;
	ST_SimConfig		st_ProbeConfig;
	ST_SimState			st_Sim;

	// 마지막으로 성공한 실행 결과. 표·그림·CSV 는 이것만 읽는다.
	ST_SimSample		*st_SampleBuf;
	ST_PlotPoint		*st_PointBuf;
	INT32				nSampleNum;
	INT32				nObjectNum;
	FLOAT64				resultStepTime;

	INT32				nCurStep;
	INT32				isPlaying;
	INT32				nPlayOriginStep;
	INT32				nPlaySpeed;
	INT64				nPlayTick;
	CString				st_StatusText;
};
