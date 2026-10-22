// 트랙 스코어 확인 창 구현. Core 의 f_Trk_UpdateState 를 그대로 부름
#include "pch.h"
#include "framework.h"

#include "Resource.h"
#include "ScoreDlg.h"
#include "UiCommon.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define SCORE_COL_SCAN			0
#define SCORE_COL_FLAG			1
#define SCORE_COL_SCORE			2
#define SCORE_COL_MEMORY		3
#define SCORE_COL_STATE			4
#define SCORE_COL_NOTE			5
#define SCORE_COL_NUM			6

// 예시 Assigned_Flag 순서와 이름
static const LPCTSTR	s_PresetName[SCORE_PRESET_NUM] =
{
	_T("과제 조건 (늘 0)"), _T("임시에서 확정"), _T("확정, 메모리, 다시 확정"), _T("메모리에서 삭제"), _T("임시에서 삭제")
};
static const LPCTSTR	s_PresetFlag[SCORE_PRESET_NUM] =
{
	_T("0 0 0"), _T("1 1 1"), _T("1 1 1 0 0 1 1"), _T("1 1 1 0 0 0 0 0"), _T("1 0 0 0")
};

// 트랙 상태 이름.
static LPCTSTR f_Score_StateName(INT32 nState)
{
	static const LPCTSTR s_Name[5] = { _T("-"), _T("1 임시"), _T("2 확정"), _T("3 메모리"), _T("4 삭제") };

	return ((nState >= TRK_STATE_TENTATIVE) && (nState <= TRK_STATE_DELETED)) ? s_Name[nState] : s_Name[0];
}

// 상태 줄 바탕색 (그림의 트랙 기호 색을 옅게).
static COLORREF f_Score_StateBack(INT32 nState)
{
	COLORREF color = RGB(243, 244, 246);

	if (nState == TRK_STATE_TENTATIVE)
	{
		color = RGB(255, 237, 213);
	}
	else if (nState == TRK_STATE_CONFIRMED)
	{
		color = RGB(220, 252, 231);
	}
	else if (nState == TRK_STATE_MEMORY)
	{
		color = RGB(243, 232, 255);
	}
	else
	{
		// 삭제
	}

	return color;
}

// 한 스캔의 상태 변화 설명.
static CString f_Score_Note(const ST_Track *st_Before, const ST_Track *st_After)
{
	CString st_Note;

	if (st_Before->TrackState == st_After->TrackState)
	{
		st_Note = (st_After->TrackState == TRK_STATE_MEMORY) ? _T("메모리 유지 (MemoryScore +1)") : _T("상태 유지");
	}
	else if ((st_After->TrackState == TRK_STATE_CONFIRMED) && (st_Before->TrackState == TRK_STATE_MEMORY))
	{
		st_Note = _T("연관 성공, 바로 확정 (MemoryScore 0)");
	}
	else if (st_After->TrackState == TRK_STATE_CONFIRMED)
	{
		st_Note.Format(_T("TrackScore %d 이상, 확정"), TRK_SCORE_CONFIRM);
	}
	else if (st_After->TrackState == TRK_STATE_MEMORY)
	{
		st_Note = _T("연관 실패, 메모리로 (MemoryScore +1)");
	}
	else if (st_Before->TrackState == TRK_STATE_MEMORY)
	{
		st_Note.Format(_T("MemoryScore %d 이상, 삭제"), TRK_MEMORY_DELETE);
	}
	else
	{
		st_Note.Format(_T("TrackScore %d 이하, 삭제"), TRK_SCORE_DELETE);
	}

	return st_Note;
}

BEGIN_MESSAGE_MAP(CScoreDlg, CDialogEx)
	ON_EN_CHANGE(IDC_SCORE_FLAGS, &CScoreDlg::f_OnFlagsChanged)
	ON_CBN_SELCHANGE(IDC_SCORE_PRESET, &CScoreDlg::f_OnPresetChanged)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_SCORE_LIST, &CScoreDlg::f_OnListCustomDraw)
END_MESSAGE_MAP()

// 트랙 스코어 확인 창.
CScoreDlg::CScoreDlg(CWnd *st_Parent)
	: CDialogEx(IDD_SCORE_DIALOG, st_Parent)
	, nRowState()
	, nRowNum(0)
{
}

// 컨트롤 변수와 대화상자 자원 연결.
VOID CScoreDlg::DoDataExchange(CDataExchange *st_Dx)
{
	CDialogEx::DoDataExchange(st_Dx);
	DDX_Control(st_Dx, IDC_SCORE_LIST, st_List);
	DDX_Control(st_Dx, IDC_SCORE_PRESET, st_Preset);
}

// 표 열, 예시 목록, 규칙 글자 준비 후 '확정, 메모리, 다시 확정' 예시로 시작.
BOOL CScoreDlg::OnInitDialog(VOID)
{
	static const LPCTSTR	s_Column[SCORE_COL_NUM] = { _T("스캔"), _T("Assigned_Flag"), _T("TrackScore"), _T("MemoryScore"), _T("TrackState"), _T("바뀐 것") };
	static const INT32		s_Width[SCORE_COL_NUM] = { 44, 92, 80, 92, 80, 230 };
	CClientDC				st_Dc(this);
	const INT32				dpi = st_Dc.GetDeviceCaps(LOGPIXELSX);
	CString					st_Rule;
	INT32					nIndex;

	(VOID)CDialogEx::OnInitDialog();

	(VOID)st_List.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

	for (nIndex = 0; nIndex < SCORE_COL_NUM; nIndex++)
	{
		(VOID)st_List.InsertColumn(nIndex, s_Column[nIndex], (nIndex == SCORE_COL_NOTE) || (nIndex == SCORE_COL_STATE) ? LVCFMT_LEFT : LVCFMT_RIGHT,
			f_Ui_Scale(s_Width[nIndex], dpi));
	}

	for (nIndex = 0; nIndex < SCORE_PRESET_NUM; nIndex++)
	{
		(VOID)st_Preset.AddString(s_PresetName[nIndex]);
	}

	st_Rule.Format(_T("개시: 두 플롯으로 만든 트랙은 임시 (1), TrackScore %d 에서 시작\r\n")
		_T("임시: 연관 성공 +1, 실패 -1. TrackScore %d 이상이면 확정 (2), %d 이하면 삭제 (4)\r\n")
		_T("확정: 연관 실패면 메모리 (3), MemoryScore +1\r\n")
		_T("메모리: 연관 성공이면 바로 확정 (MemoryScore 0), 실패면 MemoryScore +1, %d 이상이면 삭제 (4)"),
		TRK_SCORE_INIT, TRK_SCORE_CONFIRM, TRK_SCORE_DELETE, TRK_MEMORY_DELETE);
	SetDlgItemText(IDC_SCORE_RULE, st_Rule);

	(VOID)st_Preset.SetCurSel(2);
	SetDlgItemText(IDC_SCORE_FLAGS, s_PresetFlag[2]);					// EN_CHANGE 로 f_Run

	return TRUE;
}

// 예시 고르면 입력 칸을 바꿈 (바뀐 입력으로 다시 계산).
VOID CScoreDlg::f_OnPresetChanged(VOID)
{
	const INT32 nSel = st_Preset.GetCurSel();

	if ((nSel >= 0) && (nSel < SCORE_PRESET_NUM))
	{
		SetDlgItemText(IDC_SCORE_FLAGS, s_PresetFlag[nSel]);
	}
}

// 입력이 바뀌면 바로 다시 계산.
VOID CScoreDlg::f_OnFlagsChanged(VOID)
{
	f_Run();
}

// 입력한 0, 1 을 차례로 Assigned_Flag 로 넣어 f_Trk_UpdateState (삭제되면 멈춤)
VOID CScoreDlg::f_Run(VOID)
{
	ST_Track	st_Track = {};
	ST_Track	st_Before;
	CString		st_Text;
	CString		st_Cell;
	INT32		nChar;
	INT32		nRow;

	(VOID)GetDlgItemText(IDC_SCORE_FLAGS, st_Text);
	(VOID)st_List.DeleteAllItems();

	// 줄 0: 개시 직후
	st_Track.TrackState		= TRK_STATE_TENTATIVE;
	st_Track.TrackScore		= TRK_SCORE_INIT;
	st_Track.MemoryScore	= 0;
	nRow					= 0;
	nRowState[0]			= st_Track.TrackState;

	(VOID)st_List.InsertItem(0, _T("0"));
	st_List.SetItemText(0, SCORE_COL_FLAG, _T("-"));
	st_Cell.Format(_T("%d"), st_Track.TrackScore);
	st_List.SetItemText(0, SCORE_COL_SCORE, st_Cell);
	st_List.SetItemText(0, SCORE_COL_MEMORY, _T("0"));
	st_List.SetItemText(0, SCORE_COL_STATE, f_Score_StateName(st_Track.TrackState));
	st_List.SetItemText(0, SCORE_COL_NOTE, _T("2 Point 개시"));

	for (nChar = 0; (nChar < st_Text.GetLength()) && (nRow < SCORE_MAX_FLAG) && (st_Track.TrackState != TRK_STATE_DELETED); nChar++)
	{
		if ((st_Text[nChar] == _T('0')) || (st_Text[nChar] == _T('1')))
		{
			st_Before				= st_Track;
			st_Track.Assigned_Flag	= (st_Text[nChar] == _T('1')) ? 1 : 0;
			f_Trk_UpdateState(&st_Track);
			nRow					= nRow + 1;
			nRowState[nRow]			= st_Track.TrackState;

			st_Cell.Format(_T("%d"), nRow);
			(VOID)st_List.InsertItem(nRow, st_Cell);
			st_Cell.Format(_T("%d"), st_Track.Assigned_Flag);
			st_List.SetItemText(nRow, SCORE_COL_FLAG, st_Cell);
			st_Cell.Format(_T("%d"), st_Track.TrackScore);
			st_List.SetItemText(nRow, SCORE_COL_SCORE, st_Cell);
			st_Cell.Format(_T("%d"), st_Track.MemoryScore);
			st_List.SetItemText(nRow, SCORE_COL_MEMORY, st_Cell);
			st_List.SetItemText(nRow, SCORE_COL_STATE, f_Score_StateName(st_Track.TrackState));
			st_List.SetItemText(nRow, SCORE_COL_NOTE, f_Score_Note(&st_Before, &st_Track));
		}
	}

	nRowNum = nRow + 1;
}

// 줄 바탕을 트랙 상태 색으로.
VOID CScoreDlg::f_OnListCustomDraw(NMHDR *st_Hdr, LRESULT *pt_Result)
{
	NMLVCUSTOMDRAW	*st_Draw = reinterpret_cast<NMLVCUSTOMDRAW *>(st_Hdr);
	const INT32		nRow = static_cast<INT32>(st_Draw->nmcd.dwItemSpec);

	*pt_Result = CDRF_DODEFAULT;

	if (st_Draw->nmcd.dwDrawStage == CDDS_PREPAINT)
	{
		*pt_Result = CDRF_NOTIFYITEMDRAW;
	}
	else if ((st_Draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) && (nRow >= 0) && (nRow < nRowNum))
	{
		st_Draw->clrTextBk	= f_Score_StateBack(nRowState[nRow]);
		st_Draw->clrText	= UI_COLOR_TEXT;
	}
	else
	{
		// 기본 그리기
	}
}
