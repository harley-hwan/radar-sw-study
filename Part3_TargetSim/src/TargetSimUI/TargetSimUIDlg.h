//
// @file	TargetSimUIDlg.h
// @brief	표적 궤적 모의 대화상자. 계산은 C 코어(TargetSimCore) 가 하고 여기서는 돌리고 보여 주기만 한다
// @author	hwan
// @date	2026.09.12.
//
#pragma once

#include "target_sim.h"
#undef BOOL					// 제공받은 Define.h 가 BOOL 을 bool 로 정의해서 MFC 의 BOOL(int) 로 되돌린다
#include "MapView.h"

class CTargetSimUIDlg : public CDialogEx
{
public:
	CTargetSimUIDlg(CWnd *pParent = nullptr);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_TARGETSIM_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange *pDX);

	HICON		m_hIcon;
	CListCtrl	m_listTargets;
	CListCtrl	m_listState;
	CSliderCtrl	m_slider;
	CComboBox	m_comboSpeed;
	CMapView	m_map;

	ST_Scenario	m_stScn;
	PathList	m_paths;		// 실행 결과. [0] 플랫폼, [1..] 표적
	BOOL		m_bPlaying;

	void	Setup();
	void	Run();
	void	ShowStep(int nStep);
	void	StopPlay();

	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnBnClickedRun();
	afx_msg void OnBnClickedSave();
	afx_msg void OnBnClickedPlay();
	afx_msg void OnBnClickedManeuver();
	virtual void OnOK() {}
	virtual void OnCancel();
	DECLARE_MESSAGE_MAP()
};
