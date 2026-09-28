#ifndef TARGET_SIM_H
#define TARGET_SIM_H

#include "Define.h"
#include "CoordinateTransform.h"

// DLL 경계. Core 를 빌드할 때만 TARGETSIMCORE_EXPORTS 가 정의된다.
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

// 기동 회전축. 값은 과제 명세를 그대로 따른다.
typedef enum
{
	TGT_TURN_NONE	= 0,
	TGT_TURN_ROLL	= 1,
	TGT_TURN_YAW	= 2,
	TGT_TURN_PITCH	= 3
} EN_TurnType;

// 기동 1건. gravityValue 의 부호가 회전 방향이며 동체축 오른손 법칙을 따른다.
// (+) 는 우측 뱅크 / 우선회 / 기수 상승. 구간은 [startTime, endTime) 이다.
typedef struct
{
	EN_TurnType				enTurnType;
	FLOAT64					gravityValue;					// [G] 중력가속도 배수
	FLOAT64					startTime;						// [s]
	FLOAT64					endTime;						// [s]
} ST_TargetManeuver;

// 표적 초기 설정.
// 위치는 위도/경도/고도, 속도는 동체 x 축(heading) 방향 크기 V_heading 으로 받는다.
typedef struct
{
	STRUCT_Coord_Lla		st_InitLla;						// Lat, Lon [rad] / Alt [m]
	STRUCT_Coord_Attitude	st_InitAtt;						// Roll, Pitch, Yaw [rad]
	FLOAT64					headingSpeed;					// V_heading [m/s]
	INT32					nManeuverNum;					// 0 ~ TGT_MAX_MANEUVER_NUM
	ST_TargetManeuver		st_Maneuver[TGT_MAX_MANEUVER_NUM];
} ST_TargetInit;

// 플랫폼 초기 설정. 기동은 갖지 않는다.
typedef struct
{
	STRUCT_Coord_Lla		st_InitLla;
	STRUCT_Coord_Attitude	st_InitAtt;
	FLOAT64					headingSpeed;					// [m/s]
} ST_PlatformInit;

// 시뮬레이션 전체 설정
typedef struct
{
	FLOAT64					durationTime;					// [s]
	FLOAT64					stepTime;						// [s]
	ST_PlatformInit			st_Platform;
	INT32					nTargetNum;						// 1 ~ TGT_MAX_TARGET_NUM
	ST_TargetInit			st_Target[TGT_MAX_TARGET_NUM];
} ST_SimConfig;

// 한 시각의 상태. 위치와 속도는 ECEF 에서 갱신하고, LLA 는 출력과 다음 스텝의 NED 기준으로 함께 보관한다.
typedef struct
{
	FLOAT64					simTime;						// [s]
	STRUCT_Coord_Rect		st_PosEcef;						// [m]
	STRUCT_Coord_Rect		st_VelEcef;						// [m/s]
	STRUCT_Coord_Lla		st_Lla;							// Lat, Lon [rad] / Alt [m]
	STRUCT_Coord_Attitude	st_Att;							// Roll, Pitch, Yaw [rad]
} ST_TargetState;

// 한 시각의 플랫폼·표적 상태. 호출자는 스텝마다 이 구조체를 복사해 보관한다.
typedef struct
{
	FLOAT64					simTime;						// [s] nStepIndex * stepTime
	INT32					nStepIndex;						// 0 ~ nStepNum
	INT32					nTargetNum;
	ST_TargetState			st_Platform;
	ST_TargetState			st_Target[TGT_MAX_TARGET_NUM];	// 설정과 같은 인덱스, 앞 nTargetNum 칸만 쓴다
} ST_SimSample;

// 시나리오 진행 상태. 설정 사본을 함께 두어 초기화 뒤 원본 설정이 바뀌어도 진행에 영향이 없다.
typedef struct
{
	INT32					nStepNum;						// 표본 수는 nStepNum + 1
	ST_SimSample			st_Sample;						// 가장 최근 표본
	ST_SimConfig			st_Config;
} ST_SimState;

TSCORE_API VOID					f_Tgt_InitSim(ST_SimState *st_Sim, const ST_SimConfig *st_Config);
TSCORE_API VOID					f_Tgt_StepSim(ST_SimState *st_Sim);

// 화면용 좌표 변환. 원본의 인자 순서 규약은 Core 안에서 흡수한다.
// 기준점(st_Ref...)은 보통 플랫폼의 t = 0 표본이다.
TSCORE_API STRUCT_Coord_Rect	f_Tgt_EcefToNed(const STRUCT_Coord_Rect *st_Ecef, const STRUCT_Coord_Rect *st_RefEcef, const STRUCT_Coord_Lla *st_RefLla);
TSCORE_API STRUCT_Coord_Lla		f_Tgt_NedToLla(const STRUCT_Coord_Rect *st_Ned, const STRUCT_Coord_Rect *st_RefEcef, const STRUCT_Coord_Lla *st_RefLla);
TSCORE_API STRUCT_Coord_Rect	f_Tgt_LlaToNed(const STRUCT_Coord_Lla *st_Lla, const STRUCT_Coord_Rect *st_RefEcef, const STRUCT_Coord_Lla *st_RefLla);

#ifdef __cplusplus
}
#endif

#endif
