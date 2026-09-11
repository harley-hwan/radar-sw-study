//
// @file	TargetSimUIDlg.cpp
// @brief	표적 궤적 모의 대화상자 구현
// @author	hwan
// @date	2026.09.12.
//
#include "pch.h"
#include "framework.h"
#include <cmath>
#include <cstdio>
#include "TargetSimUI.h"
#include "TargetSimUIDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define TIMER_PLAY		1
#define TIMER_PLAY_MS	100			// 한 틱 = 시뮬레이션 0.1 s 한 스텝 (1 배속일 때 실시간)

static const int s_anSpeed[] = { 1, 2, 5, 10 };		// 배속 = 한 틱에 진행하는 스텝 수


CTargetSimUIDlg::CTargetSimUIDlg(CWnd *pParent /*=nullptr*/)
	: CDialogEx(IDD_TARGETSIM_DIALOG, pParent)
	, m_bPlaying(FALSE)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CTargetSimUIDlg::DoDataExchange(CDataExchange *pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_TARGETS, m_listTargets);
	DDX_Control(pDX, IDC_LIST_STATE,   m_listState);
	DDX_Control(pDX, IDC_SLIDER_TIME,  m_slider);
	DDX_Control(pDX, IDC_COMBO_SPEED,  m_comboSpeed);
	DDX_Control(pDX, IDC_MAP,          m_map);
}

BEGIN_MESSAGE_MAP(CTargetSimUIDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_HSCROLL()
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_BTN_RUN,        &CTargetSimUIDlg::OnBnClickedRun)
	ON_BN_CLICKED(IDC_BTN_SAVE,       &CTargetSimUIDlg::OnBnClickedSave)
	ON_BN_CLICKED(IDC_BTN_PLAY,       &CTargetSimUIDlg::OnBnClickedPlay)
	ON_BN_CLICKED(IDC_CHECK_MANEUVER, &CTargetSimUIDlg::OnBnClickedManeuver)
END_MESSAGE_MAP()


BOOL CTargetSimUIDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	SetIcon(m_hIcon, TRUE);
	SetIcon(m_hIcon, FALSE);

	m_listTargets.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	m_listTargets.InsertColumn(0, _T("구분"),     LVCFMT_LEFT,  46);
	m_listTargets.InsertColumn(1, _T("위도"),     LVCFMT_RIGHT, 62);
	m_listTargets.InsertColumn(2, _T("경도"),     LVCFMT_RIGHT, 62);
	m_listTargets.InsertColumn(3, _T("고도 m"),   LVCFMT_RIGHT, 48);
	m_listTargets.InsertColumn(4, _T("속력 m/s"), LVCFMT_RIGHT, 56);
	m_listTargets.InsertColumn(5, _T("r/y/p"),   LVCFMT_RIGHT, 62);
	m_listTargets.InsertColumn(6, _T("기동 (g, 축, 시작~끝 s)"), LVCFMT_LEFT, 200);

	m_listState.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	m_listState.InsertColumn(0, _T("구분"),   LVCFMT_LEFT,  46);
	m_listState.InsertColumn(1, _T("위도"),   LVCFMT_RIGHT, 84);
	m_listState.InsertColumn(2, _T("경도"),   LVCFMT_RIGHT, 84);
	m_listState.InsertColumn(3, _T("고도 m"), LVCFMT_RIGHT, 64);
	m_listState.InsertColumn(4, _T("yaw"),    LVCFMT_RIGHT, 56);

	for (int n : s_anSpeed)
	{
		CString str;
		str.Format(_T("%d 배속"), n);
		m_comboSpeed.AddString(str);
	}
	m_comboSpeed.SetCurSel(1);

	Setup();
	return TRUE;
}


// 과제 조건으로 시나리오를 만들고 표에 보여 준다
void CTargetSimUIDlg::Setup()
{
	static const LPCTSTR apszAxis[] = { _T("-"), _T("roll"), _T("yaw"), _T("pitch") };
	CString str, strMan;

	StopPlay();
	f_SetAssignment(&m_stScn, IsDlgButtonChecked(IDC_CHECK_MANEUVER) == BST_CHECKED);

	m_listTargets.DeleteAllItems();
	for (int i = 0; i <= m_stScn.nTargetCnt; i++)
	{
		const ST_Target *pstTgt = (i == 0) ? &m_stScn.stPlatform : &m_stScn.astTarget[i - 1];

		if (i == 0) str = _T("플랫폼"); else str.Format(_T("표적 %d"), i);
		m_listTargets.InsertItem(i, str);
		str.Format(_T("%.4f"), RAD2DEG(pstTgt->stInitLla.Lat)); m_listTargets.SetItemText(i, 1, str);
		str.Format(_T("%.4f"), RAD2DEG(pstTgt->stInitLla.Lon)); m_listTargets.SetItemText(i, 2, str);
		str.Format(_T("%.0f"),  pstTgt->stInitLla.Alt);         m_listTargets.SetItemText(i, 3, str);
		str.Format(_T("%.0f"),  pstTgt->dVheading);             m_listTargets.SetItemText(i, 4, str);
		str.Format(_T("%.0f/%.0f/%.0f"), RAD2DEG(pstTgt->stInitAtt.Roll), RAD2DEG(pstTgt->stInitAtt.Yaw), RAD2DEG(pstTgt->stInitAtt.Pitch));
		m_listTargets.SetItemText(i, 5, str);
		strMan.Empty();
		for (int j = 0; j < pstTgt->nManeuverCnt; j++)
		{
			const ST_Maneuver *pstMan = &pstTgt->astManeuver[j];
			str.Format(_T("%s%+.1fg %s %.0f~%.0f"), (j ? _T(", ") : _T("")), pstMan->dGravity, apszAxis[pstMan->nTurnType], pstMan->dStartTime, pstMan->dEndTime);
			strMan += str;
		}
		m_listTargets.SetItemText(i, 6, strMan);
	}

	m_paths.clear();
	m_map.SetPaths(nullptr);
	m_listState.DeleteAllItems();
	m_slider.EnableWindow(FALSE);
	GetDlgItem(IDC_BTN_PLAY)->EnableWindow(FALSE);
	GetDlgItem(IDC_BTN_SAVE)->EnableWindow(FALSE);
	SetDlgItemText(IDC_STATIC_TIME, _T("t = 0.0 s"));
}


// 60 초를 0.1 초 간격으로 돌리며 매 스텝 위경도를 모은다
void CTargetSimUIDlg::Run()
{
	const int nSteps = static_cast<int>(std::floor(m_stScn.dSimTime / m_stScn.dDt + 0.5));
	auto record = [&]()
	{
		for (int i = 0; i <= m_stScn.nTargetCnt; i++)
		{
			const ST_Target *pstTgt = (i == 0) ? &m_stScn.stPlatform : &m_stScn.astTarget[i - 1];
			ST_Point pt = { RAD2DEG(pstTgt->stLla.Lat), RAD2DEG(pstTgt->stLla.Lon), pstTgt->stLla.Alt, RAD2DEG(pstTgt->stAtt.Yaw) };
			m_paths[i].push_back(pt);
		}
	};

	m_paths.assign(m_stScn.nTargetCnt + 1, std::vector<ST_Point>());
	f_StartScenario(&m_stScn);
	record();
	for (int k = 0; k < nSteps; k++)
	{
		f_StepScenario(&m_stScn, k * m_stScn.dDt);
		record();
	}

	m_map.SetPaths(&m_paths);
	m_slider.SetRange(0, nSteps, TRUE);
	m_slider.SetTicFreq(nSteps / 6);
	m_slider.EnableWindow(TRUE);
	GetDlgItem(IDC_BTN_PLAY)->EnableWindow(TRUE);
	GetDlgItem(IDC_BTN_SAVE)->EnableWindow(TRUE);
	m_slider.SetPos(0);
	ShowStep(0);
}


void CTargetSimUIDlg::ShowStep(int nStep)
{
	CString str;

	if (m_paths.empty())
	{
		return;
	}
	m_map.SetStep(nStep);
	m_listState.DeleteAllItems();
	for (int i = 0; i < static_cast<int>(m_paths.size()); i++)
	{
		const ST_Point &p = m_paths[i][nStep];

		if (i == 0) str = _T("플랫폼"); else str.Format(_T("표적 %d"), i);
		m_listState.InsertItem(i, str);
		str.Format(_T("%.6f"), p.dLat); m_listState.SetItemText(i, 1, str);
		str.Format(_T("%.6f"), p.dLon); m_listState.SetItemText(i, 2, str);
		str.Format(_T("%.2f"),  p.dAlt); m_listState.SetItemText(i, 3, str);
		str.Format(_T("%.1f"),  p.dYaw); m_listState.SetItemText(i, 4, str);
	}
	str.Format(_T("t = %.1f s"), nStep * m_stScn.dDt);
	SetDlgItemText(IDC_STATIC_TIME, str);
}


// 재생 : 타이머 한 틱마다 배속만큼 스텝을 넘긴다. 끝에 닿으면 멈춘다
void CTargetSimUIDlg::OnBnClickedPlay()
{
	if (m_bPlaying)
	{
		StopPlay();
		return;
	}
	if (m_paths.empty())
	{
		return;
	}
	if (m_slider.GetPos() >= m_slider.GetRangeMax())
	{
		m_slider.SetPos(0);
		ShowStep(0);
	}
	SetTimer(TIMER_PLAY, TIMER_PLAY_MS, nullptr);
	m_bPlaying = TRUE;
	SetDlgItemText(IDC_BTN_PLAY, _T("정지"));
}

void CTargetSimUIDlg::StopPlay()
{
	if (m_bPlaying)
	{
		KillTimer(TIMER_PLAY);
		m_bPlaying = FALSE;
		SetDlgItemText(IDC_BTN_PLAY, _T("재생"));
	}
}

void CTargetSimUIDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == TIMER_PLAY)
	{
		const int nSel  = m_comboSpeed.GetCurSel();
		const int nStep = m_slider.GetPos() + s_anSpeed[(nSel < 0) ? 0 : nSel];

		if (nStep >= m_slider.GetRangeMax())
		{
			m_slider.SetPos(m_slider.GetRangeMax());
			ShowStep(m_slider.GetRangeMax());
			StopPlay();
		}
		else
		{
			m_slider.SetPos(nStep);
			ShowStep(nStep);
		}
	}
	CDialogEx::OnTimer(nIDEvent);
}


void CTargetSimUIDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar)
{
	if ((pScrollBar != nullptr) && (pScrollBar->GetDlgCtrlID() == IDC_SLIDER_TIME))
	{
		StopPlay();
		ShowStep(m_slider.GetPos());
	}
	CDialogEx::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CTargetSimUIDlg::OnBnClickedManeuver()
{
	Setup();
}

void CTargetSimUIDlg::OnBnClickedRun()
{
	Setup();
	Run();
}


// t, id, 위도, 경도, 고도, yaw 를 한 줄씩
void CTargetSimUIDlg::OnBnClickedSave()
{
	CFileDialog dlg(FALSE, _T("csv"), _T("trajectory.csv"), OFN_OVERWRITEPROMPT, _T("CSV (*.csv)|*.csv||"), this);

	if (dlg.DoModal() != IDOK)
	{
		return;
	}
	FILE *fp = _wfopen(dlg.GetPathName(), L"w");
	if (fp == nullptr)
	{
		AfxMessageBox(_T("파일을 열 수 없습니다"));
		return;
	}
	fprintf(fp, "t,id,lat_deg,lon_deg,alt_m,yaw_deg\n");
	for (size_t k = 0; k < m_paths[0].size(); k++)
	{
		for (size_t i = 0; i < m_paths.size(); i++)
		{
			const ST_Point &p = m_paths[i][k];
			fprintf(fp, "%.1f,%d,%.9f,%.9f,%.4f,%.4f\n", k * m_stScn.dDt, static_cast<int>(i), p.dLat, p.dLon, p.dAlt, p.dYaw);
		}
	}
	fclose(fp);
}


void CTargetSimUIDlg::OnCancel()
{
	StopPlay();
	CDialogEx::OnCancel();
}

void CTargetSimUIDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this);
		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);
		CRect rect;
		GetClientRect(&rect);
		dc.DrawIcon((rect.Width() - GetSystemMetrics(SM_CXICON) + 1) / 2, (rect.Height() - GetSystemMetrics(SM_CYICON) + 1) / 2, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

HCURSOR CTargetSimUIDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}
