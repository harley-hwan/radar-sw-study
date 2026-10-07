//
// @file	PlotSim.c
// @brief	난수 검증, 측정 표준편차, 플롯 / 클러터 모의.
//			난수는 참고 소스 NoiseGeneration 의 UNIRAN, GAUSS, 좌표 변환은 참고 소스 CoordinateTransform 을 그대로 씀.
// @author	hwan
// @date	2026.10.07.
//
#include <math.h>
#include <string.h>

#include "PlotSim.h"
#include "NoiseGeneration.h"

// 히스토그램. isGauss 가 0 이면 UNIRAN, 아니면 GAUSS(0, 1) 를 PCS_RAND_NUM 개 뽑음.
static VOID f_Pcs_FillHist(ST_PcsHist *st_Hist, INT32 nBin, FLOAT64 binMin, FLOAT64 binMax, INT32 isGauss)
{
	INT32	nCount[PCS_GAUSS_BIN] = { 0 };
	FLOAT64	sum = 0.0;
	FLOAT64	sumSq = 0.0;
	FLOAT64	value;
	INT32	nBinIndex;
	INT32	nIndex;

	(VOID)memset(st_Hist, 0, sizeof(*st_Hist));
	st_Hist->nBin		= nBin;
	st_Hist->binMin		= binMin;
	st_Hist->binWidth	= (binMax - binMin) / (FLOAT64)nBin;

	for (nIndex = 0; nIndex < PCS_RAND_NUM; nIndex++)
	{
		value	= (isGauss != 0) ? GAUSS(0.0, 1.0) : UNIRAN();
		sum		= sum + value;
		sumSq	= sumSq + (value * value);

		// 구간 밖 값은 세지 않음 (GAUSS 의 |x| > 4)
		nBinIndex = (INT32)floor((value - binMin) / st_Hist->binWidth);

		if ((nBinIndex >= 0) && (nBinIndex < nBin))
		{
			nCount[nBinIndex]++;
		}
	}

	// PDF = 개수 / (N * bin 폭)
	for (nIndex = 0; nIndex < nBin; nIndex++)
	{
		st_Hist->pdf[nIndex] = (FLOAT64)nCount[nIndex] / ((FLOAT64)PCS_RAND_NUM * st_Hist->binWidth);
	}

	st_Hist->mean	= sum / (FLOAT64)PCS_RAND_NUM;
	st_Hist->std	= sqrt((sumSq / (FLOAT64)PCS_RAND_NUM) - (st_Hist->mean * st_Hist->mean));
}

// 표적 ECEF -> 레이다 안테나 구좌표 (참값). ECEF -> NED -> 동체 -> 안테나 -> 구좌표
static STRUCT_Coord_Sph f_Pcs_EcefToSph(const STRUCT_Coord_Rect *st_Ecef, const ST_TargetState *st_Radar)
{
	const STRUCT_Coord_Attitude	*st_Att = &st_Radar->st_Att;
	STRUCT_Coord_Rect			st_Ned;
	STRUCT_Coord_Rect			st_Body;
	STRUCT_Coord_Rect			st_Ant;

	st_Ned	= f_Trans_Ecef_To_Ned(st_Ecef->x, st_Ecef->y, st_Ecef->z, st_Radar->st_PosEcef.x, st_Radar->st_PosEcef.y, st_Radar->st_PosEcef.z,
		st_Radar->st_Lla.Lat, st_Radar->st_Lla.Lon);
	st_Body	= f_Trans_Ned_To_Body(st_Ned.x, st_Ned.y, st_Ned.z, st_Att->Roll, st_Att->Yaw, st_Att->Pitch);
	st_Ant	= f_Trans_Body_To_Ant(st_Body.x, st_Body.y, st_Body.z);

	return f_Trans_Ant_XYZ_To_Sph(st_Ant.x, st_Ant.y, st_Ant.z);
}

// 레이다 안테나 구좌표 -> LLA (전시용). 구좌표 -> 안테나 -> 동체 -> NED -> ECEF -> LLA
static STRUCT_Coord_Lla f_Pcs_SphToLla(const STRUCT_Coord_Sph *st_Sph, const ST_TargetState *st_Radar)
{
	const STRUCT_Coord_Attitude	*st_Att = &st_Radar->st_Att;
	STRUCT_Coord_Rect			st_Ant;
	STRUCT_Coord_Rect			st_Body;
	STRUCT_Coord_Rect			st_Ned;
	STRUCT_Coord_Rect			st_Ecef;

	st_Ant	= f_Trans_Ant_Sph_To_XYZ(st_Sph->r, st_Sph->az, st_Sph->el);
	st_Body	= f_Trans_Ant_To_Body(st_Ant.x, st_Ant.y, st_Ant.z);
	st_Ned	= f_Trans_Body_To_Ned(st_Body.x, st_Body.y, st_Body.z, st_Att->Roll, st_Att->Yaw, st_Att->Pitch);
	st_Ecef	= f_Trans_Ned_To_Ecef(st_Ned.x, st_Ned.y, st_Ned.z, st_Radar->st_PosEcef.x, st_Radar->st_PosEcef.y, st_Radar->st_PosEcef.z,
		st_Radar->st_Lla.Lat, st_Radar->st_Lla.Lon);

	return f_Trans_Ecef_To_Lla(st_Ecef.x, st_Ecef.y, st_Ecef.z);
}

//
// @brief	UNIRAN, GAUSS(0, 1) 검증용 히스토그램. 검증마다 seed 를 처음 값으로 맞춤
// @param	st_Uniform	UNIRAN 결과 ([0, 1], bin 10개)
// @param	st_Gauss	GAUSS 결과 ([-4, 4], bin 32개)
// @return	없음
// @author	hwan
//
VOID f_Pcs_TestRand(ST_PcsHist *st_Uniform, ST_PcsHist *st_Gauss)
{
	changeSEED(PCS_SEED);
	f_Pcs_FillHist(st_Uniform, PCS_UNIFORM_BIN, 0.0, 1.0, 0);

	changeSEED(PCS_SEED);
	f_Pcs_FillHist(st_Gauss, PCS_GAUSS_BIN, PCS_GAUSS_MIN, PCS_GAUSS_MAX, 1);
}

//
// @brief	거리 / 방위각 / 고각 측정 표준편차 p
//			p_rng = c / (2 * B * sqrt(2 * SNR)), p_azi = 빔폭 / (K_M * sqrt(2 * SNR)), p_ele 도 같은 식
//			과제 원문은 square(2 * SNR) 이지만 제곱근이 맞음 (제곱이면 p_rng 가 0.019 m 로 잡음이 거의 없어짐)
// @param	snr		선형 SNR (0 보다 커야 함)
// @return	r = p_rng [m], az = p_azi [rad], el = p_ele [rad]
// @author	hwan
//
STRUCT_Coord_Sph f_Pcs_CalcSigma(FLOAT64 snr)
{
	const FLOAT64		root = sqrt(2.0 * snr);
	STRUCT_Coord_Sph	st_Sigma;

	st_Sigma.r	= LIGHT_SPEED / (2.0 * PCS_BANDWIDTH * root);
	st_Sigma.az	= f_Deg_To_Rad(PCS_BEAM_AZI) / (PCS_KM * root);
	st_Sigma.el	= f_Deg_To_Rad(PCS_BEAM_ELE) / (PCS_KM * root);

	return st_Sigma;
}

//
// @brief	600 스캔 모의. 스캔마다 참값 -> 플롯 -> 클러터 순서로 만들고 표적을 0.1 s 진행함
//			난수 순서: 플롯은 거리, 방위각, 고각. 클러터는 한 개씩 거리, 방위각, 고각
// @param	st_Scan		결과 (PCS_SCAN_NUM 개)
// @param	snr			선형 SNR (0 보다 커야 함)
// @param	nClutterNum	스캔당 클러터 개수 (0 ~ PCS_MAX_CLUTTER, 벗어나면 끝값으로 맞춤)
// @return	없음
// @author	hwan
//
VOID f_Pcs_Run(ST_PcsScan *st_Scan, FLOAT64 snr, INT32 nClutterNum)
{
	const STRUCT_Coord_Sph	st_Sigma = f_Pcs_CalcSigma(snr);
	ST_SimConfig			st_Config;
	ST_SimState				st_Sim;
	const ST_TargetState	*st_Radar = &st_Sim.st_Sample.st_Platform;
	const ST_TargetState	*st_Target = &st_Sim.st_Sample.st_Target[0];
	ST_PcsScan				*st_Out;
	STRUCT_Coord_Sph		*st_Clutter;
	INT32					nScan;
	INT32					nClutter;

	// 표적 궤적 (Part 3): 플랫폼은 고도 0 m 에 정지, 표적은 이전 차수 대공 표적 하나
	(VOID)memset(&st_Config, 0, sizeof(st_Config));
	st_Config.durationTime					= PCS_SCAN_NUM * PCS_SCAN_TIME;
	st_Config.stepTime						= PCS_SCAN_TIME;
	st_Config.st_Platform.st_InitLla.Lat	= f_Deg_To_Rad(PCS_RADAR_LAT);
	st_Config.st_Platform.st_InitLla.Lon	= f_Deg_To_Rad(PCS_RADAR_LON);
	st_Config.nTargetNum					= 1;
	st_Config.st_Target[0].st_InitLla.Lat	= f_Deg_To_Rad(32.12);
	st_Config.st_Target[0].st_InitLla.Lon	= f_Deg_To_Rad(126.0);
	st_Config.st_Target[0].st_InitLla.Alt	= 300.0;
	st_Config.st_Target[0].st_InitAtt.Yaw	= f_Deg_To_Rad(180.0);
	st_Config.st_Target[0].headingSpeed		= 200.0;
	f_Tgt_InitSim(&st_Sim, &st_Config);

	nClutterNum = (nClutterNum < 0) ? 0 : ((nClutterNum > PCS_MAX_CLUTTER) ? PCS_MAX_CLUTTER : nClutterNum);
	changeSEED(PCS_SEED);

	for (nScan = 0; nScan < PCS_SCAN_NUM; nScan++)
	{
		st_Out			= &st_Scan[nScan];
		st_Out->time	= st_Sim.st_Sample.simTime;

		// 참값 x: 표적 위치를 레이다 안테나 구좌표로
		st_Out->st_True		= f_Pcs_EcefToSph(&st_Target->st_PosEcef, st_Radar);
		st_Out->st_TrueLla	= st_Target->st_Lla;

		// 플롯 z = x + v, v ~ N(0, p^2). 성분마다 GAUSS 를 따로 부름
		st_Out->st_Plot.r	= st_Out->st_True.r + GAUSS(0.0, st_Sigma.r);
		st_Out->st_Plot.az	= st_Out->st_True.az + GAUSS(0.0, st_Sigma.az);
		st_Out->st_Plot.el	= st_Out->st_True.el + GAUSS(0.0, st_Sigma.el);
		st_Out->st_PlotLla	= f_Pcs_SphToLla(&st_Out->st_Plot, st_Radar);

		// 클러터: FOV 안에서 성분마다 c = min + (max - min) * UNIRAN(). 표적과 상관없이 매 스캔 새로 뽑음
		st_Out->nClutterNum = nClutterNum;

		for (nClutter = 0; nClutter < nClutterNum; nClutter++)
		{
			st_Clutter		= &st_Out->st_Clutter[nClutter];
			st_Clutter->r	= PCS_CLT_RNG_MIN + ((PCS_CLT_RNG_MAX - PCS_CLT_RNG_MIN) * UNIRAN());
			st_Clutter->az	= f_Deg_To_Rad(PCS_CLT_AZI_MIN + ((PCS_CLT_AZI_MAX - PCS_CLT_AZI_MIN) * UNIRAN()));
			st_Clutter->el	= f_Deg_To_Rad(PCS_CLT_ELE_MIN + ((PCS_CLT_ELE_MAX - PCS_CLT_ELE_MIN) * UNIRAN()));
			st_Out->st_ClutterLla[nClutter] = f_Pcs_SphToLla(st_Clutter, st_Radar);
		}

		f_Tgt_StepSim(&st_Sim);
	}
}
