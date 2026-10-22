// 2-Point 트랙 개시와 트랙 스코어 기반 트랙 관리
#ifndef TRACK_MGR_H
#define TRACK_MGR_H

#include "TargetSim.h"
#include "matrixCalcLib.h"

#ifdef __cplusplus
extern "C" {
#endif

// 트랙 상태
#define TRK_STATE_TENTATIVE		1						// 임시
#define TRK_STATE_CONFIRMED		2						// 확정
#define TRK_STATE_MEMORY		3						// 메모리
#define TRK_STATE_DELETED		4						// 삭제

// 과제 조건 (개시 스코어만 정한 값)
#define TRK_VMAX				800.0					// 속도 게이트 [m/s]
#define TRK_SCORE_CONFIRM		5						// TrackScore >= 5 이면 확정
#define TRK_SCORE_DELETE		0						// TrackScore <= 0 이면 삭제
#define TRK_MEMORY_DELETE		5						// MemoryScore >= 5 이면 삭제
#define TRK_SCORE_INIT			2						// 개시 점수 (두 플롯 = 성공 2 번)
#define TRK_ID_MIN				100						// 트랙 ID 범위
#define TRK_ID_MAX				10000
#define TRK_R_SIGMA_RANGE		10.0					// R 표준편차: 거리 [m]
#define TRK_R_SIGMA_AZIMUTH		0.2						// 방위각 [deg]
#define TRK_R_SIGMA_ELEVATION	0.2						// 고각 [deg]

#define TRK_STATE_DIM			9						// X = [x x' x'' y y' y'' z z' z'']
#define TRK_MAX_TRACK			256						// 트랙 표 크기
#define TRK_MAX_PLOT			64						// 스캔당 최대 플롯 수

// 트랙 처리에 넣는 플롯 (VC-NED)
typedef struct
{
	UINT32					PlotId;
	FLOAT64					time;							// 스캔 시각 [s]
	FLOAT64					pos[3];							// 북, 동, 아래 [m]
	FLOAT64					cov[3][3];						// 측정잡음 공분산 R [m^2]
	INT32					isUsed;							// 1 = 트랙 개시에 씀 (다음 스캔 후보에서 뺌)
} ST_TrkPlot;

// 트랙 하나 (변수 이름은 과제 명세 그대로)
typedef struct
{
	UINT32					TrackId;						// 100 ~ 10000
	INT32					TrackState;						// 1 임시, 2 확정, 3 메모리, 4 삭제
	INT32					TrackScore;
	INT32					MemoryScore;
	INT32					Assigned_Flag;					// 연관 결과: 1 성공, 0 실패
	matrix					mat_EstX;						// 상태변수 9 x 1 (VC-NED)
	matrix					mat_EstP;						// 오차 공분산 9 x 9
	FLOAT64					initTime;						// 개시 시각 [s]
	INT32					nInitScan;						// 개시 스캔
	UINT32					PlotId[2];						// 개시 플롯 ID (앞 스캔, 이번 스캔)
} ST_Track;

// 트랙 표
typedef struct
{
	INT32					nTrackNum;
	ST_Track				st_Track[TRK_MAX_TRACK];
	UINT32					nextTrackId;					// 다음 트랙 ID 후보
	INT32					isPrevValid;					// 1 = 앞 스캔 플롯 있음
	FLOAT64					prevTime;						// 앞 스캔 시각 [s]
	INT32					nPrevPlotNum;
	ST_TrkPlot				st_PrevPlot[TRK_MAX_PLOT];		// 앞 스캔 플롯 (개시 후보)
	INT32					nCreatedSum;					// 누적: 만든 트랙
	INT32					nDeletedSum;					// 누적: 지운 트랙
	INT32					nTableFullSum;					// 누적: 표가 차서 못 만든 트랙
} ST_TrkTable;

// 화면 · 기록용 트랙 요약 (스캔 처리 뒤)
typedef struct
{
	UINT32					TrackId;
	INT32					TrackState;
	INT32					TrackScore;
	INT32					MemoryScore;
	INT32					Assigned_Flag;
	INT32					nInitScan;
	UINT32					PlotId[2];
	FLOAT64					pos[3];							// 위치 (VC-NED) [m]
	FLOAT64					vel[3];							// 속도 [m/s]
	FLOAT64					posStd[3];						// sqrt(P) 위치 대각 [m]
	FLOAT64					velStd[3];						// sqrt(P) 속도 대각 [m/s]
} ST_TrkView;

// 스캔 하나의 처리 기록 (화면 · 검증용)
typedef struct
{
	INT32					nPlotNum;						// 받은 플롯
	INT32					nCandidateNum;					// 앞 스캔에서 남은 개시 후보
	INT32					nPairNum;						// 검사한 쌍
	INT32					nGatePassNum;					// 게이트 통과 쌍
	INT32					nCreatedNum;					// 이번 스캔: 생성
	INT32					nConfirmedNum;					// 확정
	INT32					nMemoryNum;						// 메모리 전환
	INT32					nDeletedNum;					// 삭제 상태
} ST_TrkScanLog;

TSCORE_API VOID		f_Trk_Init(ST_TrkTable *st_Table);
TSCORE_API VOID		f_Trk_MoveOrigin(ST_TrkTable *st_Table, const FLOAT64 shift[3]);
TSCORE_API VOID		f_Trk_ProcessScan(ST_TrkTable *st_Table, const ST_TrkPlot *st_Plot, INT32 nPlotNum, FLOAT64 time, INT32 nScan, ST_TrkScanLog *st_Log);
TSCORE_API VOID		f_Trk_UpdateState(ST_Track *st_Track);
TSCORE_API VOID		f_Trk_TwoPointInit(ST_Track *st_Track, const ST_TrkPlot *st_Prev, const ST_TrkPlot *st_Cur);
TSCORE_API VOID		f_Trk_GetView(ST_TrkView *st_View, const ST_Track *st_Track);
TSCORE_API VOID		f_Trk_GetMeasSigma(STRUCT_Coord_Sph *st_Sigma);
TSCORE_API VOID		f_Trk_MeasCov(FLOAT64 cov[3][3], const STRUCT_Coord_Sph *st_Meas, const STRUCT_Coord_Sph *st_Sigma, const STRUCT_Coord_Attitude *st_Att);

#ifdef __cplusplus
}
#endif

#endif
