#include "pch.h"

#include <math.h>

#include "CalcLog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define LOG_RULE				_T("==========================================================================")

// 계산 로그. 콘솔은 f_Open 으로 열 때까지 없음.
CCalcLog::CCalcLog() noexcept
	: isOpen(0)
	, st_Config()
	, nLogObject(0)
	, nLastManeuver(-1)
	, nStepNum(0)
{
}

// 열려 있으면 콘솔 닫기.
CCalcLog::~CCalcLog()
{
	f_Close();
}

// 콘솔 창 열기. 열지 못하면 0 반환.
INT32 CCalcLog::f_Open(VOID)
{
	DWORD inputMode;

	if ((isOpen == 0) && (::AllocConsole() != FALSE))
	{
		isOpen = 1;
		(VOID)::SetConsoleTitle(_T("TargetSim 계산 로그"));

		// 콘솔 창을 닫거나 Ctrl+C 를 누르면 프로그램 전체가 끝나므로 닫기 단추를 없애고 Ctrl+C 는 무시.
		(VOID)::DeleteMenu(::GetSystemMenu(::GetConsoleWindow(), FALSE), SC_CLOSE, MF_BYCOMMAND);
		(VOID)::SetConsoleCtrlHandler(nullptr, TRUE);

		// 콘솔 글을 마우스로 고르는 동안에는 쓰기가 멈춰 UI 도 멈추므로 빠른 편집을 끔 (복사는 오른쪽 클릭 메뉴로).
		if (::GetConsoleMode(::GetStdHandle(STD_INPUT_HANDLE), &inputMode) != FALSE)
		{
			(VOID)::SetConsoleMode(::GetStdHandle(STD_INPUT_HANDLE), (inputMode | ENABLE_EXTENDED_FLAGS) & ~static_cast<DWORD>(ENABLE_QUICK_EDIT_MODE));
		}
	}

	return isOpen;
}

// 콘솔 창 닫기.
VOID CCalcLog::f_Close(VOID)
{
	if (isOpen != 0)
	{
		(VOID)::FreeConsole();
		isOpen = 0;
	}
}

// 콘솔이 열려 있으면 1.
INT32 CCalcLog::f_IsOpen(VOID) const
{
	return isOpen;
}

// 결과 표에서 보는 객체의 계산 과정을 콘솔에 처음부터 다시 씀. nObject: -1 = 전체, 0 = 플랫폼, k = 표적 k.
VOID CCalcLog::f_Write(const CScenario *st_Scenario, INT32 nObject, LPCTSTR pt_Title)
{
	const HANDLE				console = ::GetStdHandle(STD_OUTPUT_HANDLE);
	const COORD					st_Home = { 0, 0 };
	CONSOLE_SCREEN_BUFFER_INFO	st_Info;
	DWORD						nWritten;
	INT32						nIndex;

	if (isOpen != 0)
	{
		// 지난 글 지우기
		if (::GetConsoleScreenBufferInfo(console, &st_Info) != FALSE)
		{
			(VOID)::FillConsoleOutputCharacter(console, _T(' '), static_cast<DWORD>(st_Info.dwSize.X) * static_cast<DWORD>(st_Info.dwSize.Y),
				st_Home, &nWritten);
			(VOID)::SetConsoleCursorPosition(console, st_Home);
		}

		f_Print(_T("TargetSim 계산 로그   %s\n"), pt_Title);
		f_Print(_T("결과 표의 기본 계산 (중점법, 표적 자리 기준).  자세 = (Roll, Pitch, Yaw),  NED = (북, 동, 아래)\n"));

		for (nIndex = 0; nIndex <= st_Scenario->nTargetNum; nIndex++)
		{
			if ((nObject < 0) || (nObject == nIndex))
			{
				f_WriteObject(st_Scenario, nIndex);
			}
		}

		// 커서를 맨 위로 옮겨 처음부터 보이게 함
		(VOID)::SetConsoleCursorPosition(console, st_Home);
	}
}

// 객체 하나: 입력값을 적고, 같은 설정으로 기본 계산을 한 번 더 돌리며 Core 의 기록 (f_OnTrace) 을 계산 순서대로 적음.
VOID CCalcLog::f_WriteObject(const CScenario *st_Scenario, INT32 nObject)
{
	const ST_ObjectText	*st_Object = (nObject == 0) ? &st_Scenario->st_Platform : &st_Scenario->st_Target[nObject - 1];
	ST_SimState			st_Sim = {};
	CString				st_Name;
	INT32				nManeuver;
	INT32				nStep;

	if (nObject == 0)
	{
		st_Name = _T("플랫폼");
	}
	else
	{
		st_Name.Format(_T("표적 %d"), nObject);
	}

	f_Print(_T("\n%s\n %s\n%s\n"), LOG_RULE, st_Name.GetString(), LOG_RULE);

	// 입력: 화면 표의 글자 그대로
	f_Print(_T("[입력]\n  초기값     위도 %s°, 경도 %s°, 고도 %s m, 속력 %s m/s, 자세 (%s, %s, %s)°\n"),
		st_Object->st_FieldText[SCN_FIELD_LAT].GetString(), st_Object->st_FieldText[SCN_FIELD_LON].GetString(),
		st_Object->st_FieldText[SCN_FIELD_ALT].GetString(), st_Object->st_FieldText[SCN_FIELD_SPEED].GetString(),
		st_Object->st_FieldText[SCN_FIELD_ROLL].GetString(), st_Object->st_FieldText[SCN_FIELD_PITCH].GetString(),
		st_Object->st_FieldText[SCN_FIELD_YAW].GetString());

	for (nManeuver = 0; nManeuver < st_Object->nManeuverNum; nManeuver++)
	{
		const ST_ManeuverText *st_Text = &st_Object->st_Maneuver[nManeuver];

		f_Print(_T("  기동 %-2d    %s, %s G, %s ~ %s s\n"), nManeuver + 1, CScenario::f_TurnName(static_cast<INT32>(st_Text->enTurnType)),
			st_Text->st_FieldText[SCN_FIELD_GRAVITY].GetString(), st_Text->st_FieldText[SCN_FIELD_START].GetString(),
			st_Text->st_FieldText[SCN_FIELD_END].GetString());
	}

	if (st_Object->nManeuverNum == 0)
	{
		f_Print(_T("  기동       없음\n"));
	}

	// 기본 계산 설정에 기록 받을 함수만 더함
	st_Config = {};
	st_Scenario->f_BuildConfig(&st_Config);
	st_Config.pf_Trace		= &CCalcLog::f_OnTrace;
	st_Config.pt_TraceUser	= this;
	nLogObject				= nObject;
	nLastManeuver			= -1;

	f_Tgt_InitSim(&st_Sim, &st_Config);
	nStepNum = st_Sim.nStepNum;

	f_Print(_T("\n[스텝 진행]  첫 스텝과 기동이 바뀌는 스텝은 계산 순서대로, 나머지는 1 초마다 한 줄\n"));

	for (nStep = 0; nStep < nStepNum; nStep++)
	{
		f_Tgt_StepSim(&st_Sim);
	}
}

// Core 가 넘기는 기록 하나. 기록할 객체의 것만 적음.
VOID CCalcLog::f_OnTrace(const ST_StepTrace *st_Trace, VOID *pt_User)
{
	CCalcLog *st_Log = static_cast<CCalcLog *>(pt_User);

	if (st_Trace->nObject == st_Log->nLogObject)
	{
		if (st_Trace->nStepIndex < 0)
		{
			st_Log->f_PrintInit(st_Trace);
		}
		else
		{
			st_Log->f_PrintStep(st_Trace);
		}
	}
}

// 0 초 상태 만들기 (f_Tgt_InitState): 입력 각도를 라디안으로, 위경도를 ECEF 위치로, 속력을 NED, ECEF 속도로.
VOID CCalcLog::f_PrintInit(const ST_StepTrace *st_Trace) const
{
	const ST_TargetState *st_State = &st_Trace->st_Start;

	f_Print(_T("\n[0 초 상태]\n"));
	f_Print(_T("  라디안     위도 %.6f, 경도 %.6f, 자세 (%.6f, %.6f, %.6f)\n"), st_State->st_Lla.Lat, st_State->st_Lla.Lon,
		st_State->st_Att.Roll, st_State->st_Att.Pitch, st_State->st_Att.Yaw);
	f_Print(_T("  ECEF 위치  (%.6f°, %.6f°, %.3f m) → (%.1f, %.1f, %.1f) m\n"), f_Rad_To_Deg(st_State->st_Lla.Lat),
		f_Rad_To_Deg(st_State->st_Lla.Lon), st_State->st_Lla.Alt, st_State->st_PosEcef.x, st_State->st_PosEcef.y, st_State->st_PosEcef.z);
	f_Print(_T("  NED 속도   속력 %g m/s 를 자세 (%.2f, %.2f, %.2f)° 로 회전 → (%.2f, %.2f, %.2f) m/s\n"), st_Trace->headingSpeed,
		f_Rad_To_Deg(st_State->st_Att.Roll), f_Rad_To_Deg(st_State->st_Att.Pitch), f_Rad_To_Deg(st_State->st_Att.Yaw),
		st_Trace->st_VelNedStart.x, st_Trace->st_VelNedStart.y, st_Trace->st_VelNedStart.z);
	f_Print(_T("  ECEF 속도  NED 속도를 위경도 (%.6f°, %.6f°) 로 회전 → (%.2f, %.2f, %.2f) m/s\n"), f_Rad_To_Deg(st_State->st_Lla.Lat),
		f_Rad_To_Deg(st_State->st_Lla.Lon), st_Trace->st_VelStart.x, st_Trace->st_VelStart.y, st_Trace->st_VelStart.z);
}

// 스텝 기록 하나. 첫 스텝과 걸린 기동이 바뀐 스텝은 계산 순서대로 적고, 1 초가 될 때마다 한 줄 요약.
VOID CCalcLog::f_PrintStep(const ST_StepTrace *st_Trace)
{
	const ST_TargetState	*st_Next = &st_Trace->st_Next;
	const INT32				nNextStep = st_Trace->nStepIndex + 1;
	const INT32				nPerSecond = static_cast<INT32>((1.0 / st_Trace->stepTime) + 0.5);
	const INT32				nManeuver = f_FindManeuver(st_Trace->st_Start.simTime);

	if ((st_Trace->nStepIndex == 0) || (nManeuver != nLastManeuver))
	{
		f_PrintDetail(st_Trace, nManeuver);
		nLastManeuver = nManeuver;
	}

	if ((nPerSecond <= 1) || ((nNextStep % nPerSecond) == 0) || (nNextStep == nStepNum))
	{
		f_Print(_T("  %5g s   위도 %.6f°   경도 %.6f°   고도 %.3f m   자세 (%.2f, %.2f, %.2f)°\n"), st_Next->simTime,
			f_Rad_To_Deg(st_Next->st_Lla.Lat), f_Rad_To_Deg(st_Next->st_Lla.Lon), st_Next->st_Lla.Alt, f_Rad_To_Deg(st_Next->st_Att.Roll),
			f_Rad_To_Deg(st_Next->st_Att.Pitch), f_Rad_To_Deg(st_Next->st_Att.Yaw));
	}
}

// 스텝 하나를 계산 순서대로 (f_Tgt_GetAttRate, f_Tgt_Propagate). 한 줄에 한 계산.
VOID CCalcLog::f_PrintDetail(const ST_StepTrace *st_Trace, INT32 nManeuver) const
{
	const ST_TargetState	*st_Start = &st_Trace->st_Start;
	const ST_TargetState	*st_Next = &st_Trace->st_Next;
	const FLOAT64			halfStep = 0.5 * st_Trace->stepTime;
	const FLOAT64			dx = st_Next->st_PosEcef.x - st_Start->st_PosEcef.x;
	const FLOAT64			dy = st_Next->st_PosEcef.y - st_Start->st_PosEcef.y;
	const FLOAT64			dz = st_Next->st_PosEcef.z - st_Start->st_PosEcef.z;
	const ST_TargetManeuver	*st_Maneuver;
	CString					st_Event;
	FLOAT64					rate;

	// 머리 줄: 스텝 번호, 시각, 기동이 바뀐 것
	if (st_Trace->nStepIndex == 0)
	{
		st_Event = _T("첫 스텝");
	}

	if ((nLastManeuver >= 0) && (nLastManeuver != nManeuver))
	{
		st_Event.AppendFormat(_T("%s기동 %d 끝"), st_Event.IsEmpty() ? _T("") : _T(", "), nLastManeuver + 1);
	}

	if ((nManeuver >= 0) && (nManeuver != nLastManeuver))
	{
		st_Event.AppendFormat(_T("%s기동 %d 시작"), st_Event.IsEmpty() ? _T("") : _T(", "), nManeuver + 1);
	}

	f_Print(_T("\n스텝 %d   %g → %g s   %s\n"), st_Trace->nStepIndex, st_Start->simTime, st_Next->simTime, st_Event.GetString());

	// 각속도: 기동 축 한 칸에만 값이 있어 세 칸의 합이 그 축의 각속도
	if (nManeuver < 0)
	{
		f_Print(_T("  각속도     걸린 기동 없음 → 0 (직진)\n"));
	}
	else
	{
		st_Maneuver	= &st_Config.st_Target[nLogObject - 1].st_Maneuver[nManeuver];
		rate		= st_Trace->st_AttRate.Roll + st_Trace->st_AttRate.Pitch + st_Trace->st_AttRate.Yaw;

		if (st_Maneuver->enTurnType == TGT_TURN_NONE)
		{
			f_Print(_T("  각속도     회전축 없음 → 0 (직진)\n"));
		}
		else
		{
			f_Print(_T("  각속도     %s, %g G → %g × %g / %g = %.4f rad/s (초당 %.2f°)\n"), CScenario::f_TurnName(static_cast<INT32>(st_Maneuver->enTurnType)),
				st_Maneuver->gravityValue, st_Maneuver->gravityValue, G_FORCE, st_Trace->headingSpeed, rate, f_Rad_To_Deg(rate));
		}
	}

	f_Print(_T("  자세       (%.2f, %.2f, %.2f)° → %g s 뒤 (%.2f, %.2f, %.2f)° → %g s 뒤 (%.2f, %.2f, %.2f)°\n"),
		f_Rad_To_Deg(st_Start->st_Att.Roll), f_Rad_To_Deg(st_Start->st_Att.Pitch), f_Rad_To_Deg(st_Start->st_Att.Yaw), halfStep,
		f_Rad_To_Deg(st_Trace->st_AttMid.Roll), f_Rad_To_Deg(st_Trace->st_AttMid.Pitch), f_Rad_To_Deg(st_Trace->st_AttMid.Yaw), st_Trace->stepTime,
		f_Rad_To_Deg(st_Next->st_Att.Roll), f_Rad_To_Deg(st_Next->st_Att.Pitch), f_Rad_To_Deg(st_Next->st_Att.Yaw));
	f_Print(_T("  출발 속도  NED (%.2f, %.2f, %.2f) → ECEF (%.2f, %.2f, %.2f) m/s\n"), st_Trace->st_VelNedStart.x, st_Trace->st_VelNedStart.y,
		st_Trace->st_VelNedStart.z, st_Trace->st_VelStart.x, st_Trace->st_VelStart.y, st_Trace->st_VelStart.z);
	f_Print(_T("  중간 지점  출발 속도로 %g s → (%.6f°, %.6f°, %.3f m)\n"), halfStep, f_Rad_To_Deg(st_Trace->st_LlaMid.Lat),
		f_Rad_To_Deg(st_Trace->st_LlaMid.Lon), st_Trace->st_LlaMid.Alt);
	f_Print(_T("  중간 속도  NED (%.2f, %.2f, %.2f) → ECEF (%.2f, %.2f, %.2f) m/s\n"), st_Trace->st_VelNedMid.x, st_Trace->st_VelNedMid.y,
		st_Trace->st_VelNedMid.z, st_Trace->st_VelMid.x, st_Trace->st_VelMid.y, st_Trace->st_VelMid.z);
	f_Print(_T("  이동       중간 속도로 %g s, %.2f m → (%.6f°, %.6f°, %.3f m)\n\n"), st_Trace->stepTime, sqrt((dx * dx) + (dy * dy) + (dz * dz)),
		f_Rad_To_Deg(st_Next->st_Lla.Lat), f_Rad_To_Deg(st_Next->st_Lla.Lon), st_Next->st_Lla.Alt);
}

// simTime 에 걸린 기동 번호 (0 부터). 없으면 -1. Core 의 기동 찾기와 같은 규칙 (목록 앞쪽 우선, 구간 [시작, 종료)).
INT32 CCalcLog::f_FindManeuver(FLOAT64 simTime) const
{
	const ST_TargetManeuver	*st_Maneuver;
	INT32					nFound = -1;
	INT32					nManeuver;

	if (nLogObject >= 1)
	{
		for (nManeuver = 0; (nFound < 0) && (nManeuver < st_Config.st_Target[nLogObject - 1].nManeuverNum); nManeuver++)
		{
			st_Maneuver = &st_Config.st_Target[nLogObject - 1].st_Maneuver[nManeuver];

			if ((st_Maneuver->startTime <= simTime) && (simTime < st_Maneuver->endTime))
			{
				nFound = nManeuver;
			}
		}
	}

	return nFound;
}

// 콘솔에 글 쓰기 (printf 서식).
VOID CCalcLog::f_Print(LPCTSTR pt_Format, ...) const
{
	CString	st_Text;
	va_list	args;
	DWORD	nWritten;

	va_start(args, pt_Format);
	st_Text.FormatV(pt_Format, args);
	va_end(args);

	(VOID)::WriteConsole(::GetStdHandle(STD_OUTPUT_HANDLE), st_Text.GetString(), static_cast<DWORD>(st_Text.GetLength()), &nWritten, nullptr);
}
