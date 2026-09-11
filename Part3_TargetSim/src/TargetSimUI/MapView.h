//
// @file	MapView.h
// @brief	궤적 지도. CStatic 위에 경도(가로) · 위도(세로) 로 궤적을 그린다
// @author	hwan
// @date	2026.09.12.
//
#pragma once

#include <vector>

struct ST_Point
{
	double	dLat;		// [deg]
	double	dLon;		// [deg]
	double	dAlt;		// [m]
	double	dYaw;		// [deg]
};

typedef std::vector<std::vector<ST_Point>> PathList;	// [표적][스텝], 0 은 플랫폼


class CMapView : public CStatic
{
public:
	CMapView() : m_pPaths(nullptr), m_nStep(0) {}

	void	SetPaths(const PathList *pPaths)	{ m_pPaths = pPaths; m_nStep = 0; Invalidate(FALSE); }
	void	SetStep(int nStep)					{ m_nStep = nStep; Invalidate(FALSE); }

	static COLORREF ColorOf(int nIdx);

protected:
	const PathList	*m_pPaths;
	int				m_nStep;

	void	Draw(CDC &dc, const CRect &rc);

	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC *pDC);
	DECLARE_MESSAGE_MAP()
};
