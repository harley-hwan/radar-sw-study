// 주 화면: 시나리오 · 추적 필터 파라미터, 송수신 시작, 그림 · 트랙 표 · 요약
#include "pch.h"
#include "framework.h"

#include <algorithm>
#include <cmath>

#include "ScoreDlg.h"
#include "TargetSimUI.h"
#include "TargetSimUIDlg.h"
#include "UiCommon.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define UI_INIT_WIDTH			1680					// [px @96dpi]
#define UI_INIT_HEIGHT			1000
#define UI_MIN_WIDTH			1160					// 머리 줄과 아래 두 카드가 들어가는 너비
#define UI_MIN_HEIGHT			780						// 필터 카드가 다 들어가고 그림이 남는 높이
#define UI_MARGIN				12
#define UI_GAP					10
#define UI_CARD_PAD				12
#define UI_CARD_RADIUS			10
#define UI_PLOT_RATIO			0.62					// 그림 카드 높이 비율. 나머지는 필터 카드, 트랙 표 카드
#define UI_3D_RATIO				0.56					// 3D + 2D 보기에서 3D 너비 비율
#define UI_TRK_RATIO			0.36					// 필터 카드 (왼쪽 아래) 너비 비율
#define UI_TRK_MIN				560						// 필터 카드 입력 줄, 요약 줄이 잘리지 않는 너비
#define UI_TRK_MAX				660
#define UI_BUTTON_WIDTH			110						// 스코어 확인, CSV 저장 단추 너비
#define UI_APPLY_WIDTH			128						// 적용 · 다시 실행 단추 너비
#define UI_PRESET_WIDTH			80						// 과제 값, 조정 값 단추 너비
#define UI_EDIT_WIDTH			64						// 파라미터 입력 칸 너비
#define UI_CHECK_BOX_PAD		24						// 체크 상자 네모와 글자 앞 간격
#define UI_DROP_HEIGHT			240						// 콤보 목록 펼친 높이
#define UI_SCN_COMBO_WIDTH		140						// 시나리오 선택 너비
#define UI_SCAN_TEXT_WIDTH		200						// 스캔 번호 글자 너비
#define UI_VIEW_MODE_WIDTH		170						// 그림 보기 선택 너비
#define UI_TRK_GAP_NUM			3						// 송신 간격 고르기 수
#define UI_TRK_GAP_WIDTH		128						// 송신 간격 선택 너비
#define UI_SUMMARY_LINES		9						// 필터 카드 요약 줄 수
#define UI_FRAMED_NUM			6						// 테두리를 그리는 컨트롤 수
#define UI_TIMER_TRACK			1						// 수신 결과를 가져오는 타이머
#define UI_TIMER_TRACK_MS		50

// 송신 간격 [ms] 과 이름. 100 ms 가 스캔 간격 (0.1 s) 과 같은 실시간.
static const INT32		s_GapMs[UI_TRK_GAP_NUM] = { 0, 10, 100 };
static const LPCTSTR	s_GapName[UI_TRK_GAP_NUM] = { _T("0 ms (바로)"), _T("10 ms (빨리)"), _T("100 ms (실시간)") };
static const LPCTSTR	s_ViewName[UI_VIEW_NUM] = { _T("3D + 2D"), _T("3D (경도-위도-고도)"), _T("2D (위도·경도-고도)"), _T("평면 (위도-경도)"),
	_T("추적 성능 (오차 · NIS)") };

// 그림 표시 체크 상자. UI_SHOW_* 순서
static const INT32		s_ShowId[UI_SHOW_NUM] = { IDC_SHOW_TRUE, IDC_SHOW_PLOT, IDC_SHOW_TRACK };

// 파라미터 입력 칸 (UI_PRM_* 순서), 이름표, 허용 범위
static const INT32		s_PrmEditId[UI_PRM_NUM] = { IDC_PRM_SIGR, IDC_PRM_SIGAZ, IDC_PRM_SIGEL, IDC_PRM_Q, IDC_PRM_TAU };
static const INT32		s_PrmLabelId[UI_PRM_NUM] = { IDC_PRM_SIGR_LABEL, IDC_PRM_SIGAZ_LABEL, IDC_PRM_SIGEL_LABEL, IDC_PRM_Q_LABEL, IDC_PRM_TAU_LABEL };
static const LPCTSTR	s_PrmName[UI_PRM_NUM] = { _T("R 거리 표준편차 [m]"), _T("R 방위각 표준편차 [°]"), _T("R 고각 표준편차 [°]"), _T("Singer q [m/s²]"),
	_T("Singer τ [s]") };
static const FLOAT64	s_PrmMax[UI_PRM_NUM] = { 10000.0, 30.0, 30.0, 1000.0, 100000.0 };
static const INT32		s_PrmZeroOk[UI_PRM_NUM] = { 0, 0, 0, 1, 0 };	// q 만 0 허용 (Q = 0)

// 필터 카드 요약 항목 이름 (빈 칸 = 윗줄 이어짐, f_UpdateTrackSummary 줄 순서)
static const LPCTSTR	s_SummaryLabel[UI_SUMMARY_LINES] = { _T("송수신"), _T(""), _T("트랙"), _T("지금 스캔"), _T(""), _T("누적"), _T(""), _T("적용 값"), _T("") };

// 컨트롤 위치, 크기 변경. 창이 최소 크기보다 작아도 너비, 높이는 0 아래로 내려가지 않음.
static VOID f_Ui_Move(CWnd *st_Dlg, INT32 ctrlId, INT32 left, INT32 top, INT32 width, INT32 height)
{
	(VOID)st_Dlg->GetDlgItem(ctrlId)->SetWindowPos(nullptr, left, top, (width > 0) ? width : 0, (height > 0) ? height : 0, SWP_NOZORDER | SWP_NOACTIVATE);
}

// 컨트롤 글자 폭 [px]. 여러 줄이면 가장 긴 줄.
static INT32 f_Ui_TextWidth(CWnd *st_Ctrl)
{
	CClientDC	st_Dc(st_Ctrl);
	CFont		*st_OldFont = st_Dc.SelectObject(st_Ctrl->GetFont());
	CString		st_Text;
	CString		st_Line;
	INT32		nPos = 0;
	INT32		lineWidth;
	INT32		width = 0;

	st_Ctrl->GetWindowText(st_Text);

	// Tokenize 는 줄이 더 없으면 nPos 를 -1 로 둠 (빈 줄은 건너뜀)
	for (st_Line = st_Text.Tokenize(_T("\r\n"), nPos); nPos >= 0; st_Line = st_Text.Tokenize(_T("\r\n"), nPos))
	{
		lineWidth	= st_Dc.GetTextExtent(st_Line).cx;
		width		= (lineWidth > width) ? lineWidth : width;
	}

	(VOID)st_Dc.SelectObject(st_OldFont);

	return width;
}

// 세 성분의 크기
static FLOAT64 f_Ui_Norm(const FLOAT64 pt_Value[3])
{
	return sqrt((pt_Value[0] * pt_Value[0]) + (pt_Value[1] * pt_Value[1]) + (pt_Value[2] * pt_Value[2]));
}

BEGIN_MESSAGE_MAP(CTargetSimUIDlg, CDialogEx)
	ON_WM_ERASEBKGND()
	ON_WM_CTLCOLOR()
	ON_WM_SIZE()
	ON_WM_GETMINMAXINFO()
	ON_WM_CLOSE()
	ON_WM_DESTROY()
	ON_WM_TIMER()
	ON_WM_HSCROLL()
	ON_WM_MOUSEWHEEL()
	ON_BN_CLICKED(IDC_SHOW_TRUE, &CTargetSimUIDlg::f_OnShowClicked)
	ON_BN_CLICKED(IDC_SHOW_PLOT, &CTargetSimUIDlg::f_OnShowClicked)
	ON_BN_CLICKED(IDC_SHOW_TRACK, &CTargetSimUIDlg::f_OnShowClicked)
	ON_CBN_SELCHANGE(IDC_VIEW_MODE, &CTargetSimUIDlg::f_OnViewModeChanged)
	ON_CBN_SELCHANGE(IDC_SCN_COMBO, &CTargetSimUIDlg::f_OnScenarioChanged)
	ON_CBN_SELCHANGE(IDC_TRK_GAP, &CTargetSimUIDlg::f_OnTrkGapChanged)
	ON_BN_CLICKED(IDC_TRK_RESEND, &CTargetSimUIDlg::f_OnApplyClicked)
	ON_BN_CLICKED(IDC_PRM_GIVEN, &CTargetSimUIDlg::f_OnGivenClicked)
	ON_BN_CLICKED(IDC_PRM_TUNED, &CTargetSimUIDlg::f_OnTunedClicked)
	ON_BN_CLICKED(IDC_TRK_SCORE, &CTargetSimUIDlg::f_OnTrkScoreClicked)
	ON_BN_CLICKED(IDC_SAVE_CSV, &CTargetSimUIDlg::f_OnSaveCsvClicked)
	ON_NOTIFY(LVN_GETDISPINFO, IDC_TRK_LIST, &CTargetSimUIDlg::f_OnTrackGetDispInfo)
END_MESSAGE_MAP()

// 주 대화상자. 그림 표시는 처음에 모두 켬, 시나리오는 대함부터.
CTargetSimUIDlg::CTargetSimUIDlg(CWnd *st_Parent)
	: CDialogEx(IDD_TARGETSIMUI_DIALOG, st_Parent)
	, st_TopBar(0, 0, 0, 0)
	, st_CardPlot(0, 0, 0, 0)
	, st_CardTrk(0, 0, 0, 0)
	, st_CardList(0, 0, 0, 0)
	, dpi(UI_BASE_DPI)
	, textHeight(16)
	, isShow{ 1, 1, 1 }
	, sendGapMs(s_GapMs[0])
	, nCurScan(0)
	, isFollowScan(1)
	, nViewMode(UI_VIEW_ALL)
	, nScenario(SCN_SHIP)
{
}

// 컨트롤 변수와 대화상자 자원 연결.
VOID CTargetSimUIDlg::DoDataExchange(CDataExchange *st_Dx)
{
	CDialogEx::DoDataExchange(st_Dx);
	DDX_Control(st_Dx, IDC_PLOT, st_Plot);
	DDX_Control(st_Dx, IDC_VIEW3D, st_View3D);
	DDX_Control(st_Dx, IDC_PROFILE_LAT, st_ProfileLat);
	DDX_Control(st_Dx, IDC_PROFILE_LON, st_ProfileLon);
	DDX_Control(st_Dx, IDC_FILTER_PLOT, st_FilterPlot);
	DDX_Control(st_Dx, IDC_SCAN_SLIDER, st_ScanSlider);
	DDX_Control(st_Dx, IDC_VIEW_MODE, st_ViewMode);
	DDX_Control(st_Dx, IDC_SCN_COMBO, st_ScnCombo);
	DDX_Control(st_Dx, IDC_TRK_GAP, st_GapCombo);
	DDX_Control(st_Dx, IDC_TRK_LIST, st_TrackList);
}

// 화면 준비 후 시나리오 계산, 송수신 시작 (파라미터는 과제 값)
BOOL CTargetSimUIDlg::OnInitDialog(VOID)
{
	CClientDC	st_Dc(this);
	CFont		*st_OldFont;
	LOGFONT		st_LogFont;
	TEXTMETRIC	st_Metric;
	ST_TrkParam	st_Param;
	CString		st_Text;
	INT32		nIndex;

	(VOID)CDialogEx::OnInitDialog();

	dpi			= st_Dc.GetDeviceCaps(LOGPIXELSX);
	st_OldFont	= st_Dc.SelectObject(GetFont());
	(VOID)st_Dc.GetTextMetrics(&st_Metric);
	(VOID)st_Dc.SelectObject(st_OldFont);
	textHeight	= st_Metric.tmHeight;

	(VOID)st_CardBrush.CreateSolidBrush(UI_COLOR_CARD);
	(VOID)st_PageBrush.CreateSolidBrush(UI_COLOR_PAGE);

	// 카드 제목은 조금 크고 굵게
	(VOID)GetFont()->GetLogFont(&st_LogFont);
	st_LogFont.lfWeight = FW_BOLD;
	st_LogFont.lfHeight = ::MulDiv(st_LogFont.lfHeight, 108, 100);
	(VOID)st_TitleFont.CreateFontIndirect(&st_LogFont);
	GetDlgItem(IDC_TRK_TITLE)->SetFont(&st_TitleFont);
	GetDlgItem(IDC_TRK_LIST_TITLE)->SetFont(&st_TitleFont);

	// 위 막대: 시나리오 선택
	for (nIndex = 0; nIndex < SCN_NUM; nIndex++)
	{
		(VOID)st_ScnCombo.AddString(f_Scn_GetName(nIndex));
	}

	(VOID)st_ScnCombo.SetCurSel(nScenario);

	// 그림: 보기 선택, 표시
	for (nIndex = 0; nIndex < UI_VIEW_NUM; nIndex++)
	{
		(VOID)st_ViewMode.AddString(s_ViewName[nIndex]);
	}

	(VOID)st_ViewMode.SetCurSel(nViewMode);

	for (nIndex = 0; nIndex < UI_SHOW_NUM; nIndex++)
	{
		CheckDlgButton(s_ShowId[nIndex], (isShow[nIndex] != 0) ? BST_CHECKED : BST_UNCHECKED);
	}

	// 필터 카드: 파라미터 (과제 값), 송신 간격, 요약 항목 이름 (배치가 항목 열 너비를 재므로 배치 전에 씀)
	f_Trk_DefaultParam(&st_Param);
	f_SetParamText(&st_Param);

	for (nIndex = 0; nIndex < UI_TRK_GAP_NUM; nIndex++)
	{
		(VOID)st_GapCombo.AddString(s_GapName[nIndex]);
	}

	(VOID)st_GapCombo.SetCurSel(0);
	st_Text.Empty();

	for (nIndex = 0; nIndex < UI_SUMMARY_LINES; nIndex++)
	{
		st_Text += s_SummaryLabel[nIndex];
		st_Text += (nIndex < (UI_SUMMARY_LINES - 1)) ? _T("\r\n") : _T("");
	}

	SetDlgItemText(IDC_TRK_SUMMARY_LABEL, st_Text);

	// 트랙 표 (가상 리스트)
	(VOID)st_TrackList.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
	f_SetupTrackColumns();

	st_Plot.f_SetDpi(dpi);
	st_View3D.f_SetDpi(dpi);
	st_ProfileLat.f_SetDpi(dpi);
	st_ProfileLon.f_SetDpi(dpi);
	st_FilterPlot.f_SetDpi(dpi);
	st_ProfileLat.f_SetAxis(PROFILE_LAT);
	st_ProfileLon.f_SetAxis(PROFILE_LON);
	f_OnShowClicked();

	// 창 크기가 그대로면 WM_SIZE 가 오지 않으므로 배치는 직접 한 번 함
	f_SetInitialSize();
	f_Layout();
	f_RunScenario();

	// 방향 키, Page Up / Down 으로 스캔을 넘겨 볼 수 있게 슬라이더에 포커스
	GotoDlgCtrl(&st_ScanSlider);

	return FALSE;
}

// 배치

// 초기 창 크기 설정. 화면이 더 작으면 화면에 맞춤.
VOID CTargetSimUIDlg::f_SetInitialSize(VOID)
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

// 최소 창 크기. 작은 화면에서는 화면 크기까지만.
VOID CTargetSimUIDlg::OnGetMinMaxInfo(MINMAXINFO *st_Info)
{
	CRect st_Min(0, 0, f_Ui_Scale(UI_MIN_WIDTH, dpi), f_Ui_Scale(UI_MIN_HEIGHT, dpi));
	CRect st_Work;

	CDialogEx::OnGetMinMaxInfo(st_Info);

	(VOID)::AdjustWindowRectEx(&st_Min, GetStyle(), FALSE, GetExStyle());
	(VOID)::SystemParametersInfo(SPI_GETWORKAREA, 0, &st_Work, 0);

	st_Info->ptMinTrackSize.x = (st_Min.Width() < st_Work.Width()) ? st_Min.Width() : st_Work.Width();
	st_Info->ptMinTrackSize.y = (st_Min.Height() < st_Work.Height()) ? st_Min.Height() : st_Work.Height();
}

// 크기 변경 시 배치 재계산. 생성 중 (컨트롤 연결 전), 최소화 때는 생략.
VOID CTargetSimUIDlg::OnSize(UINT32 type, INT32 width, INT32 height)
{
	CDialogEx::OnSize(type, width, height);

	if ((type != SIZE_MINIMIZED) && (st_TrackList.GetSafeHwnd() != nullptr))
	{
		f_Layout();
	}
}

// 배치: 위 막대 (시나리오), 그림 카드 (위), 필터 카드 (왼쪽 아래), 트랙 표 카드 (오른쪽 아래)
VOID CTargetSimUIDlg::f_Layout(VOID)
{
	const INT32	margin = f_Ui_Scale(UI_MARGIN, dpi);
	const INT32	gap = f_Ui_Scale(UI_GAP, dpi);
	const INT32	pad = f_Ui_Scale(UI_CARD_PAD, dpi);
	const INT32	tight = f_Ui_Scale(6, dpi);
	const INT32	buttonHeight = textHeight + f_Ui_Scale(12, dpi);
	const INT32	buttonWidth = f_Ui_Scale(UI_BUTTON_WIDTH, dpi);
	const INT32	editWidth = f_Ui_Scale(UI_EDIT_WIDTH, dpi);
	const INT32	scanTextWidth = f_Ui_Scale(UI_SCAN_TEXT_WIDTH, dpi);
	const INT32	viewModeWidth = f_Ui_Scale(UI_VIEW_MODE_WIDTH, dpi);
	CRect		st_Client;
	CRect		st_Combo;
	INT32		contentTop;
	INT32		contentBottom;
	INT32		plotHeight;
	INT32		trkHeight;
	INT32		trkWidth;
	INT32		labelWidth;
	INT32		rowLabelWidth;
	INT32		checkWidth;
	INT32		x;
	INT32		y;
	INT32		nIndex;

	GetClientRect(&st_Client);

	// 위 막대: 시나리오 선택 + 조건
	st_TopBar.SetRect(0, 0, st_Client.right, buttonHeight + (2 * f_Ui_Scale(10, dpi)));
	y			= f_Ui_Scale(10, dpi);
	labelWidth	= f_Ui_TextWidth(GetDlgItem(IDC_SCN_LABEL)) + tight;
	f_Ui_Move(this, IDC_SCN_LABEL, margin, y, labelWidth, buttonHeight);
	st_ScnCombo.GetWindowRect(&st_Combo);
	f_Ui_Move(this, IDC_SCN_COMBO, margin + labelWidth, y + ((buttonHeight - st_Combo.Height()) / 2), f_Ui_Scale(UI_SCN_COMBO_WIDTH, dpi),
		st_Combo.Height() + f_Ui_Scale(UI_DROP_HEIGHT, dpi));
	x = margin + labelWidth + f_Ui_Scale(UI_SCN_COMBO_WIDTH, dpi) + gap;
	f_Ui_Move(this, IDC_SCN_NAME, x, y, st_Client.Width() - margin - x, buttonHeight);

	contentTop		= st_TopBar.bottom + margin;
	contentBottom	= st_Client.bottom - margin;

	// 아래 카드는 필터 카드 내용이 다 들어가는 높이 이상 (모자라면 그림 카드를 줄임)
	trkHeight	= pad + buttonHeight + tight + (3 * (buttonHeight + tight)) + gap + (UI_SUMMARY_LINES * textHeight) + f_Ui_Scale(4, dpi) + pad;
	plotHeight	= static_cast<INT32>(static_cast<FLOAT64>(contentBottom - contentTop - gap) * UI_PLOT_RATIO);

	if ((contentBottom - contentTop - gap - plotHeight) < trkHeight)
	{
		plotHeight = contentBottom - contentTop - gap - trkHeight;
	}

	// 그림 카드: 머리 줄 (스캔, 슬라이더, 표시, 보기) 아래에 보기에 맞춘 그림
	st_CardPlot.SetRect(margin, contentTop, st_Client.right - margin, contentTop + plotHeight);
	x = st_CardPlot.right - pad - viewModeWidth;
	y = st_CardPlot.top + pad;
	st_ViewMode.GetWindowRect(&st_Combo);
	f_Ui_Move(this, IDC_VIEW_MODE, x, y + ((buttonHeight - st_Combo.Height()) / 2), viewModeWidth, st_Combo.Height() + f_Ui_Scale(UI_DROP_HEIGHT, dpi));

	// 표시 체크 상자는 보기 선택 왼쪽에 오른쪽부터 (트랙, 플롯, 참값)
	for (nIndex = UI_SHOW_NUM - 1; nIndex >= 0; nIndex--)
	{
		checkWidth	= f_Ui_TextWidth(GetDlgItem(s_ShowId[nIndex])) + f_Ui_Scale(UI_CHECK_BOX_PAD, dpi);
		x			= x - gap - checkWidth;
		f_Ui_Move(this, s_ShowId[nIndex], x, y, checkWidth, buttonHeight);
	}

	f_Ui_Move(this, IDC_SCAN_TEXT, st_CardPlot.left + pad, y, scanTextWidth, buttonHeight);
	f_Ui_Move(this, IDC_SCAN_SLIDER, st_CardPlot.left + pad + scanTextWidth + tight, y, x - (2 * gap) - (st_CardPlot.left + pad + scanTextWidth + tight),
		buttonHeight);
	f_LayoutViews(st_CardPlot.left + pad, y + buttonHeight + tight, st_CardPlot.right - pad, st_CardPlot.bottom - pad);

	// 아래 카드 너비: 필터 카드는 비율대로 하되 입력 줄, 요약 줄이 잘리지 않는 범위, 나머지는 트랙 표 카드
	trkWidth = static_cast<INT32>(static_cast<FLOAT64>(st_Client.Width()) * UI_TRK_RATIO);
	trkWidth = (trkWidth < f_Ui_Scale(UI_TRK_MIN, dpi)) ? f_Ui_Scale(UI_TRK_MIN, dpi) : trkWidth;
	trkWidth = (trkWidth > f_Ui_Scale(UI_TRK_MAX, dpi)) ? f_Ui_Scale(UI_TRK_MAX, dpi) : trkWidth;

	st_CardTrk.SetRect(margin, st_CardPlot.bottom + gap, margin + trkWidth, contentBottom);
	st_CardList.SetRect(st_CardTrk.right + gap, st_CardPlot.bottom + gap, st_Client.right - margin, contentBottom);

	// 필터 카드 1 줄: 제목 + 스코어 확인 단추
	x = st_CardTrk.right - pad - buttonWidth;
	y = st_CardTrk.top + pad;
	f_Ui_Move(this, IDC_TRK_SCORE, x, y, buttonWidth, buttonHeight);
	f_Ui_Move(this, IDC_TRK_TITLE, st_CardTrk.left + pad, y, x - tight - (st_CardTrk.left + pad), buttonHeight);

	// 2 줄: R 표준편차 (거리, 방위각, 고각), 3 줄: Singer q, τ. 줄 이름 열 너비는 두 줄 가운데 긴 쪽
	rowLabelWidth	= std::max(f_Ui_TextWidth(GetDlgItem(IDC_PRM_R_LABEL)), f_Ui_TextWidth(GetDlgItem(IDC_PRM_SINGER_LABEL))) + gap;
	y				= y + buttonHeight + tight;
	f_Ui_Move(this, IDC_PRM_R_LABEL, st_CardTrk.left + pad, y, rowLabelWidth, buttonHeight);
	f_Ui_Move(this, IDC_PRM_SINGER_LABEL, st_CardTrk.left + pad, y + buttonHeight + tight, rowLabelWidth, buttonHeight);
	x = st_CardTrk.left + pad + rowLabelWidth;

	for (nIndex = 0; nIndex < UI_PRM_NUM; nIndex++)
	{
		// 4 번째 칸 (q) 부터 3 줄
		if (nIndex == UI_PRM_SINGER_Q)
		{
			x = st_CardTrk.left + pad + rowLabelWidth;
			y = y + buttonHeight + tight;
		}

		labelWidth = f_Ui_TextWidth(GetDlgItem(s_PrmLabelId[nIndex])) + tight;
		f_Ui_Move(this, s_PrmLabelId[nIndex], x, y, labelWidth, buttonHeight);
		f_Ui_Move(this, s_PrmEditId[nIndex], x + labelWidth, y + f_Ui_Scale(2, dpi), editWidth, buttonHeight - f_Ui_Scale(4, dpi));
		x = x + labelWidth + editWidth + gap;
	}

	// 4 줄: 과제 값, 조정 값, 송신 간격, 적용 · 다시 실행
	x = st_CardTrk.left + pad;
	y = y + buttonHeight + tight;
	f_Ui_Move(this, IDC_PRM_GIVEN, x, y, f_Ui_Scale(UI_PRESET_WIDTH, dpi), buttonHeight);
	x = x + f_Ui_Scale(UI_PRESET_WIDTH, dpi) + tight;
	f_Ui_Move(this, IDC_PRM_TUNED, x, y, f_Ui_Scale(UI_PRESET_WIDTH, dpi), buttonHeight);
	x = x + f_Ui_Scale(UI_PRESET_WIDTH, dpi) + gap;
	labelWidth = f_Ui_TextWidth(GetDlgItem(IDC_TRK_GAP_LABEL)) + tight;
	f_Ui_Move(this, IDC_TRK_GAP_LABEL, x, y, labelWidth, buttonHeight);
	x = x + labelWidth;
	st_GapCombo.GetWindowRect(&st_Combo);
	f_Ui_Move(this, IDC_TRK_GAP, x, y + ((buttonHeight - st_Combo.Height()) / 2), f_Ui_Scale(UI_TRK_GAP_WIDTH, dpi),
		st_Combo.Height() + f_Ui_Scale(UI_DROP_HEIGHT, dpi));
	f_Ui_Move(this, IDC_TRK_RESEND, st_CardTrk.right - pad - f_Ui_Scale(UI_APPLY_WIDTH, dpi), y, f_Ui_Scale(UI_APPLY_WIDTH, dpi), buttonHeight);

	// 요약 (항목 열 + 값 열)
	y = y + buttonHeight + gap;
	labelWidth = f_Ui_TextWidth(GetDlgItem(IDC_TRK_SUMMARY_LABEL)) + (2 * gap);
	f_Ui_Move(this, IDC_TRK_SUMMARY_LABEL, st_CardTrk.left + pad, y, labelWidth, st_CardTrk.bottom - pad - y);
	f_Ui_Move(this, IDC_TRK_SUMMARY, st_CardTrk.left + pad + labelWidth, y, st_CardTrk.Width() - (2 * pad) - labelWidth, st_CardTrk.bottom - pad - y);

	// 트랙 표 카드: 제목 (지금 스캔의 트랙 수) + CSV 저장 단추, 표
	x = st_CardList.right - pad - buttonWidth;
	y = st_CardList.top + pad;
	f_Ui_Move(this, IDC_SAVE_CSV, x, y, buttonWidth, buttonHeight);
	f_Ui_Move(this, IDC_TRK_LIST_TITLE, st_CardList.left + pad, y, x - tight - (st_CardList.left + pad), buttonHeight);
	y = y + buttonHeight + tight;
	f_Ui_Move(this, IDC_TRK_LIST, st_CardList.left + pad, y, st_CardList.Width() - (2 * pad), st_CardList.bottom - pad - y);

	Invalidate(TRUE);
}

// 그림 자리: 보기에 맞춰 3D, 2D 두 장, 평면, 추적 성능을 놓고 나머지는 숨김
VOID CTargetSimUIDlg::f_LayoutViews(INT32 left, INT32 top, INT32 right, INT32 bottom)
{
	const INT32	gap = f_Ui_Scale(UI_GAP, dpi);
	const INT32	width = right - left;
	const INT32	height = bottom - top;
	const INT32	width3d = static_cast<INT32>(static_cast<FLOAT64>(width - gap) * UI_3D_RATIO);
	const INT32	halfWidth = (width - gap) / 2;
	const INT32	halfHeight = (height - gap) / 2;

	switch (nViewMode)
	{
	case UI_VIEW_3D:
		f_Ui_Move(this, IDC_VIEW3D, left, top, width, height);
		break;

	case UI_VIEW_2D:
		f_Ui_Move(this, IDC_PROFILE_LAT, left, top, halfWidth, height);
		f_Ui_Move(this, IDC_PROFILE_LON, left + halfWidth + gap, top, width - halfWidth - gap, height);
		break;

	case UI_VIEW_MAP:
		f_Ui_Move(this, IDC_PLOT, left, top, width, height);
		break;

	case UI_VIEW_PERF:
		f_Ui_Move(this, IDC_FILTER_PLOT, left, top, width, height);
		break;

	default:
		// 3D + 2D: 왼쪽 3D, 오른쪽 위 위도-고도, 오른쪽 아래 경도-고도
		f_Ui_Move(this, IDC_VIEW3D, left, top, width3d, height);
		f_Ui_Move(this, IDC_PROFILE_LAT, left + width3d + gap, top, width - width3d - gap, halfHeight);
		f_Ui_Move(this, IDC_PROFILE_LON, left + width3d + gap, top + halfHeight + gap, width - width3d - gap, height - halfHeight - gap);
		break;
	}

	st_View3D.ShowWindow(((nViewMode == UI_VIEW_ALL) || (nViewMode == UI_VIEW_3D)) ? SW_SHOWNA : SW_HIDE);
	st_ProfileLat.ShowWindow(((nViewMode == UI_VIEW_ALL) || (nViewMode == UI_VIEW_2D)) ? SW_SHOWNA : SW_HIDE);
	st_ProfileLon.ShowWindow(((nViewMode == UI_VIEW_ALL) || (nViewMode == UI_VIEW_2D)) ? SW_SHOWNA : SW_HIDE);
	st_Plot.ShowWindow((nViewMode == UI_VIEW_MAP) ? SW_SHOWNA : SW_HIDE);
	st_FilterPlot.ShowWindow((nViewMode == UI_VIEW_PERF) ? SW_SHOWNA : SW_HIDE);

	// 범례는 3D 에 (2D 만 보일 때는 위도-고도에)
	st_View3D.f_SetLegend(1);
	st_ProfileLat.f_SetLegend((nViewMode == UI_VIEW_2D) ? 1 : 0);
	st_ProfileLon.f_SetLegend(0);
}

// 흰 둥근 사각형 카드 그리기.
VOID CTargetSimUIDlg::f_DrawCard(CDC *st_Dc, const CRect &st_Card) const
{
	const INT32	radius = f_Ui_Scale(UI_CARD_RADIUS, dpi);
	CPen		st_Pen(PS_SOLID, 1, UI_COLOR_BORDER);
	CBrush		st_Brush(UI_COLOR_CARD);
	CPen		*st_OldPen = st_Dc->SelectObject(&st_Pen);
	CBrush		*st_OldBrush = st_Dc->SelectObject(&st_Brush);

	(VOID)st_Dc->RoundRect(&st_Card, CPoint(radius, radius));
	(VOID)st_Dc->SelectObject(st_OldBrush);
	(VOID)st_Dc->SelectObject(st_OldPen);
}

// 페이지 바탕, 카드, 표와 그림 테두리 그리기.
BOOL CTargetSimUIDlg::OnEraseBkgnd(CDC *st_Dc)
{
	static const INT32	s_FramedCtrl[UI_FRAMED_NUM] = { IDC_TRK_LIST, IDC_PLOT, IDC_VIEW3D, IDC_PROFILE_LAT, IDC_PROFILE_LON, IDC_FILTER_PLOT };
	CBrush				st_Border(UI_COLOR_BORDER);
	CRect				st_Client;
	CRect				st_Frame;
	INT32				nIndex;

	GetClientRect(&st_Client);
	st_Dc->FillSolidRect(&st_Client, UI_COLOR_PAGE);
	st_Dc->FillSolidRect(&st_TopBar, UI_COLOR_CARD);
	st_Dc->FillSolidRect(st_TopBar.left, st_TopBar.bottom - 1, st_TopBar.Width(), 1, UI_COLOR_BORDER);

	f_DrawCard(st_Dc, st_CardPlot);
	f_DrawCard(st_Dc, st_CardTrk);
	f_DrawCard(st_Dc, st_CardList);

	// 숨긴 그림 (보기 선택) 은 테두리도 그리지 않음
	for (nIndex = 0; nIndex < UI_FRAMED_NUM; nIndex++)
	{
		if (GetDlgItem(s_FramedCtrl[nIndex])->IsWindowVisible() != FALSE)
		{
			GetDlgItem(s_FramedCtrl[nIndex])->GetWindowRect(&st_Frame);
			ScreenToClient(&st_Frame);
			st_Frame.InflateRect(1, 1);
			st_Dc->FrameRect(&st_Frame, &st_Border);
		}
	}

	return TRUE;
}

// 글자 배경 = 카드 색. 시나리오 조건, 요약 항목 이름은 옅은 색.
HBRUSH CTargetSimUIDlg::OnCtlColor(CDC *st_Dc, CWnd *st_Wnd, UINT32 ctlColor)
{
	HBRUSH	brush = CDialogEx::OnCtlColor(st_Dc, st_Wnd, ctlColor);
	INT32	ctrlId;

	if (ctlColor == CTLCOLOR_DLG)
	{
		brush = static_cast<HBRUSH>(st_PageBrush.GetSafeHandle());
	}
	else if ((ctlColor == CTLCOLOR_STATIC) || (ctlColor == CTLCOLOR_BTN))
	{
		ctrlId = st_Wnd->GetDlgCtrlID();

		(VOID)st_Dc->SetTextColor(((ctrlId == IDC_SCN_NAME) || (ctrlId == IDC_TRK_SUMMARY_LABEL)) ? UI_COLOR_TEXT_SUB : UI_COLOR_TEXT);
		(VOID)st_Dc->SetBkColor(UI_COLOR_CARD);
		brush = static_cast<HBRUSH>(st_CardBrush.GetSafeHandle());
	}
	else
	{
		// 기본 색 (입력 칸 등)
	}

	return brush;
}

// 글자가 바뀐 경우에만 바꿈. 타이머로 자주 부르는 요약, 제목이 깜박이지 않게 함.
VOID CTargetSimUIDlg::f_SetText(INT32 ctrlId, const CString &st_Text)
{
	CString st_Old;

	(VOID)GetDlgItemText(ctrlId, st_Old);

	if (st_Old != st_Text)
	{
		SetDlgItemText(ctrlId, st_Text);
	}
}

// Enter: 파라미터 입력 칸에서 누르면 적용 (다른 곳에서는 창 유지).
VOID CTargetSimUIDlg::OnOK(VOID)
{
	const CWnd	*st_Focus = GetFocus();
	INT32		nIndex;

	for (nIndex = 0; (st_Focus != nullptr) && (nIndex < UI_PRM_NUM); nIndex++)
	{
		if (st_Focus->GetDlgCtrlID() == s_PrmEditId[nIndex])
		{
			f_OnApplyClicked();
			break;
		}
	}
}

// Esc: 창 유지. 닫기는 닫기 단추 (OnClose).
VOID CTargetSimUIDlg::OnCancel(VOID)
{
	// 대화상자 기본 동작 (창 닫기) 을 막음
}

// 닫기 단추. 플롯 메시지 송수신 쓰레드를 멈추고 창 닫기.
VOID CTargetSimUIDlg::OnClose(VOID)
{
	(VOID)KillTimer(UI_TIMER_TRACK);
	st_TrackResult.f_Stop();
	EndDialog(IDCANCEL);
}

// 창이 없어질 때 송수신 쓰레드가 남지 않게 한 번 더 멈춤 (이미 멈췄으면 할 일 없음).
VOID CTargetSimUIDlg::OnDestroy(VOID)
{
	(VOID)KillTimer(UI_TIMER_TRACK);
	st_TrackResult.f_Stop();

	CDialogEx::OnDestroy();
}

// 실행

// 고른 시나리오로 궤적과 측정 모의를 한 번 계산해 그림에 연결한 뒤 플롯 메시지 송수신을 시작.
VOID CTargetSimUIDlg::f_RunScenario(VOID)
{
	const STRUCT_Coord_Sph	st_Noise = f_Pcs_CalcSigma(SCN_SNR);
	ST_SimConfig			st_Config;
	CString					st_Text;

	f_Scn_BuildConfig(&st_Config, nScenario);
	st_Result.f_Run(&st_Config);
	st_PlotResult.f_Run(&st_Result, SCN_SNR);

	st_Plot.f_SetData(&st_Result, &st_PlotResult, &st_TrackResult);
	st_View3D.f_SetData(&st_Result, &st_PlotResult, &st_TrackResult);
	st_ProfileLat.f_SetData(&st_Result, &st_PlotResult, &st_TrackResult);
	st_ProfileLon.f_SetData(&st_Result, &st_PlotResult, &st_TrackResult);
	st_FilterPlot.f_SetData(&st_Result, &st_TrackResult);

	// 위 막대: 시나리오 조건 (궤적, 측정 조건은 고정)
	st_Text.Format(_T("%s  ·  시간 %g s, 간격 %g s  ·  SNR %g (플롯 잡음 σ %.2f m, %.2f°, %.2f°), 클러터 없음  ·  탐지 플롯은 트랙에 무조건 연관 (가정)"),
		f_Scn_GetNote(nScenario), SCN_DURATION_TIME, SCN_STEP_TIME, SCN_SNR, st_Noise.r, f_Rad_To_Deg(st_Noise.az), f_Rad_To_Deg(st_Noise.el));
	SetDlgItemText(IDC_SCN_NAME, st_Text);

	f_RunTrack();
}

// 입력 칸의 파라미터로 플롯 메시지 송신 시작 (지난 송수신은 멈추고 결과를 비움). 처리된 스캔은 타이머로 가져옴
// 입력 칸 값이 잘못되면 알리고 시작하지 않음 (지난 결과 유지)
VOID CTargetSimUIDlg::f_RunTrack(VOID)
{
	ST_TrkParam st_Param;

	if (f_ReadParam(&st_Param) != 0)
	{
		(VOID)KillTimer(UI_TIMER_TRACK);

		if (st_TrackResult.f_Start(&st_Result, &st_PlotResult, sendGapMs, &st_Param) != 0)
		{
			(VOID)SetTimer(UI_TIMER_TRACK, UI_TIMER_TRACK_MS, nullptr);
		}

		st_ScanSlider.SetRange(0, (st_TrackResult.nScanTotal > 0) ? (st_TrackResult.nScanTotal - 1) : 0, TRUE);
		st_ScanSlider.SetLineSize(1);										// 화살표 키: 한 스캔
		st_ScanSlider.SetPageSize(10);										// PgUp / PgDn: 10 스캔 (1 s)
		isFollowScan = 1;
		f_ShowScan(0);
	}
}

// 파라미터 -> 입력 칸
VOID CTargetSimUIDlg::f_SetParamText(const ST_TrkParam *st_Param)
{
	const FLOAT64	value[UI_PRM_NUM] = { st_Param->sigmaRange, st_Param->sigmaAzimuth, st_Param->sigmaElevation, st_Param->singerQ, st_Param->singerTau };
	CString			st_Text;
	INT32			nIndex;

	for (nIndex = 0; nIndex < UI_PRM_NUM; nIndex++)
	{
		st_Text.Format(_T("%g"), value[nIndex]);
		SetDlgItemText(s_PrmEditId[nIndex], st_Text);
	}
}

// 입력 칸 -> 파라미터. 숫자가 아니거나 범위 밖이면 그 칸으로 옮겨 알리고 0 반환 (σ, τ > 0, q >= 0)
INT32 CTargetSimUIDlg::f_ReadParam(ST_TrkParam *st_Param)
{
	FLOAT64		value[UI_PRM_NUM] = {};
	CString		st_Text;
	CString		st_Message;
	TCHAR		*pt_End = nullptr;
	INT32		isOk = 1;
	INT32		isValid;
	INT32		nIndex;

	for (nIndex = 0; (nIndex < UI_PRM_NUM) && (isOk != 0); nIndex++)
	{
		(VOID)GetDlgItemText(s_PrmEditId[nIndex], st_Text);
		(VOID)st_Text.Trim();
		value[nIndex]	= _tcstod(st_Text.GetString(), &pt_End);
		isValid			= ((st_Text.IsEmpty() == FALSE) && (*pt_End == _T('\0')) && (std::isfinite(value[nIndex]) != false)) ? 1 : 0;
		isValid			= ((isValid != 0) && (value[nIndex] <= s_PrmMax[nIndex])
							&& ((value[nIndex] > 0.0) || ((s_PrmZeroOk[nIndex] != 0) && (value[nIndex] == 0.0)))) ? 1 : 0;

		if (isValid == 0)
		{
			isOk = 0;
			st_Message.Format(_T("%s 값 \"%s\" 을(를) 쓸 수 없습니다.\r\n%s %g 이하의 숫자를 넣으십시오."), s_PrmName[nIndex], st_Text.GetString(),
				(s_PrmZeroOk[nIndex] != 0) ? _T("0 이상") : _T("0 보다 크고"), s_PrmMax[nIndex]);
			(VOID)AfxMessageBox(st_Message, MB_ICONWARNING);
			GotoDlgCtrl(GetDlgItem(s_PrmEditId[nIndex]));
		}
	}

	if (isOk != 0)
	{
		st_Param->sigmaRange		= value[UI_PRM_SIGMA_RANGE];
		st_Param->sigmaAzimuth		= value[UI_PRM_SIGMA_AZIMUTH];
		st_Param->sigmaElevation	= value[UI_PRM_SIGMA_ELEVATION];
		st_Param->singerQ			= value[UI_PRM_SINGER_Q];
		st_Param->singerTau			= value[UI_PRM_SINGER_TAU];
	}

	return isOk;
}

// 트랙 표 열 (이름은 과제 명세 그대로). σ위치는 P 위치 대각 세 축의 RSS, 오차는 참값 대비
VOID CTargetSimUIDlg::f_SetupTrackColumns(VOID)
{
	static const LPCTSTR	s_Name[TRK_COL_NUM] =
	{
		_T("트랙 ID"), _T("TrackState"), _T("TrackScore"), _T("MemoryScore"), _T("Assigned_Flag"), _T("개시 스캔"), _T("갱신 플롯"), _T("위도 [°]"), _T("경도 [°]"),
		_T("고도 [m]"), _T("속력 [m/s]"), _T("가속도 [m/s²]"), _T("σ위치 [m]"), _T("NIS"), _T("게이트 밖"), _T("발산 판정"), _T("오차 [m]")
	};
	static const INT32		s_MinWidth[TRK_COL_NUM] = { 64, 86, 76, 86, 90, 66, 80, 92, 92, 70, 76, 90, 76, 60, 70, 70, 70 };
	INT32					nColumn;
	INT32					width;

	for (nColumn = 0; nColumn < TRK_COL_NUM; nColumn++)
	{
		width = st_TrackList.GetStringWidth(s_Name[nColumn]) + f_Ui_Scale(16, dpi);
		width = (width > f_Ui_Scale(s_MinWidth[nColumn], dpi)) ? width : f_Ui_Scale(s_MinWidth[nColumn], dpi);
		(VOID)st_TrackList.InsertColumn(nColumn, s_Name[nColumn], (nColumn == TRK_COL_STATE) ? LVCFMT_LEFT : LVCFMT_RIGHT, width);
	}
}

// 결과 보이기

// 보일 스캔 바꾸기: 슬라이더, 스캔 글자, 그림, 트랙 표, 요약의 지금 스캔.
VOID CTargetSimUIDlg::f_ShowScan(INT32 nScan)
{
	CString st_Text;

	nCurScan = nScan;
	st_ScanSlider.SetPos(nScan);

	// 스캔 시각은 궤적 계산 표본의 시각 (받지 않은 스캔도 보임)
	if ((nScan >= 0) && (nScan < st_Result.f_GetSampleNum()))
	{
		st_Text.Format(_T("스캔 %d (0 ~ %d)   %.1f s"), nScan, st_TrackResult.nScanTotal - 1, st_Result.f_GetSample(nScan)->simTime);
	}
	else
	{
		st_Text.Format(_T("스캔 %d (0 ~ %d)"), nScan, st_TrackResult.nScanTotal - 1);
	}

	f_SetText(IDC_SCAN_TEXT, st_Text);
	st_Plot.f_SetScan(nScan);
	st_View3D.f_SetScan(nScan);
	st_ProfileLat.f_SetScan(nScan);
	st_ProfileLon.f_SetScan(nScan);
	st_FilterPlot.f_SetScan(nScan);
	f_UpdateTrackList();
	f_UpdateTrackSummary();
}

// 트랙 표: 지금 스캔의 트랙 수만큼 행, 제목에 상태별 수
VOID CTargetSimUIDlg::f_UpdateTrackList(VOID)
{
	const INT32	nIndex = st_TrackResult.f_FindScan(nCurScan);
	const INT32	nTrackNum = (nIndex >= 0) ? st_TrackResult.f_GetTrackNum(nIndex) : 0;
	INT32		nStateNum[TRK_STATE_DELETED + 1] = {};
	CString		st_Title;
	INT32		nState;
	INT32		nTrack;

	for (nTrack = 0; nTrack < nTrackNum; nTrack++)
	{
		nState = st_TrackResult.f_GetTrack(nIndex, nTrack)->st_View.TrackState;

		if ((nState >= TRK_STATE_TENTATIVE) && (nState <= TRK_STATE_DELETED))
		{
			nStateNum[nState]++;
		}
	}

	if (nIndex >= 0)
	{
		st_Title.Format(_T("트랙 표  ·  스캔 %d, %d개 (임시 %d, 확정 %d, 메모리 %d, 삭제 %d)"), nCurScan, nTrackNum, nStateNum[TRK_STATE_TENTATIVE],
			nStateNum[TRK_STATE_CONFIRMED], nStateNum[TRK_STATE_MEMORY], nStateNum[TRK_STATE_DELETED]);
	}
	else
	{
		st_Title.Format(_T("트랙 표  ·  스캔 %d 받지 않음"), nCurScan);
	}

	f_SetText(IDC_TRK_LIST_TITLE, st_Title);
	(VOID)st_TrackList.SetItemCountEx(nTrackNum, LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
	st_TrackList.Invalidate(FALSE);
}

// 필터 카드 요약: 송수신, 트랙 누적, 지금 스캔 (갱신, NIS, 참값 오차), 누적 오차 · NIS, 적용 값
VOID CTargetSimUIDlg::f_UpdateTrackSummary(VOID)
{
	const ST_LnkStatus	*st_Status = &st_TrackResult.st_Status;
	const ST_TrkParam	*st_Param = &st_TrackResult.st_Param;
	const INT32			nIndex = st_TrackResult.f_FindScan(nCurScan);
	CString				st_Line[UI_SUMMARY_LINES];
	CString				st_Text;
	ST_TrkStats			st_Stats;
	INT32				nLine;

	// 누적은 보고 있는 스캔까지 (슬라이더를 앞으로 옮기면 그 스캔까지만 셈)
	st_TrackResult.f_GetStats(nCurScan, &st_Stats);

	// 송수신. 수신 쪽이 포트를 잡기 전 (다시 보낸 직후) 은 준비 중
	if (st_Status->errorCode != 0)
	{
		st_Line[0].Format(_T("UDP %hs:%u   소켓 오류 %d"), LNK_IP, st_Status->port, st_Status->errorCode);
	}
	else if (st_Status->port == 0U)
	{
		st_Line[0].Format(_T("UDP %hs (준비 중)"), LNK_IP);
	}
	else
	{
		st_Line[0].Format(_T("UDP %hs:%u (%s)   받은 데이터 %lld B"), LNK_IP, st_Status->port, (st_Status->isRunning != 0) ? _T("보내는 중") : _T("끝"),
			st_Status->recvBytes);
	}

	st_Line[1].Format(_T("보냄 %d, 받음 %d, 손실 %d, 처리 %d / %d 스캔"), st_Status->nSentNum, st_Status->nRecvNum, st_Status->nLostNum,
		st_TrackResult.f_GetScanNum(), st_TrackResult.nScanTotal);

	// 형식이 맞지 않아 버린 메시지가 있을 때만
	if (st_Status->nBadNum > 0)
	{
		st_Line[1].AppendFormat(_T(", 잘못된 메시지 %d"), st_Status->nBadNum);
	}

	// 트랙 누적
	st_Line[2].Format(_T("생성 %d, 삭제 %d, 나온 트랙 ID %d 개 (스캔 %d 까지)"), st_Stats.nCreatedNum, st_Stats.nDeletedNum, st_Stats.nTrackIdNum, nCurScan);

	// 지금 스캔: 갱신 기록과 첫 트랙의 참값 오차, NIS
	if (nIndex >= 0)
	{
		const ST_TrkScanLog *st_Log = &st_TrackResult.f_GetScan(nIndex)->st_Log;

		st_Line[3].Format(_T("플롯 %d, 갱신 %d, 게이트 밖 %d, 발산 판정 %d, 트랙 생성 %d"), st_Log->nPlotNum, st_Log->nUpdatedNum, st_Log->nGateOutNum,
			st_Log->nDivergedNum, st_Log->nCreatedNum);

		if (st_TrackResult.f_GetTrackNum(nIndex) > 0)
		{
			const ST_TrackItem *st_Item = st_TrackResult.f_GetTrack(nIndex, 0);

			if (st_Item->st_View.nis >= 0.0)
			{
				st_Line[4].Format(_T("트랙 %u: 참값 대비 오차 %.1f m (σ위치 %.1f m), NIS %.2f, 게이트 밖 연속 %d"), st_Item->st_View.TrackId, st_Item->errNorm,
					f_Ui_Norm(st_Item->st_View.posStd), st_Item->st_View.nis, st_Item->st_View.nGateOutRun);
			}
			else
			{
				st_Line[4].Format(_T("트랙 %u: 참값 대비 오차 %.1f m (σ위치 %.1f m), 이번 스캔 갱신 없음"), st_Item->st_View.TrackId, st_Item->errNorm,
					f_Ui_Norm(st_Item->st_View.posStd));
			}
		}
		else
		{
			st_Line[4] = _T("트랙 없음 (2-Point 개시 전)");
		}
	}
	else
	{
		st_Line[3] = _T("받지 않음 (아직 처리 전이거나 UDP 손실)");
	}

	// 누적: 갱신 TRK_STEADY_UPDATES 번 뒤 (2-Point 개시 직후 큰 오차 제외)
	st_Line[5].Format(_T("위치 오차 RMS %.1f m, 최대 %.1f m (갱신 %d 번 뒤부터)"), st_Stats.rmsError, st_Stats.maxError, TRK_STEADY_UPDATES);
	st_Line[6].Format(_T("NIS 평균 %.2f (일치하면 3), 게이트 밖 %.1f %%, 발산 판정 %d 번"), st_Stats.meanNis, st_Stats.gateOutPct, st_Stats.nDivergedNum);

	// 적용 값 (이번 실행), 판정 기준
	st_Line[7].Format(_T("R σ %g m, %g°, %g°  ·  Singer q %g m/s², τ %g s"), st_Param->sigmaRange, st_Param->sigmaAzimuth, st_Param->sigmaElevation,
		st_Param->singerQ, st_Param->singerTau);
	st_Line[8].Format(_T("게이트 χ²(3) 99 %% = %g, %d 번 이어지면 발산 판정"), TRK_GATE_CHI2, TRK_DIVERGE_NUM);

	for (nLine = 0; nLine < UI_SUMMARY_LINES; nLine++)
	{
		st_Text += st_Line[nLine];
		st_Text += (nLine < (UI_SUMMARY_LINES - 1)) ? _T("\r\n") : _T("");
	}

	f_SetText(IDC_TRK_SUMMARY, st_Text);
}

// 받은 스캔 가운데 마지막 스캔 번호. 아직 없으면 0.
INT32 CTargetSimUIDlg::f_GetLastScan(VOID) const
{
	const INT32 nScanNum = st_TrackResult.f_GetScanNum();

	return (nScanNum > 0) ? static_cast<INT32>(st_TrackResult.f_GetScan(nScanNum - 1)->ScanNum) : 0;
}

// 가상 리스트 요청 칸의 글자: 지금 스캔의 트랙 하나. 보이는 행만 요청됨.
VOID CTargetSimUIDlg::f_OnTrackGetDispInfo(NMHDR *st_Hdr, LRESULT *pt_Result)
{
	NMLVDISPINFO	*st_Info = reinterpret_cast<NMLVDISPINFO *>(st_Hdr);
	const INT32		nIndex = st_TrackResult.f_FindScan(nCurScan);
	CHAR			pt_Cell[RES_CELL_SIZE];

	if ((st_Info->item.mask & LVIF_TEXT) != 0U)
	{
		pt_Cell[0] = '\0';

		if ((nIndex >= 0) && (st_Info->item.iItem >= 0) && (st_Info->item.iItem < st_TrackResult.f_GetTrackNum(nIndex)))
		{
			st_TrackResult.f_FormatCell(nIndex, st_Info->item.iItem, st_Info->item.iSubItem, pt_Cell, RES_CELL_SIZE);
		}

		(VOID)MultiByteToWideChar(CP_UTF8, 0, pt_Cell, -1, st_Info->item.pszText, st_Info->item.cchTextMax);
	}

	*pt_Result = 0;
}

// 처리된 스캔 가져오기. 따라가는 중이면 마지막 스캔을 보임. 송수신이 끝나면 타이머 멈춤
VOID CTargetSimUIDlg::OnTimer(UINT_PTR nEvent)
{
	INT32 isNew;
	INT32 isViewNew;

	if (nEvent == UI_TIMER_TRACK)
	{
		isViewNew	= (st_TrackResult.f_FindScan(nCurScan) < 0) ? 1 : 0;
		isNew		= st_TrackResult.f_Poll();
		isViewNew	= ((isViewNew != 0) && (st_TrackResult.f_FindScan(nCurScan) >= 0)) ? 1 : 0;

		if ((isNew != 0) && (isFollowScan != 0))
		{
			f_ShowScan(f_GetLastScan());
		}
		else if (isViewNew != 0)
		{
			f_ShowScan(nCurScan);
		}
		else
		{
			// 새로 처리된 스캔이 없거나, 보던 스캔과 상관없는 스캔만 옴
		}

		// 송수신 상태 (보내는 중 -> 끝, 손실 수) 는 새 스캔이 없어도 바뀜
		f_UpdateTrackSummary();

		if (st_TrackResult.st_Status.isRunning == 0)
		{
			(VOID)KillTimer(UI_TIMER_TRACK);
		}
	}

	CDialogEx::OnTimer(nEvent);
}

// 스캔 슬라이더. 마지막 스캔에 두면 새 스캔을 따라감
VOID CTargetSimUIDlg::OnHScroll(UINT32 code, UINT32 pos, CScrollBar *st_Bar)
{
	INT32 nScan;

	if ((st_Bar != nullptr) && (st_Bar->GetDlgCtrlID() == IDC_SCAN_SLIDER))
	{
		nScan			= (st_ScanSlider.GetPos() > f_GetLastScan()) ? f_GetLastScan() : st_ScanSlider.GetPos();
		isFollowScan	= (nScan >= f_GetLastScan()) ? 1 : 0;
		f_ShowScan(nScan);
	}

	CDialogEx::OnHScroll(code, pos, st_Bar);
}

// 휠: 커서 아래 그림에 전달 (확대)
BOOL CTargetSimUIDlg::OnMouseWheel(UINT32 flags, INT16 delta, CPoint st_Point)
{
	CRect st_Rect;

	st_Plot.GetWindowRect(&st_Rect);

	if ((st_Plot.IsWindowVisible() != FALSE) && (st_Rect.PtInRect(st_Point) != FALSE))
	{
		st_Plot.f_Zoom(delta, st_Point);
	}

	st_View3D.GetWindowRect(&st_Rect);

	if ((st_View3D.IsWindowVisible() != FALSE) && (st_Rect.PtInRect(st_Point) != FALSE))
	{
		st_View3D.f_Zoom(delta, st_Point);
	}

	st_ProfileLat.GetWindowRect(&st_Rect);

	if ((st_ProfileLat.IsWindowVisible() != FALSE) && (st_Rect.PtInRect(st_Point) != FALSE))
	{
		st_ProfileLat.f_Zoom(delta, st_Point);
	}

	st_ProfileLon.GetWindowRect(&st_Rect);

	if ((st_ProfileLon.IsWindowVisible() != FALSE) && (st_Rect.PtInRect(st_Point) != FALSE))
	{
		st_ProfileLon.f_Zoom(delta, st_Point);
	}

	return CDialogEx::OnMouseWheel(flags, delta, st_Point);
}

// 그림 표시 (참값, 플롯, 트랙) 켜기 / 끄기
VOID CTargetSimUIDlg::f_OnShowClicked(VOID)
{
	INT32 nIndex;

	for (nIndex = 0; nIndex < UI_SHOW_NUM; nIndex++)
	{
		isShow[nIndex] = (IsDlgButtonChecked(s_ShowId[nIndex]) == BST_CHECKED) ? 1 : 0;
	}

	st_Plot.f_SetShow(isShow[UI_SHOW_TRUE], isShow[UI_SHOW_PLOT], isShow[UI_SHOW_TRACK]);
	st_View3D.f_SetShow(isShow[UI_SHOW_TRUE], isShow[UI_SHOW_PLOT], isShow[UI_SHOW_TRACK]);
	st_ProfileLat.f_SetShow(isShow[UI_SHOW_TRUE], isShow[UI_SHOW_PLOT], isShow[UI_SHOW_TRACK]);
	st_ProfileLon.f_SetShow(isShow[UI_SHOW_TRUE], isShow[UI_SHOW_PLOT], isShow[UI_SHOW_TRACK]);
}

// 그림 보기 바꾸기 (추적 성능에서는 표시 체크 상자를 끔)
VOID CTargetSimUIDlg::f_OnViewModeChanged(VOID)
{
	const INT32	nSel = st_ViewMode.GetCurSel();
	INT32		nIndex;

	if ((nSel >= 0) && (nSel < UI_VIEW_NUM))
	{
		nViewMode = nSel;

		for (nIndex = 0; nIndex < UI_SHOW_NUM; nIndex++)
		{
			(VOID)GetDlgItem(s_ShowId[nIndex])->EnableWindow((nViewMode != UI_VIEW_PERF) ? TRUE : FALSE);
		}

		f_Layout();
	}
}

// 시나리오 바꾸면 궤적, 측정 모의부터 다시 (파라미터 입력 칸은 그대로)
VOID CTargetSimUIDlg::f_OnScenarioChanged(VOID)
{
	const INT32 nSel = st_ScnCombo.GetCurSel();

	if ((nSel >= 0) && (nSel < SCN_NUM) && (nSel != nScenario))
	{
		nScenario = nSel;
		f_RunScenario();
	}
}

// 송신 간격 바꾸면 처음부터 다시 보냄 (100 ms = 실시간)
VOID CTargetSimUIDlg::f_OnTrkGapChanged(VOID)
{
	const INT32 nSel = st_GapCombo.GetCurSel();

	if ((nSel >= 0) && (nSel < UI_TRK_GAP_NUM) && (s_GapMs[nSel] != sendGapMs))
	{
		sendGapMs = s_GapMs[nSel];
		f_RunTrack();
	}
}

// 입력 칸의 파라미터로 처음부터 다시 보냄 (궤적, 측정 모의 결과는 그대로).
VOID CTargetSimUIDlg::f_OnApplyClicked(VOID)
{
	f_RunTrack();
}

// 과제 값 (10 m, 0.3°, 0.3°, q 0.5, τ 10) 으로 다시 실행
VOID CTargetSimUIDlg::f_OnGivenClicked(VOID)
{
	ST_TrkParam st_Param;

	f_Trk_DefaultParam(&st_Param);
	f_SetParamText(&st_Param);
	f_RunTrack();
}

// 지금 시나리오의 조정 값으로 다시 실행
VOID CTargetSimUIDlg::f_OnTunedClicked(VOID)
{
	ST_TrkParam st_Param;

	f_Scn_GetTunedParam(&st_Param, nScenario);
	f_SetParamText(&st_Param);
	f_RunTrack();
}

// 트랙 스코어 확인 창 (Assigned_Flag 를 직접 넣어 확정 · 메모리 확인)
VOID CTargetSimUIDlg::f_OnTrkScoreClicked(VOID)
{
	CScoreDlg st_ScoreDlg(this);

	(VOID)st_ScoreDlg.DoModal();
}

// 받은 전체 스캔의 트랙을 CSV 로 저장 (한 줄 = 스캔 하나의 트랙 하나). 기본 파일 이름에 시나리오와 q, 각도 σ
VOID CTargetSimUIDlg::f_OnSaveCsvClicked(VOID)
{
	CString st_Name;

	st_Name.Format(_T("TargetSim_%s_q%g_az%g.csv"), (nScenario == SCN_AIR) ? _T("air") : _T("ship"), st_TrackResult.st_Param.singerQ,
		st_TrackResult.st_Param.sigmaAzimuth);

	CFileDialog st_FileDlg(FALSE, _T("csv"), st_Name, OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY, _T("CSV (*.csv)|*.csv||"), this);

	if ((st_FileDlg.DoModal() == IDOK) && (st_TrackResult.f_WriteCsv(st_FileDlg.GetPathName()) == 0))
	{
		(VOID)AfxMessageBox(_T("CSV 파일을 쓸 수 없습니다. 다른 프로그램이 열고 있지 않은지 확인하십시오."), MB_ICONWARNING);
	}
}
