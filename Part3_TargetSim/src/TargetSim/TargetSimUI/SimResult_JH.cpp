#include "pch.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SimResult_JH.h"
#include "Scenario_JH.h"
#include "UiNumber_JH.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define RES_CSV_BUFFER_SIZE		1048576U
#define RES_BYTE_PER_MB			1048576.0

static INT32 f_Res_IsStateFinite(const ST_TargetState *st_State)
{
	INT32 isFinite = 0;

	if (isfinite(st_State->simTime) && isfinite(st_State->st_Lla.lat) && isfinite(st_State->st_Lla.lon) &&
		isfinite(st_State->st_Lla.alt) && isfinite(st_State->st_PosEcef.x) && isfinite(st_State->st_PosEcef.y) &&
		isfinite(st_State->st_PosEcef.z) && isfinite(st_State->st_VelEcef.x) && isfinite(st_State->st_VelEcef.y) &&
		isfinite(st_State->st_VelEcef.z))
	{
		isFinite = 1;
	}

	return isFinite;
}

CSimResult::CSimResult() noexcept
	: st_SampleBuf(nullptr)
	, st_PointBuf(nullptr)
	, nSampleNum(0)
	, nObjectNum(0)
	, stepTime(0.0)
	, runMs(0.0)
	, st_PendingSample(nullptr)
	, st_PendingPoint(nullptr)
	, nPendingSampleNum(0)
	, nPendingObjectNum(0)
	, pendingStepTime(0.0)
	, pendingRunMs(0.0)
{
	(VOID)memset(&st_Sim, 0, sizeof(st_Sim));
}

CSimResult::~CSimResult()
{
	free(st_SampleBuf);
	free(st_PointBuf);
	free(st_PendingSample);
	free(st_PendingPoint);
}

VOID CSimResult::f_Commit(VOID)
{
	if (st_PendingSample != nullptr)
	{
		free(st_SampleBuf);
		free(st_PointBuf);

		st_SampleBuf		= st_PendingSample;
		st_PointBuf			= st_PendingPoint;
		nSampleNum			= nPendingSampleNum;
		nObjectNum			= nPendingObjectNum;
		stepTime			= pendingStepTime;
		runMs				= pendingRunMs;

		st_PendingSample	= nullptr;
		st_PendingPoint		= nullptr;
		nPendingSampleNum	= 0;
		nPendingObjectNum	= 0;
	}
}

INT32 CSimResult::f_Run(const ST_SimConfig *st_Config, CString *st_Error)
{
	ST_SimSample	*st_NewSample = nullptr;
	ST_PlotPoint	*st_NewPoint = nullptr;
	EN_TgtStatus	enStatus;
	LARGE_INTEGER	st_Frequency;
	LARGE_INTEGER	st_Begin;
	LARGE_INTEGER	st_End;
	UINT64			sampleBytes;
	UINT64			pointBytes;
	INT32			nNewSampleNum = 0;
	INT32			nNewObjectNum = 0;
	INT32			nStep;
	INT32			isOk = 1;

	st_Error->Empty();
	(VOID)::QueryPerformanceFrequency(&st_Frequency);
	(VOID)::QueryPerformanceCounter(&st_Begin);

	// 앞선 실행이 반영되지 않고 남았으면 버린다.
	free(st_PendingSample);
	free(st_PendingPoint);
	st_PendingSample	= nullptr;
	st_PendingPoint		= nullptr;

	// Core 호출 계약: 0 으로 채운 상태로 초기화하고, 초기화가 실패하면 진행하지 않는다.
	(VOID)memset(&st_Sim, 0, sizeof(st_Sim));
	enStatus = f_Tgt_InitSim(&st_Sim, st_Config);

	if (enStatus != TGT_OK)
	{
		*st_Error = CString(_T("초기화 실패: ")) + CScenario::f_StatusText(enStatus, 0);
		isOk = 0;
	}
	// 초기화가 성공했으면 스텝 수는 1 ~ TGT_MAX_STEP_NUM 이다. 버퍼 크기를 정하기 전에 한 번 더 확인한다.
	else if ((st_Sim.nStepNum < 1) || (st_Sim.nStepNum > TGT_MAX_STEP_NUM))
	{
		*st_Error = CString(_T("초기화 실패: ")) + CScenario::f_StatusText(TGT_ERR_SIM_STATE, 0);
		isOk = 0;
	}
	else
	{
		nNewSampleNum	= st_Sim.nStepNum + 1;
		nNewObjectNum	= st_Config->nTargetNum + 1;
		sampleBytes		= static_cast<UINT64>(nNewSampleNum) * static_cast<UINT64>(sizeof(ST_SimSample));
		pointBytes		= static_cast<UINT64>(nNewSampleNum) * static_cast<UINT64>(nNewObjectNum) * static_cast<UINT64>(sizeof(ST_PlotPoint));

		// 최악 116 MB 라 MFC new 의 예외 대신 calloc 의 NULL 로 실패를 받는다.
		st_NewSample	= static_cast<ST_SimSample *>(calloc(static_cast<UINT64>(nNewSampleNum), sizeof(ST_SimSample)));
		st_NewPoint		= static_cast<ST_PlotPoint *>(calloc(static_cast<UINT64>(nNewSampleNum) * static_cast<UINT64>(nNewObjectNum), sizeof(ST_PlotPoint)));

		if ((st_NewSample == nullptr) || (st_NewPoint == nullptr))
		{
			st_Error->Format(_T("메모리 부족: 결과 버퍼에 약 %.0f MB 가 필요합니다"), static_cast<FLOAT64>(sampleBytes + pointBytes) / RES_BYTE_PER_MB);
			isOk = 0;
		}
	}

	if (isOk != 0)
	{
		st_NewSample[0] = st_Sim.st_Sample;

		for (nStep = 1; (nStep < nNewSampleNum) && (isOk != 0); nStep++)
		{
			enStatus = f_Tgt_StepSim(&st_Sim);

			if (enStatus == TGT_OK)
			{
				st_NewSample[nStep] = st_Sim.st_Sample;
			}
			else
			{
				st_Error->Format(_T("스텝 %d (t = %s s) 진행 실패: "), nStep,
					f_Num_ToText(static_cast<FLOAT64>(nStep) * st_Config->stepTime, 3).GetString());
				*st_Error += CScenario::f_StatusText(enStatus, 0);
				isOk = 0;
			}
		}
	}

	if (isOk != 0)
	{
		isOk = f_ComputePoints(st_NewSample, st_NewPoint, nNewSampleNum, nNewObjectNum, st_Error);
	}

	if (isOk != 0)
	{
		(VOID)::QueryPerformanceCounter(&st_End);

		st_PendingSample	= st_NewSample;
		st_PendingPoint		= st_NewPoint;
		nPendingSampleNum	= nNewSampleNum;
		nPendingObjectNum	= nNewObjectNum;
		pendingStepTime		= st_Config->stepTime;
		pendingRunMs		= (st_Frequency.QuadPart > 0) ?
			((static_cast<FLOAT64>(st_End.QuadPart - st_Begin.QuadPart) * 1000.0) / static_cast<FLOAT64>(st_Frequency.QuadPart)) : 0.0;
	}
	else
	{
		free(st_NewSample);
		free(st_NewPoint);
	}

	return isOk;
}

INT32 CSimResult::f_ComputePoints(const ST_SimSample *st_Sample, ST_PlotPoint *st_Point, INT32 nNewSampleNum, INT32 nNewObjectNum, CString *st_Error) const
{
	const ST_TargetState	*st_Origin = &st_Sample[0].st_Platform;
	const ST_TargetState	*st_State;
	ST_Matrix				st_Dcm;
	ST_CoordRect			st_Diff;
	ST_CoordRect			st_Ned;
	INT32					isOk = 1;
	INT32					nStep;
	INT32					nObject;

	// 기준 DCM 은 한 번만 만든다. 점마다 f_Trans_Ecef_To_Ned 를 부르면 매번 새로 만든다.
	if ((f_Res_IsStateFinite(st_Origin) == 0) || (f_Coord_Dcm_Ned_To_Ecef(&st_Dcm, st_Origin->st_Lla.lat, st_Origin->st_Lla.lon) != COORD_OK))
	{
		*st_Error = _T("스텝 0 플랫폼: 결과에 유한하지 않은 값이 있거나 기준 좌표를 만들 수 없습니다");
		isOk = 0;
	}

	for (nStep = 0; (nStep < nNewSampleNum) && (isOk != 0); nStep++)
	{
		for (nObject = 0; (nObject < nNewObjectNum) && (isOk != 0); nObject++)
		{
			st_State = (nObject == 0) ? &st_Sample[nStep].st_Platform : &st_Sample[nStep].st_Target[nObject - 1];

			// 표·CSV·그림에 nan, inf 가 나오지 않게 하는 마지막 방어선
			if (f_Res_IsStateFinite(st_State) == 0)
			{
				if (nObject == 0)
				{
					st_Error->Format(_T("스텝 %d 플랫폼: 결과에 유한하지 않은 값이 있어 버렸습니다"), nStep);
				}
				else
				{
					st_Error->Format(_T("스텝 %d 표적 %d: 결과에 유한하지 않은 값이 있어 버렸습니다"), nStep, nObject);
				}

				isOk = 0;
			}
			else if ((f_Coord_VecSub(&st_Diff, &st_State->st_PosEcef, &st_Origin->st_PosEcef) != COORD_OK) ||
					 (f_Coord_RotateVecInv(&st_Ned, &st_Dcm, &st_Diff) != COORD_OK))
			{
				st_Error->Format(_T("스텝 %d: 그림 좌표 변환에 실패했습니다"), nStep);
				isOk = 0;
			}
			else
			{
				st_Point[(nStep * nNewObjectNum) + nObject].east	= st_Ned.y;
				st_Point[(nStep * nNewObjectNum) + nObject].north	= st_Ned.x;
			}
		}
	}

	return isOk;
}

INT32 CSimResult::f_GetSampleNum(VOID) const
{
	return nSampleNum;
}

INT32 CSimResult::f_GetObjectNum(VOID) const
{
	return nObjectNum;
}

INT32 CSimResult::f_GetColumnNum(VOID) const
{
	return (nSampleNum > 0) ? (2 + (3 * nObjectNum)) : 0;
}

FLOAT64 CSimResult::f_GetStepTime(VOID) const
{
	return stepTime;
}

FLOAT64 CSimResult::f_GetRunMs(VOID) const
{
	return runMs;
}

const ST_SimSample *CSimResult::f_GetSample(INT32 nStep) const
{
	const ST_SimSample *st_Sample = nullptr;

	if ((st_SampleBuf != nullptr) && (nStep >= 0) && (nStep < nSampleNum))
	{
		st_Sample = &st_SampleBuf[nStep];
	}

	return st_Sample;
}

const ST_TargetState *CSimResult::f_GetState(INT32 nStep, INT32 nObject) const
{
	const ST_SimSample		*st_Sample = f_GetSample(nStep);
	const ST_TargetState	*st_State = nullptr;

	if ((st_Sample != nullptr) && (nObject >= 0) && (nObject < nObjectNum))
	{
		st_State = (nObject == 0) ? &st_Sample->st_Platform : &st_Sample->st_Target[nObject - 1];
	}

	return st_State;
}

const ST_PlotPoint *CSimResult::f_GetPoint(INT32 nStep, INT32 nObject) const
{
	const ST_PlotPoint *st_Point = nullptr;

	if ((st_PointBuf != nullptr) && (nStep >= 0) && (nStep < nSampleNum) && (nObject >= 0) && (nObject < nObjectNum))
	{
		st_Point = &st_PointBuf[(nStep * nObjectNum) + nObject];
	}

	return st_Point;
}

INT32 CSimResult::f_FormatCell(INT32 nRow, INT32 nColumn, CHAR *pt_Buf, INT32 bufSize) const
{
	const ST_SimSample		*st_Sample;
	const ST_TargetState	*st_State;
	INT32					nLength = -1;
	INT32					nObject;
	INT32					nPart;

	if ((pt_Buf != nullptr) && (bufSize > 0))
	{
		pt_Buf[0] = '\0';
	}

	if ((pt_Buf != nullptr) && (bufSize > 1) && (st_SampleBuf != nullptr) && (nRow >= 0) && (nRow < nSampleNum) &&
		(nColumn >= 0) && (nColumn < (2 + (3 * nObjectNum))))
	{
		st_Sample = &st_SampleBuf[nRow];

		if (nColumn == 0)
		{
			nLength = _snprintf_s(pt_Buf, static_cast<UINT64>(bufSize), _TRUNCATE, "%d", st_Sample->nStepIndex);
		}
		else if (nColumn == 1)
		{
			nLength = f_Num_FormatFixed(pt_Buf, bufSize, st_Sample->simTime, 3);
		}
		else
		{
			nObject		= (nColumn - 2) / 3;
			nPart		= (nColumn - 2) % 3;
			st_State	= (nObject == 0) ? &st_Sample->st_Platform : &st_Sample->st_Target[nObject - 1];

			if (nPart == 0)
			{
				nLength = f_Num_FormatFixed(pt_Buf, bufSize, f_Rad_To_Deg(st_State->st_Lla.lat), 9);
			}
			else if (nPart == 1)
			{
				nLength = f_Num_FormatFixed(pt_Buf, bufSize, f_Rad_To_Deg(st_State->st_Lla.lon), 9);
			}
			else
			{
				nLength = f_Num_FormatFixed(pt_Buf, bufSize, st_State->st_Lla.alt, 4);
			}
		}
	}

	return nLength;
}

INT32 CSimResult::f_WriteCsv(const CString &st_Path, INT32 *pt_LineNum) const
{
	const INT32		nColumnNum = 2 + (3 * nObjectNum);
	FILE			*st_File = nullptr;
	CHAR			pt_Cell[RES_CELL_SIZE];
	INT32			errorCode;
	INT32			nLineNum = 0;
	INT32			nObject;
	INT32			nRow;
	INT32			nColumn;

	errorCode = _wfopen_s(&st_File, st_Path.GetString(), _T("wb"));

	if ((errorCode == 0) && (st_File != nullptr))
	{
		(VOID)setvbuf(st_File, nullptr, _IOFBF, RES_CSV_BUFFER_SIZE);

		// 가로형: 한 줄이 한 시각이라 최악 100002 줄로 엑셀 행 한계 안에 든다.
		(VOID)fputs("step,time_s", st_File);

		for (nObject = 0; nObject < nObjectNum; nObject++)
		{
			if (nObject == 0)
			{
				(VOID)fputs(",platform_lat_deg,platform_lon_deg,platform_alt_m", st_File);
			}
			else
			{
				(VOID)fprintf(st_File, ",target%d_lat_deg,target%d_lon_deg,target%d_alt_m", nObject, nObject, nObject);
			}
		}

		(VOID)fputs("\r\n", st_File);
		nLineNum = 1;

		for (nRow = 0; nRow < nSampleNum; nRow++)
		{
			for (nColumn = 0; nColumn < nColumnNum; nColumn++)
			{
				if (nColumn > 0)
				{
					(VOID)fputc(',', st_File);
				}

				if (f_FormatCell(nRow, nColumn, pt_Cell, RES_CELL_SIZE) > 0)
				{
					(VOID)fputs(pt_Cell, st_File);
				}
			}

			(VOID)fputs("\r\n", st_File);
			nLineNum = nLineNum + 1;
		}

		if (ferror(st_File) != 0)
		{
			errorCode = (errno != 0) ? errno : EIO;
		}

		if ((fclose(st_File) != 0) && (errorCode == 0))
		{
			errorCode = (errno != 0) ? errno : EIO;
		}

		// 반쯤 쓴 파일을 남기지 않는다.
		if (errorCode != 0)
		{
			(VOID)_wremove(st_Path.GetString());
		}
	}
	else if (errorCode == 0)
	{
		errorCode = EIO;
	}
	else
	{
		nLineNum = 0;
	}

	*pt_LineNum = nLineNum;

	return errorCode;
}
