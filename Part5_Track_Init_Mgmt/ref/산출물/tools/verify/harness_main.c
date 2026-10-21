/* Validation harness (Linux): runs the real TargetSimCore C sources incl. PlotLink.c over loopback UDP and dumps CSVs. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include "PlotLink.h"
#include "PlotSim.h"
#include "TrackMgr.h"
#include "Idd.h"

#define SCN_DURATION_TIME 116.0
#define SCN_STEP_TIME 0.1
#define SCN_TARGET_NUM 2
#define SCN_SNR 20.0
#define SCN_CLUTTER_NUM 5

typedef struct { double lat, lon, alt, speed, roll, pitch, yaw; } OBJ;
static const OBJ s_Platform = { 32.0, 126.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
static const OBJ s_Target[2] = { { 32.125, 126.03, 0.0, 30.0, 0.0, 0.0, 270.0 }, { 32.12, 126.0, 2000.0, 200.0, 0.0, 0.0, 180.0 } };
typedef struct { int tgt; ST_TargetManeuver m; } MAN;
static const MAN s_Man[10] = {
	{ 0, { TGT_TURN_YAW, -1.0, 28.4, 30.8 } }, { 0, { TGT_TURN_YAW, 1.0, 41.4, 43.8 } }, { 0, { TGT_TURN_YAW, 1.0, 72.2, 74.6 } }, { 0, { TGT_TURN_YAW, -1.0, 85.2, 87.6 } },
	{ 1, { TGT_TURN_YAW, -4.0, 6.0, 14.0 } }, { 1, { TGT_TURN_YAW, 4.0, 24.1, 32.1 } }, { 1, { TGT_TURN_YAW, 4.0, 35.2, 43.2 } }, { 1, { TGT_TURN_YAW, -4.0, 67.5, 75.5 } },
	{ 1, { TGT_TURN_YAW, -4.0, 78.5, 86.5 } }, { 1, { TGT_TURN_YAW, 4.0, 102.6, 116.0 } } };

static void setobj(const OBJ *o, STRUCT_Coord_Lla *lla, STRUCT_Coord_Attitude *att, double *spd)
{
	lla->Lat = f_Deg_To_Rad(o->lat); lla->Lon = f_Deg_To_Rad(o->lon); lla->Alt = o->alt; *spd = o->speed;
	att->Roll = f_Deg_To_Rad(o->roll); att->Pitch = f_Deg_To_Rad(o->pitch); att->Yaw = f_Deg_To_Rad(o->yaw);
}

static int g_fail = 0;
#define CHECK(c, ...) do { if (c) { printf("  PASS: "); } else { printf("  FAIL: "); g_fail++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static void unit_tests(void)
{
	printf("[unit] score logic\n");
	{
		/* sequence: 1 1 1 -> confirmed at score 5 */
		ST_Track t; memset(&t, 0, sizeof(t)); t.TrackState = TRK_STATE_TENTATIVE; t.TrackScore = TRK_SCORE_INIT;
		int f[] = { 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0 }; int i;
		const int expState[] = { 1, 1, 2, 3, 3, 2, 3, 3, 3, 3, 4 };
		const int expScore[] = { 3, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5 };
		const int expMem[]   = { 0, 0, 0, 1, 2, 0, 1, 2, 3, 4, 5 };
		int ok = 1;
		for (i = 0; i < 11; i++) { t.Assigned_Flag = f[i]; f_Trk_UpdateState(&t); printf("    flag %d -> state %d score %d mem %d\n", f[i], t.TrackState, t.TrackScore, t.MemoryScore);
			if (t.TrackState != expState[i] || t.TrackScore != expScore[i] || t.MemoryScore != expMem[i]) ok = 0; }
		CHECK(ok, "tentative->confirmed(score>=5)->memory(mem+1)->confirmed(mem=0)->...->deleted(mem>=5)");
	}
	{
		ST_Track t; memset(&t, 0, sizeof(t)); t.TrackState = TRK_STATE_TENTATIVE; t.TrackScore = TRK_SCORE_INIT;
		t.Assigned_Flag = 0; f_Trk_UpdateState(&t); int s1 = t.TrackState, c1 = t.TrackScore;
		f_Trk_UpdateState(&t);
		CHECK(s1 == 1 && c1 == 1 && t.TrackState == 4 && t.TrackScore == 0, "flag 0 always: score 2 -> 1 (tentative) -> 0 (deleted, <= 0)");
	}
	printf("[unit] 2-point init\n");
	{
		ST_TrkPlot a, b; ST_Track t; int i, j, okAcc = 1;
		memset(&a, 0, sizeof(a)); memset(&b, 0, sizeof(b)); memset(&t, 0, sizeof(t));
		a.time = 1.0; b.time = 1.1; a.pos[0] = 1000; a.pos[1] = 2000; a.pos[2] = -300; b.pos[0] = 1010; b.pos[1] = 1990; b.pos[2] = -300;
		for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) { a.cov[i][j] = (i == j) ? 100.0 : 5.0; b.cov[i][j] = (i == j) ? 400.0 : 7.0; }
		f_Trk_TwoPointInit(&t, &a, &b);
		for (i = 0; i < 9; i++) { if ((i % 3) == 2) { if (t.mat_EstX.e[i][0] != 0.0) okAcc = 0; for (j = 0; j < 9; j++) if (t.mat_EstP.e[i][j] != 0.0 || t.mat_EstP.e[j][i] != 0.0) okAcc = 0; } }
		CHECK(fabs(t.mat_EstX.e[0][0] - 1010) < 1e-9 && fabs(t.mat_EstX.e[1][0] - 100) < 1e-6 && fabs(t.mat_EstX.e[4][0] + 100) < 1e-6, "X: pos = z_k, vel = (z_k - z_k-1)/T  (x' = 100, y' = -100 m/s)");
		CHECK(okAcc, "acceleration rows/cols of mat_EstX, mat_EstP are 0");
		CHECK(fabs(t.mat_EstP.e[0][0] - 400) < 1e-9 && fabs(t.mat_EstP.e[0][1] - 4000) < 1e-6 && fabs(t.mat_EstP.e[1][1] - 50000) < 1e-3 && fabs(t.mat_EstP.e[0][3] - 7) < 1e-9 && fabs(t.mat_EstP.e[1][4] - 1200) < 1e-6,
			"P: Ppp = R_k (400), Ppv = R_k/T (4000), Pvv = (R_k + R_k-1)/T^2 (50000), cross-axis too");
		CHECK(t.mat_EstX.row == 9 && t.mat_EstX.column == 1 && t.mat_EstP.row == 9 && t.mat_EstP.column == 9, "dims 9x1, 9x9");
	}
	printf("[unit] velocity gate + track id\n");
	{
		static ST_TrkTable tb; ST_TrkPlot p[2]; ST_TrkScanLog lg; int k;
		f_Trk_Init(&tb); memset(p, 0, sizeof(p));
		p[0].pos[0] = 0; p[0].PlotId = 10000; f_Trk_ProcessScan(&tb, p, 1, 0.0, 0, &lg);
		p[0].pos[0] = 79.9; p[0].PlotId = 10001; f_Trk_ProcessScan(&tb, p, 1, 0.1, 1, &lg);
		CHECK(lg.nCreatedNum == 1 && tb.st_Track[0].TrackId == 100, "|dz|/T = 799 m/s < 800 -> track created, first ID 100");
		f_Trk_Init(&tb);
		p[0].pos[0] = 0; f_Trk_ProcessScan(&tb, p, 1, 0.0, 0, &lg);
		p[0].pos[0] = 80.1; f_Trk_ProcessScan(&tb, p, 1, 0.1, 1, &lg);
		CHECK(lg.nCreatedNum == 0 && lg.nPairNum == 1 && lg.nGatePassNum == 0, "|dz|/T = 801 m/s >= 800 -> rejected");
		{ unsigned ids[8]; int nIds = 0, q;
		f_Trk_Init(&tb); tb.nextTrackId = 9999;
		for (k = 0; k < 6; k++) { p[0].pos[0] = k * 10.0; f_Trk_ProcessScan(&tb, p, 1, 0.1 * k, k, &lg);
			for (q = 0; q < tb.nTrackNum; q++) if (tb.st_Track[q].nInitScan == k && nIds < 8) ids[nIds++] = tb.st_Track[q].TrackId; }
		printf("    created ids in order:"); for (k = 0; k < nIds; k++) printf(" %u", ids[k]); printf("\n");
		CHECK(nIds == 3 && ids[0] == 9999 && ids[1] == 10000 && ids[2] == 100, "ID wraps 9999 -> 10000 -> 100"); }
	}
	printf("[unit] R sigma\n");
	{
		STRUCT_Coord_Sph s; f_Trk_GetMeasSigma(&s);
		CHECK(fabs(s.r - 10.0) < 1e-12 && fabs(f_Rad_To_Deg(s.az) - 0.2) < 1e-12 && fabs(f_Rad_To_Deg(s.el) - 0.2) < 1e-12, "R sigma = 10 m, 0.2 deg, 0.2 deg");
	}
}

int main(int argc, char **argv)
{
	const char *out = (argc > 1) ? argv[1] : ".";
	static ST_SimConfig cfg; static ST_SimState sim; ST_SimSample *smp; ST_PcsScan *scn; int nStep, nScan, i, k;
	char path[512]; FILE *f;

	unit_tests();

	memset(&cfg, 0, sizeof(cfg)); cfg.durationTime = SCN_DURATION_TIME; cfg.stepTime = SCN_STEP_TIME; cfg.nTargetNum = SCN_TARGET_NUM;
	setobj(&s_Platform, &cfg.st_Platform.st_InitLla, &cfg.st_Platform.st_InitAtt, &cfg.st_Platform.headingSpeed);
	for (i = 0; i < 2; i++) setobj(&s_Target[i], &cfg.st_Target[i].st_InitLla, &cfg.st_Target[i].st_InitAtt, &cfg.st_Target[i].headingSpeed);
	for (i = 0; i < 10; i++) { ST_TargetInit *t = &cfg.st_Target[s_Man[i].tgt]; t->st_Maneuver[t->nManeuverNum++] = s_Man[i].m; }

	f_Tgt_InitSim(&sim, &cfg);
	smp = (ST_SimSample *)malloc(sizeof(ST_SimSample) * (sim.nStepNum + 1));
	smp[0] = sim.st_Sample;
	for (nStep = 0; nStep < sim.nStepNum; nStep++) { f_Tgt_StepSim(&sim); smp[nStep + 1] = sim.st_Sample; }
	nScan = sim.nStepNum;
	scn = (ST_PcsScan *)malloc(sizeof(ST_PcsScan) * nScan);
	f_Pcs_Measure(scn, smp, nScan, SCN_SNR, SCN_CLUTTER_NUM);
	{ STRUCT_Coord_Sph p = f_Pcs_CalcSigma(SCN_SNR); printf("[sim] samples %d, scans %d, plot noise sigma r %.3f m az %.4f deg el %.4f deg\n", nStep + 1, nScan, p.r, f_Rad_To_Deg(p.az), f_Rad_To_Deg(p.el)); }
	printf("[icd] sizeof(STRUCT_PlotInfo) %zu, head %u, msg with 7 plots %u B\n", sizeof(STRUCT_PlotInfo), (unsigned)IDD_MSG_HEAD_SIZE, (unsigned)(IDD_MSG_HEAD_SIZE + 7 * sizeof(STRUCT_PlotInfo)));

	/* truth */
	snprintf(path, sizeof(path), "%s/truth.csv", out); f = fopen(path, "w"); fprintf(f, "step,time,obj,east,north,up,r,az_deg,el_deg\n");
	for (i = 0; i <= nStep; i++) for (k = 0; k <= SCN_TARGET_NUM; k++) {
		const ST_TargetState *s = (k == 0) ? &smp[i].st_Platform : &smp[i].st_Target[k - 1];
		STRUCT_Coord_Rect n = f_Tgt_EcefToNed(&s->st_PosEcef, &smp[0].st_Platform.st_PosEcef, &smp[0].st_Platform.st_Lla);
		double r = 0, az = 0, el = 0; if (k > 0 && i < nScan) { r = scn[i].st_True[k - 1].r; az = f_Rad_To_Deg(scn[i].st_True[k - 1].az); el = f_Rad_To_Deg(scn[i].st_True[k - 1].el); }
		fprintf(f, "%d,%.3f,%d,%.3f,%.3f,%.3f,%.3f,%.5f,%.5f\n", i, smp[i].simTime, k, n.y, n.x, -n.z, r, az, el); }
	fclose(f);

	if (!f_Lnk_Start(scn, smp, nScan, 0)) { printf("start failed\n"); return 1; }
	{ ST_LnkStatus st; do { usleep(20000); f_Lnk_GetStatus(&st); } while (st.isRunning);
	  printf("[link] port %u sent %d recv %d lost %d bad %d proc %d err %d bytes %lld/%lld created %d deleted %d full %d\n", st.port, st.nSentNum, st.nRecvNum, st.nLostNum, st.nBadNum, st.nProcNum, st.errorCode,
		  st.sentBytes, st.recvBytes, st.nCreatedSum, st.nDeletedSum, st.nTableFullSum);
	  CHECK(st.nSentNum == nScan && st.nRecvNum == nScan && st.nLostNum == 0 && st.nProcNum == nScan && st.errorCode == 0, "all %d plot messages sent / received / processed over UDP", nScan);
	}
	f_Lnk_Stop();

	{
		static ST_TrkView tv[TRK_MAX_TRACK]; static ST_LnkPlot lp[IDD_MAX_PLOT]; ST_LnkScan ls; FILE *fp, *ft, *fl;
		unsigned minId = 0xFFFFFFFFu, maxId = 0, minTid = 0xFFFFFFFFu, maxTid = 0; int flagNonZero = 0, badState = 0, accNonZero = 0;
		double maxRerr = 0;
		snprintf(path, sizeof(path), "%s/plots.csv", out); fp = fopen(path, "w"); fprintf(fp, "scan,time,plot_id,source,north,east,down\n");
		snprintf(path, sizeof(path), "%s/tracks.csv", out); ft = fopen(path, "w");
		fprintf(ft, "scan,time,track_id,state,score,memory,flag,init_scan,plot_prev,plot_cur,src_prev,src_cur,north,east,down,vn,ve,vd,psn,pse,psd,vsn,vse,vsd\n");
		snprintf(path, sizeof(path), "%s/scanlog.csv", out); fl = fopen(path, "w"); fprintf(fl, "scan,time,plots,cand,pairs,gate_pass,created,confirmed,memory,deleted,tracks,msg_size\n");
		for (i = 0; f_Lnk_CopyScan(i, &ls, tv, lp); i++) {
			for (k = 0; k < ls.nPlotNum; k++) { fprintf(fp, "%u,%.3f,%u,%d,%.3f,%.3f,%.3f\n", ls.ScanNum, ls.time, lp[k].PlotId, f_Lnk_GetPlotSource(lp[k].PlotId), lp[k].pos[0], lp[k].pos[1], lp[k].pos[2]);
				if (lp[k].PlotId < minId) minId = lp[k].PlotId; if (lp[k].PlotId > maxId) maxId = lp[k].PlotId; }
			for (k = 0; k < ls.nTrackNum; k++) { ST_TrkView *v = &tv[k];
				fprintf(ft, "%u,%.3f,%u,%d,%d,%d,%d,%d,%u,%u,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n", ls.ScanNum, ls.time, v->TrackId, v->TrackState, v->TrackScore, v->MemoryScore, v->Assigned_Flag,
					v->nInitScan, v->PlotId[0], v->PlotId[1], f_Lnk_GetPlotSource(v->PlotId[0]), f_Lnk_GetPlotSource(v->PlotId[1]), v->pos[0], v->pos[1], v->pos[2], v->vel[0], v->vel[1], v->vel[2],
					v->posStd[0], v->posStd[1], v->posStd[2], v->velStd[0], v->velStd[1], v->velStd[2]);
				if (v->TrackId < minTid) minTid = v->TrackId; if (v->TrackId > maxTid) maxTid = v->TrackId;
				if (v->Assigned_Flag != 0) flagNonZero++; if (v->TrackState < 1 || v->TrackState > 4) badState++;
				/* at creation scan, posStd^2 must equal R built from 10 m / 0.2 deg / 0.2 deg at the current plot */
				if (v->nInitScan == (int)ls.ScanNum) { int q; for (q = 0; q < ls.nPlotNum; q++) if (lp[q].PlotId == v->PlotId[1]) {
					STRUCT_Coord_Sph sg, ms; STRUCT_Coord_Attitude at = { 0, 0, 0 }; double R[3][3]; double h, n = lp[q].pos[0], e = lp[q].pos[1], d = lp[q].pos[2]; int ax;
					f_Trk_GetMeasSigma(&sg); h = sqrt(n * n + e * e); ms.r = sqrt(h * h + d * d);
					/* antenna frame = body = NED here (platform attitude 0): az left +, el up + */
					{ STRUCT_Coord_Rect bd = f_Trans_Ned_To_Body(n, e, d, 0, 0, 0); STRUCT_Coord_Rect an = f_Trans_Body_To_Ant(bd.x, bd.y, bd.z); ms = f_Trans_Ant_XYZ_To_Sph(an.x, an.y, an.z); }
					f_Trk_MeasCov(R, &ms, &sg, &at);
					for (ax = 0; ax < 3; ax++) { double err = fabs(v->posStd[ax] - sqrt(R[ax][ax])); if (err > maxRerr) maxRerr = err; } } }
			}
			fprintf(fl, "%u,%.3f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%u\n", ls.ScanNum, ls.time, ls.st_Log.nPlotNum, ls.st_Log.nCandidateNum, ls.st_Log.nPairNum, ls.st_Log.nGatePassNum, ls.st_Log.nCreatedNum,
				ls.st_Log.nConfirmedNum, ls.st_Log.nMemoryNum, ls.st_Log.nDeletedNum, ls.nTrackNum, ls.MsgSize);
		}
		fclose(fp); fclose(ft); fclose(fl);
		CHECK(minId >= 10000 && maxId <= 1000000, "plot IDs within 10000..1000000 (got %u..%u)", minId, maxId);
		CHECK(minTid >= 100 && maxTid <= 10000, "track IDs within 100..10000 (got %u..%u)", minTid, maxTid);
		CHECK(flagNonZero == 0, "Assigned_Flag == 0 for every track every scan");
		CHECK(badState == 0, "TrackState in 1..4");
		CHECK(maxRerr < 1e-3, "track initial P position std == sqrt(diag R) with R from 10 m / 0.2 deg / 0.2 deg (max err %.2e m)", maxRerr);
	}
	printf("FAILURES: %d\n", g_fail);
	return g_fail ? 2 : 0;
}
