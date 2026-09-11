//
// @file	SimPath.h
// @brief	실행 결과 한 점. 대화상자가 매 스텝 모아 두고 지도 · PPI 가 같이 읽는다
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
	double	dRange;		// 플랫폼에서 본 거리 [m]
	double	dAz;		// 플랫폼에서 본 방위 [deg, 북 0 · 시계방향]
	double	dEl;		// 플랫폼에서 본 고각 [deg]
};

typedef std::vector<std::vector<ST_Point>> PathList;	// [표적][스텝], 0 은 플랫폼
