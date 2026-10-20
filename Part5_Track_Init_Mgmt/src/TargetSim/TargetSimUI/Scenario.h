#pragma once

#include <vector>

#include "TargetSim.h"

// 고정 시나리오 (과제 5 시뮬레이션 조건). 궤적, 기동, 측정 조건은 바꾸지 않으므로 화면 편집 없이 값으로 둠.
// 응용 시나리오 (원본 크기): 플랫폼 정지, 표적 1 대함 · 표적 2 대공의 지그재그 기동. 표적 2 고도만 2,000 m 로 올림.
// 116 s, 간격 0.1 s, SNR 20, 스캔당 클러터 5.
#define SCN_NAME				_T("응용 시나리오 (원본 크기, 대공 표적 고도 2,000 m)")
#define SCN_DURATION_TIME		116.0					// [s]
#define SCN_STEP_TIME			0.1						// [s] = 스캔 간격
#define SCN_TARGET_NUM			2
#define SCN_SNR					20.0					// 측정 모의 SNR (선형)
#define SCN_CLUTTER_NUM			5						// 스캔당 클러터 수

#define RES_CELL_SIZE			48						// 표 칸, CSV 칸 하나의 글자 버퍼 [B]

// 플랫폼 시작 위치 기준 국지 좌표 [m]. up 은 3D 그림용 (NED 의 -아래)
typedef struct
{
	FLOAT64		east;
	FLOAT64		north;
	FLOAT64		up;
} ST_PlotPoint;

VOID	f_Scn_BuildConfig(ST_SimConfig *st_Config);

// 궤적 계산 결과. 표본과 그림 좌표 보관. 측정 모의의 입력이자 그림의 참값 (True).
class CSimResult
{
public:
	VOID	f_Run(const ST_SimConfig *st_Config);

	INT32					f_GetSampleNum(VOID) const;
	INT32					f_GetObjectNum(VOID) const;
	const ST_SimSample *	f_GetSample(INT32 nStep) const;
	const ST_TargetState *	f_GetState(INT32 nStep, INT32 nObject) const;
	const ST_PlotPoint *	f_GetPoint(INT32 nStep, INT32 nObject) const;

private:
	VOID	f_ComputePoints(VOID);

	std::vector<ST_SimSample>	st_SampleBuf;
	std::vector<ST_PlotPoint>	st_PointBuf;					// 표본마다 객체 수만큼
	INT32						nObjectNum = 0;
};
