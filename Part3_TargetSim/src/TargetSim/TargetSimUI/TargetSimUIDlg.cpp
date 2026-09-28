#include "pch.h"
#include "framework.h"

#include "TargetSimUI.h"
#include "TargetSimUIDlg.h"
#include "UiCommon.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define UI_INIT_WIDTH			1400					// [px @96dpi]
#define UI_INIT_HEIGHT			880
#define UI_MIN_WIDTH			1180
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

static LPCTSTR			s_TurnChoice[SCN_TURN_TYPE_NUM];

// 컨트롤 위치, 크기 변경.
static VOID f_Ui_Move(CWnd *st_Dlg, INT32 ctrlId, INT32 left, INT32 top, INT32 width, INT32 height)
{
	(VOID)st_Dlg->GetDlgItem(ctrlId)->SetWindowPos(nullptr, left, top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

BEGIN_MESSAGE_MAP(CTargetSimUIDlg, CDialogEx)
	ON_WM_ERASEBKGND()
	ON_WM_CTLCOLOR()
	ON_WM_SIZE()
	ON_WM_GETMINMAXINFO()
	ON_WM_CLOSE()
	ON_BN_CLICKED(IDC_SCN_MENU, &CTargetSimUIDlg::f_OnScenarioMenuClicked)
	ON_COMMAND_RANGE(ID_SCN_PRESET_FIRST, ID_SCN_PRESET_FIRST + SCN_PRESET_NUM - 1, &CTargetSimUIDlg::f_OnScenarioPreset)
	ON_BN_CLICKED(IDC_OBJ_ADD, &CTargetSimUIDlg::f_OnTargetAddClicked)
	ON_BN_CLICKED(IDC_OBJ_DELETE, &CTargetSimUIDlg::f_OnTargetDeleteClicked)
	ON_BN_CLICKED(IDC_MNV_ADD, &CTargetSimUIDlg::f_OnManeuverAddClicked)
	ON_BN_CLICKED(IDC_MNV_DELETE, &CTargetSimUIDlg::f_OnManeuverDeleteClicked)
	ON_BN_CLICKED(IDC_SAVE_CSV, &CTargetSimUIDlg::f_OnSaveCsvClicked)
	ON_NOTIFY(GRIDN_CELLCHANGED, IDC_OBJ_GRID, &CTargetSimUIDlg::f_OnObjCellChanged)
	ON_NOTIFY(GRIDN_ROWCHANGED, IDC_OBJ_GRID, &CTargetSimUIDlg::f_OnObjRowChanged)
	ON_NOTIFY(GRIDN_CELLCHANGED, IDC_MNV_GRID, &CTargetSimUIDlg::f_OnMnvCellChanged)
	ON_NOTIFY(LVN_GETDISPINFO, IDC_RESULT_LIST, &CTargetSimUIDlg::f_OnResultGetDispInfo)
END_MESSAGE_MAP()

// 주 대화상자.
CTargetSimUIDlg::CTargetSimUIDlg(CWnd *st_Parent)
	: CDialogEx(IDD_TARGETSIMUI_DIALOG, st_Parent)
	, st_TopBar(0, 0, 0, 0)
	, st_CardObj(0, 0, 0, 0)
	, st_CardMnv(0, 0, 0, 0)
	, st_CardPlot(0, 0, 0, 0)
	, st_CardResult(0, 0, 0, 0)
	, dpi(UI_BASE_DPI)
	, textHeight(16)
	, nCurObject(1)
	, nResultColumnNum(0)
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
}

// 글꼴, 색, 표 준비 후 명세 시나리오 첫 실행.
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

	(VOID)st_ResultList.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

	st_Plot.f_SetDpi(dpi);
	f_SetupGrids();
	f_SetInitialSize();
	f_LoadPreset(SCN_PRESET_SPEC);

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

	// 위 막대
	st_TopBar.SetRect(0, 0, st_Client.right, buttonHeight + (2 * f_Ui_Scale(10, dpi)));
	x = margin + f_Ui_Scale(110, dpi) + (2 * gap);
	y = f_Ui_Scale(10, dpi);
	f_Ui_Move(this, IDC_SCN_MENU, margin, y, f_Ui_Scale(110, dpi), buttonHeight);
	f_Ui_Move(this, IDC_SCN_NAME, x, y, st_Client.right - margin - x, buttonHeight);

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

	// 기동 카드 (아래에 도움말 두 줄)
	st_CardMnv.SetRect(margin, st_CardObj.bottom + gap, margin + leftWidth, contentBottom);
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

	// 그림 카드
	st_CardPlot.SetRect(rightLeft, contentTop, rightLeft + rightWidth, contentTop + static_cast<INT32>(static_cast<FLOAT64>(contentBottom - contentTop) * UI_PLOT_RATIO));
	f_Ui_Move(this, IDC_PLOT, st_CardPlot.left + pad, st_CardPlot.top + pad, st_CardPlot.Width() - (2 * pad), st_CardPlot.Height() - (2 * pad));

	// 결과 카드
	st_CardResult.SetRect(rightLeft, st_CardPlot.bottom + gap, rightLeft + rightWidth, contentBottom);
	y = st_CardResult.top + pad;
	f_Ui_Move(this, IDC_SAVE_CSV, st_CardResult.right - pad - f_Ui_Scale(110, dpi), y, f_Ui_Scale(110, dpi), buttonHeight);
	f_Ui_Move(this, IDC_RESULT_TITLE, st_CardResult.left + pad, y, st_CardResult.Width() - (2 * pad) - f_Ui_Scale(110, dpi) - tight, buttonHeight);
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
}

// Esc: 창 유지, 입력 중인 값 취소.
VOID CTargetSimUIDlg::OnCancel(VOID)
{
	st_ObjGrid.f_EndEdit(0);
	st_MnvGrid.f_EndEdit(0);
}

// 닫기 단추. 입력 중인 값은 버리고 창 닫기.
VOID CTargetSimUIDlg::OnClose(VOID)
{
	OnCancel();
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
	st_Name.Format(_T("%s  ·  시간 %g s, 간격 %g s"), CScenario::f_PresetName(nPreset), SCN_DURATION_TIME, SCN_STEP_TIME);
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

// 편집 글자로 설정 생성, 전 구간 실행 후 결과 표와 그림 갱신.
VOID CTargetSimUIDlg::f_RunScenario(VOID)
{
	ST_SimConfig st_Config = {};

	st_Scenario.f_BuildConfig(&st_Config);
	st_Result.f_Run(&st_Config);

	f_SetupResultList();
	st_Plot.f_SetResult(&st_Result);
}

// 결과 표: 열은 객체 수 기준, 행은 표본 수. 열 순서는 CSV 와 동일.
VOID CTargetSimUIDlg::f_SetupResultList(VOID)
{
	static const LPCTSTR	s_PartName[3] = { _T(" 위도 [°]"), _T(" 경도 [°]"), _T(" 고도 [m]") };
	static const INT32		s_PartWidth[3] = { 124, 128, 112 };
	const INT32				nColumnNum = 2 + (3 * st_Result.f_GetObjectNum());
	CString					st_Object;
	INT32					nColumn;
	INT32					nObject;
	INT32					nPart;

	// 객체 수 변경 시에만 열 재생성.
	if (nColumnNum != nResultColumnNum)
	{
		for (nColumn = nResultColumnNum - 1; nColumn >= 0; nColumn--)
		{
			(VOID)st_ResultList.DeleteColumn(nColumn);
		}

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

		nResultColumnNum = nColumnNum;
	}

	(VOID)st_ResultList.SetItemCountEx(st_Result.f_GetSampleNum(), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
	st_ResultList.Invalidate(FALSE);
}

// 가상 리스트 요청 칸의 글자 제공. 보이는 행만 요청됨.
VOID CTargetSimUIDlg::f_OnResultGetDispInfo(NMHDR *st_Hdr, LRESULT *pt_Result)
{
	NMLVDISPINFO	*st_Info = reinterpret_cast<NMLVDISPINFO *>(st_Hdr);
	CHAR			pt_Cell[RES_CELL_SIZE];

	if ((st_Info->item.mask & LVIF_TEXT) != 0U)
	{
		st_Result.f_FormatCell(st_Info->item.iItem, st_Info->item.iSubItem, pt_Cell, RES_CELL_SIZE);
		(VOID)MultiByteToWideChar(CP_UTF8, 0, pt_Cell, -1, st_Info->item.pszText, st_Info->item.cchTextMax);
	}

	*pt_Result = 0;
}

// 결과 CSV 저장. 입력 중인 값은 단추 클릭 시 확정되어 이미 결과에 반영됨.
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
