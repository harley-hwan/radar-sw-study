#include "pch.h"
#include "framework.h"

#include <math.h>

#include "Resource.h"
#include "RandDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// 확률밀도를 구한 과정 다섯 줄: 칸 나누기, 확률밀도 식, 예로 든 칸, 이론값, 평균 · 표준편차 식.
// 칸 개수는 PDF 에서 되돌려 구함 (개수 = PDF x 표본 수 x 칸 폭). 예로 드는 칸은 UNIRAN 은 첫 칸, GAUSS 는 0 에서 시작하는 칸.
static VOID f_Rand_CalcText(CString *st_Text, const ST_PcsHist *st_Hist, INT32 isGauss)
{
	const FLOAT64	denom = static_cast<FLOAT64>(PCS_RAND_NUM) * st_Hist->binWidth;		// 개수를 확률밀도로 바꿀 때 나누는 값
	const FLOAT64	binMax = st_Hist->binMin + (static_cast<FLOAT64>(st_Hist->nBin) * st_Hist->binWidth);
	const INT32		nDigits = (isGauss != 0) ? 2 : 1;										// 칸 경계 소수 자릿수 (0.25, 0.1)
	INT32			nExample = 0;
	CString			st_Line;

	if (isGauss != 0)
	{
		nExample = static_cast<INT32>(floor((0.0 - st_Hist->binMin) / st_Hist->binWidth));
		nExample = (nExample < 0) ? 0 : ((nExample < st_Hist->nBin) ? nExample : (st_Hist->nBin - 1));
	}

	const FLOAT64	left = st_Hist->binMin + (static_cast<FLOAT64>(nExample) * st_Hist->binWidth);
	const FLOAT64	pdf = st_Hist->pdf[nExample];
	const INT32		nCount = static_cast<INT32>(lround(pdf * denom));

	st_Text->Format(_T("칸 폭 %.*f 로 %d 칸 (%g ~ %g), 칸마다 개수를 셈\r\n"), nDigits, st_Hist->binWidth, st_Hist->nBin, st_Hist->binMin, binMax);
	st_Line.Format(_T("확률밀도 = 개수 ÷ (N × 칸 폭) = 개수 ÷ (%d × %.*f)\r\n"), PCS_RAND_NUM, nDigits, st_Hist->binWidth);
	*st_Text += st_Line;
	st_Line.Format(_T("예) %.*f ~ %.*f 칸: %d 개 → %d ÷ %.0f = %.3f\r\n"), nDigits, left, nDigits, left + st_Hist->binWidth, nCount, nCount, denom, pdf);
	*st_Text += st_Line;

	if (isGauss != 0)
	{
		const FLOAT64 center = left + (0.5 * st_Hist->binWidth);

		st_Line.Format(_T("이론 f(x) = exp(-x²/2) ÷ √(2π),  f(%.3f) = %.3f\r\n"), center, exp(-0.5 * center * center) / sqrt(2.0 * PI));
	}
	else
	{
		FLOAT64	pdfMin = st_Hist->pdf[0];
		FLOAT64	pdfMax = st_Hist->pdf[0];
		INT32	nIndex;

		for (nIndex = 1; nIndex < st_Hist->nBin; nIndex++)
		{
			pdfMin = fmin(pdfMin, st_Hist->pdf[nIndex]);
			pdfMax = fmax(pdfMax, st_Hist->pdf[nIndex]);
		}

		st_Line.Format(_T("%d 칸 모두 %.3f ~ %.3f  (이론 1)\r\n"), st_Hist->nBin, pdfMin, pdfMax);
	}

	*st_Text += st_Line;
	*st_Text += _T("평균 = Σx ÷ N,  표준편차 = √(Σx² ÷ N - 평균²)");
}

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
// 히스토그램 아래에 확률밀도를 구한 과정을 적음.
BOOL CRandDlg::OnInitDialog(VOID)
{
	CString st_Text;

	(VOID)CDialogEx::OnInitDialog();

	f_Pcs_TestRand(&st_Uniform, &st_Gauss);
	st_UniformView.f_SetHist(&st_Uniform, 0);
	st_GaussView.f_SetHist(&st_Gauss, 1);

	f_Rand_CalcText(&st_Text, &st_Uniform, 0);
	SetDlgItemText(IDC_RAND_UNIFORM_CALC, st_Text);
	f_Rand_CalcText(&st_Text, &st_Gauss, 1);
	SetDlgItemText(IDC_RAND_GAUSS_CALC, st_Text);

	return TRUE;
}
