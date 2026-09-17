#pragma once

#include "TargetSim_JH.h"

#define RES_CELL_SIZE			48

// 플랫폼 표본 0 을 원점으로 한 국지 ENU 수평 좌표 [m]
typedef struct
{
	FLOAT64		east;
	FLOAT64		north;
} ST_PlotPoint;

// 실행 결과. Core 를 돌려 표본과 그림 좌표를 채우고, 표·CSV 가 쓰는 서식을 한곳에서 맡는다.
class CSimResult
{
public:
	CSimResult() noexcept;
	~CSimResult();

	// 새 결과를 따로 만든다. 성공하면 1 이고 f_Commit 을 부를 때까지 지금 결과는 그대로다. 실패하면 0 이고 *st_Error 에 문구가 든다.
	INT32	f_Run(const ST_SimConfig *st_Config, CString *st_Error);
	VOID	f_Commit(VOID);

	INT32					f_GetSampleNum(VOID) const;
	INT32					f_GetObjectNum(VOID) const;
	INT32					f_GetColumnNum(VOID) const;
	FLOAT64					f_GetStepTime(VOID) const;
	FLOAT64					f_GetRunMs(VOID) const;
	const ST_SimSample *	f_GetSample(INT32 nStep) const;
	const ST_TargetState *	f_GetState(INT32 nStep, INT32 nObject) const;
	const ST_PlotPoint *	f_GetPoint(INT32 nStep, INT32 nObject) const;

	// 표와 CSV 가 함께 쓰는 셀 서식. 열 순서는 스텝, 시각, 객체마다 위도·경도·고도. 길이를 돌려주고 실패하면 -1.
	INT32	f_FormatCell(INT32 nRow, INT32 nColumn, CHAR *pt_Buf, INT32 bufSize) const;
	INT32	f_WriteCsv(const CString &st_Path, INT32 *pt_LineNum) const;

private:
	INT32	f_ComputePoints(const ST_SimSample *st_Sample, ST_PlotPoint *st_Point, INT32 nNewSampleNum, INT32 nNewObjectNum, CString *st_Error) const;

	ST_SimState		st_Sim;

	ST_SimSample	*st_SampleBuf;
	ST_PlotPoint	*st_PointBuf;
	INT32			nSampleNum;
	INT32			nObjectNum;
	FLOAT64			stepTime;
	FLOAT64			runMs;

	ST_SimSample	*st_PendingSample;
	ST_PlotPoint	*st_PendingPoint;
	INT32			nPendingSampleNum;
	INT32			nPendingObjectNum;
	FLOAT64			pendingStepTime;
	FLOAT64			pendingRunMs;
};
