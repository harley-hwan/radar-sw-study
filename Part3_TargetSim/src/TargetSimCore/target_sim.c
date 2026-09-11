//
// @file	target_sim.c
// @brief	표적 궤적 모의 구현.
//			한 스텝 = 기동 판단 → 자세 갱신 → 속도 변환(동체→NED→ECEF) → 위치 적분(ECEF) → LLA 변환
// @author	hwan
// @date	2026.09.12.
//
#include <string.h>
#include "target_sim.h"


//
// @brief	동체 속력 [V, 0, 0] 을 ECEF 속도로. 자세각으로 한 번, 표적 자신의 위경도로 한 번 회전한다.
//			속도는 벡터라 평행이동이 없다 → f_Trans_Ned_To_Ecef 의 플랫폼 위치 인자에 0.
// @author	hwan
//
static STRUCT_Coord_Rect f_BodyVelToEcef(const ST_Target *pstTgt)
{
    STRUCT_Coord_Rect stNed;

    stNed = f_Trans_Body_To_Ned(pstTgt->dVheading, 0.0, 0.0,
                                pstTgt->stAtt.Roll, pstTgt->stAtt.Yaw, pstTgt->stAtt.Pitch);
    return f_Trans_Ned_To_Ecef(stNed.x, stNed.y, stNed.z, 0.0, 0.0, 0.0,
                               pstTgt->stLla.Lat, pstTgt->stLla.Lon);
}


//
// @brief	입력 칸을 채운다 (각도는 도로 받아 라디안으로)
// @author	hwan
//
VOID f_SetTarget(ST_Target *pstTgt, DOUBLE64 dLatDeg, DOUBLE64 dLonDeg, DOUBLE64 dAlt,
                 DOUBLE64 dVheading, DOUBLE64 dRollDeg, DOUBLE64 dYawDeg, DOUBLE64 dPitchDeg)
{
    memset(pstTgt, 0, sizeof(*pstTgt));
    pstTgt->stInitLla.Lat   = DEG2RAD(dLatDeg);
    pstTgt->stInitLla.Lon   = DEG2RAD(dLonDeg);
    pstTgt->stInitLla.Alt   = dAlt;
    pstTgt->dVheading       = dVheading;
    pstTgt->stInitAtt.Roll  = DEG2RAD(dRollDeg);
    pstTgt->stInitAtt.Yaw   = DEG2RAD(dYawDeg);
    pstTgt->stInitAtt.Pitch = DEG2RAD(dPitchDeg);
}


//
// @brief	기동을 하나 붙인다 (MAX_MANEUVER 넘으면 무시)
// @author	hwan
//
VOID f_AddManeuver(ST_Target *pstTgt, DOUBLE64 dGravity, INT32 nTurnType, DOUBLE64 dStartTime, DOUBLE64 dEndTime)
{
    ST_Maneuver *pstMan;

    if (pstTgt->nManeuverCnt >= MAX_MANEUVER)
    {
        return;
    }
    pstMan = &pstTgt->astManeuver[pstTgt->nManeuverCnt++];
    pstMan->dGravity   = dGravity;
    pstMan->nTurnType  = nTurnType;
    pstMan->dStartTime = dStartTime;
    pstMan->dEndTime   = dEndTime;
}


//
// @brief	과제 3 조건. bWithManeuver 면 발표용 기동 예시를 붙인다
// @author	hwan
//
VOID f_SetAssignment(ST_Scenario *pstScn, INT32 bWithManeuver)
{
    memset(pstScn, 0, sizeof(*pstScn));
    pstScn->dSimTime = 60.0;
    pstScn->dDt      = 0.1;
    pstScn->nTargetCnt = 2;

    f_SetTarget(&pstScn->stPlatform,   32.0,   126.0,    0.0,   0.0, 0.0,   0.0, 0.0);
    f_SetTarget(&pstScn->astTarget[0], 32.125, 126.03,   0.0,  30.0, 0.0, 270.0, 0.0);   // 대함 : 서쪽으로
    f_SetTarget(&pstScn->astTarget[1], 32.12,  126.0,  300.0, 200.0, 0.0, 180.0, 0.0);   // 대공 : 남쪽으로

    if (bWithManeuver)
    {
        f_AddManeuver(&pstScn->astTarget[0], -0.1, TURN_YAW,   20.0, 50.0);              // 대함 좌선회
        f_AddManeuver(&pstScn->astTarget[1],  3.0, TURN_YAW,   10.0, 20.0);              // 대공 3 g 우선회
        f_AddManeuver(&pstScn->astTarget[1],  2.0, TURN_PITCH, 35.0, 40.0);              // 대공 기수 들기
    }
}


// 상태 칸 초기화. f_Trans_Lla_To_Ecef 는 인자 순서가 (경도, 위도, 고도) 다
static VOID f_InitTarget(ST_Target *pstTgt)
{
    pstTgt->stPos = f_Trans_Lla_To_Ecef(pstTgt->stInitLla.Lon, pstTgt->stInitLla.Lat, pstTgt->stInitLla.Alt);
    pstTgt->stAtt = pstTgt->stInitAtt;
    pstTgt->stLla = pstTgt->stInitLla;
    pstTgt->stVel = f_BodyVelToEcef(pstTgt);
}


// 표적 한 스텝
static VOID f_StepTarget(ST_Target *pstTgt, DOUBLE64 dTime, DOUBLE64 dDt)
{
    INT32 i;

    // 1) 기동 판단, 2) 자세 갱신 : 각속도 ω = n g / V
    for (i = 0; i < pstTgt->nManeuverCnt; i++)
    {
        const ST_Maneuver *pstMan = &pstTgt->astManeuver[i];

        if ((dTime + 1e-6 >= pstMan->dStartTime) && (dTime + 1e-6 < pstMan->dEndTime) && (pstTgt->dVheading > 0.0))
        {
            DOUBLE64 dAng = pstMan->dGravity * G_FORCE / pstTgt->dVheading * dDt;

            if (pstMan->nTurnType == TURN_ROLL)       pstTgt->stAtt.Roll  += dAng;
            else if (pstMan->nTurnType == TURN_YAW)   pstTgt->stAtt.Yaw   += dAng;
            else if (pstMan->nTurnType == TURN_PITCH) pstTgt->stAtt.Pitch += dAng;
            break;
        }
    }

    // 3) 속도 변환 (표적 자신의 현재 위경도 기준 NED)
    pstTgt->stVel = f_BodyVelToEcef(pstTgt);

    // 4) 위치 적분 (ECEF)
    pstTgt->stPos.x += pstTgt->stVel.x * dDt;
    pstTgt->stPos.y += pstTgt->stVel.y * dDt;
    pstTgt->stPos.z += pstTgt->stVel.z * dDt;

    // 5) LLA 변환. 출력용이자 다음 스텝의 NED 기준 (이 덕에 수평 비행이 지구 곡률을 따라간다)
    pstTgt->stLla = f_Trans_Ecef_To_Lla(pstTgt->stPos.x, pstTgt->stPos.y, pstTgt->stPos.z);
}


//
// @brief	시뮬레이션 시작. 입력 칸으로 상태 칸을 채운다
// @author	hwan
//
VOID f_StartScenario(ST_Scenario *pstScn)
{
    INT32 i;

    f_InitTarget(&pstScn->stPlatform);
    for (i = 0; i < pstScn->nTargetCnt; i++)
    {
        f_InitTarget(&pstScn->astTarget[i]);
    }
}


//
// @brief	시각 dTime 에서 dDt 만큼 전체를 한 스텝 진행
// @author	hwan
//
VOID f_StepScenario(ST_Scenario *pstScn, DOUBLE64 dTime)
{
    INT32 i;

    f_StepTarget(&pstScn->stPlatform, dTime, pstScn->dDt);
    for (i = 0; i < pstScn->nTargetCnt; i++)
    {
        f_StepTarget(&pstScn->astTarget[i], dTime, pstScn->dDt);
    }
}
