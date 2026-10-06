//
// @file	PlotSimTest.c
// @brief	PlotSimCore 검증. 과제 실습 항목(난수, 표준편차, 좌표 변환, 플롯, 클러터)을 기대값과 비교해 PASS / FAIL 출력.
//			조건: 대공 표적 1개, SNR 20 (선형), 60 s / 0.1 s, 스캔당 클러터 5개, seed 12345
// @author	hwan
// @date	2026.10.06.
//
// windows.h 는 PlotSim.h 보다 먼저 둠 (Define.h 의 INT32, VOID 매크로가 Windows 헤더 typedef 와 겹침)
#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#ifdef _MSC_VER
		#pragma warning(push)
		#pragma warning(disable : 5105)		// 10.0.20348 이전 SDK 의 winbase.h 가 /std:c17 에서 내는 경고
	#endif
	#include <windows.h>
	#ifdef _MSC_VER
		#pragma warning(pop)
	#endif
#endif

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "PlotSim.h"

#define TEST_ITEM_NUM		6
#define TEST_CHI2_BIN		10
#define TEST_CHI2_LIMIT		21.67					// 자유도 9, 유의수준 1 %

// 합, 제곱합으로 평균 / 표준편차
typedef struct
{
	FLOAT64		sum;
	FLOAT64		sumSq;
	INT32		nNum;
} ST_TestStat;

static ST_PcsState	s_State;					// 크기가 커서 정적 변수로 둠
static ST_PcsScan	s_Scan;
static INT32		s_PassNum = 0;

static VOID f_Test_Add(ST_TestStat *st_Stat, FLOAT64 value)
{
	st_Stat->sum	= st_Stat->sum + value;
	st_Stat->sumSq	= st_Stat->sumSq + (value * value);
	st_Stat->nNum++;
}

static FLOAT64 f_Test_Mean(const ST_TestStat *st_Stat)
{
	return (st_Stat->nNum > 0) ? (st_Stat->sum / (FLOAT64)st_Stat->nNum) : 0.0;
}

static FLOAT64 f_Test_Std(const ST_TestStat *st_Stat)
{
	const FLOAT64 mean = f_Test_Mean(st_Stat);

	return (st_Stat->nNum > 0) ? sqrt(fmax(0.0, (st_Stat->sumSq / (FLOAT64)st_Stat->nNum) - (mean * mean))) : 0.0;
}

// 출력에서 -0.0000 처럼 보이지 않게
static FLOAT64 f_Test_Clean(FLOAT64 value)
{
	return (fabs(value) < 5.0e-5) ? 0.0 : value;
}

static const CHAR *f_Test_Judge(INT32 isPass)
{
	if (isPass != 0)
	{
		s_PassNum++;
		return "PASS";
	}

	return "FAIL";
}

// 두 LLA 사이 거리 [m]. 레이다 기준 NED 로 바꿔서 잼 (Part 3 f_Tgt_LlaToNed)
static FLOAT64 f_Test_LlaDistance(const STRUCT_Coord_Lla *st_A, const STRUCT_Coord_Lla *st_B, const ST_TargetState *st_Radar)
{
	const STRUCT_Coord_Rect st_NedA = f_Tgt_LlaToNed(st_A, &st_Radar->st_PosEcef, &st_Radar->st_Lla);
	const STRUCT_Coord_Rect st_NedB = f_Tgt_LlaToNed(st_B, &st_Radar->st_PosEcef, &st_Radar->st_Lla);

	return sqrt(((st_NedA.x - st_NedB.x) * (st_NedA.x - st_NedB.x)) + ((st_NedA.y - st_NedB.y) * (st_NedA.y - st_NedB.y))
		+ ((st_NedA.z - st_NedB.z) * (st_NedA.z - st_NedB.z)));
}

// 균등분포 카이제곱 (bin TEST_CHI2_BIN 개)
static FLOAT64 f_Test_Chi2(const INT32 *pt_Count, INT32 nTotal)
{
	const FLOAT64	expect = (FLOAT64)nTotal / (FLOAT64)TEST_CHI2_BIN;
	FLOAT64			chi2 = 0.0;
	INT32			nBin;

	for (nBin = 0; nBin < TEST_CHI2_BIN; nBin++)
	{
		chi2 = chi2 + ((((FLOAT64)pt_Count[nBin] - expect) * ((FLOAT64)pt_Count[nBin] - expect)) / expect);
	}

	return chi2;
}

static INT32 f_Test_BinIndex(FLOAT64 value, FLOAT64 minValue, FLOAT64 maxValue)
{
	INT32 nBin = (INT32)floor(((value - minValue) / (maxValue - minValue)) * (FLOAT64)TEST_CHI2_BIN);

	return (nBin < 0) ? 0 : ((nBin >= TEST_CHI2_BIN) ? (TEST_CHI2_BIN - 1) : nBin);
}

// [1] UNIRAN: 10,000개 PDF 가 1 근처인지
static VOID f_Test_Uniform(VOID)
{
	const FLOAT64	theoryStd = 1.0 / sqrt(12.0);
	ST_PcsHist		st_Hist;
	FLOAT64			minPdf = HUGE_VAL;
	FLOAT64			maxPdf = 0.0;
	INT32			isPass;
	INT32			nBin;

	f_Pcs_SetSeed(PCS_DEFAULT_SEED);
	isPass = (f_Pcs_TestUniform(&st_Hist, PCS_RAND_TEST_NUM, PCS_UNIFORM_BIN_NUM) == PCS_PASS) ? 1 : 0;

	printf("[1] UNIRAN 균등난수 %d개, bin %d개\n", PCS_RAND_TEST_NUM, PCS_UNIFORM_BIN_NUM);
	printf("    구간          개수    PDF\n");

	for (nBin = 0; nBin < st_Hist.nBin; nBin++)
	{
		printf("    %.1f ~ %.1f   %5d   %.3f\n", st_Hist.binMin + (st_Hist.binWidth * (FLOAT64)nBin),
			st_Hist.binMin + (st_Hist.binWidth * (FLOAT64)(nBin + 1)), st_Hist.nCount[nBin], st_Hist.pdf[nBin]);
		minPdf = fmin(minPdf, st_Hist.pdf[nBin]);
		maxPdf = fmax(maxPdf, st_Hist.pdf[nBin]);
	}

	isPass = isPass && (minPdf > 0.9) && (maxPdf < 1.1) && (fabs(st_Hist.mean - 0.5) < 0.01) && (fabs(st_Hist.std - theoryStd) < 0.01)
		&& (st_Hist.nOutside == 0);

	printf("    평균 %.4f (이론 0.5000), 표준편차 %.4f (이론 %.4f)\n", st_Hist.mean, st_Hist.std, theoryStd);
	printf("    최솟값 %.6f, 최댓값 %.6f\n", st_Hist.minValue, st_Hist.maxValue);
	printf("    PDF %.3f ~ %.3f (기준 1 ± 0.1)  -> %s\n\n", minPdf, maxPdf, f_Test_Judge(isPass));
}

// [2] GAUSS(0, 1): 10,000개 PDF 가 표준정규 곡선과 맞는지
static VOID f_Test_Gauss(VOID)
{
	ST_PcsHist	st_Hist;
	FLOAT64		center;
	FLOAT64		phi;
	FLOAT64		maxDiff = 0.0;
	FLOAT64		maxAt = 0.0;
	INT32		isPass;
	INT32		nBin;

	f_Pcs_SetSeed(PCS_DEFAULT_SEED);
	isPass = (f_Pcs_TestGauss(&st_Hist, PCS_RAND_TEST_NUM, PCS_GAUSS_BIN_NUM, PCS_GAUSS_BIN_MIN, PCS_GAUSS_BIN_MAX) == PCS_PASS) ? 1 : 0;

	for (nBin = 0; nBin < st_Hist.nBin; nBin++)
	{
		center	= st_Hist.binMin + (st_Hist.binWidth * ((FLOAT64)nBin + 0.5));
		phi		= exp(-0.5 * center * center) / sqrt(2.0 * PI);

		if (fabs(st_Hist.pdf[nBin] - phi) > maxDiff)
		{
			maxDiff	= fabs(st_Hist.pdf[nBin] - phi);
			maxAt	= center;
		}
	}

	isPass = isPass && (fabs(st_Hist.mean) < 0.03) && (fabs(st_Hist.std - 1.0) < 0.03) && (maxDiff < 0.05);

	printf("[2] GAUSS(0, 1) 정규난수 %d개, bin %d개 [%.0f, %.0f]\n", PCS_RAND_TEST_NUM, PCS_GAUSS_BIN_NUM, PCS_GAUSS_BIN_MIN, PCS_GAUSS_BIN_MAX);
	printf("    평균 %+.4f (이론 0), 표준편차 %.4f (이론 1), 구간 밖 %d개\n", st_Hist.mean, st_Hist.std, st_Hist.nOutside);
	printf("    표준정규 PDF 와 차이 최대 %.4f (x = %+.3f, 기준 0.05)  -> %s\n\n", maxDiff, maxAt, f_Test_Judge(isPass));
}

// [3] 측정 표준편차. 기대값은 사양서 4.3절
static VOID f_Test_Sigma(VOID)
{
	ST_PcsConfig	st_Config;
	ST_PcsSigma		st_Linear;
	ST_PcsSigma		st_Db;
	INT32			isPass;

	f_Pcs_DefaultConfig(&st_Config, PCS_PRESET_AIR);
	isPass = (f_Pcs_CalcSigma(&st_Linear, &st_Config.st_Radar) == PCS_PASS) ? 1 : 0;
	st_Config.st_Radar.enSnrUnit = PCS_SNR_DB;
	isPass = isPass && (f_Pcs_CalcSigma(&st_Db, &st_Config.st_Radar) == PCS_PASS);

	isPass = isPass && (fabs(st_Linear.rng - 4.740) < 0.0005) && (fabs(st_Linear.azi - 0.4781) < 0.00005) && (fabs(st_Linear.ele - 0.4939) < 0.00005)
		&& (fabs(st_Db.rng - 2.120) < 0.0005) && (fabs(st_Db.azi - 0.2138) < 0.00005) && (fabs(st_Db.ele - 0.2209) < 0.00005);

	printf("[3] 측정 표준편차 (c %.0f m/s, B %.0f MHz, 빔폭 %.2f / %.2f deg, K_M %.1f)\n", PCS_LIGHT_SPEED, PCS_BANDWIDTH / 1.0e6,
		PCS_BEAM_AZI, PCS_BEAM_ELE, PCS_KM_AZI);
	printf("    SNR 20 (선형)    p_rng %.4f m   p_azi %.5f deg   p_ele %.5f deg\n", st_Linear.rng, st_Linear.azi, st_Linear.ele);
	printf("    SNR 20 dB (100)  p_rng %.4f m   p_azi %.5f deg   p_ele %.5f deg\n", st_Db.rng, st_Db.azi, st_Db.ele);
	printf("    기대값 (사양서 4.3절)  4.740 / 0.4781 / 0.4939,  2.120 / 0.2138 / 0.2209  -> %s\n\n", f_Test_Judge(isPass));
}

// [4] ~ [6] 600 스캔 실행 결과로 좌표 변환, 플롯, 클러터 확인
static VOID f_Test_Scan(VOID)
{
	static const CHAR	*s_Name[3] = { "거리 [m]    ", "방위각 [deg]", "고각 [deg]  " };
	ST_PcsConfig		st_Config;
	const ST_TargetState	*st_Radar = &s_State.st_Sim.st_Sample.st_Platform;
	STRUCT_Coord_Lla	st_Back;
	ST_TestStat			st_PlotErr[3];
	ST_TestStat			st_Clutter[3];
	ST_TestStat			st_CountStat;
	ST_PcsSph			st_First[2];
	const ST_PcsPoint	*st_True;
	const ST_PcsPoint	*st_Plot;
	const ST_PcsPoint	*st_Point;
	FLOAT64				sigma[3];
	FLOAT64				diff[3];
	FLOAT64				minValue[3];
	FLOAT64				maxValue[3];
	FLOAT64				lineRange;
	FLOAT64				maxBackErr = 0.0;
	FLOAT64				maxRangeErr = 0.0;
	FLOAT64				lastTime = 0.0;
	FLOAT64				mean;
	FLOAT64				std;
	FLOAT64				theoryStd;
	FLOAT64				chi2;
	INT32				nCount[3][TEST_CHI2_BIN];
	INT32				nScan = 0;
	INT32				nOutside = 0;
	INT32				nBadCount = 0;
	INT32				nMinCount = PCS_MAX_CLUTTER;
	INT32				nMaxCount = 0;
	INT32				nClutter;
	INT32				nPart;
	INT32				isPass;

	(VOID)memset(st_PlotErr, 0, sizeof(st_PlotErr));
	(VOID)memset(st_Clutter, 0, sizeof(st_Clutter));
	(VOID)memset(&st_CountStat, 0, sizeof(st_CountStat));
	(VOID)memset(st_First, 0, sizeof(st_First));
	(VOID)memset(nCount, 0, sizeof(nCount));

	f_Pcs_DefaultConfig(&st_Config, PCS_PRESET_AIR);
	isPass = (f_Pcs_InitSim(&s_State, &st_Config) == PCS_PASS) ? 1 : 0;

	sigma[0]	= s_State.st_Sigma.rng;
	sigma[1]	= s_State.st_Sigma.azi;
	sigma[2]	= s_State.st_Sigma.ele;
	minValue[0]	= st_Config.st_Clutter.rngMin;
	maxValue[0]	= st_Config.st_Clutter.rngMax;
	minValue[1]	= st_Config.st_Clutter.aziMin;
	maxValue[1]	= st_Config.st_Clutter.aziMax;
	minValue[2]	= st_Config.st_Clutter.eleMin;
	maxValue[2]	= st_Config.st_Clutter.eleMax;

	printf("[4] 좌표 변환 (대공 표적, 레이다 기준)\n");

	while ((isPass != 0) && (f_Pcs_StepScan(&s_State, &s_Scan) == PCS_PASS))
	{
		st_True	= &s_Scan.st_True[0];
		st_Plot	= &s_Scan.st_Plot[0];

		// 거리는 레이다-표적 직선거리와 같아야 하고, 구좌표 -> LLA 로 되돌리면 원래 위치가 나와야 함 (플랫폼은 정지)
		lineRange	= f_Test_LlaDistance(&st_True->st_Lla, &s_Scan.st_RadarLla, st_Radar);
		maxRangeErr	= fmax(maxRangeErr, fabs(lineRange - st_True->st_Sph.rng));
		st_Back		= f_Pcs_SphToLla(&st_True->st_Sph, st_Radar);
		maxBackErr	= fmax(maxBackErr, f_Test_LlaDistance(&st_Back, &st_True->st_Lla, st_Radar));

		if (nScan == 0)
		{
			st_First[0] = st_True->st_Sph;
		}

		st_First[1]	= st_True->st_Sph;
		lastTime	= s_Scan.scanTime;

		diff[0] = st_Plot->st_Sph.rng - st_True->st_Sph.rng;
		diff[1] = st_Plot->st_Sph.azi - st_True->st_Sph.azi;
		diff[2] = st_Plot->st_Sph.ele - st_True->st_Sph.ele;

		for (nPart = 0; nPart < 3; nPart++)
		{
			f_Test_Add(&st_PlotErr[nPart], diff[nPart]);
		}

		// 클러터
		nBadCount = nBadCount + ((s_Scan.nClutterNum != st_Config.st_Clutter.nClutterNum) ? 1 : 0);
		nMinCount = (s_Scan.nClutterNum < nMinCount) ? s_Scan.nClutterNum : nMinCount;
		nMaxCount = (s_Scan.nClutterNum > nMaxCount) ? s_Scan.nClutterNum : nMaxCount;

		for (nClutter = 0; nClutter < s_Scan.nClutterNum; nClutter++)
		{
			st_Point = &s_Scan.st_Clutter[nClutter];
			diff[0] = st_Point->st_Sph.rng;
			diff[1] = st_Point->st_Sph.azi;
			diff[2] = st_Point->st_Sph.ele;

			for (nPart = 0; nPart < 3; nPart++)
			{
				f_Test_Add(&st_Clutter[nPart], diff[nPart]);
				nOutside = nOutside + (((diff[nPart] < minValue[nPart]) || (diff[nPart] > maxValue[nPart])) ? 1 : 0);
				nCount[nPart][f_Test_BinIndex(diff[nPart], minValue[nPart], maxValue[nPart])]++;
			}
		}

		nScan++;
	}

	// [4]
	printf("                  거리 [m]  방위각 [deg]  고각 [deg]\n");
	printf("    t =  0.0 s  %10.2f  %12.4f  %10.4f\n", st_First[0].rng, f_Test_Clean(st_First[0].azi), st_First[0].ele);
	printf("    t = %4.1f s  %10.2f  %12.4f  %10.4f\n", lastTime, st_First[1].rng, f_Test_Clean(st_First[1].azi), st_First[1].ele);
	printf("    직선거리와 차이 최대 %.2e m, 왕복 오차 최대 %.2e m (기준 1 mm)  -> %s\n\n", maxRangeErr, maxBackErr,
		f_Test_Judge((nScan == s_State.nScanNum) && (maxRangeErr < 1.0e-3) && (maxBackErr < 1.0e-3)));

	// [5]
	printf("[5] 플롯 오차 z - x (대공 표적 %d 스캔)\n", nScan);
	printf("                      평균    표준편차           p  표준편차/p\n");
	isPass = (nScan > 1) ? 1 : 0;

	for (nPart = 0; nPart < 3; nPart++)
	{
		mean	= f_Test_Mean(&st_PlotErr[nPart]);
		std		= f_Test_Std(&st_PlotErr[nPart]);
		printf("    %s  %+10.4f  %10.4f  %10.4f  %10.3f\n", s_Name[nPart], mean, std, sigma[nPart], std / sigma[nPart]);
		isPass = isPass && (fabs(mean) < ((4.0 * sigma[nPart]) / sqrt((FLOAT64)nScan))) && (fabs((std / sigma[nPart]) - 1.0) < 0.1);
	}

	printf("    기준: |평균| < 4p / sqrt(N), 표준편차 / p = 1 ± 0.1  -> %s\n\n", f_Test_Judge(isPass));

	// [6]
	printf("[6] 클러터 (%d 스캔, 총 %d개)\n", nScan, st_Clutter[0].nNum);
	printf("                      평균      (이론)    표준편차      (이론)  카이제곱\n");
	isPass = (nBadCount == 0) && (nOutside == 0) && (st_Clutter[0].nNum > 0);

	for (nPart = 0; nPart < 3; nPart++)
	{
		mean		= f_Test_Mean(&st_Clutter[nPart]);
		std			= f_Test_Std(&st_Clutter[nPart]);
		theoryStd	= (maxValue[nPart] - minValue[nPart]) / sqrt(12.0);
		chi2		= f_Test_Chi2(nCount[nPart], st_Clutter[nPart].nNum);
		printf("    %s  %10.2f  %10.2f  %10.2f  %10.2f  %8.2f\n", s_Name[nPart], mean, 0.5 * (minValue[nPart] + maxValue[nPart]), std, theoryStd,
			chi2);
		isPass = isPass && (fabs(mean - (0.5 * (minValue[nPart] + maxValue[nPart]))) < ((4.0 * theoryStd) / sqrt((FLOAT64)st_Clutter[nPart].nNum)))
			&& (fabs((std / theoryStd) - 1.0) < 0.03) && (chi2 < TEST_CHI2_LIMIT);
	}

	printf("    FOV 밖 %d개, %d개가 아닌 스캔 %d개, 카이제곱 기준 %.2f (자유도 %d, 1 %%)\n", nOutside, st_Config.st_Clutter.nClutterNum, nBadCount,
		TEST_CHI2_LIMIT, TEST_CHI2_BIN - 1);

	// 개수 RANDOM 모드: 스캔마다 0 ~ N 개 중 하나. 평균 N / 2
	st_Config.st_Clutter.enMode = PCS_CLUTTER_RANDOM;
	(VOID)memset(&st_CountStat, 0, sizeof(st_CountStat));
	nMinCount = PCS_MAX_CLUTTER;
	nMaxCount = 0;

	if (f_Pcs_InitSim(&s_State, &st_Config) == PCS_PASS)
	{
		while (f_Pcs_StepScan(&s_State, &s_Scan) == PCS_PASS)
		{
			f_Test_Add(&st_CountStat, (FLOAT64)s_Scan.nClutterNum);
			nMinCount = (s_Scan.nClutterNum < nMinCount) ? s_Scan.nClutterNum : nMinCount;
			nMaxCount = (s_Scan.nClutterNum > nMaxCount) ? s_Scan.nClutterNum : nMaxCount;
		}
	}

	mean		= 0.5 * (FLOAT64)st_Config.st_Clutter.nClutterNum;
	theoryStd	= sqrt(((FLOAT64)((st_Config.st_Clutter.nClutterNum + 1) * (st_Config.st_Clutter.nClutterNum + 1)) - 1.0) / 12.0);
	isPass		= isPass && (st_CountStat.nNum == nScan) && (nMinCount >= 0) && (nMaxCount <= st_Config.st_Clutter.nClutterNum)
		&& (fabs(f_Test_Mean(&st_CountStat) - mean) < ((4.0 * theoryStd) / sqrt((FLOAT64)st_CountStat.nNum)));

	printf("    RANDOM 모드: 스캔당 %d ~ %d개, 평균 %.2f개 (이론 %.1f)  -> %s\n\n", nMinCount, nMaxCount, f_Test_Mean(&st_CountStat), mean,
		f_Test_Judge(isPass));
}

int main(VOID)
{
#ifdef _WIN32
	(VOID)SetConsoleOutputCP(CP_UTF8);
#endif

	printf("PlotSim 검증 (seed %d)\n\n", PCS_DEFAULT_SEED);

	f_Test_Uniform();
	f_Test_Gauss();
	f_Test_Sigma();
	f_Test_Scan();

	printf("결과: %d / %d PASS\n", s_PassNum, TEST_ITEM_NUM);

	return (s_PassNum == TEST_ITEM_NUM) ? 0 : 1;
}
