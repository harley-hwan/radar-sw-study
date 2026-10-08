#include "pch.h"
#include "framework.h"

#include <math.h>

#include "RandDlg.h"
#include "TargetSimUI.h"
#include "TargetSimUIDlg.h"
#include "UiCommon.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define UI_INIT_WIDTH			1400					// [px @96dpi]
#define UI_INIT_HEIGHT			880
#define UI_MIN_WIDTH			1300					// 결과 카드 머리 (보기, 비교 두 개, CSV) 가 겹치지 않는 너비
#define UI_MIN_HEIGHT			720
#define UI_MARGIN				12
#define UI_GAP					10
#define UI_CARD_PAD				12
#define UI_CARD_RADIUS			10
#define UI_LEFT_RATIO			0.50
#define UI_LEFT_MIN				690						// 객체 표 8 열이 잘리지 않는 너비
#define UI_LEFT_MAX				860
#define UI_RIGHT_MIN			420
#define UI_PLOT_RATIO			0.60
#define UI_OBJ_MIN_ROWS			3
#define UI_VIEW_WIDTH			96						// 결과 보기 선택 너비
#define UI_VIEW_DROP_HEIGHT		240						// 결과 보기 목록 펼친 높이 (12 줄)
#define UI_CHECK_BOX_PAD		24						// 체크 상자 네모와 글자 앞 간격
#define UI_RESULT_METHOD_MAX	3						// 결과 표에 나란히 보일 계산 수 (기본 + 비교 2 개)
#define UI_PCS_SNR				20.0					// 측정 모의 처음 값: SNR (선형), 스캔당 클러터 수 (과제 조건)
#define UI_PCS_CLUTTER			5
#define UI_PCS_SNR_WIDTH		64						// SNR 입력 칸 너비
#define UI_PCS_CLUTTER_WIDTH	48
#define UI_PCS_BUTTON_WIDTH		110						// 난수 확인 단추 너비

static LPCTSTR			s_TurnChoice[SCN_TURN_TYPE_NUM];

// 컨트롤 위치, 크기 변경.
static VOID f_Ui_Move(CWnd *st_Dlg, INT32 ctrlId, INT32 left, INT32 top, INT32 width, INT32 height)
{
	(VOID)st_Dlg->GetDlgItem(ctrlId)->SetWindowPos(nullptr, left, top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

// 컨트롤 글자 폭 [px].
static INT32 f_Ui_TextWidth(CWnd *st_Ctrl)
{
	CClientDC	st_Dc(st_Ctrl);
	CFont		*st_OldFont = st_Dc.SelectObject(st_Ctrl->GetFont());
	CString		st_Text;
	INT32		width;

	st_Ctrl->GetWindowText(st_Text);
	width = st_Dc.GetTextExtent(st_Text).cx;
	(VOID)st_Dc.SelectObject(st_OldFont);

	return width;
}

BEGIN_MESSAGE_MAP(CTargetSimUIDlg, CDialogEx)
	ON_WM_ERASEBKGND()
	ON_WM_CTLCOLOR()
	ON_WM_SIZE()
	ON_WM_GETMINMAXINFO()
	ON_WM_CLOSE()
	ON_BN_CLICKED(IDC_SCN_MENU, &CTargetSimUIDlg::f_OnScenarioMenuClicked)
	ON_COMMAND_RANGE(ID_SCN_PRESET_FIRST, ID_SCN_PRESET_FIRST + SCN_PRESET_NUM - 1, &CTargetSimUIDlg::f_OnScenarioPreset)
	ON_BN_CLICKED(IDC_CALC_LOG, &CTargetSimUIDlg::f_OnCalcLogClicked)
	ON_BN_CLICKED(IDC_OBJ_ADD, &CTargetSimUIDlg::f_OnTargetAddClicked)
	ON_BN_CLICKED(IDC_OBJ_DELETE, &CTargetSimUIDlg::f_OnTargetDeleteClicked)
	ON_BN_CLICKED(IDC_MNV_ADD, &CTargetSimUIDlg::f_OnManeuverAddClicked)
	ON_BN_CLICKED(IDC_MNV_DELETE, &CTargetSimUIDlg::f_OnManeuverDeleteClicked)
	ON_BN_CLICKED(IDC_SAVE_CSV, &CTargetSimUIDlg::f_OnSaveCsvClicked)
	ON_CBN_SELCHANGE(IDC_RESULT_VIEW, &CTargetSimUIDlg::f_OnResultViewChanged)
	ON_BN_CLICKED(IDC_RESULT_COMPARE, &CTargetSimUIDlg::f_OnResultCompareClicked)
	ON_BN_CLICKED(IDC_RESULT_PLATFORM, &CTargetSimUIDlg::f_OnResultCompareClicked)
	ON_NOTIFY(GRIDN_CELLCHANGED, IDC_OBJ_GRID, &CTargetSimUIDlg::f_OnObjCellChanged)
	ON_NOTIFY(GRIDN_ROWCHANGED, IDC_OBJ_GRID, &CTargetSimUIDlg::f_OnObjRowChanged)
	ON_NOTIFY(GRIDN_CELLCHANGED, IDC_MNV_GRID, &CTargetSimUIDlg::f_OnMnvCellChanged)
	ON_NOTIFY(LVN_GETDISPINFO, IDC_RESULT_LIST, &CTargetSimUIDlg::f_OnResultGetDispInfo)
	ON_EN_KILLFOCUS(IDC_PCS_SNR, &CTargetSimUIDlg::f_OnPcsInputKillFocus)
	ON_EN_KILLFOCUS(IDC_PCS_CLUTTER, &CTargetSimUIDlg::f_OnPcsInputKillFocus)
	ON_BN_CLICKED(IDC_PCS_SHOW_PLOT, &CTargetSimUIDlg::f_OnPcsShowClicked)
	ON_BN_CLICKED(IDC_PCS_SHOW_CLUTTER, &CTargetSimUIDlg::f_OnPcsShowClicked)
	ON_BN_CLICKED(IDC_PCS_RAND, &CTargetSimUIDlg::f_OnPcsRandClicked)
END_MESSAGE_MAP()

// 주 대화상자.
CTargetSimUIDlg::CTargetSimUIDlg(CWnd *st_Parent)
	: CDialogEx(IDD_TARGETSIMUI_DIALOG, st_Parent)
	, st_TopBar(0, 0, 0, 0)
	, st_CardObj(0, 0, 0, 0)
	, st_CardMnv(0, 0, 0, 0)
	, st_CardPlot(0, 0, 0, 0)
	, st_CardResult(0, 0, 0, 0)
	, st_CardPcs(0, 0, 0, 0)
	, dpi(UI_BASE_DPI)
	, textHeight(16)
	, nCurObject(1)
	, nResultObject(1)
	, nResultObjectNum(0)
	, isResultCompare(0)
	, isPlatformCompare(0)
	, snr(UI_PCS_SNR)
	, nClutterNum(UI_PCS_CLUTTER)
	, isShowPlot(1)
	, isShowClutter(1)
{
}

// 컨트롤 변수와 대화상자 자원 연결.
VOID CTargetSimUIDlg::DoDataExchange(CDataExchange *st_Dx)
{
	CDialogEx::DoDataExchange(st_Dx);
	DDX_Control(st_Dx, IDC_OBJ_GRID, st_ObjGrid);
	DDX_Control(st_Dx, IDC_MNV_GRID, st_MnvGrid);
	DDX_Control(st_Dx, IDC_PLOT, st_Plot);
	DDX_Control(st_Dx, IDC_RESULT_LIST, st_ResultList);
	DDX_Control(st_Dx, IDC_RESULT_VIEW, st_ResultView);
}

// 글꼴, 색, 표 준비 후 과제 4 시나리오 (기본 시나리오 + 표적 3개) 첫 실행.
BOOL CTargetSimUIDlg::OnInitDialog(VOID)
{
	CClientDC	st_Dc(this);
	CFont		*st_OldFont;
	LOGFONT		st_LogFont;
	TEXTMETRIC	st_Metric;

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
	GetDlgItem(IDC_OBJ_TITLE)->SetFont(&st_TitleFont);
	GetDlgItem(IDC_MNV_TITLE)->SetFont(&st_TitleFont);
	GetDlgItem(IDC_RESULT_TITLE)->SetFont(&st_TitleFont);
	GetDlgItem(IDC_PCS_TITLE)->SetFont(&st_TitleFont);

	(VOID)st_ResultList.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

	// 측정 모의 입력, 지도 표시
	f_ShowMeasureInput();
	CheckDlgButton(IDC_PCS_SHOW_PLOT, BST_CHECKED);
	CheckDlgButton(IDC_PCS_SHOW_CLUTTER, BST_CHECKED);

	st_Plot.f_SetDpi(dpi);
	f_SetupGrids();
	f_SetInitialSize();
	f_LoadPreset(SCN_PRESET_TARGET5);

	GotoDlgCtrl(&st_ObjGrid);

	return FALSE;
}

// 배치

// 두 표의 열 정의. 너비 비율 = 최소 폭일 때 열에 필요한 픽셀.
VOID CTargetSimUIDlg::f_SetupGrids(VOID)
{
	static const ST_GridColumn	s_ObjColumn[UI_OBJ_COL_NUM] =
	{
		{ _T("객체"),			GRID_KIND_LABEL,	76,		nullptr,		0 },
		{ _T("위도 [°]"),		GRID_KIND_NUMBER,	86,		nullptr,		0 },
		{ _T("경도 [°]"),		GRID_KIND_NUMBER,	90,		nullptr,		0 },
		{ _T("고도 [m]"),		GRID_KIND_NUMBER,	66,		nullptr,		0 },
		{ _T("속력 [m/s]"),	GRID_KIND_NUMBER,	78,		nullptr,		0 },
		{ _T("Roll [°]"),		GRID_KIND_NUMBER,	60,		nullptr,		0 },
		{ _T("Pitch [°]"),	GRID_KIND_NUMBER,	66,		nullptr,		0 },
		{ _T("Yaw [°]"),		GRID_KIND_NUMBER,	62,		nullptr,		0 }
	};
	static const ST_GridColumn	s_MnvColumn[UI_MNV_COL_NUM] =
	{
		{ _T("#"),				GRID_KIND_LABEL,	40,		nullptr,		0 },
		{ _T("축"),				GRID_KIND_CHOICE,	92,		s_TurnChoice,	SCN_TURN_TYPE_NUM },
		{ _T("G"),				GRID_KIND_NUMBER,	74,		nullptr,		0 },
		{ _T("시작 [s]"),		GRID_KIND_NUMBER,	84,		nullptr,		0 },
		{ _T("종료 [s]"),		GRID_KIND_NUMBER,	84,		nullptr,		0 }
	};
	INT32						nType;

	for (nType = 0; nType < SCN_TURN_TYPE_NUM; nType++)
	{
		s_TurnChoice[nType] = CScenario::f_TurnName(nType);
	}

	st_ObjGrid.f_Setup(s_ObjColumn, UI_OBJ_COL_NUM, dpi);
	st_MnvGrid.f_Setup(s_MnvColumn, UI_MNV_COL_NUM, dpi);
}

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

// 크기 변경 시 배치 재계산. 생성 중, 최소화 때는 생략.
VOID CTargetSimUIDlg::OnSize(UINT32 type, INT32 width, INT32 height)
{
	CDialogEx::OnSize(type, width, height);

	if ((type != SIZE_MINIMIZED) && (st_ObjGrid.GetSafeHwnd() != nullptr))
	{
		f_Layout();
	}
}

// 위 막대, 좌우 카드 배치 계산. 모든 치수에 화면 배율 적용.
VOID CTargetSimUIDlg::f_Layout(VOID)
{
	const INT32	margin = f_Ui_Scale(UI_MARGIN, dpi);
	const INT32	gap = f_Ui_Scale(UI_GAP, dpi);
	const INT32	pad = f_Ui_Scale(UI_CARD_PAD, dpi);
	const INT32	tight = f_Ui_Scale(6, dpi);
	const INT32	buttonHeight = textHeight + f_Ui_Scale(12, dpi);
	const INT32	hintHeight = textHeight + f_Ui_Scale(4, dpi);
	CRect		st_Client;
	CRect		st_View;
	INT32		leftWidth;
	INT32		contentTop;
	INT32		contentBottom;
	INT32		rightLeft;
	INT32		rightWidth;
	INT32		gridHeight;
	INT32		nShownRow;
	INT32		checkWidth;
	INT32		labelWidth;
	INT32		pcsHeight;
	INT32		x;
	INT32		y;

	GetClientRect(&st_Client);

	// 위 막대 (오른쪽 끝에 계산 로그)
	st_TopBar.SetRect(0, 0, st_Client.right, buttonHeight + (2 * f_Ui_Scale(10, dpi)));
	x = margin + f_Ui_Scale(110, dpi) + (2 * gap);
	y = f_Ui_Scale(10, dpi);
	checkWidth = f_Ui_TextWidth(GetDlgItem(IDC_CALC_LOG)) + f_Ui_Scale(UI_CHECK_BOX_PAD, dpi);
	f_Ui_Move(this, IDC_SCN_MENU, margin, y, f_Ui_Scale(110, dpi), buttonHeight);
	f_Ui_Move(this, IDC_SCN_NAME, x, y, st_Client.right - margin - checkWidth - gap - x, buttonHeight);
	f_Ui_Move(this, IDC_CALC_LOG, st_Client.right - margin - checkWidth, y, checkWidth, buttonHeight);

	contentTop		= st_TopBar.bottom + margin;
	contentBottom	= st_Client.bottom - margin;
	leftWidth		= static_cast<INT32>(static_cast<FLOAT64>(st_Client.Width()) * UI_LEFT_RATIO);

	if (leftWidth < f_Ui_Scale(UI_LEFT_MIN, dpi))
	{
		leftWidth = f_Ui_Scale(UI_LEFT_MIN, dpi);
	}
	else if (leftWidth > f_Ui_Scale(UI_LEFT_MAX, dpi))
	{
		leftWidth = f_Ui_Scale(UI_LEFT_MAX, dpi);
	}
	else
	{
		// 비율대로
	}

	if (leftWidth > (st_Client.Width() - (3 * margin) - f_Ui_Scale(UI_RIGHT_MIN, dpi)))
	{
		leftWidth = st_Client.Width() - (3 * margin) - f_Ui_Scale(UI_RIGHT_MIN, dpi);
	}

	rightLeft	= margin + leftWidth + gap;
	rightWidth	= st_Client.right - margin - rightLeft;

	// 객체 카드는 표적 수만큼, 나머지는 기동 카드
	nShownRow = st_Scenario.nTargetNum + 1;

	if (nShownRow < UI_OBJ_MIN_ROWS)
	{
		nShownRow = UI_OBJ_MIN_ROWS;
	}

	gridHeight = st_ObjGrid.f_GetHeightForRows(nShownRow);

	if (gridHeight > (((contentBottom - contentTop) * 58) / 100))
	{
		gridHeight = ((contentBottom - contentTop) * 58) / 100;
	}

	st_CardObj.SetRect(margin, contentTop, margin + leftWidth, contentTop + pad + buttonHeight + tight + gridHeight + pad);
	x = st_CardObj.right - pad - f_Ui_Scale(64, dpi);
	y = st_CardObj.top + pad;
	f_Ui_Move(this, IDC_OBJ_DELETE, x, y, f_Ui_Scale(64, dpi), buttonHeight);
	x = x - tight - f_Ui_Scale(84, dpi);
	f_Ui_Move(this, IDC_OBJ_ADD, x, y, f_Ui_Scale(84, dpi), buttonHeight);
	f_Ui_Move(this, IDC_OBJ_TITLE, st_CardObj.left + pad, y, x - tight - (st_CardObj.left + pad), buttonHeight);
	f_Ui_Move(this, IDC_OBJ_GRID, st_CardObj.left + pad, y + buttonHeight + tight, st_CardObj.Width() - (2 * pad), gridHeight);

	// 측정 모의 카드 (과제 4). 왼쪽 맨 아래에 높이 고정: 제목 줄, 입력 줄, 요약 두 줄
	pcsHeight = pad + buttonHeight + tight + buttonHeight + tight + (2 * textHeight) + f_Ui_Scale(4, dpi) + pad;
	st_CardPcs.SetRect(margin, contentBottom - pcsHeight, margin + leftWidth, contentBottom);

	// 기동 카드 (아래에 도움말 두 줄). 측정 모의 카드 위까지
	st_CardMnv.SetRect(margin, st_CardObj.bottom + gap, margin + leftWidth, st_CardPcs.top - gap);
	x = st_CardMnv.right - pad - f_Ui_Scale(64, dpi);
	y = st_CardMnv.top + pad;
	f_Ui_Move(this, IDC_MNV_DELETE, x, y, f_Ui_Scale(64, dpi), buttonHeight);
	x = x - tight - f_Ui_Scale(84, dpi);
	f_Ui_Move(this, IDC_MNV_ADD, x, y, f_Ui_Scale(84, dpi), buttonHeight);
	f_Ui_Move(this, IDC_MNV_TITLE, st_CardMnv.left + pad, y, x - tight - (st_CardMnv.left + pad), buttonHeight);
	gridHeight = st_CardMnv.bottom - pad - (2 * hintHeight) - tight - (y + buttonHeight + tight);
	f_Ui_Move(this, IDC_MNV_GRID, st_CardMnv.left + pad, y + buttonHeight + tight, st_CardMnv.Width() - (2 * pad), gridHeight);
	f_Ui_Move(this, IDC_MNV_HINT, st_CardMnv.left + pad, st_CardMnv.bottom - pad - (2 * hintHeight), st_CardMnv.Width() - (2 * pad), hintHeight);
	f_Ui_Move(this, IDC_EDIT_HINT, st_CardMnv.left + pad, st_CardMnv.bottom - pad - hintHeight, st_CardMnv.Width() - (2 * pad), hintHeight);

	// 측정 모의 카드: 제목 + 난수 확인 단추, SNR, 스캔당 클러터, 지도 표시 두 개, 요약
	x = st_CardPcs.right - pad - f_Ui_Scale(UI_PCS_BUTTON_WIDTH, dpi);
	y = st_CardPcs.top + pad;
	f_Ui_Move(this, IDC_PCS_RAND, x, y, f_Ui_Scale(UI_PCS_BUTTON_WIDTH, dpi), buttonHeight);
	f_Ui_Move(this, IDC_PCS_TITLE, st_CardPcs.left + pad, y, x - tight - (st_CardPcs.left + pad), buttonHeight);
	x = st_CardPcs.left + pad;
	y = y + buttonHeight + tight;
	labelWidth = f_Ui_TextWidth(GetDlgItem(IDC_PCS_SNR_LABEL)) + tight;
	f_Ui_Move(this, IDC_PCS_SNR_LABEL, x, y, labelWidth, buttonHeight);
	x = x + labelWidth;
	f_Ui_Move(this, IDC_PCS_SNR, x, y, f_Ui_Scale(UI_PCS_SNR_WIDTH, dpi), buttonHeight);
	x = x + f_Ui_Scale(UI_PCS_SNR_WIDTH, dpi) + (2 * gap);
	labelWidth = f_Ui_TextWidth(GetDlgItem(IDC_PCS_CLUTTER_LABEL)) + tight;
	f_Ui_Move(this, IDC_PCS_CLUTTER_LABEL, x, y, labelWidth, buttonHeight);
	x = x + labelWidth;
	f_Ui_Move(this, IDC_PCS_CLUTTER, x, y, f_Ui_Scale(UI_PCS_CLUTTER_WIDTH, dpi), buttonHeight);
	x = x + f_Ui_Scale(UI_PCS_CLUTTER_WIDTH, dpi) + (2 * gap);
	checkWidth = f_Ui_TextWidth(GetDlgItem(IDC_PCS_SHOW_PLOT)) + f_Ui_Scale(UI_CHECK_BOX_PAD, dpi);
	f_Ui_Move(this, IDC_PCS_SHOW_PLOT, x, y, checkWidth, buttonHeight);
	x = x + checkWidth + gap;
	f_Ui_Move(this, IDC_PCS_SHOW_CLUTTER, x, y, f_Ui_TextWidth(GetDlgItem(IDC_PCS_SHOW_CLUTTER)) + f_Ui_Scale(UI_CHECK_BOX_PAD, dpi), buttonHeight);
	y = y + buttonHeight + tight;
	f_Ui_Move(this, IDC_PCS_SUMMARY, st_CardPcs.left + pad, y, st_CardPcs.Width() - (2 * pad), st_CardPcs.bottom - pad - y);

	// 그림 카드
	st_CardPlot.SetRect(rightLeft, contentTop, rightLeft + rightWidth, contentTop + static_cast<INT32>(static_cast<FLOAT64>(contentBottom - contentTop) * UI_PLOT_RATIO));
	f_Ui_Move(this, IDC_PLOT, st_CardPlot.left + pad, st_CardPlot.top + pad, st_CardPlot.Width() - (2 * pad), st_CardPlot.Height() - (2 * pad));

	// 결과 카드 (제목 바로 뒤에 보기 선택, 비중점법 비교, 플랫폼 기준 비교)
	st_CardResult.SetRect(rightLeft, st_CardPlot.bottom + gap, rightLeft + rightWidth, contentBottom);
	y = st_CardResult.top + pad;
	f_Ui_Move(this, IDC_SAVE_CSV, st_CardResult.right - pad - f_Ui_Scale(110, dpi), y, f_Ui_Scale(110, dpi), buttonHeight);
	x = st_CardResult.left + pad + f_Ui_TextWidth(GetDlgItem(IDC_RESULT_TITLE)) + tight;
	f_Ui_Move(this, IDC_RESULT_TITLE, st_CardResult.left + pad, y, x - (st_CardResult.left + pad), buttonHeight);
	// 콤보 높이 = 닫힌 높이 + 펼친 목록 높이
	st_ResultView.GetWindowRect(&st_View);
	f_Ui_Move(this, IDC_RESULT_VIEW, x + tight, y + ((buttonHeight - st_View.Height()) / 2), f_Ui_Scale(UI_VIEW_WIDTH, dpi),
		st_View.Height() + f_Ui_Scale(UI_VIEW_DROP_HEIGHT, dpi));
	x = x + tight + f_Ui_Scale(UI_VIEW_WIDTH, dpi) + gap;
	checkWidth = f_Ui_TextWidth(GetDlgItem(IDC_RESULT_COMPARE)) + f_Ui_Scale(UI_CHECK_BOX_PAD, dpi);
	f_Ui_Move(this, IDC_RESULT_COMPARE, x, y, checkWidth, buttonHeight);
	x = x + checkWidth + gap;
	f_Ui_Move(this, IDC_RESULT_PLATFORM, x, y, f_Ui_TextWidth(GetDlgItem(IDC_RESULT_PLATFORM)) + f_Ui_Scale(UI_CHECK_BOX_PAD, dpi), buttonHeight);
	f_Ui_Move(this, IDC_RESULT_LIST, st_CardResult.left + pad, y + buttonHeight + tight, st_CardResult.Width() - (2 * pad),
		st_CardResult.bottom - pad - (y + buttonHeight + tight));

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
	static const INT32	s_FramedCtrl[4] = { IDC_OBJ_GRID, IDC_MNV_GRID, IDC_RESULT_LIST, IDC_PLOT };
	CBrush				st_Border(UI_COLOR_BORDER);
	CRect				st_Client;
	CRect				st_Frame;
	INT32				nIndex;

	GetClientRect(&st_Client);
	st_Dc->FillSolidRect(&st_Client, UI_COLOR_PAGE);
	st_Dc->FillSolidRect(&st_TopBar, UI_COLOR_CARD);
	st_Dc->FillSolidRect(st_TopBar.left, st_TopBar.bottom - 1, st_TopBar.Width(), 1, UI_COLOR_BORDER);

	f_DrawCard(st_Dc, st_CardObj);
	f_DrawCard(st_Dc, st_CardMnv);
	f_DrawCard(st_Dc, st_CardPlot);
	f_DrawCard(st_Dc, st_CardResult);
	f_DrawCard(st_Dc, st_CardPcs);

	for (nIndex = 0; nIndex < 4; nIndex++)
	{
		GetDlgItem(s_FramedCtrl[nIndex])->GetWindowRect(&st_Frame);
		ScreenToClient(&st_Frame);
		st_Frame.InflateRect(1, 1);
		st_Dc->FrameRect(&st_Frame, &st_Border);
	}

	return TRUE;
}

// 글자 배경 = 카드 색. 도움말, 시나리오 이름은 옅은 색.
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

		(VOID)st_Dc->SetTextColor(((ctrlId == IDC_MNV_HINT) || (ctrlId == IDC_EDIT_HINT) || (ctrlId == IDC_SCN_NAME)) ? UI_COLOR_TEXT_SUB : UI_COLOR_TEXT);
		(VOID)st_Dc->SetBkColor(UI_COLOR_CARD);
		brush = static_cast<HBRUSH>(st_CardBrush.GetSafeHandle());
	}
	else
	{
		// 기본 색
	}

	return brush;
}

// Enter: 창 유지, 입력 중인 값 확정.
VOID CTargetSimUIDlg::OnOK(VOID)
{
	st_ObjGrid.f_EndEdit(1);
	st_MnvGrid.f_EndEdit(1);
	f_ApplyMeasureInput();
}

// Esc: 창 유지, 입력 중인 값 취소.
VOID CTargetSimUIDlg::OnCancel(VOID)
{
	st_ObjGrid.f_EndEdit(0);
	st_MnvGrid.f_EndEdit(0);
	f_ShowMeasureInput();
}

// 닫기 단추. 입력 중인 값은 버리고 창 닫기. 계산 로그 콘솔도 닫음.
VOID CTargetSimUIDlg::OnClose(VOID)
{
	OnCancel();
	st_CalcLog.f_Close();
	EndDialog(IDCANCEL);
}

// 편집

// 객체 번호 -> 편집 글자. 0 = 플랫폼.
ST_ObjectText *CTargetSimUIDlg::f_GetObject(INT32 nObject)
{
	return (nObject == 0) ? &st_Scenario.st_Platform : &st_Scenario.st_Target[nObject - 1];
}

// 프리셋 불러오기, 두 표 채우기, 실행.
VOID CTargetSimUIDlg::f_LoadPreset(INT32 nPreset)
{
	CString st_Name;

	st_Scenario.f_LoadPreset(nPreset);
	st_Name.Format(_T("%s  ·  시간 %g s, 간격 %g s"), CScenario::f_PresetName(nPreset), st_Scenario.durationTime, SCN_STEP_TIME);
	SetDlgItemText(IDC_SCN_NAME, st_Name);

	f_RefreshObjGrid();
	f_Layout();
	f_SelectObject(1);
	f_RunScenario();
}

// 객체 표 채우기.
VOID CTargetSimUIDlg::f_RefreshObjGrid(VOID)
{
	const INT32	nRowNum = st_Scenario.nTargetNum + 1;
	CString		st_Name;
	INT32		nObject;
	INT32		nField;

	st_ObjGrid.f_SetRowNum(nRowNum);

	for (nObject = 0; nObject < nRowNum; nObject++)
	{
		if (nObject == 0)
		{
			st_Name = _T("플랫폼");
		}
		else
		{
			st_Name.Format(_T("표적 %d"), nObject);
		}

		(VOID)st_ObjGrid.SetItemText(nObject, UI_OBJ_COL_NAME, st_Name);

		for (nField = 0; nField < SCN_OBJ_FIELD_NUM; nField++)
		{
			(VOID)st_ObjGrid.SetItemText(nObject, UI_OBJ_COL_FIELD + nField, f_GetObject(nObject)->st_FieldText[nField]);
		}
	}
}

// 선택 표적의 기동 표 채우기. 플랫폼은 기동 없음.
VOID CTargetSimUIDlg::f_RefreshMnvGrid(VOID)
{
	const INT32	nManeuverNum = (nCurObject >= 1) ? f_GetObject(nCurObject)->nManeuverNum : 0;
	CString		st_Index;
	INT32		nManeuver;
	INT32		nField;

	st_MnvGrid.f_SetRowNum(nManeuverNum);

	for (nManeuver = 0; nManeuver < nManeuverNum; nManeuver++)
	{
		const ST_ManeuverText *st_Maneuver = &f_GetObject(nCurObject)->st_Maneuver[nManeuver];

		st_Index.Format(_T("%d"), nManeuver + 1);
		(VOID)st_MnvGrid.SetItemText(nManeuver, UI_MNV_COL_INDEX, st_Index);
		(VOID)st_MnvGrid.SetItemText(nManeuver, UI_MNV_COL_TYPE, CScenario::f_TurnName(static_cast<INT32>(st_Maneuver->enTurnType)));

		for (nField = 0; nField < SCN_MNV_FIELD_NUM; nField++)
		{
			(VOID)st_MnvGrid.SetItemText(nManeuver, UI_MNV_COL_FIELD + nField, st_Maneuver->st_FieldText[nField]);
		}
	}

	f_UpdateTitles();
}

// 두 표 제목에 현재 / 최대 개수 표시.
VOID CTargetSimUIDlg::f_UpdateTitles(VOID)
{
	CString st_Text;

	st_Text.Format(_T("플랫폼 / 표적  (%d/%d)"), st_Scenario.nTargetNum, TGT_MAX_TARGET_NUM);
	SetDlgItemText(IDC_OBJ_TITLE, st_Text);

	if (nCurObject >= 1)
	{
		st_Text.Format(_T("기동: 표적 %d  (%d/%d)"), nCurObject, f_GetObject(nCurObject)->nManeuverNum, TGT_MAX_MANEUVER_NUM);
	}
	else
	{
		st_Text = _T("기동 (플랫폼은 기동 없음)");
	}

	SetDlgItemText(IDC_MNV_TITLE, st_Text);
}

// 객체 선택 후 기동 표 갱신.
VOID CTargetSimUIDlg::f_SelectObject(INT32 nObject)
{
	nCurObject = nObject;
	st_ObjGrid.f_SelectRow(nObject);
	f_RefreshMnvGrid();
}

// 객체 표 행 변경 시 해당 객체의 기동 표시.
VOID CTargetSimUIDlg::f_OnObjRowChanged(NMHDR *st_Hdr, LRESULT *pt_Result)
{
	const ST_GridNotify *st_Notify = reinterpret_cast<ST_GridNotify *>(st_Hdr);

	if (st_Notify->nRow != nCurObject)
	{
		f_SelectObject(st_Notify->nRow);
	}

	*pt_Result = 0;
}

// 객체 표 칸 변경 시 편집 글자 반영 후 재실행.
VOID CTargetSimUIDlg::f_OnObjCellChanged(NMHDR *st_Hdr, LRESULT *pt_Result)
{
	const ST_GridNotify *st_Notify = reinterpret_cast<ST_GridNotify *>(st_Hdr);

	f_GetObject(st_Notify->nRow)->st_FieldText[st_Notify->nColumn - UI_OBJ_COL_FIELD] = st_ObjGrid.GetItemText(st_Notify->nRow, st_Notify->nColumn);
	f_RunScenario();

	*pt_Result = 0;
}

// 기동 표 칸 변경 시 편집 글자 반영 후 재실행. 축 칸은 이름으로 회전축 검색.
VOID CTargetSimUIDlg::f_OnMnvCellChanged(NMHDR *st_Hdr, LRESULT *pt_Result)
{
	const ST_GridNotify	*st_Notify = reinterpret_cast<ST_GridNotify *>(st_Hdr);
	const CString		st_Text = st_MnvGrid.GetItemText(st_Notify->nRow, st_Notify->nColumn);
	ST_ManeuverText		*st_Maneuver = &f_GetObject(nCurObject)->st_Maneuver[st_Notify->nRow];
	INT32				nType;

	if (st_Notify->nColumn == UI_MNV_COL_TYPE)
	{
		for (nType = 0; nType < SCN_TURN_TYPE_NUM; nType++)
		{
			if (st_Text == CScenario::f_TurnName(nType))
			{
				st_Maneuver->enTurnType = static_cast<EN_TurnType>(nType);
			}
		}
	}
	else
	{
		st_Maneuver->st_FieldText[st_Notify->nColumn - UI_MNV_COL_FIELD] = st_Text;
	}

	f_RunScenario();

	*pt_Result = 0;
}

// 표적 추가 (선택 표적 초기값 복사). 10 개면 무시.
VOID CTargetSimUIDlg::f_OnTargetAddClicked(VOID)
{
	const INT32 nNewTarget = st_Scenario.f_AddTarget(nCurObject - 1);

	if (nNewTarget >= 0)
	{
		f_RefreshObjGrid();
		f_Layout();
		f_SelectObject(nNewTarget + 1);
		f_RunScenario();
	}
}

// 선택 표적 삭제 후 같은 자리(마지막이면 앞) 표적 선택. 최소 1 개 유지.
VOID CTargetSimUIDlg::f_OnTargetDeleteClicked(VOID)
{
	if ((nCurObject >= 1) && (st_Scenario.nTargetNum > 1))
	{
		st_Scenario.f_DeleteTarget(nCurObject - 1);
		f_RefreshObjGrid();
		f_Layout();
		f_SelectObject((nCurObject > st_Scenario.nTargetNum) ? st_Scenario.nTargetNum : nCurObject);
		f_RunScenario();
	}
}

// 선택 표적에 기동 추가. 30 개면 무시.
VOID CTargetSimUIDlg::f_OnManeuverAddClicked(VOID)
{
	const INT32 nNewManeuver = (nCurObject >= 1) ? st_Scenario.f_AddManeuver(nCurObject - 1) : -1;

	if (nNewManeuver >= 0)
	{
		f_RefreshMnvGrid();
		st_MnvGrid.f_SelectRow(nNewManeuver);
		f_RunScenario();
	}
}

// 선택 기동 삭제.
VOID CTargetSimUIDlg::f_OnManeuverDeleteClicked(VOID)
{
	const INT32 nManeuver = st_MnvGrid.f_GetCurRow();

	if (nManeuver >= 0)
	{
		st_Scenario.f_DeleteManeuver(nCurObject - 1, nManeuver);
		f_RefreshMnvGrid();
		f_RunScenario();
	}
}

// 시나리오 단추 아래 프리셋 메뉴 표시.
VOID CTargetSimUIDlg::f_OnScenarioMenuClicked(VOID)
{
	CMenu	st_Menu;
	CRect	st_Button;
	INT32	nPreset;

	(VOID)st_Menu.CreatePopupMenu();

	for (nPreset = 0; nPreset < SCN_PRESET_NUM; nPreset++)
	{
		(VOID)st_Menu.AppendMenu(MF_STRING, static_cast<UINT_PTR>(ID_SCN_PRESET_FIRST) + static_cast<UINT_PTR>(nPreset), CScenario::f_PresetName(nPreset));
	}

	GetDlgItem(IDC_SCN_MENU)->GetWindowRect(&st_Button);
	(VOID)st_Menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_TOPALIGN, st_Button.left, st_Button.bottom + 2, this);
}

// 메뉴에서 고른 프리셋 불러오기.
VOID CTargetSimUIDlg::f_OnScenarioPreset(UINT32 command)
{
	f_LoadPreset(static_cast<INT32>(command) - ID_SCN_PRESET_FIRST);
}

// 실행

// 편집 글자로 설정 생성, 전 구간 실행 후 결과 표와 그림 갱신. 측정 모의도 다시 함. 계산 로그를 켰으면 콘솔도 다시 씀.
VOID CTargetSimUIDlg::f_RunScenario(VOID)
{
	ST_SimConfig st_Config = {};

	st_Scenario.f_BuildConfig(&st_Config);
	st_Result.f_Run(&st_Config);

	// 같은 설정을 비중점법으로 한 번 더 (결과 표 비교용)
	st_Config.noMidPoint = 1;
	st_NoMidResult.f_Run(&st_Config);

	// 같은 설정을 플랫폼 기준으로 한 번 더 (결과 표 비교용)
	st_Config.noMidPoint	= 0;
	st_Config.usePlatform	= 1;
	st_PlatformResult.f_Run(&st_Config);

	f_SetupResultList();
	st_Plot.f_SetResult(&st_Result, (isPlatformCompare != 0) ? &st_PlatformResult : nullptr);
	f_RunMeasure();
	f_WriteCalcLog();
}

// 결과 표: 행은 표본 수. 객체 수가 바뀐 경우에만 보기 목록, 열 재생성.
VOID CTargetSimUIDlg::f_SetupResultList(VOID)
{
	if (st_Result.f_GetObjectNum() != nResultObjectNum)
	{
		nResultObjectNum = st_Result.f_GetObjectNum();
		f_SetupResultView();
		f_SetupResultColumns();
	}

	(VOID)st_ResultList.SetItemCountEx(st_Result.f_GetSampleNum(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
	st_ResultList.Invalidate(FALSE);
}

// 보기 목록: 전체, 플랫폼, 표적 1 ~ N. 보던 표적이 지워졌으면 마지막 표적.
VOID CTargetSimUIDlg::f_SetupResultView(VOID)
{
	CString st_Name;
	INT32	nObject;

	st_ResultView.ResetContent();
	(VOID)st_ResultView.AddString(_T("전체"));
	(VOID)st_ResultView.AddString(_T("플랫폼"));

	for (nObject = 1; nObject < nResultObjectNum; nObject++)
	{
		st_Name.Format(_T("표적 %d"), nObject);
		(VOID)st_ResultView.AddString(st_Name);
	}

	if (nResultObject >= nResultObjectNum)
	{
		nResultObject = nResultObjectNum - 1;
	}

	(VOID)st_ResultView.SetCurSel(nResultObject + 1);
}

// 결과 표 열. 전체 보기는 CSV 와 같은 순서, 객체 하나 보기는 스텝, 시각, 위도, 경도, 고도.
// 비교를 켜면 위도, 경도, 고도마다 기본 열 바로 뒤에 켠 비교 (비중점법, 플랫폼 기준) 열.
VOID CTargetSimUIDlg::f_SetupResultColumns(VOID)
{
	static const LPCTSTR	s_PartName[3] = { _T("위도 [°]"), _T("경도 [°]"), _T("고도 [m]") };
	static const INT32		s_PartWidth[3] = { 124, 128, 128 };
	const INT32				nShownNum = (nResultObject < 0) ? nResultObjectNum : 1;
	LPCTSTR					pt_MethodName[UI_RESULT_METHOD_MAX];
	const CSimResult		*st_MethodSource[UI_RESULT_METHOD_MAX];
	const INT32				nMethodNum = f_GetResultMethod(pt_MethodName, st_MethodSource);
	CString					st_Object;
	CString					st_Title;
	INT32					nColumn;
	INT32					nShown;
	INT32					nPart;
	INT32					nMethod;
	INT32					width;

	for (nColumn = st_ResultList.GetHeaderCtrl()->GetItemCount() - 1; nColumn >= 0; nColumn--)
	{
		(VOID)st_ResultList.DeleteColumn(nColumn);
	}

	(VOID)st_ResultList.InsertColumn(0, _T("스텝"), LVCFMT_LEFT, f_Ui_Scale(58, dpi));
	(VOID)st_ResultList.InsertColumn(1, _T("시각 [s]"), LVCFMT_RIGHT, f_Ui_Scale(74, dpi));
	nColumn = 2;

	for (nShown = 0; nShown < nShownNum; nShown++)
	{
		// 전체 보기만 열 제목 앞에 객체 이름
		if (nResultObject >= 0)
		{
			st_Object.Empty();
		}
		else if (nShown == 0)
		{
			st_Object = _T("플랫폼 ");
		}
		else
		{
			st_Object.Format(_T("표적%d "), nShown);
		}

		for (nPart = 0; nPart < 3; nPart++)
		{
			for (nMethod = 0; nMethod < nMethodNum; nMethod++)
			{
				// 비교할 때만 열 제목에 계산 이름. 열 너비는 제목이 다 보이게.
				st_Title	= st_Object + pt_MethodName[nMethod] + s_PartName[nPart];
				width		= st_ResultList.GetStringWidth(st_Title) + f_Ui_Scale(16, dpi);
				width		= (width > f_Ui_Scale(s_PartWidth[nPart], dpi)) ? width : f_Ui_Scale(s_PartWidth[nPart], dpi);
				(VOID)st_ResultList.InsertColumn(nColumn, st_Title, LVCFMT_RIGHT, width);
				nColumn		= nColumn + 1;
			}
		}
	}
}

// 결과 표에 나란히 보일 계산. 첫째는 기본 계산이고 켠 비교가 뒤에 붙음. 이름은 열 제목 앞에 붙는 말. 계산 수 반환.
INT32 CTargetSimUIDlg::f_GetResultMethod(LPCTSTR pt_Name[], const CSimResult *st_Source[]) const
{
	INT32 nMethodNum = 1;

	// 기본 계산 이름: 비교가 하나면 그 비교와 짝이 되는 말, 둘이면 '기본'
	if (isResultCompare != 0)
	{
		pt_Name[0] = (isPlatformCompare != 0) ? _T("기본 ") : _T("중점 ");
	}
	else
	{
		pt_Name[0] = (isPlatformCompare != 0) ? _T("표적 기준 ") : _T("");
	}

	st_Source[0] = &st_Result;

	if (isResultCompare != 0)
	{
		pt_Name[nMethodNum]		= _T("비중점 ");
		st_Source[nMethodNum]	= &st_NoMidResult;
		nMethodNum				= nMethodNum + 1;
	}

	if (isPlatformCompare != 0)
	{
		pt_Name[nMethodNum]		= _T("플랫폼 기준 ");
		st_Source[nMethodNum]	= &st_PlatformResult;
		nMethodNum				= nMethodNum + 1;
	}

	return nMethodNum;
}

// 보기 변경. 행은 그대로 두고 열만 다시 만듦. 계산 로그도 그 객체로 다시 씀.
VOID CTargetSimUIDlg::f_OnResultViewChanged(VOID)
{
	nResultObject = st_ResultView.GetCurSel() - 1;
	f_SetupResultColumns();
	st_ResultList.Invalidate(FALSE);
	f_WriteCalcLog();
}

// 비중점법 비교, 플랫폼 기준 비교 켜기 / 끄기. 결과는 이미 계산돼 있어 열과 그림만 다시 만듦.
// 플랫폼 기준 비교는 궤적 그림에도 점선으로 겹쳐 그림.
VOID CTargetSimUIDlg::f_OnResultCompareClicked(VOID)
{
	isResultCompare		= (IsDlgButtonChecked(IDC_RESULT_COMPARE) == BST_CHECKED) ? 1 : 0;
	isPlatformCompare	= (IsDlgButtonChecked(IDC_RESULT_PLATFORM) == BST_CHECKED) ? 1 : 0;
	f_SetupResultColumns();
	st_ResultList.Invalidate(FALSE);
	st_Plot.f_SetResult(&st_Result, (isPlatformCompare != 0) ? &st_PlatformResult : nullptr);
}

// 가상 리스트 요청 칸의 글자 제공. 보이는 행만 요청됨.
// 위도, 경도, 고도 열은 어느 객체, 어느 방식의 열인지 찾아 그 결과의 CSV 열 번호로 바꿔 요청.
VOID CTargetSimUIDlg::f_OnResultGetDispInfo(NMHDR *st_Hdr, LRESULT *pt_Result)
{
	NMLVDISPINFO		*st_Info = reinterpret_cast<NMLVDISPINFO *>(st_Hdr);
	LPCTSTR				pt_MethodName[UI_RESULT_METHOD_MAX];
	const CSimResult	*st_MethodSource[UI_RESULT_METHOD_MAX];
	const INT32			nMethodNum = f_GetResultMethod(pt_MethodName, st_MethodSource);
	const CSimResult	*st_Source = &st_Result;
	CHAR				pt_Cell[RES_CELL_SIZE];
	INT32				nColumn = st_Info->item.iSubItem;
	INT32				nObject;
	INT32				nPart;

	if ((st_Info->item.mask & LVIF_TEXT) != 0U)
	{
		// 3 열부터 객체마다 (위도, 경도, 고도) x 방식 수 만큼
		if (nColumn >= 2)
		{
			nObject		= (nResultObject >= 0) ? nResultObject : ((nColumn - 2) / (3 * nMethodNum));
			nPart		= ((nColumn - 2) % (3 * nMethodNum)) / nMethodNum;
			st_Source	= st_MethodSource[(nColumn - 2) % nMethodNum];
			nColumn		= 2 + (3 * nObject) + nPart;
		}

		st_Source->f_FormatCell(st_Info->item.iItem, nColumn, pt_Cell, RES_CELL_SIZE);
		(VOID)MultiByteToWideChar(CP_UTF8, 0, pt_Cell, -1, st_Info->item.pszText, st_Info->item.cchTextMax);
	}

	*pt_Result = 0;
}

// 결과 CSV 저장 (보기, 비교와 관계없이 전체 객체의 기본 결과). 입력 중인 값은 단추 클릭 시 확정되어 이미 결과에 반영됨.
VOID CTargetSimUIDlg::f_OnSaveCsvClicked(VOID)
{
	CFileDialog st_FileDlg(FALSE, _T("csv"), _T("TargetSim_LLA.csv"), OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY,
		_T("CSV (*.csv)|*.csv||"), this);

	if (st_FileDlg.DoModal() == IDOK)
	{
		if (st_Result.f_WriteCsv(st_FileDlg.GetPathName()) == 0)
		{
			(VOID)AfxMessageBox(_T("CSV 파일을 쓸 수 없습니다. 다른 프로그램이 열고 있지 않은지 확인하십시오."), MB_ICONWARNING);
		}
	}
}

// 계산 로그 켜기 / 끄기. 켜면 콘솔 창을 열고 지금 보기 객체의 계산 과정을 씀. 콘솔을 못 열면 체크를 되돌림.
VOID CTargetSimUIDlg::f_OnCalcLogClicked(VOID)
{
	if (IsDlgButtonChecked(IDC_CALC_LOG) != BST_CHECKED)
	{
		st_CalcLog.f_Close();
	}
	else if (st_CalcLog.f_Open() != 0)
	{
		f_WriteCalcLog();
	}
	else
	{
		CheckDlgButton(IDC_CALC_LOG, BST_UNCHECKED);
	}
}

// 계산 로그가 켜져 있으면 지금 시나리오, 지금 보기 객체 (결과 표와 같음) 의 계산 과정을 다시 씀.
VOID CTargetSimUIDlg::f_WriteCalcLog(VOID)
{
	CString st_Title;

	if (st_CalcLog.f_IsOpen() != 0)
	{
		(VOID)GetDlgItemText(IDC_SCN_NAME, st_Title);
		st_CalcLog.f_Write(&st_Scenario, nResultObject, st_Title);
	}
}

// 측정 모의 (과제 4)

// 기본 계산 결과 (st_Result) 를 스캔마다 재서 플롯, 클러터를 만들고 요약 줄과 지도 갱신.
VOID CTargetSimUIDlg::f_RunMeasure(VOID)
{
	CString st_Text;

	st_PlotResult.f_Run(&st_Result, snr, nClutterNum);
	st_Text.Format(_T("표준편차 p (이론)     거리 %.3f m,  방위각 %.4f°,  고각 %.4f°\r\n플롯 오차 표준편차   거리 %.3f m,  방위각 %.4f°,  고각 %.4f°     (플롯 %d개, 클러터 %d개)"),
		st_PlotResult.st_Sigma.r, f_Rad_To_Deg(st_PlotResult.st_Sigma.az), f_Rad_To_Deg(st_PlotResult.st_Sigma.el), st_PlotResult.st_ErrorStd.r,
		f_Rad_To_Deg(st_PlotResult.st_ErrorStd.az), f_Rad_To_Deg(st_PlotResult.st_ErrorStd.el), st_PlotResult.nPlotNum, st_PlotResult.nClutterSum);
	SetDlgItemText(IDC_PCS_SUMMARY, st_Text);
	st_Plot.f_SetMeasure(&st_PlotResult, isShowPlot, isShowClutter);
}

// SNR, 스캔당 클러터 입력 확정. 바뀌었으면 측정 모의만 다시 함 (표적 궤적은 그대로).
// SNR 은 0 보다 큰 수, 클러터 수는 0 ~ PCS_MAX_CLUTTER. 아니면 경고음 후 지금 값으로 되돌림.
VOID CTargetSimUIDlg::f_ApplyMeasureInput(VOID)
{
	CString	st_Text;
	TCHAR	*pt_End = nullptr;
	FLOAT64	newSnr;
	UINT32	nNewClutter;
	BOOL	isNumber = FALSE;

	(VOID)GetDlgItemText(IDC_PCS_SNR, st_Text);
	(VOID)st_Text.Trim();
	newSnr		= _tcstod(st_Text.GetString(), &pt_End);
	nNewClutter	= GetDlgItemInt(IDC_PCS_CLUTTER, &isNumber, FALSE);

	if ((st_Text.IsEmpty() != FALSE) || (*pt_End != _T('\0')) || !isfinite(newSnr) || (newSnr <= 0.0) || (isNumber == FALSE)
		|| (nNewClutter > static_cast<UINT32>(PCS_MAX_CLUTTER)))
	{
		(VOID)::MessageBeep(MB_ICONWARNING);
		f_ShowMeasureInput();
	}
	else if ((newSnr != snr) || (static_cast<INT32>(nNewClutter) != nClutterNum))
	{
		snr			= newSnr;
		nClutterNum	= static_cast<INT32>(nNewClutter);
		f_RunMeasure();
	}
	else
	{
		// 바뀐 것 없음
	}
}

// 지금 쓰는 SNR, 클러터 수를 입력 칸에 표시.
VOID CTargetSimUIDlg::f_ShowMeasureInput(VOID)
{
	CString st_Text;

	st_Text.Format(_T("%g"), snr);
	SetDlgItemText(IDC_PCS_SNR, st_Text);
	SetDlgItemInt(IDC_PCS_CLUTTER, static_cast<UINT32>(nClutterNum), FALSE);
}

// 입력 칸에서 나가면 입력 확정.
VOID CTargetSimUIDlg::f_OnPcsInputKillFocus(VOID)
{
	f_ApplyMeasureInput();
}

// 지도에 플롯, 클러터 보이기 / 숨기기. 결과는 그대로 두고 그림만 다시 그림.
VOID CTargetSimUIDlg::f_OnPcsShowClicked(VOID)
{
	isShowPlot		= (IsDlgButtonChecked(IDC_PCS_SHOW_PLOT) == BST_CHECKED) ? 1 : 0;
	isShowClutter	= (IsDlgButtonChecked(IDC_PCS_SHOW_CLUTTER) == BST_CHECKED) ? 1 : 0;
	st_Plot.f_SetMeasure(&st_PlotResult, isShowPlot, isShowClutter);
}

// 난수 확인 창 (UNIRAN, GAUSS 히스토그램).
VOID CTargetSimUIDlg::f_OnPcsRandClicked(VOID)
{
	CRandDlg st_RandDlg(this);

	(VOID)st_RandDlg.DoModal();
}
