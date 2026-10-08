#ifndef TARGET_SIM_H
#define TARGET_SIM_H

#include "Define.h"
#include "CoordinateTransform.h"

// DLL 내보내기 / 가져오기. Core 빌드 시에만 TARGETSIMCORE_EXPORTS 정의.
#if defined(_WIN32)
	#ifdef TARGETSIMCORE_EXPORTS
		#define TSCORE_API	__declspec(dllexport)
	#else
		#define TSCORE_API	__declspec(dllimport)
	#endif
#else
	#define TSCORE_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define TGT_MAX_TARGET_NUM			10
#define TGT_MAX_MANEUVER_NUM		30

// 계산 과정 기록을 받는 함수 (계산 로그용). 기록 내용 ST_StepTrace 는 맨 아래에 정의.
typedef struct ST_StepTrace	ST_StepTrace;
typedef VOID (*PF_TgtTrace)(const ST_StepTrace *st_Trace, VOID *pt_User);

// 기동 회전축. 값은 과제 명세 그대로.
typedef enum
{
	TGT_TURN_NONE	= 0,
	TGT_TURN_ROLL	= 1,
	TGT_TURN_YAW	= 2,
	TGT_TURN_PITCH	= 3
} EN_TurnType;

// 기동 1건.
// gravityValue 부호가 회전 방향. + 는 우측 뱅크 / 우선회 / 기수 상승.
typedef struct
{
	EN_TurnType				enTurnType;						// 회전축 (0 없음, 1 Roll, 2 Yaw, 3 Pitch)
	FLOAT64					gravityValue;					// [G] 세기, 부호가 회전 방향
	FLOAT64					startTime;						// [s] 시작 시각
	FLOAT64					endTime;						// [s] 종료 시각
} ST_TargetManeuver;

// 표적 초기 설정. 속도는 기수 방향 크기 V_heading 만 입력.
typedef struct
{
	STRUCT_Coord_Lla		st_InitLla;						// 초기 위도, 경도 [rad], 고도 [m]
	STRUCT_Coord_Attitude	st_InitAtt;						// 초기 Roll, Pitch, Yaw [rad]
	FLOAT64					headingSpeed;					// 속력 V_heading [m/s]
	INT32					nManeuverNum;					// 기동 수 (0 ~ TGT_MAX_MANEUVER_NUM)
	ST_TargetManeuver		st_Maneuver[TGT_MAX_MANEUVER_NUM];	// 기동 목록
} ST_TargetInit;

// 플랫폼 초기 설정. 기동이 없어 자세, 속력은 처음 값 그대로.
typedef struct
{
	STRUCT_Coord_Lla		st_InitLla;						// 초기 위도, 경도 [rad], 고도 [m]
	STRUCT_Coord_Attitude	st_InitAtt;						// 초기 자세 [rad]. 움직이는 방향을 정함
	FLOAT64					headingSpeed;					// 속력 [m/s]
} ST_PlatformInit;

// 시뮬레이션 전체 설정.
typedef struct
{
	FLOAT64					durationTime;					// [s]
	FLOAT64					stepTime;						// [s]
	INT32					noMidPoint;						// 0 = 중점법, 1 = 비중점법 (비교용)
	INT32					usePlatform;					// 0 = 표적 자리 기준, 1 = 플랫폼 기준 (비교용)
	ST_PlatformInit			st_Platform;
	INT32					nTargetNum;						// 1 ~ TGT_MAX_TARGET_NUM
	ST_TargetInit			st_Target[TGT_MAX_TARGET_NUM];
	PF_TgtTrace				pf_Trace;						// 계산 과정을 받을 함수. NULL 이면 기록 안 함
	VOID					*pt_TraceUser;					// pf_Trace 에 그대로 넘기는 값
} ST_SimConfig;

// 플랫폼 또는 표적 하나의 한 시각 상태. 위치, 속도는 ECEF 이고 LLA 는 출력용.
typedef struct
{
	FLOAT64					simTime;						// 시각 [s]
	STRUCT_Coord_Rect		st_PosEcef;						// ECEF 위치 [m]
	STRUCT_Coord_Rect		st_VelEcef;						// ECEF 속도 [m/s]
	STRUCT_Coord_Lla		st_Lla;							// 위도, 경도 [rad], 고도 [m]
	STRUCT_Coord_Attitude	st_Att;							// Roll, Pitch, Yaw [rad]
} ST_TargetState;

// 한 시각의 결과. 플랫폼, 표적 상태.
typedef struct
{
	FLOAT64					simTime;						// 시각 [s] = nStepIndex * stepTime
	INT32					nStepIndex;						// 스텝 번호 (0 ~ nStepNum)
	INT32					nTargetNum;						// 표적 수
	ST_TargetState			st_Platform;					// 플랫폼 상태
	ST_TargetState			st_Target[TGT_MAX_TARGET_NUM];	// 표적 상태 (nTargetNum 개)
} ST_SimSample;

// 시뮬레이션 진행 상태. 복사해 둔 설정 포함.
typedef struct
{
	INT32					nStepNum;						// 스텝 수 (결과는 nStepNum + 1 개)
	ST_SimSample			st_Sample;						// 가장 최근 결과
	ST_SimConfig			st_Config;
} ST_SimState;

TSCORE_API VOID					f_Tgt_InitSim(ST_SimState *st_Sim, const ST_SimConfig *st_Config);
TSCORE_API VOID					f_Tgt_StepSim(ST_SimState *st_Sim);

// 화면 표시용 좌표 변환. 기준점은 플랫폼 시작 위치.
TSCORE_API STRUCT_Coord_Rect	f_Tgt_EcefToNed(const STRUCT_Coord_Rect *st_Ecef, const STRUCT_Coord_Rect *st_RefEcef, const STRUCT_Coord_Lla *st_RefLla);
TSCORE_API STRUCT_Coord_Lla		f_Tgt_NedToLla(const STRUCT_Coord_Rect *st_Ned, const STRUCT_Coord_Rect *st_RefEcef, const STRUCT_Coord_Lla *st_RefLla);
TSCORE_API STRUCT_Coord_Rect	f_Tgt_LlaToNed(const STRUCT_Coord_Lla *st_Lla, const STRUCT_Coord_Rect *st_RefEcef, const STRUCT_Coord_Lla *st_RefLla);

// 자세 각속도. Roll, Pitch, Yaw 가 1 초에 바뀌는 양.
typedef struct
{
	FLOAT64					Roll;							// [rad/s]
	FLOAT64					Pitch;							// [rad/s]
	FLOAT64					Yaw;							// [rad/s]
} ST_AttRate;

// 계산 과정 기록. pf_Trace 가 있으면 0 초 상태를 만들 때 한 번, 그 뒤 스텝마다 객체마다 한 번씩 넘어감.
// 값은 Core 가 계산에 쓴 것 그대로. 0 초 기록은 반 스텝 뒤 값 칸에도 0 초 상태가 들어감.
struct ST_StepTrace
{
	INT32					nObject;						// 0 = 플랫폼, k = 표적 k
	INT32					nStepIndex;						// -1 = 0 초 상태 만들기, 0 이상 = 진행 전 스텝 번호
	FLOAT64					stepTime;						// [s]
	FLOAT64					headingSpeed;					// 속력 [m/s]
	ST_AttRate				st_AttRate;						// 이 스텝의 기동 각속도
	ST_TargetState			st_Start;						// 스텝 시작 상태
	STRUCT_Coord_Rect		st_VelNedStart;					// 출발 속도의 북, 동, 아래 성분 [m/s]
	STRUCT_Coord_Rect		st_VelStart;					// 출발 속도 ECEF [m/s]
	STRUCT_Coord_Attitude	st_AttMid;						// 반 스텝 뒤 자세 [rad]
	STRUCT_Coord_Rect		st_PosMid;						// 반 스텝 뒤 위치 ECEF [m]
	STRUCT_Coord_Lla		st_LlaMid;						// 반 스텝 뒤 위경도
	STRUCT_Coord_Rect		st_VelNedMid;					// 중간 지점 속도의 북, 동, 아래 성분 [m/s]
	STRUCT_Coord_Rect		st_VelMid;						// 중간 지점 속도 ECEF [m/s]
	ST_TargetState			st_Next;						// 스텝 끝 상태
};

#ifdef __cplusplus
}
#endif

#endif
