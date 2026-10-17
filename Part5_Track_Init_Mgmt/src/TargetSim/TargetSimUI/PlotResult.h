#pragma once

#include <vector>

#include "PlotLink.h"
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

// 트랙 표 열 (화면 표, CSV 공용)
#define TRK_COL_ID				0
#define TRK_COL_STATE			1
#define TRK_COL_SCORE			2
#define TRK_COL_MEMORY			3
#define TRK_COL_FLAG			4
#define TRK_COL_INIT_SCAN		5
#define TRK_COL_PLOT_PREV		6
#define TRK_COL_PLOT_CUR		7
#define TRK_COL_SOURCE			8
#define TRK_COL_POS				9						// 북, 동, 아래 세 칸
#define TRK_COL_VEL				12						// 북, 동, 아래 세 칸
#define TRK_COL_SPEED			15
#define TRK_COL_NUM				16

#define TRK_SOURCE_MIXED		(-1)					// 개시 플롯 두 개의 참값이 다름
#define TRK_LEAD_TIME			1.0						// [s] 그림의 속도 선 길이 (이 시간 동안 가는 거리)

// 트랙 하나 (그림, 표용). 위치는 받을 때 그림 좌표로 바꿔 둠.
typedef struct
{
	ST_TrkView				st_View;
	INT32					nSource;						// 개시 플롯 두 개의 참값. k = 표적 k, 0 = 클러터, TRK_SOURCE_MIXED = 서로 다름
	ST_PlotPoint			st_Point;						// 트랙 위치
	ST_PlotPoint			st_Lead;						// 속도로 TRK_LEAD_TIME 만큼 간 위치
} ST_TrackItem;

// 데이터 처리 쓰레드가 받은 플롯 하나
typedef struct
{
	UINT32					PlotId;
	INT32					nSource;						// 0 = 클러터, k = 표적 k (송신 쪽 기록, 화면 확인용)
	ST_PlotPoint			st_Point;
} ST_RecvPlot;

// 트랙 개시 · 관리 결과 (과제 5). 플롯 메시지 송수신 쓰레드 (Core) 가 처리한 스캔을 타이머로 받아 모아 둠.
// 수신 쪽 위치 (그 스캔 레이다 기준 VC-NED) 는 받을 때 그림 좌표 (플랫폼 초기 위치 기준) 로 바꿔 둠.
class CTrackResult
{
public:
	INT32	f_Start(const CSimResult *st_NewSource, const CPlotResult *st_Measure, INT32 sendGapMs);
	VOID	f_Stop(VOID);
	INT32	f_Poll(VOID);

	INT32					f_GetScanNum(VOID) const;
	const ST_LnkScan *		f_GetScan(INT32 nIndex) const;
	INT32					f_FindScan(INT32 nScanNum) const;
	INT32					f_GetTrackNum(INT32 nIndex) const;
	const ST_TrackItem *	f_GetTrack(INT32 nIndex, INT32 nTrack) const;
	INT32					f_GetPlotNum(INT32 nIndex) const;
	const ST_RecvPlot *		f_GetPlot(INT32 nIndex, INT32 nPlot) const;
	VOID					f_FormatCell(INT32 nIndex, INT32 nTrack, INT32 nColumn, INT32 isCsv, CHAR *pt_Buf, INT32 bufSize) const;
	INT32					f_WriteCsv(const CString &st_Path) const;

	ST_LnkStatus			st_Status = {};
	INT32					nScanTotal = 0;										// 보낼 스캔 수
	INT32					nSourceCount[TGT_MAX_TARGET_NUM + 2] = {};			// 만든 트랙의 참값별 수. [0] 클러터, [k] 표적 k, 마지막 칸 섞임

private:
	VOID					f_AddScan(const ST_LnkScan *st_Scan);
	ST_PlotPoint			f_ToPoint(const FLOAT64 pt_Pos[3], UINT32 scanNum) const;

	const CSimResult			*st_Source = nullptr;
	INT32						nCopiedNum = 0;				// Core 에서 가져온 스캔 수 (건너뛴 스캔 포함)
	std::vector<ST_LnkScan>		st_ScanBuf;
	std::vector<INT32>			st_TrackStart;				// 받은 스캔마다 st_TrackBuf 시작 자리
	std::vector<ST_TrackItem>	st_TrackBuf;
	std::vector<INT32>			st_PlotStart;
	std::vector<ST_RecvPlot>	st_PlotBuf;
	std::vector<INT32>			st_ScanIndex;				// 스캔 번호마다 받은 순서. 못 받았으면 -1
	std::vector<ST_TrkView>		st_ViewTemp;				// Core 에서 복사해 올 자리
	std::vector<ST_LnkPlot>		st_PlotTemp;
};
