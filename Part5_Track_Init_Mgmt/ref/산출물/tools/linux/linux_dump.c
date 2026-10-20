// Linux harness: dump_fig_data.c 와 같은 시나리오. UDP 쓰레드 대신 BuildMsg -> ProcessMsg 를 직접 호출.
#include "PlotLink.c"
#include <stdio.h>

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
static void addMnv(ST_TargetInit *t, double g, double t0, double t1)
{
	ST_TargetManeuver *m = &t->st_Maneuver[t->nManeuverNum++];
	m->enTurnType = TGT_TURN_YAW; m->gravityValue = g; m->startTime = t0; m->endTime = t1;
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

// f_Lnk_Start 의 준비 부분 (쓰레드, 소켓 없이)
static int linkPrepare(const ST_PcsScan *st_Scan, const ST_SimSample *st_Sample, int nScanNum, const STRUCT_Coord_Sph *st_Sigma)
{
	int nScan;
	memset(&s_Link, 0, sizeof(s_Link));
	s_Link.isLockReady = 1;
	s_Link.st_Status.nScanNum = nScanNum;
	s_Link.nLastScan = -1;
	s_Link.isPrevPlatform = 0;
	s_Link.st_Sigma = *st_Sigma;
	f_Trk_Init(&s_Link.st_Table);
	s_Link.st_InScan = (ST_PcsScan *)malloc((size_t)nScanNum * sizeof(ST_PcsScan));
	s_Link.st_InRadar = (ST_TargetState *)malloc((size_t)nScanNum * sizeof(ST_TargetState));
	s_Link.nPlotSourceCap = nScanNum * IDD_MAX_PLOT;
	s_Link.pt_PlotSource = (INT32 *)malloc((size_t)s_Link.nPlotSourceCap * sizeof(INT32));
	s_Link.st_ResScan = (ST_LnkScanRec *)calloc((size_t)nScanNum, sizeof(ST_LnkScanRec));
	s_Link.nResTrackCap = nScanNum * 8;
	s_Link.st_ResTrack = (ST_TrkView *)malloc((size_t)s_Link.nResTrackCap * sizeof(ST_TrkView));
	s_Link.st_ResPlot = (ST_LnkPlot *)malloc((size_t)nScanNum * IDD_MAX_PLOT * sizeof(ST_LnkPlot));
	memcpy(s_Link.st_InScan, st_Scan, (size_t)nScanNum * sizeof(ST_PcsScan));
	for (nScan = 0; nScan < nScanNum; nScan++) s_Link.st_InRadar[nScan] = st_Sample[nScan].st_Platform;
	return 1;
}

int main(int argc, char **argv)
{
	static ST_SimConfig cfg;
	static ST_SimState sim;
	ST_LnkScan sc;
	const char *dir = (argc > 1) ? argv[1] : ".";
	double snr = (argc > 2) ? atof(argv[2]) : 20.0;
	char path[512];
	FILE *f;
	int nStep, nScanNum, i, k, t, a, b;
	STRUCT_Coord_Sph sigma;
	UINT32 plotId = IDD_PLOT_ID_MIN;

	cfg.durationTime = 116.0; cfg.stepTime = 0.1;
	setObj(&cfg.st_Platform.st_InitLla, &cfg.st_Platform.st_InitAtt, &cfg.st_Platform.headingSpeed, 32.0, 126.0, 0, 0, 0);
	cfg.nTargetNum = 2;
	setObj(&cfg.st_Target[0].st_InitLla, &cfg.st_Target[0].st_InitAtt, &cfg.st_Target[0].headingSpeed, 32.125, 126.03, 0, 30, 270);
	setObj(&cfg.st_Target[1].st_InitLla, &cfg.st_Target[1].st_InitAtt, &cfg.st_Target[1].headingSpeed, 32.12, 126.0, 2000, 200, 180);
	addMnv(&cfg.st_Target[0], -1.0, 28.4, 30.8);
	addMnv(&cfg.st_Target[0], 1.0, 41.4, 43.8);
	addMnv(&cfg.st_Target[0], 1.0, 72.2, 74.6);
	addMnv(&cfg.st_Target[0], -1.0, 85.2, 87.6);
	addMnv(&cfg.st_Target[1], -4.0, 6.0, 14.0);
	addMnv(&cfg.st_Target[1], 4.0, 24.1, 32.1);
	addMnv(&cfg.st_Target[1], 4.0, 35.2, 43.2);
	addMnv(&cfg.st_Target[1], -4.0, 67.5, 75.5);
	addMnv(&cfg.st_Target[1], -4.0, 78.5, 86.5);
	addMnv(&cfg.st_Target[1], 4.0, 102.6, 116.0);

	f_Tgt_InitSim(&sim, &cfg);
	g_Sample[0] = sim.st_Sample;
	for (nStep = 0; nStep < sim.nStepNum; nStep++) { f_Tgt_StepSim(&sim); g_Sample[nStep + 1] = sim.st_Sample; }
	nScanNum = sim.nStepNum;
	f_Pcs_Measure(g_Scan, g_Sample, nScanNum, snr, 5);
	sigma = f_Pcs_CalcSigma(snr);
	printf("scans %d sigma r %.3f az %.4f deg el %.4f deg\n", nScanNum, sigma.r, f_Rad_To_Deg(sigma.az), f_Rad_To_Deg(sigma.el));

	sprintf(path, "%s/truth.csv", dir); f = fopen(path, "w");
	fprintf(f, "scan,target,north,east,down,vn,ve,vd,range\n");
	for (i = 0; i <= nScanNum; i++)
		for (k = 0; k < cfg.nTargetNum; k++)
		{
			STRUCT_Coord_Rect p = toNed(&g_Sample[i].st_Target[k].st_PosEcef, i);
			STRUCT_Coord_Rect e2 = g_Sample[i].st_Target[k].st_PosEcef;
			STRUCT_Coord_Rect q;
			e2.x += g_Sample[i].st_Target[k].st_VelEcef.x; e2.y += g_Sample[i].st_Target[k].st_VelEcef.y; e2.z += g_Sample[i].st_Target[k].st_VelEcef.z;
			q = toNed(&e2, i);
			fprintf(f, "%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n", i, k + 1, p.x, p.y, p.z, q.x - p.x, q.y - p.y, q.z - p.z, sqrt(p.x * p.x + p.y * p.y + p.z * p.z));
		}
	fclose(f);

	sprintf(path, "%s/gate.csv", dir); f = fopen(path, "w");
	fprintf(f, "scan,target,range,speed,pass\n");
	for (i = 1; i < nScanNum; i++)
		for (k = 0; k < cfg.nTargetNum; k++)
		{
			ST_TrkPlot p0 = mkPlot(i - 1, &g_Scan[i - 1].st_Plot[k], &sigma);
			ST_TrkPlot p1 = mkPlot(i, &g_Scan[i].st_Plot[k], &sigma);
			double dx = p1.pos[0] - p0.pos[0], dy = p1.pos[1] - p0.pos[1], dz = p1.pos[2] - p0.pos[2];
			double spd = sqrt(dx * dx + dy * dy + dz * dz) / (p1.time - p0.time);
			fprintf(f, "%d,%d,%.1f,%.2f,%d\n", i, k + 1, g_Scan[i].st_True[k].r, spd, spd < TRK_VMAX ? 1 : 0);
		}
	fclose(f);

	{
		const int ex = 0;
		int s0 = 0;
		ST_TrkPlot p0, p1;
		ST_Track trk;
		for (; s0 + 1 < nScanNum; s0++)
		{
			double dx, dy, dz;
			p0 = mkPlot(s0, &g_Scan[s0].st_Plot[ex], &sigma);
			p1 = mkPlot(s0 + 1, &g_Scan[s0 + 1].st_Plot[ex], &sigma);
			dx = p1.pos[0] - p0.pos[0]; dy = p1.pos[1] - p0.pos[1]; dz = p1.pos[2] - p0.pos[2];
			if (sqrt(dx * dx + dy * dy + dz * dz) / (p1.time - p0.time) < TRK_VMAX) break;
		}
		memset(&trk, 0, sizeof(trk));
		f_Trk_TwoPointInit(&trk, &p0, &p1);
		sprintf(path, "%s/p_example.csv", dir); f = fopen(path, "w");
		fprintf(f, "# target %d scan %d->%d, range %.1f\n", ex + 1, s0, s0 + 1, g_Scan[s0 + 1].st_True[ex].r);
		for (a = 0; a < 9; a++) { for (b = 0; b < 9; b++) fprintf(f, "%s%.6g", b ? "," : "", trk.mat_EstP.e[a][b]); fprintf(f, "\n"); }
		fprintf(f, "# X\n");
		for (a = 0; a < 9; a++) fprintf(f, "%s%.6g", a ? "," : "", trk.mat_EstX.e[a][0]);
		fprintf(f, "\n# R1\n");
		for (a = 0; a < 3; a++) fprintf(f, "%.6g,%.6g,%.6g\n", p1.cov[a][0], p1.cov[a][1], p1.cov[a][2]);
		fprintf(f, "# plot0 %.3f %.3f %.3f plot1 %.3f %.3f %.3f\n", p0.pos[0], p0.pos[1], p0.pos[2], p1.pos[0], p1.pos[1], p1.pos[2]);
		fclose(f);
	}

	// 송신 -> 수신 (직접 호출)
	linkPrepare(g_Scan, g_Sample, nScanNum, &sigma);
	for (i = 0; i < nScanNum; i++)
	{
		UINT32 sz = f_Lnk_BuildMsg(&s_Link.st_TxMsg, i, &plotId);
		memcpy(&s_Link.st_RxMsg, &s_Link.st_TxMsg, sz);
		f_Lnk_ProcessMsg(&s_Link.st_RxMsg);
	}
	printf("proc %d created %d deleted %d tablefull %d\n", s_Link.st_Status.nProcNum, s_Link.st_Status.nCreatedSum, s_Link.st_Status.nDeletedSum, s_Link.st_Status.nTableFullSum);

	sprintf(path, "%s/plots.csv", dir); f = fopen(path, "w");
	{
		FILE *g, *h;
		sprintf(path, "%s/tracks.csv", dir); g = fopen(path, "w");
		sprintf(path, "%s/scanlog.csv", dir); h = fopen(path, "w");
		fprintf(f, "scan,plot_id,source,north,east,down\n");
		fprintf(g, "scan,track_id,state,score,memory,flag,init_scan,plot0,plot1,src0,src1,north,east,down,vn,ve,vd,sn,se,sd,svn,sve,svd\n");
		fprintf(h, "scan,time,msgsize,nplot,ncand,npair,ngate,ncreated,nconfirmed,nmemory,ndeleted,ntrack\n");
		for (i = 0; i < s_Link.st_Status.nProcNum; i++)
		{
			f_Lnk_CopyScan(i, &sc, g_Track, g_Plot);
			fprintf(h, "%u,%.3f,%u,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", sc.ScanNum, sc.time, sc.MsgSize, sc.st_Log.nPlotNum, sc.st_Log.nCandidateNum, sc.st_Log.nPairNum,
				sc.st_Log.nGatePassNum, sc.st_Log.nCreatedNum, sc.st_Log.nConfirmedNum, sc.st_Log.nMemoryNum, sc.st_Log.nDeletedNum, sc.nTrackNum);
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
		fclose(g); fclose(h);
	}
	fclose(f);
	return 0;
}
