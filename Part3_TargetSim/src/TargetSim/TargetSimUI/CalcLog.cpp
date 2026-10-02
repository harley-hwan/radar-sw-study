#include "pch.h"

#include <math.h>

#include "CalcLog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define LOG_BUFFER_LINES		9999					// 콘솔 화면 버퍼 줄 수. 글이 더 길면 f_Show 에서 늘림
#define LOG_BUFFER_MAX_LINES	32766					// 콘솔 화면 버퍼 최대 줄 수
#define LOG_BUFFER_WIDTH		120						// 콘솔 화면 버퍼 최소 너비 [칸]. 가장 긴 줄이 접히지 않는 너비
#define LOG_WRITE_CHUNK			4096					// 콘솔에 한 번에 쓰는 글자 수
#define LOG_TITLE_WIDTH			20						// 계산 단계 이름 칸 [콘솔 칸]. 한글 한 글자 = 2 칸
#define LOG_LABEL_WIDTH			16						// 값 줄 이름 칸 [콘솔 칸]
#define LOG_RULE				_T("================================================================================\n")

// 콘솔에서 Ctrl+C, Ctrl+Break 를 눌러도 프로그램이 끝나지 않게 함.
static BOOL WINAPI f_Log_OnConsoleCtrl(DWORD ctrlType)
{
	return ((ctrlType == CTRL_C_EVENT) || (ctrlType == CTRL_BREAK_EVENT)) ? TRUE : FALSE;
}

// 들여쓰기 + 이름. 이름 칸은 width 칸으로 맞춤 (콘솔에서 한글은 두 칸).
static VOID f_Log_AddName(CString *st_Text, INT32 indent, LPCTSTR pt_Name, INT32 width)
{
	const TCHAR	*pt_Char;
	INT32		nameWidth = 0;

	for (pt_Char = pt_Name; *pt_Char != _T('\0'); pt_Char++)
	{
		nameWidth = nameWidth + (((*pt_Char >= 0xAC00) && (*pt_Char <= 0xD7A3)) ? 2 : 1);
	}

	st_Text->AppendFormat(_T("%*s%s%*s"), indent, _T(""), pt_Name, (nameWidth < width) ? (width - nameWidth) : 1, _T(""));
}

// 벡터 크기.
static FLOAT64 f_Log_Norm(const STRUCT_Coord_Rect *st_Value)
{
	return sqrt((st_Value->x * st_Value->x) + (st_Value->y * st_Value->y) + (st_Value->z * st_Value->z));
}

// ECEF 위치 또는 속도 한 줄. 속도는 크기도 붙임.
static VOID f_Log_AddXyz(CString *st_Text, LPCTSTR pt_Label, const STRUCT_Coord_Rect *st_Value, INT32 isVelocity)
{
	f_Log_AddName(st_Text, 6, pt_Label, LOG_LABEL_WIDTH);
	st_Text->AppendFormat(_T("x %14.4f   y %14.4f   z %14.4f   "), st_Value->x, st_Value->y, st_Value->z);

	if (isVelocity != 0)
	{
		st_Text->AppendFormat(_T("[m/s]   크기 %.4f\n"), f_Log_Norm(st_Value));
	}
	else
	{
		*st_Text += _T("[m]\n");
	}
}

// 북, 동, 아래 속도 한 줄. 열은 ECEF 줄과 맞춤.
static VOID f_Log_AddNed(CString *st_Text, LPCTSTR pt_Label, const STRUCT_Coord_Rect *st_Ned)
{
	f_Log_AddName(st_Text, 6, pt_Label, LOG_LABEL_WIDTH);
	st_Text->AppendFormat(_T("북 %13.4f   동 %13.4f   아래 %11.4f   [m/s]\n"), st_Ned->x, st_Ned->y, st_Ned->z);
}

// 위도, 경도, 고도 한 줄. 결과 표와 같은 소수 9 자리.
static VOID f_Log_AddLla(CString *st_Text, LPCTSTR pt_Label, const STRUCT_Coord_Lla *st_Lla)
{
	f_Log_AddName(st_Text, 6, pt_Label, LOG_LABEL_WIDTH);
	st_Text->AppendFormat(_T("위도 %12.9f°   경도 %13.9f°   고도 %13.9f m\n"), f_Rad_To_Deg(st_Lla->Lat), f_Rad_To_Deg(st_Lla->Lon), st_Lla->Alt);
}

// 자세 한 줄 [deg]. 열은 ECEF 줄과 맞춤.
static VOID f_Log_AddAtt(CString *st_Text, LPCTSTR pt_Label, const STRUCT_Coord_Attitude *st_Att)
{
	f_Log_AddName(st_Text, 6, pt_Label, LOG_LABEL_WIDTH);
	st_Text->AppendFormat(_T("Roll %11.6f   Pitch %10.6f   Yaw %12.6f   [°]\n"), f_Rad_To_Deg(st_Att->Roll), f_Rad_To_Deg(st_Att->Pitch),
		f_Rad_To_Deg(st_Att->Yaw));
}

// 기동 축의 각속도 [rad/s]. 회전축이 없으면 0.
static FLOAT64 f_Log_AxisRate(const ST_AttRate *st_AttRate, EN_TurnType enTurnType)
{
	FLOAT64 rate = 0.0;

	switch (enTurnType)
	{
	case TGT_TURN_ROLL:
		rate = st_AttRate->Roll;
		break;

	case TGT_TURN_YAW:
		rate = st_AttRate->Yaw;
		break;

	case TGT_TURN_PITCH:
		rate = st_AttRate->Pitch;
		break;

	case TGT_TURN_NONE:
	default:
		// 회전 없음.
		break;
	}

	return rate;
}

// 계산 로그. 콘솔은 f_Open 으로 열 때까지 없음.
CCalcLog::CCalcLog() noexcept
	: consoleOut(nullptr)
	, isVtOutput(0)
	, st_Config()
	, nLogObject(-1)
	, nStepNum(0)
	, nStepPerSecond(1)
	, nLastManeuver()
	, isTableOpen()
{
}

// 열려 있으면 콘솔 닫기.
CCalcLog::~CCalcLog()
{
	f_Close();
}

// 콘솔 창 열기. 이미 열려 있으면 그대로. 열지 못하면 0 반환.
INT32 CCalcLog::f_Open(VOID)
{
	CONSOLE_SCREEN_BUFFER_INFO	st_Info;
	COORD						st_Size;
	HANDLE						consoleIn;
	HWND						consoleWnd;
	DWORD						inputMode;
	DWORD						outputMode;

	if ((consoleOut == nullptr) && (::AllocConsole() != FALSE))
	{
		consoleOut = ::CreateFile(_T("CONOUT$"), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);

		if (consoleOut == INVALID_HANDLE_VALUE)
		{
			consoleOut = nullptr;
			(VOID)::FreeConsole();
		}
		else
		{
			(VOID)::SetConsoleTitle(_T("TargetSim 계산 로그"));
			(VOID)::SetConsoleCtrlHandler(&f_Log_OnConsoleCtrl, TRUE);

			// 콘솔 창을 닫으면 프로그램도 같이 끝나므로 닫기 단추를 없앰. 끄기는 '계산 로그' 체크로.
			consoleWnd = ::GetConsoleWindow();

			if (consoleWnd != nullptr)
			{
				(VOID)::DeleteMenu(::GetSystemMenu(consoleWnd, FALSE), SC_CLOSE, MF_BYCOMMAND);
			}

			// 콘솔을 클릭해 글을 고르는 동안에는 콘솔 쓰기가 멈춰 UI 도 멈추므로 빠른 편집을 끔.
			// 글 복사는 콘솔 창의 오른쪽 클릭 메뉴 (표시, 모두 선택) 로 가능.
			consoleIn = ::CreateFile(_T("CONIN$"), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);

			if (consoleIn != INVALID_HANDLE_VALUE)
			{
				if (::GetConsoleMode(consoleIn, &inputMode) != FALSE)
				{
					(VOID)::SetConsoleMode(consoleIn, (inputMode | ENABLE_EXTENDED_FLAGS) & ~static_cast<DWORD>(ENABLE_QUICK_EDIT_MODE));
				}

				(VOID)::CloseHandle(consoleIn);
			}

			// Windows Terminal 처럼 지난 글을 화면 버퍼 밖에 따로 쌓는 콘솔도 지울 수 있게 VT 명령 처리를 켬. 안 되는 콘솔은 그대로.
			isVtOutput = ((::GetConsoleMode(consoleOut, &outputMode) != FALSE)
				&& (::SetConsoleMode(consoleOut, outputMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != FALSE)) ? 1 : 0;

			// 화면 버퍼를 늘려 로그 앞부분도 스크롤로 볼 수 있게 하고, 긴 줄이 접히지 않게 함.
			if ((::GetConsoleScreenBufferInfo(consoleOut, &st_Info) != FALSE)
				&& ((st_Info.dwSize.X < LOG_BUFFER_WIDTH) || (st_Info.dwSize.Y < LOG_BUFFER_LINES)))
			{
				st_Size.X = (st_Info.dwSize.X > LOG_BUFFER_WIDTH) ? st_Info.dwSize.X : static_cast<SHORT>(LOG_BUFFER_WIDTH);
				st_Size.Y = (st_Info.dwSize.Y > LOG_BUFFER_LINES) ? st_Info.dwSize.Y : static_cast<SHORT>(LOG_BUFFER_LINES);
				(VOID)::SetConsoleScreenBufferSize(consoleOut, st_Size);
			}
		}
	}

	return (consoleOut != nullptr) ? 1 : 0;
}

// 콘솔 창 닫기.
VOID CCalcLog::f_Close(VOID)
{
	if (consoleOut != nullptr)
	{
		(VOID)::CloseHandle(consoleOut);
		(VOID)::SetConsoleCtrlHandler(&f_Log_OnConsoleCtrl, FALSE);
		(VOID)::FreeConsole();
		consoleOut = nullptr;
	}
}

// 콘솔이 열려 있으면 1.
INT32 CCalcLog::f_IsOpen(VOID) const
{
	return (consoleOut != nullptr) ? 1 : 0;
}

// 기본 계산을 한 번 더 돌리며 Core 의 계산 과정 기록을 받아 콘솔에 다시 씀.
// nObject: -1 = 전체, 0 = 플랫폼, k = 표적 k. pt_Title 은 맨 위에 쓸 시나리오 이름.
VOID CCalcLog::f_Write(const CScenario *st_Scenario, INT32 nObject, LPCTSTR pt_Title)
{
	ST_SimState	st_Sim = {};
	CString		st_Text;
	CString		st_View;
	INT32		nObjectNum;
	INT32		nIndex;
	INT32		nStep;

	if (consoleOut != nullptr)
	{
		// 결과 표의 기본 열과 같은 설정에 기록 받을 함수만 더함
		st_Config = {};
		st_Scenario->f_BuildConfig(&st_Config);
		st_Config.pf_Trace		= &CCalcLog::f_OnTrace;
		st_Config.pt_TraceUser	= this;

		nObjectNum		= st_Config.nTargetNum + 1;
		nLogObject		= ((nObject >= 0) && (nObject < nObjectNum)) ? nObject : -1;
		nStepPerSecond	= static_cast<INT32>((1.0 / st_Config.stepTime) + 0.5);
		nStepPerSecond	= (nStepPerSecond > 1) ? nStepPerSecond : 1;

		for (nIndex = 0; nIndex < nObjectNum; nIndex++)
		{
			nLastManeuver[nIndex]	= -1;
			isTableOpen[nIndex]		= 0;
			st_ObjectText[nIndex].Empty();

			if ((nLogObject < 0) || (nLogObject == nIndex))
			{
				f_AddInput(st_Scenario, nIndex);
			}
		}

		// 0 초 상태와 스텝마다 f_OnTrace 로 기록이 들어옴
		f_Tgt_InitSim(&st_Sim, &st_Config);
		nStepNum = st_Sim.nStepNum;

		for (nIndex = 0; nIndex < nObjectNum; nIndex++)
		{
			if ((nLogObject < 0) || (nLogObject == nIndex))
			{
				st_ObjectText[nIndex].AppendFormat(_T("\n[스텝 진행]  f_Tgt_StepSim %d 번. 첫 스텝과 걸린 기동이 바뀐 스텝은 자세히, 나머지는 1 초마다 한 줄\n"),
					nStepNum);
				st_ObjectText[nIndex] += _T("             1 초 요약: 위도 · 경도 · 자세 [°], 고도 [m], 속력 [m/s]. 위경도와 고도는 결과 표와 같은 값\n");
			}
		}

		for (nStep = 0; nStep < nStepNum; nStep++)
		{
			f_Tgt_StepSim(&st_Sim);
		}

		// 머리말 뒤에 객체 순서대로
		if (nLogObject < 0)
		{
			st_View = _T("전체");
		}
		else if (nLogObject == 0)
		{
			st_View = _T("플랫폼");
		}
		else
		{
			st_View.Format(_T("표적 %d"), nLogObject);
		}

		st_Text = _T("TargetSim 계산 로그\n");
		f_Log_AddName(&st_Text, 2, _T("시나리오"), LOG_TITLE_WIDTH);
		st_Text.AppendFormat(_T("%s\n"), pt_Title);
		f_Log_AddName(&st_Text, 2, _T("보기"), LOG_TITLE_WIDTH);
		st_Text.AppendFormat(_T("%s  (결과 표의 보기를 바꾸면 그 객체로 다시 씀)\n"), st_View.GetString());
		f_Log_AddName(&st_Text, 2, _T("계산"), LOG_TITLE_WIDTH);
		st_Text += _T("중점법, 표적 자리 기준 (결과 표의 기본 열). 값은 Core 가 계산에 쓴 그대로\n");

		for (nIndex = 0; nIndex < nObjectNum; nIndex++)
		{
			st_Text += st_ObjectText[nIndex];
		}

		f_Show(st_Text);
	}
}

// Core 가 넘기는 기록 하나. 기록할 객체의 것만 글로 옮김.
VOID CCalcLog::f_OnTrace(const ST_StepTrace *st_Trace, VOID *pt_User)
{
	CCalcLog *st_Log = static_cast<CCalcLog *>(pt_User);

	if ((st_Log->nLogObject < 0) || (st_Log->nLogObject == st_Trace->nObject))
	{
		if (st_Trace->nStepIndex < 0)
		{
			st_Log->f_AddInit(st_Trace);
		}
		else
		{
			st_Log->f_AddStep(st_Trace);
		}
	}
}

// 객체 제목과 입력값 (화면 표의 글자 그대로).
VOID CCalcLog::f_AddInput(const CScenario *st_Scenario, INT32 nObject)
{
	const ST_ObjectText	*st_Object = (nObject == 0) ? &st_Scenario->st_Platform : &st_Scenario->st_Target[nObject - 1];
	CString				*st_Text = &st_ObjectText[nObject];
	INT32				nManeuver;

	*st_Text += _T("\n");
	*st_Text += LOG_RULE;

	if (nObject == 0)
	{
		*st_Text += _T("  플랫폼\n");
	}
	else
	{
		st_Text->AppendFormat(_T("  표적 %d\n"), nObject);
	}

	*st_Text += LOG_RULE;
	*st_Text += _T("\n[입력]  화면 표에 넣은 값\n");

	f_Log_AddName(st_Text, 2, _T("초기값"), LOG_TITLE_WIDTH);
	st_Text->AppendFormat(_T("위도 %s°   경도 %s°   고도 %s m   속력 %s m/s\n"), st_Object->st_FieldText[SCN_FIELD_LAT].GetString(),
		st_Object->st_FieldText[SCN_FIELD_LON].GetString(), st_Object->st_FieldText[SCN_FIELD_ALT].GetString(),
		st_Object->st_FieldText[SCN_FIELD_SPEED].GetString());
	f_Log_AddName(st_Text, 2, _T(""), LOG_TITLE_WIDTH);
	st_Text->AppendFormat(_T("Roll %s°   Pitch %s°   Yaw %s°\n"), st_Object->st_FieldText[SCN_FIELD_ROLL].GetString(),
		st_Object->st_FieldText[SCN_FIELD_PITCH].GetString(), st_Object->st_FieldText[SCN_FIELD_YAW].GetString());

	f_Log_AddName(st_Text, 2, _T("기동"), LOG_TITLE_WIDTH);

	if (nObject == 0)
	{
		*st_Text += _T("없음 (플랫폼은 처음 자세 그대로 움직임)\n");
	}
	else if (st_Object->nManeuverNum == 0)
	{
		*st_Text += _T("없음 (직진)\n");
	}
	else
	{
		st_Text->AppendFormat(_T("%d 개. G 부호가 회전 방향, 구간은 [시작, 종료)\n"), st_Object->nManeuverNum);

		for (nManeuver = 0; nManeuver < st_Object->nManeuverNum; nManeuver++)
		{
			const ST_ManeuverText *st_Maneuver = &st_Object->st_Maneuver[nManeuver];

			st_Text->AppendFormat(_T("%*s%3d   "), 2 + LOG_TITLE_WIDTH, _T(""), nManeuver + 1);
			f_Log_AddName(st_Text, 0, CScenario::f_TurnName(static_cast<INT32>(st_Maneuver->enTurnType)), 10);
			st_Text->AppendFormat(_T("%6s G   %6s ~ %s s\n"), st_Maneuver->st_FieldText[SCN_FIELD_GRAVITY].GetString(),
				st_Maneuver->st_FieldText[SCN_FIELD_START].GetString(), st_Maneuver->st_FieldText[SCN_FIELD_END].GetString());
		}
	}
}

// 0 초 상태 만들기 (f_Tgt_InitState): 각도 -> 라디안, 위경도 -> ECEF 위치, 속력 -> NED -> ECEF 속도.
VOID CCalcLog::f_AddInit(const ST_StepTrace *st_Trace)
{
	const ST_TargetState	*st_State = &st_Trace->st_Start;
	CString					*st_Text = &st_ObjectText[st_Trace->nObject];

	*st_Text += _T("\n[0 초 상태]  f_Tgt_InitState\n");

	f_Log_AddName(st_Text, 2, _T("각도를 라디안으로"), LOG_TITLE_WIDTH);
	*st_Text += _T("도 × π / 180. UI 가 설정을 만들 때 바꿔 넘김 (CScenario::f_BuildConfig)\n");
	f_Log_AddName(st_Text, 6, _T("위경도"), LOG_LABEL_WIDTH);
	st_Text->AppendFormat(_T("위도 %.9f   경도 %.9f   [rad]   고도 %.9f m\n"), st_State->st_Lla.Lat, st_State->st_Lla.Lon, st_State->st_Lla.Alt);
	f_Log_AddName(st_Text, 6, _T("자세"), LOG_LABEL_WIDTH);
	st_Text->AppendFormat(_T("Roll %.9f   Pitch %.9f   Yaw %.9f   [rad]\n"), st_State->st_Att.Roll, st_State->st_Att.Pitch, st_State->st_Att.Yaw);

	f_Log_AddName(st_Text, 2, _T("ECEF 위치"), LOG_TITLE_WIDTH);
	*st_Text += _T("f_Trans_Lla_To_Ecef (경도, 위도, 고도)\n");
	f_Log_AddXyz(st_Text, _T("위치"), &st_State->st_PosEcef, 0);

	f_Log_AddName(st_Text, 2, _T("속도를 NED 로"), LOG_TITLE_WIDTH);
	st_Text->AppendFormat(_T("기수 방향 속도 (%g, 0, 0) 를 자세로 회전. f_Trans_Body_To_Ned (V, 0, 0, Roll, Yaw, Pitch)\n"), st_Trace->headingSpeed);
	f_Log_AddNed(st_Text, _T("NED"), &st_Trace->st_VelNedStart);

	f_Log_AddName(st_Text, 2, _T("속도를 ECEF 로"), LOG_TITLE_WIDTH);
	*st_Text += _T("이 자리의 위도, 경도로 회전만. f_Trans_Ned_To_Ecef (북, 동, 아래, 0, 0, 0, 위도, 경도)\n");
	f_Log_AddXyz(st_Text, _T("속도"), &st_Trace->st_VelStart, 1);
}

// 스텝 기록 하나. 첫 스텝과 걸린 기동이 바뀐 스텝은 자세히 적고, 1 초가 될 때마다 요약 한 줄.
VOID CCalcLog::f_AddStep(const ST_StepTrace *st_Trace)
{
	const INT32	nObject = st_Trace->nObject;
	const INT32	nNextStep = st_Trace->nStepIndex + 1;
	const INT32	nManeuver = f_FindManeuver(nObject, static_cast<FLOAT64>(st_Trace->nStepIndex) * st_Trace->stepTime);

	if ((st_Trace->nStepIndex == 0) || (nManeuver != nLastManeuver[nObject]))
	{
		f_AddStepDetail(st_Trace, nManeuver);
		nLastManeuver[nObject]	= nManeuver;
		isTableOpen[nObject]	= 0;
	}

	if (((nNextStep % nStepPerSecond) == 0) || (nNextStep == nStepNum))
	{
		f_AddSummary(st_Trace);
	}
}

// 스텝 하나를 자세히. 기동 찾기 다음은 f_Tgt_Propagate 의 계산 순서 그대로.
VOID CCalcLog::f_AddStepDetail(const ST_StepTrace *st_Trace, INT32 nManeuver)
{
	const INT32				nObject = st_Trace->nObject;
	const INT32				nLast = nLastManeuver[nObject];
	const FLOAT64			halfStep = 0.5 * st_Trace->stepTime;
	const ST_TargetManeuver	*st_Maneuver;
	CString					*st_Text = &st_ObjectText[nObject];
	CString					st_Name;
	STRUCT_Coord_Rect		st_Move;
	FLOAT64					rate;

	// 제목: 스텝 번호, 시각, 기동이 바뀐 것
	st_Text->AppendFormat(_T("\n[스텝 %d → %d]  %.3f → %.3f s"), st_Trace->nStepIndex, st_Trace->nStepIndex + 1, st_Trace->st_Start.simTime,
		st_Trace->st_Next.simTime);

	if (st_Trace->nStepIndex == 0)
	{
		*st_Text += _T("   첫 스텝");
	}

	if ((nLast >= 0) && (nLast != nManeuver))
	{
		st_Text->AppendFormat(_T("   기동 %d 끝"), nLast + 1);
	}

	if ((nManeuver >= 0) && (nManeuver != nLast))
	{
		st_Text->AppendFormat(_T("   기동 %d 시작"), nManeuver + 1);
	}

	*st_Text += _T("\n");

	f_Log_AddName(st_Text, 2, _T("시작 상태"), LOG_TITLE_WIDTH);
	*st_Text += (st_Trace->nStepIndex == 0) ? _T("0 초 상태\n") : _T("지난 스텝의 결과\n");
	f_Log_AddLla(st_Text, _T("위경도"), &st_Trace->st_Start.st_Lla);
	f_Log_AddAtt(st_Text, _T("자세"), &st_Trace->st_Start.st_Att);
	f_Log_AddXyz(st_Text, _T("위치"), &st_Trace->st_Start.st_PosEcef, 0);

	// 기동 찾기 (f_Tgt_GetAttRate)
	f_Log_AddName(st_Text, 2, _T("기동 찾기"), LOG_TITLE_WIDTH);

	if (nManeuver < 0)
	{
		st_Text->AppendFormat(_T("%.3f s 에 걸린 기동 없음. 각속도 0 (직진)\n"), st_Trace->st_Start.simTime);
	}
	else
	{
		st_Maneuver = &st_Config.st_Target[nObject - 1].st_Maneuver[nManeuver];
		st_Text->AppendFormat(_T("%.3f s 에 걸린 기동 %d (%s, %g G, %g ~ %g s)\n"), st_Trace->st_Start.simTime, nManeuver + 1,
			CScenario::f_TurnName(static_cast<INT32>(st_Maneuver->enTurnType)), st_Maneuver->gravityValue, st_Maneuver->startTime, st_Maneuver->endTime);
		f_Log_AddName(st_Text, 2, _T(""), LOG_TITLE_WIDTH);

		if (st_Maneuver->enTurnType == TGT_TURN_NONE)
		{
			*st_Text += _T("회전축이 없어 각속도 0 (직진)\n");
		}
		else
		{
			rate = f_Log_AxisRate(&st_Trace->st_AttRate, st_Maneuver->enTurnType);
			st_Text->AppendFormat(_T("G × %g / V = %g × %g / %g = %.6f rad/s (초당 %.3f°). 기동 축에만 넣음\n"), G_FORCE, st_Maneuver->gravityValue,
				G_FORCE, st_Trace->headingSpeed, rate, f_Rad_To_Deg(rate));
		}
	}

	f_Log_AddName(st_Text, 6, _T("각속도"), LOG_LABEL_WIDTH);
	st_Text->AppendFormat(_T("Roll %11.6f   Pitch %10.6f   Yaw %12.6f   [rad/s]\n"), st_Trace->st_AttRate.Roll, st_Trace->st_AttRate.Pitch,
		st_Trace->st_AttRate.Yaw);

	// 0.05초 뒤 자세 (방향을 구하는 데만 씀), 0.1초 뒤 자세 (다음 스텝으로 넘어감)
	st_Name.Format(_T("%g초 뒤 자세"), halfStep);
	f_Log_AddName(st_Text, 2, st_Name, LOG_TITLE_WIDTH);
	st_Text->AppendFormat(_T("= 자세 + 각속도 × %g s\n"), halfStep);
	f_Log_AddAtt(st_Text, _T("자세"), &st_Trace->st_AttMid);

	st_Name.Format(_T("%g초 뒤 자세"), st_Trace->stepTime);
	f_Log_AddName(st_Text, 2, st_Name, LOG_TITLE_WIDTH);
	st_Text->AppendFormat(_T("= 자세 + 각속도 × %g s\n"), st_Trace->stepTime);
	f_Log_AddAtt(st_Text, _T("자세"), &st_Trace->st_Next.st_Att);

	// 0.05초 뒤 위치: 출발 속도는 시작 자세와 위경도로 구함 (f_Tgt_GetVelEcef)
	st_Name.Format(_T("%g초 뒤 위치"), halfStep);
	f_Log_AddName(st_Text, 2, st_Name, LOG_TITLE_WIDTH);
	st_Text->AppendFormat(_T("= 위치 + 출발 속도 × %g s. 출발 속도는 시작 자세로 NED, 시작 위경도로 ECEF\n"), halfStep);
	f_Log_AddNed(st_Text, _T("출발 속도 NED"), &st_Trace->st_VelNedStart);
	f_Log_AddXyz(st_Text, _T("출발 속도"), &st_Trace->st_VelStart, 1);
	f_Log_AddXyz(st_Text, _T("위치"), &st_Trace->st_PosMid, 0);

	// 중간 지점 속도: 0.05초 뒤 위치의 위경도 (f_Trans_Ecef_To_Lla) 와 0.05초 뒤 자세로 다시 구함
	f_Log_AddName(st_Text, 2, _T("중간 지점 속도"), LOG_TITLE_WIDTH);
	st_Text->AppendFormat(_T("%g초 뒤 위치를 위경도로 바꾸고 (f_Trans_Ecef_To_Lla), %g초 뒤 자세로 속도를 다시 구함\n"), halfStep, halfStep);
	f_Log_AddLla(st_Text, _T("위경도"), &st_Trace->st_LlaMid);
	f_Log_AddNed(st_Text, _T("속도 NED"), &st_Trace->st_VelNedMid);
	f_Log_AddXyz(st_Text, _T("속도"), &st_Trace->st_VelMid, 1);

	// 0.1초 이동: 처음 위치에서 중간 지점 속도로
	st_Move.x = st_Trace->st_Next.st_PosEcef.x - st_Trace->st_Start.st_PosEcef.x;
	st_Move.y = st_Trace->st_Next.st_PosEcef.y - st_Trace->st_Start.st_PosEcef.y;
	st_Move.z = st_Trace->st_Next.st_PosEcef.z - st_Trace->st_Start.st_PosEcef.z;
	st_Name.Format(_T("%g초 이동"), st_Trace->stepTime);
	f_Log_AddName(st_Text, 2, st_Name, LOG_TITLE_WIDTH);
	st_Text->AppendFormat(_T("= 위치 + 중간 지점 속도 × %g s. 움직인 거리 %.4f m\n"), st_Trace->stepTime, f_Log_Norm(&st_Move));
	f_Log_AddXyz(st_Text, _T("위치"), &st_Trace->st_Next.st_PosEcef, 0);

	// 새 위경도와 속도: 다음 스텝의 시작 상태
	f_Log_AddName(st_Text, 2, _T("새 위경도와 속도"), LOG_TITLE_WIDTH);
	st_Text->AppendFormat(_T("새 위치를 위경도로 바꾸고, %g초 뒤 자세로 속도를 구함. 다음 스텝의 시작 상태\n"), st_Trace->stepTime);
	f_Log_AddLla(st_Text, _T("위경도"), &st_Trace->st_Next.st_Lla);
	f_Log_AddXyz(st_Text, _T("속도"), &st_Trace->st_Next.st_VelEcef, 1);
}

// 1 초 요약 한 줄. 자세히 적은 스텝 뒤에는 열 제목부터.
VOID CCalcLog::f_AddSummary(const ST_StepTrace *st_Trace)
{
	const ST_TargetState	*st_Next = &st_Trace->st_Next;
	CString					*st_Text = &st_ObjectText[st_Trace->nObject];

	if (isTableOpen[st_Trace->nObject] == 0)
	{
		*st_Text += _T("\n    스텝      시각          위도           경도           고도      Roll     Pitch       Yaw       속력\n");
		isTableOpen[st_Trace->nObject] = 1;
	}

	st_Text->AppendFormat(_T("  %6d  %8.3f  %12.9f  %13.9f  %13.9f  %8.3f  %8.3f  %8.3f  %9.4f\n"), st_Trace->nStepIndex + 1, st_Next->simTime,
		f_Rad_To_Deg(st_Next->st_Lla.Lat), f_Rad_To_Deg(st_Next->st_Lla.Lon), st_Next->st_Lla.Alt, f_Rad_To_Deg(st_Next->st_Att.Roll),
		f_Rad_To_Deg(st_Next->st_Att.Pitch), f_Rad_To_Deg(st_Next->st_Att.Yaw), f_Log_Norm(&st_Next->st_VelEcef));
}

// simTime 에 걸린 기동 번호 (0 부터). 없으면 -1. Core 의 기동 찾기와 같은 규칙 (목록 앞쪽 우선, 구간 [시작, 종료)).
INT32 CCalcLog::f_FindManeuver(INT32 nObject, FLOAT64 simTime) const
{
	const ST_TargetManeuver	*st_Maneuver;
	INT32					nFound = -1;
	INT32					nManeuver;

	if (nObject >= 1)
	{
		for (nManeuver = 0; (nFound < 0) && (nManeuver < st_Config.st_Target[nObject - 1].nManeuverNum); nManeuver++)
		{
			st_Maneuver = &st_Config.st_Target[nObject - 1].st_Maneuver[nManeuver];

			if ((st_Maneuver->startTime <= simTime) && (simTime < st_Maneuver->endTime))
			{
				nFound = nManeuver;
			}
		}
	}

	return nFound;
}

// 콘솔을 지우고 글 전체를 씀. 줄이 화면 버퍼보다 많으면 버퍼를 늘리고, 다 쓴 뒤 창을 맨 위로 올려 입력부터 보이게 함.
VOID CCalcLog::f_Show(const CString &st_Text) const
{
	const COORD					st_Home = { 0, 0 };
	const TCHAR					*pt_Text = st_Text.GetString();
	const INT32					nLength = st_Text.GetLength();
	CONSOLE_SCREEN_BUFFER_INFO	st_Info;
	SMALL_RECT					st_Window;
	COORD						st_Size;
	DWORD						nCellNum;
	DWORD						nWritten;
	INT32						isInfo;
	INT32						nLineNum = 1;
	INT32						nPos;
	INT32						nChunk;

	for (nPos = 0; nPos < nLength; nPos++)
	{
		nLineNum = nLineNum + ((pt_Text[nPos] == _T('\n')) ? 1 : 0);
	}

	// 화면 버퍼 밖에 쌓인 지난 글 지우기 (ESC [3J)
	if (isVtOutput != 0)
	{
		(VOID)::WriteConsole(consoleOut, _T("\x1b[3J"), 4, &nWritten, nullptr);
	}

	isInfo = (::GetConsoleScreenBufferInfo(consoleOut, &st_Info) != FALSE) ? 1 : 0;

	if (isInfo != 0)
	{
		if ((nLineNum >= st_Info.dwSize.Y) && (st_Info.dwSize.Y < LOG_BUFFER_MAX_LINES))
		{
			st_Size.X = st_Info.dwSize.X;
			st_Size.Y = static_cast<SHORT>((nLineNum < LOG_BUFFER_MAX_LINES) ? (nLineNum + 1) : LOG_BUFFER_MAX_LINES);

			if (::SetConsoleScreenBufferSize(consoleOut, st_Size) != FALSE)
			{
				st_Info.dwSize = st_Size;
			}
		}

		nCellNum = static_cast<DWORD>(st_Info.dwSize.X) * static_cast<DWORD>(st_Info.dwSize.Y);
		(VOID)::FillConsoleOutputCharacter(consoleOut, _T(' '), nCellNum, st_Home, &nWritten);
		(VOID)::FillConsoleOutputAttribute(consoleOut, st_Info.wAttributes, nCellNum, st_Home, &nWritten);
		(VOID)::SetConsoleCursorPosition(consoleOut, st_Home);
	}

	// 한 번에 쓰는 양에 한도가 있어 나눠 씀
	for (nPos = 0; nPos < nLength; nPos = nPos + nChunk)
	{
		nChunk = ((nLength - nPos) < LOG_WRITE_CHUNK) ? (nLength - nPos) : LOG_WRITE_CHUNK;
		(VOID)::WriteConsole(consoleOut, pt_Text + nPos, static_cast<DWORD>(nChunk), &nWritten, nullptr);
	}

	if (isInfo != 0)
	{
		st_Window.Left		= 0;
		st_Window.Top		= 0;
		st_Window.Right		= static_cast<SHORT>(st_Info.srWindow.Right - st_Info.srWindow.Left);
		st_Window.Bottom	= static_cast<SHORT>(st_Info.srWindow.Bottom - st_Info.srWindow.Top);
		(VOID)::SetConsoleWindowInfo(consoleOut, TRUE, &st_Window);
	}
}
