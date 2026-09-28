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

// 기동 회전축. 값은 과제 명세 그대로.
typedef enum
{
	TGT_TURN_NONE	= 0,
	TGT_TURN_ROLL	= 1,
	TGT_TURN_YAW	= 2,
	TGT_TURN_PITCH	= 3
} EN_TurnType;

// 기동 1건. 구간 [startTime, endTime).
// gravityValue 부호가 회전 방향. + 는 우측 뱅크 / 우선회 / 기수 상승.
typedef struct
{
	EN_TurnType				enTurnType;						// 회전축 (0 없음, 1 Roll, 2 Yaw, 3 Pitch)
	FLOAT64					gravityValue;					// [G] 세기, 부호가 회전 방향
	FLOAT64					startTime;						// [s] 시작, 이 시각 포함
	FLOAT64					endTime;						// [s] 종료, 이 시각 직전까지
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

// 플랫폼 초기 설정. 기동 없음.
typedef struct
{
	STRUCT_Coord_Lla		st_InitLla;
	STRUCT_Coord_Attitude	st_InitAtt;
	FLOAT64					headingSpeed;					// [m/s]
} ST_PlatformInit;

// 시뮬레이션 전체 설정.
typedef struct
{
	FLOAT64					durationTime;					// [s]
	FLOAT64					stepTime;						// [s]
	ST_PlatformInit			st_Platform;
	INT32					nTargetNum;						// 1 ~ TGT_MAX_TARGET_NUM
	ST_TargetInit			st_Target[TGT_MAX_TARGET_NUM];
} ST_SimConfig;

// 객체 하나의 한 시각 상태. 위치, 속도는 ECEF 이고 LLA 는 출력용.
typedef struct
{
	FLOAT64					simTime;						// 시각 [s]
	STRUCT_Coord_Rect		st_PosEcef;						// ECEF 위치 [m]
	STRUCT_Coord_Rect		st_VelEcef;						// ECEF 속도 [m/s]
	STRUCT_Coord_Lla		st_Lla;							// 위도, 경도 [rad], 고도 [m]
	STRUCT_Coord_Attitude	st_Att;							// Roll, Pitch, Yaw [rad]
} ST_TargetState;

// 한 시각의 플랫폼, 표적 상태 (표본).
typedef struct
{
	FLOAT64					simTime;						// 시각 [s] = nStepIndex * stepTime
	INT32					nStepIndex;						// 스텝 번호 (0 ~ nStepNum)
	INT32					nTargetNum;						// 표적 수
	ST_TargetState			st_Platform;					// 플랫폼 상태
	ST_TargetState			st_Target[TGT_MAX_TARGET_NUM];	// 표적 상태 (nTargetNum 개)
} ST_SimSample;

// 시뮬레이션 진행 상태. 설정 사본 포함.
typedef struct
{
	INT32					nStepNum;						// 표본 수 = nStepNum + 1
	ST_SimSample			st_Sample;						// 가장 최근 표본
	ST_SimConfig			st_Config;
} ST_SimState;

TSCORE_API VOID					f_Tgt_InitSim(ST_SimState *st_Sim, const ST_SimConfig *st_Config);
TSCORE_API VOID					f_Tgt_StepSim(ST_SimState *st_Sim);

// 화면 표시용 좌표 변환. 기준점은 플랫폼 t = 0 위치.
TSCORE_API STRUCT_Coord_Rect	f_Tgt_EcefToNed(const STRUCT_Coord_Rect *st_Ecef, const STRUCT_Coord_Rect *st_RefEcef, const STRUCT_Coord_Lla *st_RefLla);
TSCORE_API STRUCT_Coord_Lla		f_Tgt_NedToLla(const STRUCT_Coord_Rect *st_Ned, const STRUCT_Coord_Rect *st_RefEcef, const STRUCT_Coord_Lla *st_RefLla);
TSCORE_API STRUCT_Coord_Rect	f_Tgt_LlaToNed(const STRUCT_Coord_Lla *st_Lla, const STRUCT_Coord_Rect *st_RefEcef, const STRUCT_Coord_Lla *st_RefLla);

#ifdef __cplusplus
}
#endif

#endif
