//
// @file	PpiView.h
// @brief	PPI 스코프. 플랫폼을 가운데 두고 스윕이 돌며, 스윕이 지나간 표적이 플롯으로 남는다
// @author	hwan
// @date	2026.09.12.
//
#pragma once

#include "SimPath.h"

class CPpiView : public CStatic
{
public:
	CPpiView() : m_pPaths(nullptr), m_nStep(0), m_dDt(0.1), m_dRing(5000.0), m_dScale(15000.0) {}

	void	SetPaths(const PathList *pPaths, double dDt);
	void	SetStep(int nStep)		{ m_nStep = nStep; Invalidate(FALSE); }

protected:
	const PathList	*m_pPaths;
	int				m_nStep;
	double			m_dDt;			// 스텝 간격 [s]
	double			m_dRing;		// 거리 링 간격 [m]
	double			m_dScale;		// 화면 끝 거리 [m]

	void	Draw(CDC &dc, const CRect &rc);

	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC *pDC);
	DECLARE_MESSAGE_MAP()
};
