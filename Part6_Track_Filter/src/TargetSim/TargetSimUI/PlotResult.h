// 측정 모의 결과 (CPlotResult) 와 추적 결과 (CTrackResult) 보관
#pragma once

#include <vector>

#include "PlotLink.h"
#include "PlotSim.h"
#include "Scenario.h"

// 측정 모의 결과: 스캔마다 표적 플롯 (116 s = 1160 스캔, 클러터 없음)
class CPlotResult
{
public:
	VOID	f_Run(const CSimResult *st_Source, FLOAT64 snr);

	INT32					f_GetScanNum(VOID) const;
	INT32					f_GetTargetNum(VOID) const;
	const ST_PcsScan *		f_GetScan(INT32 nScan) const;
	const ST_PlotPoint *	f_GetPlotPoint(INT32 nScan, INT32 nTarget) const;

	STRUCT_Coord_Sph		st_Sigma = {};					// 플롯 모의 잡음 표준편차 (과제 4 SNR, R 과 별개)

private:
	std::vector<ST_PcsScan>		st_ScanBuf;
	std::vector<ST_PlotPoint>	st_PlotPointBuf;			// 스캔마다 표적 수만큼
	INT32						nTargetNum = 0;
};

// 트랙 표 열 (화면 표)
#define TRK_COL_ID				0
#define TRK_COL_STATE			1
#define TRK_COL_SCORE			2
#define TRK_COL_MEMORY			3
#define TRK_COL_FLAG			4
#define TRK_COL_INIT_SCAN		5
#define TRK_COL_PLOT_LAST		6						// 마지막으로 갱신에 쓴 플롯
#define TRK_COL_LAT				7
#define TRK_COL_LON				8
#define TRK_COL_ALT				9
#define TRK_COL_SPEED			10
#define TRK_COL_ACC				11						// 가속도 크기
#define TRK_COL_POS_STD			12						// σ위치: mat_EstP 위치 대각 세 축의 RSS
#define TRK_COL_NIS				13
#define TRK_COL_GATE_RUN		14						// 게이트 밖 연속 횟수
#define TRK_COL_DIVERGE			15						// 발산 판정 횟수
#define TRK_COL_ERROR			16						// 참값 대비 위치 오차
#define TRK_COL_NUM				17

#define TRK_LEAD_TIME			1.0						// [s] 그림의 속도 선 길이 (이 시간 동안 가는 거리)
#define TRK_STEADY_UPDATES		20						// 오차 통계는 갱신 20 번 (2 s) 뒤부터 (2-Point 개시 직후 큰 오차 제외)

// 트랙 하나 (그림, 표용). 위치는 받을 때 위경도 · 그림 좌표로 바꾸고 참값 대비 오차를 구해 둠
typedef struct
{
	ST_TrkView				st_View;
	ST_PlotPoint			st_Point;						// 트랙 위치 (그림 좌표)
	ST_PlotPoint			st_Lead;						// 속도로 TRK_LEAD_TIME 만큼 간 위치
	FLOAT64					latDeg;							// 트랙 위치 [deg, m]
	FLOAT64					lonDeg;
	FLOAT64					alt;
	FLOAT64					err[3];							// 참값 대비 위치 오차 (VC-NED: 북, 동, 아래) [m]
	FLOAT64					errNorm;						// |err| [m]
} ST_TrackItem;

// 데이터 처리 쓰레드가 받은 플롯 하나
typedef struct
{
	UINT32					PlotId;
	INT32					nSource;						// 0 = 클러터, k = 표적 k (송신 쪽 기록, 화면 확인용)
	ST_PlotPoint			st_Point;
} ST_RecvPlot;

// 스캔 nScanNum 까지의 누적 (요약용)
typedef struct
{
	INT32					nCreatedNum;					// 생성 트랙
	INT32					nDeletedNum;					// 삭제 상태가 된 트랙
	INT32					nDivergedNum;					// 발산 판정
	INT32					nTrackIdNum;					// 나온 트랙 ID 수
	INT32					nSteadyNum;						// 오차 통계 표본 수 (갱신 TRK_STEADY_UPDATES 번 뒤)
	FLOAT64					rmsError;						// [m]
	FLOAT64					maxError;						// [m]
	INT32					nNisNum;						// NIS 표본 수 (같은 범위의 갱신)
	FLOAT64					meanNis;
	FLOAT64					gateOutPct;						// NIS 가 게이트 밖인 비율 [%]
} ST_TrkStats;

// 추적 결과. Core 가 처리한 스캔을 타이머로 받아 위경도 · 그림 좌표, 참값 오차로 바꿔 모아 둠
class CTrackResult
{
public:
	INT32	f_Start(const CSimResult *st_NewSource, const CPlotResult *st_Measure, INT32 sendGapMs, const ST_TrkParam *st_NewParam);
	VOID	f_Stop(VOID);
	INT32	f_Poll(VOID);

	INT32					f_GetScanNum(VOID) const;
	const ST_LnkScan *		f_GetScan(INT32 nIndex) const;
	INT32					f_FindScan(INT32 nScanNum) const;
	INT32					f_FindLastUpTo(INT32 nScanNum) const;
	INT32					f_GetTrackNum(INT32 nIndex) const;
	const ST_TrackItem *	f_GetTrack(INT32 nIndex, INT32 nTrack) const;
	INT32					f_GetPlotNum(INT32 nIndex) const;
	const ST_RecvPlot *		f_GetPlot(INT32 nIndex, INT32 nPlot) const;
	INT32					f_GetColorIndex(UINT32 trackId) const;
	VOID					f_GetStats(INT32 nScanNum, ST_TrkStats *st_Stats) const;
	VOID					f_FormatCell(INT32 nIndex, INT32 nTrack, INT32 nColumn, CHAR *pt_Buf, INT32 bufSize) const;
	INT32					f_WriteCsv(const CString &st_Path) const;

	ST_LnkStatus			st_Status = {};
	ST_TrkParam				st_Param = {};										// 이번 실행의 추적 필터 파라미터
	INT32					nScanTotal = 0;										// 보낼 스캔 수

private:
	VOID					f_AddScan(const ST_LnkScan *st_Scan);
	VOID					f_ToGeo(const FLOAT64 pt_Pos[3], UINT32 scanNum, ST_PlotPoint *st_Point, STRUCT_Coord_Lla *st_Lla) const;

	const CSimResult			*st_Source = nullptr;
	INT32						nCopiedNum = 0;				// Core 에서 가져온 스캔 수 (건너뛴 스캔 포함)
	std::vector<ST_LnkScan>		st_ScanBuf;
	std::vector<INT32>			st_TrackStart;				// 받은 스캔마다 st_TrackBuf 시작 자리
	std::vector<ST_TrackItem>	st_TrackBuf;
	std::vector<INT32>			st_PlotStart;
	std::vector<ST_RecvPlot>	st_PlotBuf;
	std::vector<INT32>			st_ScanIndex;				// 스캔 번호마다 받은 순서. 못 받았으면 -1
	std::vector<UINT32>			st_TrackIdBuf;				// 나온 트랙 ID (나온 순서 = 그림 색 순서)
	std::vector<ST_TrkView>		st_ViewTemp;				// Core 에서 복사해 올 자리
	std::vector<ST_LnkPlot>		st_PlotTemp;
};
