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
	INT32	f_GetResultMethod(LPCTSTR pt_Name[], const CSimResult *st_Source[]) const;
	VOID	f_WriteCalcLog(VOID);

	// 측정 모의 (과제 4)
	VOID	f_RunMeasure(VOID);
	VOID	f_ApplyMeasureInput(VOID);
	VOID	f_ShowMeasureInput(VOID);

	CGridCtrl			st_ObjGrid;
	CGridCtrl			st_MnvGrid;
	CTrajectoryPlot		st_Plot;
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

	CScenario			st_Scenario;
	CSimResult			st_Result;
	CSimResult			st_NoMidResult;						// 같은 설정의 비중점법 결과 (비교용)
	CSimResult			st_PlatformResult;					// 같은 설정의 플랫폼 기준 결과 (비교용)
	CCalcLog			st_CalcLog;							// 기본 계산의 과정을 적는 콘솔
	CPlotResult			st_PlotResult;						// 측정 모의 결과. 기본 계산 (st_Result) 을 잰 것

	INT32				dpi;
	INT32				textHeight;
	INT32				nCurObject;							// 0 = 플랫폼, k = 표적 k
	INT32				nResultObject;						// 결과 표에 보일 객체. -1 = 전체
	INT32				nResultObjectNum;					// 보기 목록과 열을 만들 때의 객체 수
	INT32				isResultCompare;					// 1 이면 결과 표에 비중점법 열도 표시
	INT32				isPlatformCompare;					// 1 이면 결과 표에 플랫폼 기준 열도 표시
	FLOAT64				snr;								// 측정 모의 SNR (선형)
	INT32				nClutterNum;						// 스캔당 클러터 수
	INT32				isShowPlot;							// 1 이면 지도에 플롯 표시
	INT32				isShowClutter;						// 1 이면 지도에 클러터 표시
};
