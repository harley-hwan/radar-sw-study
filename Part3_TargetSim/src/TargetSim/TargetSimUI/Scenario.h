#pragma once

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
#define SCN_PRESET_MULTI		2
#define SCN_PRESET_NUM			3

#define SCN_TEXT_LIMIT			32
#define SCN_TURN_TYPE_NUM		4

// 입력 허용 범위. Core 는 비유한 값과 비물리 값을 거르지 않는다.
#define SCN_LAT_LIMIT			89.9					// [deg]
#define SCN_LON_LIMIT			180.0					// [deg]
#define SCN_ALT_MIN				-1000.0					// [m]
#define SCN_ALT_MAX				100000.0				// [m]
#define SCN_SPEED_MAX			3000.0					// [m/s]
#define SCN_TGT_SPEED_MIN		0.1						// [m/s]
#define SCN_ROLL_LIMIT			180.0					// [deg]
#define SCN_PITCH_LIMIT			90.0					// [deg]
#define SCN_YAW_LIMIT			360.0					// [deg]
#define SCN_G_LIMIT				50.0					// [G]
#define SCN_STEP_MAX			1.0						// [s]
#define SCN_DURATION_MAX		100000.0				// [s]
#define SCN_TIME_RES			1.0e-3					// [s]
#define SCN_GRID_TOL			1.0e-6
#define SCN_TURN_STEP_MAX		(PI / 6.0)				// [rad] 스텝당 회전각

#define RES_CELL_SIZE			48

// CString 을 품은 구조체는 초기값을 두려고 이름 있는 struct 로 선언한다.
struct ST_ManeuverText
{
	EN_TurnType			enTurnType = TGT_TURN_NONE;
	CString				st_FieldText[SCN_MNV_FIELD_NUM];
};

// 객체 하나의 편집값. 화면 글자를 그대로 들고 있다가 실행할 때 숫자로 읽는다. 플랫폼은 기동을 쓰지 않는다.
struct ST_ObjectText
{
	CString				st_FieldText[SCN_OBJ_FIELD_NUM];
	INT32				nManeuverNum = 0;
	ST_ManeuverText		st_Maneuver[TGT_MAX_MANEUVER_NUM];
};

typedef enum
{
	SCN_AT_NONE = 0,
	SCN_AT_DURATION,
	SCN_AT_STEP,
	SCN_AT_PLATFORM,
	SCN_AT_TARGET,
	SCN_AT_MANEUVER
} EN_ScnPlace;

// 입력 오류의 위치와 문구. 해당 없는 칸은 -1
struct ST_ScnIssue
{
	EN_ScnPlace			enPlace = SCN_AT_NONE;
	INT32				nTarget = -1;
	INT32				nManeuver = -1;
	INT32				nField = -1;
	CString				st_Message;
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
	CScenario();

	VOID	f_LoadPreset(INT32 nPreset);
	INT32	f_AddTarget(INT32 nSourceTarget, INT32 isWithManeuver);
	INT32	f_DeleteTarget(INT32 nTarget);
	INT32	f_AddManeuver(INT32 nTarget);
	INT32	f_DeleteManeuver(INT32 nTarget, INT32 nManeuver);

	// 편집값 → 설정. UI 범위 검사 뒤 Core 검증. 실패하면 0 이고 *st_Issue 에 위치와 문구
	INT32	f_BuildConfig(ST_SimConfig *st_Config, ST_ScnIssue *st_Issue);

	INT32	f_Save(const CString &st_Path) const;
	INT32	f_Load(const CString &st_Path, CString *st_Error);

	static LPCTSTR	f_PresetName(INT32 nPreset);
	static LPCTSTR	f_TurnName(INT32 nTurnType);
	static CString	f_StatusText(EN_TgtStatus enStatus, INT32 isManeuver);

	CString			st_DurationText;
	CString			st_StepText;
	ST_ObjectText	st_Platform;
	ST_ObjectText	st_Target[TGT_MAX_TARGET_NUM];
	INT32			nTargetNum;

private:
	INT32	f_ReadNumber(const CString &st_Text, FLOAT64 minValue, FLOAT64 maxValue, LPCTSTR pt_Where, LPCTSTR pt_Name,
				FLOAT64 *pt_Value, ST_ScnIssue *st_Issue) const;
	INT32	f_ReadObject(const ST_ObjectText *st_Object, EN_ScnPlace enPlace, INT32 nTarget, FLOAT64 minSpeed,
				STRUCT_Coord_Lla *st_Lla, STRUCT_Coord_Attitude *st_Att, FLOAT64 *pt_Speed, ST_ScnIssue *st_Issue) const;
	VOID	f_LocateCoreError(const ST_SimConfig *st_Config, EN_TgtStatus enStatus, ST_ScnIssue *st_Issue);

	ST_SimConfig	st_ProbeConfig;						// 오류 위치 탐침용 (10 KB 라 멤버로)
};

// 실행 결과 (출력). Core 를 돌려 표본과 그림 좌표를 채우고, 표와 CSV 서식을 맡는다.
class CSimResult
{
public:
	CSimResult() noexcept;
	~CSimResult();

	// 새 결과를 따로 만든다. 성공하면 1, f_Commit 전까지 지금 결과는 그대로. 실패하면 0 이고 *st_Error 에 문구
	INT32	f_Run(const ST_SimConfig *st_Config, CString *st_Error);
	VOID	f_Commit(VOID);

	INT32					f_GetSampleNum(VOID) const;
	INT32					f_GetObjectNum(VOID) const;
	INT32					f_GetColumnNum(VOID) const;
	FLOAT64					f_GetStepTime(VOID) const;
	FLOAT64					f_GetRunMs(VOID) const;
	const ST_SimSample *	f_GetSample(INT32 nStep) const;
	const ST_TargetState *	f_GetState(INT32 nStep, INT32 nObject) const;
	const ST_PlotPoint *	f_GetPoint(INT32 nStep, INT32 nObject) const;

	// 열: 스텝, 시각, 객체마다 위도, 경도, 고도. 길이를 돌려주고 실패하면 -1
	INT32	f_FormatCell(INT32 nRow, INT32 nColumn, CHAR *pt_Buf, INT32 bufSize) const;
	INT32	f_WriteCsv(const CString &st_Path, INT32 *pt_LineNum) const;

private:
	INT32	f_ComputePoints(const ST_SimSample *st_Sample, ST_PlotPoint *st_Point, INT32 nNewSampleNum, INT32 nNewObjectNum, CString *st_Error) const;

	ST_SimState		st_Sim;

	ST_SimSample	*st_SampleBuf;
	ST_PlotPoint	*st_PointBuf;
	INT32			nSampleNum;
	INT32			nObjectNum;
	FLOAT64			stepTime;
	FLOAT64			runMs;

	ST_SimSample	*st_PendingSample;
	ST_PlotPoint	*st_PendingPoint;
	INT32			nPendingSampleNum;
	INT32			nPendingObjectNum;
	FLOAT64			pendingStepTime;
	FLOAT64			pendingRunMs;
};
