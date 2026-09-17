#pragma once

#include "TargetSim_JH.h"

#define SCN_OBJ_FIELD_NUM		7						// 위도, 경도, 고도, 속력, Roll, Pitch, Yaw (객체 표의 열 순서)
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
#define SCN_ADD_NO_ROOM			(-2)					// f_AddManeuver: 직전 기동이 시뮬레이션 끝까지 차 있어 넣을 자리가 없다
#define SCN_TURN_TYPE_NUM		4

// 입력 허용 범위. Core 는 비유한 값과 비물리 값을 걸러내지 않으므로 여기서 막는다.
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

// CString 을 품은 아래 세 구조체는 멤버 초기값을 두려고 이름 있는 구조체로 선언한다.
struct ST_ManeuverText
{
	EN_TurnType			enTurnType = TGT_TURN_NONE;
	CString				st_FieldText[SCN_MNV_FIELD_NUM];
};

// 객체 하나의 편집값. 화면에 보이는 글자를 그대로 들고 있다가 실행할 때만 숫자로 읽는다.
// CString 이 있으므로 memset 하지 말고 대입으로 비운다. 플랫폼은 기동을 쓰지 않는다.
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

// 입력 오류의 위치와 문구. 해당 없는 칸은 -1.
struct ST_ScnIssue
{
	EN_ScnPlace			enPlace = SCN_AT_NONE;
	INT32				nTarget = -1;
	INT32				nManeuver = -1;
	INT32				nField = -1;
	CString				st_Message;
};

class CScenario
{
public:
	CScenario();

	VOID	f_LoadPreset(INT32 nPreset);
	INT32	f_AddTarget(INT32 nSourceTarget, INT32 isWithManeuver);
	INT32	f_DeleteTarget(INT32 nTarget);
	INT32	f_AddManeuver(INT32 nTarget);
	INT32	f_DeleteManeuver(INT32 nTarget, INT32 nManeuver);

	// 편집값을 읽어 설정을 만든다. UI 범위 검사 뒤 Core 검증까지 거친다. 실패하면 0 이고 *st_Issue 에 위치와 문구가 든다.
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
				ST_CoordLla *st_Lla, ST_CoordAtt *st_Att, FLOAT64 *pt_Speed, ST_ScnIssue *st_Issue) const;
	VOID	f_LocateCoreError(const ST_SimConfig *st_Config, EN_TgtStatus enStatus, ST_ScnIssue *st_Issue);

	// Core 오류 위치를 찾을 때 쓰는 설정 사본. 10 KB 라 스택에 두지 않는다.
	ST_SimConfig	st_ProbeConfig;
};
