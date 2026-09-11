//
// @file	target_sim.h
// @brief	표적 궤적 모의. 표적 구조체와 한 스텝 갱신.
//			초기값은 위경도 · 동체 속력 · 자세각으로 받고, 상태는 ECEF 에서 갱신한다.
//			좌표변환은 제공받은 CoordinateTransform.c 를 그대로 쓴다 (각도 라디안, 거리 m).
// @author	hwan
// @date	2026.09.12.
//
#ifndef TARGET_SIM_H_
#define TARGET_SIM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "Define.h"
#include "CoordinateTransform.h"

#define MAX_TARGET          10          // 표적 수 (과제 조건)
#define MAX_MANEUVER        30          // 표적당 기동 수 (과제 조건)

#define TURN_NONE           0
#define TURN_ROLL           1
#define TURN_YAW            2
#define TURN_PITCH          3

#define DEG2RAD(d)          ((d) * PI / 180.0)
#define RAD2DEG(r)          ((r) * 180.0 / PI)

//
// @struct	ST_Maneuver
// @brief	기동 하나. StartTime <= t < EndTime 동안 dGravity [g] 로 nTurnType 축을 돌린다
//
typedef struct
{
    DOUBLE64                dGravity;       // @var 하중배수 [g]. 부호가 방향
    INT32                   nTurnType;      // @var TURN_xxx
    DOUBLE64                dStartTime;     // @var [s]
    DOUBLE64                dEndTime;       // @var [s]
} ST_Maneuver;

//
// @struct	ST_Target
// @brief	표적 하나. 위 다섯 칸이 입력, 아래 네 칸이 상태
//
typedef struct
{
    STRUCT_Coord_Lla        stInitLla;                  // @var 초기 위치 [rad, rad, m]
    DOUBLE64                dVheading;                  // @var 동체 속력 [m/s]
    STRUCT_Coord_Attitude   stInitAtt;                  // @var 초기 자세 [rad]
    INT32                   nManeuverCnt;               // @var 기동 수
    ST_Maneuver             astManeuver[MAX_MANEUVER];  // @var 기동 목록

    STRUCT_Coord_Rect       stPos;                      // @var ECEF 위치 [m]
    STRUCT_Coord_Rect       stVel;                      // @var ECEF 속도 [m/s]
    STRUCT_Coord_Attitude   stAtt;                      // @var 현재 자세 [rad]
    STRUCT_Coord_Lla        stLla;                      // @var 현재 위경도 (출력용)
} ST_Target;

//
// @struct	ST_Scenario
// @brief	플랫폼 하나 + 표적들 + 시뮬레이션 시간. 플랫폼도 속력 0 인 표적으로 다룬다
//
typedef struct
{
    ST_Target               stPlatform;
    INT32                   nTargetCnt;
    ST_Target               astTarget[MAX_TARGET];
    DOUBLE64                dSimTime;                   // @var [s]
    DOUBLE64                dDt;                        // @var [s]
} ST_Scenario;

VOID    f_SetTarget(ST_Target *pstTgt, DOUBLE64 dLatDeg, DOUBLE64 dLonDeg, DOUBLE64 dAlt,
                    DOUBLE64 dVheading, DOUBLE64 dRollDeg, DOUBLE64 dYawDeg, DOUBLE64 dPitchDeg);
VOID    f_AddManeuver(ST_Target *pstTgt, DOUBLE64 dGravity, INT32 nTurnType, DOUBLE64 dStartTime, DOUBLE64 dEndTime);
VOID    f_SetAssignment(ST_Scenario *pstScn, INT32 bWithManeuver);
VOID    f_StartScenario(ST_Scenario *pstScn);
VOID    f_StepScenario(ST_Scenario *pstScn, DOUBLE64 dTime);

#ifdef __cplusplus
}
#endif

#endif
