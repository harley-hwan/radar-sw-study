#pragma once

#include "HistView.h"

#define RAND_KIND_UNIFORM		0						// 왼쪽: UNIRAN
#define RAND_KIND_GAUSS			1						// 오른쪽: GAUSS(0, 1)
#define RAND_KIND_NUM			2

// 난수 확인 (과제 4 실습 1): UNIRAN, GAUSS 를 각 10,000 개 뽑은 히스토그램과 이론 PDF, 그리고 그 계산 과정을 실제 값으로 보여 줌.
// 히스토그램 아래: ① 뽑은 값 앞 10 개와 들어간 칸, ② 칸마다 센 개수, ③ 확률밀도와 이론값, ④ 평균 · 표준편차 식.
// 계산 예로 든 표본과 칸은 두 표의 줄과 히스토그램 막대에 같은 색을 칠함.
// 창을 열 때, [다시 뽑기] 를 누를 때마다 앞에서 멈춘 SEED 에서 이어 뽑으므로 값이 매번 다름.
class CRandDlg : public CDialogEx
{
public:
	explicit CRandDlg(CWnd *st_Parent = nullptr);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_RAND_DIALOG };
#endif

protected:
	VOID	DoDataExchange(CDataExchange *st_Dx) override;
	BOOL	OnInitDialog(VOID) override;

	afx_msg VOID	f_OnRedrawClicked(VOID);
	afx_msg VOID	f_OnListCustomDraw(NMHDR *st_Hdr, LRESULT *pt_Result);
	afx_msg VOID	f_OnListItemChanging(NMHDR *st_Hdr, LRESULT *pt_Result);

	DECLARE_MESSAGE_MAP()

private:
	VOID	f_SetupLists(VOID);
	VOID	f_Layout(VOID);
	VOID	f_Draw(VOID);
	INT32	f_GetMarkRow(INT32 ctrlId) const;

	CHistView	st_HistView[RAND_KIND_NUM];
	CListCtrl	st_SampleList[RAND_KIND_NUM];			// ① 뽑은 값
	CListCtrl	st_BinList[RAND_KIND_NUM];				// ② 칸마다 개수, ③ 확률밀도
	CFont		st_TitleFont;
	ST_PcsHist	st_Hist[RAND_KIND_NUM];
	INT32		nMarkSample[RAND_KIND_NUM];				// 계산 예로 든 표본 (뽑은 값 표의 줄)
	INT32		nMarkBin[RAND_KIND_NUM];				// 그 표본이 들어간 칸 (칸 표의 줄, 히스토그램 막대)
	INT32		dpi;
	INT32		textHeight;
};
