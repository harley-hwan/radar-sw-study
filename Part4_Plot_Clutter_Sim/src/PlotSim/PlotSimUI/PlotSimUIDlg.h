#pragma once

#include <vector>

#include "ChartView.h"

// 주 대화상자. 위: 입력, 실행, 결과 요약. 아래: LLA 지도, 난수 히스토그램 2개.
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

	afx_msg VOID	f_OnRunClicked(VOID);

	DECLARE_MESSAGE_MAP()

private:
	VOID	f_ShowSummary(FLOAT64 snr);

	CChartView				st_MapView;
	CChartView				st_UniformView;
	CChartView				st_GaussView;
	ST_PcsHist				st_Uniform;
	ST_PcsHist				st_Gauss;
	std::vector<ST_PcsScan>	st_Scan;						// PCS_SCAN_NUM 개
};
