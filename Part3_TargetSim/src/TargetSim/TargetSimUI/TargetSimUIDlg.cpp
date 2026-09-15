#include "pch.h"
#include "framework.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "TargetSimUI.h"
#include "TargetSimUIDlg.h"
#include "UiNumber_JH.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define IDM_ABOUTBOX			0x0010
#define UI_SPEC_TARGET_NUM		2
#define UI_TURN_TYPE_NUM		4
#define UI_CSV_BUFFER_SIZE		1048576U
#define UI_BYTE_PER_MB			1048576.0

static const INT32		s_PlaySpeed[UI_PLAY_SPEED_NUM] = { 1, 2, 5, 10, 50, 100 };
static const LPCTSTR	s_PlaySpeedName[UI_PLAY_SPEED_NUM] = { _T("x1"), _T("x2"), _T("x5"), _T("x10"), _T("x50"), _T("x100") };
static const LPCTSTR	s_TurnName[UI_TURN_TYPE_NUM] = { _T("0 없음"), _T("1 Roll"), _T("2 Yaw"), _T("3 Pitch") };
static const LPCTSTR	s_FieldName[UI_OBJ_FIELD_NUM] = { _T("위도"), _T("경도"), _T("고도"), _T("속력"), _T("Roll"), _T("Pitch"), _T("Yaw") };

// 과제 명세 시나리오: 플랫폼 정지, 대함 표적 1, 대공 표적 1
static const LPCTSTR	s_SpecPlatform[UI_OBJ_FIELD_NUM] = { _T("32.0"), _T("126.0"), _T("0"), _T("0"), _T("0"), _T("0"), _T("0") };
static const LPCTSTR	s_SpecTarget[UI_SPEC_TARGET_NUM][UI_OBJ_FIELD_NUM] =
{
	{ _T("32.125"), _T("126.03"), _T("0"), _T("30"), _T("0"), _T("0"), _T("270") },
	{ _T("32.12"), _T("126.0"), _T("300"), _T("200"), _T("0"), _T("0"), _T("180") }
};

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

static VOID f_Ui_SetIssue(ST_UiIssue *st_Issue, INT32 ctrlId, INT32 nTarget, INT32 nManeuver, const CString &st_Message)
{
	st_Issue->ctrlId		= ctrlId;
	st_Issue->nTarget		= nTarget;
	st_Issue->nManeuver		= nManeuver;
	st_Issue->st_Message	= st_Message;
}

static CString f_Ui_Location(INT32 ctrlId, INT32 nTarget, INT32 nManeuver)
{
	CString st_Location;

	if ((nTarget >= 0) && (nManeuver >= 0))
	{
		st_Location.Format(_T("표적 %d 기동 %d"), nTarget + 1, nManeuver + 1);
	}
	else if (nTarget >= 0)
	{
		st_Location.Format(_T("표적 %d"), nTarget + 1);
	}
	else if (ctrlId <= IDC_LOAD_SPEC)
	{
		st_Location = _T("시뮬레이션");
	}
	else
	{
		st_Location = _T("플랫폼");
	}

	return st_Location;
}

static CString f_Ui_StatusText(EN_TgtStatus enStatus, INT32 isManeuver)
{
	CString st_Text;

	switch (enStatus)
	{
	case TGT_OK:
		st_Text = _T("정상");
		break;

	case TGT_ERR_NULL:
		st_Text = _T("내부 오류: 설정 또는 상태가 비어 있습니다");
		break;

	case TGT_ERR_TARGET_NUM:
		st_Text.Format(_T("표적 수는 1~%d 개여야 합니다"), TGT_MAX_TARGET_NUM);
		break;

	case TGT_ERR_MANEUVER_NUM:
		st_Text.Format(_T("표적별 기동 수는 0~%d 개여야 합니다"), TGT_MAX_MANEUVER_NUM);
		break;

	case TGT_ERR_TIME:
		if (isManeuver != 0)
		{
			st_Text = _T("기동 시각은 0 ≤ 시작 < 종료 ≤ 시뮬레이션 시간이어야 합니다");
		}
		else
		{
			st_Text.Format(_T("시뮬레이션 시간은 시간 간격의 정수배이고 스텝 수는 1~%d 이어야 합니다"), TGT_MAX_STEP_NUM);
		}
		break;

	case TGT_ERR_SPEED:
		st_Text = _T("속력이 허용 범위를 벗어났습니다");
		break;

	case TGT_ERR_TURN_TYPE:
		st_Text = _T("기동 축은 0~3 이어야 합니다");
		break;

	case TGT_ERR_MANEUVER_OVERLAP:
		st_Text = _T("같은 표적의 앞선 기동과 구간이 겹칩니다 (종료 = 다음 시작은 허용)");
		break;

	case TGT_ERR_COORD:
		st_Text = _T("좌표 변환에 실패했습니다");
		break;

	case TGT_ERR_SIM_STATE:
		st_Text = _T("내부 오류: 시뮬레이션 상태가 설정과 맞지 않습니다");
		break;

	case TGT_ERR_SIM_END:
		st_Text = _T("내부 오류: 마지막 표본 뒤로 진행을 요청했습니다");
		break;

	default:
		st_Text.Format(_T("알 수 없는 상태 코드 %d"), static_cast<INT32>(enStatus));
		break;
	}

	return st_Text + _T(" [") + CString(f_Tgt_StatusStr(enStatus)) + _T("]");
}

static INT32 f_Ui_IsGridMultiple(FLOAT64 value, FLOAT64 unit)
{
	const FLOAT64 ratio = value / unit;

	return (fabs(ratio - floor(ratio + 0.5)) <= UI_GRID_TOL) ? 1 : 0;
}

static INT32 f_Ui_IsStateFinite(const ST_TargetState *st_State)
{
	INT32 isFinite = 0;

	if (isfinite(st_State->simTime) && isfinite(st_State->st_Lla.lat) && isfinite(st_State->st_Lla.lon) &&
		isfinite(st_State->st_Lla.alt) && isfinite(st_State->st_PosEcef.x) && isfinite(st_State->st_PosEcef.y) &&
		isfinite(st_State->st_PosEcef.z) && isfinite(st_State->st_VelEcef.x) && isfinite(st_State->st_VelEcef.y) &&
		isfinite(st_State->st_VelEcef.z))
	{
		isFinite = 1;
	}

	return isFinite;
}

static INT32 f_Ui_IsAbsolutePath(const CString &st_Path)
{
	INT32 isAbsolute = 0;

	if (st_Path.GetLength() >= 3)
	{
		const TCHAR drive = st_Path[0];

		if ((((drive >= _T('A')) && (drive <= _T('Z'))) || ((drive >= _T('a')) && (drive <= _T('z')))) &&
			(st_Path[1] == _T(':')) && ((st_Path[2] == _T('\\')) || (st_Path[2] == _T('/'))))
		{
			isAbsolute = 1;
		}
		else if ((st_Path[0] == _T('\\')) && (st_Path[1] == _T('\\')))
		{
			isAbsolute = 1;
		}
		else
		{
			isAbsolute = 0;
		}
	}

	return isAbsolute;
}

static INT32 f_Ui_DluToPixel(const CDialog *st_Dlg, INT32 widthDlu)
{
	CRect st_Rect(0, 0, widthDlu, 0);

	st_Dlg->MapDialogRect(&st_Rect);

	return st_Rect.Width();
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

static VOID f_Ui_ClearTarget(ST_TargetText *st_Target)
{
	INT32 nField;
	INT32 nManeuver;

	for (nField = 0; nField < UI_OBJ_FIELD_NUM; nField++)
	{
		st_Target->st_FieldText[nField].Empty();
	}

	st_Target->nManeuverNum = 0;

	for (nManeuver = 0; nManeuver < TGT_MAX_MANEUVER_NUM; nManeuver++)
	{
		st_Target->st_Maneuver[nManeuver].enTurnType = TGT_TURN_NONE;

		for (nField = 0; nField < UI_MNV_FIELD_NUM; nField++)
		{
			st_Target->st_Maneuver[nManeuver].st_FieldText[nField].Empty();
		}
	}
}

BEGIN_MESSAGE_MAP(CTargetSimUIDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_TIMER()
	ON_WM_HSCROLL()
	ON_WM_CLOSE()
	ON_BN_CLICKED(IDC_LOAD_SPEC, &CTargetSimUIDlg::f_OnLoadSpecClicked)
	ON_BN_CLICKED(IDC_TGT_ADD, &CTargetSimUIDlg::f_OnTargetAddClicked)
	ON_BN_CLICKED(IDC_TGT_DELETE, &CTargetSimUIDlg::f_OnTargetDeleteClicked)
	ON_BN_CLICKED(IDC_MNV_ADD, &CTargetSimUIDlg::f_OnManeuverAddClicked)
	ON_BN_CLICKED(IDC_MNV_DELETE, &CTargetSimUIDlg::f_OnManeuverDeleteClicked)
	ON_BN_CLICKED(IDC_RUN, &CTargetSimUIDlg::f_OnRunClicked)
	ON_BN_CLICKED(IDC_PLAY, &CTargetSimUIDlg::f_OnPlayClicked)
	ON_BN_CLICKED(IDC_CSV_BROWSE, &CTargetSimUIDlg::f_OnCsvBrowseClicked)
	ON_BN_CLICKED(IDC_SAVE_CSV, &CTargetSimUIDlg::f_OnSaveCsvClicked)
	ON_CBN_SELCHANGE(IDC_TGT_SELECT, &CTargetSimUIDlg::f_OnTargetSelChange)
	ON_CBN_SELCHANGE(IDC_MNV_TYPE, &CTargetSimUIDlg::f_OnManeuverTypeChange)
	ON_CBN_SELCHANGE(IDC_PLAY_SPEED, &CTargetSimUIDlg::f_OnSpeedSelChange)
	ON_CONTROL_RANGE(EN_CHANGE, IDC_SIM_DURATION, IDC_SIM_STEP, &CTargetSimUIDlg::f_OnConfigFieldChange)
	ON_CONTROL_RANGE(EN_CHANGE, IDC_PLT_LAT, IDC_PLT_YAW, &CTargetSimUIDlg::f_OnConfigFieldChange)
	ON_CONTROL_RANGE(EN_CHANGE, IDC_TGT_LAT, IDC_TGT_YAW, &CTargetSimUIDlg::f_OnTargetFieldChange)
	ON_CONTROL_RANGE(EN_CHANGE, IDC_MNV_GRAVITY, IDC_MNV_END, &CTargetSimUIDlg::f_OnManeuverFieldChange)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_MNV_LIST, &CTargetSimUIDlg::f_OnManeuverItemChanged)
	ON_NOTIFY(LVN_GETDISPINFO, IDC_RESULT_LIST, &CTargetSimUIDlg::f_OnResultGetDispInfo)
	ON_NOTIFY(LVN_ODFINDITEM, IDC_RESULT_LIST, &CTargetSimUIDlg::f_OnResultFindItem)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_RESULT_LIST, &CTargetSimUIDlg::f_OnResultItemChanged)
END_MESSAGE_MAP()

CTargetSimUIDlg::CTargetSimUIDlg(CWnd *st_Parent)
	: CDialogEx(IDD_TARGETSIMUI_DIALOG, st_Parent)
	, iconHandle(nullptr)
	, nTargetNum(0)
	, nCurTarget(0)
	, nCurManeuver(-1)
	, isLoading(0)
	, isSyncing(0)
	, isConfigChanged(0)
	, st_SampleBuf(nullptr)
	, st_PointBuf(nullptr)
	, nSampleNum(0)
	, nObjectNum(0)
	, resultStepTime(0.0)
	, nCurStep(0)
	, isPlaying(0)
	, nPlayOriginStep(0)
	, nPlaySpeed(1)
	, nPlayTick(0)
{
	INT32 nTarget;

	iconHandle = ::LoadIcon(nullptr, IDI_APPLICATION);

	for (nTarget = 0; nTarget < TGT_MAX_TARGET_NUM; nTarget++)
	{
		f_Ui_ClearTarget(&st_TargetText[nTarget]);
	}

	(VOID)memset(&st_RunConfig, 0, sizeof(st_RunConfig));
	(VOID)memset(&st_ProbeConfig, 0, sizeof(st_ProbeConfig));
	(VOID)memset(&st_Sim, 0, sizeof(st_Sim));
}

CTargetSimUIDlg::~CTargetSimUIDlg()
{
	free(st_SampleBuf);
	free(st_PointBuf);
}

VOID CTargetSimUIDlg::DoDataExchange(CDataExchange *st_Dx)
{
	CDialogEx::DoDataExchange(st_Dx);
	DDX_Control(st_Dx, IDC_TGT_SELECT, st_TargetCombo);
	DDX_Control(st_Dx, IDC_MNV_TYPE, st_TypeCombo);
	DDX_Control(st_Dx, IDC_PLAY_SPEED, st_SpeedCombo);
	DDX_Control(st_Dx, IDC_MNV_LIST, st_ManeuverList);
	DDX_Control(st_Dx, IDC_RESULT_LIST, st_ResultList);
	DDX_Control(st_Dx, IDC_TIME_SLIDER, st_TimeSlider);
	DDX_Control(st_Dx, IDC_PLOT, st_Plot);
}

BOOL CTargetSimUIDlg::OnInitDialog(VOID)
{
	static const INT32		s_NumberEditId[] =
	{
		IDC_SIM_DURATION, IDC_SIM_STEP,
		IDC_PLT_LAT, IDC_PLT_LON, IDC_PLT_ALT, IDC_PLT_SPEED, IDC_PLT_ROLL, IDC_PLT_PITCH, IDC_PLT_YAW,
		IDC_TGT_LAT, IDC_TGT_LON, IDC_TGT_ALT, IDC_TGT_SPEED, IDC_TGT_ROLL, IDC_TGT_PITCH, IDC_TGT_YAW,
		IDC_MNV_GRAVITY, IDC_MNV_START, IDC_MNV_END
	};
	static const LPCTSTR	s_ManeuverColumn[5] = { _T("#"), _T("축"), _T("G"), _T("시작 [s]"), _T("종료 [s]") };
	static const INT32		s_ManeuverColumnDlu[5] = { 16, 40, 44, 50, 50 };
	const INT32				nNumberEditNum = static_cast<INT32>(sizeof(s_NumberEditId) / sizeof(s_NumberEditId[0]));
	CMenu					*st_SysMenu;
	CString					st_AboutText;
	CString					st_Module;
	INT32					nIndex;
	INT32					nSlash;

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

	for (nIndex = 0; nIndex < nNumberEditNum; nIndex++)
	{
		(VOID)SendDlgItemMessage(s_NumberEditId[nIndex], EM_LIMITTEXT, UI_TEXT_LIMIT, 0);
	}

	(VOID)SendDlgItemMessage(IDC_CSV_PATH, EM_LIMITTEXT, MAX_PATH, 0);

	(VOID)st_ManeuverList.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	for (nIndex = 0; nIndex < 5; nIndex++)
	{
		(VOID)st_ManeuverList.InsertColumn(nIndex, s_ManeuverColumn[nIndex], (nIndex < 2) ? LVCFMT_LEFT : LVCFMT_RIGHT,
			f_Ui_DluToPixel(this, s_ManeuverColumnDlu[nIndex]));
	}

	(VOID)st_ResultList.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

	for (nIndex = 0; nIndex < UI_TURN_TYPE_NUM; nIndex++)
	{
		(VOID)st_TypeCombo.AddString(s_TurnName[nIndex]);
	}

	for (nIndex = 0; nIndex < UI_PLAY_SPEED_NUM; nIndex++)
	{
		(VOID)st_SpeedCombo.AddString(s_PlaySpeedName[nIndex]);
	}

	(VOID)st_SpeedCombo.SetCurSel(0);
	nPlaySpeed = s_PlaySpeed[0];

	// CSV 기본 경로는 실행 파일 폴더
	(VOID)GetModuleFileName(nullptr, st_Module.GetBuffer(MAX_PATH), MAX_PATH);
	st_Module.ReleaseBuffer();
	nSlash = st_Module.ReverseFind(_T('\\'));

	if (nSlash >= 0)
	{
		SetDlgItemText(IDC_CSV_PATH, st_Module.Left(nSlash + 1) + _T("TargetSim_LLA.csv"));
	}

	f_LoadSpecScenario();
	f_EnableResultControls(0);
	st_Plot.f_Clear();
	f_SetStatus(CString(_T("명세 시나리오를 불러왔습니다. [실행]을 누르십시오.")));

	return TRUE;
}

VOID CTargetSimUIDlg::OnSysCommand(UINT32 commandId, LPARAM lParam)
{
	if ((commandId & 0xFFF0U) == IDM_ABOUTBOX)
	{
		CAboutDlg st_AboutDlg;

		f_StopPlay();
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
	f_StopPlay();
	EndDialog(IDCANCEL);
}

// ---------------------------------------------------------------------------
// 편집

VOID CTargetSimUIDlg::f_LoadSpecScenario(VOID)
{
	INT32 nField;
	INT32 nTarget;

	isLoading = 1;
	SetDlgItemText(IDC_SIM_DURATION, _T("60"));
	SetDlgItemText(IDC_SIM_STEP, _T("0.1"));

	for (nField = 0; nField < UI_OBJ_FIELD_NUM; nField++)
	{
		SetDlgItemText(IDC_PLT_LAT + nField, s_SpecPlatform[nField]);
	}

	isLoading = 0;

	for (nTarget = 0; nTarget < TGT_MAX_TARGET_NUM; nTarget++)
	{
		f_Ui_ClearTarget(&st_TargetText[nTarget]);

		if (nTarget < UI_SPEC_TARGET_NUM)
		{
			for (nField = 0; nField < UI_OBJ_FIELD_NUM; nField++)
			{
				st_TargetText[nTarget].st_FieldText[nField] = s_SpecTarget[nTarget][nField];
			}
		}
	}

	nTargetNum		= UI_SPEC_TARGET_NUM;
	nCurTarget		= 0;
	nCurManeuver	= -1;

	f_RefreshTargetCombo();
	f_LoadTargetEdits();
	f_MarkConfigChanged();
}

VOID CTargetSimUIDlg::f_SelectTarget(INT32 nTarget)
{
	if ((nTarget >= 0) && (nTarget < nTargetNum))
	{
		nCurTarget = nTarget;
		(VOID)st_TargetCombo.SetCurSel(nTarget);
		f_LoadTargetEdits();
	}
}

VOID CTargetSimUIDlg::f_RefreshTargetCombo(VOID)
{
	CString	st_Name;
	INT32	nTarget;

	st_TargetCombo.ResetContent();

	for (nTarget = 0; nTarget < nTargetNum; nTarget++)
	{
		st_Name.Format(_T("표적 %d"), nTarget + 1);
		(VOID)st_TargetCombo.AddString(st_Name);
	}

	(VOID)st_TargetCombo.SetCurSel(nCurTarget);
}

VOID CTargetSimUIDlg::f_LoadTargetEdits(VOID)
{
	INT32 nField;

	isLoading = 1;

	for (nField = 0; nField < UI_OBJ_FIELD_NUM; nField++)
	{
		SetDlgItemText(IDC_TGT_LAT + nField, st_TargetText[nCurTarget].st_FieldText[nField]);
	}

	isLoading = 0;

	f_RefreshManeuverList();
}

VOID CTargetSimUIDlg::f_RefreshManeuverList(VOID)
{
	CString	st_Index;
	INT32	nManeuver;

	isLoading		= 1;
	nCurManeuver	= -1;
	(VOID)st_ManeuverList.DeleteAllItems();

	for (nManeuver = 0; nManeuver < st_TargetText[nCurTarget].nManeuverNum; nManeuver++)
	{
		st_Index.Format(_T("%d"), nManeuver + 1);
		(VOID)st_ManeuverList.InsertItem(nManeuver, st_Index);
		f_UpdateManeuverRow(nManeuver);
	}

	isLoading = 0;

	f_LoadManeuverEdits();
	f_UpdateEditButtons();
}

VOID CTargetSimUIDlg::f_UpdateManeuverRow(INT32 nManeuver)
{
	const ST_ManeuverText	*st_Maneuver = &st_TargetText[nCurTarget].st_Maneuver[nManeuver];
	const INT32				nType = static_cast<INT32>(st_Maneuver->enTurnType);
	INT32					nField;

	if ((nType >= 0) && (nType < UI_TURN_TYPE_NUM))
	{
		(VOID)st_ManeuverList.SetItemText(nManeuver, 1, s_TurnName[nType]);
	}

	for (nField = 0; nField < UI_MNV_FIELD_NUM; nField++)
	{
		(VOID)st_ManeuverList.SetItemText(nManeuver, 2 + nField, st_Maneuver->st_FieldText[nField]);
	}
}

VOID CTargetSimUIDlg::f_SelectManeuver(INT32 nManeuver)
{
	isLoading = 1;
	(VOID)st_ManeuverList.SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);

	if ((nManeuver >= 0) && (nManeuver < st_TargetText[nCurTarget].nManeuverNum))
	{
		(VOID)st_ManeuverList.SetItemState(nManeuver, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		(VOID)st_ManeuverList.EnsureVisible(nManeuver, FALSE);
		nCurManeuver = nManeuver;
	}
	else
	{
		nCurManeuver = -1;
	}

	isLoading = 0;

	f_LoadManeuverEdits();
	f_UpdateEditButtons();
}

VOID CTargetSimUIDlg::f_LoadManeuverEdits(VOID)
{
	const INT32	isEnabled = (nCurManeuver >= 0) ? 1 : 0;
	INT32		nField;

	isLoading = 1;

	if (isEnabled != 0)
	{
		const ST_ManeuverText *st_Maneuver = &st_TargetText[nCurTarget].st_Maneuver[nCurManeuver];

		(VOID)st_TypeCombo.SetCurSel(static_cast<INT32>(st_Maneuver->enTurnType));

		for (nField = 0; nField < UI_MNV_FIELD_NUM; nField++)
		{
			SetDlgItemText(IDC_MNV_GRAVITY + nField, st_Maneuver->st_FieldText[nField]);
		}
	}
	else
	{
		(VOID)st_TypeCombo.SetCurSel(-1);

		for (nField = 0; nField < UI_MNV_FIELD_NUM; nField++)
		{
			SetDlgItemText(IDC_MNV_GRAVITY + nField, _T(""));
		}
	}

	isLoading = 0;

	f_Ui_EnableCtrl(this, IDC_MNV_TYPE, isEnabled);

	for (nField = 0; nField < UI_MNV_FIELD_NUM; nField++)
	{
		f_Ui_EnableCtrl(this, IDC_MNV_GRAVITY + nField, isEnabled);
	}
}

VOID CTargetSimUIDlg::f_UpdateEditButtons(VOID)
{
	CString st_Count;

	f_Ui_EnableCtrl(this, IDC_TGT_ADD, (nTargetNum < TGT_MAX_TARGET_NUM) ? 1 : 0);
	f_Ui_EnableCtrl(this, IDC_TGT_DELETE, (nTargetNum > 1) ? 1 : 0);
	f_Ui_EnableCtrl(this, IDC_MNV_ADD, (st_TargetText[nCurTarget].nManeuverNum < TGT_MAX_MANEUVER_NUM) ? 1 : 0);
	f_Ui_EnableCtrl(this, IDC_MNV_DELETE, (nCurManeuver >= 0) ? 1 : 0);

	st_Count.Format(_T("%d / %d"), nTargetNum, TGT_MAX_TARGET_NUM);
	SetDlgItemText(IDC_TGT_COUNT, st_Count);
	st_Count.Format(_T("%d / %d"), st_TargetText[nCurTarget].nManeuverNum, TGT_MAX_MANEUVER_NUM);
	SetDlgItemText(IDC_MNV_COUNT, st_Count);
}

VOID CTargetSimUIDlg::f_MarkConfigChanged(VOID)
{
	isConfigChanged = 1;
	f_RefreshStatus();
}

VOID CTargetSimUIDlg::f_OnLoadSpecClicked(VOID)
{
	f_LoadSpecScenario();
	f_SetStatus(CString(_T("명세 시나리오를 불러왔습니다. [실행]을 누르십시오.")));
}

VOID CTargetSimUIDlg::f_OnTargetAddClicked(VOID)
{
	INT32 nField;

	if (nTargetNum < TGT_MAX_TARGET_NUM)
	{
		const INT32 nNewTarget = nTargetNum;

		// 새 표적은 현재 표적의 초기값을 복사하고 기동은 비운다.
		f_Ui_ClearTarget(&st_TargetText[nNewTarget]);

		for (nField = 0; nField < UI_OBJ_FIELD_NUM; nField++)
		{
			st_TargetText[nNewTarget].st_FieldText[nField] = st_TargetText[nCurTarget].st_FieldText[nField];
		}

		nTargetNum	= nTargetNum + 1;
		nCurTarget	= nNewTarget;

		f_RefreshTargetCombo();
		f_LoadTargetEdits();
		f_MarkConfigChanged();
	}
}

VOID CTargetSimUIDlg::f_OnTargetDeleteClicked(VOID)
{
	INT32 nTarget;

	if (nTargetNum > 1)
	{
		for (nTarget = nCurTarget; nTarget < (nTargetNum - 1); nTarget++)
		{
			st_TargetText[nTarget] = st_TargetText[nTarget + 1];
		}

		f_Ui_ClearTarget(&st_TargetText[nTargetNum - 1]);
		nTargetNum = nTargetNum - 1;

		if (nCurTarget > (nTargetNum - 1))
		{
			nCurTarget = nTargetNum - 1;
		}

		f_RefreshTargetCombo();
		f_LoadTargetEdits();
		f_MarkConfigChanged();
		GotoDlgCtrl(&st_TargetCombo);
	}
}

VOID CTargetSimUIDlg::f_OnTargetSelChange(VOID)
{
	const INT32 nSelected = st_TargetCombo.GetCurSel();

	if ((nSelected >= 0) && (nSelected < nTargetNum) && (nSelected != nCurTarget))
	{
		nCurTarget = nSelected;
		f_LoadTargetEdits();
	}
}

VOID CTargetSimUIDlg::f_OnManeuverAddClicked(VOID)
{
	ST_TargetText	*st_Target = &st_TargetText[nCurTarget];
	CWnd			*st_EndEdit;

	if (st_Target->nManeuverNum < TGT_MAX_MANEUVER_NUM)
	{
		const INT32			nNewManeuver = st_Target->nManeuverNum;
		ST_ManeuverText		*st_Maneuver = &st_Target->st_Maneuver[nNewManeuver];

		// 시작은 직전 기동의 종료로 채워 맞닿은 구간을 쉽게 만든다.
		st_Maneuver->enTurnType						= TGT_TURN_NONE;
		st_Maneuver->st_FieldText[UI_FIELD_GRAVITY]	= _T("0");
		st_Maneuver->st_FieldText[UI_FIELD_START]	= (nNewManeuver > 0) ? st_Target->st_Maneuver[nNewManeuver - 1].st_FieldText[UI_FIELD_END] : CString(_T("0"));
		st_Maneuver->st_FieldText[UI_FIELD_END].Empty();
		st_Target->nManeuverNum						= nNewManeuver + 1;

		f_RefreshManeuverList();
		f_SelectManeuver(nNewManeuver);
		f_MarkConfigChanged();

		st_EndEdit = GetDlgItem(IDC_MNV_END);

		if (st_EndEdit != nullptr)
		{
			GotoDlgCtrl(st_EndEdit);
		}
	}
}

VOID CTargetSimUIDlg::f_OnManeuverDeleteClicked(VOID)
{
	ST_TargetText	*st_Target = &st_TargetText[nCurTarget];
	INT32			nManeuver;
	INT32			nField;

	if ((nCurManeuver >= 0) && (nCurManeuver < st_Target->nManeuverNum))
	{
		for (nManeuver = nCurManeuver; nManeuver < (st_Target->nManeuverNum - 1); nManeuver++)
		{
			st_Target->st_Maneuver[nManeuver] = st_Target->st_Maneuver[nManeuver + 1];
		}

		st_Target->st_Maneuver[st_Target->nManeuverNum - 1].enTurnType = TGT_TURN_NONE;

		for (nField = 0; nField < UI_MNV_FIELD_NUM; nField++)
		{
			st_Target->st_Maneuver[st_Target->nManeuverNum - 1].st_FieldText[nField].Empty();
		}

		st_Target->nManeuverNum = st_Target->nManeuverNum - 1;

		f_RefreshManeuverList();
		f_MarkConfigChanged();
		GotoDlgCtrl(&st_ManeuverList);
	}
}

VOID CTargetSimUIDlg::f_OnManeuverTypeChange(VOID)
{
	const INT32 nSelected = st_TypeCombo.GetCurSel();

	if ((isLoading == 0) && (nCurManeuver >= 0) && (nSelected >= 0) && (nSelected < UI_TURN_TYPE_NUM))
	{
		st_TargetText[nCurTarget].st_Maneuver[nCurManeuver].enTurnType = static_cast<EN_TurnType>(nSelected);
		f_UpdateManeuverRow(nCurManeuver);
		f_MarkConfigChanged();
	}
}

VOID CTargetSimUIDlg::f_OnConfigFieldChange(UINT32 ctrlId)
{
	UNREFERENCED_PARAMETER(ctrlId);

	if (isLoading == 0)
	{
		f_MarkConfigChanged();
	}
}

VOID CTargetSimUIDlg::f_OnTargetFieldChange(UINT32 ctrlId)
{
	const INT32 nField = static_cast<INT32>(ctrlId) - IDC_TGT_LAT;

	if ((isLoading == 0) && (nField >= 0) && (nField < UI_OBJ_FIELD_NUM))
	{
		(VOID)GetDlgItemText(static_cast<INT32>(ctrlId), st_TargetText[nCurTarget].st_FieldText[nField]);
		f_MarkConfigChanged();
	}
}

VOID CTargetSimUIDlg::f_OnManeuverFieldChange(UINT32 ctrlId)
{
	const INT32 nField = static_cast<INT32>(ctrlId) - IDC_MNV_GRAVITY;

	if ((isLoading == 0) && (nCurManeuver >= 0) && (nField >= 0) && (nField < UI_MNV_FIELD_NUM))
	{
		(VOID)GetDlgItemText(static_cast<INT32>(ctrlId), st_TargetText[nCurTarget].st_Maneuver[nCurManeuver].st_FieldText[nField]);
		f_UpdateManeuverRow(nCurManeuver);
		f_MarkConfigChanged();
	}
}

VOID CTargetSimUIDlg::f_OnManeuverItemChanged(NMHDR *st_Hdr, LRESULT *pt_Result)
{
	const NMLISTVIEW *st_Item = reinterpret_cast<NMLISTVIEW *>(st_Hdr);

	if ((isLoading == 0) && ((st_Item->uChanged & LVIF_STATE) != 0U))
	{
		// 선택 해제와 선택이 따로 오므로 현재 선택을 다시 읽는다.
		const INT32 nSelected = st_ManeuverList.GetNextItem(-1, LVNI_SELECTED);

		if (nSelected != nCurManeuver)
		{
			nCurManeuver = nSelected;
			f_LoadManeuverEdits();
			f_UpdateEditButtons();
		}
	}

	*pt_Result = 0;
}

// ---------------------------------------------------------------------------
// 검증

INT32 CTargetSimUIDlg::f_ReadNumber(const CString &st_Text, INT32 ctrlId, INT32 nTarget, INT32 nManeuver, FLOAT64 minValue, FLOAT64 maxValue,
	LPCTSTR pt_Name, FLOAT64 *pt_Value, ST_UiIssue *st_Issue) const
{
	INT32		isOk = 0;
	FLOAT64		value = 0.0;
	CString		st_Rule;

	switch (f_Num_Parse(st_Text, &value))
	{
	case NUM_OK:
		if ((value >= minValue) && (value <= maxValue))
		{
			*pt_Value	= value;
			isOk		= 1;
		}
		else
		{
			st_Rule.Format(_T("%g ~ %g 범위여야 합니다"), minValue, maxValue);
		}
		break;

	case NUM_EMPTY:
		st_Rule = _T("값을 입력하십시오");
		break;

	case NUM_SYNTAX:
		st_Rule = _T("숫자로 읽을 수 없습니다 (소수점은 '.', 예: -0.5, 1e3)");
		break;

	case NUM_NOT_FINITE:
		st_Rule = _T("유한한 값이어야 합니다");
		break;

	default:
		st_Rule = _T("알 수 없는 입력 오류입니다");
		break;
	}

	if (isOk == 0)
	{
		f_Ui_SetIssue(st_Issue, ctrlId, nTarget, nManeuver,
			f_Ui_Location(ctrlId, nTarget, nManeuver) + _T(" ") + pt_Name + _T(": ") + st_Rule + _T(" (입력: '") + st_Text + _T("')"));
	}

	return isOk;
}

INT32 CTargetSimUIDlg::f_ReadObject(const CString *st_FieldText, INT32 baseCtrlId, INT32 nTarget, FLOAT64 minSpeed,
	ST_CoordLla *st_Lla, ST_CoordAtt *st_Att, FLOAT64 *pt_Speed, ST_UiIssue *st_Issue) const
{
	const FLOAT64	minValue[UI_OBJ_FIELD_NUM] = { -UI_LAT_LIMIT, -UI_LON_LIMIT, UI_ALT_MIN, minSpeed, -UI_ROLL_LIMIT, -UI_PITCH_LIMIT, -UI_YAW_LIMIT };
	const FLOAT64	maxValue[UI_OBJ_FIELD_NUM] = { UI_LAT_LIMIT, UI_LON_LIMIT, UI_ALT_MAX, UI_SPEED_MAX, UI_ROLL_LIMIT, UI_PITCH_LIMIT, UI_YAW_LIMIT };
	FLOAT64			fieldValue[UI_OBJ_FIELD_NUM] = { 0.0 };
	INT32			isOk = 1;
	INT32			nField;

	for (nField = 0; nField < UI_OBJ_FIELD_NUM; nField++)
	{
		if (isOk != 0)
		{
			isOk = f_ReadNumber(st_FieldText[nField], baseCtrlId + nField, nTarget, -1, minValue[nField], maxValue[nField],
				s_FieldName[nField], &fieldValue[nField], st_Issue);
		}
	}

	if (isOk != 0)
	{
		st_Lla->lat		= f_Deg_To_Rad(fieldValue[UI_FIELD_LAT]);
		st_Lla->lon		= f_Deg_To_Rad(fieldValue[UI_FIELD_LON]);
		st_Lla->alt		= fieldValue[UI_FIELD_ALT];
		*pt_Speed		= fieldValue[UI_FIELD_SPEED];
		st_Att->roll	= f_Deg_To_Rad(fieldValue[UI_FIELD_ROLL]);
		st_Att->pitch	= f_Deg_To_Rad(fieldValue[UI_FIELD_PITCH]);
		st_Att->yaw		= f_Deg_To_Rad(fieldValue[UI_FIELD_YAW]);
	}

	return isOk;
}

INT32 CTargetSimUIDlg::f_BuildConfig(ST_SimConfig *st_Config, ST_UiIssue *st_Issue) const
{
	CString		st_EditText;
	CString		st_PlatformText[UI_OBJ_FIELD_NUM];
	CString		st_Message;
	FLOAT64		duration = 0.0;
	FLOAT64		step = 0.0;
	FLOAT64		gravity = 0.0;
	FLOAT64		startTime = 0.0;
	FLOAT64		endTime = 0.0;
	FLOAT64		turnPerStep;
	INT32		isOk;
	INT32		nField;
	INT32		nTarget;
	INT32		nManeuver;

	(VOID)memset(st_Config, 0, sizeof(ST_SimConfig));

	(VOID)GetDlgItemText(IDC_SIM_DURATION, st_EditText);
	isOk = f_ReadNumber(st_EditText, IDC_SIM_DURATION, -1, -1, UI_TIME_RES, UI_DURATION_MAX, _T("시간"), &duration, st_Issue);

	if (isOk != 0)
	{
		(VOID)GetDlgItemText(IDC_SIM_STEP, st_EditText);
		isOk = f_ReadNumber(st_EditText, IDC_SIM_STEP, -1, -1, UI_TIME_RES, UI_STEP_MAX, _T("간격"), &step, st_Issue);
	}

	// 1 ms 격자면 시각을 %.3f 로 정확히 쓸 수 있다.
	if ((isOk != 0) && (f_Ui_IsGridMultiple(step, UI_TIME_RES) == 0))
	{
		f_Ui_SetIssue(st_Issue, IDC_SIM_STEP, -1, -1,
			CString(_T("시뮬레이션 간격: 1 ms (0.001 s) 단위여야 합니다 (입력: '")) + st_EditText + _T("')"));
		isOk = 0;
	}

	if (isOk != 0)
	{
		st_Config->durationTime	= duration;
		st_Config->stepTime		= step;

		for (nField = 0; nField < UI_OBJ_FIELD_NUM; nField++)
		{
			(VOID)GetDlgItemText(IDC_PLT_LAT + nField, st_PlatformText[nField]);
		}

		isOk = f_ReadObject(st_PlatformText, IDC_PLT_LAT, -1, 0.0,
			&st_Config->st_Platform.st_InitLla, &st_Config->st_Platform.st_InitAtt, &st_Config->st_Platform.headingSpeed, st_Issue);
	}

	if (isOk != 0)
	{
		st_Config->nTargetNum = nTargetNum;
	}

	for (nTarget = 0; (nTarget < nTargetNum) && (isOk != 0); nTarget++)
	{
		const ST_TargetText		*st_Source = &st_TargetText[nTarget];
		ST_TargetInit			*st_Init = &st_Config->st_Target[nTarget];

		isOk = f_ReadObject(st_Source->st_FieldText, IDC_TGT_LAT, nTarget, UI_TGT_SPEED_MIN,
			&st_Init->st_InitLla, &st_Init->st_InitAtt, &st_Init->headingSpeed, st_Issue);

		if (isOk != 0)
		{
			st_Init->nManeuverNum = st_Source->nManeuverNum;
		}

		for (nManeuver = 0; (nManeuver < st_Source->nManeuverNum) && (isOk != 0); nManeuver++)
		{
			const ST_ManeuverText	*st_ManeuverText = &st_Source->st_Maneuver[nManeuver];
			ST_TargetManeuver		*st_Maneuver = &st_Init->st_Maneuver[nManeuver];

			isOk = f_ReadNumber(st_ManeuverText->st_FieldText[UI_FIELD_GRAVITY], IDC_MNV_GRAVITY, nTarget, nManeuver,
				-UI_G_LIMIT, UI_G_LIMIT, _T("G"), &gravity, st_Issue);

			// 시작·종료의 순서와 겹침은 Core 가 판정하므로 여기서는 유한성만 본다.
			if (isOk != 0)
			{
				isOk = f_ReadNumber(st_ManeuverText->st_FieldText[UI_FIELD_START], IDC_MNV_START, nTarget, nManeuver,
					-HUGE_VAL, HUGE_VAL, _T("시작"), &startTime, st_Issue);
			}

			if (isOk != 0)
			{
				isOk = f_ReadNumber(st_ManeuverText->st_FieldText[UI_FIELD_END], IDC_MNV_END, nTarget, nManeuver,
					-HUGE_VAL, HUGE_VAL, _T("종료"), &endTime, st_Issue);
			}

			// 스텝 격자 밖 시각은 Core 에서 다음 스텝으로 밀리거나 기동이 통째로 무시될 수 있다.
			if ((isOk != 0) && (f_Ui_IsGridMultiple(startTime, step) == 0))
			{
				st_Message.Format(_T("%s 시작: 시뮬레이션 간격 %g s 의 정수배여야 합니다 (입력: '%s')"),
					f_Ui_Location(IDC_MNV_START, nTarget, nManeuver).GetString(), step, st_ManeuverText->st_FieldText[UI_FIELD_START].GetString());
				f_Ui_SetIssue(st_Issue, IDC_MNV_START, nTarget, nManeuver, st_Message);
				isOk = 0;
			}

			if ((isOk != 0) && (f_Ui_IsGridMultiple(endTime, step) == 0))
			{
				st_Message.Format(_T("%s 종료: 시뮬레이션 간격 %g s 의 정수배여야 합니다 (입력: '%s')"),
					f_Ui_Location(IDC_MNV_END, nTarget, nManeuver).GetString(), step, st_ManeuverText->st_FieldText[UI_FIELD_END].GetString());
				f_Ui_SetIssue(st_Issue, IDC_MNV_END, nTarget, nManeuver, st_Message);
				isOk = 0;
			}

			// 스텝당 회전각을 30 도 이하로 묶어 중점법 오차와 비물리 선회를 막는다.
			if ((isOk != 0) && (st_ManeuverText->enTurnType != TGT_TURN_NONE))
			{
				turnPerStep = ((fabs(gravity) * G_FORCE) / st_Init->headingSpeed) * step;

				if (turnPerStep > UI_TURN_STEP_MAX)
				{
					// 허용 |G| 는 내림해서 보여 줘야 그 값을 그대로 넣었을 때 다시 걸리지 않는다.
					st_Message.Format(_T("%s G: 스텝당 회전각이 %.0f° 를 넘습니다 (%.3f°, 이 표적 속력·간격에서 허용 |G| ≤ %.3f)"),
						f_Ui_Location(IDC_MNV_GRAVITY, nTarget, nManeuver).GetString(), f_Rad_To_Deg(UI_TURN_STEP_MAX), f_Rad_To_Deg(turnPerStep),
						floor(((UI_TURN_STEP_MAX * st_Init->headingSpeed) / (G_FORCE * step)) * 1000.0) / 1000.0);
					f_Ui_SetIssue(st_Issue, IDC_MNV_GRAVITY, nTarget, nManeuver, st_Message);
					isOk = 0;
				}
			}

			if (isOk != 0)
			{
				st_Maneuver->enTurnType		= st_ManeuverText->enTurnType;
				st_Maneuver->gravityValue	= gravity;
				st_Maneuver->startTime		= startTime;
				st_Maneuver->endTime		= endTime;
			}
		}
	}

	return isOk;
}

VOID CTargetSimUIDlg::f_LocateCoreError(EN_TgtStatus enStatus, ST_UiIssue *st_Issue)
{
	EN_TgtStatus	enProbe;
	CString			st_Message;
	INT32			isFound = 0;
	INT32			nTarget;
	INT32			nManeuver;

	// 판정은 Core 가 하고, 기동을 뺀 설정과 기동을 하나씩 늘린 설정을 다시 검증해 위치만 찾는다.
	st_ProbeConfig = st_RunConfig;

	for (nTarget = 0; nTarget < TGT_MAX_TARGET_NUM; nTarget++)
	{
		st_ProbeConfig.st_Target[nTarget].nManeuverNum = 0;
	}

	enProbe = f_Tgt_ValidateConfig(&st_ProbeConfig);

	if (enProbe == TGT_ERR_TIME)
	{
		st_Message.Format(_T("시뮬레이션 시간: %s (시간 / 간격 = %.6f)"),
			f_Ui_StatusText(enProbe, 0).GetString(), st_RunConfig.durationTime / st_RunConfig.stepTime);
		f_Ui_SetIssue(st_Issue, IDC_SIM_DURATION, -1, -1, st_Message);
		isFound = 1;
	}
	else if (enProbe == TGT_ERR_SPEED)
	{
		f_Ui_SetIssue(st_Issue, IDC_PLT_SPEED, -1, -1, CString(_T("속력: ")) + f_Ui_StatusText(enProbe, 0));
		isFound = 1;
	}
	else if (enProbe != TGT_OK)
	{
		f_Ui_SetIssue(st_Issue, 0, -1, -1, f_Ui_StatusText(enProbe, 0));
		isFound = 1;
	}
	else
	{
		isFound = 0;
	}

	for (nTarget = 0; (nTarget < st_RunConfig.nTargetNum) && (nTarget < TGT_MAX_TARGET_NUM) && (isFound == 0); nTarget++)
	{
		st_ProbeConfig.nTargetNum	= 1;
		st_ProbeConfig.st_Target[0]	= st_RunConfig.st_Target[nTarget];

		for (nManeuver = 0; (nManeuver < st_RunConfig.st_Target[nTarget].nManeuverNum) && (nManeuver < TGT_MAX_MANEUVER_NUM) && (isFound == 0); nManeuver++)
		{
			st_ProbeConfig.st_Target[0].nManeuverNum = nManeuver + 1;
			enProbe = f_Tgt_ValidateConfig(&st_ProbeConfig);

			if (enProbe != TGT_OK)
			{
				f_Ui_SetIssue(st_Issue, IDC_MNV_START, nTarget, nManeuver,
					f_Ui_Location(IDC_MNV_START, nTarget, nManeuver) + _T(": ") + f_Ui_StatusText(enProbe, 1));
				isFound = 1;
			}
		}
	}

	if (isFound == 0)
	{
		f_Ui_SetIssue(st_Issue, 0, -1, -1, f_Ui_StatusText(enStatus, 0));
	}
}

VOID CTargetSimUIDlg::f_ReportIssue(const ST_UiIssue *st_Issue)
{
	CWnd	*st_Ctrl = nullptr;
	CString	st_Text = st_Issue->st_Message;

	f_StopPlay();

	if ((st_Issue->nTarget >= 0) && (st_Issue->nTarget < nTargetNum))
	{
		if (st_Issue->nTarget != nCurTarget)
		{
			f_SelectTarget(st_Issue->nTarget);
		}

		if (st_Issue->nManeuver >= 0)
		{
			f_SelectManeuver(st_Issue->nManeuver);
		}
	}

	if (st_Issue->ctrlId > 0)
	{
		st_Ctrl = GetDlgItem(st_Issue->ctrlId);
	}

	if (st_Ctrl != nullptr)
	{
		GotoDlgCtrl(st_Ctrl);
		(VOID)SendDlgItemMessage(st_Issue->ctrlId, EM_SETSEL, 0, -1);
	}

	if (nSampleNum > 0)
	{
		st_Text += _T("\r\n실행 실패 — 표시 중인 결과는 이전 실행입니다.");
	}

	f_SetStatus(st_Text);
	(VOID)AfxMessageBox(st_Issue->st_Message, MB_OK | MB_ICONWARNING);
}

// ---------------------------------------------------------------------------
// 실행·출력

VOID CTargetSimUIDlg::f_OnRunClicked(VOID)
{
	f_RunScenario();
}

VOID CTargetSimUIDlg::f_RunScenario(VOID)
{
	ST_UiIssue		st_Issue;
	ST_SimSample	*st_NewSample = nullptr;
	ST_PlotPoint	*st_NewPoint = nullptr;
	EN_TgtStatus	enStatus;
	CWnd			*st_StatusCtrl;
	CString			st_Message;
	CHAR			pt_Time[UI_CELL_SIZE];
	CHAR			pt_Duration[UI_CELL_SIZE];
	CHAR			pt_Step[UI_CELL_SIZE];
	UINT64			sampleBytes;
	UINT64			pointBytes;
	INT32			nNewSampleNum = 0;
	INT32			nNewObjectNum = 0;
	INT32			nStep;
	INT32			isOk;

	f_StopPlay();
	f_Ui_SetIssue(&st_Issue, 0, -1, -1, CString());
	f_SetStatus(CString(_T("실행 중...")));

	st_StatusCtrl = GetDlgItem(IDC_RUN_STATUS);

	if (st_StatusCtrl != nullptr)
	{
		st_StatusCtrl->UpdateWindow();
	}

	{
		CWaitCursor st_Wait;

		isOk = f_BuildConfig(&st_RunConfig, &st_Issue);

		if (isOk != 0)
		{
			enStatus = f_Tgt_ValidateConfig(&st_RunConfig);

			if (enStatus != TGT_OK)
			{
				f_LocateCoreError(enStatus, &st_Issue);
				isOk = 0;
			}
		}

		// Core 호출 계약: 0 으로 채운 상태로 초기화하고, 초기화가 실패하면 진행하지 않는다.
		if (isOk != 0)
		{
			(VOID)memset(&st_Sim, 0, sizeof(st_Sim));
			enStatus = f_Tgt_InitSim(&st_Sim, &st_RunConfig);

			if (enStatus != TGT_OK)
			{
				f_Ui_SetIssue(&st_Issue, 0, -1, -1, CString(_T("초기화 실패: ")) + f_Ui_StatusText(enStatus, 0));
				isOk = 0;
			}
		}

		// 초기화가 성공했으면 스텝 수는 1 ~ TGT_MAX_STEP_NUM 이다. 버퍼 크기를 정하기 전에 한 번 더 확인한다.
		if ((isOk != 0) && ((st_Sim.nStepNum < 1) || (st_Sim.nStepNum > TGT_MAX_STEP_NUM)))
		{
			f_Ui_SetIssue(&st_Issue, 0, -1, -1, CString(_T("초기화 실패: ")) + f_Ui_StatusText(TGT_ERR_SIM_STATE, 0));
			isOk = 0;
		}

		if (isOk != 0)
		{
			nNewSampleNum	= st_Sim.nStepNum + 1;
			nNewObjectNum	= st_RunConfig.nTargetNum + 1;
			sampleBytes		= static_cast<UINT64>(nNewSampleNum) * static_cast<UINT64>(sizeof(ST_SimSample));
			pointBytes		= static_cast<UINT64>(nNewSampleNum) * static_cast<UINT64>(nNewObjectNum) * static_cast<UINT64>(sizeof(ST_PlotPoint));

			// 최악 116 MB 라 MFC new 의 예외 대신 calloc 의 NULL 로 실패를 받는다.
			st_NewSample	= static_cast<ST_SimSample *>(calloc(static_cast<UINT64>(nNewSampleNum), sizeof(ST_SimSample)));
			st_NewPoint		= static_cast<ST_PlotPoint *>(calloc(static_cast<UINT64>(nNewSampleNum) * static_cast<UINT64>(nNewObjectNum), sizeof(ST_PlotPoint)));

			if ((st_NewSample == nullptr) || (st_NewPoint == nullptr))
			{
				st_Message.Format(_T("메모리 부족: 결과 버퍼에 약 %.0f MB 가 필요합니다"),
					static_cast<FLOAT64>(sampleBytes + pointBytes) / UI_BYTE_PER_MB);
				f_Ui_SetIssue(&st_Issue, 0, -1, -1, st_Message);
				isOk = 0;
			}
		}

		if (isOk != 0)
		{
			st_NewSample[0] = st_Sim.st_Sample;

			for (nStep = 1; (nStep < nNewSampleNum) && (isOk != 0); nStep++)
			{
				enStatus = f_Tgt_StepSim(&st_Sim);

				if (enStatus == TGT_OK)
				{
					st_NewSample[nStep] = st_Sim.st_Sample;
				}
				else
				{
					(VOID)f_Num_FormatFixed(pt_Time, UI_CELL_SIZE, static_cast<FLOAT64>(nStep) * st_RunConfig.stepTime, 3);
					st_Message.Format(_T("스텝 %d (t = %s s) 진행 실패: "), nStep, CString(pt_Time).GetString());
					f_Ui_SetIssue(&st_Issue, 0, -1, -1, st_Message + f_Ui_StatusText(enStatus, 0));
					isOk = 0;
				}
			}
		}

		if (isOk != 0)
		{
			isOk = f_ComputePlotPoints(st_NewSample, st_NewPoint, nNewSampleNum, nNewObjectNum, &st_Issue);
		}

		// 성공했을 때만 이전 결과와 바꾼다. 표와 그림이 옛 버퍼를 가리키지 않게 먼저 끊는다.
		if (isOk != 0)
		{
			st_Plot.f_Clear();
			(VOID)st_ResultList.SetItemCountEx(0, LVSICF_NOINVALIDATEALL);
			free(st_SampleBuf);
			free(st_PointBuf);

			st_SampleBuf	= st_NewSample;
			st_PointBuf		= st_NewPoint;
			st_NewSample	= nullptr;
			st_NewPoint		= nullptr;
			nSampleNum		= nNewSampleNum;
			nObjectNum		= nNewObjectNum;
			resultStepTime	= st_RunConfig.stepTime;
			nCurStep		= 0;

			f_SetupResultList();
			st_Plot.f_SetData(st_PointBuf, st_SampleBuf, nSampleNum, nObjectNum);
			st_TimeSlider.SetRange(0, nSampleNum - 1, TRUE);
			(VOID)st_TimeSlider.SetLineSize(1);
			(VOID)st_TimeSlider.SetPageSize(((nSampleNum - 1) / 10 > 1) ? ((nSampleNum - 1) / 10) : 1);
			f_EnableResultControls(1);
			isConfigChanged = 0;
			f_ShowStep(0, UI_SYNC_NONE);
		}

		free(st_NewSample);
		free(st_NewPoint);
	}

	if (isOk != 0)
	{
		(VOID)f_Num_FormatFixed(pt_Duration, UI_CELL_SIZE, st_RunConfig.durationTime, 3);
		(VOID)f_Num_FormatFixed(pt_Step, UI_CELL_SIZE, st_RunConfig.stepTime, 3);
		st_Message.Format(_T("실행 완료: 표적 %d, 스텝 %d (%s s / %s s), 표본 %d"), st_RunConfig.nTargetNum, nSampleNum - 1,
			CString(pt_Duration).GetString(), CString(pt_Step).GetString(), nSampleNum);
		f_SetStatus(st_Message);
	}
	else
	{
		f_ReportIssue(&st_Issue);
	}
}

INT32 CTargetSimUIDlg::f_ComputePlotPoints(const ST_SimSample *st_Sample, ST_PlotPoint *st_Point, INT32 nNewSampleNum, INT32 nNewObjectNum,
	ST_UiIssue *st_Issue) const
{
	const ST_TargetState	*st_Origin = &st_Sample[0].st_Platform;
	const ST_TargetState	*st_State;
	ST_Matrix				st_Dcm;
	ST_CoordRect			st_Diff;
	ST_CoordRect			st_Ned;
	CString					st_Message;
	INT32					isOk = 1;
	INT32					nStep;
	INT32					nObject;

	// 기준 DCM 은 한 번만 만든다. 점마다 f_Trans_Ecef_To_Ned 를 부르면 매번 새로 만든다.
	if ((f_Ui_IsStateFinite(st_Origin) == 0) || (f_Coord_Dcm_Ned_To_Ecef(&st_Dcm, st_Origin->st_Lla.lat, st_Origin->st_Lla.lon) != COORD_OK))
	{
		st_Message = _T("스텝 0 플랫폼: 결과에 유한하지 않은 값이 있거나 기준 좌표를 만들 수 없습니다");
		isOk = 0;
	}

	for (nStep = 0; (nStep < nNewSampleNum) && (isOk != 0); nStep++)
	{
		for (nObject = 0; (nObject < nNewObjectNum) && (isOk != 0); nObject++)
		{
			st_State = (nObject == 0) ? &st_Sample[nStep].st_Platform : &st_Sample[nStep].st_Target[nObject - 1];

			// 표·CSV·그림에 nan, inf 가 나오지 않게 하는 마지막 방어선
			if (f_Ui_IsStateFinite(st_State) == 0)
			{
				st_Message.Format(_T("스텝 %d %s: 결과에 유한하지 않은 값이 있어 버렸습니다"), nStep,
					(nObject == 0) ? _T("플랫폼") : f_Ui_Location(0, nObject - 1, -1).GetString());
				isOk = 0;
			}
			else if ((f_Coord_VecSub(&st_Diff, &st_State->st_PosEcef, &st_Origin->st_PosEcef) != COORD_OK) ||
					 (f_Coord_RotateVecInv(&st_Ned, &st_Dcm, &st_Diff) != COORD_OK))
			{
				st_Message.Format(_T("스텝 %d: 그림 좌표 변환에 실패했습니다"), nStep);
				isOk = 0;
			}
			else
			{
				st_Point[(nStep * nNewObjectNum) + nObject].east	= st_Ned.y;
				st_Point[(nStep * nNewObjectNum) + nObject].north	= st_Ned.x;
			}
		}
	}

	if (isOk == 0)
	{
		f_Ui_SetIssue(st_Issue, 0, -1, -1, st_Message);
	}

	return isOk;
}

VOID CTargetSimUIDlg::f_SetupResultList(VOID)
{
	static const LPCTSTR	s_PartName[3] = { _T(" 위도 [deg]"), _T(" 경도 [deg]"), _T(" 고도 [m]") };
	static const INT32		s_PartDlu[3] = { 70, 70, 60 };
	CHeaderCtrl				*st_Header = st_ResultList.GetHeaderCtrl();
	CString					st_Object;
	INT32					nColumn;
	INT32					nObject;
	INT32					nPart;

	isSyncing = 1;

	if (st_Header != nullptr)
	{
		for (nColumn = st_Header->GetItemCount() - 1; nColumn >= 0; nColumn--)
		{
			(VOID)st_ResultList.DeleteColumn(nColumn);
		}
	}

	// 열 순서는 CSV 와 같다.
	(VOID)st_ResultList.InsertColumn(0, _T("스텝"), LVCFMT_LEFT, f_Ui_DluToPixel(this, 32));
	(VOID)st_ResultList.InsertColumn(1, _T("시각 [s]"), LVCFMT_RIGHT, f_Ui_DluToPixel(this, 40));

	for (nObject = 0; nObject < nObjectNum; nObject++)
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
			(VOID)st_ResultList.InsertColumn(2 + (nObject * 3) + nPart, st_Object + s_PartName[nPart], LVCFMT_RIGHT,
				f_Ui_DluToPixel(this, s_PartDlu[nPart]));
		}
	}

	(VOID)st_ResultList.SetItemCountEx(nSampleNum, LVSICF_NOINVALIDATEALL);
	st_ResultList.Invalidate();

	isSyncing = 0;
}

INT32 CTargetSimUIDlg::f_FormatCell(INT32 nRow, INT32 nColumn, CHAR *pt_Buf, INT32 bufSize) const
{
	const ST_SimSample		*st_Sample;
	const ST_TargetState	*st_State;
	INT32					nLength = -1;
	INT32					nObject;
	INT32					nPart;

	if ((pt_Buf != nullptr) && (bufSize > 0))
	{
		pt_Buf[0] = '\0';
	}

	if ((pt_Buf != nullptr) && (bufSize > 1) && (st_SampleBuf != nullptr) && (nRow >= 0) && (nRow < nSampleNum) &&
		(nColumn >= 0) && (nColumn < (2 + (3 * nObjectNum))))
	{
		st_Sample = &st_SampleBuf[nRow];

		if (nColumn == 0)
		{
			nLength = _snprintf_s(pt_Buf, static_cast<UINT64>(bufSize), _TRUNCATE, "%d", st_Sample->nStepIndex);
		}
		else if (nColumn == 1)
		{
			nLength = f_Num_FormatFixed(pt_Buf, bufSize, st_Sample->simTime, 3);
		}
		else
		{
			nObject		= (nColumn - 2) / 3;
			nPart		= (nColumn - 2) % 3;
			st_State	= (nObject == 0) ? &st_Sample->st_Platform : &st_Sample->st_Target[nObject - 1];

			if (nPart == 0)
			{
				nLength = f_Num_FormatFixed(pt_Buf, bufSize, f_Rad_To_Deg(st_State->st_Lla.lat), 9);
			}
			else if (nPart == 1)
			{
				nLength = f_Num_FormatFixed(pt_Buf, bufSize, f_Rad_To_Deg(st_State->st_Lla.lon), 9);
			}
			else
			{
				nLength = f_Num_FormatFixed(pt_Buf, bufSize, st_State->st_Lla.alt, 4);
			}
		}
	}

	return nLength;
}

VOID CTargetSimUIDlg::f_OnResultGetDispInfo(NMHDR *st_Hdr, LRESULT *pt_Result)
{
	NMLVDISPINFO	*st_Info = reinterpret_cast<NMLVDISPINFO *>(st_Hdr);
	CHAR			pt_Cell[UI_CELL_SIZE];

	if (((st_Info->item.mask & LVIF_TEXT) != 0U) && (st_Info->item.pszText != nullptr) && (st_Info->item.cchTextMax > 0))
	{
		st_Info->item.pszText[0] = L'\0';

		if (f_FormatCell(st_Info->item.iItem, st_Info->item.iSubItem, pt_Cell, UI_CELL_SIZE) > 0)
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

INT32 CTargetSimUIDlg::f_WriteCsv(const CString &st_Path, INT32 *pt_LineNum) const
{
	const INT32		nColumnNum = 2 + (3 * nObjectNum);
	FILE			*st_File = nullptr;
	CHAR			pt_Cell[UI_CELL_SIZE];
	INT32			errorCode;
	INT32			nLineNum = 0;
	INT32			nObject;
	INT32			nRow;
	INT32			nColumn;

	errorCode = _wfopen_s(&st_File, st_Path.GetString(), _T("wb"));

	if ((errorCode == 0) && (st_File != nullptr))
	{
		(VOID)setvbuf(st_File, nullptr, _IOFBF, UI_CSV_BUFFER_SIZE);

		// 가로형: 한 줄이 한 시각이라 최악 100002 줄로 엑셀 행 한계 안에 든다.
		(VOID)fputs("step,time_s", st_File);

		for (nObject = 0; nObject < nObjectNum; nObject++)
		{
			if (nObject == 0)
			{
				(VOID)fputs(",platform_lat_deg,platform_lon_deg,platform_alt_m", st_File);
			}
			else
			{
				(VOID)fprintf(st_File, ",target%d_lat_deg,target%d_lon_deg,target%d_alt_m", nObject, nObject, nObject);
			}
		}

		(VOID)fputs("\r\n", st_File);
		nLineNum = 1;

		for (nRow = 0; nRow < nSampleNum; nRow++)
		{
			for (nColumn = 0; nColumn < nColumnNum; nColumn++)
			{
				if (nColumn > 0)
				{
					(VOID)fputc(',', st_File);
				}

				if (f_FormatCell(nRow, nColumn, pt_Cell, UI_CELL_SIZE) > 0)
				{
					(VOID)fputs(pt_Cell, st_File);
				}
			}

			(VOID)fputs("\r\n", st_File);
			nLineNum = nLineNum + 1;
		}

		if (ferror(st_File) != 0)
		{
			errorCode = (errno != 0) ? errno : EIO;
		}

		if ((fclose(st_File) != 0) && (errorCode == 0))
		{
			errorCode = (errno != 0) ? errno : EIO;
		}

		// 반쯤 쓴 파일을 남기지 않는다.
		if (errorCode != 0)
		{
			(VOID)_wremove(st_Path.GetString());
		}
	}
	else if (errorCode == 0)
	{
		errorCode = EIO;
	}
	else
	{
		nLineNum = 0;
	}

	*pt_LineNum = nLineNum;

	return errorCode;
}

VOID CTargetSimUIDlg::f_OnCsvBrowseClicked(VOID)
{
	CString st_Path;

	f_StopPlay();
	(VOID)GetDlgItemText(IDC_CSV_PATH, st_Path);

	CFileDialog st_FileDlg(FALSE, _T("csv"), st_Path, OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY,
		_T("CSV (*.csv)|*.csv||"), this);

	if (st_FileDlg.DoModal() == IDOK)
	{
		SetDlgItemText(IDC_CSV_PATH, st_FileDlg.GetPathName());
	}
}

VOID CTargetSimUIDlg::f_OnSaveCsvClicked(VOID)
{
	CString	st_Path;
	CString	st_Name;
	CString	st_Message;
	INT32	nLineNum = 0;
	INT32	errorCode;

	f_StopPlay();
	(VOID)GetDlgItemText(IDC_CSV_PATH, st_Path);
	(VOID)st_Path.Trim();

	if (st_Path.IsEmpty())
	{
		f_OnCsvBrowseClicked();
		(VOID)GetDlgItemText(IDC_CSV_PATH, st_Path);
		(VOID)st_Path.Trim();
	}

	if ((nSampleNum <= 0) || st_Path.IsEmpty())
	{
		f_SetStatus(CString(_T("저장할 결과나 CSV 경로가 없습니다.")));
	}
	else if (f_Ui_IsAbsolutePath(st_Path) == 0)
	{
		st_Message = CString(_T("CSV 경로는 절대 경로여야 합니다 (예: C:\\temp\\TargetSim_LLA.csv): ")) + st_Path;
		f_SetStatus(st_Message);
		(VOID)AfxMessageBox(st_Message, MB_OK | MB_ICONWARNING);
	}
	else
	{
		{
			CWaitCursor st_Wait;

			errorCode = f_WriteCsv(st_Path, &nLineNum);
		}

		// 긴 경로는 상태 줄에서 잘리므로 파일 이름만 보이고, 전체 경로는 경로 칸에 남아 있다.
		st_Name = st_Path.Mid(st_Path.ReverseFind(_T('\\')) + 1);

		if (errorCode == 0)
		{
			st_Message.Format(_T("CSV 저장: %s (%d 줄)"), st_Name.GetString(), nLineNum);
			f_SetStatus(st_Message);
		}
		else
		{
			st_Message.Format(_T("CSV 를 쓰지 못했습니다 (errno %d): %s"), errorCode, st_Path.GetString());
			f_SetStatus(st_Message);
			(VOID)AfxMessageBox(st_Message, MB_OK | MB_ICONWARNING);
		}
	}
}

// ---------------------------------------------------------------------------
// 재생·표시

VOID CTargetSimUIDlg::f_ShowStep(INT32 nStep, INT32 syncSource)
{
	CHAR	pt_Now[UI_CELL_SIZE];
	CHAR	pt_End[UI_CELL_SIZE];
	CString	st_Text;
	INT32	nShow = nStep;

	// 현재 시각을 바꾸는 유일한 경로. 호출한 쪽 컨트롤은 다시 건드리지 않아 되먹임이 없다.
	if ((nSampleNum > 0) && (st_SampleBuf != nullptr))
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

		if ((f_Num_FormatFixed(pt_Now, UI_CELL_SIZE, st_SampleBuf[nShow].simTime, 3) > 0) &&
			(f_Num_FormatFixed(pt_End, UI_CELL_SIZE, st_SampleBuf[nSampleNum - 1].simTime, 3) > 0))
		{
			st_Text.Format(_T("t = %s / %s s"), CString(pt_Now).GetString(), CString(pt_End).GetString());
			SetDlgItemText(IDC_TIME_TEXT, st_Text);
		}

		if (syncSource != UI_SYNC_TABLE)
		{
			isSyncing = 1;
			(VOID)st_ResultList.SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
			(VOID)st_ResultList.SetItemState(nShow, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
			(VOID)st_ResultList.EnsureVisible(nShow, FALSE);
			isSyncing = 0;
		}

		st_Plot.f_SetStep(nShow);
	}
}

VOID CTargetSimUIDlg::f_StartPlay(VOID)
{
	if ((nSampleNum > 1) && (resultStepTime > 0.0))
	{
		if (nCurStep >= (nSampleNum - 1))
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
		SetDlgItemText(IDC_PLAY, _T("재생"));
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
	FLOAT64	advance;
	INT32	nStep;

	if ((timerId == UI_PLAY_TIMER_ID) && (isPlaying != 0) && (nSampleNum > 0) && (resultStepTime > 0.0))
	{
		// 실제 경과 시간이 아니라 틱 수로 진행해 재생 위치가 결정적이다.
		nPlayTick	= nPlayTick + 1;
		advance		= floor(((static_cast<FLOAT64>(nPlayTick) * static_cast<FLOAT64>(nPlaySpeed) * static_cast<FLOAT64>(UI_PLAY_TICK_MS)) /
						(1000.0 * resultStepTime)) + 1.0e-9);

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
	if ((st_ScrollBar != nullptr) && (nSampleNum > 0) && (st_ScrollBar->GetSafeHwnd() == st_TimeSlider.GetSafeHwnd()))
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

VOID CTargetSimUIDlg::f_SetStatus(const CString &st_Text)
{
	st_StatusText = st_Text;
	f_RefreshStatus();
}

VOID CTargetSimUIDlg::f_RefreshStatus(VOID)
{
	CString st_Text = st_StatusText;

	if ((isConfigChanged != 0) && (nSampleNum > 0))
	{
		st_Text += _T("\r\n설정이 바뀌었습니다. 표시 중인 결과는 마지막 실행 기준입니다.");
	}

	if (GetSafeHwnd() != nullptr)
	{
		SetDlgItemText(IDC_RUN_STATUS, st_Text);
	}
}
