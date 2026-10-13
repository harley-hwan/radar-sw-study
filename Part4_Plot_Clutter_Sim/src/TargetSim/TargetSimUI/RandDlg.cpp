#include "pch.h"
#include "framework.h"

#include <math.h>

#include "Resource.h"
#include "RandDlg.h"
#include "UiCommon.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define RAND_MARGIN				12						// [px @96dpi] 창 가장자리
#define RAND_GAP				20						// UNIRAN 쪽과 GAUSS 쪽 사이
#define RAND_LIST_GAP			10						// 뽑은 값 표와 칸 표 사이
#define RAND_ROW_GAP			8						// 히스토그램, 표, 계산 과정, 아래 줄 사이
#define RAND_HIST_HEIGHT		190
#define RAND_HIST_MIN_HEIGHT	130						// 화면이 작아 줄일 때 히스토그램 최소 높이
#define RAND_CELL_PAD			14						// 표 칸 글자 좌우 여백 합
#define RAND_CALC_SLACK			32						// 계산 과정 줄은 뽑을 때마다 숫자 자릿수가 달라 조금 넉넉히
#define RAND_CALC_LINE_NUM		4
#define RAND_BUTTON_WIDTH		96
#define RAND_COLOR_MARK			RGB(254, 243, 199)		// 계산 예로 든 표본, 칸의 줄 (히스토그램의 색칠한 막대와 같은 계열)

static const INT32	s_HistId[RAND_KIND_NUM]			= { IDC_RAND_UNIFORM, IDC_RAND_GAUSS };
static const INT32	s_SampleTitleId[RAND_KIND_NUM]	= { IDC_RAND_UNIFORM_SAMPLE_TITLE, IDC_RAND_GAUSS_SAMPLE_TITLE };
static const INT32	s_BinTitleId[RAND_KIND_NUM]		= { IDC_RAND_UNIFORM_BIN_TITLE, IDC_RAND_GAUSS_BIN_TITLE };
static const INT32	s_SampleId[RAND_KIND_NUM]		= { IDC_RAND_UNIFORM_SAMPLE, IDC_RAND_GAUSS_SAMPLE };
static const INT32	s_BinId[RAND_KIND_NUM]			= { IDC_RAND_UNIFORM_BIN, IDC_RAND_GAUSS_BIN };
static const INT32	s_CalcId[RAND_KIND_NUM]			= { IDC_RAND_UNIFORM_CALC, IDC_RAND_GAUSS_CALC };

// 둘 중 큰 값.
static INT32 f_Rand_Max(INT32 a, INT32 b)
{
	return (a > b) ? a : b;
}

// 컨트롤 위치, 크기 변경.
static VOID f_Rand_Move(CWnd *st_Dlg, INT32 ctrlId, INT32 left, INT32 top, INT32 width, INT32 height)
{
	(VOID)st_Dlg->GetDlgItem(ctrlId)->SetWindowPos(nullptr, left, top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

// 컨트롤 글자에서 가장 긴 줄의 폭 [px].
static INT32 f_Rand_TextWidth(CWnd *st_Ctrl)
{
	CClientDC	st_Dc(st_Ctrl);
	CFont		*st_OldFont = st_Dc.SelectObject(st_Ctrl->GetFont());
	CString		st_Text;
	CString		st_Line;
	INT32		width = 0;
	INT32		nLine;

	st_Ctrl->GetWindowText(st_Text);

	for (nLine = 0; AfxExtractSubString(st_Line, st_Text, nLine, _T('\n')) != FALSE; nLine++)
	{
		(VOID)st_Line.TrimRight(_T('\r'));
		width = f_Rand_Max(width, st_Dc.GetTextExtent(st_Line).cx);
	}

	(VOID)st_Dc.SelectObject(st_OldFont);

	return width;
}

// 고정 소수 자릿수 글자. 반올림해서 0 이 되는 음수 (-0.0000) 는 부호를 뗌.
static CString f_Rand_Fixed(FLOAT64 value, INT32 nDecimal)
{
	CString st_Text;

	st_Text.Format(_T("%.*f"), nDecimal, value);

	if ((st_Text.GetLength() > 1) && (st_Text[0] == _T('-')) && (st_Text.SpanIncluding(_T("-0.")).GetLength() == st_Text.GetLength()))
	{
		(VOID)st_Text.Delete(0, 1);
	}

	return st_Text;
}

// 소수 자릿수 아래를 버린 글자 (음수도 아래로). 반올림하면 0.79996 이 0.8000 으로 보여 칸 번호와 어긋나 보이므로 버림.
static CString f_Rand_Floor(FLOAT64 value, INT32 nDecimal)
{
	const FLOAT64 scale = pow(10.0, static_cast<FLOAT64>(nDecimal));

	return f_Rand_Fixed(floor(value * scale) / scale, nDecimal);
}

// 칸 범위 글자 "왼쪽 ~ 오른쪽". 자릿수는 칸 폭에 맞춤 (0.1 -> 1, 0.25 -> 2).
static CString f_Rand_BinRange(const ST_PcsHist *st_Hist, INT32 nBin, INT32 nDigits)
{
	const FLOAT64	left = st_Hist->binMin + (static_cast<FLOAT64>(nBin) * st_Hist->binWidth);
	CString			st_Text;

	st_Text.Format(_T("%s ~ %s"), f_Rand_Fixed(left, nDigits).GetString(), f_Rand_Fixed(left + st_Hist->binWidth, nDigits).GetString());

	return st_Text;
}

// 칸의 이론 확률밀도. UNIRAN 은 1, GAUSS 는 표준정규 곡선의 칸 가운데 높이 exp(-x²/2) ÷ √(2π).
static FLOAT64 f_Rand_TheoryPdf(const ST_PcsHist *st_Hist, INT32 nBin, INT32 isGauss)
{
	const FLOAT64 center = st_Hist->binMin + ((static_cast<FLOAT64>(nBin) + 0.5) * st_Hist->binWidth);

	return (isGauss != 0) ? (exp(-0.5 * center * center) / sqrt(2.0 * PI)) : 1.0;
}

// 열 하나 추가. 너비 = 제목과 가장 긴 칸 글자 중 넓은 쪽 + 여백.
static VOID f_Rand_AddColumn(CListCtrl *st_List, INT32 nColumn, LPCTSTR pt_Title, LPCTSTR pt_Widest, INT32 nFormat, INT32 pad)
{
	const INT32 width = f_Rand_Max(st_List->GetStringWidth(pt_Title), st_List->GetStringWidth(pt_Widest)) + pad;

	(VOID)st_List->InsertColumn(nColumn, pt_Title, nFormat, width);
}

// 열 너비 합.
static INT32 f_Rand_ColumnSum(CListCtrl *st_List)
{
	const INT32	nColumnNum = st_List->GetHeaderCtrl()->GetItemCount();
	INT32		nSum = 0;
	INT32		nColumn;

	for (nColumn = 0; nColumn < nColumnNum; nColumn++)
	{
		nSum = nSum + st_List->GetColumnWidth(nColumn);
	}

	return nSum;
}

// 열 너비를 같은 비율로 늘려 표 안쪽 너비를 채움. 마지막 열이 나머지를 받아 합이 딱 맞음 (가로 스크롤 막대가 안 생김).
static VOID f_Rand_FitColumns(CListCtrl *st_List, INT32 clientWidth)
{
	const INT32	nColumnNum = st_List->GetHeaderCtrl()->GetItemCount();
	const INT32	nSum = f_Rand_ColumnSum(st_List);
	INT32		nUsed = 0;
	INT32		width;
	INT32		nColumn;

	for (nColumn = 0; nColumn < (nColumnNum - 1); nColumn++)
	{
		width	= ::MulDiv(st_List->GetColumnWidth(nColumn), clientWidth, nSum);
		nUsed	= nUsed + width;
		(VOID)st_List->SetColumnWidth(nColumn, width);
	}

	(VOID)st_List->SetColumnWidth(nColumnNum - 1, clientWidth - nUsed);
}

// nRow 줄이 가운데쯤 오게 스크롤. 맨 끝으로 갔다가 위 줄로 올라오면 그 줄이 맨 위가 됨.
static VOID f_Rand_ScrollTo(CListCtrl *st_List, INT32 nRow)
{
	const INT32	nRowNum = st_List->GetItemCount();
	const INT32	nPage = st_List->GetCountPerPage();
	INT32		nTop = nRow - ((nPage - 1) / 2);

	if (nTop > (nRowNum - nPage))
	{
		nTop = nRowNum - nPage;
	}

	if (nTop < 0)
	{
		nTop = 0;
	}

	if (nRowNum > 0)
	{
		(VOID)st_List->EnsureVisible(nRowNum - 1, FALSE);
		(VOID)st_List->EnsureVisible(nTop, FALSE);
	}
}

// ① 뽑은 값 표: 번호, 값, 칸 번호 계산 ((값 - 칸 시작) ÷ 칸 폭), 칸. 칸 밖 값은 '밖'.
// 값과 계산은 자릿수 아래를 버려 적어, 보이는 값으로 다시 계산해도 같은 칸이 나오게 함.
static VOID f_Rand_FillSample(CListCtrl *st_List, const ST_PcsHist *st_Hist)
{
	CString	st_Text;
	INT32	nIndex;

	st_List->SetRedraw(FALSE);
	(VOID)st_List->DeleteAllItems();

	for (nIndex = 0; nIndex < PCS_RAND_SHOW; nIndex++)
	{
		const FLOAT64 value = st_Hist->sample[nIndex];

		st_Text.Format(_T("%d"), nIndex + 1);
		(VOID)st_List->InsertItem(nIndex, st_Text);
		(VOID)st_List->SetItemText(nIndex, 1, f_Rand_Floor(value, 4));
		(VOID)st_List->SetItemText(nIndex, 2, f_Rand_Floor((value - st_Hist->binMin) / st_Hist->binWidth, 2));

		if (st_Hist->nSampleBin[nIndex] >= 0)
		{
			st_Text.Format(_T("%d"), st_Hist->nSampleBin[nIndex]);
		}
		else
		{
			st_Text = _T("밖");
		}

		(VOID)st_List->SetItemText(nIndex, 3, st_Text);
	}

	st_List->SetRedraw(TRUE);
	st_List->Invalidate(FALSE);
}

// ② ③ 칸 표: 칸, 범위, 개수, 확률밀도 (개수 ÷ (N × 칸 폭)), 이론 확률밀도.
static VOID f_Rand_FillBin(CListCtrl *st_List, const ST_PcsHist *st_Hist, INT32 isGauss)
{
	const INT32	nRangeDigits = (isGauss != 0) ? 2 : 1;				// 칸 경계 자릿수 (0.25, 0.1)
	const INT32	nPdfDigits = (isGauss != 0) ? 4 : 3;				// 개수 ÷ 2500, 개수 ÷ 1000 이 딱 떨어지는 자릿수
	CString		st_Text;
	INT32		nBin;

	st_List->SetRedraw(FALSE);
	(VOID)st_List->DeleteAllItems();

	for (nBin = 0; nBin < st_Hist->nBin; nBin++)
	{
		st_Text.Format(_T("%d"), nBin);
		(VOID)st_List->InsertItem(nBin, st_Text);
		(VOID)st_List->SetItemText(nBin, 1, f_Rand_BinRange(st_Hist, nBin, nRangeDigits));
		st_Text.Format(_T("%d"), st_Hist->nCount[nBin]);
		(VOID)st_List->SetItemText(nBin, 2, st_Text);
		(VOID)st_List->SetItemText(nBin, 3, f_Rand_Fixed(st_Hist->pdf[nBin], nPdfDigits));
		(VOID)st_List->SetItemText(nBin, 4, f_Rand_Fixed(f_Rand_TheoryPdf(st_Hist, nBin, isGauss), nPdfDigits));
	}

	st_List->SetRedraw(TRUE);
	st_List->Invalidate(FALSE);
}

// 계산 과정 네 줄에 실제 값을 넣음: ② 칸 번호, ③ 예로 든 칸의 확률밀도 (이론값은 칸 표의 같은 줄), ④ 평균, ④ 표준편차.
static VOID f_Rand_CalcText(CString *st_Text, const ST_PcsHist *st_Hist, INT32 isGauss, INT32 nMarkBin)
{
	const FLOAT64	sqMean = st_Hist->sumSq / static_cast<FLOAT64>(PCS_RAND_NUM);			// Σx² ÷ N
	const FLOAT64	meanSq = st_Hist->mean * st_Hist->mean;									// 평균²
	const INT32		nPdfDigits = (isGauss != 0) ? 4 : 3;
	const CString	st_Sum = f_Rand_Fixed(st_Hist->sum, 4);									// 보이는 숫자로 다시 계산해도
	const CString	st_Mean = f_Rand_Fixed(st_Hist->mean, 4);								// 평균 · 표준편차 끝자리가 같게
	const CString	st_SqMean = f_Rand_Fixed(sqMean, 7);									// 넉넉한 자릿수로 적음
	const CString	st_MeanSq = f_Rand_Fixed(meanSq, 7);
	const CString	st_Std = f_Rand_Fixed(st_Hist->std, 4);
	CString			st_Line;

	if (isGauss != 0)
	{
		st_Text->Format(_T("② 칸 번호 = (값 + %g) ÷ %g 의 정수 부분 (±%g 밖은 세지 않음: %d 개)\r\n"), -st_Hist->binMin, st_Hist->binWidth, PCS_GAUSS_MAX,
			st_Hist->nOutNum);
	}
	else
	{
		st_Text->Format(_T("② 칸 번호 = 값 ÷ %g 의 정수 부분\r\n"), st_Hist->binWidth);
	}

	st_Line.Format(_T("③ 확률밀도 = 개수 ÷ (N × 칸 폭) → 칸 %d: %d ÷ (%d × %g) = %s\r\n"), nMarkBin, st_Hist->nCount[nMarkBin], PCS_RAND_NUM,
		st_Hist->binWidth, f_Rand_Fixed(st_Hist->pdf[nMarkBin], nPdfDigits).GetString());
	*st_Text += st_Line;

	if (isGauss != 0)
	{
		st_Line.Format(_T("④ 평균 = Σx ÷ N = %s ÷ %d = %s  (이론 0)\r\n④ 표준편차 = √(Σx² ÷ N - 평균²) = √(%s - %s) = %s  (이론 1)"),
			st_Sum.GetString(), PCS_RAND_NUM, st_Mean.GetString(), st_SqMean.GetString(), st_MeanSq.GetString(), st_Std.GetString());
	}
	else
	{
		st_Line.Format(_T("④ 평균 = Σx ÷ N = %s ÷ %d = %s  (이론 0.5)\r\n④ 표준편차 = √(Σx² ÷ N - 평균²) = √(%s - %s) = %s  (이론 %.4f)"),
			st_Sum.GetString(), PCS_RAND_NUM, st_Mean.GetString(), st_SqMean.GetString(), st_MeanSq.GetString(), st_Std.GetString(), 1.0 / sqrt(12.0));
	}

	*st_Text += st_Line;
}

BEGIN_MESSAGE_MAP(CRandDlg, CDialogEx)
	ON_BN_CLICKED(IDC_RAND_REDRAW, &CRandDlg::f_OnRedrawClicked)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_RAND_UNIFORM_SAMPLE, &CRandDlg::f_OnListCustomDraw)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_RAND_UNIFORM_BIN, &CRandDlg::f_OnListCustomDraw)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_RAND_GAUSS_SAMPLE, &CRandDlg::f_OnListCustomDraw)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_RAND_GAUSS_BIN, &CRandDlg::f_OnListCustomDraw)
	ON_NOTIFY(LVN_ITEMCHANGING, IDC_RAND_UNIFORM_SAMPLE, &CRandDlg::f_OnListItemChanging)
	ON_NOTIFY(LVN_ITEMCHANGING, IDC_RAND_UNIFORM_BIN, &CRandDlg::f_OnListItemChanging)
	ON_NOTIFY(LVN_ITEMCHANGING, IDC_RAND_GAUSS_SAMPLE, &CRandDlg::f_OnListItemChanging)
	ON_NOTIFY(LVN_ITEMCHANGING, IDC_RAND_GAUSS_BIN, &CRandDlg::f_OnListItemChanging)
END_MESSAGE_MAP()

// 난수 확인 창.
CRandDlg::CRandDlg(CWnd *st_Parent)
	: CDialogEx(IDD_RAND_DIALOG, st_Parent)
	, st_Hist()
	, nMarkSample()
	, nMarkBin()
	, dpi(UI_BASE_DPI)
	, textHeight(16)
{
}

// 컨트롤 변수와 대화상자 자원 연결.
VOID CRandDlg::DoDataExchange(CDataExchange *st_Dx)
{
	INT32 nKind;

	CDialogEx::DoDataExchange(st_Dx);

	for (nKind = 0; nKind < RAND_KIND_NUM; nKind++)
	{
		DDX_Control(st_Dx, s_HistId[nKind], st_HistView[nKind]);
		DDX_Control(st_Dx, s_SampleId[nKind], st_SampleList[nKind]);
		DDX_Control(st_Dx, s_BinId[nKind], st_BinList[nKind]);
	}
}

// 글꼴, 표 열 준비, 첫 뽑기, 배치. 처음 초점은 닫기 단추 (표에 초점 테두리가 생기지 않게).
BOOL CRandDlg::OnInitDialog(VOID)
{
	CClientDC	st_Dc(this);
	CFont		*st_OldFont;
	TEXTMETRIC	st_Metric;
	LOGFONT		st_LogFont;
	INT32		nKind;

	(VOID)CDialogEx::OnInitDialog();

	dpi			= st_Dc.GetDeviceCaps(LOGPIXELSX);
	st_OldFont	= st_Dc.SelectObject(GetFont());
	(VOID)st_Dc.GetTextMetrics(&st_Metric);
	(VOID)st_Dc.SelectObject(st_OldFont);
	textHeight	= st_Metric.tmHeight;

	// 표 제목은 굵게
	(VOID)GetFont()->GetLogFont(&st_LogFont);
	st_LogFont.lfWeight = FW_BOLD;
	(VOID)st_TitleFont.CreateFontIndirect(&st_LogFont);

	for (nKind = 0; nKind < RAND_KIND_NUM; nKind++)
	{
		GetDlgItem(s_SampleTitleId[nKind])->SetFont(&st_TitleFont);
		GetDlgItem(s_BinTitleId[nKind])->SetFont(&st_TitleFont);
	}

	f_SetupLists();
	f_Draw();
	f_Layout();

	// 표 높이가 정해진 뒤 예로 든 칸이 보이게 다시 스크롤
	for (nKind = 0; nKind < RAND_KIND_NUM; nKind++)
	{
		f_Rand_ScrollTo(&st_BinList[nKind], nMarkBin[nKind]);
	}

	GotoDlgCtrl(GetDlgItem(IDOK));

	return FALSE;
}

// 두 쪽 표의 열. 너비는 제목과 가장 긴 칸 글자에 맞춤 (f_Layout 이 표 너비에 맞춰 늘림).
VOID CRandDlg::f_SetupLists(VOID)
{
	const INT32	pad = f_Ui_Scale(RAND_CELL_PAD, dpi);
	CString		st_Title;
	INT32		nKind;

	for (nKind = 0; nKind < RAND_KIND_NUM; nKind++)
	{
		CListCtrl *st_Sample = &st_SampleList[nKind];
		CListCtrl *st_Bin = &st_BinList[nKind];

		// 칸 번호 계산 열 제목: (값 - 칸 시작) ÷ 칸 폭
		if (nKind == RAND_KIND_GAUSS)
		{
			st_Title.Format(_T("(값 + %g) ÷ %g"), -PCS_GAUSS_MIN, (PCS_GAUSS_MAX - PCS_GAUSS_MIN) / static_cast<FLOAT64>(PCS_GAUSS_BIN));
		}
		else
		{
			st_Title.Format(_T("값 ÷ %g"), 1.0 / static_cast<FLOAT64>(PCS_UNIFORM_BIN));
		}

		(VOID)st_Sample->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
		f_Rand_AddColumn(st_Sample, 0, _T("#"), _T("00"), LVCFMT_LEFT, pad);
		f_Rand_AddColumn(st_Sample, 1, _T("값"), _T("-0.0000"), LVCFMT_RIGHT, pad);
		f_Rand_AddColumn(st_Sample, 2, st_Title, _T("00.00"), LVCFMT_RIGHT, pad);
		f_Rand_AddColumn(st_Sample, 3, _T("칸"), _T("00"), LVCFMT_CENTER, pad);

		(VOID)st_Bin->SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
		f_Rand_AddColumn(st_Bin, 0, _T("칸"), _T("00"), LVCFMT_LEFT, pad);
		f_Rand_AddColumn(st_Bin, 1, _T("범위"), _T("-0.00 ~ -0.00"), LVCFMT_CENTER, pad);
		f_Rand_AddColumn(st_Bin, 2, _T("개수"), _T("00000"), LVCFMT_RIGHT, pad);
		f_Rand_AddColumn(st_Bin, 3, _T("확률밀도"), _T("0.0000"), LVCFMT_RIGHT, pad);
		f_Rand_AddColumn(st_Bin, 4, _T("이론"), _T("0.0000"), LVCFMT_RIGHT, pad);
	}
}

// 배치와 창 크기. 쪽 너비는 두 표 (글자에 맞춘 열 너비 합), 표 제목, 계산 과정 줄 중 가장 넓은 것에 맞추고,
// 창은 내용에 맞춘 뒤 부모 가운데에 둠. 화면 작업 영역보다 높으면 히스토그램 높이를 줄임.
// 열 너비를 늘려 놓으므로 OnInitDialog 에서 한 번만 부름.
VOID CRandDlg::f_Layout(VOID)
{
	const INT32	margin = f_Ui_Scale(RAND_MARGIN, dpi);
	const INT32	gap = f_Ui_Scale(RAND_GAP, dpi);
	const INT32	listGap = f_Ui_Scale(RAND_LIST_GAP, dpi);
	const INT32	rowGap = f_Ui_Scale(RAND_ROW_GAP, dpi);
	const INT32	tight = f_Ui_Scale(4, dpi);
	const INT32	buttonWidth = f_Ui_Scale(RAND_BUTTON_WIDTH, dpi);
	const INT32	buttonHeight = textHeight + f_Ui_Scale(12, dpi);
	const INT32	titleHeight = textHeight + f_Ui_Scale(2, dpi);
	const INT32	calcHeight = (RAND_CALC_LINE_NUM * textHeight) + tight;
	const INT32	scrollWidth = ::GetSystemMetrics(SM_CXVSCROLL);
	CRect		st_Border(0, 0, 0, 0);
	CRect		st_Header(0, 0, 0, 0);
	CRect		st_Row(0, 0, 0, 0);
	CRect		st_Window(0, 0, 0, 0);
	CRect		st_Frame(0, 0, 0, 0);
	CRect		st_Work(0, 0, 0, 0);
	INT32		histHeight = f_Ui_Scale(RAND_HIST_HEIGHT, dpi);
	INT32		overHeight;
	INT32		sampleWidth = 0;
	INT32		binWidth = 0;
	INT32		calcWidth = 0;
	INT32		panelWidth;
	INT32		listHeight;
	INT32		clientWidth;
	INT32		left;
	INT32		x;
	INT32		y = margin;
	INT32		nKind;

	// 표 테두리, 머리글, 줄 높이. 네 표는 글꼴과 모양이 같아 첫 표로 잼. 앞 표본 10 줄이 스크롤 없이 다 보이는 높이.
	(VOID)::AdjustWindowRectEx(&st_Border, st_SampleList[0].GetStyle(), FALSE, st_SampleList[0].GetExStyle());
	st_SampleList[0].GetHeaderCtrl()->GetWindowRect(&st_Header);
	(VOID)st_SampleList[0].GetItemRect(0, &st_Row, LVIR_BOUNDS);
	listHeight = st_Border.Height() + st_Header.Height() + (PCS_RAND_SHOW * st_Row.Height()) + f_Ui_Scale(2, dpi);

	// 쪽 너비. 칸 표는 세로 스크롤 막대 자리 포함 (GAUSS 는 32 칸). 두 쪽은 같은 너비.
	for (nKind = 0; nKind < RAND_KIND_NUM; nKind++)
	{
		sampleWidth	= f_Rand_Max(sampleWidth, f_Rand_ColumnSum(&st_SampleList[nKind]) + st_Border.Width());
		sampleWidth	= f_Rand_Max(sampleWidth, f_Rand_TextWidth(GetDlgItem(s_SampleTitleId[nKind])));
		binWidth	= f_Rand_Max(binWidth, f_Rand_ColumnSum(&st_BinList[nKind]) + st_Border.Width() + scrollWidth);
		binWidth	= f_Rand_Max(binWidth, f_Rand_TextWidth(GetDlgItem(s_BinTitleId[nKind])));
		calcWidth	= f_Rand_Max(calcWidth, f_Rand_TextWidth(GetDlgItem(s_CalcId[nKind])) + f_Ui_Scale(RAND_CALC_SLACK, dpi));
	}

	panelWidth = sampleWidth + listGap + binWidth;

	// 계산 과정 줄이 더 넓으면 남는 너비는 칸 표에 줌
	if (calcWidth > panelWidth)
	{
		binWidth	= binWidth + (calcWidth - panelWidth);
		panelWidth	= calcWidth;
	}

	clientWidth = (2 * margin) + (2 * panelWidth) + gap;

	// 창 높이 (테두리, 제목 표시줄 포함) 가 화면 작업 영역보다 크면 넘치는 만큼 히스토그램을 줄임 (최소 높이까지)
	(VOID)::AdjustWindowRectEx(&st_Frame, GetStyle(), FALSE, GetExStyle());
	(VOID)::SystemParametersInfo(SPI_GETWORKAREA, 0, &st_Work, 0);
	overHeight = st_Frame.Height() + (2 * margin) + histHeight + titleHeight + tight + listHeight + calcHeight + buttonHeight + (3 * rowGap) - st_Work.Height();

	if (overHeight > 0)
	{
		histHeight = f_Rand_Max(histHeight - overHeight, f_Ui_Scale(RAND_HIST_MIN_HEIGHT, dpi));
	}

	for (nKind = 0; nKind < RAND_KIND_NUM; nKind++)
	{
		left	= margin + (nKind * (panelWidth + gap));
		y		= margin;
		f_Rand_Move(this, s_HistId[nKind], left, y, panelWidth, histHeight);

		y = y + histHeight + rowGap;
		f_Rand_Move(this, s_SampleTitleId[nKind], left, y, sampleWidth, titleHeight);
		f_Rand_Move(this, s_BinTitleId[nKind], left + sampleWidth + listGap, y, binWidth, titleHeight);

		// 열을 먼저 표 안쪽 너비에 맞춘 뒤 표 크기를 바꿈 (가로 스크롤 막대가 잠깐이라도 생기지 않게)
		y = y + titleHeight + tight;
		f_Rand_FitColumns(&st_SampleList[nKind], sampleWidth - st_Border.Width());
		f_Rand_FitColumns(&st_BinList[nKind], binWidth - st_Border.Width() - ((st_BinList[nKind].GetItemCount() > PCS_RAND_SHOW) ? scrollWidth : 0));
		f_Rand_Move(this, s_SampleId[nKind], left, y, sampleWidth, listHeight);
		f_Rand_Move(this, s_BinId[nKind], left + sampleWidth + listGap, y, binWidth, listHeight);

		y = y + listHeight + rowGap;
		f_Rand_Move(this, s_CalcId[nKind], left, y, panelWidth, calcHeight);
	}

	// 아래 줄: 안내 글, 다시 뽑기, 닫기
	y = y + calcHeight + rowGap;
	x = clientWidth - margin - buttonWidth;
	f_Rand_Move(this, IDOK, x, y, buttonWidth, buttonHeight);
	x = x - f_Ui_Scale(8, dpi) - buttonWidth;
	f_Rand_Move(this, IDC_RAND_REDRAW, x, y, buttonWidth, buttonHeight);
	f_Rand_Move(this, IDC_RAND_INFO, margin, y, x - gap - margin, buttonHeight);

	// 창 크기 = 내용 + 테두리, 제목 표시줄
	st_Window.SetRect(0, 0, clientWidth, y + buttonHeight + margin);
	(VOID)::AdjustWindowRectEx(&st_Window, GetStyle(), FALSE, GetExStyle());
	(VOID)SetWindowPos(nullptr, 0, 0, st_Window.Width(), st_Window.Height(), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	CenterWindow();
}

// 10,000 개씩 새로 뽑아 히스토그램, 두 표, 계산 과정, 아래 줄을 채움.
// 계산 예로 쓸 표본은 칸에 들어간 첫 표본 (모두 칸 밖이면 가운데 칸만 예로 듦).
VOID CRandDlg::f_Draw(VOID)
{
	CString	st_Text;
	INT32	nKind;
	INT32	nIndex;

	f_Pcs_TestRand(&st_Hist[RAND_KIND_UNIFORM], &st_Hist[RAND_KIND_GAUSS]);

	for (nKind = 0; nKind < RAND_KIND_NUM; nKind++)
	{
		const ST_PcsHist	*st_Cur = &st_Hist[nKind];
		const INT32			isGauss = (nKind == RAND_KIND_GAUSS) ? 1 : 0;

		nMarkSample[nKind]	= -1;
		nMarkBin[nKind]		= st_Cur->nBin / 2;

		for (nIndex = 0; (nIndex < PCS_RAND_SHOW) && (nMarkSample[nKind] < 0); nIndex++)
		{
			if (st_Cur->nSampleBin[nIndex] >= 0)
			{
				nMarkSample[nKind]	= nIndex;
				nMarkBin[nKind]		= st_Cur->nSampleBin[nIndex];
			}
		}

		st_HistView[nKind].f_SetHist(st_Cur, isGauss, nMarkBin[nKind]);
		f_Rand_FillSample(&st_SampleList[nKind], st_Cur);
		f_Rand_FillBin(&st_BinList[nKind], st_Cur, isGauss);
		f_Rand_ScrollTo(&st_BinList[nKind], nMarkBin[nKind]);
		f_Rand_CalcText(&st_Text, st_Cur, isGauss, nMarkBin[nKind]);
		SetDlgItemText(s_CalcId[nKind], st_Text);
	}

	st_Text.Format(_T("창을 열거나 [다시 뽑기] 를 누를 때마다 지난번에 멈춘 SEED 에서 이어서 새로 뽑음.   이번 시작 SEED: UNIRAN %lld, GAUSS %lld"),
		st_Hist[RAND_KIND_UNIFORM].seedStart, st_Hist[RAND_KIND_GAUSS].seedStart);
	SetDlgItemText(IDC_RAND_INFO, st_Text);
}

// 다시 뽑기. 창 배치는 그대로 두고 값만 바꿈.
VOID CRandDlg::f_OnRedrawClicked(VOID)
{
	f_Draw();
}

// 표 줄 색칠: 계산 예로 든 표본, 칸.
VOID CRandDlg::f_OnListCustomDraw(NMHDR *st_Hdr, LRESULT *pt_Result)
{
	NMLVCUSTOMDRAW *st_Draw = reinterpret_cast<NMLVCUSTOMDRAW *>(st_Hdr);

	*pt_Result = CDRF_DODEFAULT;

	if (st_Draw->nmcd.dwDrawStage == CDDS_PREPAINT)
	{
		*pt_Result = CDRF_NOTIFYITEMDRAW;
	}
	else if ((st_Draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
		&& (static_cast<INT32>(st_Draw->nmcd.dwItemSpec) == f_GetMarkRow(static_cast<INT32>(st_Hdr->idFrom))))
	{
		st_Draw->clrTextBk = RAND_COLOR_MARK;
	}
	else
	{
		// 기본 그리기
	}
}

// 표 줄은 고를 수 없게 함. 고른 줄의 바탕색이 계산 예로 칠한 줄을 가리지 않게.
VOID CRandDlg::f_OnListItemChanging(NMHDR *st_Hdr, LRESULT *pt_Result)
{
	const NMLISTVIEW *st_Change = reinterpret_cast<NMLISTVIEW *>(st_Hdr);

	*pt_Result = (((st_Change->uChanged & LVIF_STATE) != 0U) && ((st_Change->uNewState & LVIS_SELECTED) != 0U)) ? TRUE : FALSE;
}

// 표 컨트롤 번호 -> 색칠할 줄. 없으면 -1.
INT32 CRandDlg::f_GetMarkRow(INT32 ctrlId) const
{
	INT32 nRow = -1;
	INT32 nKind;

	for (nKind = 0; nKind < RAND_KIND_NUM; nKind++)
	{
		if (ctrlId == s_SampleId[nKind])
		{
			nRow = nMarkSample[nKind];
		}
		else if (ctrlId == s_BinId[nKind])
		{
			nRow = nMarkBin[nKind];
		}
		else
		{
			// 다른 표
		}
	}

	return nRow;
}
