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

// 한 시각의 상태. 위치와 속도는 ECEF 에서 갱신하고 LLA 는 출력용으로 함께 보관한다.
typedef struct
{
	FLOAT64				simTime;							// [s]
	ST_CoordRect		st_PosEcef;							// [m]
	ST_CoordRect		st_VelEcef;							// [m/s]
	ST_CoordLla			st_Lla;								// lat, lon [rad] / alt [m]
	ST_CoordAtt			st_Att;								// roll, pitch, yaw [rad]
} ST_TargetState;

#ifdef __cplusplus
}
#endif

#endif
