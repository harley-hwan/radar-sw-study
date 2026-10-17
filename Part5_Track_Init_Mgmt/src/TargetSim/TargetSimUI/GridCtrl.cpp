#include "pch.h"

#include "GridCtrl.h"
#include "UiCommon.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define GRID_ID_EDIT			101
#define GRID_ID_COMBO			103
#define GRID_PAD_Y				6						// [px @96dpi] 행 위아래 여백
#define GRID_DROP_HEIGHT		160						// [px @96dpi] 목록 높이
#define GRID_WM_OPEN_CHOICE		(WM_APP + 21)

BEGIN_MESSAGE_MAP(CGridCtrl, CListCtrl)
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_SIZE()
	ON_WM_VSCROLL()
	ON_WM_MOUSEWHEEL()
	ON_NOTIFY_REFLECT(LVN_ITEMCHANGED, &CGridCtrl::f_OnItemChanged)
	ON_EN_KILLFOCUS(GRID_ID_EDIT, &CGridCtrl::f_OnEditKillFocus)
	ON_CBN_SELENDOK(GRID_ID_COMBO, &CGridCtrl::f_OnComboSelEndOk)
	ON_CBN_CLOSEUP(GRID_ID_COMBO, &CGridCtrl::f_OnComboCloseUp)
	ON_MESSAGE(GRID_WM_OPEN_CHOICE, &CGridCtrl::f_OnOpenChoice)
END_MESSAGE_MAP()

// 칸 직접 편집 표.
CGridCtrl::CGridCtrl() noexcept
	: st_Column(nullptr)
	, nColumnNum(0)
	, dpi(UI_BASE_DPI)
	, nRowHeight(24)
	, nEditRow(-1)
	, nEditColumn(-1)
{
}

// 열 정의, 화면 배율로 표 구성.
VOID CGridCtrl::f_Setup(const ST_GridColumn *st_NewColumn, INT32 nNewColumnNum, INT32 newDpi)
{
	CClientDC	st_Dc(this);
	CFont		*st_OldFont;
	TEXTMETRIC	st_Metric;
	INT32		nColumn;

	st_Column	= st_NewColumn;
	nColumnNum	= nNewColumnNum;
	dpi			= newDpi;

	(VOID)SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

	// 행 높이 설정 (이미지 목록 높이 사용).
	st_OldFont = st_Dc.SelectObject(GetFont());
	(VOID)st_Dc.GetTextMetrics(&st_Metric);
	(VOID)st_Dc.SelectObject(st_OldFont);
	nRowHeight = st_Metric.tmHeight + (2 * f_Ui_Scale(GRID_PAD_Y, dpi));
	(VOID)st_RowSizer.Create(1, nRowHeight, ILC_COLOR, 1, 1);
	(VOID)SetImageList(&st_RowSizer, LVSIL_SMALL);

	for (nColumn = 0; nColumn < nColumnNum; nColumn++)
	{
		(VOID)InsertColumn(nColumn, st_Column[nColumn].pt_Title, (st_Column[nColumn].kind == GRID_KIND_NUMBER) ? LVCFMT_RIGHT : LVCFMT_LEFT, 40);
	}

	// 열 너비는 f_FitColumns 에서 조정.
	(VOID)GetHeaderCtrl()->ModifyStyle(0, HDS_NOSIZING);
	f_FitColumns();
}

// 행 수 맞춤. 부족분 추가, 초과분 삭제.
VOID CGridCtrl::f_SetRowNum(INT32 nRowNum)
{
	INT32 nCount;

	f_EndEdit(0);

	for (nCount = GetItemCount(); nCount < nRowNum; nCount++)
	{
		(VOID)InsertItem(nCount, _T(""));
	}

	for (nCount = GetItemCount(); nCount > nRowNum; nCount--)
	{
		(VOID)DeleteItem(nCount - 1);
	}

	// 열 너비 다시 맞춤.
	f_FitColumns();
}

// 행 선택. 변경 알림은 f_OnItemChanged 담당.
VOID CGridCtrl::f_SelectRow(INT32 nRow)
{
	(VOID)SetItemState(nRow, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	(VOID)EnsureVisible(nRow, FALSE);
}

// 선택 행. 없으면 -1.
INT32 CGridCtrl::f_GetCurRow(VOID) const
{
	return GetNextItem(-1, LVNI_SELECTED);
}

// 행 nRowNum 개가 스크롤 없이 들어가는 높이. 행이 있으면 실제 행 높이 사용.
INT32 CGridCtrl::f_GetHeightForRows(INT32 nRowNum) const
{
	CRect st_Header;
	CRect st_Item(0, 0, 0, nRowHeight);

	GetHeaderCtrl()->GetWindowRect(&st_Header);

	if (GetItemCount() > 0)
	{
		(VOID)GetItemRect(0, &st_Item, LVIR_BOUNDS);
	}

	return st_Header.Height() + (nRowNum * st_Item.Height()) + 4;
}

// 칸 하나의 사각형 (머리글 위치 기준).
CRect CGridCtrl::f_GetCellRect(INT32 nRow, INT32 nColumn) const
{
	CRect st_Row;
	CRect st_HeaderItem;

	(VOID)GetItemRect(nRow, &st_Row, LVIR_BOUNDS);
	(VOID)GetHeaderCtrl()->GetItemRect(nColumn, &st_HeaderItem);

	return CRect(st_Row.left + st_HeaderItem.left, st_Row.top, st_Row.left + st_HeaderItem.right, st_Row.bottom);
}

// 열 너비를 비율대로 배분. 마지막 열이 나머지 차지.
VOID CGridCtrl::f_FitColumns(VOID)
{
	CRect	st_Client;
	INT32	totalWeight = 0;
	INT32	usedWidth = 0;
	INT32	width;
	INT32	nColumn;

	GetClientRect(&st_Client);

	for (nColumn = 0; nColumn < nColumnNum; nColumn++)
	{
		totalWeight = totalWeight + st_Column[nColumn].nWeight;
	}

	for (nColumn = 0; nColumn < nColumnNum; nColumn++)
	{
		width = (nColumn == (nColumnNum - 1)) ? (st_Client.Width() - usedWidth) : ::MulDiv(st_Client.Width(), st_Column[nColumn].nWeight, totalWeight);
		(VOID)SetColumnWidth(nColumn, width);
		usedWidth = usedWidth + width;
	}
}

// 부모에게 WM_NOTIFY 전송.
VOID CGridCtrl::f_Notify(UINT32 code, INT32 nRow, INT32 nColumn)
{
	ST_GridNotify st_Notify;

	st_Notify.st_Hdr.hwndFrom	= GetSafeHwnd();
	st_Notify.st_Hdr.idFrom		= static_cast<UINT_PTR>(GetDlgCtrlID());
	st_Notify.st_Hdr.code		= code;
	st_Notify.nRow				= nRow;
	st_Notify.nColumn			= nColumn;

	(VOID)GetParent()->SendMessage(WM_NOTIFY, static_cast<WPARAM>(GetDlgCtrlID()), reinterpret_cast<LPARAM>(&st_Notify));
}

// 편집

// 칸 위에 편집 컨트롤 표시. 숫자 칸은 편집칸, 목록 칸은 콤보.
VOID CGridCtrl::f_BeginEdit(INT32 nRow, INT32 nColumn)
{
	const INT32	padY = f_Ui_Scale(GRID_PAD_Y, dpi);
	CRect		st_Cell;
	INT32		nChoice;

	f_EndEdit(1);

	st_Cell		= f_GetCellRect(nRow, nColumn);
	nEditRow	= nRow;
	nEditColumn	= nColumn;

	if (st_Column[nColumn].kind == GRID_KIND_NUMBER)
	{
		const CRect st_EditRect(st_Cell.left + 3, st_Cell.top + padY, st_Cell.right - 3, st_Cell.bottom - padY);

		if (st_Edit.GetSafeHwnd() == nullptr)
		{
			(VOID)st_Edit.Create(WS_CHILD | ES_AUTOHSCROLL | ES_RIGHT, st_EditRect, this, GRID_ID_EDIT);
			st_Edit.SetFont(GetFont());
		}

		st_Edit.MoveWindow(&st_EditRect);
		st_Edit.SetWindowText(GetItemText(nRow, nColumn));
		(VOID)st_Edit.ShowWindow(SW_SHOW);
		(VOID)st_Edit.SetFocus();
		st_Edit.SetSel(0, -1);
	}
	else
	{
		const CRect st_ComboRect(st_Cell.left + 1, st_Cell.top, st_Cell.right - 1, st_Cell.bottom + f_Ui_Scale(GRID_DROP_HEIGHT, dpi));

		if (st_Combo.GetSafeHwnd() == nullptr)
		{
			(VOID)st_Combo.Create(WS_CHILD | WS_VSCROLL | CBS_DROPDOWNLIST, st_ComboRect, this, GRID_ID_COMBO);
			st_Combo.SetFont(GetFont());
		}

		st_Combo.ResetContent();

		for (nChoice = 0; nChoice < st_Column[nColumn].nChoiceNum; nChoice++)
		{
			(VOID)st_Combo.AddString(st_Column[nColumn].pt_Choice[nChoice]);
		}

		(VOID)st_Combo.SetCurSel(st_Combo.FindStringExact(-1, GetItemText(nRow, nColumn)));
		(VOID)st_Combo.SetItemHeight(-1, static_cast<UINT32>(st_Cell.Height() - 6));
		st_Combo.MoveWindow(&st_ComboRect);
		(VOID)st_Combo.ShowWindow(SW_SHOW);
		(VOID)st_Combo.SetFocus();
		st_Combo.ShowDropDown(TRUE);
	}
}

// 편집 종료. isCommit 이면 편집칸 글자 반영 후 부모 알림.
VOID CGridCtrl::f_EndEdit(INT32 isCommit)
{
	const INT32	nRow = nEditRow;
	const INT32	nColumn = nEditColumn;
	CWnd		*st_Editor;
	CString		st_NewText;

	if (nRow >= 0)
	{
		// 먼저 닫힘 상태로 설정 (재진입 방지).
		nEditRow	= -1;
		nEditColumn	= -1;
		st_Editor	= (st_Column[nColumn].kind == GRID_KIND_NUMBER) ? static_cast<CWnd *>(&st_Edit) : static_cast<CWnd *>(&st_Combo);

		// 숨기기 전 표로 포커스 이동.
		if (::GetFocus() == st_Editor->GetSafeHwnd())
		{
			(VOID)SetFocus();
		}

		(VOID)st_Editor->ShowWindow(SW_HIDE);

		if ((isCommit != 0) && (st_Column[nColumn].kind == GRID_KIND_NUMBER))
		{
			st_Edit.GetWindowText(st_NewText);
			(VOID)st_NewText.Trim();

			if (st_NewText != GetItemText(nRow, nColumn))
			{
				(VOID)SetItemText(nRow, nColumn, st_NewText);
				f_Notify(GRIDN_CELLCHANGED, nRow, nColumn);
			}
		}
	}
}

// 편집칸 포커스 잃으면 확정.
VOID CGridCtrl::f_OnEditKillFocus(VOID)
{
	f_EndEdit(1);
}

// 목록 선택값 반영 후 부모 알림.
VOID CGridCtrl::f_OnComboSelEndOk(VOID)
{
	CString st_NewText;

	if (nEditRow >= 0)
	{
		st_Combo.GetLBText(st_Combo.GetCurSel(), st_NewText);

		if (st_NewText != GetItemText(nEditRow, nEditColumn))
		{
			(VOID)SetItemText(nEditRow, nEditColumn, st_NewText);
			f_Notify(GRIDN_CELLCHANGED, nEditRow, nEditColumn);
		}
	}
}

// 목록 닫히면 편집 종료.
VOID CGridCtrl::f_OnComboCloseUp(VOID)
{
	f_EndEdit(1);
}

// 클릭 후 목록 펼침 (OnLButtonDown 에서 게시).
LRESULT CGridCtrl::f_OnOpenChoice(WPARAM wParam, LPARAM lParam)
{
	f_BeginEdit(static_cast<INT32>(wParam), static_cast<INT32>(lParam));

	return 0;
}

// 입력

// 누른 칸의 행 선택. 목록 칸이면 콤보 표시.
VOID CGridCtrl::OnLButtonDown(UINT32 flags, CPoint st_Point)
{
	LVHITTESTINFO st_Hit;

	UNREFERENCED_PARAMETER(flags);

	f_EndEdit(1);
	(VOID)SetFocus();

	(VOID)memset(&st_Hit, 0, sizeof(st_Hit));
	st_Hit.pt = st_Point;

	if (SubItemHitTest(&st_Hit) >= 0)
	{
		f_SelectRow(st_Hit.iItem);

		// 목록 칸은 클릭이 끝난 뒤 열기.
		if (st_Column[st_Hit.iSubItem].kind == GRID_KIND_CHOICE)
		{
			(VOID)PostMessage(GRID_WM_OPEN_CHOICE, static_cast<WPARAM>(st_Hit.iItem), static_cast<LPARAM>(st_Hit.iSubItem));
		}
	}
}

// 숫자 칸 더블클릭 시 편집칸 열기.
VOID CGridCtrl::OnLButtonDblClk(UINT32 flags, CPoint st_Point)
{
	LVHITTESTINFO st_Hit;

	UNREFERENCED_PARAMETER(flags);

	(VOID)memset(&st_Hit, 0, sizeof(st_Hit));
	st_Hit.pt = st_Point;

	if ((SubItemHitTest(&st_Hit) >= 0) && (st_Column[st_Hit.iSubItem].kind == GRID_KIND_NUMBER))
	{
		f_BeginEdit(st_Hit.iItem, st_Hit.iSubItem);
	}
}

// 크기 변경 시 편집 종료, 열 너비 재조정.
VOID CGridCtrl::OnSize(UINT32 type, INT32 width, INT32 height)
{
	CListCtrl::OnSize(type, width, height);

	f_EndEdit(1);
	f_FitColumns();
}

// 스크롤 전 편집 종료.
VOID CGridCtrl::OnVScroll(UINT32 code, UINT32 pos, CScrollBar *st_ScrollBar)
{
	f_EndEdit(1);
	CListCtrl::OnVScroll(code, pos, st_ScrollBar);
}

// 휠 스크롤 전 편집 종료.
BOOL CGridCtrl::OnMouseWheel(UINT32 flags, SHORT delta, CPoint st_Point)
{
	f_EndEdit(1);

	return CListCtrl::OnMouseWheel(flags, delta, st_Point);
}

// 새 행 선택 시 부모 알림.
VOID CGridCtrl::f_OnItemChanged(NMHDR *st_Hdr, LRESULT *pt_Result)
{
	const NMLISTVIEW *st_Item = reinterpret_cast<NMLISTVIEW *>(st_Hdr);

	if (((st_Item->uChanged & LVIF_STATE) != 0U) && ((st_Item->uNewState & LVIS_SELECTED) != 0U) && ((st_Item->uOldState & LVIS_SELECTED) == 0U))
	{
		f_Notify(GRIDN_ROWCHANGED, st_Item->iItem, 0);
	}

	*pt_Result = 0;
}
