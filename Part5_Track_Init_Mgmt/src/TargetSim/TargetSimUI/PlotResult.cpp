#include "pch.h"

#include <math.h>

#include "PlotResult.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// 위경도 -> 플랫폼 초기 위치 기준 동-북-위 좌표 (CSimResult 그림 좌표와 같은 기준).
static ST_PlotPoint f_Meas_ToPoint(const STRUCT_Coord_Lla *st_Lla, const ST_TargetState *st_Origin)
{
	const STRUCT_Coord_Rect	st_Ned = f_Tgt_LlaToNed(st_Lla, &st_Origin->st_PosEcef, &st_Origin->st_Lla);
	ST_PlotPoint			st_Point;

	st_Point.east	= st_Ned.y;
	st_Point.north	= st_Ned.x;
	st_Point.up		= -st_Ned.z;

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

// CTrackResult

// 트랙 상태 이름 (1 임시, 2 확정, 3 메모리, 4 삭제).
static const CHAR *f_Trk_StateName(INT32 nState)
{
	static const CHAR *s_Name[5] = { "-", "1 임시", "2 확정", "3 메모리", "4 삭제" };

	return ((nState >= TRK_STATE_TENTATIVE) && (nState <= TRK_STATE_DELETED)) ? s_Name[nState] : s_Name[0];
}

// 플롯 메시지 송수신 시작. 측정 모의 결과의 전체 스캔을 보냄. 지난 결과는 비움. 시작하면 1.
INT32 CTrackResult::f_Start(const CSimResult *st_NewSource, const CPlotResult *st_Measure, INT32 sendGapMs)
{
	const INT32	nScanNum = st_Measure->f_GetScanNum();
	INT32		isOk = 0;

	f_Lnk_Stop();

	st_Source	= st_NewSource;
	nScanTotal	= nScanNum;
	st_Status	= {};
	(VOID)memset(nSourceCount, 0, sizeof(nSourceCount));
	st_ScanBuf.clear();
	st_TrackStart.clear();
	st_TrackBuf.clear();
	st_PlotStart.clear();
	st_PlotBuf.clear();
	st_ScanIndex.assign(static_cast<UINT64>(nScanNum), -1);
	st_ViewTemp.resize(TRK_MAX_TRACK);
	st_PlotTemp.resize(IDD_MAX_PLOT);
	nCopiedNum	= 0;

	// 스캔이 없어도 불러 Core 의 지난 상태를 지움 (스캔 0 개면 시작하지 않고 0 반환)
	isOk = f_Lnk_Start((nScanNum > 0) ? st_Measure->f_GetScan(0) : nullptr, (nScanNum > 0) ? st_NewSource->f_GetSample(0) : nullptr, nScanNum,
		&st_Measure->st_Sigma, sendGapMs);
	f_Lnk_GetStatus(&st_Status);

	return isOk;
}

// 송수신 멈춤 (창을 닫을 때).
VOID CTrackResult::f_Stop(VOID)
{
	f_Lnk_Stop();
	f_Lnk_GetStatus(&st_Status);
}

// 진행 상태를 읽고, 처리가 끝났지만 아직 안 가져온 스캔을 가져옴. 새 스캔이 있으면 1.
// Core 는 보낸 범위 밖 스캔 번호를 처리하지 않지만, 그런 스캔이 오면 그림 좌표를 구할 수 없어 건너뜀.
INT32 CTrackResult::f_Poll(VOID)
{
	ST_LnkScan	st_Scan;
	INT32		isNew = 0;

	f_Lnk_GetStatus(&st_Status);

	while ((nCopiedNum < st_Status.nProcNum) && (f_Lnk_CopyScan(nCopiedNum, &st_Scan, st_ViewTemp.data(), st_PlotTemp.data()) != 0))
	{
		if (st_Scan.ScanNum < static_cast<UINT32>(nScanTotal))
		{
			f_AddScan(&st_Scan);
			isNew = 1;
		}

		nCopiedNum = nCopiedNum + 1;
	}

	return isNew;
}

// Core 에서 복사해 온 스캔 하나 (st_ViewTemp, st_PlotTemp) 를 그림 좌표로 바꿔 쌓음.
VOID CTrackResult::f_AddScan(const ST_LnkScan *st_Scan)
{
	ST_TrackItem	st_Item;
	ST_RecvPlot		st_Plot;
	FLOAT64			lead[3];
	INT32			nTrack;
	INT32			nPlot;
	INT32			nAxis;
	INT32			nSource[2];

	st_ScanIndex[st_Scan->ScanNum] = f_GetScanNum();
	st_ScanBuf.push_back(*st_Scan);
	st_TrackStart.push_back(static_cast<INT32>(st_TrackBuf.size()));
	st_PlotStart.push_back(static_cast<INT32>(st_PlotBuf.size()));

	for (nTrack = 0; nTrack < st_Scan->nTrackNum; nTrack++)
	{
		st_Item.st_View	= st_ViewTemp[static_cast<UINT64>(nTrack)];
		nSource[0]		= f_Lnk_GetPlotSource(st_Item.st_View.PlotId[0]);
		nSource[1]		= f_Lnk_GetPlotSource(st_Item.st_View.PlotId[1]);
		st_Item.nSource	= (nSource[0] == nSource[1]) ? nSource[0] : TRK_SOURCE_MIXED;

		for (nAxis = 0; nAxis < 3; nAxis++)
		{
			lead[nAxis] = st_Item.st_View.pos[nAxis] + (st_Item.st_View.vel[nAxis] * TRK_LEAD_TIME);
		}

		st_Item.st_Point	= f_ToPoint(st_Item.st_View.pos, st_Scan->ScanNum);
		st_Item.st_Lead		= f_ToPoint(lead, st_Scan->ScanNum);
		st_TrackBuf.push_back(st_Item);

		// 이번 스캔에 만든 트랙만 셈
		if (st_Item.st_View.nInitScan == static_cast<INT32>(st_Scan->ScanNum))
		{
			nSourceCount[(st_Item.nSource == TRK_SOURCE_MIXED) ? (TGT_MAX_TARGET_NUM + 1) : st_Item.nSource]++;
		}
	}

	for (nPlot = 0; nPlot < st_Scan->nPlotNum; nPlot++)
	{
		st_Plot.PlotId		= st_PlotTemp[static_cast<UINT64>(nPlot)].PlotId;
		st_Plot.nSource		= f_Lnk_GetPlotSource(st_Plot.PlotId);
		st_Plot.st_Point	= f_ToPoint(st_PlotTemp[static_cast<UINT64>(nPlot)].pos, st_Scan->ScanNum);
		st_PlotBuf.push_back(st_Plot);
	}
}

// 수신 쪽 위치 (스캔 시각 레이다 기준 VC-NED) -> 그림 좌표 (플랫폼 초기 위치 기준 동-북-위).
ST_PlotPoint CTrackResult::f_ToPoint(const FLOAT64 pt_Pos[3], UINT32 scanNum) const
{
	const ST_TargetState	*st_Radar = st_Source->f_GetState(static_cast<INT32>(scanNum), 0);
	const ST_TargetState	*st_Origin = st_Source->f_GetState(0, 0);
	const STRUCT_Coord_Rect	st_Ned = { pt_Pos[0], pt_Pos[1], pt_Pos[2] };
	const STRUCT_Coord_Lla	st_Lla = f_Tgt_NedToLla(&st_Ned, &st_Radar->st_PosEcef, &st_Radar->st_Lla);
	const STRUCT_Coord_Rect	st_Map = f_Tgt_LlaToNed(&st_Lla, &st_Origin->st_PosEcef, &st_Origin->st_Lla);
	ST_PlotPoint			st_Point;

	st_Point.east	= st_Map.y;
	st_Point.north	= st_Map.x;
	st_Point.up		= -st_Map.z;

	return st_Point;
}

// 받은 스캔 수.
INT32 CTrackResult::f_GetScanNum(VOID) const
{
	return static_cast<INT32>(st_ScanBuf.size());
}

// 받은 순서 nIndex 의 스캔 기록.
const ST_LnkScan *CTrackResult::f_GetScan(INT32 nIndex) const
{
	return &st_ScanBuf[static_cast<UINT64>(nIndex)];
}

// 스캔 번호 -> 받은 순서. 아직 안 왔거나 잃은 스캔이면 -1.
INT32 CTrackResult::f_FindScan(INT32 nScanNum) const
{
	return ((nScanNum >= 0) && (nScanNum < static_cast<INT32>(st_ScanIndex.size()))) ? st_ScanIndex[static_cast<UINT64>(nScanNum)] : -1;
}

// 스캔 처리 뒤 트랙 수.
INT32 CTrackResult::f_GetTrackNum(INT32 nIndex) const
{
	return st_ScanBuf[static_cast<UINT64>(nIndex)].nTrackNum;
}

// 스캔 처리 뒤 트랙 하나.
const ST_TrackItem *CTrackResult::f_GetTrack(INT32 nIndex, INT32 nTrack) const
{
	return &st_TrackBuf[static_cast<UINT64>(st_TrackStart[static_cast<UINT64>(nIndex)] + nTrack)];
}

// 스캔에 받은 플롯 수.
INT32 CTrackResult::f_GetPlotNum(INT32 nIndex) const
{
	return st_ScanBuf[static_cast<UINT64>(nIndex)].nPlotNum;
}

// 스캔에 받은 플롯 하나.
const ST_RecvPlot *CTrackResult::f_GetPlot(INT32 nIndex, INT32 nPlot) const
{
	return &st_PlotBuf[static_cast<UINT64>(st_PlotStart[static_cast<UINT64>(nIndex)] + nPlot)];
}

// 트랙 표 칸 하나의 글자. 화면 표, CSV 공용. CSV 는 상태, 참값을 숫자로 씀 (참값: k 표적 k, 0 클러터, -1 섞임).
VOID CTrackResult::f_FormatCell(INT32 nIndex, INT32 nTrack, INT32 nColumn, INT32 isCsv, CHAR *pt_Buf, INT32 bufSize) const
{
	const ST_TrackItem	*st_Item = f_GetTrack(nIndex, nTrack);
	const ST_TrkView	*st_View = &st_Item->st_View;
	const UINT64		size = static_cast<UINT64>(bufSize);

	if (nColumn == TRK_COL_ID)
	{
		(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "%u", st_View->TrackId);
	}
	else if ((nColumn == TRK_COL_STATE) && (isCsv == 0))
	{
		(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "%s", f_Trk_StateName(st_View->TrackState));
	}
	else if (nColumn == TRK_COL_STATE)
	{
		(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "%d", st_View->TrackState);
	}
	else if (nColumn == TRK_COL_SCORE)
	{
		(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "%d", st_View->TrackScore);
	}
	else if (nColumn == TRK_COL_MEMORY)
	{
		(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "%d", st_View->MemoryScore);
	}
	else if (nColumn == TRK_COL_FLAG)
	{
		(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "%d", st_View->Assigned_Flag);
	}
	else if (nColumn == TRK_COL_INIT_SCAN)
	{
		(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "%d", st_View->nInitScan);
	}
	else if ((nColumn == TRK_COL_PLOT_PREV) || (nColumn == TRK_COL_PLOT_CUR))
	{
		(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "%u", st_View->PlotId[nColumn - TRK_COL_PLOT_PREV]);
	}
	else if ((nColumn == TRK_COL_SOURCE) && (isCsv != 0))
	{
		(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "%d", st_Item->nSource);
	}
	else if (nColumn == TRK_COL_SOURCE)
	{
		if (st_Item->nSource > 0)
		{
			(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "표적 %d", st_Item->nSource);
		}
		else
		{
			(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "%s", (st_Item->nSource == 0) ? "클러터" : "섞임");
		}
	}
	else if ((nColumn >= TRK_COL_POS) && (nColumn < TRK_COL_VEL))
	{
		(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "%.1f", st_View->pos[nColumn - TRK_COL_POS]);
	}
	else if ((nColumn >= TRK_COL_VEL) && (nColumn < TRK_COL_SPEED))
	{
		(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "%.1f", st_View->vel[nColumn - TRK_COL_VEL]);
	}
	else
	{
		(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "%.1f",
			sqrt((st_View->vel[0] * st_View->vel[0]) + (st_View->vel[1] * st_View->vel[1]) + (st_View->vel[2] * st_View->vel[2])));
	}
}

// 받은 전체 스캔의 트랙 CSV 저장 (한 줄 = 스캔 하나의 트랙 하나). 파일 열기 실패 시 0 반환.
INT32 CTrackResult::f_WriteCsv(const CString &st_Path) const
{
	FILE	*st_File = nullptr;
	CHAR	pt_Cell[RES_CELL_SIZE];
	INT32	isWritten = 0;
	INT32	nIndex;
	INT32	nTrack;
	INT32	nColumn;

	(VOID)_wfopen_s(&st_File, st_Path.GetString(), L"wb");

	if (st_File != nullptr)
	{
		(VOID)fputs("scan,time_s,track_id,track_state,track_score,memory_score,assigned_flag,init_scan,init_plot_id_prev,init_plot_id_cur,source,"
			"north_m,east_m,down_m,v_north_mps,v_east_mps,v_down_mps,speed_mps\r\n", st_File);

		for (nIndex = 0; nIndex < f_GetScanNum(); nIndex++)
		{
			for (nTrack = 0; nTrack < f_GetTrackNum(nIndex); nTrack++)
			{
				(VOID)fprintf(st_File, "%u,%.3f", f_GetScan(nIndex)->ScanNum, f_GetScan(nIndex)->time);

				for (nColumn = 0; nColumn < TRK_COL_NUM; nColumn++)
				{
					f_FormatCell(nIndex, nTrack, nColumn, 1, pt_Cell, RES_CELL_SIZE);
					(VOID)fputc(',', st_File);
					(VOID)fputs(pt_Cell, st_File);
				}

				(VOID)fputs("\r\n", st_File);
			}
		}

		(VOID)fclose(st_File);
		isWritten = 1;
	}

	return isWritten;
}
