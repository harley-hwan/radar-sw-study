#pragma once

#include "ChartView.h"

#define UI_CHECK_ROW_NUM		8									// 검증 결과 줄 수
#define UI_RESULT_COL_NUM		10									// 결과 목록 열 수

// 주 대화상자. 위: 설정, 왼쪽: 검증 결과와 결과 목록, 오른쪽: 그림.
class CPlotSimUIDlg : public CDialogEx
{
public:
	explicit CPlotSimUIDlg(CWnd *st_Parent = nullptr);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_PLOTSIMUI_DIALOG };
#endif

protected:
	VOID	DoDataExchange(CDataExchange *st_Dx) override;
	BOOL	OnInitDialog(VOID) override;
	VOID	OnOK(VOID) override;
	VOID	OnCancel(VOID) override;

	afx_msg VOID	OnSize(UINT32 type, INT32 width, INT32 height);
	afx_msg VOID	OnGetMinMaxInfo(MINMAXINFO *st_Info);
	afx_msg VOID	OnClose(VOID);
	afx_msg VOID	f_OnRunClicked(VOID);
	afx_msg VOID	f_OnSaveCsvClicked(VOID);
	afx_msg VOID	f_OnViewChanged(VOID);
	afx_msg VOID	f_OnResultGetDispInfo(NMHDR *st_Hdr, LRESULT *pt_Result);

	DECLARE_MESSAGE_MAP()

private:
	VOID	f_SetupControls(VOID);
	VOID	f_SetInitialSize(VOID);
	VOID	f_Layout(VOID);
	INT32	f_GetListHeight(const CListCtrl *st_List, INT32 nRowNum) const;

	INT32	f_ReadConfig(ST_PcsConfig *st_NewConfig, CString *st_Error);
	VOID	f_RunAll(VOID);
	VOID	f_AddScan(const ST_PcsScan *st_Scan);
	VOID	f_UpdateCheckList(VOID);
	VOID	f_UpdateParamText(VOID);
	CString	f_GetCellText(INT32 nRow, INT32 nColumn) const;

	CComboBox			st_PresetCombo;
	CComboBox			st_SnrUnitCombo;
	CComboBox			st_ClutterModeCombo;
	CComboBox			st_ViewCombo;
	CListCtrl			st_CheckList;
	CListCtrl			st_ResultList;
	CChartView			st_Chart;

	ST_PcsConfig		st_Config;							// 마지막 실행 설정
	ST_PcsState			st_State;
	ST_ChartData		st_Data;

	// 검증 결과용 누적 값
	FLOAT64				errSum[3];							// 플롯 오차 z - x 합 (거리, 방위각, 고각)
	FLOAT64				errSumSq[3];
	INT32				nErrNum;
	INT32				nScanDone;
	INT32				nClutterOut;						// FOV 밖 클러터 수

	INT32				dpi;
	INT32				textHeight;
	INT32				isReady;							// 컨트롤 준비가 끝나면 1
};
