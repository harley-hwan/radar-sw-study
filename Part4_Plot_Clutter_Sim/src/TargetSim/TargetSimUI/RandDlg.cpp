#include "pch.h"
#include "framework.h"

#include "Resource.h"
#include "RandDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CRandDlg, CDialogEx)
END_MESSAGE_MAP()

// 난수 확인 창.
CRandDlg::CRandDlg(CWnd *st_Parent)
	: CDialogEx(IDD_RAND_DIALOG, st_Parent)
	, st_Uniform()
	, st_Gauss()
{
}

// 컨트롤 변수와 대화상자 자원 연결.
VOID CRandDlg::DoDataExchange(CDataExchange *st_Dx)
{
	CDialogEx::DoDataExchange(st_Dx);
	DDX_Control(st_Dx, IDC_RAND_UNIFORM, st_UniformView);
	DDX_Control(st_Dx, IDC_RAND_GAUSS, st_GaussView);
}

// 열 때마다 seed 를 처음 값으로 맞춰 다시 뽑음 (측정 모의 결과에는 영향 없음).
BOOL CRandDlg::OnInitDialog(VOID)
{
	(VOID)CDialogEx::OnInitDialog();

	f_Pcs_TestRand(&st_Uniform, &st_Gauss);
	st_UniformView.f_SetHist(&st_Uniform, 0);
	st_GaussView.f_SetHist(&st_Gauss, 1);

	return TRUE;
}
