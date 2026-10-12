#pragma once

#include "HistView.h"

// 난수 확인 (과제 4 실습 1): UNIRAN, GAUSS 를 각 10,000 개 뽑은 히스토그램과 이론 PDF.
// 히스토그램 아래에는 확률밀도를 구한 과정 (칸, 식, 예로 든 칸, 이론값, 평균 · 표준편차 식) 을 글로 적음.
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

	DECLARE_MESSAGE_MAP()

private:
	CHistView	st_UniformView;
	CHistView	st_GaussView;
	ST_PcsHist	st_Uniform;
	ST_PcsHist	st_Gauss;
};
