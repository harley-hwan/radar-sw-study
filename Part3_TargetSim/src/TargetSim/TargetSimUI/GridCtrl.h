#pragma once

#include "TargetSim.h"

// 열 종류
#define GRID_KIND_LABEL			0						// 읽기 전용 글자
#define GRID_KIND_NUMBER		1						// 숫자 편집
#define GRID_KIND_CHOICE		2						// 목록에서 고르기

// 부모에게 보내는 WM_NOTIFY 코드
#define GRIDN_CELLCHANGED		2001U
#define GRIDN_ROWCHANGED		2002U

typedef struct
{
	LPCTSTR				pt_Title;
	INT32				kind;
	INT32				nWeight;						// 열 너비 비율
	const LPCTSTR		*pt_Choice;
	INT32				nChoiceNum;
} ST_GridColumn;

typedef struct
{
	NMHDR				st_Hdr;
	INT32				nRow;
	INT32				nColumn;
} ST_GridNotify;

// 칸을 바로 고치는 표. 숫자 칸은 두 번 눌러 편집칸을, 목록 칸은 한 번 눌러 콤보를 띄운다.
// Enter 와 Esc 는 대화상자가 받아 f_EndEdit 로 넘긴다.
class CGridCtrl : public CListCtrl
{
public:
	CGridCtrl() noexcept;

	// 열 정의는 표가 살아 있는 동안 유지되어야 한다.
	VOID	f_Setup(const ST_GridColumn *st_NewColumn, INT32 nNewColumnNum, INT32 newDpi);
	VOID	f_SetRowNum(INT32 nRowNum);
	VOID	f_SelectRow(INT32 nRow);
	INT32	f_GetCurRow(VOID) const;
	INT32	f_GetHeightForRows(INT32 nRowNum) const;
	VOID	f_EndEdit(INT32 isCommit);

protected:
	afx_msg VOID	OnLButtonDown(UINT32 flags, CPoint st_Point);
	afx_msg VOID	OnLButtonDblClk(UINT32 flags, CPoint st_Point);
	afx_msg VOID	OnSize(UINT32 type, INT32 width, INT32 height);
	afx_msg VOID	OnVScroll(UINT32 code, UINT32 pos, CScrollBar *st_ScrollBar);
	afx_msg BOOL	OnMouseWheel(UINT32 flags, SHORT delta, CPoint st_Point);
	afx_msg VOID	f_OnItemChanged(NMHDR *st_Hdr, LRESULT *pt_Result);
	afx_msg VOID	f_OnEditKillFocus(VOID);
	afx_msg VOID	f_OnComboSelEndOk(VOID);
	afx_msg VOID	f_OnComboCloseUp(VOID);
	afx_msg LRESULT	f_OnOpenChoice(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()

private:
	CRect	f_GetCellRect(INT32 nRow, INT32 nColumn) const;
	VOID	f_FitColumns(VOID);
	VOID	f_BeginEdit(INT32 nRow, INT32 nColumn);
	VOID	f_Notify(UINT32 code, INT32 nRow, INT32 nColumn);

	const ST_GridColumn	*st_Column;
	INT32				nColumnNum;
	INT32				dpi;
	INT32				nRowHeight;
	INT32				nEditRow;							// 편집 중이 아니면 -1
	INT32				nEditColumn;
	CEdit				st_Edit;
	CComboBox			st_Combo;
	CImageList			st_RowSizer;						// 행 높이용
};
