#pragma once

#include "Define_JH.h"

#define GRID_KIND_LABEL			0						// 읽기 전용 글자. 왼쪽 정렬, 행 색 표식이 붙는다
#define GRID_KIND_NUMBER		1						// 숫자 편집. 편집칸 + 증감 버튼
#define GRID_KIND_CHOICE		2						// 목록에서 고르기
#define GRID_KIND_VALUE			3						// 읽기 전용 값. 오른쪽 정렬, 흐린 글자
#define GRID_KIND_CUSTOM		4						// 부모가 그리는 칸

#define GRID_MAX_COLUMN			12
#define GRID_MAX_ROW			32

// 부모에게 보내는 WM_NOTIFY 코드
#define GRIDN_CELLCHANGED		2001U					// 칸 글자가 바뀌었다 (isLive = 1 이면 증감 중이라 편집이 아직 열려 있다)
#define GRIDN_ROWCHANGED		2002U					// 현재 행이 바뀌었다
#define GRIDN_DRAWCELL			2003U					// GRID_KIND_CUSTOM 칸을 그려 달라

typedef struct
{
	LPCTSTR				pt_Title;
	INT32				kind;
	INT32				nWeight;						// 열 너비 비율
	INT32				nMinDecimal;					// 증감할 때 지킬 최소 소수 자릿수
	FLOAT64				nudgeStep;
	FLOAT64				minValue;
	FLOAT64				maxValue;
	const LPCTSTR		*pt_Choice;
	INT32				nChoiceNum;
} ST_GridColumn;

typedef struct
{
	NMHDR				st_Hdr;
	INT32				nRow;
	INT32				nColumn;
	INT32				isLive;
	HDC					dcHandle;
	RECT				st_Rect;
} ST_GridNotify;

class CGridCtrl;

// 칸 위에 뜨는 편집칸. 대화상자가 Enter·Tab·Esc 를 가로채지 않게 모든 키를 직접 받는다.
class CGridEdit : public CEdit
{
public:
	CGridEdit() noexcept;

	VOID	f_SetOwner(CGridCtrl *st_NewOwner);

protected:
	afx_msg UINT32	OnGetDlgCode(VOID);
	afx_msg VOID	OnKeyDown(UINT32 key, UINT32 repeat, UINT32 flags);
	afx_msg VOID	OnChar(UINT32 key, UINT32 repeat, UINT32 flags);
	afx_msg VOID	OnKillFocus(CWnd *st_NewWnd);
	afx_msg BOOL	OnMouseWheel(UINT32 flags, SHORT delta, CPoint st_Point);

	DECLARE_MESSAGE_MAP()

private:
	CGridCtrl	*st_Owner;
};

// 칸을 바로 고치는 표. 글자는 리스트 항목에 두고, 그리기와 편집만 직접 한다.
class CGridCtrl : public CListCtrl
{
public:
	CGridCtrl() noexcept;

	VOID	f_Setup(const ST_GridColumn *st_NewColumn, INT32 nNewColumnNum, INT32 newDpi);
	VOID	f_SetRowNum(INT32 nRowNum);
	VOID	f_SetCellText(INT32 nRow, INT32 nColumn, const CString &st_Text);
	CString	f_GetCellText(INT32 nRow, INT32 nColumn) const;
	VOID	f_SetRowColor(INT32 nRow, COLORREF color);
	VOID	f_SetColumnNudge(INT32 nColumn, FLOAT64 nudgeStep, INT32 nMinDecimal, FLOAT64 minValue, FLOAT64 maxValue);
	VOID	f_SetErrorCell(INT32 nRow, INT32 nColumn);
	VOID	f_SetCurCell(INT32 nRow, INT32 nColumn, INT32 isEdit);
	VOID	f_SetEmptyText(const CString &st_Text);
	INT32	f_GetCurRow(VOID) const;
	INT32	f_GetCurColumn(VOID) const;
	INT32	f_GetHeightForRows(INT32 nRowNum) const;
	VOID	f_EndEdit(INT32 isCommit);

	// 편집칸이 부른다.
	VOID	f_OnEditKey(UINT32 key);
	VOID	f_OnEditWheel(INT32 nNotch);
	VOID	f_OnEditKillFocus(VOID);

protected:
	afx_msg UINT32	OnGetDlgCode(VOID);
	afx_msg VOID	OnKeyDown(UINT32 key, UINT32 repeat, UINT32 flags);
	afx_msg VOID	OnChar(UINT32 key, UINT32 repeat, UINT32 flags);
	afx_msg VOID	OnLButtonDown(UINT32 flags, CPoint st_Point);
	afx_msg VOID	OnLButtonDblClk(UINT32 flags, CPoint st_Point);
	afx_msg VOID	OnSize(UINT32 type, INT32 width, INT32 height);
	afx_msg VOID	OnSetFocus(CWnd *st_OldWnd);
	afx_msg VOID	OnKillFocus(CWnd *st_NewWnd);
	afx_msg VOID	OnVScroll(UINT32 code, UINT32 pos, CScrollBar *st_ScrollBar);
	afx_msg BOOL	OnMouseWheel(UINT32 flags, SHORT delta, CPoint st_Point);
	afx_msg VOID	f_OnCustomDraw(NMHDR *st_Hdr, LRESULT *pt_Result);
	afx_msg VOID	f_OnItemChanged(NMHDR *st_Hdr, LRESULT *pt_Result);
	afx_msg VOID	f_OnSpinDelta(NMHDR *st_Hdr, LRESULT *pt_Result);
	afx_msg VOID	f_OnComboSelEndOk(VOID);
	afx_msg VOID	f_OnComboCloseUp(VOID);
	afx_msg LRESULT	f_OnOpenChoice(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()

private:
	INT32	f_IsEditable(INT32 nColumn) const;
	INT32	f_GetCellRect(INT32 nRow, INT32 nColumn, CRect *st_Rect) const;
	VOID	f_FitColumns(VOID);
	VOID	f_DrawCell(CDC *st_Dc, INT32 nRow, INT32 nColumn);
	VOID	f_InvalidateCell(INT32 nRow, INT32 nColumn);
	VOID	f_BeginEdit(INT32 nRow, INT32 nColumn, LPCTSTR pt_InitialText);
	VOID	f_MoveEdit(INT32 nDirection);
	VOID	f_Nudge(INT32 nDirection, INT32 isCoarse);
	VOID	f_Notify(UINT32 code, INT32 nRow, INT32 nColumn, INT32 isLive, HDC dcHandle, const RECT *st_Rect);

	ST_GridColumn	st_Column[GRID_MAX_COLUMN];
	COLORREF		rowColor[GRID_MAX_ROW];
	INT32			nColumnNum;
	INT32			dpi;
	INT32			nCurRow;
	INT32			nCurColumn;
	INT32			nErrorRow;
	INT32			nErrorColumn;
	INT32			nEditRow;							// 편집 중이 아니면 -1
	INT32			nEditColumn;
	INT32			isEnding;							// 편집을 닫는 중의 포커스 이동은 무시한다
	INT32			nRowHeight;
	CString			st_EditOriginal;
	CString			st_EmptyText;
	CGridEdit		st_Edit;
	CSpinButtonCtrl	st_Spin;
	CComboBox		st_Combo;
	CImageList		st_RowSizer;
};
