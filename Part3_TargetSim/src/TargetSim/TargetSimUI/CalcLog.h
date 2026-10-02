#pragma once

#include "Scenario.h"
#include "UiCommon.h"

// 계산 로그: 기본 계산 (중점법, 표적 자리 기준) 의 과정을 콘솔 창에 씀.
// 같은 설정으로 한 번 더 계산하면서 Core 가 넘기는 기록 (ST_StepTrace) 을 받아 입력 -> 0 초 상태 -> 스텝 순서로 적음.
// 스텝은 첫 스텝과 걸린 기동이 바뀌는 스텝만 자세히, 나머지는 1 초마다 한 줄.
class CCalcLog
{
public:
	CCalcLog() noexcept;
	~CCalcLog();
	CCalcLog(const CCalcLog &) = delete;
	CCalcLog &operator=(const CCalcLog &) = delete;

	INT32	f_Open(VOID);
	VOID	f_Close(VOID);
	INT32	f_IsOpen(VOID) const;
	VOID	f_Write(const CScenario *st_Scenario, INT32 nObject, LPCTSTR pt_Title);

private:
	static VOID	f_OnTrace(const ST_StepTrace *st_Trace, VOID *pt_User);

	VOID	f_AddInput(const CScenario *st_Scenario, INT32 nObject);
	VOID	f_AddInit(const ST_StepTrace *st_Trace);
	VOID	f_AddStep(const ST_StepTrace *st_Trace);
	VOID	f_AddStepDetail(const ST_StepTrace *st_Trace, INT32 nManeuver);
	VOID	f_AddSummary(const ST_StepTrace *st_Trace);
	INT32	f_FindManeuver(INT32 nObject, FLOAT64 simTime) const;
	VOID	f_Show(const CString &st_Text) const;

	HANDLE			consoleOut;							// 콘솔 화면 버퍼. 닫혀 있으면 nullptr
	INT32			isVtOutput;							// 1 이면 콘솔이 VT 명령을 처리 (화면 버퍼 밖의 지난 글도 지울 수 있음)
	ST_SimConfig	st_Config;							// 기록 중인 계산의 설정
	INT32			nLogObject;							// 기록할 객체. -1 = 전체
	INT32			nStepNum;
	INT32			nStepPerSecond;						// 요약 한 줄 간격 [스텝]
	INT32			nLastManeuver[UI_OBJECT_NUM];		// 객체마다 지난 스텝에 걸린 기동. -1 = 없음
	INT32			isTableOpen[UI_OBJECT_NUM];			// 1 이면 1 초 요약의 열 제목을 이미 씀
	CString			st_ObjectText[UI_OBJECT_NUM];		// 객체마다 쌓는 글
};
