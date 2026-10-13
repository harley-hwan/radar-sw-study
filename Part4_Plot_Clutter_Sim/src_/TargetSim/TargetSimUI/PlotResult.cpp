#include "pch.h"

#include <math.h>

#include "PlotResult.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// 위경도 -> 플랫폼 초기 위치 기준 동-북 평면 좌표 (CSimResult 그림 좌표와 같은 기준).
static ST_PlotPoint f_Meas_ToPoint(const STRUCT_Coord_Lla *st_Lla, const ST_TargetState *st_Origin)
{
	const STRUCT_Coord_Rect	st_Ned = f_Tgt_LlaToNed(st_Lla, &st_Origin->st_PosEcef, &st_Origin->st_Lla);
	ST_PlotPoint			st_Point;

	st_Point.east	= st_Ned.y;
	st_Point.north	= st_Ned.x;

	return st_Point;
}

// 합, 제곱합 -> 모집단 표준편차 (N 으로 나눔).
static FLOAT64 f_Meas_Std(FLOAT64 sum, FLOAT64 sumSq, INT32 nNum)
{
	FLOAT64 mean;
	FLOAT64 std = 0.0;

	if (nNum > 0)
	{
		mean	= sum / static_cast<FLOAT64>(nNum);
		std		= sqrt(fmax(0.0, (sumSq / static_cast<FLOAT64>(nNum)) - (mean * mean)));
	}

	return std;
}

// CPlotResult

// Part 3 결과로 측정 모의 실행 후 그림 좌표, 통계 계산.
// 표본은 CSimResult 안의 vector 에 이어져 있어 첫 표본 주소를 배열로 넘김.
VOID CPlotResult::f_Run(const CSimResult *st_Source, FLOAT64 snr, INT32 nClutterNum)
{
	const ST_TargetState	*st_Origin = st_Source->f_GetState(0, 0);
	const INT32				nScanNum = st_Source->f_GetSampleNum() - 1;
	FLOAT64					sum[3] = { 0.0, 0.0, 0.0 };
	FLOAT64					sumSq[3] = { 0.0, 0.0, 0.0 };
	INT32					nScan;
	INT32					nTarget;
	INT32					nClutter;
	INT32					nPart;

	nTargetNum	= st_Source->f_GetObjectNum() - 1;
	st_Sigma	= f_Pcs_CalcSigma(snr);
	st_ScanBuf.resize(static_cast<UINT64>(nScanNum));
	f_Pcs_Measure(st_ScanBuf.data(), st_Source->f_GetSample(0), nScanNum, snr, nClutterNum);

	st_PlotPointBuf.clear();
	st_ClutterPointBuf.assign(static_cast<UINT64>(nScanNum) * PCS_MAX_CLUTTER, ST_PlotPoint());
	nPlotNum	= 0;
	nClutterSum	= 0;

	for (nScan = 0; nScan < nScanNum; nScan++)
	{
		const ST_PcsScan *st_Scan = &st_ScanBuf[static_cast<UINT64>(nScan)];

		// 플롯 오차 z - x 는 성분마다 넣은 잡음 v 와 같음
		for (nTarget = 0; nTarget < nTargetNum; nTarget++)
		{
			const FLOAT64 diff[3] = { st_Scan->st_Plot[nTarget].r - st_Scan->st_True[nTarget].r, st_Scan->st_Plot[nTarget].az - st_Scan->st_True[nTarget].az,
				st_Scan->st_Plot[nTarget].el - st_Scan->st_True[nTarget].el };

			for (nPart = 0; nPart < 3; nPart++)
			{
				sum[nPart]		= sum[nPart] + diff[nPart];
				sumSq[nPart]	= sumSq[nPart] + (diff[nPart] * diff[nPart]);
			}

			st_PlotPointBuf.push_back(f_Meas_ToPoint(&st_Scan->st_PlotLla[nTarget], st_Origin));
		}

		for (nClutter = 0; nClutter < st_Scan->nClutterNum; nClutter++)
		{
			st_ClutterPointBuf[(static_cast<UINT64>(nScan) * PCS_MAX_CLUTTER) + static_cast<UINT64>(nClutter)] = f_Meas_ToPoint(&st_Scan->st_ClutterLla[nClutter], st_Origin);
		}

		nPlotNum	= nPlotNum + nTargetNum;
		nClutterSum	= nClutterSum + st_Scan->nClutterNum;
	}

	st_ErrorStd.r	= f_Meas_Std(sum[0], sumSq[0], nPlotNum);
	st_ErrorStd.az	= f_Meas_Std(sum[1], sumSq[1], nPlotNum);
	st_ErrorStd.el	= f_Meas_Std(sum[2], sumSq[2], nPlotNum);
}

// 스캔 수.
INT32 CPlotResult::f_GetScanNum(VOID) const
{
	return static_cast<INT32>(st_ScanBuf.size());
}

// 표적 수 (플랫폼 제외).
INT32 CPlotResult::f_GetTargetNum(VOID) const
{
	return nTargetNum;
}

// 스캔 하나의 측정 결과.
const ST_PcsScan *CPlotResult::f_GetScan(INT32 nScan) const
{
	return &st_ScanBuf[static_cast<UINT64>(nScan)];
}

// 스캔, 표적 하나의 플롯 그림 좌표. nTarget 0 = 표적 1.
const ST_PlotPoint *CPlotResult::f_GetPlotPoint(INT32 nScan, INT32 nTarget) const
{
	return &st_PlotPointBuf[static_cast<UINT64>((nScan * nTargetNum) + nTarget)];
}

// 스캔, 클러터 하나의 그림 좌표.
const ST_PlotPoint *CPlotResult::f_GetClutterPoint(INT32 nScan, INT32 nClutter) const
{
	return &st_ClutterPointBuf[static_cast<UINT64>((nScan * PCS_MAX_CLUTTER) + nClutter)];
}
