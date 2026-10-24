// 2-Point 트랙 개시, 트랙 스코어 관리 (과제 5) + Singer 모델 CMKF 추적 필터 (과제 6)
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

// 과제 5 조건 (개시 스코어만 정한 값)
#define TRK_VMAX				800.0					// 속도 게이트 [m/s]
#define TRK_SCORE_CONFIRM		5						// TrackScore >= 5 이면 확정
#define TRK_SCORE_DELETE		0						// TrackScore <= 0 이면 삭제
#define TRK_MEMORY_DELETE		5						// MemoryScore >= 5 이면 삭제
#define TRK_SCORE_INIT			2						// 개시 점수 (두 플롯 = 성공 2 번)
#define TRK_ID_MIN				100						// 트랙 ID 범위
#define TRK_ID_MAX				10000

// 과제 6 추적 필터 기본값 (화면에서 바꿀 수 있음)
#define TRK_R_SIGMA_RANGE		10.0					// R 표준편차: 거리 [m]
#define TRK_R_SIGMA_AZIMUTH		0.3						// 방위각 [deg]
#define TRK_R_SIGMA_ELEVATION	0.3						// 고각 [deg]
#define TRK_SINGER_Q			0.5						// Singer 공정잡음 표준편차 q [m/s^2]
#define TRK_SINGER_TAU			10.0					// Singer 시상수 τ [s]

// 발산 판정 (정한 값)
#define TRK_GATE_CHI2			11.34					// χ²(자유도 3) 99 % 값. NIS 가 이보다 크면 게이트 밖
#define TRK_DIVERGE_NUM			5						// 게이트 밖이 이만큼 이어지면 발산 (메모리 삭제 수와 같게)

#define TRK_SERIES_X			0.05					// T/τ 가 이보다 작으면 Q 를 급수로 계산 (닫힌 식은 자리 손실)
#define TRK_STATE_DIM			9						// X = [x x' x'' y y' y'' z z' z'']
#define TRK_MEAS_DIM			3						// z = [x y z]
#define TRK_MAX_TRACK			256						// 트랙 표 크기
#define TRK_MAX_PLOT			64						// 스캔당 최대 플롯 수

// 추적 필터 파라미터 (조정 대상)
typedef struct
{
	FLOAT64					sigmaRange;						// R 표준편차: 거리 [m]
	FLOAT64					sigmaAzimuth;					// 방위각 [deg]
	FLOAT64					sigmaElevation;					// 고각 [deg]
	FLOAT64					singerQ;						// Singer q: 기동 가속도 표준편차 [m/s^2]
	FLOAT64					singerTau;						// Singer τ: 기동 시상수 [s]
} ST_TrkParam;

// 트랙 처리에 넣는 플롯 (VC-NED)
typedef struct
{
	UINT32					PlotId;
	FLOAT64					time;							// 스캔 시각 [s]
	FLOAT64					pos[3];							// 북, 동, 아래 [m]
	FLOAT64					cov[3][3];						// 측정잡음 공분산 R [m^2]
	INT32					isUsed;							// 1 = 트랙 갱신이나 개시에 씀 (다른 트랙, 개시 후보에서 뺌)
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

	FLOAT64					stateTime;						// mat_EstX 의 시각 [s]
	UINT32					PlotIdLast;						// 마지막으로 갱신에 쓴 플롯
	FLOAT64					innov[3];						// 마지막 갱신의 잔차 z - H X [m]
	FLOAT64					innovCov[3][3];					// 그때의 잔차 공분산 S = H P H^T + R [m^2]
	FLOAT64					nis;							// 그때의 NIS = 잔차^T S^-1 잔차. 이번 스캔에 갱신 안 했으면 -1
	INT32					nUpdateNum;						// 갱신 횟수
	INT32					nGateOutRun;					// 게이트 밖 연속 횟수
	INT32					nGateOutSum;					// 게이트 밖 누적
	INT32					nDivergeNum;					// 발산 판정 횟수 (게이트 밖이 TRK_DIVERGE_NUM 번 이어질 때마다 1)
} ST_Track;

// 트랙 표
typedef struct
{
	ST_TrkParam				st_Param;						// 추적 필터 파라미터 (q, τ 는 예측, R 표준편차는 송신 쪽이 씀)
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
	INT32					nDivergedSum;					// 누적: 발산 판정
	FLOAT64					distance[TRK_MAX_TRACK][TRK_MAX_PLOT];	// 연관 가정에 쓰는 트랙-플롯 통계적 거리 (스캔마다 다시 계산)
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
	UINT32					PlotIdLast;
	FLOAT64					pos[3];							// 위치 (VC-NED) [m]
	FLOAT64					vel[3];							// 속도 [m/s]
	FLOAT64					acc[3];							// 가속도 [m/s^2]
	FLOAT64					posStd[3];						// sqrt(P) 위치 대각 [m]
	FLOAT64					velStd[3];						// sqrt(P) 속도 대각 [m/s]
	FLOAT64					accStd[3];						// sqrt(P) 가속도 대각 [m/s^2]
	FLOAT64					posCov[3][3];					// P 위치 블록 [m^2] (참값 오차의 NEES 계산용)
	FLOAT64					innov[3];						// 마지막 갱신 잔차 [m]
	FLOAT64					nis;							// 이번 스캔 NIS. 갱신 안 했으면 -1
	INT32					nUpdateNum;
	INT32					nGateOutRun;
	INT32					nGateOutSum;
	INT32					nDivergeNum;
} ST_TrkView;

// 스캔 하나의 처리 기록 (화면 · 검증용)
typedef struct
{
	INT32					nPlotNum;						// 받은 플롯
	INT32					nUpdatedNum;					// 연관 (가정) 해 갱신한 트랙
	INT32					nGateOutNum;					// 갱신한 트랙 가운데 게이트 밖
	INT32					nDivergedNum;					// 이번 스캔에 발산 판정이 난 트랙
	INT32					nCandidateNum;					// 앞 스캔에서 남은 개시 후보
	INT32					nPairNum;						// 검사한 쌍
	INT32					nGatePassNum;					// 속도 게이트 통과 쌍
	INT32					nCreatedNum;					// 이번 스캔: 생성
	INT32					nConfirmedNum;					// 확정
	INT32					nMemoryNum;						// 메모리 전환
	INT32					nDeletedNum;					// 삭제 상태
} ST_TrkScanLog;

TSCORE_API VOID		f_Trk_DefaultParam(ST_TrkParam *st_Param);
TSCORE_API VOID		f_Trk_Init(ST_TrkTable *st_Table, const ST_TrkParam *st_Param);
TSCORE_API VOID		f_Trk_MoveOrigin(ST_TrkTable *st_Table, const FLOAT64 shift[3]);
TSCORE_API VOID		f_Trk_ProcessScan(ST_TrkTable *st_Table, const ST_TrkPlot *st_Plot, INT32 nPlotNum, FLOAT64 time, INT32 nScan, ST_TrkScanLog *st_Log);
TSCORE_API VOID		f_Trk_UpdateState(ST_Track *st_Track);
TSCORE_API VOID		f_Trk_TwoPointInit(ST_Track *st_Track, const ST_TrkPlot *st_Prev, const ST_TrkPlot *st_Cur);
TSCORE_API VOID		f_Trk_SingerF(matrix *st_F, FLOAT64 dt, FLOAT64 tau);
TSCORE_API VOID		f_Trk_SingerQ(matrix *st_Q, FLOAT64 dt, FLOAT64 q, FLOAT64 tau);
TSCORE_API VOID		f_Trk_Predict(ST_Track *st_Track, FLOAT64 time, const ST_TrkParam *st_Param);
TSCORE_API FLOAT64	f_Trk_Distance(const ST_Track *st_Track, const ST_TrkPlot *st_Plot);
TSCORE_API VOID		f_Trk_Update(ST_Track *st_Track, const ST_TrkPlot *st_Plot);
TSCORE_API VOID		f_Trk_GetView(ST_TrkView *st_View, const ST_Track *st_Track);
TSCORE_API VOID		f_Trk_GetMeasSigma(STRUCT_Coord_Sph *st_Sigma, const ST_TrkParam *st_Param);
TSCORE_API VOID		f_Trk_MeasCov(FLOAT64 cov[3][3], const STRUCT_Coord_Sph *st_Meas, const STRUCT_Coord_Sph *st_Sigma, const STRUCT_Coord_Attitude *st_Att);

#ifdef __cplusplus
}
#endif

#endif
