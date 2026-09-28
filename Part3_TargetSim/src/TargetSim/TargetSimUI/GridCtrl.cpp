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

// 칸을 바로 고치는 표.
CGridCtrl::CGridCtrl() noexcept
	: st_Column(nullptr)
	, nColumnNum(0)
	, dpi(UI_BASE_DPI)
	, nRowHeight(24)
	, nEditRow(-1)
	, nEditColumn(-1)
{
}

// 열 정의와 화면 배율을 받아 표를 갖춘다.
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

	// 행 높이는 작은 이미지 목록 높이로 정해진다. 편집칸이 들어갈 만큼 위아래 여백을 둔다.
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

	// 열 너비는 f_FitColumns 가 맞춘다.
	(VOID)GetHeaderCtrl()->ModifyStyle(0, HDS_NOSIZING);
	f_FitColumns();
}

// 행 수를 맞춘다. 모자라면 넣고 남으면 지운다.
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

	// 세로 스크롤 막대가 생기거나 사라지면 너비가 바뀐다.
	f_FitColumns();
}

// 행 하나를 고른다. 선택이 바뀌면 f_OnItemChanged 가 부모에게 알린다.
VOID CGridCtrl::f_SelectRow(INT32 nRow)
{
	(VOID)SetItemState(nRow, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	(VOID)EnsureVisible(nRow, FALSE);
}

// 고른 행. 없으면 -1
INT32 CGridCtrl::f_GetCurRow(VOID) const
{
	return GetNextItem(-1, LVNI_SELECTED);
}

// 행 nRowNum 개가 스크롤 없이 들어가는 높이. 행이 있으면 실제 행 높이를 잰다.
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

// 칸 하나의 사각형. 0 열의 부분 항목 사각형은 행 전체라 머리글 위치로 만든다.
CRect CGridCtrl::f_GetCellRect(INT32 nRow, INT32 nColumn) const
{
	CRect st_Row;
	CRect st_HeaderItem;

	(VOID)GetItemRect(nRow, &st_Row, LVIR_BOUNDS);
	(VOID)GetHeaderCtrl()->GetItemRect(nColumn, &st_HeaderItem);

	return CRect(st_Row.left + st_HeaderItem.left, st_Row.top, st_Row.left + st_HeaderItem.right, st_Row.bottom);
}

// 열 너비를 비율대로 클라이언트 너비에 맞춘다. 마지막 열이 나머지를 가진다.
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

// 부모에게 WM_NOTIFY 를 보낸다.
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

// 칸 위에 숫자 칸이면 편집칸을, 목록 칸이면 콤보를 띄운다.
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

// 편집을 닫는다. isCommit 이면 편집칸의 글자를 칸에 넣고 부모에게 알린다. 목록 칸은 고르는 즉시 반영했다.
VOID CGridCtrl::f_EndEdit(INT32 isCommit)
{
	const INT32	nRow = nEditRow;
	const INT32	nColumn = nEditColumn;
	CWnd		*st_Editor;
	CString		st_NewText;

	if (nRow >= 0)
	{
		// 숨기면서 생기는 EN_KILLFOCUS, CBN_CLOSEUP 으로 다시 불려도 아무 일 없게 먼저 닫힌 상태로 둔다.
		nEditRow	= -1;
		nEditColumn	= -1;
		st_Editor	= (st_Column[nColumn].kind == GRID_KIND_NUMBER) ? static_cast<CWnd *>(&st_Edit) : static_cast<CWnd *>(&st_Combo);

		// 포커스를 가진 채 숨기면 포커스가 사라지므로 표로 옮긴다.
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

// 편집칸이 포커스를 잃으면 확정한다.
VOID CGridCtrl::f_OnEditKillFocus(VOID)
{
	f_EndEdit(1);
}

// 목록에서 고른 값을 칸에 넣고 부모에게 알린다. 콤보는 CBN_SELENDOK 을 CBN_CLOSEUP 보다 먼저 보낸다.
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

// 목록이 닫히면 편집을 끝낸다.
VOID CGridCtrl::f_OnComboCloseUp(VOID)
{
	f_EndEdit(1);
}

// 클릭이 끝난 뒤 목록을 편다. OnLButtonDown 이 이 메시지를 게시한다.
LRESULT CGridCtrl::f_OnOpenChoice(WPARAM wParam, LPARAM lParam)
{
	f_BeginEdit(static_cast<INT32>(wParam), static_cast<INT32>(lParam));

	return 0;
}

// 입력

// 누른 칸의 행을 고르고, 목록 칸이면 콤보를 띄운다. 기본 처리는 끌기 판정 때문에 클릭이 늦게 먹어 직접 다룬다.
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

		// 버튼을 누른 채 목록을 펴면 떼는 순간 닫히므로 클릭이 끝난 뒤에 연다.
		if (st_Column[st_Hit.iSubItem].kind == GRID_KIND_CHOICE)
		{
			(VOID)PostMessage(GRID_WM_OPEN_CHOICE, static_cast<WPARAM>(st_Hit.iItem), static_cast<LPARAM>(st_Hit.iSubItem));
		}
	}
}

// 숫자 칸을 두 번 누르면 편집칸을 연다.
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

// 크기가 바뀌면 편집을 닫고 열 너비를 다시 맞춘다.
VOID CGridCtrl::OnSize(UINT32 type, INT32 width, INT32 height)
{
	CListCtrl::OnSize(type, width, height);

	f_EndEdit(1);
	f_FitColumns();
}

// 스크롤 전에 편집을 닫는다. 편집칸이 엉뚱한 행 위에 남지 않게 한다.
VOID CGridCtrl::OnVScroll(UINT32 code, UINT32 pos, CScrollBar *st_ScrollBar)
{
	f_EndEdit(1);
	CListCtrl::OnVScroll(code, pos, st_ScrollBar);
}

// 휠 스크롤 전에 편집을 닫는다.
BOOL CGridCtrl::OnMouseWheel(UINT32 flags, SHORT delta, CPoint st_Point)
{
	f_EndEdit(1);

	return CListCtrl::OnMouseWheel(flags, delta, st_Point);
}

// 행이 새로 골라지면 부모에게 알린다.
VOID CGridCtrl::f_OnItemChanged(NMHDR *st_Hdr, LRESULT *pt_Result)
{
	const NMLISTVIEW *st_Item = reinterpret_cast<NMLISTVIEW *>(st_Hdr);

	if (((st_Item->uChanged & LVIF_STATE) != 0U) && ((st_Item->uNewState & LVIS_SELECTED) != 0U) && ((st_Item->uOldState & LVIS_SELECTED) == 0U))
	{
		f_Notify(GRIDN_ROWCHANGED, st_Item->iItem, 0);
	}

	*pt_Result = 0;
}
