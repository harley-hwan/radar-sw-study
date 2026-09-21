#ifndef TARGET_SIM_JH_H
#define TARGET_SIM_JH_H

#include "Define_JH.h"
#include "TargetSimCoreApi.h"
#include "CoordinateTransform_JH.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TGT_MAX_TARGET_NUM			10
#define TGT_MAX_MANEUVER_NUM		30
#define TGT_MAX_STEP_NUM			100000
#define TGT_MIN_STEP_TIME			1.0e-3								// [s]

typedef enum
{
	TGT_OK = 0,
	TGT_ERR_NULL,
	TGT_ERR_TARGET_NUM,
	TGT_ERR_MANEUVER_NUM,
	TGT_ERR_TIME,
	TGT_ERR_SPEED,
	TGT_ERR_TURN_TYPE,
	TGT_ERR_MANEUVER_OVERLAP,
	TGT_ERR_COORD,
	TGT_ERR_SIM_STATE,
	TGT_ERR_SIM_END
} EN_TgtStatus;

// 기동 회전축. 값은 과제 명세를 그대로 따른다.
typedef enum
{
	TGT_TURN_NONE	= 0,
	TGT_TURN_ROLL	= 1,
	TGT_TURN_YAW	= 2,
	TGT_TURN_PITCH	= 3
} EN_TurnType;

// 기동 1건. 명세가 규정한 네 가지 정보만 담는다.
// gravityValue 의 부호가 회전 방향이며 동체축 오른손 법칙을 따른다.
//   Roll  (+) 우측 뱅크 / (-) 좌측 뱅크
//   Yaw   (+) 우선회    / (-) 좌선회
//   Pitch (+) 기수 상승 / (-) 기수 하강
typedef struct
{
	EN_TurnType			enTurnType;
	FLOAT64				gravityValue;						// [G] 중력가속도 배수
	FLOAT64				startTime;							// [s]
	FLOAT64				endTime;							// [s]
} ST_TargetManeuver;

// 표적 초기 설정.
// 위치는 위도/경도/고도, 속도는 동체 x 축(heading) 방향 크기 V_heading 으로 받는다.
typedef struct
{
	ST_CoordLla			st_InitLla;							// lat, lon [rad] / alt [m]
	ST_CoordAtt			st_InitAtt;							// roll, pitch, yaw [rad]
	FLOAT64				headingSpeed;						// V_heading [m/s]
	INT32				nManeuverNum;						// 0 ~ TGT_MAX_MANEUVER_NUM
	ST_TargetManeuver	st_Maneuver[TGT_MAX_MANEUVER_NUM];
} ST_TargetInit;

// 플랫폼 초기 설정. 기동은 갖지 않는다.
typedef struct
{
	ST_CoordLla			st_InitLla;
	ST_CoordAtt			st_InitAtt;
	FLOAT64				headingSpeed;						// [m/s]
} ST_PlatformInit;

// 시뮬레이션 전체 설정
typedef struct
{
	FLOAT64				durationTime;						// [s]
	FLOAT64				stepTime;							// [s]
	ST_PlatformInit		st_Platform;
	INT32				nTargetNum;							// 1 ~ TGT_MAX_TARGET_NUM
	ST_TargetInit		st_Target[TGT_MAX_TARGET_NUM];
} ST_SimConfig;

// 한 시각의 상태. 위치와 속도는 ECEF 에서 갱신하고, LLA 는 출력과 다음 스텝의 NED 기준으로 함께 보관한다.
typedef struct
{
	FLOAT64				simTime;							// [s]
	ST_CoordRect		st_PosEcef;							// [m]
	ST_CoordRect		st_VelEcef;							// [m/s]
	ST_CoordLla			st_Lla;								// lat, lon [rad] / alt [m]
	ST_CoordAtt			st_Att;								// roll, pitch, yaw [rad]
} ST_TargetState;

// 한 시각의 플랫폼·표적 상태. 호출자는 스텝마다 이 구조체를 복사해 보관한다.
typedef struct
{
	FLOAT64				simTime;							// [s] nStepIndex * stepTime
	INT32				nStepIndex;							// 0 ~ nStepNum
	INT32				nTargetNum;
	ST_TargetState		st_Platform;
	ST_TargetState		st_Target[TGT_MAX_TARGET_NUM];		// 설정과 같은 인덱스, nTargetNum 이후 칸은 0
} ST_SimSample;

// 시나리오 진행 상태. 설정 사본을 함께 두어 초기화 뒤 원본 설정이 바뀌어도 진행에 영향이 없다.
typedef struct
{
	INT32				nStepNum;							// 표본 수는 nStepNum + 1
	ST_SimSample		st_Sample;							// 가장 최근 표본
	ST_SimConfig		st_Config;
} ST_SimState;

TSCORE_API EN_TgtStatus		f_Tgt_ValidateConfig(const ST_SimConfig *st_Config);
TSCORE_API EN_TgtStatus		f_Tgt_InitState(ST_TargetState *st_State, const ST_CoordLla *st_InitLla, const ST_CoordAtt *st_InitAtt, FLOAT64 headingSpeed);
TSCORE_API EN_TgtStatus		f_Tgt_StepTarget(ST_TargetState *st_State, const ST_TargetInit *st_Target, FLOAT64 stepTime);
TSCORE_API EN_TgtStatus		f_Tgt_InitSim(ST_SimState *st_Sim, const ST_SimConfig *st_Config);
TSCORE_API EN_TgtStatus		f_Tgt_StepSim(ST_SimState *st_Sim);
TSCORE_API const CHAR *		f_Tgt_StatusStr(EN_TgtStatus status);

#ifdef __cplusplus
}
#endif

#endif
