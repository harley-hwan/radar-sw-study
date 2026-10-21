//
// @file	TargetSimUIDlg.cpp
// @brief	주 화면 구현: 시나리오 실행, 플롯 메시지 송수신 시작, True · Plot · Track 그림 (2D, 3D), 트랙 표, 요약
// @author	hwan
// @date	2026.10.21.
//
#include "pch.h"
#include "framework.h"

#include "ScoreDlg.h"
#include "TargetSimUI.h"
#include "TargetSimUIDlg.h"
#include "UiCommon.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define UI_INIT_WIDTH			1640					// [px @96dpi]
#define UI_INIT_HEIGHT			980
#define UI_MIN_WIDTH			1100					// 그림 카드 머리 줄 (스캔, 슬라이더, 표시 넷, 보기) 과 아래 두 카드가 들어가는 너비
#define UI_MIN_HEIGHT			720						// 트랙 카드 요약이 다 들어가고 그림이 남는 높이
#define UI_MARGIN				12
#define UI_GAP					10
#define UI_CARD_PAD				12
#define UI_CARD_RADIUS			10
#define UI_PLOT_RATIO			0.64					// 그림 카드 높이 비율. 나머지는 트랙 카드, 트랙 표 카드
#define UI_TRK_RATIO			0.36					// 트랙 카드 (왼쪽 아래) 너비 비율
#define UI_TRK_MIN				500						// 트랙 카드 요약 줄이 잘리지 않는 너비
#define UI_TRK_MAX				620
#define UI_BUTTON_WIDTH			110						// 스코어 확인, 다시 보내기, CSV 저장 단추 너비
#define UI_CHECK_BOX_PAD		24						// 체크 상자 네모와 글자 앞 간격
#define UI_DROP_HEIGHT			240						// 콤보 목록 펼친 높이
#define UI_SCAN_TEXT_WIDTH		200						// 스캔 번호 글자 너비 ("스캔 1159 (0 ~ 1159)   115.9 s" 가 들어가게)
#define UI_VIEW_MODE_WIDTH		110						// 그림 보기 선택 너비
#define UI_TRK_GAP_NUM			3						// 송신 간격 고르기 수
#define UI_TRK_GAP_WIDTH		128						// 송신 간격 선택 너비
#define UI_SUMMARY_LINES		10						// 트랙 카드 요약 줄 수
#define UI_FRAMED_NUM			3						// 테두리를 그리는 컨트롤 수
#define UI_TIMER_TRACK			1						// 수신 결과를 가져오는 타이머
#define UI_TIMER_TRACK_MS		50

// 송신 간격 [ms] 과 이름. 100 ms 가 스캔 간격 (0.1 s) 과 같은 실시간.
static const INT32		s_GapMs[UI_TRK_GAP_NUM] = { 0, 10, 100 };
static const LPCTSTR	s_GapName[UI_TRK_GAP_NUM] = { _T("0 ms (바로)"), _T("10 ms (빨리)"), _T("100 ms (실시간)") };
static const LPCTSTR	s_ViewName[UI_VIEW_NUM] = { _T("2D + 3D"), _T("2D"), _T("3D") };

// 그림 표시 체크 상자. UI_SHOW_* 순서
static const INT32		s_ShowId[UI_SHOW_NUM] = { IDC_SHOW_TRUE, IDC_SHOW_PLOT, IDC_SHOW_CLUTTER, IDC_SHOW_TRACK };

// 트랙 카드 요약의 항목 이름. 줄마다 하나, 빈 칸은 윗줄 항목이 이어지는 줄 (f_UpdateTrackSummary 의 줄 순서와 같음)
static const LPCTSTR	s_SummaryLabel[UI_SUMMARY_LINES] = { _T("송수신"), _T(""), _T("누적 트랙"), _T(""), _T("지금 스캔"), _T(""), _T(""), _T("조건"), _T(""), _T("") };

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
	ON_BN_CLICKED(IDC_SHOW_CLUTTER, &CTargetSimUIDlg::f_OnShowClicked)
	ON_BN_CLICKED(IDC_SHOW_TRACK, &CTargetSimUIDlg::f_OnShowClicked)
	ON_CBN_SELCHANGE(IDC_VIEW_MODE, &CTargetSimUIDlg::f_OnViewModeChanged)
	ON_CBN_SELCHANGE(IDC_TRK_GAP, &CTargetSimUIDlg::f_OnTrkGapChanged)
	ON_BN_CLICKED(IDC_TRK_RESEND, &CTargetSimUIDlg::f_OnTrkResendClicked)
	ON_BN_CLICKED(IDC_TRK_SCORE, &CTargetSimUIDlg::f_OnTrkScoreClicked)
	ON_BN_CLICKED(IDC_SAVE_CSV, &CTargetSimUIDlg::f_OnSaveCsvClicked)
	ON_NOTIFY(LVN_GETDISPINFO, IDC_TRK_LIST, &CTargetSimUIDlg::f_OnTrackGetDispInfo)
END_MESSAGE_MAP()

// 주 대화상자. 그림 표시는 처음에 모두 켬.
CTargetSimUIDlg::CTargetSimUIDlg(CWnd *st_Parent)
	: CDialogEx(IDD_TARGETSIMUI_DIALOG, st_Parent)
	, st_TopBar(0, 0, 0, 0)
	, st_CardPlot(0, 0, 0, 0)
	, st_CardTrk(0, 0, 0, 0)
	, st_CardList(0, 0, 0, 0)
	, dpi(UI_BASE_DPI)
	, textHeight(16)
	, isShow{ 1, 1, 1, 1 }
	, sendGapMs(s_GapMs[0])
	, nCurScan(0)
	, isFollowScan(1)
	, nViewMode(UI_VIEW_BOTH)
{
}

// 컨트롤 변수와 대화상자 자원 연결.
VOID CTargetSimUIDlg::DoDataExchange(CDataExchange *st_Dx)
{
	CDialogEx::DoDataExchange(st_Dx);
	DDX_Control(st_Dx, IDC_PLOT, st_Plot);
	DDX_Control(st_Dx, IDC_VIEW3D, st_View3D);
	DDX_Control(st_Dx, IDC_SCAN_SLIDER, st_ScanSlider);
	DDX_Control(st_Dx, IDC_VIEW_MODE, st_ViewMode);
	DDX_Control(st_Dx, IDC_TRK_GAP, st_GapCombo);
	DDX_Control(st_Dx, IDC_TRK_LIST, st_TrackList);
}

// 글꼴, 색, 표, 그림 준비 후 고정 시나리오를 계산하고 플롯 메시지 송수신 (트랙 개시 · 관리) 을 시작.
BOOL CTargetSimUIDlg::OnInitDialog(VOID)
{
	CClientDC	st_Dc(this);
	CFont		*st_OldFont;
	LOGFONT		st_LogFont;
	TEXTMETRIC	st_Metric;
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

	// 위 막대: 고정 시나리오 조건 (편집 없음)
	st_Text.Format(_T("%s  ·  시간 %g s, 간격 %g s  ·  SNR %g, 스캔당 클러터 %d  ·  궤적, 기동, 측정 조건 고정"), SCN_NAME, SCN_DURATION_TIME,
		SCN_STEP_TIME, SCN_SNR, SCN_CLUTTER_NUM);
	SetDlgItemText(IDC_SCN_NAME, st_Text);

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

	// 트랙 카드: 송신 간격, 요약 항목 이름 (배치가 항목 열 너비를 재므로 배치 전에 씀)
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
	st_Plot.f_SetShow(isShow[UI_SHOW_TRUE], isShow[UI_SHOW_PLOT], isShow[UI_SHOW_CLUTTER], isShow[UI_SHOW_TRACK]);
	st_View3D.f_SetDpi(dpi);
	st_View3D.f_SetShow(isShow[UI_SHOW_TRUE], isShow[UI_SHOW_PLOT], isShow[UI_SHOW_CLUTTER], isShow[UI_SHOW_TRACK]);

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

// 위 막대, 그림 카드 (위, 전체 너비), 트랙 카드 (왼쪽 아래), 트랙 표 카드 (오른쪽 아래) 배치. 모든 치수에 화면 배율 적용.
VOID CTargetSimUIDlg::f_Layout(VOID)
{
	const INT32	margin = f_Ui_Scale(UI_MARGIN, dpi);
	const INT32	gap = f_Ui_Scale(UI_GAP, dpi);
	const INT32	pad = f_Ui_Scale(UI_CARD_PAD, dpi);
	const INT32	tight = f_Ui_Scale(6, dpi);
	const INT32	buttonHeight = textHeight + f_Ui_Scale(12, dpi);
	const INT32	buttonWidth = f_Ui_Scale(UI_BUTTON_WIDTH, dpi);
	const INT32	scanTextWidth = f_Ui_Scale(UI_SCAN_TEXT_WIDTH, dpi);
	const INT32	viewModeWidth = f_Ui_Scale(UI_VIEW_MODE_WIDTH, dpi);
	CRect		st_Client;
	CRect		st_Combo;
	INT32		contentTop;
	INT32		contentBottom;
	INT32		plotHeight;
	INT32		trkHeight;
	INT32		trkWidth;
	INT32		viewWidth;
	INT32		labelWidth;
	INT32		checkWidth;
	INT32		x;
	INT32		y;
	INT32		nIndex;

	GetClientRect(&st_Client);

	// 위 막대: 고정 시나리오 조건
	st_TopBar.SetRect(0, 0, st_Client.right, buttonHeight + (2 * f_Ui_Scale(10, dpi)));
	f_Ui_Move(this, IDC_SCN_NAME, margin, f_Ui_Scale(10, dpi), st_Client.Width() - (2 * margin), buttonHeight);

	contentTop		= st_TopBar.bottom + margin;
	contentBottom	= st_Client.bottom - margin;

	// 아래 카드는 트랙 카드 (제목 줄, 송신 간격 줄, 요약) 가 다 들어가는 높이 이상. 모자라면 그림 카드를 줄임
	trkHeight	= pad + buttonHeight + tight + buttonHeight + gap + (UI_SUMMARY_LINES * textHeight) + f_Ui_Scale(4, dpi) + pad;
	plotHeight	= static_cast<INT32>(static_cast<FLOAT64>(contentBottom - contentTop - gap) * UI_PLOT_RATIO);

	if ((contentBottom - contentTop - gap - plotHeight) < trkHeight)
	{
		plotHeight = contentBottom - contentTop - gap - trkHeight;
	}

	// 그림 카드: 머리 줄에 스캔 글자, 스캔 슬라이더, 표시 체크 상자, 보기 선택. 아래는 2D, 3D 그림 (보기에 따라 나란히 또는 하나만)
	st_CardPlot.SetRect(margin, contentTop, st_Client.right - margin, contentTop + plotHeight);
	x = st_CardPlot.right - pad - viewModeWidth;
	y = st_CardPlot.top + pad;
	st_ViewMode.GetWindowRect(&st_Combo);
	f_Ui_Move(this, IDC_VIEW_MODE, x, y + ((buttonHeight - st_Combo.Height()) / 2), viewModeWidth, st_Combo.Height() + f_Ui_Scale(UI_DROP_HEIGHT, dpi));

	// 표시 체크 상자는 보기 선택 왼쪽에 오른쪽부터 (트랙, 클러터, 플롯, 참값)
	for (nIndex = UI_SHOW_NUM - 1; nIndex >= 0; nIndex--)
	{
		checkWidth	= f_Ui_TextWidth(GetDlgItem(s_ShowId[nIndex])) + f_Ui_Scale(UI_CHECK_BOX_PAD, dpi);
		x			= x - gap - checkWidth;
		f_Ui_Move(this, s_ShowId[nIndex], x, y, checkWidth, buttonHeight);
	}

	f_Ui_Move(this, IDC_SCAN_TEXT, st_CardPlot.left + pad, y, scanTextWidth, buttonHeight);
	f_Ui_Move(this, IDC_SCAN_SLIDER, st_CardPlot.left + pad + scanTextWidth + tight, y, x - (2 * gap) - (st_CardPlot.left + pad + scanTextWidth + tight),
		buttonHeight);
	y = y + buttonHeight + tight;
	viewWidth = (nViewMode == UI_VIEW_BOTH) ? ((st_CardPlot.Width() - (2 * pad) - gap) / 2) : (st_CardPlot.Width() - (2 * pad));
	f_Ui_Move(this, IDC_PLOT, st_CardPlot.left + pad, y, viewWidth, st_CardPlot.bottom - pad - y);
	f_Ui_Move(this, IDC_VIEW3D, st_CardPlot.right - pad - viewWidth, y, viewWidth, st_CardPlot.bottom - pad - y);
	st_Plot.ShowWindow((nViewMode != UI_VIEW_3D) ? SW_SHOWNA : SW_HIDE);
	st_View3D.ShowWindow((nViewMode != UI_VIEW_2D) ? SW_SHOWNA : SW_HIDE);
	st_View3D.f_SetLegend((nViewMode == UI_VIEW_3D) ? 1 : 0);

	// 아래 카드 너비: 트랙 카드는 비율대로 하되 요약 줄이 잘리지 않는 범위, 나머지는 트랙 표 카드
	trkWidth = static_cast<INT32>(static_cast<FLOAT64>(st_Client.Width()) * UI_TRK_RATIO);

	if (trkWidth < f_Ui_Scale(UI_TRK_MIN, dpi))
	{
		trkWidth = f_Ui_Scale(UI_TRK_MIN, dpi);
	}
	else if (trkWidth > f_Ui_Scale(UI_TRK_MAX, dpi))
	{
		trkWidth = f_Ui_Scale(UI_TRK_MAX, dpi);
	}
	else
	{
		// 비율대로
	}

	st_CardTrk.SetRect(margin, st_CardPlot.bottom + gap, margin + trkWidth, contentBottom);
	st_CardList.SetRect(st_CardTrk.right + gap, st_CardPlot.bottom + gap, st_Client.right - margin, contentBottom);

	// 트랙 카드: 제목 + 스코어 확인 단추, 송신 간격 + 다시 보내기 단추, 요약 (항목 열 + 값 열)
	x = st_CardTrk.right - pad - buttonWidth;
	y = st_CardTrk.top + pad;
	f_Ui_Move(this, IDC_TRK_SCORE, x, y, buttonWidth, buttonHeight);
	f_Ui_Move(this, IDC_TRK_TITLE, st_CardTrk.left + pad, y, x - tight - (st_CardTrk.left + pad), buttonHeight);
	x = st_CardTrk.left + pad;
	y = y + buttonHeight + tight;
	labelWidth = f_Ui_TextWidth(GetDlgItem(IDC_TRK_GAP_LABEL)) + tight;
	f_Ui_Move(this, IDC_TRK_GAP_LABEL, x, y, labelWidth, buttonHeight);
	x = x + labelWidth;
	st_GapCombo.GetWindowRect(&st_Combo);
	f_Ui_Move(this, IDC_TRK_GAP, x, y + ((buttonHeight - st_Combo.Height()) / 2), f_Ui_Scale(UI_TRK_GAP_WIDTH, dpi),
		st_Combo.Height() + f_Ui_Scale(UI_DROP_HEIGHT, dpi));
	x = x + f_Ui_Scale(UI_TRK_GAP_WIDTH, dpi) + tight;
	f_Ui_Move(this, IDC_TRK_RESEND, x, y, buttonWidth, buttonHeight);
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
	static const INT32	s_FramedCtrl[UI_FRAMED_NUM] = { IDC_TRK_LIST, IDC_PLOT, IDC_VIEW3D };
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
		// 기본 색
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

// Enter: 창 유지 (편집할 입력 칸이 없음).
VOID CTargetSimUIDlg::OnOK(VOID)
{
	// 대화상자 기본 동작 (창 닫기) 을 막음
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

// 고정 시나리오로 궤적과 측정 모의를 한 번 계산해 그림에 연결한 뒤 플롯 메시지 송수신을 시작.
VOID CTargetSimUIDlg::f_RunScenario(VOID)
{
	ST_SimConfig st_Config;

	f_Scn_BuildConfig(&st_Config);
	st_Result.f_Run(&st_Config);
	st_PlotResult.f_Run(&st_Result, SCN_SNR, SCN_CLUTTER_NUM);

	st_Plot.f_SetData(&st_Result, &st_PlotResult, &st_TrackResult);
	st_View3D.f_SetData(&st_Result, &st_PlotResult, &st_TrackResult);
	f_RunTrack();
}

// 측정 모의 결과를 플롯 메시지로 보내기 시작 (Core 의 송신 쓰레드 -> UDP -> 수신 쓰레드). 지난 송수신은 멈추고 결과를 비움.
// 받은 쪽이 처리한 스캔은 타이머로 가져와 그림, 트랙 표, 요약에 보임. 처음엔 스캔 0 부터 새 스캔을 따라감.
VOID CTargetSimUIDlg::f_RunTrack(VOID)
{
	(VOID)KillTimer(UI_TIMER_TRACK);

	if (st_TrackResult.f_Start(&st_Result, &st_PlotResult, sendGapMs) != 0)
	{
		(VOID)SetTimer(UI_TIMER_TRACK, UI_TIMER_TRACK_MS, nullptr);
	}

	st_ScanSlider.SetRange(0, (st_TrackResult.nScanTotal > 0) ? (st_TrackResult.nScanTotal - 1) : 0, TRUE);
	st_ScanSlider.SetLineSize(1);											// 화살표 키: 한 스캔
	st_ScanSlider.SetPageSize(10);											// PgUp / PgDn: 10 스캔 (1 s)
	isFollowScan = 1;
	f_ShowScan(0);
}

// 트랙 표 열. 이름은 과제 명세 그대로 (TrackState, TrackScore, MemoryScore, Assigned_Flag). 위치, 속도는 받은 스캔의 VC-NED 상태변수 (mat_EstX).
// σ 는 오차 공분산 (mat_EstP) 의 위치, 속도 대각을 세 축 RSS 로 묶은 값.
VOID CTargetSimUIDlg::f_SetupTrackColumns(VOID)
{
	static const LPCTSTR	s_Name[TRK_COL_NUM] =
	{
		_T("트랙 ID"), _T("TrackState"), _T("TrackScore"), _T("MemoryScore"), _T("Assigned_Flag"), _T("개시 스캔"), _T("개시 플롯 (앞)"), _T("개시 플롯 (이번)"),
		_T("참값"), _T("북 [m]"), _T("동 [m]"), _T("아래 [m]"), _T("V북 [m/s]"), _T("V동 [m/s]"), _T("V아래 [m/s]"), _T("속력 [m/s]"), _T("σ위치 [m]"), _T("σ속도 [m/s]")
	};
	static const INT32		s_MinWidth[TRK_COL_NUM] = { 64, 86, 76, 86, 90, 66, 96, 104, 64, 76, 76, 70, 76, 76, 80, 76, 80, 84 };
	INT32					nColumn;
	INT32					width;

	for (nColumn = 0; nColumn < TRK_COL_NUM; nColumn++)
	{
		width = st_TrackList.GetStringWidth(s_Name[nColumn]) + f_Ui_Scale(16, dpi);
		width = (width > f_Ui_Scale(s_MinWidth[nColumn], dpi)) ? width : f_Ui_Scale(s_MinWidth[nColumn], dpi);
		(VOID)st_TrackList.InsertColumn(nColumn, s_Name[nColumn], ((nColumn == TRK_COL_STATE) || (nColumn == TRK_COL_SOURCE)) ? LVCFMT_LEFT : LVCFMT_RIGHT, width);
	}
}

// 결과 보이기

// 보일 스캔 바꾸기: 슬라이더, 스캔 글자, 2D, 3D 그림, 트랙 표, 요약의 지금 스캔.
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
	f_UpdateTrackList();
	f_UpdateTrackSummary();
}

// 트랙 표: 지금 스캔의 트랙 수만큼 행을 두고, 제목에 상태별 수. 칸 글자는 f_OnTrackGetDispInfo 가 채움.
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

//
// @brief	트랙 카드 요약 (s_SummaryLabel 의 줄 순서): 송수신 (UDP 주소, 보냄, 받음, 손실, 처리), 누적 트랙 (개시 플롯의 참값별),
//			지금 스캔의 처리 기록 (받은 플롯, 2 Point 개시 후보 · 쌍 · 게이트, 상태 전이), 조건 (V_max, 문턱치, R 표준편차)
// @return	없음
// @author	hwan
//
VOID CTargetSimUIDlg::f_UpdateTrackSummary(VOID)
{
	const ST_LnkStatus	*st_Status = &st_TrackResult.st_Status;
	const INT32			nIndex = st_TrackResult.f_FindScan(nCurScan);
	CString				st_Line[UI_SUMMARY_LINES];
	CString				st_Text;
	INT32				pt_Count[TGT_MAX_TARGET_NUM + 2];
	INT32				nCreatedNum = 0;
	INT32				nDeletedNum = 0;
	INT32				nTargetSum = 0;
	INT32				nTargetPlot = 0;
	INT32				nClutterPlot = 0;
	INT32				nObject;
	INT32				nLine;

	// 누적은 보고 있는 스캔까지 (슬라이더를 앞으로 옮기면 그 스캔까지만 셈)
	st_TrackResult.f_CountTo(nCurScan, &nCreatedNum, &nDeletedNum, pt_Count);

	for (nObject = 1; nObject <= TGT_MAX_TARGET_NUM; nObject++)
	{
		nTargetSum = nTargetSum + pt_Count[nObject];
	}

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

	// 누적 트랙
	st_Line[2].Format(_T("생성 %d, 삭제 %d (스캔 %d 까지)"), nCreatedNum, nDeletedNum, nCurScan);

	// 트랙 표가 차서 못 만든 트랙이 있을 때만
	if (st_Status->nTableFullSum > 0)
	{
		st_Line[2].AppendFormat(_T(", 표가 차서 못 만듦 %d"), st_Status->nTableFullSum);
	}

	st_Line[3].Format(_T("개시 플롯: 같은 표적 %d, 클러터끼리 %d, 섞임 %d"), nTargetSum, pt_Count[0], pt_Count[TGT_MAX_TARGET_NUM + 1]);

	// 지금 스캔: 트랙 관리 (스코어 로직) 다음 2 Point 개시 순서로 처리한 기록
	if (nIndex >= 0)
	{
		const ST_TrkScanLog *st_Log = &st_TrackResult.f_GetScan(nIndex)->st_Log;

		st_TrackResult.f_CountPlot(nIndex, &nTargetPlot, &nClutterPlot);
		st_Line[4].Format(_T("받은 플롯 %d (표적 %d, 클러터 %d)"), st_Log->nPlotNum, nTargetPlot, nClutterPlot);
		st_Line[5].Format(_T("개시 후보 %d, 검사한 쌍 %d, 게이트 통과 %d"), st_Log->nCandidateNum, st_Log->nPairNum, st_Log->nGatePassNum);
		st_Line[6].Format(_T("트랙 생성 %d, 확정 %d, 메모리 %d, 삭제 %d"), st_Log->nCreatedNum, st_Log->nConfirmedNum, st_Log->nMemoryNum, st_Log->nDeletedNum);
	}
	else
	{
		st_Line[4] = _T("받지 않음 (아직 처리 전이거나 UDP 손실)");
	}

	// 조건: 과제 값, 개시 스코어는 정한 값
	st_Line[7].Format(_T("V_max %.0f m/s, 개시 스코어 %d, Assigned_Flag 0 고정"), TRK_VMAX, TRK_SCORE_INIT);
	st_Line[8].Format(_T("확정 %d, 삭제 %d, 메모리 삭제 %d"), TRK_SCORE_CONFIRM, TRK_SCORE_DELETE, TRK_MEMORY_DELETE);
	st_Line[9].Format(_T("R 표준편차: σr %g m, σaz %g°, σel %g°"), TRK_R_SIGMA_RANGE, TRK_R_SIGMA_AZIMUTH, TRK_R_SIGMA_ELEVATION);

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
			st_TrackResult.f_FormatCell(nIndex, st_Info->item.iItem, st_Info->item.iSubItem, 0, pt_Cell, RES_CELL_SIZE);
		}

		(VOID)MultiByteToWideChar(CP_UTF8, 0, pt_Cell, -1, st_Info->item.pszText, st_Info->item.cchTextMax);
	}

	*pt_Result = 0;
}

// 수신 쪽이 처리한 스캔을 가져옴. 따라가는 중이면 마지막 스캔을 보이고, 아니면 보던 스캔이 이번에 막 왔을 때만 다시 그림.
// 송수신이 끝나면 (쓰레드 둘 다 끝) 남은 스캔까지 가져온 뒤 타이머를 멈춤.
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

// 스캔 슬라이더. 마지막으로 받은 스캔에 두면 새 스캔을 따라가고, 앞으로 옮기면 그 스캔에 머묾. 아직 받지 않은 스캔으로는 가지 않음.
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

// 휠: 커서가 2D, 3D 그림 위면 그 그림을 확대 · 축소 (포커스가 다른 컨트롤에 있어도).
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

	return CDialogEx::OnMouseWheel(flags, delta, st_Point);
}

// 그림 표시 (참값, 플롯, 클러터, 트랙) 켜기 / 끄기. 결과는 그대로 두고 2D, 3D 그림만 다시 그림.
VOID CTargetSimUIDlg::f_OnShowClicked(VOID)
{
	INT32 nIndex;

	for (nIndex = 0; nIndex < UI_SHOW_NUM; nIndex++)
	{
		isShow[nIndex] = (IsDlgButtonChecked(s_ShowId[nIndex]) == BST_CHECKED) ? 1 : 0;
	}

	st_Plot.f_SetShow(isShow[UI_SHOW_TRUE], isShow[UI_SHOW_PLOT], isShow[UI_SHOW_CLUTTER], isShow[UI_SHOW_TRACK]);
	st_View3D.f_SetShow(isShow[UI_SHOW_TRUE], isShow[UI_SHOW_PLOT], isShow[UI_SHOW_CLUTTER], isShow[UI_SHOW_TRACK]);
}

// 그림 보기 (2D + 3D, 2D, 3D) 바꾸면 배치를 다시 함.
VOID CTargetSimUIDlg::f_OnViewModeChanged(VOID)
{
	const INT32 nSel = st_ViewMode.GetCurSel();

	if ((nSel >= 0) && (nSel < UI_VIEW_NUM))
	{
		nViewMode = nSel;
		f_Layout();
	}
}

// 송신 간격 바꾸면 처음부터 다시 보냄 (100 ms 면 스캔 간격과 같은 실시간이라 트랙이 생기고 지워지는 모습을 따라 볼 수 있음).
VOID CTargetSimUIDlg::f_OnTrkGapChanged(VOID)
{
	const INT32 nSel = st_GapCombo.GetCurSel();

	if ((nSel >= 0) && (nSel < UI_TRK_GAP_NUM) && (s_GapMs[nSel] != sendGapMs))
	{
		sendGapMs = s_GapMs[nSel];
		f_RunTrack();
	}
}

// 같은 송신 간격으로 처음부터 다시 보냄 (궤적, 측정 모의 결과는 그대로).
VOID CTargetSimUIDlg::f_OnTrkResendClicked(VOID)
{
	f_RunTrack();
}

// 트랙 스코어 확인 창. 연관 결과 (Assigned_Flag) 를 직접 넣어 확정, 메모리 상태 변화를 봄 (시뮬레이션은 늘 0).
VOID CTargetSimUIDlg::f_OnTrkScoreClicked(VOID)
{
	CScoreDlg st_ScoreDlg(this);

	(VOID)st_ScoreDlg.DoModal();
}

// 받은 전체 스캔의 트랙을 CSV 로 저장 (한 줄 = 스캔 하나의 트랙 하나).
VOID CTargetSimUIDlg::f_OnSaveCsvClicked(VOID)
{
	CFileDialog st_FileDlg(FALSE, _T("csv"), _T("TargetSim_Track.csv"), OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY,
		_T("CSV (*.csv)|*.csv||"), this);

	if ((st_FileDlg.DoModal() == IDOK) && (st_TrackResult.f_WriteCsv(st_FileDlg.GetPathName()) == 0))
	{
		(VOID)AfxMessageBox(_T("CSV 파일을 쓸 수 없습니다. 다른 프로그램이 열고 있지 않은지 확인하십시오."), MB_ICONWARNING);
	}
}
