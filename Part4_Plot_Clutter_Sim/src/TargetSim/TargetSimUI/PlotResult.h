#pragma once

#include <vector>

#include "PlotSim.h"
#include "Scenario.h"

// 측정 모의 결과 (과제 4). Part 3 결과의 표본을 스캔마다 재서 만든 플롯, 클러터와 그 통계.
// 스캔은 0.1 s 구간마다 시작 시각에 한 번이라 60 s 면 600 스캔 (끝 시각 표본은 재지 않음).
class CPlotResult
{
public:
	VOID	f_Run(const CSimResult *st_Source, FLOAT64 snr, INT32 nClutterNum);

	INT32					f_GetScanNum(VOID) const;
	INT32					f_GetTargetNum(VOID) const;
	const ST_PcsScan *		f_GetScan(INT32 nScan) const;
	const ST_PlotPoint *	f_GetPlotPoint(INT32 nScan, INT32 nTarget) const;
	const ST_PlotPoint *	f_GetClutterPoint(INT32 nScan, INT32 nClutter) const;

	STRUCT_Coord_Sph		st_Sigma = {};					// 이론 표준편차 p [m, rad]
	STRUCT_Coord_Sph		st_ErrorStd = {};				// 플롯 오차 (z - x) 의 표준편차 [m, rad]
	INT32					nPlotNum = 0;
	INT32					nClutterSum = 0;

private:
	std::vector<ST_PcsScan>		st_ScanBuf;
	std::vector<ST_PlotPoint>	st_PlotPointBuf;			// 스캔마다 표적 수만큼
	std::vector<ST_PlotPoint>	st_ClutterPointBuf;			// 스캔마다 PCS_MAX_CLUTTER 칸
	INT32						nTargetNum = 0;
};
