#include "pch.h"

#include "GridCtrl_JH.h"
#include "UiNumber_JH.h"
#include "UiTheme_JH.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define GRID_ID_EDIT			101
#define GRID_ID_SPIN			102
#define GRID_ID_COMBO			103
#define GRID_TEXT_LIMIT			32
#define GRID_PAD_X				8						// [px @96dpi] 칸 안쪽 좌우 여백
#define GRID_PAD_Y				6						// [px @96dpi] 글자 위아래 여백
#define GRID_SWATCH_SIZE		10
#define GRID_SPIN_WIDTH			17
#define GRID_DROP_HEIGHT		160
#define GRID_COARSE_FACTOR		10.0
#define GRID_SPIN_RANGE			1000000
#define GRID_WM_OPEN_CHOICE		(WM_APP + 21)			// 클릭이 끝난 뒤에 고르기 목록을 연다

// ---------------------------------------------------------------------------
// 편집칸

BEGIN_MESSAGE_MAP(CGridEdit, CEdit)
	ON_WM_GETDLGCODE()
	ON_WM_KEYDOWN()
	ON_WM_CHAR()
	ON_WM_KILLFOCUS()
	ON_WM_MOUSEWHEEL()
END_MESSAGE_MAP()

CGridEdit::CGridEdit() noexcept
	: st_Owner(nullptr)
{
}

VOID CGridEdit::f_SetOwner(CGridCtrl *st_NewOwner)
{
	st_Owner = st_NewOwner;
}

UINT32 CGridEdit::OnGetDlgCode(VOID)
{
	return DLGC_WANTALLKEYS | DLGC_HASSETSEL;
}

VOID CGridEdit::OnKeyDown(UINT32 key, UINT32 repeat, UINT32 flags)
{
	if ((st_Owner != nullptr) && ((key == VK_RETURN) || (key == VK_ESCAPE) || (key == VK_TAB) || (key == VK_UP) || (key == VK_DOWN)))
	{
		st_Owner->f_OnEditKey(key);
	}
	else
	{
		CEdit::OnKeyDown(key, repeat, flags);
	}
}

VOID CGridEdit::OnChar(UINT32 key, UINT32 repeat, UINT32 flags)
{
	// Enter·Tab·Esc 는 OnKeyDown 에서 처리했다. 여기서 기본 처리로 넘기면 경고음이 난다.
	if ((key != VK_RETURN) && (key != VK_TAB) && (key != VK_ESCAPE))
	{
		CEdit::OnChar(key, repeat, flags);
	}
}

VOID CGridEdit::OnKillFocus(CWnd *st_NewWnd)
{
	CEdit::OnKillFocus(st_NewWnd);

	if (st_Owner != nullptr)
	{
		st_Owner->f_OnEditKillFocus();
	}
}

BOOL CGridEdit::OnMouseWheel(UINT32 flags, SHORT delta, CPoint st_Point)
{
	UNREFERENCED_PARAMETER(flags);
	UNREFERENCED_PARAMETER(st_Point);

	if ((st_Owner != nullptr) && (delta != 0))
	{
		st_Owner->f_OnEditWheel((delta > 0) ? 1 : -1);
	}

	return TRUE;
}

// ---------------------------------------------------------------------------
// 표

BEGIN_MESSAGE_MAP(CGridCtrl, CListCtrl)
	ON_WM_GETDLGCODE()
	ON_WM_KEYDOWN()
	ON_WM_CHAR()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_SIZE()
	ON_WM_SETFOCUS()
	ON_WM_KILLFOCUS()
	ON_WM_VSCROLL()
	ON_WM_MOUSEWHEEL()
	ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, &CGridCtrl::f_OnCustomDraw)
	ON_NOTIFY_REFLECT(LVN_ITEMCHANGED, &CGridCtrl::f_OnItemChanged)
	ON_NOTIFY(UDN_DELTAPOS, GRID_ID_SPIN, &CGridCtrl::f_OnSpinDelta)
	ON_CBN_SELENDOK(GRID_ID_COMBO, &CGridCtrl::f_OnComboSelEndOk)
	ON_CBN_CLOSEUP(GRID_ID_COMBO, &CGridCtrl::f_OnComboCloseUp)
	ON_MESSAGE(GRID_WM_OPEN_CHOICE, &CGridCtrl::f_OnOpenChoice)
END_MESSAGE_MAP()

CGridCtrl::CGridCtrl() noexcept
	: nColumnNum(0)
	, dpi(UI_BASE_DPI)
	, nCurRow(-1)
	, nCurColumn(-1)
	, nErrorRow(-1)
	, nErrorColumn(-1)
	, nEditRow(-1)
	, nEditColumn(-1)
	, isEnding(0)
	, nRowHeight(24)
{
	INT32 nRow;

	(VOID)memset(st_Column, 0, sizeof(st_Column));

	for (nRow = 0; nRow < GRID_MAX_ROW; nRow++)
	{
		rowColor[nRow] = CLR_NONE;
	}
}

VOID CGridCtrl::f_Setup(const ST_GridColumn *st_NewColumn, INT32 nNewColumnNum, INT32 newDpi)
{
	CClientDC	st_Dc(this);
	CFont		*st_OldFont;
	TEXTMETRIC	st_Metric;
	INT32		nColumn;

	dpi			= newDpi;
	nColumnNum	= (nNewColumnNum < GRID_MAX_COLUMN) ? nNewColumnNum : GRID_MAX_COLUMN;

	(VOID)SetExtendedStyle(LVS_EX_DOUBLEBUFFER);
	(VOID)SetBkColor(UI_COLOR_CARD);
	(VOID)SetTextBkColor(UI_COLOR_CARD);

	// 행 높이는 작은 이미지 목록의 높이를 따른다. 글자 높이에 위아래 여백을 더한다.
	st_OldFont = st_Dc.SelectObject(GetFont());
	(VOID)st_Dc.GetTextMetrics(&st_Metric);
	(VOID)st_Dc.SelectObject(st_OldFont);
	nRowHeight = st_Metric.tmHeight + (2 * f_Ui_Scale(GRID_PAD_Y, dpi));

	if (st_RowSizer.GetSafeHandle() == nullptr)
	{
		(VOID)st_RowSizer.Create(1, nRowHeight, ILC_COLOR, 1, 1);
		(VOID)SetImageList(&st_RowSizer, LVSIL_SMALL);
	}

	for (nColumn = 0; nColumn < nColumnNum; nColumn++)
	{
		st_Column[nColumn] = st_NewColumn[nColumn];

		(VOID)InsertColumn(nColumn, st_Column[nColumn].pt_Title,
			((st_Column[nColumn].kind == GRID_KIND_NUMBER) || (st_Column[nColumn].kind == GRID_KIND_VALUE)) ? LVCFMT_RIGHT : LVCFMT_LEFT, 40);
	}

	if (GetHeaderCtrl() != nullptr)
	{
		// 열 너비는 표 너비에 맞춰 자동으로 나누므로 사용자가 끌어서 바꾸지 못하게 한다.
		(VOID)GetHeaderCtrl()->ModifyStyle(0, HDS_NOSIZING);
	}

	f_FitColumns();
}

VOID CGridCtrl::f_SetRowNum(INT32 nRowNum)
{
	INT32 nTarget = nRowNum;
	INT32 nCount;

	if (nTarget < 0)
	{
		nTarget = 0;
	}
	else if (nTarget > GRID_MAX_ROW)
	{
		nTarget = GRID_MAX_ROW;
	}
	else
	{
		// 범위 안이면 그대로 쓴다.
	}

	f_EndEdit(0);

	for (nCount = GetItemCount(); nCount < nTarget; nCount++)
	{
		(VOID)InsertItem(nCount, _T(""));
	}

	for (nCount = GetItemCount(); nCount > nTarget; nCount--)
	{
		(VOID)DeleteItem(nCount - 1);
	}

	if (nCurRow >= nTarget)
	{
		nCurRow = nTarget - 1;
	}

	if (nErrorRow >= nTarget)
	{
		nErrorRow		= -1;
		nErrorColumn	= -1;
	}

	// 세로 스크롤바가 생기거나 없어지면 쓸 수 있는 너비가 달라진다.
	f_FitColumns();
	Invalidate(FALSE);
}

VOID CGridCtrl::f_SetCellText(INT32 nRow, INT32 nColumn, const CString &st_Text)
{
	if ((nRow >= 0) && (nRow < GetItemCount()) && (nColumn >= 0) && (nColumn < nColumnNum))
	{
		if (GetItemText(nRow, nColumn) != st_Text)
		{
			(VOID)SetItemText(nRow, nColumn, st_Text);
		}
	}
}

CString CGridCtrl::f_GetCellText(INT32 nRow, INT32 nColumn) const
{
	CString st_Text;

	if ((nRow >= 0) && (nRow < GetItemCount()) && (nColumn >= 0) && (nColumn < nColumnNum))
	{
		st_Text = GetItemText(nRow, nColumn);
	}

	return st_Text;
}

VOID CGridCtrl::f_SetRowColor(INT32 nRow, COLORREF color)
{
	if ((nRow >= 0) && (nRow < GRID_MAX_ROW))
	{
		rowColor[nRow] = color;
	}
}

VOID CGridCtrl::f_SetColumnNudge(INT32 nColumn, FLOAT64 nudgeStep, INT32 nMinDecimal, FLOAT64 minValue, FLOAT64 maxValue)
{
	if ((nColumn >= 0) && (nColumn < nColumnNum))
	{
		st_Column[nColumn].nudgeStep	= nudgeStep;
		st_Column[nColumn].nMinDecimal	= nMinDecimal;
		st_Column[nColumn].minValue		= minValue;
		st_Column[nColumn].maxValue		= maxValue;
	}
}

VOID CGridCtrl::f_SetErrorCell(INT32 nRow, INT32 nColumn)
{
	if ((nRow != nErrorRow) || (nColumn != nErrorColumn))
	{
		f_InvalidateCell(nErrorRow, nErrorColumn);
		nErrorRow		= nRow;
		nErrorColumn	= nColumn;
		f_InvalidateCell(nErrorRow, nErrorColumn);
	}
}

VOID CGridCtrl::f_SetEmptyText(const CString &st_Text)
{
	st_EmptyText = st_Text;

	if (GetItemCount() == 0)
	{
		Invalidate(FALSE);
	}
}

INT32 CGridCtrl::f_GetCurRow(VOID) const
{
	return nCurRow;
}

INT32 CGridCtrl::f_GetCurColumn(VOID) const
{
	return nCurColumn;
}

INT32 CGridCtrl::f_GetHeightForRows(INT32 nRowNum) const
{
	CRect	st_Header(0, 0, 0, 0);
	CRect	st_Item(0, 0, 0, 0);
	INT32	headerHeight;
	INT32	rowPitch = nRowHeight + 1;

	if (GetHeaderCtrl() != nullptr)
	{
		GetHeaderCtrl()->GetWindowRect(&st_Header);
	}

	// 리스트는 이미지 높이에 1 px 를 더해 행을 놓는다. 행이 있으면 실제 간격을 잰다.
	if ((GetItemCount() > 0) && (GetItemRect(0, &st_Item, LVIR_BOUNDS) != FALSE) && (st_Item.Height() > 0))
	{
		rowPitch = st_Item.Height();
	}

	headerHeight = (st_Header.Height() > 0) ? st_Header.Height() : nRowHeight;

	// 마지막 행 아래에 4 px 를 남긴다.
	return headerHeight + (nRowNum * rowPitch) + 4;
}

VOID CGridCtrl::f_SetCurCell(INT32 nRow, INT32 nColumn, INT32 isEdit)
{
	if ((nRow >= 0) && (nRow < GetItemCount()) && (nColumn >= 0) && (nColumn < nColumnNum))
	{
		f_EndEdit(1);
		f_InvalidateCell(nCurRow, nCurColumn);

		nCurColumn = nColumn;

		// 행이 바뀌면 f_OnItemChanged 가 nCurRow 를 고치고 부모에게 알린다.
		(VOID)SetItemState(nRow, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		(VOID)EnsureVisible(nRow, FALSE);
		nCurRow = nRow;
		f_InvalidateCell(nCurRow, nCurColumn);

		if ((isEdit != 0) && (f_IsEditable(nColumn) != 0))
		{
			f_BeginEdit(nRow, nColumn, nullptr);
		}
	}
}

INT32 CGridCtrl::f_IsEditable(INT32 nColumn) const
{
	INT32 isEditable = 0;

	if ((nColumn >= 0) && (nColumn < nColumnNum))
	{
		isEditable = ((st_Column[nColumn].kind == GRID_KIND_NUMBER) || (st_Column[nColumn].kind == GRID_KIND_CHOICE)) ? 1 : 0;
	}

	return isEditable;
}

INT32 CGridCtrl::f_GetCellRect(INT32 nRow, INT32 nColumn, CRect *st_Rect) const
{
	CRect	st_Row;
	CRect	st_HeaderItem;
	INT32	isOk = 0;

	if ((nRow >= 0) && (nRow < GetItemCount()) && (nColumn >= 0) && (nColumn < nColumnNum) && (GetHeaderCtrl() != nullptr))
	{
		// 0 번 열의 부분 항목 사각형은 행 전체를 돌려주므로 머리글의 열 위치로 직접 만든다.
		if ((GetItemRect(nRow, &st_Row, LVIR_BOUNDS) != FALSE) &&
			(GetHeaderCtrl()->GetItemRect(nColumn, &st_HeaderItem) != FALSE))
		{
			st_Rect->SetRect(st_Row.left + st_HeaderItem.left, st_Row.top, st_Row.left + st_HeaderItem.right, st_Row.bottom);
			isOk = 1;
		}
	}

	return isOk;
}

VOID CGridCtrl::f_FitColumns(VOID)
{
	CRect	st_Client;
	INT32	totalWeight = 0;
	INT32	usedWidth = 0;
	INT32	width;
	INT32	nColumn;

	if ((nColumnNum > 0) && (GetSafeHwnd() != nullptr))
	{
		GetClientRect(&st_Client);

		for (nColumn = 0; nColumn < nColumnNum; nColumn++)
		{
			totalWeight = totalWeight + st_Column[nColumn].nWeight;
		}

		if ((totalWeight > 0) && (st_Client.Width() > 0))
		{
			SetRedraw(FALSE);

			for (nColumn = 0; nColumn < nColumnNum; nColumn++)
			{
				// 마지막 열이 나머지를 가져가 오른쪽에 빈 띠가 남지 않는다.
				width = (nColumn == (nColumnNum - 1)) ? (st_Client.Width() - usedWidth) : ::MulDiv(st_Client.Width(), st_Column[nColumn].nWeight, totalWeight);

				if (width < 8)
				{
					width = 8;
				}

				if (GetColumnWidth(nColumn) != width)
				{
					(VOID)SetColumnWidth(nColumn, width);
				}

				usedWidth = usedWidth + width;
			}

			SetRedraw(TRUE);
			Invalidate(FALSE);
		}
	}
}

VOID CGridCtrl::f_InvalidateCell(INT32 nRow, INT32 nColumn)
{
	CRect st_Rect;

	if (f_GetCellRect(nRow, nColumn, &st_Rect) != 0)
	{
		InvalidateRect(&st_Rect, FALSE);
	}
}

VOID CGridCtrl::f_Notify(UINT32 code, INT32 nRow, INT32 nColumn, INT32 isLive, HDC dcHandle, const RECT *st_Rect)
{
	ST_GridNotify	st_Notify;
	CWnd			*st_Parent = GetParent();

	if (st_Parent != nullptr)
	{
		(VOID)memset(&st_Notify, 0, sizeof(st_Notify));
		st_Notify.st_Hdr.hwndFrom	= GetSafeHwnd();
		st_Notify.st_Hdr.idFrom		= static_cast<UINT_PTR>(GetDlgCtrlID());
		st_Notify.st_Hdr.code		= code;
		st_Notify.nRow				= nRow;
		st_Notify.nColumn			= nColumn;
		st_Notify.isLive			= isLive;
		st_Notify.dcHandle			= dcHandle;

		if (st_Rect != nullptr)
		{
			st_Notify.st_Rect = *st_Rect;
		}

		(VOID)st_Parent->SendMessage(WM_NOTIFY, static_cast<WPARAM>(GetDlgCtrlID()), reinterpret_cast<LPARAM>(&st_Notify));
	}
}

// ---------------------------------------------------------------------------
// 그리기

VOID CGridCtrl::f_OnCustomDraw(NMHDR *st_Hdr, LRESULT *pt_Result)
{
	NMLVCUSTOMDRAW *st_Draw = reinterpret_cast<NMLVCUSTOMDRAW *>(st_Hdr);

	switch (st_Draw->nmcd.dwDrawStage)
	{
	case CDDS_PREPAINT:
		*pt_Result = CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
		break;

	case CDDS_ITEMPREPAINT:
		*pt_Result = CDRF_NOTIFYSUBITEMDRAW;
		break;

	case (CDDS_ITEMPREPAINT | CDDS_SUBITEM):
		f_DrawCell(CDC::FromHandle(st_Draw->nmcd.hdc), static_cast<INT32>(st_Draw->nmcd.dwItemSpec), st_Draw->iSubItem);
		*pt_Result = CDRF_SKIPDEFAULT;
		break;

	case CDDS_POSTPAINT:
		if ((GetItemCount() == 0) && (!st_EmptyText.IsEmpty()))
		{
			CDC		*st_Dc = CDC::FromHandle(st_Draw->nmcd.hdc);
			CFont	*st_OldFont = st_Dc->SelectObject(GetFont());
			CRect	st_Client;
			CRect	st_Header(0, 0, 0, 0);

			GetClientRect(&st_Client);

			if (GetHeaderCtrl() != nullptr)
			{
				GetHeaderCtrl()->GetWindowRect(&st_Header);
			}

			st_Client.top = st_Client.top + ((st_Header.Height() > 0) ? st_Header.Height() : nRowHeight);
			(VOID)st_Dc->SetBkMode(TRANSPARENT);
			(VOID)st_Dc->SetTextColor(UI_COLOR_TEXT_SUB);
			(VOID)st_Dc->DrawText(st_EmptyText, &st_Client, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
			(VOID)st_Dc->SelectObject(st_OldFont);
		}

		*pt_Result = CDRF_DODEFAULT;
		break;

	default:
		*pt_Result = CDRF_DODEFAULT;
		break;
	}
}

VOID CGridCtrl::f_DrawCell(CDC *st_Dc, INT32 nRow, INT32 nColumn)
{
	const INT32	padX = f_Ui_Scale(GRID_PAD_X, dpi);
	CRect		st_Cell;
	CRect		st_Text;
	CFont		*st_OldFont;
	CString		st_Value;
	COLORREF	background = UI_COLOR_CARD;
	INT32		isSelected;
	INT32		isCurrent;
	INT32		isError;
	INT32		kind;

	if ((st_Dc != nullptr) && (f_GetCellRect(nRow, nColumn, &st_Cell) != 0))
	{
		kind		= st_Column[nColumn].kind;
		isSelected	= (GetItemState(nRow, LVIS_SELECTED) != 0U) ? 1 : 0;
		isCurrent	= ((nRow == nCurRow) && (nColumn == nCurColumn) && (f_IsEditable(nColumn) != 0)) ? 1 : 0;
		isError		= ((nRow == nErrorRow) && (nColumn == nErrorColumn)) ? 1 : 0;

		if (isError != 0)
		{
			background = UI_COLOR_ERROR_SOFT;
		}
		else if (isCurrent != 0)
		{
			background = UI_COLOR_CARD;
		}
		else if (isSelected != 0)
		{
			background = UI_COLOR_ACCENT_SOFT;
		}
		else
		{
			background = UI_COLOR_CARD;
		}

		st_Dc->FillSolidRect(&st_Cell, background);
		st_Dc->FillSolidRect(st_Cell.left, st_Cell.bottom - 1, st_Cell.Width(), 1, UI_COLOR_GRID_LINE);

		st_OldFont = st_Dc->SelectObject(GetFont());
		(VOID)st_Dc->SetBkMode(TRANSPARENT);
		st_Value	= GetItemText(nRow, nColumn);
		st_Text		= st_Cell;
		st_Text.DeflateRect(padX, 0);

		if (kind == GRID_KIND_CUSTOM)
		{
			st_Text.DeflateRect(0, f_Ui_Scale(GRID_PAD_Y, dpi) - 1);
			f_Notify(GRIDN_DRAWCELL, nRow, nColumn, 0, st_Dc->GetSafeHdc(), &st_Text);
		}
		else if (kind == GRID_KIND_LABEL)
		{
			if ((nRow < GRID_MAX_ROW) && (rowColor[nRow] != CLR_NONE))
			{
				const INT32	size = f_Ui_Scale(GRID_SWATCH_SIZE, dpi);
				const INT32	top = st_Cell.top + ((st_Cell.Height() - size) / 2);
				CBrush		st_Brush(rowColor[nRow]);
				CPen		st_Pen(PS_SOLID, 1, rowColor[nRow]);
				CBrush		*st_OldBrush = st_Dc->SelectObject(&st_Brush);
				CPen		*st_OldPen = st_Dc->SelectObject(&st_Pen);

				(VOID)st_Dc->RoundRect(st_Text.left, top, st_Text.left + size, top + size, 4, 4);
				(VOID)st_Dc->SelectObject(st_OldPen);
				(VOID)st_Dc->SelectObject(st_OldBrush);
				st_Text.left = st_Text.left + size + padX;
			}

			(VOID)st_Dc->SetTextColor(UI_COLOR_TEXT);
			(VOID)st_Dc->DrawText(st_Value, &st_Text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
		}
		else if (kind == GRID_KIND_CHOICE)
		{
			CRect st_Arrow = st_Text;

			st_Arrow.left = st_Arrow.right - f_Ui_Scale(12, dpi);
			st_Text.right = st_Arrow.left;
			(VOID)st_Dc->SetTextColor(UI_COLOR_TEXT);
			(VOID)st_Dc->DrawText(st_Value, &st_Text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
			(VOID)st_Dc->SetTextColor(UI_COLOR_TEXT_SUB);
			(VOID)st_Dc->DrawText(CString(_T("▾")), &st_Arrow, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		}
		else
		{
			(VOID)st_Dc->SetTextColor((kind == GRID_KIND_VALUE) ? UI_COLOR_TEXT_SUB : ((isError != 0) ? UI_COLOR_ERROR : UI_COLOR_TEXT));
			(VOID)st_Dc->DrawText(st_Value, &st_Text, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
		}

		if (isError != 0)
		{
			CBrush st_Frame(UI_COLOR_ERROR);

			st_Dc->FrameRect(&st_Cell, &st_Frame);
		}
		else if (isCurrent != 0)
		{
			// 표가 포커스를 가졌을 때만 강조색으로 두른다.
			const CWnd	*st_Focus = GetFocus();
			CBrush		st_Frame(((st_Focus == this) || (nEditRow >= 0)) ? UI_COLOR_ACCENT : UI_COLOR_TEXT_OFF);
			CRect		st_Inner = st_Cell;

			st_Dc->FrameRect(&st_Inner, &st_Frame);
			st_Inner.DeflateRect(1, 1);
			st_Dc->FrameRect(&st_Inner, &st_Frame);
		}
		else
		{
			// 테두리 없음
		}

		(VOID)st_Dc->SelectObject(st_OldFont);
	}
}

// ---------------------------------------------------------------------------
// 편집

VOID CGridCtrl::f_BeginEdit(INT32 nRow, INT32 nColumn, LPCTSTR pt_InitialText)
{
	CRect	st_Cell;
	INT32	nChoice;
	INT32	nSelected;

	if ((nEditRow < 0) && (f_IsEditable(nColumn) != 0) && (f_GetCellRect(nRow, nColumn, &st_Cell) != 0))
	{
		(VOID)EnsureVisible(nRow, FALSE);
		(VOID)f_GetCellRect(nRow, nColumn, &st_Cell);

		nEditRow		= nRow;
		nEditColumn		= nColumn;
		st_EditOriginal	= GetItemText(nRow, nColumn);

		if (st_Column[nColumn].kind == GRID_KIND_NUMBER)
		{
			const INT32	spinWidth = f_Ui_Scale(GRID_SPIN_WIDTH, dpi);
			const INT32	padY = f_Ui_Scale(GRID_PAD_Y, dpi);
			CRect		st_EditRect(st_Cell.left + 3, st_Cell.top + padY, st_Cell.right - spinWidth - 3, st_Cell.bottom - padY);
			CRect		st_SpinRect(st_Cell.right - spinWidth - 2, st_Cell.top + 2, st_Cell.right - 2, st_Cell.bottom - 2);

			if (st_Edit.GetSafeHwnd() == nullptr)
			{
				(VOID)st_Edit.Create(WS_CHILD | ES_AUTOHSCROLL | ES_RIGHT, st_EditRect, this, GRID_ID_EDIT);
				st_Edit.f_SetOwner(this);
				st_Edit.SetFont(GetFont());
				st_Edit.SetLimitText(GRID_TEXT_LIMIT);
				(VOID)st_Spin.Create(WS_CHILD | UDS_NOTHOUSANDS, st_SpinRect, this, GRID_ID_SPIN);
				st_Spin.SetRange32(-GRID_SPIN_RANGE, GRID_SPIN_RANGE);
			}

			st_Edit.MoveWindow(&st_EditRect);
			st_Spin.MoveWindow(&st_SpinRect);
			(VOID)st_Spin.SetPos32(0);
			st_Edit.SetWindowText((pt_InitialText != nullptr) ? pt_InitialText : st_EditOriginal.GetString());
			(VOID)st_Edit.ShowWindow(SW_SHOW);
			(VOID)st_Spin.ShowWindow(SW_SHOW);
			(VOID)st_Edit.SetFocus();

			if (pt_InitialText != nullptr)
			{
				// 글자를 쳐서 연 편집은 그 글자 뒤에서 이어 쓴다.
				st_Edit.SetSel(st_Edit.GetWindowTextLength(), st_Edit.GetWindowTextLength());
			}
			else
			{
				st_Edit.SetSel(0, -1);
			}
		}
		else
		{
			CRect st_ComboRect(st_Cell.left + 1, st_Cell.top, st_Cell.right - 1, st_Cell.bottom + f_Ui_Scale(GRID_DROP_HEIGHT, dpi));

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

			nSelected = st_Combo.FindStringExact(-1, st_EditOriginal);
			(VOID)st_Combo.SetCurSel((nSelected >= 0) ? nSelected : 0);
			(VOID)st_Combo.SetItemHeight(-1, static_cast<UINT32>(st_Cell.Height() - 6));
			st_Combo.MoveWindow(&st_ComboRect);
			(VOID)st_Combo.ShowWindow(SW_SHOW);
			(VOID)st_Combo.SetFocus();
			st_Combo.ShowDropDown(TRUE);
		}

		f_InvalidateCell(nRow, nColumn);
	}
}

VOID CGridCtrl::f_EndEdit(INT32 isCommit)
{
	CString	st_NewText;
	INT32	nRow;
	INT32	nColumn;
	INT32	hadFocus;

	if ((nEditRow >= 0) && (isEnding == 0))
	{
		isEnding	= 1;
		nRow		= nEditRow;
		nColumn		= nEditColumn;

		if (st_Column[nColumn].kind == GRID_KIND_NUMBER)
		{
			hadFocus = (::GetFocus() == st_Edit.GetSafeHwnd()) ? 1 : 0;

			if (isCommit != 0)
			{
				st_Edit.GetWindowText(st_NewText);
				(VOID)st_NewText.Trim();
			}
			else
			{
				st_NewText = st_EditOriginal;
			}

			// 포커스를 가진 채 숨기면 포커스가 사라지므로 표로 먼저 옮긴다.
			if (hadFocus != 0)
			{
				(VOID)SetFocus();
			}

			(VOID)st_Edit.ShowWindow(SW_HIDE);
			(VOID)st_Spin.ShowWindow(SW_HIDE);
		}
		else
		{
			hadFocus = (::GetFocus() == st_Combo.GetSafeHwnd()) ? 1 : 0;
			st_NewText = GetItemText(nRow, nColumn);

			if (hadFocus != 0)
			{
				(VOID)SetFocus();
			}

			(VOID)st_Combo.ShowWindow(SW_HIDE);
		}

		nEditRow	= -1;
		nEditColumn	= -1;

		// 증감으로 이미 바뀐 값을 Esc 로 되돌리는 경우도 여기서 함께 알린다.
		if (GetItemText(nRow, nColumn) != st_NewText)
		{
			(VOID)SetItemText(nRow, nColumn, st_NewText);
			f_Notify(GRIDN_CELLCHANGED, nRow, nColumn, 0, nullptr, nullptr);
		}

		f_InvalidateCell(nRow, nColumn);
		isEnding = 0;
	}
}

VOID CGridCtrl::f_MoveEdit(INT32 nDirection)
{
	const INT32	nRowNum = GetItemCount();
	INT32		nRow = nEditRow;
	INT32		nColumn = nEditColumn;
	INT32		nTry;
	INT32		isFound = 0;

	f_EndEdit(1);

	// 편집할 수 있는 다음 칸을 찾는다. 행 끝에서는 다음 행으로 넘어간다.
	for (nTry = 0; (nTry < (nColumnNum * nRowNum)) && (isFound == 0) && (nRow >= 0); nTry++)
	{
		nColumn = nColumn + nDirection;

		if (nColumn >= nColumnNum)
		{
			nColumn	= 0;
			nRow	= nRow + 1;
		}
		else if (nColumn < 0)
		{
			nColumn	= nColumnNum - 1;
			nRow	= nRow - 1;
		}
		else
		{
			// 같은 행 안에서 옮긴다.
		}

		if ((nRow < 0) || (nRow >= nRowNum))
		{
			nRow = -1;
		}
		else if (f_IsEditable(nColumn) != 0)
		{
			isFound = 1;
		}
		else
		{
			// 읽기 전용 칸은 건너뛴다.
		}
	}

	if (isFound != 0)
	{
		f_SetCurCell(nRow, nColumn, 1);
	}
}

VOID CGridCtrl::f_Nudge(INT32 nDirection, INT32 isCoarse)
{
	CString	st_Text;
	CString	st_NewText;
	FLOAT64	delta;

	if ((nEditRow >= 0) && (st_Column[nEditColumn].kind == GRID_KIND_NUMBER) && (st_Column[nEditColumn].nudgeStep > 0.0))
	{
		delta = st_Column[nEditColumn].nudgeStep * static_cast<FLOAT64>(nDirection);

		if (isCoarse != 0)
		{
			delta = delta * GRID_COARSE_FACTOR;
		}

		st_Edit.GetWindowText(st_Text);

		if (f_Num_Nudge(st_Text, delta, st_Column[nEditColumn].nMinDecimal, st_Column[nEditColumn].minValue, st_Column[nEditColumn].maxValue, &st_NewText) != 0)
		{
			st_Edit.SetWindowText(st_NewText);
			st_Edit.SetSel(0, -1);

			// 편집을 닫지 않고 바로 반영해, 값을 굴리는 동안 그림이 따라 움직인다.
			if (GetItemText(nEditRow, nEditColumn) != st_NewText)
			{
				(VOID)SetItemText(nEditRow, nEditColumn, st_NewText);
				f_Notify(GRIDN_CELLCHANGED, nEditRow, nEditColumn, 1, nullptr, nullptr);
			}
		}
	}
}

VOID CGridCtrl::f_OnEditKey(UINT32 key)
{
	const INT32 isCoarse = (::GetKeyState(VK_CONTROL) < 0) ? 1 : 0;

	switch (key)
	{
	case VK_RETURN:
		f_EndEdit(1);
		break;

	case VK_ESCAPE:
		f_EndEdit(0);
		break;

	case VK_TAB:
		f_MoveEdit((::GetKeyState(VK_SHIFT) < 0) ? -1 : 1);
		break;

	case VK_UP:
		f_Nudge(1, isCoarse);
		break;

	case VK_DOWN:
		f_Nudge(-1, isCoarse);
		break;

	default:
		break;
	}
}

VOID CGridCtrl::f_OnEditWheel(INT32 nNotch)
{
	f_Nudge(nNotch, (::GetKeyState(VK_CONTROL) < 0) ? 1 : 0);
}

VOID CGridCtrl::f_OnEditKillFocus(VOID)
{
	// 다른 곳을 누르면 입력하던 값을 그대로 반영한다.
	f_EndEdit(1);
}

VOID CGridCtrl::f_OnSpinDelta(NMHDR *st_Hdr, LRESULT *pt_Result)
{
	const NMUPDOWN *st_UpDown = reinterpret_cast<NMUPDOWN *>(st_Hdr);

	if (st_UpDown->iDelta != 0)
	{
		f_Nudge((st_UpDown->iDelta > 0) ? 1 : -1, (::GetKeyState(VK_CONTROL) < 0) ? 1 : 0);
	}

	// 위치는 늘 0 에 두어 범위 끝에 닿지 않게 한다.
	*pt_Result = 1;
}

VOID CGridCtrl::f_OnComboSelEndOk(VOID)
{
	CString	st_NewText;
	INT32	nSelected;

	if ((nEditRow >= 0) && (st_Column[nEditColumn].kind == GRID_KIND_CHOICE))
	{
		nSelected = st_Combo.GetCurSel();

		if (nSelected >= 0)
		{
			st_Combo.GetLBText(nSelected, st_NewText);

			if (GetItemText(nEditRow, nEditColumn) != st_NewText)
			{
				(VOID)SetItemText(nEditRow, nEditColumn, st_NewText);
				f_Notify(GRIDN_CELLCHANGED, nEditRow, nEditColumn, 0, nullptr, nullptr);
			}
		}
	}
}

VOID CGridCtrl::f_OnComboCloseUp(VOID)
{
	f_EndEdit(1);
}

// ---------------------------------------------------------------------------
// 입력

UINT32 CGridCtrl::OnGetDlgCode(VOID)
{
	const MSG	*st_Msg = GetCurrentMessage();
	UINT32		code = DLGC_WANTARROWS | DLGC_WANTCHARS;

	// Enter 는 대화상자의 기본 버튼 대신 칸 편집을 연다.
	if ((st_Msg != nullptr) && (st_Msg->message == WM_GETDLGCODE) && (st_Msg->lParam != 0))
	{
		const MSG *st_Key = reinterpret_cast<const MSG *>(st_Msg->lParam);

		if ((st_Key->message == WM_KEYDOWN) && (st_Key->wParam == VK_RETURN))
		{
			code = code | DLGC_WANTMESSAGE;
		}
	}

	return code;
}

VOID CGridCtrl::OnKeyDown(UINT32 key, UINT32 repeat, UINT32 flags)
{
	INT32 nColumn;
	INT32 isHandled = 0;

	if ((key == VK_LEFT) || (key == VK_RIGHT))
	{
		// 편집할 수 있는 이웃 칸으로 옮긴다.
		nColumn = nCurColumn + ((key == VK_RIGHT) ? 1 : -1);

		while ((nColumn >= 0) && (nColumn < nColumnNum) && (f_IsEditable(nColumn) == 0))
		{
			nColumn = nColumn + ((key == VK_RIGHT) ? 1 : -1);
		}

		if ((nColumn >= 0) && (nColumn < nColumnNum) && (nCurRow >= 0))
		{
			f_InvalidateCell(nCurRow, nCurColumn);
			nCurColumn = nColumn;
			f_InvalidateCell(nCurRow, nCurColumn);
		}

		isHandled = 1;
	}
	else if ((key == VK_F2) || (key == VK_RETURN) || (key == VK_SPACE))
	{
		if ((nCurRow >= 0) && (f_IsEditable(nCurColumn) != 0))
		{
			f_BeginEdit(nCurRow, nCurColumn, nullptr);
		}

		isHandled = 1;
	}
	else
	{
		isHandled = 0;
	}

	if (isHandled == 0)
	{
		CListCtrl::OnKeyDown(key, repeat, flags);
	}
}

VOID CGridCtrl::OnChar(UINT32 key, UINT32 repeat, UINT32 flags)
{
	const TCHAR	typed = static_cast<TCHAR>(key);
	TCHAR		pt_Initial[2] = { typed, _T('\0') };

	UNREFERENCED_PARAMETER(repeat);
	UNREFERENCED_PARAMETER(flags);

	// 숫자를 치면 그 글자로 편집을 연다. 나머지 글자는 리스트의 글자 검색으로 넘기지 않고 버린다.
	if ((nCurRow >= 0) && (nCurColumn >= 0) && (nCurColumn < nColumnNum) && (st_Column[nCurColumn].kind == GRID_KIND_NUMBER) &&
		(((typed >= _T('0')) && (typed <= _T('9'))) || (typed == _T('-')) || (typed == _T('+')) || (typed == _T('.'))))
	{
		f_BeginEdit(nCurRow, nCurColumn, pt_Initial);
	}
}

VOID CGridCtrl::OnLButtonDown(UINT32 flags, CPoint st_Point)
{
	LVHITTESTINFO	st_Hit;
	INT32			wasCurrent;

	UNREFERENCED_PARAMETER(flags);

	// 기본 처리는 끌기 판정 때문에 클릭이 늦게 먹으므로 선택과 포커스를 직접 다룬다.
	f_EndEdit(1);
	(VOID)SetFocus();

	(VOID)memset(&st_Hit, 0, sizeof(st_Hit));
	st_Hit.pt = st_Point;

	if ((SubItemHitTest(&st_Hit) >= 0) && (st_Hit.iItem >= 0) && (st_Hit.iSubItem >= 0))
	{
		wasCurrent = ((st_Hit.iItem == nCurRow) && (st_Hit.iSubItem == nCurColumn)) ? 1 : 0;
		f_SetCurCell(st_Hit.iItem, st_Hit.iSubItem, 0);

		// 고르기 칸은 한 번, 숫자 칸은 이미 선택된 칸을 다시 누르면 편집이 열린다.
		if (st_Hit.iSubItem < nColumnNum)
		{
			if (st_Column[st_Hit.iSubItem].kind == GRID_KIND_CHOICE)
			{
				// 버튼을 누른 채로 목록을 펴면 버튼을 떼는 순간 목록이 닫힌다. 클릭이 끝난 뒤에 열리도록 게시한다.
				(VOID)PostMessage(GRID_WM_OPEN_CHOICE, static_cast<WPARAM>(st_Hit.iItem), static_cast<LPARAM>(st_Hit.iSubItem));
			}
			else if ((st_Column[st_Hit.iSubItem].kind == GRID_KIND_NUMBER) && (wasCurrent != 0))
			{
				f_BeginEdit(st_Hit.iItem, st_Hit.iSubItem, nullptr);
			}
			else
			{
				// 읽기 전용 칸이거나 처음 누른 숫자 칸이다.
			}
		}
	}
}

LRESULT CGridCtrl::f_OnOpenChoice(WPARAM wParam, LPARAM lParam)
{
	const INT32 nRow = static_cast<INT32>(wParam);
	const INT32 nColumn = static_cast<INT32>(lParam);

	// 게시한 뒤에 선택이 바뀌었으면 열지 않는다.
	if ((nRow == nCurRow) && (nColumn == nCurColumn) && (nColumn >= 0) && (nColumn < nColumnNum) && (st_Column[nColumn].kind == GRID_KIND_CHOICE))
	{
		f_BeginEdit(nRow, nColumn, nullptr);
	}

	return 0;
}

VOID CGridCtrl::OnLButtonDblClk(UINT32 flags, CPoint st_Point)
{
	LVHITTESTINFO st_Hit;

	UNREFERENCED_PARAMETER(flags);

	(VOID)memset(&st_Hit, 0, sizeof(st_Hit));
	st_Hit.pt = st_Point;

	if ((SubItemHitTest(&st_Hit) >= 0) && (st_Hit.iItem >= 0) && (st_Hit.iSubItem >= 0) && (st_Hit.iSubItem < nColumnNum))
	{
		// 고르기 칸은 첫 클릭에서 이미 목록을 열도록 게시했다.
		f_SetCurCell(st_Hit.iItem, st_Hit.iSubItem, (st_Column[st_Hit.iSubItem].kind == GRID_KIND_NUMBER) ? 1 : 0);
	}
}

VOID CGridCtrl::OnSize(UINT32 type, INT32 width, INT32 height)
{
	CListCtrl::OnSize(type, width, height);

	f_EndEdit(1);
	f_FitColumns();
}

VOID CGridCtrl::OnSetFocus(CWnd *st_OldWnd)
{
	CListCtrl::OnSetFocus(st_OldWnd);
	f_InvalidateCell(nCurRow, nCurColumn);
}

VOID CGridCtrl::OnKillFocus(CWnd *st_NewWnd)
{
	CListCtrl::OnKillFocus(st_NewWnd);
	f_InvalidateCell(nCurRow, nCurColumn);
}

VOID CGridCtrl::OnVScroll(UINT32 code, UINT32 pos, CScrollBar *st_ScrollBar)
{
	f_EndEdit(1);
	CListCtrl::OnVScroll(code, pos, st_ScrollBar);
}

BOOL CGridCtrl::OnMouseWheel(UINT32 flags, SHORT delta, CPoint st_Point)
{
	f_EndEdit(1);

	return CListCtrl::OnMouseWheel(flags, delta, st_Point);
}

VOID CGridCtrl::f_OnItemChanged(NMHDR *st_Hdr, LRESULT *pt_Result)
{
	const NMLISTVIEW *st_Item = reinterpret_cast<NMLISTVIEW *>(st_Hdr);

	if (((st_Item->uChanged & LVIF_STATE) != 0U) && ((st_Item->uNewState & LVIS_SELECTED) != 0U) && ((st_Item->uOldState & LVIS_SELECTED) == 0U))
	{
		if (st_Item->iItem != nCurRow)
		{
			nCurRow = st_Item->iItem;
			f_Notify(GRIDN_ROWCHANGED, nCurRow, nCurColumn, 0, nullptr, nullptr);
		}

		Invalidate(FALSE);
	}

	*pt_Result = 0;
}
