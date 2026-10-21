//
// @file	TrackMgr.h
// @brief	트랙 개시 (2-Point Track Initiation) 와 트랙 스코어 기반 트랙 관리
//			데이터 처리 쓰레드가 스캔마다 받은 플롯으로 트랙을 만들고, 트랙 상태 (임시 / 확정 / 메모리 / 삭제) 를 관리함
// @author	hwan
// @date	2026.10.21.
//
#ifndef TRACK_MGR_H
#define TRACK_MGR_H

#include "TargetSim.h"
#include "matrixCalcLib.h"

#ifdef __cplusplus
extern "C" {
#endif

// @def TRK_STATE_TENTATIVE
// @brief 트랙 상태 1: 임시 트랙 (과제 명세 값)
#define TRK_STATE_TENTATIVE		1

// @def TRK_STATE_CONFIRMED
// @brief 트랙 상태 2: 확정 트랙
#define TRK_STATE_CONFIRMED		2

// @def TRK_STATE_MEMORY
// @brief 트랙 상태 3: 메모리 트랙
#define TRK_STATE_MEMORY		3

// @def TRK_STATE_DELETED
// @brief 트랙 상태 4: 삭제 트랙
#define TRK_STATE_DELETED		4

// @def TRK_VMAX
// @brief 2 Point 트랙 개시 속도 게이트 V_max [m/s] (과제 조건 800 m/s)
#define TRK_VMAX				800.0

// @def TRK_SCORE_CONFIRM
// @brief 임시 트랙 확정 문턱치 (과제 조건 5). TrackScore >= 5 이면 확정
#define TRK_SCORE_CONFIRM		5

// @def TRK_SCORE_DELETE
// @brief 임시 트랙 삭제 문턱치 (과제 조건 0). TrackScore <= 0 이면 삭제
#define TRK_SCORE_DELETE		0

// @def TRK_MEMORY_DELETE
// @brief 메모리 트랙 삭제 문턱치 (과제 조건 5). MemoryScore >= 5 이면 삭제
#define TRK_MEMORY_DELETE		5

// @def TRK_SCORE_INIT
// @brief 개시 직후 TrackScore. 과제에 없어 정한 값: 두 플롯 (연관 성공 두 번에 해당) 으로 만든 트랙이라 2
#define TRK_SCORE_INIT			2

// @def TRK_ID_MIN
// @brief 트랙 ID 최소값 (과제 조건 100 ~ 10000)
#define TRK_ID_MIN				100

// @def TRK_ID_MAX
// @brief 트랙 ID 최대값
#define TRK_ID_MAX				10000

// @def TRK_R_SIGMA_RANGE
// @brief 트랙 초기화 측정잡음 공분산 R 의 거리 표준편차 [m] (과제 조건 10 m)
#define TRK_R_SIGMA_RANGE		10.0

// @def TRK_R_SIGMA_AZIMUTH
// @brief R 의 방위각 표준편차 [deg] (과제 조건 0.2 도)
#define TRK_R_SIGMA_AZIMUTH		0.2

// @def TRK_R_SIGMA_ELEVATION
// @brief R 의 고각 표준편차 [deg] (과제 조건 0.2 도)
#define TRK_R_SIGMA_ELEVATION	0.2

// @def TRK_STATE_DIM
// @brief 상태 변수 차원. X = [x x' x'' y y' y'' z z' z'']^T
#define TRK_STATE_DIM			9

// @def TRK_MAX_TRACK
// @brief 트랙 표 최대 트랙 수 (구현상 크기 제한)
#define TRK_MAX_TRACK			256

// @def TRK_MAX_PLOT
// @brief 스캔 하나에 받는 플롯 최대 개수 (구현상 크기 제한)
#define TRK_MAX_PLOT			64

//
// @struct ST_TrkPlot
// @brief	데이터 처리에 들어가는 플롯 하나. 수신 쓰레드가 플롯 메시지의 안테나 구좌표를 VC-NED 로 바꿔 채움
//
typedef struct
{
	UINT32					PlotId;							// @var 플롯 ID (10000 ~ 1000000)
	FLOAT64					time;							// @var 스캔 시각 [s]
	FLOAT64					pos[3];							// @var VC-NED 위치 (북, 동, 아래) [m]
	FLOAT64					cov[3][3];						// @var 위치 측정잡음 공분산 R (VC-NED) [m^2]
	INT32					isUsed;							// @var 1 이면 트랙 개시에 씀 (다음 스캔 개시 후보에서 뺌)
} ST_TrkPlot;

//
// @struct ST_Track
// @brief	트랙 하나. 변수 이름은 과제 명세 그대로 (TrackScore, MemoryScore, TrackState, Assigned_Flag, mat_EstX, mat_EstP)
//
typedef struct
{
	UINT32					TrackId;						// @var 트랙 ID (100 ~ 10000)
	INT32					TrackState;						// @var 1 임시, 2 확정, 3 메모리, 4 삭제
	INT32					TrackScore;						// @var 트랙 점수
	INT32					MemoryScore;					// @var 메모리 점수
	INT32					Assigned_Flag;					// @var 트랙 연관 결과. 1 성공, 0 실패
	matrix					mat_EstX;						// @var 9 x 1 상태변수 (VC-NED)
	matrix					mat_EstP;						// @var 9 x 9 오차 공분산
	FLOAT64					initTime;						// @var 개시 시각 [s]
	INT32					nInitScan;						// @var 개시 스캔 번호
	UINT32					PlotId[2];						// @var 개시에 쓴 플롯 ID (앞 스캔, 이번 스캔)
} ST_Track;

//
// @struct ST_TrkTable
// @brief	트랙 표. 스캔마다 f_Trk_ProcessScan 으로 갱신
//
typedef struct
{
	INT32					nTrackNum;						// @var 표에 있는 트랙 수
	ST_Track				st_Track[TRK_MAX_TRACK];		// @var 트랙 목록
	UINT32					nextTrackId;					// @var 다음에 줄 트랙 ID 후보
	INT32					isPrevValid;					// @var 앞 스캔 플롯이 있는지
	FLOAT64					prevTime;						// @var 앞 스캔 시각 [s]
	INT32					nPrevPlotNum;					// @var 앞 스캔 플롯 수
	ST_TrkPlot				st_PrevPlot[TRK_MAX_PLOT];		// @var 앞 스캔 플롯 (개시 후보)
	INT32					nCreatedSum;					// @var 지금까지 만든 트랙 수
	INT32					nDeletedSum;					// @var 지금까지 지운 트랙 수
	INT32					nTableFullSum;					// @var 표가 차서 못 만든 수
} ST_TrkTable;

//
// @struct ST_TrkView
// @brief	화면, 기록용 트랙 요약. 스캔 처리가 끝난 뒤의 값
//
typedef struct
{
	UINT32					TrackId;						// @var 트랙 ID
	INT32					TrackState;						// @var 트랙 상태
	INT32					TrackScore;						// @var 트랙 점수
	INT32					MemoryScore;					// @var 메모리 점수
	INT32					Assigned_Flag;					// @var 트랙 연관 결과
	INT32					nInitScan;						// @var 개시 스캔 번호
	UINT32					PlotId[2];						// @var 개시 플롯 ID
	FLOAT64					pos[3];							// @var mat_EstX 위치 (VC-NED) [m]
	FLOAT64					vel[3];							// @var mat_EstX 속도 [m/s]
	FLOAT64					posStd[3];						// @var mat_EstP 위치 대각의 제곱근 [m]
	FLOAT64					velStd[3];						// @var mat_EstP 속도 대각의 제곱근 [m/s]
} ST_TrkView;

//
// @struct ST_TrkScanLog
// @brief	스캔 하나의 처리 기록 (화면, 검증용)
//
typedef struct
{
	INT32					nPlotNum;						// @var 받은 플롯 수
	INT32					nCandidateNum;					// @var 앞 스캔에서 개시 후보로 남은 플롯 수
	INT32					nPairNum;						// @var 검사한 플롯 쌍 수
	INT32					nGatePassNum;					// @var 속도 게이트를 넘은 쌍 수
	INT32					nCreatedNum;					// @var 이번 스캔에 만든 트랙 수
	INT32					nConfirmedNum;					// @var 이번 스캔에 확정된 트랙 수
	INT32					nMemoryNum;						// @var 이번 스캔에 메모리로 바뀐 트랙 수
	INT32					nDeletedNum;					// @var 이번 스캔에 삭제 상태가 된 트랙 수
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
