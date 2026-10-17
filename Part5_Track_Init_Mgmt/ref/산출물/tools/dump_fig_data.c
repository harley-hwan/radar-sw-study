// 발표 그림용 자료 뽑기: 기본 시나리오 + 표적 3개 -> 측정 모의 -> UDP 송수신 -> 트랙. CSV 로 씀.
// Core 소스 (TargetSimCore/src/*.c) 와 같이 컴파일. 사용: dump_fig_data.exe <outdir> [snr]
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "PlotLink.h"

#define NSTEP_MAX 2000

static ST_SimSample	g_Sample[NSTEP_MAX];
static ST_PcsScan	g_Scan[NSTEP_MAX];
static ST_TrkView	g_Track[TRK_MAX_TRACK];
static ST_LnkPlot	g_Plot[IDD_MAX_PLOT];

static void setObj(STRUCT_Coord_Lla *lla, STRUCT_Coord_Attitude *att, double *spd, double lat, double lon, double alt, double v, double yaw)
{
	lla->Lat = f_Deg_To_Rad(lat); lla->Lon = f_Deg_To_Rad(lon); lla->Alt = alt;
	att->Roll = 0; att->Pitch = 0; att->Yaw = f_Deg_To_Rad(yaw);
	*spd = v;
}

static STRUCT_Coord_Rect toNed(const STRUCT_Coord_Rect *e, int n)
{
	const ST_TargetState *r = &g_Sample[n].st_Platform;
	return f_Trans_Ecef_To_Ned(e->x, e->y, e->z, r->st_PosEcef.x, r->st_PosEcef.y, r->st_PosEcef.z, r->st_Lla.Lat, r->st_Lla.Lon);
}

static ST_TrkPlot mkPlot(int n, const STRUCT_Coord_Sph *z, const STRUCT_Coord_Sph *sig)
{
	const ST_TargetState *r = &g_Sample[n].st_Platform;
	ST_TrkPlot p;
	STRUCT_Coord_Rect a = f_Trans_Ant_Sph_To_XYZ(z->r, z->az, z->el);
	STRUCT_Coord_Rect b = f_Trans_Ant_To_Body(a.x, a.y, a.z);
	STRUCT_Coord_Rect c = f_Trans_Body_To_Ned(b.x, b.y, b.z, r->st_Att.Roll, r->st_Att.Yaw, r->st_Att.Pitch);
	memset(&p, 0, sizeof(p));
	p.time = g_Scan[n].time;
	p.pos[0] = c.x; p.pos[1] = c.y; p.pos[2] = c.z;
	f_Trk_MeasCov(p.cov, z, sig, &r->st_Att);
	return p;
}

int main(int argc, char **argv)
{
	static ST_SimConfig cfg;
	static ST_SimState sim;
	ST_LnkStatus st;
	ST_LnkScan sc;
	const char *dir = (argc > 1) ? argv[1] : ".";
	double snr = (argc > 2) ? atof(argv[2]) : 20.0;
	char path[512];
	FILE *f;
	int nStep, nScanNum, i, k, t, a, b;
	STRUCT_Coord_Sph sigma;

	cfg.durationTime = 60.0; cfg.stepTime = 0.1;
	setObj(&cfg.st_Platform.st_InitLla, &cfg.st_Platform.st_InitAtt, &cfg.st_Platform.headingSpeed, 32.0, 126.0, 0, 0, 0);
	cfg.nTargetNum = 5;
	setObj(&cfg.st_Target[0].st_InitLla, &cfg.st_Target[0].st_InitAtt, &cfg.st_Target[0].headingSpeed, 32.125, 126.03, 0, 30, 270);
	setObj(&cfg.st_Target[1].st_InitLla, &cfg.st_Target[1].st_InitAtt, &cfg.st_Target[1].headingSpeed, 32.12, 126.0, 300, 200, 180);
	setObj(&cfg.st_Target[2].st_InitLla, &cfg.st_Target[2].st_InitAtt, &cfg.st_Target[2].headingSpeed, 32.05, 126.08, 1500, 150, 0);
	setObj(&cfg.st_Target[3].st_InitLla, &cfg.st_Target[3].st_InitAtt, &cfg.st_Target[3].headingSpeed, 32.10, 125.93, 2000, 180, 110);
	setObj(&cfg.st_Target[4].st_InitLla, &cfg.st_Target[4].st_InitAtt, &cfg.st_Target[4].headingSpeed, 32.04, 125.96, 0, 20, 45);
	cfg.st_Target[2].nManeuverNum = 1;
	cfg.st_Target[2].st_Maneuver[0].enTurnType = TGT_TURN_YAW;
	cfg.st_Target[2].st_Maneuver[0].gravityValue = -1.5;
	cfg.st_Target[2].st_Maneuver[0].startTime = 10.0;
	cfg.st_Target[2].st_Maneuver[0].endTime = 40.0;

	f_Tgt_InitSim(&sim, &cfg);
	g_Sample[0] = sim.st_Sample;
	for (nStep = 0; nStep < sim.nStepNum; nStep++) { f_Tgt_StepSim(&sim); g_Sample[nStep + 1] = sim.st_Sample; }
	nScanNum = sim.nStepNum;
	f_Pcs_Measure(g_Scan, g_Sample, nScanNum, snr, 5);
	sigma = f_Pcs_CalcSigma(snr);

	// 참값 (VC-NED, 스캔마다), 속도
	sprintf(path, "%s/truth.csv", dir); f = fopen(path, "w");
	fprintf(f, "scan,target,north,east,down,vn,ve,vd,range\n");
	for (i = 0; i <= nScanNum; i++)
		for (k = 0; k < 5; k++)
		{
			STRUCT_Coord_Rect p = toNed(&g_Sample[i].st_Target[k].st_PosEcef, i);
			STRUCT_Coord_Rect e2 = g_Sample[i].st_Target[k].st_PosEcef;
			STRUCT_Coord_Rect q;
			e2.x += g_Sample[i].st_Target[k].st_VelEcef.x; e2.y += g_Sample[i].st_Target[k].st_VelEcef.y; e2.z += g_Sample[i].st_Target[k].st_VelEcef.z;
			q = toNed(&e2, i);
			fprintf(f, "%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n", i, k + 1, p.x, p.y, p.z, q.x - p.x, q.y - p.y, q.z - p.z, sqrt(p.x * p.x + p.y * p.y + p.z * p.z));
		}
	fclose(f);

	// 참 표적 쌍의 게이트 통과 (짝짓기 규칙과 무관한 순수 통과율)
	sprintf(path, "%s/gate.csv", dir); f = fopen(path, "w");
	fprintf(f, "scan,target,range,speed,pass\n");
	for (i = 1; i < nScanNum; i++)
		for (k = 0; k < 5; k++)
		{
			ST_TrkPlot p0 = mkPlot(i - 1, &g_Scan[i - 1].st_Plot[k], &sigma);
			ST_TrkPlot p1 = mkPlot(i, &g_Scan[i].st_Plot[k], &sigma);
			double dx = p1.pos[0] - p0.pos[0], dy = p1.pos[1] - p0.pos[1], dz = p1.pos[2] - p0.pos[2];
			double spd = sqrt(dx * dx + dy * dy + dz * dz) / (p1.time - p0.time);
			fprintf(f, "%d,%d,%.1f,%.2f,%d\n", i, k + 1, g_Scan[i].st_True[k].r, spd, spd < TRK_VMAX ? 1 : 0);
		}
	fclose(f);

	// 예시 P (표적 5, 스캔 100 부터 처음 게이트를 넘는 쌍) 와 R
	{
		int s0 = 100;
		ST_TrkPlot p0, p1;
		ST_Track trk;
		for (;;)
		{
			double dx, dy, dz;
			p0 = mkPlot(s0, &g_Scan[s0].st_Plot[4], &sigma);
			p1 = mkPlot(s0 + 1, &g_Scan[s0 + 1].st_Plot[4], &sigma);
			dx = p1.pos[0] - p0.pos[0]; dy = p1.pos[1] - p0.pos[1]; dz = p1.pos[2] - p0.pos[2];
			if (sqrt(dx * dx + dy * dy + dz * dz) / 0.1 < TRK_VMAX) break;
			s0++;
		}
		memset(&trk, 0, sizeof(trk));
		f_Trk_TwoPointInit(&trk, &p0, &p1);
		sprintf(path, "%s/p_example.csv", dir); f = fopen(path, "w");
		fprintf(f, "# target 5 scan %d->%d, range %.1f\n", s0, s0 + 1, g_Scan[s0 + 1].st_True[4].r);
		for (a = 0; a < 9; a++) { for (b = 0; b < 9; b++) fprintf(f, "%s%.6g", b ? "," : "", trk.mat_EstP.e[a][b]); fprintf(f, "\n"); }
		fprintf(f, "# X\n");
		for (a = 0; a < 9; a++) fprintf(f, "%s%.6g", a ? "," : "", trk.mat_EstX.e[a][0]);
		fprintf(f, "\n# R1\n");
		for (a = 0; a < 3; a++) fprintf(f, "%.6g,%.6g,%.6g\n", p1.cov[a][0], p1.cov[a][1], p1.cov[a][2]);
		fprintf(f, "# plot0 %.3f %.3f %.3f plot1 %.3f %.3f %.3f\n", p0.pos[0], p0.pos[1], p0.pos[2], p1.pos[0], p1.pos[1], p1.pos[2]);
		fclose(f);
	}

	// 송수신 후 받은 플롯, 트랙
	if (!f_Lnk_Start(g_Scan, g_Sample, nScanNum, &sigma, 0)) { printf("start failed\n"); return 1; }
	do { Sleep(20); f_Lnk_GetStatus(&st); } while (st.isRunning);
	printf("sent %d recv %d lost %d proc %d created %d deleted %d\n", st.nSentNum, st.nRecvNum, st.nLostNum, st.nProcNum, st.nCreatedSum, st.nDeletedSum);

	sprintf(path, "%s/plots.csv", dir); f = fopen(path, "w");
	{
		FILE *g;
		sprintf(path, "%s/tracks.csv", dir); g = fopen(path, "w");
		fprintf(f, "scan,plot_id,source,north,east,down\n");
		fprintf(g, "scan,track_id,state,score,memory,flag,init_scan,plot0,plot1,src0,src1,north,east,down,vn,ve,vd,sn,se,sd,svn,sve,svd\n");
		for (i = 0; i < st.nProcNum; i++)
		{
			f_Lnk_CopyScan(i, &sc, g_Track, g_Plot);
			for (k = 0; k < sc.nPlotNum; k++)
				fprintf(f, "%u,%u,%d,%.3f,%.3f,%.3f\n", sc.ScanNum, g_Plot[k].PlotId, f_Lnk_GetPlotSource(g_Plot[k].PlotId), g_Plot[k].pos[0], g_Plot[k].pos[1], g_Plot[k].pos[2]);
			for (t = 0; t < sc.nTrackNum; t++)
			{
				ST_TrkView *v = &g_Track[t];
				fprintf(g, "%u,%u,%d,%d,%d,%d,%d,%u,%u,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n", sc.ScanNum, v->TrackId, v->TrackState, v->TrackScore,
					v->MemoryScore, v->Assigned_Flag, v->nInitScan, v->PlotId[0], v->PlotId[1], f_Lnk_GetPlotSource(v->PlotId[0]), f_Lnk_GetPlotSource(v->PlotId[1]),
					v->pos[0], v->pos[1], v->pos[2], v->vel[0], v->vel[1], v->vel[2], v->posStd[0], v->posStd[1], v->posStd[2], v->velStd[0], v->velStd[1], v->velStd[2]);
			}
		}
		fclose(g);
	}
	fclose(f);
	f_Lnk_Stop();
	return 0;
}
