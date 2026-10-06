#include "pch.h"
#include "framework.h"

#include <math.h>
#include <stdio.h>

#include <cmath>

#include "PlotSimUI.h"
#include "PlotSimUIDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define UI_INIT_WIDTH			1360					// [px @96dpi]
#define UI_INIT_HEIGHT			860
#define UI_MIN_WIDTH			1200					// 위 설정 줄이 겹치지 않는 너비
#define UI_MIN_HEIGHT			700
#define UI_MARGIN				10
#define UI_GAP					8
#define UI_LEFT_RATIO			0.42
#define UI_LEFT_MIN				480
#define UI_LEFT_MAX				660
#define UI_COMBO_DROP			220						// 콤보 목록 펼친 높이
#define UI_SEED_MAX				2147483646.0			// UNIRAN seed 상한 (2^31 - 2)

// 검증 결과 열
#define UI_CHECK_COL_ITEM		0
#define UI_CHECK_COL_VALUE		1
#define UI_CHECK_COL_REF		2

static VOID f_Ui_Move(CWnd *st_Dlg, INT32 ctrlId, INT32 left, INT32 top, INT32 width, INT32 height)
{
	CWnd *st_Ctrl = st_Dlg->GetDlgItem(ctrlId);

	if (st_Ctrl != nullptr)
	{
		(VOID)st_Ctrl->SetWindowPos(nullptr, left, top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
	}
}

// 글자 -> 실수. 앞뒤 공백은 허용, 숫자 뒤에 다른 글자가 있으면 0.
static INT32 f_Ui_ParseNumber(const CString &st_Text, FLOAT64 *pt_Value)
{
	CString	st_Trim = st_Text;
	LPTSTR	pt_End = nullptr;
	FLOAT64	value;
	INT32	isOk = 0;

	(VOID)st_Trim.Trim();

	if (!st_Trim.IsEmpty())
	{
		value = _tcstod(st_Trim.GetString(), &pt_End);

		if ((pt_End != nullptr) && (*pt_End == _T('\0')) && std::isfinite(value))
		{
			*pt_Value	= value;
			isOk		= 1;
		}
	}

	return isOk;
}

BEGIN_MESSAGE_MAP(CPlotSimUIDlg, CDialogEx)
	ON_WM_SIZE()
	ON_WM_GETMINMAXINFO()
	ON_WM_CLOSE()
	ON_BN_CLICKED(IDC_RUN, &CPlotSimUIDlg::f_OnRunClicked)
	ON_BN_CLICKED(IDC_SAVE_CSV, &CPlotSimUIDlg::f_OnSaveCsvClicked)
	ON_CBN_SELCHANGE(IDC_VIEW, &CPlotSimUIDlg::f_OnViewChanged)
	ON_NOTIFY(LVN_GETDISPINFO, IDC_RESULT_LIST, &CPlotSimUIDlg::f_OnResultGetDispInfo)
END_MESSAGE_MAP()

CPlotSimUIDlg::CPlotSimUIDlg(CWnd *st_Parent)
	: CDialogEx(IDD_PLOTSIMUI_DIALOG, st_Parent)
	, st_Config()
	, st_State()
	, st_Data()
	, errSum()
	, errSumSq()
	, nErrNum(0)
	, nScanDone(0)
	, nClutterOut(0)
	, dpi(UI_BASE_DPI)
	, textHeight(16)
	, isReady(0)
{
}

VOID CPlotSimUIDlg::DoDataExchange(CDataExchange *st_Dx)
{
	CDialogEx::DoDataExchange(st_Dx);
	DDX_Control(st_Dx, IDC_PRESET, st_PresetCombo);
	DDX_Control(st_Dx, IDC_SNR_UNIT, st_SnrUnitCombo);
	DDX_Control(st_Dx, IDC_CLUTTER_MODE, st_ClutterModeCombo);
	DDX_Control(st_Dx, IDC_VIEW, st_ViewCombo);
	DDX_Control(st_Dx, IDC_CHECK_LIST, st_CheckList);
	DDX_Control(st_Dx, IDC_RESULT_LIST, st_ResultList);
	DDX_Control(st_Dx, IDC_CHART, st_Chart);
}

// 글꼴 높이를 재고 컨트롤 준비 후 기본 설정으로 한 번 실행.
BOOL CPlotSimUIDlg::OnInitDialog(VOID)
{
	CClientDC	st_Dc(this);
	CFont		*st_OldFont;
	TEXTMETRIC	st_Metric;

	(VOID)CDialogEx::OnInitDialog();

	dpi			= st_Dc.GetDeviceCaps(LOGPIXELSX);
	st_OldFont	= st_Dc.SelectObject(GetFont());
	(VOID)st_Dc.GetTextMetrics(&st_Metric);
	(VOID)st_Dc.SelectObject(st_OldFont);
	textHeight	= st_Metric.tmHeight;

	f_Pcs_DefaultConfig(&st_Config, PCS_PRESET_AIR);
	st_Chart.f_SetDpi(dpi);
	f_SetupControls();

	isReady = 1;
	f_SetInitialSize();
	f_Layout();
	f_RunAll();

	return TRUE;
}

VOID CPlotSimUIDlg::f_SetupControls(VOID)
{
	static const LPCTSTR	s_CheckItem[UI_CHECK_ROW_NUM] =
	{
		_T("UNIRAN 평균 / 표준편차"), _T("UNIRAN PDF (bin 10개)"), _T("GAUSS 평균 / 표준편차"), _T("거리 오차 표준편차"),
		_T("방위각 오차 표준편차"), _T("고각 오차 표준편차"), _T("클러터 개수"), _T("FOV 밖 클러터")
	};
	static const LPCTSTR	s_ResultTitle[UI_RESULT_COL_NUM] =
	{
		_T("스캔"), _T("시각 [s]"), _T("종류"), _T("번호"), _T("거리 [m]"), _T("방위각 [°]"), _T("고각 [°]"), _T("위도 [°]"), _T("경도 [°]"), _T("고도 [m]")
	};
	static const INT32		s_ResultWidth[UI_RESULT_COL_NUM] = { 46, 62, 58, 42, 86, 76, 72, 100, 104, 78 };
	INT32					nIndex;

	(VOID)st_PresetCombo.AddString(_T("대공 표적 1개 (과제 조건)"));
	(VOID)st_PresetCombo.AddString(_T("표적 5개 (대공 3, 대함 2)"));
	(VOID)st_PresetCombo.SetCurSel(static_cast<INT32>(PCS_PRESET_AIR));

	(VOID)st_SnrUnitCombo.AddString(_T("선형"));
	(VOID)st_SnrUnitCombo.AddString(_T("dB"));
	(VOID)st_SnrUnitCombo.SetCurSel(static_cast<INT32>(PCS_SNR_LINEAR));

	(VOID)st_ClutterModeCombo.AddString(_T("스캔마다 N개"));
	(VOID)st_ClutterModeCombo.AddString(_T("스캔마다 0 ~ N개"));
	(VOID)st_ClutterModeCombo.SetCurSel(static_cast<INT32>(PCS_CLUTTER_FIXED));

	(VOID)st_ViewCombo.AddString(_T("난수 검증"));
	(VOID)st_ViewCombo.AddString(_T("LLA 전시"));
	(VOID)st_ViewCombo.SetCurSel(CHART_VIEW_MAP);
	st_Chart.f_SetView(CHART_VIEW_MAP);

	SetDlgItemText(IDC_SNR, _T("20"));
	SetDlgItemText(IDC_CLUTTER_NUM, _T("5"));
	SetDlgItemText(IDC_SEED, _T("12345"));

	(VOID)st_CheckList.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
	(VOID)st_CheckList.InsertColumn(UI_CHECK_COL_ITEM, _T("항목"), LVCFMT_LEFT, f_Ui_Scale(150, dpi));
	(VOID)st_CheckList.InsertColumn(UI_CHECK_COL_VALUE, _T("결과"), LVCFMT_LEFT, f_Ui_Scale(170, dpi));
	(VOID)st_CheckList.InsertColumn(UI_CHECK_COL_REF, _T("기준"), LVCFMT_LEFT, f_Ui_Scale(150, dpi));

	for (nIndex = 0; nIndex < UI_CHECK_ROW_NUM; nIndex++)
	{
		(VOID)st_CheckList.InsertItem(nIndex, s_CheckItem[nIndex]);
	}

	// 결과 목록은 가상 목록 (보이는 칸만 f_OnResultGetDispInfo 로 요청).
	// 0 번 열은 항상 왼쪽 정렬이라 빈 열을 0 번에 넣고 실제 열을 1 번부터 넣은 뒤 빈 열을 지움.
	(VOID)st_ResultList.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
	(VOID)st_ResultList.InsertColumn(0, _T(""), LVCFMT_LEFT, 0);

	for (nIndex = 0; nIndex < UI_RESULT_COL_NUM; nIndex++)
	{
		(VOID)st_ResultList.InsertColumn(nIndex + 1, s_ResultTitle[nIndex], (nIndex == 2) ? LVCFMT_LEFT : LVCFMT_RIGHT,
			f_Ui_Scale(s_ResultWidth[nIndex], dpi));
	}

	(VOID)st_ResultList.DeleteColumn(0);

	f_UpdateParamText();
}

// 처음 창 크기. 화면이 더 작으면 화면에 맞춤.
VOID CPlotSimUIDlg::f_SetInitialSize(VOID)
{
	CRect	st_Work;
	CRect	st_Window(0, 0, f_Ui_Scale(UI_INIT_WIDTH, dpi), f_Ui_Scale(UI_INIT_HEIGHT, dpi));
	INT32	width;
	INT32	height;

	(VOID)::AdjustWindowRectEx(&st_Window, GetStyle(), FALSE, GetExStyle());
	(VOID)::SystemParametersInfo(SPI_GETWORKAREA, 0, &st_Work, 0);

	width	= (st_Window.Width() < (st_Work.Width() - 40)) ? st_Window.Width() : (st_Work.Width() - 40);
	height	= (st_Window.Height() < (st_Work.Height() - 40)) ? st_Window.Height() : (st_Work.Height() - 40);

	(VOID)SetWindowPos(nullptr, st_Work.left + ((st_Work.Width() - width) / 2), st_Work.top + ((st_Work.Height() - height) / 2),
		width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

VOID CPlotSimUIDlg::OnGetMinMaxInfo(MINMAXINFO *st_Info)
{
	CRect st_Min(0, 0, f_Ui_Scale(UI_MIN_WIDTH, dpi), f_Ui_Scale(UI_MIN_HEIGHT, dpi));
	CRect st_Work;

	CDialogEx::OnGetMinMaxInfo(st_Info);

	(VOID)::AdjustWindowRectEx(&st_Min, GetStyle(), FALSE, GetExStyle());
	(VOID)::SystemParametersInfo(SPI_GETWORKAREA, 0, &st_Work, 0);

	st_Info->ptMinTrackSize.x = (st_Min.Width() < st_Work.Width()) ? st_Min.Width() : st_Work.Width();
	st_Info->ptMinTrackSize.y = (st_Min.Height() < st_Work.Height()) ? st_Min.Height() : st_Work.Height();
}

VOID CPlotSimUIDlg::OnSize(UINT32 type, INT32 width, INT32 height)
{
	CDialogEx::OnSize(type, width, height);

	if ((type != SIZE_MINIMIZED) && (isReady != 0))
	{
		f_Layout();
	}
}

// 목록에 nRowNum 줄이 스크롤 없이 보이는 높이.
INT32 CPlotSimUIDlg::f_GetListHeight(const CListCtrl *st_List, INT32 nRowNum) const
{
	CRect st_Header;
	CRect st_Item(0, 0, 0, textHeight + f_Ui_Scale(6, dpi));

	st_List->GetHeaderCtrl()->GetWindowRect(&st_Header);

	if (st_List->GetItemCount() > 0)
	{
		(VOID)st_List->GetItemRect(0, &st_Item, LVIR_BOUNDS);
	}

	return st_Header.Height() + (nRowNum * st_Item.Height()) + f_Ui_Scale(6, dpi);
}

// 배치: 위 두 줄(설정, 고정 파라미터), 왼쪽(검증 결과, 결과 목록), 오른쪽(보기 선택, 그림).
VOID CPlotSimUIDlg::f_Layout(VOID)
{
	const INT32	margin = f_Ui_Scale(UI_MARGIN, dpi);
	const INT32	gap = f_Ui_Scale(UI_GAP, dpi);
	const INT32	tight = f_Ui_Scale(4, dpi);
	const INT32	rowHeight = textHeight + f_Ui_Scale(12, dpi);
	const INT32	titleHeight = textHeight + f_Ui_Scale(6, dpi);
	const INT32	drop = f_Ui_Scale(UI_COMBO_DROP, dpi);
	CRect		st_Client;
	CRect		st_Combo;
	CRect		st_ListClient;
	INT32		comboTop;
	INT32		leftWidth;
	INT32		contentTop;
	INT32		checkHeight;
	INT32		rightLeft;
	INT32		usedWidth;
	INT32		x;
	INT32		y;

	GetClientRect(&st_Client);

	// 1 줄: 설정. 콤보와 편집칸은 콤보 높이로 세로 가운데
	st_PresetCombo.GetWindowRect(&st_Combo);
	comboTop	= margin + ((rowHeight - st_Combo.Height()) / 2);
	x			= margin;
	y			= margin;
	f_Ui_Move(this, IDC_PRESET_LABEL, x, y, f_Ui_Scale(34, dpi), rowHeight);
	x = x + f_Ui_Scale(34, dpi);
	f_Ui_Move(this, IDC_PRESET, x, comboTop, f_Ui_Scale(210, dpi), st_Combo.Height() + drop);
	x = x + f_Ui_Scale(210, dpi) + (2 * gap);
	f_Ui_Move(this, IDC_SNR_LABEL, x, y, f_Ui_Scale(32, dpi), rowHeight);
	x = x + f_Ui_Scale(32, dpi);
	f_Ui_Move(this, IDC_SNR, x, comboTop, f_Ui_Scale(52, dpi), st_Combo.Height());
	x = x + f_Ui_Scale(52, dpi) + tight;
	f_Ui_Move(this, IDC_SNR_UNIT, x, comboTop, f_Ui_Scale(64, dpi), st_Combo.Height() + drop);
	x = x + f_Ui_Scale(64, dpi) + (2 * gap);
	f_Ui_Move(this, IDC_CLUTTER_LABEL, x, y, f_Ui_Scale(48, dpi), rowHeight);
	x = x + f_Ui_Scale(48, dpi);
	f_Ui_Move(this, IDC_CLUTTER_NUM, x, comboTop, f_Ui_Scale(40, dpi), st_Combo.Height());
	x = x + f_Ui_Scale(40, dpi) + tight;
	f_Ui_Move(this, IDC_CLUTTER_MODE, x, comboTop, f_Ui_Scale(150, dpi), st_Combo.Height() + drop);
	x = x + f_Ui_Scale(150, dpi) + (2 * gap);
	f_Ui_Move(this, IDC_SEED_LABEL, x, y, f_Ui_Scale(36, dpi), rowHeight);
	x = x + f_Ui_Scale(36, dpi);
	f_Ui_Move(this, IDC_SEED, x, comboTop, f_Ui_Scale(80, dpi), st_Combo.Height());
	f_Ui_Move(this, IDC_SAVE_CSV, st_Client.right - margin - f_Ui_Scale(100, dpi), y, f_Ui_Scale(100, dpi), rowHeight);
	f_Ui_Move(this, IDC_RUN, st_Client.right - margin - f_Ui_Scale(100, dpi) - gap - f_Ui_Scale(88, dpi), y, f_Ui_Scale(88, dpi), rowHeight);

	// 2 줄: 고정 파라미터
	y = margin + rowHeight + tight;
	f_Ui_Move(this, IDC_PARAM_TEXT, margin, y, st_Client.Width() - (2 * margin), titleHeight);
	contentTop = y + titleHeight + gap;

	leftWidth = static_cast<INT32>(static_cast<FLOAT64>(st_Client.Width()) * UI_LEFT_RATIO);
	leftWidth = (leftWidth < f_Ui_Scale(UI_LEFT_MIN, dpi)) ? f_Ui_Scale(UI_LEFT_MIN, dpi) : leftWidth;
	leftWidth = (leftWidth > f_Ui_Scale(UI_LEFT_MAX, dpi)) ? f_Ui_Scale(UI_LEFT_MAX, dpi) : leftWidth;

	// 왼쪽 위: 검증 결과. 마지막 열이 남은 폭을 채움
	f_Ui_Move(this, IDC_CHECK_TITLE, margin, contentTop, leftWidth, titleHeight);
	checkHeight = f_GetListHeight(&st_CheckList, UI_CHECK_ROW_NUM);
	f_Ui_Move(this, IDC_CHECK_LIST, margin, contentTop + titleHeight, leftWidth, checkHeight);
	st_CheckList.GetClientRect(&st_ListClient);
	usedWidth = st_CheckList.GetColumnWidth(UI_CHECK_COL_ITEM) + st_CheckList.GetColumnWidth(UI_CHECK_COL_VALUE);
	(VOID)st_CheckList.SetColumnWidth(UI_CHECK_COL_REF, ((st_ListClient.Width() - usedWidth) > f_Ui_Scale(120, dpi)) ? (st_ListClient.Width() - usedWidth)
		: f_Ui_Scale(120, dpi));

	// 왼쪽 아래: 결과 목록
	y = contentTop + titleHeight + checkHeight + gap;
	f_Ui_Move(this, IDC_RESULT_TITLE, margin, y, leftWidth, titleHeight);
	f_Ui_Move(this, IDC_RESULT_LIST, margin, y + titleHeight, leftWidth, st_Client.bottom - margin - (y + titleHeight));

	// 오른쪽: 보기 선택, 그림
	rightLeft = margin + leftWidth + gap;
	st_ViewCombo.GetWindowRect(&st_Combo);
	f_Ui_Move(this, IDC_VIEW_LABEL, rightLeft, contentTop, f_Ui_Scale(34, dpi), rowHeight);
	f_Ui_Move(this, IDC_VIEW, rightLeft + f_Ui_Scale(34, dpi), contentTop + ((rowHeight - st_Combo.Height()) / 2), f_Ui_Scale(160, dpi), st_Combo.Height() + drop);
	f_Ui_Move(this, IDC_CHART, rightLeft, contentTop + rowHeight + tight, st_Client.right - margin - rightLeft,
		st_Client.bottom - margin - (contentTop + rowHeight + tight));

	Invalidate(TRUE);
}

// Enter 로 창이 닫히지 않게 함 ('실행' 이 기본 단추).
VOID CPlotSimUIDlg::OnOK(VOID)
{
}

// Esc 로 창이 닫히지 않게 함.
VOID CPlotSimUIDlg::OnCancel(VOID)
{
}

VOID CPlotSimUIDlg::OnClose(VOID)
{
	EndDialog(IDCANCEL);
}

// 설정 칸 -> ST_PcsConfig. 값이 잘못되면 0 과 안내 글자.
INT32 CPlotSimUIDlg::f_ReadConfig(ST_PcsConfig *st_NewConfig, CString *st_Error)
{
	CString	st_Text;
	FLOAT64	snr = 0.0;
	FLOAT64	clutterNum = 0.0;
	FLOAT64	seed = 0.0;

	f_Pcs_DefaultConfig(st_NewConfig, (st_PresetCombo.GetCurSel() == static_cast<INT32>(PCS_PRESET_MULTI)) ? PCS_PRESET_MULTI : PCS_PRESET_AIR);

	(VOID)GetDlgItemText(IDC_SNR, st_Text);

	if ((f_Ui_ParseNumber(st_Text, &snr) == 0) || ((st_SnrUnitCombo.GetCurSel() != static_cast<INT32>(PCS_SNR_DB)) && (snr <= 0.0)))
	{
		*st_Error = _T("SNR 을 확인하세요. 선형값은 0 보다 커야 합니다.");
		return 0;
	}

	(VOID)GetDlgItemText(IDC_CLUTTER_NUM, st_Text);

	if ((f_Ui_ParseNumber(st_Text, &clutterNum) == 0) || (clutterNum < 0.0) || (clutterNum > static_cast<FLOAT64>(PCS_MAX_CLUTTER))
		|| (floor(clutterNum) != clutterNum))
	{
		st_Error->Format(_T("클러터 수는 0 ~ %d 사이 정수로 넣으세요."), PCS_MAX_CLUTTER);
		return 0;
	}

	(VOID)GetDlgItemText(IDC_SEED, st_Text);

	if ((f_Ui_ParseNumber(st_Text, &seed) == 0) || (seed < 1.0) || (seed > UI_SEED_MAX) || (floor(seed) != seed))
	{
		*st_Error = _T("seed 는 1 ~ 2147483646 사이 정수로 넣으세요.");
		return 0;
	}

	st_NewConfig->st_Radar.snr			= snr;
	st_NewConfig->st_Radar.enSnrUnit	= (st_SnrUnitCombo.GetCurSel() == static_cast<INT32>(PCS_SNR_DB)) ? PCS_SNR_DB : PCS_SNR_LINEAR;
	st_NewConfig->st_Clutter.nClutterNum	= static_cast<INT32>(clutterNum);
	st_NewConfig->st_Clutter.enMode			= (st_ClutterModeCombo.GetCurSel() == static_cast<INT32>(PCS_CLUTTER_RANDOM)) ? PCS_CLUTTER_RANDOM
		: PCS_CLUTTER_FIXED;
	st_NewConfig->seed					= static_cast<INT32>(seed);

	return 1;
}

VOID CPlotSimUIDlg::f_OnRunClicked(VOID)
{
	f_RunAll();
}

// 난수 검증과 600 스캔을 실행하고 목록, 그림을 다시 만듦.
VOID CPlotSimUIDlg::f_RunAll(VOID)
{
	ST_PcsConfig	st_NewConfig;
	ST_PcsScan		st_Scan;
	CString			st_Error;
	INT32			nPart;

	if (f_ReadConfig(&st_NewConfig, &st_Error) == 0)
	{
		(VOID)AfxMessageBox(st_Error, MB_ICONWARNING);
		return;
	}

	CWaitCursor st_Wait;

	st_Config = st_NewConfig;

	// 난수 검증. 검증마다 seed 를 다시 맞춰 PlotSimTest 와 같은 값이 나오게 함
	f_Pcs_SetSeed(st_Config.seed);
	(VOID)f_Pcs_TestUniform(&st_Data.st_Uniform, PCS_RAND_TEST_NUM, PCS_UNIFORM_BIN_NUM);
	f_Pcs_SetSeed(st_Config.seed);
	(VOID)f_Pcs_TestGauss(&st_Data.st_Gauss, PCS_RAND_TEST_NUM, PCS_GAUSS_BIN_NUM, PCS_GAUSS_BIN_MIN, PCS_GAUSS_BIN_MAX);

	st_Data.st_Points.clear();
	st_Data.nPlotNum	= 0;
	st_Data.nClutterNum	= 0;
	nErrNum				= 0;
	nScanDone			= 0;
	nClutterOut			= 0;

	for (nPart = 0; nPart < 3; nPart++)
	{
		errSum[nPart]	= 0.0;
		errSumSq[nPart]	= 0.0;
	}

	if (f_Pcs_InitSim(&st_State, &st_Config) != PCS_PASS)
	{
		(VOID)AfxMessageBox(_T("설정 값이 잘못되어 시뮬레이션을 시작할 수 없습니다."), MB_ICONWARNING);
	}
	else
	{
		st_Data.st_RadarLla	= st_State.st_Sim.st_Sample.st_Platform.st_Lla;
		st_Data.nTargetNum	= st_State.st_Sim.st_Sample.nTargetNum;
		st_Data.st_Points.reserve(static_cast<size_t>(st_State.nScanNum) * static_cast<size_t>((2 * st_Data.nTargetNum) + st_Config.st_Clutter.nClutterNum));

		while (f_Pcs_StepScan(&st_State, &st_Scan) == PCS_PASS)
		{
			f_AddScan(&st_Scan);
		}
	}

	f_UpdateCheckList();
	f_UpdateParamText();
	(VOID)st_ResultList.SetItemCountEx(static_cast<INT32>(st_Data.st_Points.size()), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
	st_ResultList.Invalidate(FALSE);
	st_Chart.f_SetData(&st_Data);
}

// 스캔 하나의 결과를 목록 / 그림 자료와 검증 값에 더함.
VOID CPlotSimUIDlg::f_AddScan(const ST_PcsScan *st_Scan)
{
	const ST_PcsClutterCfg	*st_Cfg = &st_Config.st_Clutter;
	ST_UiPoint				st_Ui;
	FLOAT64					diff[3];
	INT32					nTarget;
	INT32					nClutter;
	INT32					nPart;

	st_Ui.nScan = st_Scan->nScanIndex;

	for (nTarget = 0; nTarget < st_Scan->nTargetNum; nTarget++)
	{
		const ST_PcsPoint *st_True = &st_Scan->st_True[nTarget];
		const ST_PcsPoint *st_Plot = &st_Scan->st_Plot[nTarget];

		st_Ui.st_Point = *st_True;
		st_Data.st_Points.push_back(st_Ui);
		st_Ui.st_Point = *st_Plot;
		st_Data.st_Points.push_back(st_Ui);
		st_Data.nPlotNum++;

		// 플롯 오차 z - x. 방위각은 -180 / 180 경계를 넘는 경우를 맞춤
		diff[0] = st_Plot->st_Sph.rng - st_True->st_Sph.rng;
		diff[1] = st_Plot->st_Sph.azi - st_True->st_Sph.azi;
		diff[2] = st_Plot->st_Sph.ele - st_True->st_Sph.ele;
		diff[1] = (diff[1] > 180.0) ? (diff[1] - 360.0) : ((diff[1] < -180.0) ? (diff[1] + 360.0) : diff[1]);

		for (nPart = 0; nPart < 3; nPart++)
		{
			errSum[nPart]	= errSum[nPart] + diff[nPart];
			errSumSq[nPart]	= errSumSq[nPart] + (diff[nPart] * diff[nPart]);
		}

		nErrNum++;
	}

	for (nClutter = 0; nClutter < st_Scan->nClutterNum; nClutter++)
	{
		const ST_PcsSph *st_Sph = &st_Scan->st_Clutter[nClutter].st_Sph;

		st_Ui.st_Point = st_Scan->st_Clutter[nClutter];
		st_Data.st_Points.push_back(st_Ui);
		st_Data.nClutterNum++;

		if ((st_Sph->rng < st_Cfg->rngMin) || (st_Sph->rng > st_Cfg->rngMax) || (st_Sph->azi < st_Cfg->aziMin) || (st_Sph->azi > st_Cfg->aziMax)
			|| (st_Sph->ele < st_Cfg->eleMin) || (st_Sph->ele > st_Cfg->eleMax))
		{
			nClutterOut++;
		}
	}

	nScanDone++;
}

// 검증 결과 목록 갱신.
VOID CPlotSimUIDlg::f_UpdateCheckList(VOID)
{
	const ST_PcsHist	*st_Uniform = &st_Data.st_Uniform;
	const ST_PcsSigma	*st_Sigma = &st_State.st_Sigma;
	CString				st_Value[UI_CHECK_ROW_NUM];
	CString				st_Ref[UI_CHECK_ROW_NUM];
	FLOAT64				minPdf = HUGE_VAL;
	FLOAT64				maxPdf = 0.0;
	FLOAT64				mean;
	FLOAT64				errStd[3] = { 0.0, 0.0, 0.0 };
	INT32				nIndex;

	for (nIndex = 0; nIndex < st_Uniform->nBin; nIndex++)
	{
		minPdf = fmin(minPdf, st_Uniform->pdf[nIndex]);
		maxPdf = fmax(maxPdf, st_Uniform->pdf[nIndex]);
	}

	for (nIndex = 0; (nIndex < 3) && (nErrNum > 0); nIndex++)
	{
		mean			= errSum[nIndex] / static_cast<FLOAT64>(nErrNum);
		errStd[nIndex]	= sqrt(fmax(0.0, (errSumSq[nIndex] / static_cast<FLOAT64>(nErrNum)) - (mean * mean)));
	}

	st_Value[0].Format(_T("%.4f / %.4f"), st_Uniform->mean, st_Uniform->std);
	st_Ref[0].Format(_T("0.5 / %.4f"), 1.0 / sqrt(12.0));
	st_Value[1].Format(_T("%.3f ~ %.3f"), (st_Uniform->nBin > 0) ? minPdf : 0.0, maxPdf);
	st_Ref[1] = _T("1");
	st_Value[2].Format(_T("%+.4f / %.4f"), st_Data.st_Gauss.mean, st_Data.st_Gauss.std);
	st_Ref[2] = _T("0 / 1");
	st_Value[3].Format(_T("%.3f m  (%d개)"), errStd[0], nErrNum);
	st_Ref[3].Format(_T("p_rng %.3f m"), st_Sigma->rng);
	st_Value[4].Format(_T("%.4f°"), errStd[1]);
	st_Ref[4].Format(_T("p_azi %.4f°"), st_Sigma->azi);
	st_Value[5].Format(_T("%.4f°"), errStd[2]);
	st_Ref[5].Format(_T("p_ele %.4f°"), st_Sigma->ele);
	st_Value[6].Format(_T("%d개 (스캔당 %.2f)"), st_Data.nClutterNum,
		(nScanDone > 0) ? (static_cast<FLOAT64>(st_Data.nClutterNum) / static_cast<FLOAT64>(nScanDone)) : 0.0);

	if (st_Config.st_Clutter.enMode == PCS_CLUTTER_RANDOM)
	{
		st_Ref[6].Format(_T("스캔당 0 ~ %d개"), st_Config.st_Clutter.nClutterNum);
	}
	else
	{
		st_Ref[6].Format(_T("스캔당 %d개"), st_Config.st_Clutter.nClutterNum);
	}

	st_Value[7].Format(_T("%d개"), nClutterOut);
	st_Ref[7] = _T("0개");

	for (nIndex = 0; nIndex < UI_CHECK_ROW_NUM; nIndex++)
	{
		(VOID)st_CheckList.SetItemText(nIndex, UI_CHECK_COL_VALUE, st_Value[nIndex]);
		(VOID)st_CheckList.SetItemText(nIndex, UI_CHECK_COL_REF, st_Ref[nIndex]);
	}
}

// 고정 파라미터 한 줄.
VOID CPlotSimUIDlg::f_UpdateParamText(VOID)
{
	const ST_PcsRadar		*st_Radar = &st_Config.st_Radar;
	const ST_PcsClutterCfg	*st_Clutter = &st_Config.st_Clutter;
	CString					st_Text;

	st_Text.Format(_T("대역폭 %.0f MHz,  빔폭 %.2f° / %.2f°,  K_M %.1f / %.1f,  클러터 FOV  거리 %.0f ~ %.0f m,  방위각 %.0f ~ %.0f°,  고각 %.0f ~ %.0f°,  %.0f s (%.1f s 간격)"),
		st_Radar->bandwidth / 1.0e6, st_Radar->beamAzi, st_Radar->beamEle, st_Radar->kmAzi, st_Radar->kmEle, st_Clutter->rngMin, st_Clutter->rngMax,
		st_Clutter->aziMin, st_Clutter->aziMax, st_Clutter->eleMin, st_Clutter->eleMax, st_Config.st_Target.durationTime, st_Config.st_Target.stepTime);
	SetDlgItemText(IDC_PARAM_TEXT, st_Text);
}

// 결과 목록 칸 하나의 글자.
CString CPlotSimUIDlg::f_GetCellText(INT32 nRow, INT32 nColumn) const
{
	static const LPCTSTR	s_KindName[3] = { _T("참값"), _T("플롯"), _T("클러터") };
	const ST_UiPoint		*st_Ui = &st_Data.st_Points[static_cast<size_t>(nRow)];
	const ST_PcsPoint		*st_Point = &st_Ui->st_Point;
	const INT32				nKind = static_cast<INT32>(st_Point->enKind);
	CString					st_Text;

	switch (nColumn)
	{
	case 0:
		st_Text.Format(_T("%d"), st_Ui->nScan);
		break;

	case 1:
		st_Text.Format(_T("%.1f"), st_Point->time);
		break;

	case 2:
		st_Text = ((nKind >= 0) && (nKind < 3)) ? s_KindName[nKind] : _T("?");
		break;

	case 3:
		st_Text.Format(_T("%d"), st_Point->nId);
		break;

	case 4:
		st_Text.Format(_T("%.2f"), st_Point->st_Sph.rng);
		break;

	case 5:
		st_Text.Format(_T("%.4f"), st_Point->st_Sph.azi);
		break;

	case 6:
		st_Text.Format(_T("%.4f"), st_Point->st_Sph.ele);
		break;

	case 7:
		st_Text.Format(_T("%.7f"), f_Rad_To_Deg(st_Point->st_Lla.Lat));
		break;

	case 8:
		st_Text.Format(_T("%.7f"), f_Rad_To_Deg(st_Point->st_Lla.Lon));
		break;

	case 9:
		st_Text.Format(_T("%.2f"), st_Point->st_Lla.Alt);
		break;

	default:
		break;
	}

	return st_Text;
}

// 가상 목록이 요청한 칸의 글자. 보이는 줄만 요청됨.
VOID CPlotSimUIDlg::f_OnResultGetDispInfo(NMHDR *st_Hdr, LRESULT *pt_Result)
{
	NMLVDISPINFO	*st_Info = reinterpret_cast<NMLVDISPINFO *>(st_Hdr);
	CString			st_Text;

	if (((st_Info->item.mask & LVIF_TEXT) != 0U) && (st_Info->item.cchTextMax > 0) && (st_Info->item.iItem >= 0)
		&& (st_Info->item.iItem < static_cast<INT32>(st_Data.st_Points.size())))
	{
		st_Text = f_GetCellText(st_Info->item.iItem, st_Info->item.iSubItem);
		(VOID)_tcsncpy_s(st_Info->item.pszText, static_cast<size_t>(st_Info->item.cchTextMax), st_Text.GetString(), _TRUNCATE);
	}

	*pt_Result = 0;
}

VOID CPlotSimUIDlg::f_OnViewChanged(VOID)
{
	st_Chart.f_SetView((st_ViewCombo.GetCurSel() == CHART_VIEW_RANDOM) ? CHART_VIEW_RANDOM : CHART_VIEW_MAP);
}

// 결과 전체 CSV 저장 (한 줄 = 점 하나).
VOID CPlotSimUIDlg::f_OnSaveCsvClicked(VOID)
{
	static const CHAR	*s_KindName[3] = { "TRUE", "PLOT", "CLUTTER" };
	CFileDialog			st_FileDlg(FALSE, _T("csv"), _T("PlotSim_Result.csv"), OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY,
							_T("CSV (*.csv)|*.csv||"), this);
	FILE				*st_File = nullptr;
	INT32				nKind;

	if (st_FileDlg.DoModal() != IDOK)
	{
		return;
	}

	(VOID)_wfopen_s(&st_File, st_FileDlg.GetPathName().GetString(), L"wb");

	if (st_File == nullptr)
	{
		(VOID)AfxMessageBox(_T("CSV 파일을 열 수 없습니다. 다른 프로그램에서 열려 있는지 확인하세요."), MB_ICONWARNING);
		return;
	}

	(VOID)fputs("scan,time_s,kind,id,rng_m,azi_deg,ele_deg,lat_deg,lon_deg,alt_m\r\n", st_File);

	for (const ST_UiPoint &st_Ui : st_Data.st_Points)
	{
		const ST_PcsPoint *st_Point = &st_Ui.st_Point;

		nKind = static_cast<INT32>(st_Point->enKind);
		(VOID)fprintf(st_File, "%d,%.3f,%s,%d,%.4f,%.6f,%.6f,%.9f,%.9f,%.4f\r\n", st_Ui.nScan, st_Point->time,
			((nKind >= 0) && (nKind < 3)) ? s_KindName[nKind] : "?", st_Point->nId, st_Point->st_Sph.rng, st_Point->st_Sph.azi, st_Point->st_Sph.ele,
			f_Rad_To_Deg(st_Point->st_Lla.Lat), f_Rad_To_Deg(st_Point->st_Lla.Lon), st_Point->st_Lla.Alt);
	}

	(VOID)fclose(st_File);
}
