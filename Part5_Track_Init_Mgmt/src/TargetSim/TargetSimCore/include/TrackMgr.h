#ifndef TRACK_MGR_H
#define TRACK_MGR_H

#include "TargetSim.h"
#include "matrixCalcLib.h"

#ifdef __cplusplus
extern "C" {
#endif

// 트랙 상태 (과제 명세 값)
#define TRK_STATE_TENTATIVE		1						// 임시 트랙
#define TRK_STATE_CONFIRMED		2						// 확정 트랙
#define TRK_STATE_MEMORY		3						// 메모리 트랙
#define TRK_STATE_DELETED		4						// 삭제 트랙

// 트랙 개시 · 관리 조건 (과제 명세 값)
#define TRK_VMAX				800.0					// [m/s] 2 Point 개시 속도 게이트
#define TRK_SCORE_CONFIRM		5						// 임시 트랙 확정 문턱치
#define TRK_SCORE_DELETE		0						// 임시 트랙 삭제 문턱치
#define TRK_MEMORY_DELETE		5						// 메모리 트랙 삭제 문턱치
#define TRK_SCORE_INIT			2						// 개시 직후 스코어 (두 플롯으로 만든 트랙). 과제에 없어 정한 값
#define TRK_ID_MIN				100
#define TRK_ID_MAX				10000

#define TRK_STATE_DIM			9						// [x x' x'' y y' y'' z z' z'']
#define TRK_MAX_TRACK			256
#define TRK_MAX_PLOT			64						// 스캔 하나에 받는 플롯 최대 개수

// 데이터 처리에 들어가는 플롯 하나. 수신 쓰레드가 플롯 메시지의 안테나 구좌표를 VC-NED 로 바꿔 채움.
typedef struct
{
	UINT32					PlotId;
	FLOAT64					time;							// [s]
	FLOAT64					pos[3];							// VC-NED 위치 (북, 동, 아래) [m]
	FLOAT64					cov[3][3];						// 위치 측정 공분산 R [m^2]
	INT32					isUsed;							// 1 이면 트랙 개시에 씀 (다음 스캔 개시 후보에서 뺌)
} ST_TrkPlot;

// 트랙 하나. 이름은 과제 명세 그대로.
typedef struct
{
	UINT32					TrackId;
	INT32					TrackState;						// 1 임시, 2 확정, 3 메모리, 4 삭제
	INT32					TrackScore;
	INT32					MemoryScore;
	INT32					Assigned_Flag;					// 트랙 연관 결과. 1 성공, 0 실패
	matrix					mat_EstX;						// 9 x 1 상태변수 (VC-NED)
	matrix					mat_EstP;						// 9 x 9 오차 공분산
	FLOAT64					initTime;						// 개시 시각 [s]
	INT32					nInitScan;						// 개시 스캔 번호
	UINT32					PlotId[2];						// 개시에 쓴 플롯 (앞 스캔, 이번 스캔)
} ST_Track;

// 트랙 표. 스캔마다 f_Trk_ProcessScan 으로 갱신.
typedef struct
{
	INT32					nTrackNum;
	ST_Track				st_Track[TRK_MAX_TRACK];
	UINT32					nextTrackId;					// 다음에 줄 트랙 ID 후보
	INT32					isPrevValid;					// 앞 스캔 플롯이 있는지
	FLOAT64					prevTime;						// 앞 스캔 시각 [s]
	INT32					nPrevPlotNum;
	ST_TrkPlot				st_PrevPlot[TRK_MAX_PLOT];		// 앞 스캔 플롯 (개시 후보)
	INT32					nCreatedSum;					// 지금까지 만든 트랙 수
	INT32					nDeletedSum;					// 지금까지 지운 트랙 수
	INT32					nTableFullSum;					// 표가 차서 못 만든 수
} ST_TrkTable;

// 화면, 기록용 트랙 요약. 스캔이 끝난 뒤의 값.
typedef struct
{
	UINT32					TrackId;
	INT32					TrackState;
	INT32					TrackScore;
	INT32					MemoryScore;
	INT32					Assigned_Flag;
	INT32					nInitScan;
	UINT32					PlotId[2];
	FLOAT64					pos[3];							// mat_EstX 위치 (VC-NED) [m]
	FLOAT64					vel[3];							// mat_EstX 속도 [m/s]
	FLOAT64					posStd[3];						// mat_EstP 위치 대각의 제곱근 [m]
	FLOAT64					velStd[3];						// mat_EstP 속도 대각의 제곱근 [m/s]
} ST_TrkView;

// 스캔 하나의 처리 기록 (화면, 검증용)
typedef struct
{
	INT32					nPlotNum;						// 받은 플롯 수
	INT32					nCandidateNum;					// 앞 스캔에서 개시 후보로 남은 플롯 수
	INT32					nPairNum;						// 검사한 플롯 쌍 수
	INT32					nGatePassNum;					// 속도 게이트를 넘은 쌍 수
	INT32					nCreatedNum;					// 이번 스캔에 만든 트랙 수
	INT32					nConfirmedNum;					// 이번 스캔에 확정된 트랙 수
	INT32					nMemoryNum;						// 이번 스캔에 메모리로 바뀐 트랙 수
	INT32					nDeletedNum;					// 이번 스캔에 삭제 상태가 된 트랙 수
} ST_TrkScanLog;

TSCORE_API VOID		f_Trk_Init(ST_TrkTable *st_Table);
TSCORE_API VOID		f_Trk_MoveOrigin(ST_TrkTable *st_Table, const FLOAT64 shift[3]);
TSCORE_API VOID		f_Trk_ProcessScan(ST_TrkTable *st_Table, const ST_TrkPlot *st_Plot, INT32 nPlotNum, FLOAT64 time, INT32 nScan, ST_TrkScanLog *st_Log);
TSCORE_API VOID		f_Trk_UpdateState(ST_Track *st_Track);
TSCORE_API VOID		f_Trk_TwoPointInit(ST_Track *st_Track, const ST_TrkPlot *st_Prev, const ST_TrkPlot *st_Cur);
TSCORE_API VOID		f_Trk_GetView(ST_TrkView *st_View, const ST_Track *st_Track);
TSCORE_API VOID		f_Trk_MeasCov(FLOAT64 cov[3][3], const STRUCT_Coord_Sph *st_Meas, const STRUCT_Coord_Sph *st_Sigma, const STRUCT_Coord_Attitude *st_Att);

#ifdef __cplusplus
}
#endif

#endif
