#include "pch.h"
#include "framework.h"

#include <math.h>
#include <string.h>

#include "TargetSimUI.h"
#include "TargetSimUIDlg.h"
#include "UiNumber_JH.h"
#include "UiTheme_JH.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define IDM_ABOUTBOX			0x0010
#define UI_STEP_CHOICE_NUM		9
#define UI_INIT_WIDTH			1400					// [px @96dpi] 처음 뜰 때의 클라이언트 크기
#define UI_INIT_HEIGHT			880
#define UI_MIN_WIDTH			1180
#define UI_MIN_HEIGHT			720
#define UI_MARGIN				12
#define UI_GAP					10
#define UI_CARD_PAD				12
#define UI_CARD_RADIUS			10
#define UI_LEFT_RATIO			0.50
#define UI_LEFT_MIN				690						// 객체 표의 아홉 열이 머리글과 값을 자르지 않는 너비
#define UI_LEFT_MAX				860
#define UI_RIGHT_MIN			420
#define UI_PLOT_RATIO			0.60
#define UI_OBJ_MIN_ROWS			3
#define UI_BAR_HEIGHT			14
#define UI_CELL_SIZE			RES_CELL_SIZE

static const INT32		s_PlaySpeed[UI_PLAY_SPEED_NUM] = { 1, 2, 5, 10, 50, 100 };
static const LPCTSTR	s_PlaySpeedName[UI_PLAY_SPEED_NUM] = { _T("x1"), _T("x2"), _T("x5"), _T("x10"), _T("x50"), _T("x100") };
static const LPCTSTR	s_StepChoice[UI_STEP_CHOICE_NUM] = { _T("0.001"), _T("0.005"), _T("0.01"), _T("0.02"), _T("0.05"), _T("0.1"), _T("0.2"), _T("0.5"), _T("1") };
static LPCTSTR			s_TurnChoice[SCN_TURN_TYPE_NUM] = { _T(""), _T(""), _T(""), _T("") };

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg() noexcept : CDialogEx(IDD_ABOUTBOX) {}

protected:
	VOID DoDataExchange(CDataExchange *st_Dx) override { CDialogEx::DoDataExchange(st_Dx); }

	DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

static VOID f_Ui_Move(CWnd *st_Dlg, INT32 ctrlId, INT32 left, INT32 top, INT32 width, INT32 height)
{
	CWnd *st_Ctrl = st_Dlg->GetDlgItem(ctrlId);

	if (st_Ctrl != nullptr)
	{
		(VOID)st_Ctrl->SetWindowPos(nullptr, left, top, (width > 0) ? width : 0, (height > 0) ? height : 0, SWP_NOZORDER | SWP_NOACTIVATE);
	}
}

static VOID f_Ui_EnableCtrl(CDialog *st_Dlg, INT32 ctrlId, INT32 isEnabled)
{
	CWnd *st_Ctrl = st_Dlg->GetDlgItem(ctrlId);

	if (st_Ctrl != nullptr)
	{
		// 포커스를 가진 컨트롤을 끄면 대화상자에 포커스가 사라져 키보드가 먹통이 되므로 다음 컨트롤로 먼저 옮긴다.
		if ((isEnabled == 0) && (::GetFocus() == st_Ctrl->GetSafeHwnd()))
		{
			st_Dlg->NextDlgCtrl();
		}

		(VOID)st_Ctrl->EnableWindow((isEnabled != 0) ? TRUE : FALSE);
	}
}

BEGIN_MESSAGE_MAP(CTargetSimUIDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_CTLCOLOR()
	ON_WM_QUERYDRAGICON()
	ON_WM_SIZE()
	ON_WM_GETMINMAXINFO()
	ON_WM_TIMER()
	ON_WM_HSCROLL()
	ON_WM_CLOSE()
	ON_BN_CLICKED(IDC_SCN_MENU, &CTargetSimUIDlg::f_OnScenarioMenuClicked)
	ON_COMMAND_RANGE(ID_SCN_PRESET_FIRST, ID_SCN_PRESET_FIRST + SCN_PRESET_NUM - 1, &CTargetSimUIDlg::f_OnScenarioPreset)
	ON_COMMAND(ID_SCN_OPEN, &CTargetSimUIDlg::f_OpenScenario)
	ON_COMMAND(ID_SCN_SAVE, &CTargetSimUIDlg::f_SaveScenario)
	ON_BN_CLICKED(IDC_RUN, &CTargetSimUIDlg::f_OnRunClicked)
	ON_BN_CLICKED(IDC_OBJ_ADD, &CTargetSimUIDlg::f_OnTargetAddClicked)
	ON_BN_CLICKED(IDC_OBJ_COPY, &CTargetSimUIDlg::f_OnTargetCopyClicked)
	ON_BN_CLICKED(IDC_OBJ_DELETE, &CTargetSimUIDlg::f_OnTargetDeleteClicked)
	ON_BN_CLICKED(IDC_MNV_ADD, &CTargetSimUIDlg::f_OnManeuverAddClicked)
	ON_BN_CLICKED(IDC_MNV_DELETE, &CTargetSimUIDlg::f_OnManeuverDeleteClicked)
	ON_BN_CLICKED(IDC_PLAY, &CTargetSimUIDlg::f_OnPlayClicked)
	ON_BN_CLICKED(IDC_SAVE_CSV, &CTargetSimUIDlg::f_OnSaveCsvClicked)
	ON_CBN_SELCHANGE(IDC_PLAY_SPEED, &CTargetSimUIDlg::f_OnSpeedSelChange)
	ON_CBN_SELCHANGE(IDC_SIM_STEP, &CTargetSimUIDlg::f_OnStepSelChange)
	ON_CBN_KILLFOCUS(IDC_SIM_STEP, &CTargetSimUIDlg::f_OnSimFieldKillFocus)
	ON_EN_KILLFOCUS(IDC_SIM_DURATION, &CTargetSimUIDlg::f_OnSimFieldKillFocus)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SIM_DURATION_SPIN, &CTargetSimUIDlg::f_OnDurationSpin)
	ON_NOTIFY(GRIDN_CELLCHANGED, IDC_OBJ_GRID, &CTargetSimUIDlg::f_OnObjCellChanged)
	ON_NOTIFY(GRIDN_ROWCHANGED, IDC_OBJ_GRID, &CTargetSimUIDlg::f_OnObjRowChanged)
	ON_NOTIFY(GRIDN_DRAWCELL, IDC_OBJ_GRID, &CTargetSimUIDlg::f_OnObjDrawCell)
	ON_NOTIFY(GRIDN_CELLCHANGED, IDC_MNV_GRID, &CTargetSimUIDlg::f_OnMnvCellChanged)
	ON_NOTIFY(PLOTN_SELECT, IDC_PLOT, &CTargetSimUIDlg::f_OnPlotSelect)
	ON_NOTIFY(PLOTN_MOVE, IDC_PLOT, &CTargetSimUIDlg::f_OnPlotMove)
	ON_NOTIFY(PLOTN_TURN, IDC_PLOT, &CTargetSimUIDlg::f_OnPlotTurn)
	ON_NOTIFY(PLOTN_SCRUB, IDC_PLOT, &CTargetSimUIDlg::f_OnPlotScrub)
	ON_NOTIFY(LVN_GETDISPINFO, IDC_RESULT_LIST, &CTargetSimUIDlg::f_OnResultGetDispInfo)
	ON_NOTIFY(LVN_ODFINDITEM, IDC_RESULT_LIST, &CTargetSimUIDlg::f_OnResultFindItem)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_RESULT_LIST, &CTargetSimUIDlg::f_OnResultItemChanged)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_RESULT_LIST, &CTargetSimUIDlg::f_OnResultCustomDraw)
END_MESSAGE_MAP()

CTargetSimUIDlg::CTargetSimUIDlg(CWnd *st_Parent)
	: CDialogEx(IDD_TARGETSIMUI_DIALOG, st_Parent)
	, iconHandle(nullptr)
	, st_TopBar(0, 0, 0, 0)
	, st_StatusBar(0, 0, 0, 0)
	, st_CardObj(0, 0, 0, 0)
	, st_CardMnv(0, 0, 0, 0)
	, st_CardPlot(0, 0, 0, 0)
	, st_CardResult(0, 0, 0, 0)
	, dpi(UI_BASE_DPI)
	, textHeight(16)
	, nCurObject(1)
	, isSyncing(0)
	, isResultStale(0)
	, nResultColumnNum(0)
	, statusKind(UI_STATUS_INFO)
	, nCurStep(0)
	, isPlaying(0)
	, nPlayOriginStep(0)
	, nPlaySpeed(1)
	, nPlayTick(0)
	, isModified(0)
	, isIssueActive(0)
	, isRunPending(0)
	, isRunning(0)
	, isClosing(0)
{
	iconHandle = ::LoadIcon(nullptr, IDI_APPLICATION);
	(VOID)memset(&st_RunConfig, 0, sizeof(st_RunConfig));
}

CTargetSimUIDlg::~CTargetSimUIDlg()
{
}

VOID CTargetSimUIDlg::DoDataExchange(CDataExchange *st_Dx)
{
	CDialogEx::DoDataExchange(st_Dx);
	DDX_Control(st_Dx, IDC_OBJ_GRID, st_ObjGrid);
	DDX_Control(st_Dx, IDC_MNV_GRID, st_MnvGrid);
	DDX_Control(st_Dx, IDC_PLOT, st_Plot);
	DDX_Control(st_Dx, IDC_RESULT_LIST, st_ResultList);
	DDX_Control(st_Dx, IDC_TIME_SLIDER, st_TimeSlider);
	DDX_Control(st_Dx, IDC_PLAY_SPEED, st_SpeedCombo);
	DDX_Control(st_Dx, IDC_SIM_STEP, st_StepCombo);
	DDX_Control(st_Dx, IDC_SIM_DURATION_SPIN, st_DurationSpin);
}

BOOL CTargetSimUIDlg::OnInitDialog(VOID)
{
	CClientDC	st_Dc(this);
	CMenu		*st_SysMenu;
	CString		st_AboutText;
	CString		st_Module;
	CFont		*st_OldFont;
	LOGFONT		st_LogFont;
	TEXTMETRIC	st_Metric;
	INT32		nIndex;
	INT32		nSlash;

	(VOID)CDialogEx::OnInitDialog();

	// IDM_ABOUTBOX 는 시스템 명령 범위 안에 있어야 한다.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	st_SysMenu = GetSystemMenu(FALSE);

	if (st_SysMenu != nullptr)
	{
		if ((st_AboutText.LoadString(IDS_ABOUTBOX) != FALSE) && (!st_AboutText.IsEmpty()))
		{
			(VOID)st_SysMenu->AppendMenu(MF_SEPARATOR);
			(VOID)st_SysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, st_AboutText);
		}
	}

	(VOID)SetIcon(iconHandle, TRUE);
	(VOID)SetIcon(iconHandle, FALSE);

	// 치수는 모두 대화상자 글꼴 높이와 DPI 에서 나온다.
	dpi = st_Dc.GetDeviceCaps(LOGPIXELSX);
	st_OldFont = st_Dc.SelectObject(GetFont());
	(VOID)st_Dc.GetTextMetrics(&st_Metric);
	(VOID)st_Dc.SelectObject(st_OldFont);
	textHeight = st_Metric.tmHeight;

	(VOID)st_CardBrush.CreateSolidBrush(UI_COLOR_CARD);
	(VOID)st_PageBrush.CreateSolidBrush(UI_COLOR_PAGE);

	if ((GetFont() != nullptr) && (GetFont()->GetLogFont(&st_LogFont) != 0))
	{
		st_LogFont.lfWeight = FW_BOLD;
		st_LogFont.lfHeight = ::MulDiv(st_LogFont.lfHeight, 108, 100);
		(VOID)st_TitleFont.CreateFontIndirect(&st_LogFont);
		GetDlgItem(IDC_OBJ_TITLE)->SetFont(&st_TitleFont);
		GetDlgItem(IDC_MNV_TITLE)->SetFont(&st_TitleFont);
		GetDlgItem(IDC_RESULT_TITLE)->SetFont(&st_TitleFont);
	}

	(VOID)SendDlgItemMessage(IDC_SIM_DURATION, EM_LIMITTEXT, SCN_TEXT_LIMIT, 0);
	(VOID)st_StepCombo.LimitText(SCN_TEXT_LIMIT);

	for (nIndex = 0; nIndex < UI_STEP_CHOICE_NUM; nIndex++)
	{
		(VOID)st_StepCombo.AddString(s_StepChoice[nIndex]);
	}

	for (nIndex = 0; nIndex < UI_PLAY_SPEED_NUM; nIndex++)
	{
		(VOID)st_SpeedCombo.AddString(s_PlaySpeedName[nIndex]);
	}

	(VOID)st_SpeedCombo.SetCurSel(0);
	nPlaySpeed = s_PlaySpeed[0];
	st_DurationSpin.SetRange32(-1000000, 1000000);
	(VOID)st_DurationSpin.SetPos32(0);

	(VOID)st_ResultList.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
	(VOID)st_ResultList.SetBkColor(UI_COLOR_CARD);

	// CSV 기본 폴더는 실행 파일 폴더
	(VOID)GetModuleFileName(nullptr, st_Module.GetBuffer(MAX_PATH), MAX_PATH);
	st_Module.ReleaseBuffer();
	nSlash = st_Module.ReverseFind(_T('\\'));
	st_CsvFolder = (nSlash >= 0) ? st_Module.Left(nSlash + 1) : CString();

	st_Plot.f_SetDpi(dpi);
	f_SetupGrids();
	f_SetInitialSize();

	st_Scenario.f_LoadPreset(SCN_PRESET_SPEC);
	st_ScenarioName = CScenario::f_PresetName(SCN_PRESET_SPEC);
	f_EnableResultControls(0);
	f_RefreshAll();

	// 첫 포커스를 객체 표에 둔다.
	GotoDlgCtrl(&st_ObjGrid);

	return FALSE;
}

// ---------------------------------------------------------------------------
// 배치·모양

VOID CTargetSimUIDlg::f_SetupGrids(VOID)
{
	ST_GridColumn	st_ObjColumn[UI_OBJ_COL_NUM] =
	{
		// 너비 비율은 가장 좁을 때 각 열에 필요한 픽셀 수다. 위도·경도는 그림에서 끌어 놓은 값(소수 6 자리)까지 들어간다.
		{ _T("객체"),			GRID_KIND_LABEL,	76,		0, 0.0,		0.0,				0.0,				nullptr, 0 },
		{ _T("위도 [°]"),		GRID_KIND_NUMBER,	86,		3, 0.001,	-SCN_LAT_LIMIT,		SCN_LAT_LIMIT,		nullptr, 0 },
		{ _T("경도 [°]"),		GRID_KIND_NUMBER,	90,		3, 0.001,	-SCN_LON_LIMIT,		SCN_LON_LIMIT,		nullptr, 0 },
		{ _T("고도 [m]"),		GRID_KIND_NUMBER,	66,		0, 10.0,	SCN_ALT_MIN,		SCN_ALT_MAX,		nullptr, 0 },
		{ _T("속력 [m/s]"),	GRID_KIND_NUMBER,	78,		0, 1.0,		0.0,				SCN_SPEED_MAX,		nullptr, 0 },
		{ _T("Roll [°]"),		GRID_KIND_NUMBER,	60,		0, 1.0,		-SCN_ROLL_LIMIT,	SCN_ROLL_LIMIT,		nullptr, 0 },
		{ _T("Pitch [°]"),	GRID_KIND_NUMBER,	66,		0, 1.0,		-SCN_PITCH_LIMIT,	SCN_PITCH_LIMIT,	nullptr, 0 },
		{ _T("Yaw [°]"),		GRID_KIND_NUMBER,	62,		0, 1.0,		-SCN_YAW_LIMIT,		SCN_YAW_LIMIT,		nullptr, 0 },
		{ _T("기동 구간"),		GRID_KIND_CUSTOM,	76,		0, 0.0,		0.0,				0.0,				nullptr, 0 }
	};
	ST_GridColumn	st_MnvColumn[UI_MNV_COL_NUM] =
	{
		{ _T("#"),				GRID_KIND_LABEL,	40,		0, 0.0,		0.0,				0.0,				nullptr, 0 },
		{ _T("축"),				GRID_KIND_CHOICE,	92,		0, 0.0,		0.0,				0.0,				s_TurnChoice, SCN_TURN_TYPE_NUM },
		{ _T("G"),				GRID_KIND_NUMBER,	74,		1, 0.1,		-SCN_G_LIMIT,		SCN_G_LIMIT,		nullptr, 0 },
		{ _T("시작 [s]"),		GRID_KIND_NUMBER,	84,		0, 0.1,		0.0,				SCN_DURATION_MAX,	nullptr, 0 },
		{ _T("종료 [s]"),		GRID_KIND_NUMBER,	84,		0, 0.1,		0.0,				SCN_DURATION_MAX,	nullptr, 0 },
		{ _T("회전각 [°]"),	GRID_KIND_VALUE,	90,		0, 0.0,		0.0,				0.0,				nullptr, 0 },
		{ _T("선회 반경 [m]"),	GRID_KIND_VALUE,	106,	0, 0.0,		0.0,				0.0,				nullptr, 0 }
	};
	INT32			nType;

	for (nType = 0; nType < SCN_TURN_TYPE_NUM; nType++)
	{
		s_TurnChoice[nType] = CScenario::f_TurnName(nType);
	}

	st_ObjGrid.f_Setup(st_ObjColumn, UI_OBJ_COL_NUM, dpi);
	st_MnvGrid.f_Setup(st_MnvColumn, UI_MNV_COL_NUM, dpi);
}

VOID CTargetSimUIDlg::f_SetInitialSize(VOID)
{
	CRect	st_Work;
	CRect	st_Window(0, 0, f_Ui_Scale(UI_INIT_WIDTH, dpi), f_Ui_Scale(UI_INIT_HEIGHT, dpi));

	(VOID)::AdjustWindowRectEx(&st_Window, GetStyle(), FALSE, GetExStyle());

	if (::SystemParametersInfo(SPI_GETWORKAREA, 0, &st_Work, 0) != FALSE)
	{
		INT32 width = st_Window.Width();
		INT32 height = st_Window.Height();

		if (width > (st_Work.Width() - 40))
		{
			width = st_Work.Width() - 40;
		}

		if (height > (st_Work.Height() - 40))
		{
			height = st_Work.Height() - 40;
		}

		(VOID)SetWindowPos(nullptr, st_Work.left + ((st_Work.Width() - width) / 2), st_Work.top + ((st_Work.Height() - height) / 2),
			width, height, SWP_NOZORDER | SWP_NOACTIVATE);
	}
}

VOID CTargetSimUIDlg::OnGetMinMaxInfo(MINMAXINFO *st_Info)
{
	CRect st_Min(0, 0, f_Ui_Scale(UI_MIN_WIDTH, dpi), f_Ui_Scale(UI_MIN_HEIGHT, dpi));
	CRect st_Work;

	CDialogEx::OnGetMinMaxInfo(st_Info);

	if (GetSafeHwnd() != nullptr)
	{
		(VOID)::AdjustWindowRectEx(&st_Min, GetStyle(), FALSE, GetExStyle());
		st_Info->ptMinTrackSize.x = st_Min.Width();
		st_Info->ptMinTrackSize.y = st_Min.Height();

		// 배율을 높인 작은 화면에서는 최소 크기가 화면보다 클 수 있다. 그때는 화면에 맞춘다.
		if (::SystemParametersInfo(SPI_GETWORKAREA, 0, &st_Work, 0) != FALSE)
		{
			if (st_Info->ptMinTrackSize.x > st_Work.Width())
			{
				st_Info->ptMinTrackSize.x = st_Work.Width();
			}

			if (st_Info->ptMinTrackSize.y > st_Work.Height())
			{
				st_Info->ptMinTrackSize.y = st_Work.Height();
			}
		}
	}
}

VOID CTargetSimUIDlg::OnSize(UINT32 type, INT32 width, INT32 height)
{
	CDialogEx::OnSize(type, width, height);

	if ((type != SIZE_MINIMIZED) && (st_ObjGrid.GetSafeHwnd() != nullptr))
	{
		f_Layout();
	}
}

VOID CTargetSimUIDlg::f_Layout(VOID)
{
	const INT32	margin = f_Ui_Scale(UI_MARGIN, dpi);
	const INT32	gap = f_Ui_Scale(UI_GAP, dpi);
	const INT32	pad = f_Ui_Scale(UI_CARD_PAD, dpi);
	const INT32	tight = f_Ui_Scale(6, dpi);
	const INT32	buttonHeight = textHeight + f_Ui_Scale(12, dpi);
	const INT32	hintHeight = textHeight + f_Ui_Scale(4, dpi);
	CRect		st_Client;
	CRect		st_Combo;
	INT32		comboHeight;
	INT32		leftWidth;
	INT32		contentTop;
	INT32		contentBottom;
	INT32		rightLeft;
	INT32		rightWidth;
	INT32		gridHeight;
	INT32		nShownRow;
	INT32		x;
	INT32		y;

	GetClientRect(&st_Client);

	if ((st_Client.Width() > 0) && (st_Client.Height() > 0))
	{
		st_StepCombo.GetWindowRect(&st_Combo);
		comboHeight = st_Combo.Height();

		// 위 막대
		st_TopBar.SetRect(0, 0, st_Client.right, buttonHeight + (2 * f_Ui_Scale(10, dpi)));
		y = f_Ui_Scale(10, dpi);
		x = margin;
		f_Ui_Move(this, IDC_SCN_MENU, x, y, f_Ui_Scale(110, dpi), buttonHeight);
		x = x + f_Ui_Scale(110, dpi) + (2 * gap);
		f_Ui_Move(this, IDC_LBL_DURATION, x, y, f_Ui_Scale(52, dpi), buttonHeight);
		x = x + f_Ui_Scale(52, dpi) + tight;
		f_Ui_Move(this, IDC_SIM_DURATION, x, y + ((buttonHeight - comboHeight) / 2), f_Ui_Scale(72, dpi), comboHeight);
		x = x + f_Ui_Scale(72, dpi);
		f_Ui_Move(this, IDC_SIM_DURATION_SPIN, x, y + ((buttonHeight - comboHeight) / 2), f_Ui_Scale(18, dpi), comboHeight);
		x = x + f_Ui_Scale(18, dpi) + (2 * gap);
		f_Ui_Move(this, IDC_LBL_STEP, x, y, f_Ui_Scale(52, dpi), buttonHeight);
		x = x + f_Ui_Scale(52, dpi) + tight;
		f_Ui_Move(this, IDC_SIM_STEP, x, y + ((buttonHeight - comboHeight) / 2), f_Ui_Scale(86, dpi), f_Ui_Scale(320, dpi));
		x = x + f_Ui_Scale(86, dpi) + (3 * gap);
		f_Ui_Move(this, IDC_SCN_NAME, x, y, st_Client.right - margin - f_Ui_Scale(118, dpi) - (2 * gap) - x, buttonHeight);

		// 콤보는 크기가 바뀌면 글자를 모두 선택해 버린다. 입력 중이 아니면 선택을 푼다.
		if ((GetFocus() == nullptr) || (GetFocus()->GetParent() != &st_StepCombo))
		{
			(VOID)st_StepCombo.SetEditSel(-1, 0);
		}
		f_Ui_Move(this, IDC_RUN, st_Client.right - margin - f_Ui_Scale(118, dpi), y, f_Ui_Scale(118, dpi), buttonHeight);

		// 아래 상태 줄
		st_StatusBar.SetRect(0, st_Client.bottom - (textHeight + f_Ui_Scale(14, dpi)), st_Client.right, st_Client.bottom);
		f_Ui_Move(this, IDC_RUN_STATUS, margin, st_StatusBar.top + 1, st_Client.Width() - (2 * margin), st_StatusBar.Height() - 1);

		contentTop		= st_TopBar.bottom + margin;
		contentBottom	= st_StatusBar.top - margin;
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
			// 비율대로 쓴다.
		}

		if (leftWidth > (st_Client.Width() - (3 * margin) - f_Ui_Scale(UI_RIGHT_MIN, dpi)))
		{
			leftWidth = st_Client.Width() - (3 * margin) - f_Ui_Scale(UI_RIGHT_MIN, dpi);
		}

		rightLeft	= margin + leftWidth + gap;
		rightWidth	= st_Client.right - margin - rightLeft;

		// 객체 카드: 표적 수만큼만 높이를 쓰고 나머지는 기동 카드에 준다.
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
		x = st_CardObj.right - pad;
		y = st_CardObj.top + pad;
		x = x - f_Ui_Scale(64, dpi);
		f_Ui_Move(this, IDC_OBJ_DELETE, x, y, f_Ui_Scale(64, dpi), buttonHeight);
		x = x - tight - f_Ui_Scale(64, dpi);
		f_Ui_Move(this, IDC_OBJ_COPY, x, y, f_Ui_Scale(64, dpi), buttonHeight);
		x = x - tight - f_Ui_Scale(84, dpi);
		f_Ui_Move(this, IDC_OBJ_ADD, x, y, f_Ui_Scale(84, dpi), buttonHeight);
		f_Ui_Move(this, IDC_OBJ_TITLE, st_CardObj.left + pad, y, x - tight - (st_CardObj.left + pad), buttonHeight);
		f_Ui_Move(this, IDC_OBJ_GRID, st_CardObj.left + pad, y + buttonHeight + tight, st_CardObj.Width() - (2 * pad), gridHeight);

		// 기동 카드
		st_CardMnv.SetRect(margin, st_CardObj.bottom + gap, margin + leftWidth, contentBottom);
		x = st_CardMnv.right - pad;
		y = st_CardMnv.top + pad;
		x = x - f_Ui_Scale(64, dpi);
		f_Ui_Move(this, IDC_MNV_DELETE, x, y, f_Ui_Scale(64, dpi), buttonHeight);
		x = x - tight - f_Ui_Scale(84, dpi);
		f_Ui_Move(this, IDC_MNV_ADD, x, y, f_Ui_Scale(84, dpi), buttonHeight);
		f_Ui_Move(this, IDC_MNV_TITLE, st_CardMnv.left + pad, y, x - tight - (st_CardMnv.left + pad), buttonHeight);
		gridHeight = st_CardMnv.bottom - pad - (2 * hintHeight) - tight - (y + buttonHeight + tight);
		f_Ui_Move(this, IDC_MNV_GRID, st_CardMnv.left + pad, y + buttonHeight + tight, st_CardMnv.Width() - (2 * pad), gridHeight);
		f_Ui_Move(this, IDC_MNV_HINT, st_CardMnv.left + pad, st_CardMnv.bottom - pad - (2 * hintHeight), st_CardMnv.Width() - (2 * pad), hintHeight);
		f_Ui_Move(this, IDC_EDIT_HINT, st_CardMnv.left + pad, st_CardMnv.bottom - pad - hintHeight, st_CardMnv.Width() - (2 * pad), hintHeight);

		// 그림 카드: 그림 아래에 재생 줄을 둔다.
		st_CardPlot.SetRect(rightLeft, contentTop, rightLeft + rightWidth, contentTop + static_cast<INT32>(static_cast<FLOAT64>(contentBottom - contentTop) * UI_PLOT_RATIO));
		y = st_CardPlot.bottom - pad - buttonHeight;
		f_Ui_Move(this, IDC_PLOT, st_CardPlot.left + pad, st_CardPlot.top + pad, st_CardPlot.Width() - (2 * pad), y - tight - (st_CardPlot.top + pad));
		x = st_CardPlot.left + pad;
		f_Ui_Move(this, IDC_PLAY, x, y, f_Ui_Scale(80, dpi), buttonHeight);
		x = x + f_Ui_Scale(80, dpi) + tight;
		f_Ui_Move(this, IDC_PLAY_SPEED, x, y + ((buttonHeight - comboHeight) / 2), f_Ui_Scale(62, dpi), f_Ui_Scale(240, dpi));
		x = x + f_Ui_Scale(62, dpi) + tight;
		f_Ui_Move(this, IDC_TIME_TEXT, st_CardPlot.right - pad - f_Ui_Scale(146, dpi), y, f_Ui_Scale(146, dpi), buttonHeight);
		f_Ui_Move(this, IDC_TIME_SLIDER, x, y, st_CardPlot.right - pad - f_Ui_Scale(146, dpi) - tight - x, buttonHeight);

		// 결과 카드
		st_CardResult.SetRect(rightLeft, st_CardPlot.bottom + gap, rightLeft + rightWidth, contentBottom);
		y = st_CardResult.top + pad;
		f_Ui_Move(this, IDC_SAVE_CSV, st_CardResult.right - pad - f_Ui_Scale(150, dpi), y, f_Ui_Scale(150, dpi), buttonHeight);
		f_Ui_Move(this, IDC_RESULT_TITLE, st_CardResult.left + pad, y, st_CardResult.Width() - (2 * pad) - f_Ui_Scale(150, dpi) - tight, buttonHeight);
		f_Ui_Move(this, IDC_RESULT_LIST, st_CardResult.left + pad, y + buttonHeight + tight, st_CardResult.Width() - (2 * pad),
			st_CardResult.bottom - pad - (y + buttonHeight + tight));

		Invalidate(TRUE);
	}
}

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

BOOL CTargetSimUIDlg::OnEraseBkgnd(CDC *st_Dc)
{
	static const INT32	s_FramedCtrl[4] = { IDC_OBJ_GRID, IDC_MNV_GRID, IDC_RESULT_LIST, IDC_PLOT };
	CBrush				st_Border(UI_COLOR_BORDER);
	CRect				st_Client;
	CRect				st_Frame;
	CWnd				*st_Ctrl;
	INT32				nIndex;

	GetClientRect(&st_Client);
	st_Dc->FillSolidRect(&st_Client, UI_COLOR_PAGE);

	// 위 막대와 상태 줄은 흰 띠에 가는 경계선
	st_Dc->FillSolidRect(&st_TopBar, UI_COLOR_CARD);
	st_Dc->FillSolidRect(st_TopBar.left, st_TopBar.bottom - 1, st_TopBar.Width(), 1, UI_COLOR_BORDER);
	st_Dc->FillSolidRect(&st_StatusBar, UI_COLOR_CARD);
	st_Dc->FillSolidRect(st_StatusBar.left, st_StatusBar.top, st_StatusBar.Width(), 1, UI_COLOR_BORDER);

	f_DrawCard(st_Dc, st_CardObj);
	f_DrawCard(st_Dc, st_CardMnv);
	f_DrawCard(st_Dc, st_CardPlot);
	f_DrawCard(st_Dc, st_CardResult);

	// 표와 그림에는 창 테두리 대신 옅은 선을 두른다.
	for (nIndex = 0; nIndex < 4; nIndex++)
	{
		st_Ctrl = GetDlgItem(s_FramedCtrl[nIndex]);

		if (st_Ctrl != nullptr)
		{
			st_Ctrl->GetWindowRect(&st_Frame);
			ScreenToClient(&st_Frame);
			st_Frame.InflateRect(1, 1);
			st_Dc->FrameRect(&st_Frame, &st_Border);
		}
	}

	return TRUE;
}

HBRUSH CTargetSimUIDlg::OnCtlColor(CDC *st_Dc, CWnd *st_Wnd, UINT32 ctlColor)
{
	HBRUSH		brush = CDialogEx::OnCtlColor(st_Dc, st_Wnd, ctlColor);
	COLORREF	textColor = UI_COLOR_TEXT;
	INT32		ctrlId;

	if (ctlColor == CTLCOLOR_DLG)
	{
		brush = static_cast<HBRUSH>(st_PageBrush.GetSafeHandle());
	}
	else if ((ctlColor == CTLCOLOR_STATIC) || (ctlColor == CTLCOLOR_BTN))
	{
		// 글자와 슬라이더는 모두 흰 카드 위에 놓인다.
		ctrlId = (st_Wnd != nullptr) ? st_Wnd->GetDlgCtrlID() : 0;

		if ((ctrlId == IDC_MNV_HINT) || (ctrlId == IDC_EDIT_HINT) || (ctrlId == IDC_LBL_DURATION) || (ctrlId == IDC_LBL_STEP) || (ctrlId == IDC_SCN_NAME))
		{
			textColor = UI_COLOR_TEXT_SUB;
		}
		else if (ctrlId == IDC_MNV_TITLE)
		{
			textColor = (nCurObject >= 1) ? f_Ui_ObjectColor(nCurObject) : UI_COLOR_TEXT_SUB;
		}
		else if (ctrlId == IDC_RUN_STATUS)
		{
			textColor = (statusKind == UI_STATUS_ERROR) ? UI_COLOR_ERROR :
				((statusKind == UI_STATUS_OK) ? UI_COLOR_OK : ((statusKind == UI_STATUS_WARN) ? RGB(180, 83, 9) : UI_COLOR_TEXT_SUB));
		}
		else
		{
			textColor = UI_COLOR_TEXT;
		}

		(VOID)st_Dc->SetTextColor(textColor);
		(VOID)st_Dc->SetBkColor(UI_COLOR_CARD);
		brush = static_cast<HBRUSH>(st_CardBrush.GetSafeHandle());
	}
	else
	{
		// 편집칸과 목록은 기본 색을 쓴다.
	}

	return brush;
}

VOID CTargetSimUIDlg::OnSysCommand(UINT32 commandId, LPARAM lParam)
{
	if ((commandId & 0xFFF0U) == IDM_ABOUTBOX)
	{
		CAboutDlg st_AboutDlg;

		f_PrepareModal();
		(VOID)st_AboutDlg.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(commandId, lParam);
	}
}

VOID CTargetSimUIDlg::OnPaint(VOID)
{
	if (IsIconic() != FALSE)
	{
		CPaintDC	st_Dc(this);
		CRect		st_Client;

		(VOID)SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(st_Dc.GetSafeHdc()), 0);

		const INT32 iconWidth = GetSystemMetrics(SM_CXICON);
		const INT32 iconHeight = GetSystemMetrics(SM_CYICON);

		GetClientRect(&st_Client);
		(VOID)st_Dc.DrawIcon((st_Client.Width() - iconWidth + 1) / 2, (st_Client.Height() - iconHeight + 1) / 2, iconHandle);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

HCURSOR CTargetSimUIDlg::OnQueryDragIcon(VOID)
{
	return static_cast<HCURSOR>(iconHandle);
}

VOID CTargetSimUIDlg::OnOK(VOID)
{
	// Enter 로 대화상자가 닫히지 않게 비워 둔다.
}

VOID CTargetSimUIDlg::OnCancel(VOID)
{
	// Esc 로 닫히지 않게 비워 둔다. 닫기는 OnClose 에서만 한다.
}

VOID CTargetSimUIDlg::OnClose(VOID)
{
	// 창이 사라지면 열려 있던 칸이 포커스를 잃으며 값을 반영하려 든다. 닫는 중에는 그 변경으로 다시 실행하지 않는다.
	isClosing = 1;
	f_StopPlay();
	(VOID)KillTimer(UI_RUN_TIMER_ID);
	st_ObjGrid.f_EndEdit(0);
	st_MnvGrid.f_EndEdit(0);
	st_Plot.f_SetResult(nullptr);
	EndDialog(IDCANCEL);
}

VOID CTargetSimUIDlg::f_PrepareModal(VOID)
{
	// 메뉴·파일 대화상자 같은 모달 창을 띄우기 전에 부른다. 치던 값을 반영하고, 미뤄 둔 자동 실행이 있으면
	// 지금 돌려서 모달 창 뒤에서 타이머가 실행을 일으키지 않게 한다.
	f_StopPlay();
	st_ObjGrid.f_EndEdit(1);
	st_MnvGrid.f_EndEdit(1);
	f_CommitSimFields();

	if (isRunPending != 0)
	{
		f_RunScenario(0);
	}
}

BOOL CTargetSimUIDlg::PreTranslateMessage(MSG *st_Msg)
{
	const INT32	isCtrl = (::GetKeyState(VK_CONTROL) < 0) ? 1 : 0;
	CWnd		*st_Focus;
	BOOL		isHandled = FALSE;

	if (st_Msg->message == WM_KEYDOWN)
	{
		if (st_Msg->wParam == VK_F5)
		{
			f_OnRunClicked();
			isHandled = TRUE;
		}
		else if ((isCtrl != 0) && (st_Msg->wParam == static_cast<WPARAM>('S')))
		{
			f_OnSaveCsvClicked();
			isHandled = TRUE;
		}
		else if ((isCtrl != 0) && (st_Msg->wParam == static_cast<WPARAM>('O')))
		{
			f_OpenScenario();
			isHandled = TRUE;
		}
		else if (st_Msg->wParam == VK_RETURN)
		{
			// 시간·간격 칸의 Enter 는 값을 반영한다.
			st_Focus = GetFocus();

			if ((st_Focus != nullptr) && ((st_Focus->GetDlgCtrlID() == IDC_SIM_DURATION) || (st_Focus->GetParent() == &st_StepCombo)) &&
				(st_StepCombo.GetDroppedState() == FALSE))
			{
				f_CommitSimFields();
				isHandled = TRUE;
			}
		}
		else
		{
			isHandled = FALSE;
		}
	}

	if (isHandled == FALSE)
	{
		isHandled = CDialogEx::PreTranslateMessage(st_Msg);
	}

	return isHandled;
}

// ---------------------------------------------------------------------------
// 편집

ST_ObjectText *CTargetSimUIDlg::f_GetObject(INT32 nObject)
{
	ST_ObjectText *st_Object = nullptr;

	if (nObject == 0)
	{
		st_Object = &st_Scenario.st_Platform;
	}
	else if ((nObject >= 1) && (nObject <= st_Scenario.nTargetNum))
	{
		st_Object = &st_Scenario.st_Target[nObject - 1];
	}
	else
	{
		st_Object = nullptr;
	}

	return st_Object;
}

VOID CTargetSimUIDlg::f_ShowScenarioName(VOID)
{
	SetDlgItemText(IDC_SCN_NAME, (isModified != 0) ? (st_ScenarioName + _T("  ·  수정됨")) : st_ScenarioName);
}

VOID CTargetSimUIDlg::f_RefreshAll(VOID)
{
	f_StopPlay();
	isModified = 0;
	f_ClearIssueMarks();
	f_ShowScenarioName();
	SetDlgItemText(IDC_SIM_DURATION, st_Scenario.st_DurationText);
	st_StepCombo.SetWindowText(st_Scenario.st_StepText);
	(VOID)st_StepCombo.SetEditSel(-1, 0);

	nCurObject = (st_Scenario.nTargetNum >= 1) ? 1 : 0;

	f_UpdateTimeNudge();
	f_RefreshObjGrid();
	f_Layout();
	st_ObjGrid.f_SetCurCell(nCurObject, UI_OBJ_COL_FIELD, 0);
	st_Plot.f_SetSelObject(nCurObject);
	f_RefreshMnvGrid(1);
	f_RunScenario(0);
}

VOID CTargetSimUIDlg::f_RefreshObjGrid(VOID)
{
	const INT32			nRowNum = st_Scenario.nTargetNum + 1;
	const ST_ObjectText	*st_Object;
	CString				st_Name;
	INT32				nObject;
	INT32				nField;

	st_ObjGrid.f_SetRowNum(nRowNum);

	for (nObject = 0; nObject < nRowNum; nObject++)
	{
		st_Object = f_GetObject(nObject);

		if (nObject == 0)
		{
			st_Name = _T("플랫폼");
		}
		else
		{
			st_Name.Format(_T("표적 %d"), nObject);
		}

		st_ObjGrid.f_SetRowColor(nObject, f_Ui_ObjectColor(nObject));
		st_ObjGrid.f_SetCellText(nObject, UI_OBJ_COL_NAME, st_Name);

		for (nField = 0; (nField < SCN_OBJ_FIELD_NUM) && (st_Object != nullptr); nField++)
		{
			st_ObjGrid.f_SetCellText(nObject, UI_OBJ_COL_FIELD + nField, st_Object->st_FieldText[nField]);
		}
	}

	st_ObjGrid.Invalidate(FALSE);
	f_UpdateTitles();
}

VOID CTargetSimUIDlg::f_RefreshMnvGrid(INT32 isNewTarget)
{
	const ST_ObjectText	*st_Object = (nCurObject >= 1) ? f_GetObject(nCurObject) : nullptr;
	CString				st_Index;
	INT32				nManeuver;
	INT32				nField;

	if (st_Object == nullptr)
	{
		st_MnvGrid.f_SetRowNum(0);
		st_MnvGrid.f_SetEmptyText(CString(_T("플랫폼은 기동하지 않습니다 — 위 표에서 표적을 고르십시오")));
	}
	else
	{
		st_MnvGrid.f_SetRowNum(st_Object->nManeuverNum);
		st_MnvGrid.f_SetEmptyText(CString(_T("기동 없음 (등속 직진) — [＋ 기동] 으로 추가")));

		for (nManeuver = 0; nManeuver < st_Object->nManeuverNum; nManeuver++)
		{
			st_Index.Format(_T("%d"), nManeuver + 1);
			st_MnvGrid.f_SetRowColor(nManeuver, f_Ui_TurnColor(st_Object->st_Maneuver[nManeuver].enTurnType));
			st_MnvGrid.f_SetCellText(nManeuver, UI_MNV_COL_INDEX, st_Index);
			st_MnvGrid.f_SetCellText(nManeuver, UI_MNV_COL_TYPE, CString(CScenario::f_TurnName(static_cast<INT32>(st_Object->st_Maneuver[nManeuver].enTurnType))));

			for (nField = 0; nField < SCN_MNV_FIELD_NUM; nField++)
			{
				st_MnvGrid.f_SetCellText(nManeuver, UI_MNV_COL_FIELD + nField, st_Object->st_Maneuver[nManeuver].st_FieldText[nField]);
			}

			f_UpdateMnvComputed(nManeuver);
		}

		// 표적이 바뀌었으면 앞 표적에서 고른 행 번호가 남지 않게 첫 기동으로 옮긴다.
		if ((isNewTarget != 0) && (st_Object->nManeuverNum > 0))
		{
			st_MnvGrid.f_SetCurCell(0, UI_MNV_COL_FIELD + SCN_FIELD_GRAVITY, 0);
		}
	}

	// 오류 표식은 행 번호로 달려 있으므로 지금 보이는 표적 기준으로 다시 단다.
	f_MarkIssue();
	st_MnvGrid.Invalidate(FALSE);
	f_UpdateTitles();
}

VOID CTargetSimUIDlg::f_UpdateMnvComputed(INT32 nManeuver)
{
	const ST_ObjectText	*st_Object = (nCurObject >= 1) ? f_GetObject(nCurObject) : nullptr;
	CString				st_Angle = _T("—");
	CString				st_Radius = _T("—");
	FLOAT64				speed = 0.0;
	FLOAT64				gravity = 0.0;
	FLOAT64				startTime = 0.0;
	FLOAT64				endTime = 0.0;

	// 기동이 무엇을 하는지 바로 보이도록 회전각과 선회 반경을 옆에 적는다. 값을 읽을 수 없으면 비워 둔다.
	if ((st_Object != nullptr) && (nManeuver >= 0) && (nManeuver < st_Object->nManeuverNum))
	{
		const ST_ManeuverText *st_Maneuver = &st_Object->st_Maneuver[nManeuver];

		if ((st_Maneuver->enTurnType != TGT_TURN_NONE) &&
			(f_Num_Parse(st_Object->st_FieldText[SCN_FIELD_SPEED], &speed) == NUM_OK) && (speed >= SCN_TGT_SPEED_MIN) &&
			(f_Num_Parse(st_Maneuver->st_FieldText[SCN_FIELD_GRAVITY], &gravity) == NUM_OK) &&
			(f_Num_Parse(st_Maneuver->st_FieldText[SCN_FIELD_START], &startTime) == NUM_OK) &&
			(f_Num_Parse(st_Maneuver->st_FieldText[SCN_FIELD_END], &endTime) == NUM_OK) && (endTime > startTime))
		{
			st_Angle = f_Num_ToText(f_Rad_To_Deg(((gravity * G_FORCE) / speed) * (endTime - startTime)), 1);

			// Roll 은 궤적을 휘지 않으므로 반경이 없다.
			if ((st_Maneuver->enTurnType != TGT_TURN_ROLL) && (fabs(gravity) > 1.0e-9))
			{
				st_Radius = f_Num_ToText((speed * speed) / (fabs(gravity) * G_FORCE), 0);
			}
		}

		st_MnvGrid.f_SetCellText(nManeuver, UI_MNV_COL_ANGLE, st_Angle);
		st_MnvGrid.f_SetCellText(nManeuver, UI_MNV_COL_RADIUS, st_Radius);
	}
}

VOID CTargetSimUIDlg::f_UpdateTitles(VOID)
{
	const ST_ObjectText	*st_Object = (nCurObject >= 1) ? f_GetObject(nCurObject) : nullptr;
	CString				st_Text;

	st_Text.Format(_T("플랫폼 · 표적   %d / %d"), st_Scenario.nTargetNum, TGT_MAX_TARGET_NUM);
	SetDlgItemText(IDC_OBJ_TITLE, st_Text);

	if (st_Object != nullptr)
	{
		st_Text.Format(_T("기동 — 표적 %d   %d / %d"), nCurObject, st_Object->nManeuverNum, TGT_MAX_MANEUVER_NUM);
	}
	else
	{
		st_Text = _T("기동");
	}

	SetDlgItemText(IDC_MNV_TITLE, st_Text);

	if (st_Result.f_GetSampleNum() > 0)
	{
		st_Text.Format(_T("결과 (LLA)   %d 행 × %d 열"), st_Result.f_GetSampleNum(), st_Result.f_GetColumnNum());
	}
	else
	{
		st_Text = _T("결과 (LLA)");
	}

	SetDlgItemText(IDC_RESULT_TITLE, st_Text);

	f_Ui_EnableCtrl(this, IDC_OBJ_ADD, (st_Scenario.nTargetNum < TGT_MAX_TARGET_NUM) ? 1 : 0);
	f_Ui_EnableCtrl(this, IDC_OBJ_COPY, ((st_Scenario.nTargetNum < TGT_MAX_TARGET_NUM) && (nCurObject >= 1)) ? 1 : 0);
	f_Ui_EnableCtrl(this, IDC_OBJ_DELETE, ((st_Scenario.nTargetNum > 1) && (nCurObject >= 1)) ? 1 : 0);
	f_Ui_EnableCtrl(this, IDC_MNV_ADD, ((st_Object != nullptr) && (st_Object->nManeuverNum < TGT_MAX_MANEUVER_NUM)) ? 1 : 0);
	f_Ui_EnableCtrl(this, IDC_MNV_DELETE, ((st_Object != nullptr) && (st_Object->nManeuverNum > 0)) ? 1 : 0);
}

VOID CTargetSimUIDlg::f_UpdateTimeNudge(VOID)
{
	FLOAT64	step = 0.0;
	FLOAT64	duration = 0.0;
	INT32	nDecimal;

	// 기동 시각은 간격의 정수배여야 하므로 증감 폭을 간격에 맞춘다.
	if ((f_Num_Parse(st_Scenario.st_StepText, &step) != NUM_OK) || (step < SCN_TIME_RES))
	{
		step = 0.1;
	}

	if ((f_Num_Parse(st_Scenario.st_DurationText, &duration) != NUM_OK) || (duration <= 0.0))
	{
		duration = SCN_DURATION_MAX;
	}

	nDecimal = f_Num_CountDecimal(st_Scenario.st_StepText);
	st_MnvGrid.f_SetColumnNudge(UI_MNV_COL_FIELD + SCN_FIELD_START, step, nDecimal, 0.0, duration);
	st_MnvGrid.f_SetColumnNudge(UI_MNV_COL_FIELD + SCN_FIELD_END, step, nDecimal, 0.0, duration);
}

VOID CTargetSimUIDlg::f_CommitSimFields(VOID)
{
	CString st_Duration;
	CString st_Step;

	(VOID)GetDlgItemText(IDC_SIM_DURATION, st_Duration);
	st_StepCombo.GetWindowText(st_Step);
	(VOID)st_Duration.Trim();
	(VOID)st_Step.Trim();

	if ((st_Duration != st_Scenario.st_DurationText) || (st_Step != st_Scenario.st_StepText))
	{
		st_Scenario.st_DurationText	= st_Duration;
		st_Scenario.st_StepText		= st_Step;
		f_UpdateTimeNudge();
		st_ObjGrid.Invalidate(FALSE);
		f_ScheduleRun();
	}
}

VOID CTargetSimUIDlg::f_OnSimFieldKillFocus(VOID)
{
	f_CommitSimFields();
}

VOID CTargetSimUIDlg::f_OnStepSelChange(VOID)
{
	const INT32	nSelected = st_StepCombo.GetCurSel();
	CString		st_Step;

	// 이 알림이 올 때는 편집칸 글자가 아직 바뀌기 전이라 목록에서 직접 읽는다.
	if (nSelected >= 0)
	{
		st_StepCombo.GetLBText(nSelected, st_Step);
		st_StepCombo.SetWindowText(st_Step);
		f_CommitSimFields();
	}
}

VOID CTargetSimUIDlg::f_OnDurationSpin(NMHDR *st_Hdr, LRESULT *pt_Result)
{
	const NMUPDOWN	*st_UpDown = reinterpret_cast<NMUPDOWN *>(st_Hdr);
	CString			st_Text;
	CString			st_NewText;
	FLOAT64			delta;
	FLOAT64			minDuration = SCN_TIME_RES;

	if (st_UpDown->iDelta != 0)
	{
		delta = (st_UpDown->iDelta > 0) ? 1.0 : -1.0;

		// 시간은 간격보다 짧을 수 없다. 거기까지만 내려간다.
		if ((f_Num_Parse(st_Scenario.st_StepText, &minDuration) != NUM_OK) || (minDuration < SCN_TIME_RES))
		{
			minDuration = SCN_TIME_RES;
		}

		if (::GetKeyState(VK_CONTROL) < 0)
		{
			delta = delta * 10.0;
		}

		(VOID)GetDlgItemText(IDC_SIM_DURATION, st_Text);

		if (f_Num_Nudge(st_Text, delta, 0, minDuration, SCN_DURATION_MAX, &st_NewText) != 0)
		{
			SetDlgItemText(IDC_SIM_DURATION, st_NewText);
			f_CommitSimFields();
		}
	}

	*pt_Result = 1;
}

VOID CTargetSimUIDlg::f_SelectObject(INT32 nObject)
{
	if ((nObject >= 0) && (nObject <= st_Scenario.nTargetNum))
	{
		INT32 nColumn = st_ObjGrid.f_GetCurColumn();

		if ((nColumn < UI_OBJ_COL_FIELD) || (nColumn >= UI_OBJ_COL_TIMELINE))
		{
			nColumn = UI_OBJ_COL_FIELD;
		}

		// 행이 바뀌면 f_OnObjRowChanged 가 기동 표와 그림 선택을 따라 바꾼다.
		st_ObjGrid.f_SetCurCell(nObject, nColumn, 0);
	}
}

VOID CTargetSimUIDlg::f_OnObjRowChanged(NMHDR *st_Hdr, LRESULT *pt_Result)
{
	const ST_GridNotify *st_Notify = reinterpret_cast<ST_GridNotify *>(st_Hdr);

	if ((st_Notify->nRow >= 0) && (st_Notify->nRow != nCurObject))
	{
		nCurObject = st_Notify->nRow;
		st_Plot.f_SetSelObject(nCurObject);
		f_RefreshMnvGrid(1);

		CWnd *st_Title = GetDlgItem(IDC_MNV_TITLE);

		if (st_Title != nullptr)
		{
			st_Title->Invalidate();
		}
	}

	*pt_Result = 0;
}

VOID CTargetSimUIDlg::f_OnObjCellChanged(NMHDR *st_Hdr, LRESULT *pt_Result)
{
	const ST_GridNotify	*st_Notify = reinterpret_cast<ST_GridNotify *>(st_Hdr);
	const INT32			nField = st_Notify->nColumn - UI_OBJ_COL_FIELD;
	ST_ObjectText		*st_Object = f_GetObject(st_Notify->nRow);
	INT32				nManeuver;

	if ((st_Object != nullptr) && (nField >= 0) && (nField < SCN_OBJ_FIELD_NUM))
	{
		st_Object->st_FieldText[nField] = st_ObjGrid.f_GetCellText(st_Notify->nRow, st_Notify->nColumn);

		// 속력이 바뀌면 같은 G 의 회전각과 반경이 달라진다.
		if ((nField == SCN_FIELD_SPEED) && (st_Notify->nRow == nCurObject))
		{
			for (nManeuver = 0; nManeuver < st_Object->nManeuverNum; nManeuver++)
			{
				f_UpdateMnvComputed(nManeuver);
			}
		}

		f_ScheduleRun();
	}

	*pt_Result = 0;
}

VOID CTargetSimUIDlg::f_OnMnvCellChanged(NMHDR *st_Hdr, LRESULT *pt_Result)
{
	const ST_GridNotify	*st_Notify = reinterpret_cast<ST_GridNotify *>(st_Hdr);
	const INT32			nField = st_Notify->nColumn - UI_MNV_COL_FIELD;
	ST_ObjectText		*st_Object = (nCurObject >= 1) ? f_GetObject(nCurObject) : nullptr;
	CString				st_Text;
	INT32				nType;

	if ((st_Object != nullptr) && (st_Notify->nRow >= 0) && (st_Notify->nRow < st_Object->nManeuverNum))
	{
		ST_ManeuverText *st_Maneuver = &st_Object->st_Maneuver[st_Notify->nRow];

		st_Text = st_MnvGrid.f_GetCellText(st_Notify->nRow, st_Notify->nColumn);

		if (st_Notify->nColumn == UI_MNV_COL_TYPE)
		{
			for (nType = 0; nType < SCN_TURN_TYPE_NUM; nType++)
			{
				if (st_Text == CScenario::f_TurnName(nType))
				{
					st_Maneuver->enTurnType = static_cast<EN_TurnType>(nType);
				}
			}

			st_MnvGrid.f_SetRowColor(st_Notify->nRow, f_Ui_TurnColor(st_Maneuver->enTurnType));
			st_MnvGrid.Invalidate(FALSE);
		}
		else if ((nField >= 0) && (nField < SCN_MNV_FIELD_NUM))
		{
			st_Maneuver->st_FieldText[nField] = st_Text;
		}
		else
		{
			// 계산해서 보여 주는 칸은 편집되지 않는다.
		}

		f_UpdateMnvComputed(st_Notify->nRow);
		st_ObjGrid.Invalidate(FALSE);
		f_ScheduleRun();
	}

	*pt_Result = 0;
}

VOID CTargetSimUIDlg::f_OnObjDrawCell(NMHDR *st_Hdr, LRESULT *pt_Result)
{
	const ST_GridNotify	*st_Notify = reinterpret_cast<ST_GridNotify *>(st_Hdr);
	const ST_ObjectText	*st_Object = (st_Notify->nRow >= 1) ? f_GetObject(st_Notify->nRow) : nullptr;
	CDC					*st_Dc = CDC::FromHandle(st_Notify->dcHandle);
	CRect				st_Cell(st_Notify->st_Rect);
	CRect				st_Track;
	CRect				st_Block;
	CString				st_Label;
	FLOAT64				duration = 0.0;
	FLOAT64				startTime = 0.0;
	FLOAT64				endTime = 0.0;
	INT32				barHeight = f_Ui_Scale(UI_BAR_HEIGHT, dpi);
	INT32				nManeuver;

	// 기동 타임라인: 0 ~ 시뮬레이션 시간을 칸 너비에 펴고, 기동 구간을 축 색 막대로 그린다.
	if ((st_Dc != nullptr) && (st_Cell.Width() > 8))
	{
		if (barHeight > st_Cell.Height())
		{
			barHeight = st_Cell.Height();
		}

		st_Track.SetRect(st_Cell.left, st_Cell.top + ((st_Cell.Height() - barHeight) / 2), st_Cell.right, st_Cell.top + ((st_Cell.Height() - barHeight) / 2) + barHeight);

		if (st_Object == nullptr)
		{
			(VOID)st_Dc->SetTextColor(UI_COLOR_TEXT_OFF);
			(VOID)st_Dc->DrawText(CString(_T("—")), &st_Cell, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		}
		else
		{
			st_Dc->FillSolidRect(&st_Track, UI_COLOR_TRACK);

			if ((f_Num_Parse(st_Scenario.st_DurationText, &duration) == NUM_OK) && (duration > 0.0))
			{
				for (nManeuver = 0; nManeuver < st_Object->nManeuverNum; nManeuver++)
				{
					const ST_ManeuverText *st_Maneuver = &st_Object->st_Maneuver[nManeuver];

					if ((f_Num_Parse(st_Maneuver->st_FieldText[SCN_FIELD_START], &startTime) == NUM_OK) &&
						(f_Num_Parse(st_Maneuver->st_FieldText[SCN_FIELD_END], &endTime) == NUM_OK) &&
						(startTime >= 0.0) && (endTime > startTime) && (startTime < duration))
					{
						if (endTime > duration)
						{
							endTime = duration;
						}

						st_Block.SetRect(st_Track.left + static_cast<INT32>((startTime / duration) * static_cast<FLOAT64>(st_Track.Width())), st_Track.top,
							st_Track.left + static_cast<INT32>((endTime / duration) * static_cast<FLOAT64>(st_Track.Width())), st_Track.bottom);

						if (st_Block.Width() < 2)
						{
							st_Block.right = st_Block.left + 2;
						}

						st_Dc->FillSolidRect(&st_Block, f_Ui_TurnColor(st_Maneuver->enTurnType));

						// 막대가 넉넉하면 축 머리글자와 G 를 적는다.
						st_Label = CString(CScenario::f_TurnName(static_cast<INT32>(st_Maneuver->enTurnType))).Mid(2, 1) + _T(" ") + st_Maneuver->st_FieldText[SCN_FIELD_GRAVITY];

						if ((st_Maneuver->enTurnType != TGT_TURN_NONE) && (st_Dc->GetTextExtent(st_Label).cx < (st_Block.Width() - 4)))
						{
							(VOID)st_Dc->SetTextColor(RGB(255, 255, 255));
							(VOID)st_Dc->DrawText(st_Label, &st_Block, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
						}
					}
				}

				// 지금 보고 있는 시각
				if ((st_Result.f_GetSampleNum() > 1) && (isResultStale == 0))
				{
					const FLOAT64	nowTime = static_cast<FLOAT64>(nCurStep) * st_Result.f_GetStepTime();
					const INT32		cursorX = st_Track.left + static_cast<INT32>((nowTime / duration) * static_cast<FLOAT64>(st_Track.Width() - 1));

					if ((cursorX >= st_Track.left) && (cursorX < st_Track.right))
					{
						// 색 막대 위에서도 보이게 흰 띠를 깔고 그 위에 선을 긋는다.
						st_Dc->FillSolidRect(cursorX - 1, st_Cell.top + 2, 3, st_Cell.Height() - 4, UI_COLOR_CARD);
						st_Dc->FillSolidRect(cursorX, st_Cell.top + 2, 1, st_Cell.Height() - 4, UI_COLOR_TEXT);
					}
				}
			}
		}
	}

	*pt_Result = 0;
}

VOID CTargetSimUIDlg::f_OnTargetAddClicked(VOID)
{
	const INT32 nNewTarget = st_Scenario.f_AddTarget(nCurObject - 1, 0);

	if (nNewTarget >= 0)
	{
		f_RefreshObjGrid();
		f_Layout();
		f_SelectObject(nNewTarget + 1);
		f_ScheduleRun();
	}
}

VOID CTargetSimUIDlg::f_OnTargetCopyClicked(VOID)
{
	const INT32 nNewTarget = (nCurObject >= 1) ? st_Scenario.f_AddTarget(nCurObject - 1, 1) : -1;

	if (nNewTarget >= 0)
	{
		f_RefreshObjGrid();
		f_Layout();
		f_SelectObject(nNewTarget + 1);
		f_ScheduleRun();
	}
}

VOID CTargetSimUIDlg::f_OnTargetDeleteClicked(VOID)
{
	INT32 nNext = nCurObject;

	if ((nCurObject >= 1) && (st_Scenario.f_DeleteTarget(nCurObject - 1) != 0))
	{
		if (nNext > st_Scenario.nTargetNum)
		{
			nNext = st_Scenario.nTargetNum;
		}

		// 같은 번호의 행이 다른 표적이 되므로 선택이 바뀐 것으로 다룬다. 오류 표식의 행 번호도 더는 맞지 않는다.
		f_ClearIssueMarks();
		nCurObject = -1;
		f_RefreshObjGrid();
		f_Layout();
		f_SelectObject(nNext);
		nCurObject = nNext;
		st_Plot.f_SetSelObject(nCurObject);
		f_RefreshMnvGrid(1);
		f_ScheduleRun();
		GotoDlgCtrl(&st_ObjGrid);
	}
}

VOID CTargetSimUIDlg::f_OnManeuverAddClicked(VOID)
{
	const INT32 nNewManeuver = (nCurObject >= 1) ? st_Scenario.f_AddManeuver(nCurObject - 1) : -1;

	if (nNewManeuver >= 0)
	{
		f_RefreshMnvGrid(0);
		st_MnvGrid.f_SetCurCell(nNewManeuver, UI_MNV_COL_FIELD + SCN_FIELD_GRAVITY, 0);
		st_ObjGrid.Invalidate(FALSE);
		f_ScheduleRun();
		GotoDlgCtrl(&st_MnvGrid);
	}
	else if (nNewManeuver == SCN_ADD_NO_ROOM)
	{
		f_SetStatus(UI_STATUS_WARN, CString(_T("직전 기동이 시뮬레이션 끝까지 차 있어 넣을 자리가 없습니다 — 시간을 늘리거나 앞 기동의 종료를 당기십시오")));
	}
	else
	{
		// 표적을 고르지 않았거나 기동 수가 한도에 찼다. 그때는 버튼이 꺼져 있다.
	}
}

VOID CTargetSimUIDlg::f_OnManeuverDeleteClicked(VOID)
{
	const ST_ObjectText	*st_Object = (nCurObject >= 1) ? f_GetObject(nCurObject) : nullptr;
	INT32				nManeuver = st_MnvGrid.f_GetCurRow();

	if (st_Object != nullptr)
	{
		// 고른 행이 없으면 마지막 기동을 지운다.
		if ((nManeuver < 0) || (nManeuver >= st_Object->nManeuverNum))
		{
			nManeuver = st_Object->nManeuverNum - 1;
		}

		if (st_Scenario.f_DeleteManeuver(nCurObject - 1, nManeuver) != 0)
		{
			// 뒤 기동의 번호가 당겨지므로 오류 표식을 지우고 다시 검사하게 둔다.
			f_ClearIssueMarks();
			f_RefreshMnvGrid(0);

			if (st_Object->nManeuverNum > 0)
			{
				st_MnvGrid.f_SetCurCell((nManeuver < st_Object->nManeuverNum) ? nManeuver : (st_Object->nManeuverNum - 1), UI_MNV_COL_FIELD, 0);
			}

			st_ObjGrid.Invalidate(FALSE);
			f_ScheduleRun();
			GotoDlgCtrl(&st_MnvGrid);
		}
	}
}

VOID CTargetSimUIDlg::f_OnScenarioMenuClicked(VOID)
{
	CMenu	st_Menu;
	CRect	st_Button;
	CWnd	*st_Ctrl = GetDlgItem(IDC_SCN_MENU);
	INT32	nPreset;

	f_PrepareModal();

	if ((st_Ctrl != nullptr) && (st_Menu.CreatePopupMenu() != FALSE))
	{
		for (nPreset = 0; nPreset < SCN_PRESET_NUM; nPreset++)
		{
			(VOID)st_Menu.AppendMenu(MF_STRING, static_cast<UINT_PTR>(ID_SCN_PRESET_FIRST) + static_cast<UINT_PTR>(nPreset), CScenario::f_PresetName(nPreset));
		}

		(VOID)st_Menu.AppendMenu(MF_SEPARATOR);
		(VOID)st_Menu.AppendMenu(MF_STRING, ID_SCN_OPEN, _T("파일에서 열기...\tCtrl+O"));
		(VOID)st_Menu.AppendMenu(MF_STRING, ID_SCN_SAVE, _T("파일로 저장..."));

		// 고른 항목은 WM_COMMAND 로 돌아와 아래 처리기들이 받는다.
		st_Ctrl->GetWindowRect(&st_Button);
		(VOID)st_Menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_TOPALIGN, st_Button.left, st_Button.bottom + 2, this);
	}
}

VOID CTargetSimUIDlg::f_OnScenarioPreset(UINT32 command)
{
	st_ObjGrid.f_EndEdit(1);
	st_MnvGrid.f_EndEdit(1);

	st_Scenario.f_LoadPreset(static_cast<INT32>(command) - ID_SCN_PRESET_FIRST);
	st_ScenarioName = CScenario::f_PresetName(static_cast<INT32>(command) - ID_SCN_PRESET_FIRST);
	f_RefreshAll();
}

VOID CTargetSimUIDlg::f_OpenScenario(VOID)
{
	CFileDialog	st_FileDlg(TRUE, _T("tsim"), nullptr, OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY,
					_T("TargetSim 시나리오 (*.tsim)|*.tsim|모든 파일 (*.*)|*.*||"), this);
	CString		st_Error;

	f_PrepareModal();

	if (st_FileDlg.DoModal() == IDOK)
	{
		if (st_Scenario.f_Load(st_FileDlg.GetPathName(), &st_Error) != 0)
		{
			st_ScenarioName = st_FileDlg.GetFileName();
			f_RefreshAll();
		}
		else
		{
			f_SetStatus(UI_STATUS_ERROR, CString(_T("시나리오를 열지 못했습니다: ")) + st_Error + _T(" (") + st_FileDlg.GetFileName() + _T(")"));
		}
	}
}

VOID CTargetSimUIDlg::f_SaveScenario(VOID)
{
	CFileDialog	st_FileDlg(FALSE, _T("tsim"), _T("scenario.tsim"), OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY,
					_T("TargetSim 시나리오 (*.tsim)|*.tsim||"), this);
	CString		st_Message;
	INT32		errorCode;

	f_PrepareModal();

	if (st_FileDlg.DoModal() == IDOK)
	{
		errorCode = st_Scenario.f_Save(st_FileDlg.GetPathName());

		if (errorCode == 0)
		{
			st_ScenarioName	= st_FileDlg.GetFileName();
			isModified		= 0;
			f_ShowScenarioName();
			f_SetStatus(UI_STATUS_OK, CString(_T("시나리오 저장: ")) + st_FileDlg.GetFileName());
		}
		else
		{
			st_Message.Format(_T("시나리오를 쓰지 못했습니다 (errno %d): %s"), errorCode, st_FileDlg.GetPathName().GetString());
			f_SetStatus(UI_STATUS_ERROR, st_Message);
		}
	}
}

// ---------------------------------------------------------------------------
// 그림에서 끌기

VOID CTargetSimUIDlg::f_OnPlotSelect(NMHDR *st_Hdr, LRESULT *pt_Result)
{
	const ST_PlotNotify *st_Notify = reinterpret_cast<ST_PlotNotify *>(st_Hdr);

	st_ObjGrid.f_EndEdit(1);
	st_MnvGrid.f_EndEdit(1);
	f_SelectObject(st_Notify->nObject);
	*pt_Result = 0;
}

VOID CTargetSimUIDlg::f_OnPlotMove(NMHDR *st_Hdr, LRESULT *pt_Result)
{
	const ST_PlotNotify	*st_Notify = reinterpret_cast<ST_PlotNotify *>(st_Hdr);
	ST_ObjectText		*st_Object = (st_Notify->nObject >= 1) ? f_GetObject(st_Notify->nObject) : nullptr;

	if (st_Object != nullptr)
	{
		// 0.000001 도는 약 0.1 m 다.
		st_Object->st_FieldText[SCN_FIELD_LAT] = f_Num_ToText(st_Notify->latDeg, 6);
		st_Object->st_FieldText[SCN_FIELD_LON] = f_Num_ToText(st_Notify->lonDeg, 6);
		st_ObjGrid.f_SetCellText(st_Notify->nObject, UI_OBJ_COL_FIELD + SCN_FIELD_LAT, st_Object->st_FieldText[SCN_FIELD_LAT]);
		st_ObjGrid.f_SetCellText(st_Notify->nObject, UI_OBJ_COL_FIELD + SCN_FIELD_LON, st_Object->st_FieldText[SCN_FIELD_LON]);
		f_ScheduleRun();
	}

	*pt_Result = 0;
}

VOID CTargetSimUIDlg::f_OnPlotTurn(NMHDR *st_Hdr, LRESULT *pt_Result)
{
	const ST_PlotNotify	*st_Notify = reinterpret_cast<ST_PlotNotify *>(st_Hdr);
	ST_ObjectText		*st_Object = (st_Notify->nObject >= 1) ? f_GetObject(st_Notify->nObject) : nullptr;

	if (st_Object != nullptr)
	{
		st_Object->st_FieldText[SCN_FIELD_YAW] = f_Num_ToText(st_Notify->yawDeg, 0);
		st_ObjGrid.f_SetCellText(st_Notify->nObject, UI_OBJ_COL_FIELD + SCN_FIELD_YAW, st_Object->st_FieldText[SCN_FIELD_YAW]);
		f_ScheduleRun();
	}

	*pt_Result = 0;
}

VOID CTargetSimUIDlg::f_OnPlotScrub(NMHDR *st_Hdr, LRESULT *pt_Result)
{
	const ST_PlotNotify *st_Notify = reinterpret_cast<ST_PlotNotify *>(st_Hdr);

	f_StopPlay();
	f_ShowStep(st_Notify->nStep, UI_SYNC_NONE);
	*pt_Result = 0;
}

// ---------------------------------------------------------------------------
// 실행·출력

VOID CTargetSimUIDlg::f_ScheduleRun(VOID)
{
	if (isClosing != 0)
	{
		// 닫는 중에 칸이 닫히며 들어온 변경은 버린다.
	}
	else
	{
		isResultStale = 1;

		// 편집은 모두 여기를 거친다.
		if (isModified == 0)
		{
			isModified = 1;
			f_ShowScenarioName();
		}

		// 빨리 끝나는 시나리오는 바로 돌려 값과 그림이 함께 움직이게 하고, 오래 걸리면 입력이 멎기를 기다린다.
		if ((st_Result.f_GetSampleNum() == 0) || (st_Result.f_GetRunMs() < UI_FAST_RUN_MS))
		{
			f_RunScenario(0);
		}
		else
		{
			isRunPending = 1;
			(VOID)SetTimer(UI_RUN_TIMER_ID, UI_RUN_DELAY_MS, nullptr);
		}
	}
}

VOID CTargetSimUIDlg::f_OnRunClicked(VOID)
{
	st_ObjGrid.f_EndEdit(1);
	st_MnvGrid.f_EndEdit(1);
	f_CommitSimFields();
	f_RunScenario(1);
}

VOID CTargetSimUIDlg::f_ClearIssueMarks(VOID)
{
	isIssueActive = 0;
	f_MarkIssue();
}

VOID CTargetSimUIDlg::f_MarkIssue(VOID)
{
	INT32 nObjRow = -1;
	INT32 nObjColumn = -1;
	INT32 nMnvRow = -1;
	INT32 nMnvColumn = -1;

	if (isIssueActive == 0)
	{
		// 표시할 오류가 없다. 아래에서 두 표의 표식을 모두 지운다.
	}
	else if ((st_LastIssue.enPlace == SCN_AT_PLATFORM) || (st_LastIssue.enPlace == SCN_AT_TARGET))
	{
		nObjRow		= (st_LastIssue.enPlace == SCN_AT_PLATFORM) ? 0 : (st_LastIssue.nTarget + 1);
		nObjColumn	= UI_OBJ_COL_FIELD + ((st_LastIssue.nField >= 0) ? st_LastIssue.nField : 0);
	}
	else if (st_LastIssue.enPlace != SCN_AT_MANEUVER)
	{
		// 시간·간격 오류는 상태 줄로만 알린다.
	}
	else if (nCurObject == (st_LastIssue.nTarget + 1))
	{
		nMnvRow		= st_LastIssue.nManeuver;
		nMnvColumn	= UI_MNV_COL_FIELD + ((st_LastIssue.nField >= 0) ? st_LastIssue.nField : SCN_FIELD_START);
	}
	else
	{
		// 다른 표적의 기동이면 객체 표의 기동 구간 칸에 표시해 어느 표적인지 알린다.
		nObjRow		= st_LastIssue.nTarget + 1;
		nObjColumn	= UI_OBJ_COL_TIMELINE;
	}

	st_ObjGrid.f_SetErrorCell(nObjRow, nObjColumn);
	st_MnvGrid.f_SetErrorCell(nMnvRow, nMnvColumn);
}

VOID CTargetSimUIDlg::f_RunScenario(INT32 isExplicit)
{
	isRunPending = 0;
	(VOID)KillTimer(UI_RUN_TIMER_ID);

	// 실행 도중의 포커스 이동이 다시 실행을 부르거나, 닫는 중에 실행이 도는 일을 막는다.
	if ((isRunning == 0) && (isClosing == 0))
	{
		isRunning = 1;
		f_RunScenarioOnce(isExplicit);
		isRunning = 0;
	}
}

VOID CTargetSimUIDlg::f_RunScenarioOnce(INT32 isExplicit)
{
	ST_ScnIssue	st_Issue;
	CString		st_Message;
	CString		st_Error;
	FLOAT64		stepRatio;
	INT32		nOldSampleNum = st_Result.f_GetSampleNum();

	// 성공해서 결과를 바꿀 때까지는 화면의 결과가 지금 입력과 다를 수 있다 (검증 실패, 자동 실행 한도 포함).
	isResultStale = 1;
	f_ClearIssueMarks();

	if (st_Scenario.f_BuildConfig(&st_RunConfig, &st_Issue) == 0)
	{
		f_ShowIssue(&st_Issue, isExplicit);
	}
	else
	{
		stepRatio = st_RunConfig.durationTime / st_RunConfig.stepTime;

		if ((isExplicit == 0) && (stepRatio > static_cast<FLOAT64>(UI_AUTO_RUN_MAX_STEP)))
		{
			st_Message.Format(_T("스텝이 %.0f 개라 자동 실행을 멈췄습니다 (한도 %d) — [실행] 또는 F5 를 누르십시오"), stepRatio, UI_AUTO_RUN_MAX_STEP);
			f_SetStatus(UI_STATUS_WARN, st_Message);
		}
		else
		{
			CWaitCursor st_Wait;

			if (st_Result.f_Run(&st_RunConfig, &st_Error) == 0)
			{
				f_SetStatus(UI_STATUS_ERROR, st_Error + ((nOldSampleNum > 0) ? _T(" — 표시 중인 결과는 마지막으로 성공한 설정입니다") : _T("")));
			}
			else
			{
				// 표와 그림이 옛 버퍼를 가리키지 않게 먼저 끊고 바꾼다.
				f_StopPlay();
				st_Plot.f_SetResult(nullptr);
				(VOID)st_ResultList.SetItemCountEx(0, LVSICF_NOINVALIDATEALL);
				st_Result.f_Commit();
				isResultStale = 0;

				f_SetupResultList();
				st_Plot.f_SetResult(&st_Result);
				st_TimeSlider.SetRange(0, st_Result.f_GetSampleNum() - 1, TRUE);
				(VOID)st_TimeSlider.SetLineSize(1);
				(VOID)st_TimeSlider.SetPageSize((((st_Result.f_GetSampleNum() - 1) / 10) > 1) ? ((st_Result.f_GetSampleNum() - 1) / 10) : 1);
				f_EnableResultControls(1);

				// 같은 길이의 시나리오를 고치는 중이면 보던 시각을 지킨다.
				if ((nCurStep < 0) || (nCurStep >= st_Result.f_GetSampleNum()) || (nOldSampleNum != st_Result.f_GetSampleNum()))
				{
					nCurStep = 0;
				}

				f_ShowStep(nCurStep, UI_SYNC_NONE);
				f_UpdateTitles();

				st_Message.Format(_T("실행 완료 — 표적 %d · 스텝 %d (%s s / %s s) · 표본 %d · %s ms"), st_RunConfig.nTargetNum, st_Result.f_GetSampleNum() - 1,
					f_Num_ToText(st_RunConfig.durationTime, 3).GetString(), f_Num_ToText(st_RunConfig.stepTime, 3).GetString(),
					st_Result.f_GetSampleNum(), f_Num_ToText(st_Result.f_GetRunMs(), 1).GetString());
				f_SetStatus(UI_STATUS_OK, st_Message);
			}
		}
	}

	st_ObjGrid.Invalidate(FALSE);
}

VOID CTargetSimUIDlg::f_ShowIssue(const ST_ScnIssue *st_Issue, INT32 isExplicit)
{
	CString	st_Text = st_Issue->st_Message;
	CWnd	*st_Ctrl = nullptr;

	if (st_Result.f_GetSampleNum() > 0)
	{
		st_Text += _T(" — 표시 중인 결과는 마지막으로 성공한 설정입니다");
	}

	f_SetStatus(UI_STATUS_ERROR, st_Text);

	// 오류 위치를 들고 있다가, 표적을 바꿔 가며 볼 때 표식을 다시 단다 (f_MarkIssue).
	st_LastIssue	= *st_Issue;
	isIssueActive	= 1;

	// 자동 실행 중에는 입력을 방해하지 않게 칸만 붉게 표시하고, [실행] 을 눌렀을 때만 그 칸으로 데려간다.
	if (isExplicit != 0)
	{
		switch (st_Issue->enPlace)
		{
		case SCN_AT_DURATION:
			st_Ctrl = GetDlgItem(IDC_SIM_DURATION);
			break;

		case SCN_AT_STEP:
			st_Ctrl = &st_StepCombo;
			break;

		case SCN_AT_PLATFORM:
		case SCN_AT_TARGET:
			GotoDlgCtrl(&st_ObjGrid);
			st_ObjGrid.f_SetCurCell((st_Issue->enPlace == SCN_AT_PLATFORM) ? 0 : (st_Issue->nTarget + 1),
				UI_OBJ_COL_FIELD + ((st_Issue->nField >= 0) ? st_Issue->nField : 0), 1);
			break;

		case SCN_AT_MANEUVER:
			f_SelectObject(st_Issue->nTarget + 1);
			GotoDlgCtrl(&st_MnvGrid);
			st_MnvGrid.f_SetCurCell(st_Issue->nManeuver, UI_MNV_COL_FIELD + ((st_Issue->nField >= 0) ? st_Issue->nField : SCN_FIELD_START), 1);
			break;

		default:
			break;
		}

		if (st_Ctrl != nullptr)
		{
			GotoDlgCtrl(st_Ctrl);
		}
	}

	f_MarkIssue();
}

VOID CTargetSimUIDlg::f_SetupResultList(VOID)
{
	static const LPCTSTR	s_PartName[3] = { _T(" 위도 [°]"), _T(" 경도 [°]"), _T(" 고도 [m]") };
	static const INT32		s_PartWidth[3] = { 124, 128, 112 };
	CHeaderCtrl				*st_Header = st_ResultList.GetHeaderCtrl();
	CString					st_Object;
	INT32					nColumn;
	INT32					nObject;
	INT32					nPart;

	isSyncing = 1;

	// 객체 수가 그대로면 열을 다시 만들지 않는다. 값을 굴리는 동안 표가 깜빡이지 않는다.
	if (nResultColumnNum != st_Result.f_GetColumnNum())
	{
		if (st_Header != nullptr)
		{
			for (nColumn = st_Header->GetItemCount() - 1; nColumn >= 0; nColumn--)
			{
				(VOID)st_ResultList.DeleteColumn(nColumn);
			}
		}

		// 열 순서는 CSV 와 같다.
		(VOID)st_ResultList.InsertColumn(0, _T("스텝"), LVCFMT_LEFT, f_Ui_Scale(58, dpi));
		(VOID)st_ResultList.InsertColumn(1, _T("시각 [s]"), LVCFMT_RIGHT, f_Ui_Scale(74, dpi));

		for (nObject = 0; nObject < st_Result.f_GetObjectNum(); nObject++)
		{
			if (nObject == 0)
			{
				st_Object = _T("플랫폼");
			}
			else
			{
				st_Object.Format(_T("표적%d"), nObject);
			}

			for (nPart = 0; nPart < 3; nPart++)
			{
				(VOID)st_ResultList.InsertColumn(2 + (nObject * 3) + nPart, st_Object + s_PartName[nPart], LVCFMT_RIGHT, f_Ui_Scale(s_PartWidth[nPart], dpi));
			}
		}

		nResultColumnNum = st_Result.f_GetColumnNum();
	}

	(VOID)st_ResultList.SetItemCountEx(st_Result.f_GetSampleNum(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
	st_ResultList.Invalidate(FALSE);

	isSyncing = 0;
}

VOID CTargetSimUIDlg::f_OnResultGetDispInfo(NMHDR *st_Hdr, LRESULT *pt_Result)
{
	NMLVDISPINFO	*st_Info = reinterpret_cast<NMLVDISPINFO *>(st_Hdr);
	CHAR			pt_Cell[UI_CELL_SIZE];

	if (((st_Info->item.mask & LVIF_TEXT) != 0U) && (st_Info->item.pszText != nullptr) && (st_Info->item.cchTextMax > 0))
	{
		st_Info->item.pszText[0] = L'\0';

		if (st_Result.f_FormatCell(st_Info->item.iItem, st_Info->item.iSubItem, pt_Cell, UI_CELL_SIZE) > 0)
		{
			if (MultiByteToWideChar(CP_UTF8, 0, pt_Cell, -1, st_Info->item.pszText, st_Info->item.cchTextMax) == 0)
			{
				st_Info->item.pszText[0] = L'\0';
			}
		}
	}

	*pt_Result = 0;
}

VOID CTargetSimUIDlg::f_OnResultFindItem(NMHDR *st_Hdr, LRESULT *pt_Result)
{
	// 가상 리스트는 글자 입력 검색 결과를 부모에게 묻는다. 0 을 돌려주면 0 행으로 건너뛰므로 '없음'(-1)으로 답한다.
	UNREFERENCED_PARAMETER(st_Hdr);

	*pt_Result = -1;
}

VOID CTargetSimUIDlg::f_OnResultItemChanged(NMHDR *st_Hdr, LRESULT *pt_Result)
{
	const NMLISTVIEW *st_Item = reinterpret_cast<NMLISTVIEW *>(st_Hdr);

	if ((isSyncing == 0) && (st_Item->iItem >= 0) && (st_Item->iItem != nCurStep) &&
		((st_Item->uChanged & LVIF_STATE) != 0U) &&
		((st_Item->uNewState & LVIS_SELECTED) != 0U) && ((st_Item->uOldState & LVIS_SELECTED) == 0U))
	{
		f_StopPlay();
		f_ShowStep(st_Item->iItem, UI_SYNC_TABLE);
	}

	*pt_Result = 0;
}

VOID CTargetSimUIDlg::f_OnResultCustomDraw(NMHDR *st_Hdr, LRESULT *pt_Result)
{
	NMLVCUSTOMDRAW *st_Draw = reinterpret_cast<NMLVCUSTOMDRAW *>(st_Hdr);

	// 줄무늬로 긴 표를 읽기 쉽게 한다.
	if (st_Draw->nmcd.dwDrawStage == CDDS_PREPAINT)
	{
		*pt_Result = CDRF_NOTIFYITEMDRAW;
	}
	else if (st_Draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
	{
		st_Draw->clrText	= UI_COLOR_TEXT;
		st_Draw->clrTextBk	= ((st_Draw->nmcd.dwItemSpec % 2U) == 0U) ? UI_COLOR_CARD : UI_COLOR_HEADER;
		*pt_Result = CDRF_NEWFONT;
	}
	else
	{
		*pt_Result = CDRF_DODEFAULT;
	}
}

VOID CTargetSimUIDlg::f_OnSaveCsvClicked(VOID)
{
	CString	st_Message;
	INT32	nLineNum = 0;
	INT32	errorCode;
	INT32	nSlash;

	// 치던 값과 미뤄 둔 실행을 먼저 반영해, 화면에 보이는 값의 결과를 저장한다.
	f_PrepareModal();

	if (st_Result.f_GetSampleNum() <= 0)
	{
		f_SetStatus(UI_STATUS_WARN, CString(_T("저장할 결과가 없습니다.")));
	}
	else
	{
		CFileDialog st_FileDlg(FALSE, _T("csv"), st_CsvFolder + _T("TargetSim_LLA.csv"), OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY,
			_T("CSV (*.csv)|*.csv||"), this);

		if (st_FileDlg.DoModal() == IDOK)
		{
			{
				CWaitCursor st_Wait;

				errorCode = st_Result.f_WriteCsv(st_FileDlg.GetPathName(), &nLineNum);
			}

			nSlash = st_FileDlg.GetPathName().ReverseFind(_T('\\'));

			if (nSlash >= 0)
			{
				st_CsvFolder = st_FileDlg.GetPathName().Left(nSlash + 1);
			}

			if (errorCode == 0)
			{
				st_Message.Format(_T("CSV 저장: %s (%d 줄)%s"), st_FileDlg.GetFileName().GetString(), nLineNum,
					(isResultStale != 0) ? _T(" — 주의: 지금 입력이 아니라 마지막으로 성공한 설정의 결과입니다") : _T(""));
				f_SetStatus((isResultStale != 0) ? UI_STATUS_WARN : UI_STATUS_OK, st_Message);
			}
			else
			{
				st_Message.Format(_T("CSV 를 쓰지 못했습니다 (errno %d): %s"), errorCode, st_FileDlg.GetPathName().GetString());
				f_SetStatus(UI_STATUS_ERROR, st_Message);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// 재생·표시

VOID CTargetSimUIDlg::f_ShowStep(INT32 nStep, INT32 syncSource)
{
	const INT32	nSampleNum = st_Result.f_GetSampleNum();
	CString		st_Text;
	INT32		nShow = nStep;

	// 현재 시각을 바꾸는 유일한 경로. 호출한 쪽 컨트롤은 다시 건드리지 않아 되먹임이 없다.
	if (nSampleNum > 0)
	{
		if (nShow < 0)
		{
			nShow = 0;
		}
		else if (nShow > (nSampleNum - 1))
		{
			nShow = nSampleNum - 1;
		}
		else
		{
			nShow = nStep;
		}

		nCurStep = nShow;

		if (syncSource != UI_SYNC_SLIDER)
		{
			st_TimeSlider.SetPos(nShow);
		}

		st_Text = CString(_T("t = ")) + f_Num_ToText(st_Result.f_GetSample(nShow)->simTime, 3) + _T(" / ") +
			f_Num_ToText(st_Result.f_GetSample(nSampleNum - 1)->simTime, 3) + _T(" s");
		SetDlgItemText(IDC_TIME_TEXT, st_Text);

		if (syncSource != UI_SYNC_TABLE)
		{
			isSyncing = 1;
			(VOID)st_ResultList.SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
			(VOID)st_ResultList.SetItemState(nShow, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
			(VOID)st_ResultList.EnsureVisible(nShow, FALSE);
			isSyncing = 0;
		}

		st_Plot.f_SetStep(nShow);
		st_ObjGrid.Invalidate(FALSE);
	}
}

VOID CTargetSimUIDlg::f_StartPlay(VOID)
{
	if ((st_Result.f_GetSampleNum() > 1) && (st_Result.f_GetStepTime() > 0.0))
	{
		if (nCurStep >= (st_Result.f_GetSampleNum() - 1))
		{
			f_ShowStep(0, UI_SYNC_NONE);
		}

		nPlayOriginStep	= nCurStep;
		nPlayTick		= 0;
		isPlaying		= (SetTimer(UI_PLAY_TIMER_ID, UI_PLAY_TICK_MS, nullptr) != 0U) ? 1 : 0;

		if (isPlaying != 0)
		{
			SetDlgItemText(IDC_PLAY, _T("일시정지"));
		}
	}
}

VOID CTargetSimUIDlg::f_StopPlay(VOID)
{
	if ((isPlaying != 0) && (GetSafeHwnd() != nullptr))
	{
		(VOID)KillTimer(UI_PLAY_TIMER_ID);
		SetDlgItemText(IDC_PLAY, _T("▶ 재생"));
	}

	isPlaying = 0;
}

VOID CTargetSimUIDlg::f_OnPlayClicked(VOID)
{
	if (isPlaying != 0)
	{
		f_StopPlay();
	}
	else
	{
		f_StartPlay();
	}
}

VOID CTargetSimUIDlg::f_OnSpeedSelChange(VOID)
{
	const INT32 nSelected = st_SpeedCombo.GetCurSel();

	if ((nSelected >= 0) && (nSelected < UI_PLAY_SPEED_NUM))
	{
		// 배속을 바꾼 지점부터 다시 센다.
		nPlaySpeed		= s_PlaySpeed[nSelected];
		nPlayOriginStep	= nCurStep;
		nPlayTick		= 0;
	}
}

VOID CTargetSimUIDlg::OnTimer(UINT_PTR timerId)
{
	const INT32	nSampleNum = st_Result.f_GetSampleNum();
	FLOAT64		advance;
	INT32		nStep;

	if (timerId == UI_RUN_TIMER_ID)
	{
		f_RunScenario(0);
	}
	else if ((timerId == UI_PLAY_TIMER_ID) && (isPlaying != 0) && (nSampleNum > 0) && (st_Result.f_GetStepTime() > 0.0))
	{
		// 실제 경과 시간이 아니라 틱 수로 진행해 재생 위치가 결정적이다.
		nPlayTick	= nPlayTick + 1;
		advance		= floor(((static_cast<FLOAT64>(nPlayTick) * static_cast<FLOAT64>(nPlaySpeed) * static_cast<FLOAT64>(UI_PLAY_TICK_MS)) /
						(1000.0 * st_Result.f_GetStepTime())) + 1.0e-9);

		if (advance >= static_cast<FLOAT64>((nSampleNum - 1) - nPlayOriginStep))
		{
			nStep = nSampleNum - 1;
		}
		else
		{
			nStep = nPlayOriginStep + static_cast<INT32>(advance);
		}

		f_ShowStep(nStep, UI_SYNC_PLAY);

		if (nStep >= (nSampleNum - 1))
		{
			f_StopPlay();
		}
	}
	else
	{
		CDialogEx::OnTimer(timerId);
	}
}

VOID CTargetSimUIDlg::OnHScroll(UINT32 scrollCode, UINT32 thumbPos, CScrollBar *st_ScrollBar)
{
	if ((st_ScrollBar != nullptr) && (st_Result.f_GetSampleNum() > 0) && (st_ScrollBar->GetSafeHwnd() == st_TimeSlider.GetSafeHwnd()))
	{
		f_StopPlay();
		f_ShowStep(st_TimeSlider.GetPos(), UI_SYNC_SLIDER);
	}
	else
	{
		CDialogEx::OnHScroll(scrollCode, thumbPos, st_ScrollBar);
	}
}

VOID CTargetSimUIDlg::f_EnableResultControls(INT32 isEnabled)
{
	f_Ui_EnableCtrl(this, IDC_PLAY, isEnabled);
	f_Ui_EnableCtrl(this, IDC_PLAY_SPEED, isEnabled);
	f_Ui_EnableCtrl(this, IDC_TIME_SLIDER, isEnabled);
	f_Ui_EnableCtrl(this, IDC_RESULT_LIST, isEnabled);
	f_Ui_EnableCtrl(this, IDC_SAVE_CSV, isEnabled);

	if (isEnabled == 0)
	{
		SetDlgItemText(IDC_TIME_TEXT, _T("t = -"));
	}
}

VOID CTargetSimUIDlg::f_SetStatus(INT32 kind, const CString &st_Text)
{
	CWnd *st_Ctrl = GetDlgItem(IDC_RUN_STATUS);

	statusKind = kind;

	if (st_Ctrl != nullptr)
	{
		st_Ctrl->SetWindowText(CString(_T("●  ")) + st_Text);
		st_Ctrl->Invalidate();
	}
}
