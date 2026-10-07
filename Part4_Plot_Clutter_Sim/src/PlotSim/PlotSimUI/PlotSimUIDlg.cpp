#include "pch.h"
#include "framework.h"

#include <math.h>

#include "PlotSimUI.h"
#include "PlotSimUIDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CPlotSimUIDlg, CDialogEx)
	ON_BN_CLICKED(IDC_RUN, &CPlotSimUIDlg::f_OnRunClicked)
END_MESSAGE_MAP()

CPlotSimUIDlg::CPlotSimUIDlg(CWnd *st_Parent)
	: CDialogEx(IDD_PLOTSIMUI_DIALOG, st_Parent)
	, st_Uniform()
	, st_Gauss()
	, st_Scan(PCS_SCAN_NUM)
{
}

VOID CPlotSimUIDlg::DoDataExchange(CDataExchange *st_Dx)
{
	CDialogEx::DoDataExchange(st_Dx);
	DDX_Control(st_Dx, IDC_MAP, st_MapView);
	DDX_Control(st_Dx, IDC_UNIFORM, st_UniformView);
	DDX_Control(st_Dx, IDC_GAUSS, st_GaussView);
}

// 난수 검증은 처음 한 번만 하고, 과제 조건 (SNR 20, 클러터 5개) 으로 한 번 실행.
BOOL CPlotSimUIDlg::OnInitDialog(VOID)
{
	(VOID)CDialogEx::OnInitDialog();

	f_Pcs_TestRand(&st_Uniform, &st_Gauss);
	st_UniformView.f_SetHist(&st_Uniform, 0);
	st_GaussView.f_SetHist(&st_Gauss, 1);

	SetDlgItemText(IDC_SNR, _T("20"));
	SetDlgItemInt(IDC_CLUTTER_NUM, 5U, FALSE);
	f_OnRunClicked();

	return TRUE;
}

// 입력을 읽어 600 스캔 실행.
VOID CPlotSimUIDlg::f_OnRunClicked(VOID)
{
	CString	st_Text;
	BOOL	isNumber = FALSE;

	(VOID)GetDlgItemText(IDC_SNR, st_Text);

	const FLOAT64	snr = _tstof(st_Text);
	const UINT32	nClutterNum = GetDlgItemInt(IDC_CLUTTER_NUM, &isNumber, FALSE);

	if ((snr <= 0.0) || (isNumber == FALSE) || (nClutterNum > PCS_MAX_CLUTTER))
	{
		st_Text.Format(_T("SNR 은 0 보다 큰 선형값으로, 클러터 수는 0 ~ %d 로 넣으세요."), PCS_MAX_CLUTTER);
		(VOID)AfxMessageBox(st_Text, MB_ICONWARNING);
		return;
	}

	f_Pcs_Run(st_Scan.data(), snr, static_cast<INT32>(nClutterNum));
	f_ShowSummary(snr);
	st_MapView.f_SetMap(st_Scan.data());
}

// 표준편차 p, 플롯 오차 (z - x) 의 표준편차 (표적 5개 플롯 전부), 클러터 수.
VOID CPlotSimUIDlg::f_ShowSummary(FLOAT64 snr)
{
	const STRUCT_Coord_Sph	st_Sigma = f_Pcs_CalcSigma(snr);
	const INT32				nPlotNum = PCS_SCAN_NUM * PCS_TARGET_NUM;
	FLOAT64					sum[3] = { 0.0, 0.0, 0.0 };
	FLOAT64					sumSq[3] = { 0.0, 0.0, 0.0 };
	FLOAT64					errStd[3];
	INT32					nClutterSum = 0;
	INT32					nTarget;
	INT32					nPart;
	CString					st_Text;

	for (const ST_PcsScan &st_One : st_Scan)
	{
		for (nTarget = 0; nTarget < PCS_TARGET_NUM; nTarget++)
		{
			const STRUCT_Coord_Sph	&st_True = st_One.st_True[nTarget];
			const STRUCT_Coord_Sph	&st_Plot = st_One.st_Plot[nTarget];
			const FLOAT64			diff[3] = { st_Plot.r - st_True.r, f_Rad_To_Deg(st_Plot.az - st_True.az), f_Rad_To_Deg(st_Plot.el - st_True.el) };

			for (nPart = 0; nPart < 3; nPart++)
			{
				sum[nPart]		= sum[nPart] + diff[nPart];
				sumSq[nPart]	= sumSq[nPart] + (diff[nPart] * diff[nPart]);
			}
		}

		nClutterSum = nClutterSum + st_One.nClutterNum;
	}

	for (nPart = 0; nPart < 3; nPart++)
	{
		const FLOAT64 mean = sum[nPart] / nPlotNum;

		errStd[nPart] = sqrt((sumSq[nPart] / nPlotNum) - (mean * mean));
	}

	st_Text.Format(_T("표준편차 p: %.3f m, %.4f°, %.4f°      플롯 오차 표준편차: %.3f m, %.4f°, %.4f° (%d개)      클러터 %d개"),
		st_Sigma.r, f_Rad_To_Deg(st_Sigma.az), f_Rad_To_Deg(st_Sigma.el), errStd[0], errStd[1], errStd[2], nPlotNum, nClutterSum);
	SetDlgItemText(IDC_SUMMARY, st_Text);
}
