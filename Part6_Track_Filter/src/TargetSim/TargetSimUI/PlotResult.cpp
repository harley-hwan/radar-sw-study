// 측정 모의 실행, 플롯 메시지 송수신 시작 · 결과 수집, 트랙 표 칸 글자 · CSV 저장
#include "pch.h"

#include <math.h>

#include <algorithm>

#include "PlotResult.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// 세 성분의 크기. 속도면 속력, 축별 표준편차면 RSS.
static FLOAT64 f_Meas_Norm(const FLOAT64 pt_Value[3])
{
	return sqrt((pt_Value[0] * pt_Value[0]) + (pt_Value[1] * pt_Value[1]) + (pt_Value[2] * pt_Value[2]));
}

// CPlotResult

// 측정 모의 실행 (클러터 없음) 후 플롯의 그림 좌표 계산
VOID CPlotResult::f_Run(const CSimResult *st_Source, FLOAT64 snr)
{
	const ST_GeoMap	*st_Map = st_Source->f_GetMap();
	const INT32		nScanNum = st_Source->f_GetSampleNum() - 1;
	INT32			nScan;
	INT32			nTarget;

	nTargetNum	= st_Source->f_GetObjectNum() - 1;
	st_Sigma	= f_Pcs_CalcSigma(snr);
	st_ScanBuf.resize(static_cast<UINT64>(nScanNum));
	f_Pcs_Measure(st_ScanBuf.data(), st_Source->f_GetSample(0), nScanNum, snr, SCN_CLUTTER_NUM);

	st_PlotPointBuf.clear();

	for (nScan = 0; nScan < nScanNum; nScan++)
	{
		for (nTarget = 0; nTarget < nTargetNum; nTarget++)
		{
			st_PlotPointBuf.push_back(f_Scn_LlaToPoint(st_Map, &st_ScanBuf[static_cast<UINT64>(nScan)].st_PlotLla[nTarget]));
		}
	}
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

// CTrackResult

// 트랙 상태 이름 (1 임시, 2 확정, 3 메모리, 4 삭제).
static const CHAR *f_Trk_StateName(INT32 nState)
{
	static const CHAR *s_Name[5] = { "-", "1 임시", "2 확정", "3 메모리", "4 삭제" };

	return ((nState >= TRK_STATE_TENTATIVE) && (nState <= TRK_STATE_DELETED)) ? s_Name[nState] : s_Name[0];
}

// 플롯 메시지 송수신 시작 (지난 결과는 비움)
INT32 CTrackResult::f_Start(const CSimResult *st_NewSource, const CPlotResult *st_Measure, INT32 sendGapMs, const ST_TrkParam *st_NewParam)
{
	const INT32	nScanNum = st_Measure->f_GetScanNum();
	INT32		isOk = 0;

	f_Lnk_Stop();

	st_Source	= st_NewSource;
	st_Param	= *st_NewParam;
	nScanTotal	= nScanNum;
	st_Status	= {};
	st_ScanBuf.clear();
	st_TrackStart.clear();
	st_TrackBuf.clear();
	st_PlotStart.clear();
	st_PlotBuf.clear();
	st_TrackIdBuf.clear();
	st_ScanIndex.assign(static_cast<UINT64>(nScanNum), -1);
	st_ViewTemp.resize(TRK_MAX_TRACK);
	st_PlotTemp.resize(IDD_MAX_PLOT);
	nCopiedNum	= 0;

	// 스캔이 없어도 불러 Core 의 지난 상태를 지움 (스캔 0 개면 시작하지 않고 0 반환)
	isOk = f_Lnk_Start((nScanNum > 0) ? st_Measure->f_GetScan(0) : nullptr, (nScanNum > 0) ? st_NewSource->f_GetSample(0) : nullptr, nScanNum,
		sendGapMs, &st_Param);
	f_Lnk_GetStatus(&st_Status);

	return isOk;
}

// 송수신 멈춤 (창을 닫을 때).
VOID CTrackResult::f_Stop(VOID)
{
	f_Lnk_Stop();
	f_Lnk_GetStatus(&st_Status);
}

// 처리가 끝난 스캔을 Core 에서 가져옴. 새 스캔이 있으면 1
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

// Core 에서 복사한 스캔 하나를 위경도 · 그림 좌표, 참값 대비 오차로 바꿔 쌓음
VOID CTrackResult::f_AddScan(const ST_LnkScan *st_Scan)
{
	const INT32			nScan = static_cast<INT32>(st_Scan->ScanNum);
	const INT32			isTarget = (st_Source->f_GetObjectNum() > 1) ? 1 : 0;
	STRUCT_Coord_Rect	st_True = { 0.0, 0.0, 0.0 };
	STRUCT_Coord_Lla	st_Lla;
	ST_TrackItem		st_Item;
	ST_RecvPlot			st_Plot;
	FLOAT64				pt_True[3];
	FLOAT64				lead[3];
	INT32				nTrack;
	INT32				nPlot;
	INT32				nAxis;
	INT32				isKnown;
	UINT64				nId;

	// 참값: 그 스캔의 표적 위치를 그 스캔 레이다의 VC-NED 로
	if (isTarget != 0)
	{
		st_True = f_Tgt_EcefToNed(&st_Source->f_GetState(nScan, 1)->st_PosEcef, &st_Source->f_GetState(nScan, 0)->st_PosEcef,
			&st_Source->f_GetState(nScan, 0)->st_Lla);
	}

	pt_True[0] = st_True.x;
	pt_True[1] = st_True.y;
	pt_True[2] = st_True.z;

	st_ScanIndex[static_cast<UINT64>(nScan)] = f_GetScanNum();
	st_ScanBuf.push_back(*st_Scan);
	st_TrackStart.push_back(static_cast<INT32>(st_TrackBuf.size()));
	st_PlotStart.push_back(static_cast<INT32>(st_PlotBuf.size()));

	for (nTrack = 0; nTrack < st_Scan->nTrackNum; nTrack++)
	{
		st_Item.st_View = st_ViewTemp[static_cast<UINT64>(nTrack)];

		for (nAxis = 0; nAxis < 3; nAxis++)
		{
			lead[nAxis]			= st_Item.st_View.pos[nAxis] + (st_Item.st_View.vel[nAxis] * TRK_LEAD_TIME);
			st_Item.err[nAxis]	= (isTarget != 0) ? (st_Item.st_View.pos[nAxis] - pt_True[nAxis]) : 0.0;
		}

		st_Item.errNorm	= f_Meas_Norm(st_Item.err);
		f_ToGeo(st_Item.st_View.pos, st_Scan->ScanNum, &st_Item.st_Point, &st_Lla);
		st_Item.latDeg	= f_Rad_To_Deg(st_Lla.Lat);
		st_Item.lonDeg	= f_Rad_To_Deg(st_Lla.Lon);
		st_Item.alt		= st_Lla.Alt;
		f_ToGeo(lead, st_Scan->ScanNum, &st_Item.st_Lead, &st_Lla);
		st_TrackBuf.push_back(st_Item);

		// 처음 나온 ID 는 색 순서에 추가
		isKnown = 0;

		for (nId = 0; nId < st_TrackIdBuf.size(); nId++)
		{
			isKnown = (st_TrackIdBuf[nId] == st_Item.st_View.TrackId) ? 1 : isKnown;
		}

		if (isKnown == 0)
		{
			st_TrackIdBuf.push_back(st_Item.st_View.TrackId);
		}
	}

	for (nPlot = 0; nPlot < st_Scan->nPlotNum; nPlot++)
	{
		st_Plot.PlotId	= st_PlotTemp[static_cast<UINT64>(nPlot)].PlotId;
		st_Plot.nSource	= f_Lnk_GetPlotSource(st_Plot.PlotId);
		f_ToGeo(st_PlotTemp[static_cast<UINT64>(nPlot)].pos, st_Scan->ScanNum, &st_Plot.st_Point, &st_Lla);
		st_PlotBuf.push_back(st_Plot);
	}
}

// VC-NED (그 스캔 레이다 기준) -> 위경도 · 고도, 그림 좌표
VOID CTrackResult::f_ToGeo(const FLOAT64 pt_Pos[3], UINT32 scanNum, ST_PlotPoint *st_Point, STRUCT_Coord_Lla *st_Lla) const
{
	const ST_TargetState	*st_Radar = st_Source->f_GetState(static_cast<INT32>(scanNum), 0);
	const STRUCT_Coord_Rect	st_Ned = { pt_Pos[0], pt_Pos[1], pt_Pos[2] };

	*st_Lla		= f_Tgt_NedToLla(&st_Ned, &st_Radar->st_PosEcef, &st_Radar->st_Lla);
	*st_Point	= f_Scn_LlaToPoint(st_Source->f_GetMap(), st_Lla);
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

// 스캔 nScanNum 까지 받은 마지막 스캔의 받은 순서 (이력 그림의 끝). 없으면 -1
INT32 CTrackResult::f_FindLastUpTo(INT32 nScanNum) const
{
	INT32 nScan = (nScanNum < static_cast<INT32>(st_ScanIndex.size())) ? nScanNum : (static_cast<INT32>(st_ScanIndex.size()) - 1);
	INT32 nIndex = -1;

	while ((nScan >= 0) && (nIndex < 0))
	{
		nIndex	= st_ScanIndex[static_cast<UINT64>(nScan)];
		nScan	= nScan - 1;
	}

	return nIndex;
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

// 트랙 ID 의 색 순서 (나온 순서, 0 부터). 모르는 ID 는 0
INT32 CTrackResult::f_GetColorIndex(UINT32 trackId) const
{
	INT32 nColor = 0;
	INT32 nId;

	for (nId = 0; nId < static_cast<INT32>(st_TrackIdBuf.size()); nId++)
	{
		nColor = (st_TrackIdBuf[static_cast<UINT64>(nId)] == trackId) ? nId : nColor;
	}

	return nColor;
}

// 스캔 nScanNum 까지의 누적: 생성 · 삭제 · 발산 판정 수, 트랙 ID 수, 참값 대비 위치 오차와 NIS (갱신 TRK_STEADY_UPDATES 번 뒤)
VOID CTrackResult::f_GetStats(INT32 nScanNum, ST_TrkStats *st_Stats) const
{
	const ST_LnkScan	*st_Scan;
	const ST_TrackItem	*st_Item;
	std::vector<UINT32>	st_Seen;
	FLOAT64				sumSq = 0.0;
	FLOAT64				sumNis = 0.0;
	INT32				nGateOut = 0;
	INT32				nIndex;
	INT32				nTrack;

	*st_Stats = {};

	for (nIndex = 0; (nIndex < f_GetScanNum()) && (static_cast<INT32>(f_GetScan(nIndex)->ScanNum) <= nScanNum); nIndex++)
	{
		st_Scan					= f_GetScan(nIndex);
		st_Stats->nCreatedNum	= st_Stats->nCreatedNum + st_Scan->st_Log.nCreatedNum;
		st_Stats->nDeletedNum	= st_Stats->nDeletedNum + st_Scan->st_Log.nDeletedNum;
		st_Stats->nDivergedNum	= st_Stats->nDivergedNum + st_Scan->st_Log.nDivergedNum;

		for (nTrack = 0; nTrack < f_GetTrackNum(nIndex); nTrack++)
		{
			st_Item = f_GetTrack(nIndex, nTrack);

			if (std::find(st_Seen.begin(), st_Seen.end(), st_Item->st_View.TrackId) == st_Seen.end())
			{
				st_Seen.push_back(st_Item->st_View.TrackId);
			}

			if (st_Item->st_View.nUpdateNum >= TRK_STEADY_UPDATES)
			{
				st_Stats->nSteadyNum	= st_Stats->nSteadyNum + 1;
				sumSq					= sumSq + (st_Item->errNorm * st_Item->errNorm);
				st_Stats->maxError		= (st_Item->errNorm > st_Stats->maxError) ? st_Item->errNorm : st_Stats->maxError;

				if (st_Item->st_View.nis >= 0.0)
				{
					st_Stats->nNisNum	= st_Stats->nNisNum + 1;
					sumNis				= sumNis + st_Item->st_View.nis;
					nGateOut			= (st_Item->st_View.nis > TRK_GATE_CHI2) ? (nGateOut + 1) : nGateOut;
				}
			}
		}
	}

	st_Stats->nTrackIdNum	= static_cast<INT32>(st_Seen.size());
	st_Stats->rmsError		= (st_Stats->nSteadyNum > 0) ? sqrt(sumSq / static_cast<FLOAT64>(st_Stats->nSteadyNum)) : 0.0;
	st_Stats->meanNis		= (st_Stats->nNisNum > 0) ? (sumNis / static_cast<FLOAT64>(st_Stats->nNisNum)) : 0.0;
	st_Stats->gateOutPct	= (st_Stats->nNisNum > 0) ? ((100.0 * static_cast<FLOAT64>(nGateOut)) / static_cast<FLOAT64>(st_Stats->nNisNum)) : 0.0;
}

// 트랙 표 칸 글자. σ위치는 P 위치 대각 세 축의 RSS, NIS 는 이번 스캔에 갱신하지 않았으면 -
VOID CTrackResult::f_FormatCell(INT32 nIndex, INT32 nTrack, INT32 nColumn, CHAR *pt_Buf, INT32 bufSize) const
{
	const ST_TrackItem	*st_Item = f_GetTrack(nIndex, nTrack);
	const ST_TrkView	*st_View = &st_Item->st_View;
	const UINT64		size = static_cast<UINT64>(bufSize);

	switch (nColumn)
	{
	case TRK_COL_ID:
		(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "%u", st_View->TrackId);
		break;

	case TRK_COL_STATE:
		(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "%s", f_Trk_StateName(st_View->TrackState));
		break;

	case TRK_COL_SCORE:
		(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "%d", st_View->TrackScore);
		break;

	case TRK_COL_MEMORY:
		(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "%d", st_View->MemoryScore);
		break;

	case TRK_COL_FLAG:
		(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "%d", st_View->Assigned_Flag);
		break;

	case TRK_COL_INIT_SCAN:
		(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "%d", st_View->nInitScan);
		break;

	case TRK_COL_PLOT_LAST:
		(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "%u", st_View->PlotIdLast);
		break;

	case TRK_COL_LAT:
		(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "%.6f", st_Item->latDeg);
		break;

	case TRK_COL_LON:
		(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "%.6f", st_Item->lonDeg);
		break;

	case TRK_COL_ALT:
		(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "%.1f", st_Item->alt);
		break;

	case TRK_COL_SPEED:
		(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "%.1f", f_Meas_Norm(st_View->vel));
		break;

	case TRK_COL_ACC:
		(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "%.2f", f_Meas_Norm(st_View->acc));
		break;

	case TRK_COL_POS_STD:
		(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "%.1f", f_Meas_Norm(st_View->posStd));
		break;

	case TRK_COL_NIS:
		if (st_View->nis < 0.0)
		{
			(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "-");
		}
		else
		{
			(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "%.2f", st_View->nis);
		}
		break;

	case TRK_COL_GATE_RUN:
		(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "%d", st_View->nGateOutRun);
		break;

	case TRK_COL_DIVERGE:
		(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "%d", st_View->nDivergeNum);
		break;

	case TRK_COL_ERROR:
		(VOID)_snprintf_s(pt_Buf, size, _TRUNCATE, "%.1f", st_Item->errNorm);
		break;

	default:
		// 없는 열
		pt_Buf[0] = '\0';
		break;
	}
}

// 받은 전체 스캔의 트랙 CSV 저장 (한 줄 = 스캔 하나의 트랙 하나). 첫 줄은 이번 실행의 파라미터. 파일 열기 실패 시 0 반환.
INT32 CTrackResult::f_WriteCsv(const CString &st_Path) const
{
	FILE				*st_File = nullptr;
	const ST_LnkScan	*st_Scan;
	const ST_TrackItem	*st_Item;
	const ST_TrkView	*st_View;
	INT32				isWritten = 0;
	INT32				nIndex;
	INT32				nTrack;

	(VOID)_wfopen_s(&st_File, st_Path.GetString(), L"wb");

	if (st_File != nullptr)
	{
		(VOID)fprintf(st_File, "# sigma_range_m=%g,sigma_az_deg=%g,sigma_el_deg=%g,singer_q_mps2=%g,singer_tau_s=%g,gate_chi2=%g,diverge_num=%d\r\n",
			st_Param.sigmaRange, st_Param.sigmaAzimuth, st_Param.sigmaElevation, st_Param.singerQ, st_Param.singerTau, TRK_GATE_CHI2, TRK_DIVERGE_NUM);
		(VOID)fputs("scan,time_s,track_id,track_state,track_score,memory_score,assigned_flag,init_scan,plot_id_last,update_num,"
			"lat_deg,lon_deg,alt_m,north_m,east_m,down_m,v_north_mps,v_east_mps,v_down_mps,a_north_mps2,a_east_mps2,a_down_mps2,"
			"pos_std_m,vel_std_mps,acc_std_mps2,nis,gate_out_run,gate_out_sum,diverge_num,err_north_m,err_east_m,err_down_m,err_m\r\n", st_File);

		for (nIndex = 0; nIndex < f_GetScanNum(); nIndex++)
		{
			st_Scan = f_GetScan(nIndex);

			for (nTrack = 0; nTrack < f_GetTrackNum(nIndex); nTrack++)
			{
				st_Item = f_GetTrack(nIndex, nTrack);
				st_View = &st_Item->st_View;

				(VOID)fprintf(st_File, "%u,%.3f,%u,%d,%d,%d,%d,%d,%u,%d,%.9f,%.9f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.4f,%.4f,%.4f,%.3f,%.3f,%.4f,%.4f,%d,%d,%d,"
					"%.3f,%.3f,%.3f,%.3f\r\n", st_Scan->ScanNum, st_Scan->time, st_View->TrackId, st_View->TrackState, st_View->TrackScore, st_View->MemoryScore,
					st_View->Assigned_Flag, st_View->nInitScan, st_View->PlotIdLast, st_View->nUpdateNum, st_Item->latDeg, st_Item->lonDeg, st_Item->alt,
					st_View->pos[0], st_View->pos[1], st_View->pos[2], st_View->vel[0], st_View->vel[1], st_View->vel[2], st_View->acc[0], st_View->acc[1],
					st_View->acc[2], f_Meas_Norm(st_View->posStd), f_Meas_Norm(st_View->velStd), f_Meas_Norm(st_View->accStd), st_View->nis,
					st_View->nGateOutRun, st_View->nGateOutSum, st_View->nDivergeNum, st_Item->err[0], st_Item->err[1], st_Item->err[2], st_Item->errNorm);
			}
		}

		(VOID)fclose(st_File);
		isWritten = 1;
	}

	return isWritten;
}
