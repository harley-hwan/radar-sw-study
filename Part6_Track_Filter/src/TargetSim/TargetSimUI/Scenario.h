// 고정 시나리오 (과제 6: 대함 · 대공 표적을 각각) 와 궤적 계산 결과 (CSimResult), 그림 좌표 (위경도 · 고도)
#pragma once

#include <vector>

#include "TargetSim.h"
#include "TrackMgr.h"

// 과제 6 조건: 116 s, 간격 0.1 s, SNR 20, 클러터 없음 (표적 플롯만)
#define SCN_SHIP				0						// 대함 표적 시나리오
#define SCN_AIR					1						// 대공 표적 시나리오
#define SCN_NUM					2
#define SCN_DURATION_TIME		116.0					// [s]
#define SCN_STEP_TIME			0.1						// [s] = 스캔 간격
#define SCN_SNR					20.0					// 측정 모의 SNR (선형)
#define SCN_CLUTTER_NUM			0						// 클러터 생성 X

#define RES_CELL_SIZE			48						// 표 칸 하나의 글자 버퍼 [B]

// 그림 좌표 [m]. 경도 · 위도를 원점에서 선형으로 m 로 바꾼 값이라 눈금은 위경도 그대로 읽힘 (격자선이 곧음), up = 고도
typedef struct
{
	FLOAT64		east;							// (경도 - 원점 경도) x 경도 1 도 길이
	FLOAT64		north;							// (위도 - 원점 위도) x 위도 1 도 길이
	FLOAT64		up;								// 고도
} ST_PlotPoint;

// 위경도 <-> 그림 좌표. 원점 = 플랫폼 시작 위치
typedef struct
{
	FLOAT64		lat0;							// 원점 위도 [deg]
	FLOAT64		lon0;							// 원점 경도 [deg]
	FLOAT64		meterPerLat;					// 원점에서 위도 1 도 길이 [m]
	FLOAT64		meterPerLon;					// 원점에서 경도 1 도 길이 [m]
} ST_GeoMap;

VOID			f_Scn_BuildConfig(ST_SimConfig *st_Config, INT32 nScenario);
LPCTSTR			f_Scn_GetName(INT32 nScenario);
LPCTSTR			f_Scn_GetNote(INT32 nScenario);
VOID			f_Scn_GetTunedParam(ST_TrkParam *st_Param, INT32 nScenario);
VOID			f_Scn_InitMap(ST_GeoMap *st_Map, const STRUCT_Coord_Lla *st_Origin);
ST_PlotPoint	f_Scn_LlaToPoint(const ST_GeoMap *st_Map, const STRUCT_Coord_Lla *st_Lla);
FLOAT64			f_Scn_NorthToLat(const ST_GeoMap *st_Map, FLOAT64 north);
FLOAT64			f_Scn_EastToLon(const ST_GeoMap *st_Map, FLOAT64 east);
FLOAT64			f_Scn_LatToNorth(const ST_GeoMap *st_Map, FLOAT64 latDeg);
FLOAT64			f_Scn_LonToEast(const ST_GeoMap *st_Map, FLOAT64 lonDeg);

// 궤적 계산 결과. 표본과 그림 좌표 보관. 측정 모의의 입력이자 그림의 참값 (True)
class CSimResult
{
public:
	VOID	f_Run(const ST_SimConfig *st_NewConfig);

	INT32					f_GetSampleNum(VOID) const;
	INT32					f_GetObjectNum(VOID) const;
	const ST_SimSample *	f_GetSample(INT32 nStep) const;
	const ST_TargetState *	f_GetState(INT32 nStep, INT32 nObject) const;
	const ST_PlotPoint *	f_GetPoint(INT32 nStep, INT32 nObject) const;
	const ST_GeoMap *		f_GetMap(VOID) const;
	const ST_SimConfig *	f_GetConfig(VOID) const;

private:
	VOID	f_ComputePoints(VOID);

	std::vector<ST_SimSample>	st_SampleBuf;
	std::vector<ST_PlotPoint>	st_PointBuf;					// 표본마다 객체 수만큼
	INT32						nObjectNum = 0;
	ST_GeoMap					st_Map = {};
	ST_SimConfig				st_Config = {};					// 실행한 설정 (기동 구간 표시용)
};
