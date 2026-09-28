#pragma once

#include <vector>

#include "TargetSim.h"

// 객체 표의 열 순서
#define SCN_OBJ_FIELD_NUM		7						// 위도, 경도, 고도, 속력, Roll, Pitch, Yaw
#define SCN_MNV_FIELD_NUM		3						// G, 시작, 종료
#define SCN_FIELD_LAT			0
#define SCN_FIELD_LON			1
#define SCN_FIELD_ALT			2
#define SCN_FIELD_SPEED			3
#define SCN_FIELD_ROLL			4
#define SCN_FIELD_PITCH			5
#define SCN_FIELD_YAW			6
#define SCN_FIELD_GRAVITY		0
#define SCN_FIELD_START			1
#define SCN_FIELD_END			2

#define SCN_PRESET_SPEC			0
#define SCN_PRESET_MANEUVER		1
#define SCN_PRESET_NUM			2

#define SCN_TURN_TYPE_NUM		4

// 과제 명세의 시뮬레이션 조건
#define SCN_DURATION_TIME		60.0					// [s]
#define SCN_STEP_TIME			0.1						// [s]

#define RES_CELL_SIZE			48

// 기동 1건의 편집값.
struct ST_ManeuverText
{
	EN_TurnType			enTurnType = TGT_TURN_NONE;
	CString				st_FieldText[SCN_MNV_FIELD_NUM];
};

// 객체 하나의 편집값 (화면 글자). 플랫폼은 기동 미사용.
struct ST_ObjectText
{
	CString				st_FieldText[SCN_OBJ_FIELD_NUM];
	INT32				nManeuverNum = 0;
	ST_ManeuverText		st_Maneuver[TGT_MAX_MANEUVER_NUM];
};

// 플랫폼 표본 0 기준 국지 수평 좌표 [m]
typedef struct
{
	FLOAT64		east;
	FLOAT64		north;
} ST_PlotPoint;

// 시나리오 (입력)
class CScenario
{
public:
	VOID	f_LoadPreset(INT32 nPreset);
	INT32	f_AddTarget(INT32 nSourceTarget);
	VOID	f_DeleteTarget(INT32 nTarget);
	INT32	f_AddManeuver(INT32 nTarget);
	VOID	f_DeleteManeuver(INT32 nTarget, INT32 nManeuver);

	// 편집 글자 -> 설정
	VOID	f_BuildConfig(ST_SimConfig *st_Config) const;

	static LPCTSTR	f_PresetName(INT32 nPreset);
	static LPCTSTR	f_TurnName(INT32 nTurnType);

	ST_ObjectText	st_Platform;
	ST_ObjectText	st_Target[TGT_MAX_TARGET_NUM];
	INT32			nTargetNum = 0;
};

// 실행 결과 (출력). 표본, 그림 좌표 보관 및 표, CSV 출력.
class CSimResult
{
public:
	VOID	f_Run(const ST_SimConfig *st_Config);

	INT32					f_GetSampleNum(VOID) const;
	INT32					f_GetObjectNum(VOID) const;
	const ST_SimSample *	f_GetSample(INT32 nStep) const;
	const ST_TargetState *	f_GetState(INT32 nStep, INT32 nObject) const;
	const ST_PlotPoint *	f_GetPoint(INT32 nStep, INT32 nObject) const;

	// 열: 스텝, 시각, 객체마다 위도, 경도, 고도
	VOID	f_FormatCell(INT32 nRow, INT32 nColumn, CHAR *pt_Buf, INT32 bufSize) const;
	INT32	f_WriteCsv(const CString &st_Path) const;

private:
	VOID	f_ComputePoints(VOID);

	std::vector<ST_SimSample>	st_SampleBuf;
	std::vector<ST_PlotPoint>	st_PointBuf;					// 표본마다 객체 수만큼
	INT32						nObjectNum = 0;
};
