#pragma once

#include "Scenario.h"

// 계산 로그: 결과 표의 기본 계산 (중점법, 표적 자리 기준) 과정을 콘솔 창에 씀.
// 같은 설정으로 한 번 더 계산하면서 Core 가 넘기는 기록 (ST_StepTrace) 을 받아 계산 순서대로 한 줄씩 적음.
class CCalcLog
{
public:
	CCalcLog() noexcept;
	~CCalcLog();

	INT32	f_Open(VOID);
	VOID	f_Close(VOID);
	INT32	f_IsOpen(VOID) const;
	VOID	f_Write(const CScenario *st_Scenario, INT32 nObject, LPCTSTR pt_Title);

private:
	static VOID	f_OnTrace(const ST_StepTrace *st_Trace, VOID *pt_User);

	VOID	f_WriteObject(const CScenario *st_Scenario, INT32 nObject);
	VOID	f_PrintInit(const ST_StepTrace *st_Trace) const;
	VOID	f_PrintStep(const ST_StepTrace *st_Trace);
	VOID	f_PrintDetail(const ST_StepTrace *st_Trace, INT32 nManeuver) const;
	INT32	f_FindManeuver(FLOAT64 simTime) const;
	VOID	f_Print(LPCTSTR pt_Format, ...) const;

	INT32			isOpen;
	ST_SimConfig	st_Config;							// 기록 중인 계산의 설정
	INT32			nLogObject;							// 기록할 객체. 0 = 플랫폼, k = 표적 k
	INT32			nLastManeuver;						// 지난 스텝에 걸린 기동. -1 = 없음
	INT32			nStepNum;
};
