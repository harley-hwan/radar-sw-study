/* Part 6 validation harness (Linux). Runs the real TargetSimCore C sources (PlotLink.c UDP threads, TrackMgr.c Singer CMKF):
 *   unit   : Singer F / Q against matrix exponential, numerical integration and 60-digit references, CMKF update against an
 *            independent implementation, association assumption, divergence counter, Part 5 logic kept (score, 2-point, gate, ID)
 *   runs   : 대함 / 대공 scenario x 과제 값 / 조정 값 over loopback UDP -> <out>/<run>/{truth,plots,tracks,scanlog}.csv, runs.csv
 *   sweep  : Monte Carlo (noise seeds) over q, R angle sigma, tau -> <out>/sweep.csv
 * usage: ./harness <out_dir> [all|unit|runs|sweep] [seeds]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/stat.h>
#include "PlotLink.h"
#include "PlotSim.h"
#include "TrackMgr.h"
#include "Idd.h"

/* scenario: same values as TargetSimUI Scenario.cpp */
#define SCN_DURATION_TIME	116.0
#define SCN_STEP_TIME		0.1
#define SCN_SNR				20.0
#define SCN_CLUTTER_NUM		0
#define SCN_SHIP			0
#define SCN_AIR				1
#define SCN_NUM				2
#define STEADY_UPDATES		20		/* metrics skip the 2-point init transient (first 20 updates = 2 s) */
#define SWEEP_SEEDS			30

/* tuned parameters (chosen from sweep.csv: no divergence judgement on any seed, smallest position RMS) */
static const ST_TrkParam s_Tuned[SCN_NUM] = { { 10.0, 0.5, 0.5, 0.5, 10.0 }, { 10.0, 0.5, 0.5, 20.0, 10.0 } };
static const char *s_ScnKey[SCN_NUM] = { "ship", "air" };

typedef struct { double lat, lon, alt, speed, roll, pitch, yaw; } OBJ;
static const OBJ s_Platform = { 32.0, 126.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
static const OBJ s_Target[SCN_NUM] = { { 32.125, 126.03, 0.0, 30.0, 0.0, 0.0, 270.0 }, { 32.12, 126.0, 2000.0, 200.0, 0.0, 0.0, 180.0 } };
typedef struct { int tgt; ST_TargetManeuver m; } MAN;
static const MAN s_Man[10] = {
	{ 0, { TGT_TURN_YAW, -1.0, 28.4, 30.8 } }, { 0, { TGT_TURN_YAW, 1.0, 41.4, 43.8 } }, { 0, { TGT_TURN_YAW, 1.0, 72.2, 74.6 } }, { 0, { TGT_TURN_YAW, -1.0, 85.2, 87.6 } },
	{ 1, { TGT_TURN_YAW, -4.0, 6.0, 14.0 } }, { 1, { TGT_TURN_YAW, 4.0, 24.1, 32.1 } }, { 1, { TGT_TURN_YAW, 4.0, 35.2, 43.2 } }, { 1, { TGT_TURN_YAW, -4.0, 67.5, 75.5 } },
	{ 1, { TGT_TURN_YAW, -4.0, 78.5, 86.5 } }, { 1, { TGT_TURN_YAW, 4.0, 102.6, 116.0 } } };

static int g_fail = 0;
#define CHECK(c, ...) do { if (c) { printf("  PASS: "); } else { printf("  FAIL: "); g_fail++; } printf(__VA_ARGS__); printf("\n"); } while (0)

/* ---------------------------------------------------------------- small linear algebra (independent of matrixCalcLib) */

static double inv3(const double a[3][3], double r[3][3])
{
	const double det = a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0]) + a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
	r[0][0] = (a[1][1] * a[2][2] - a[1][2] * a[2][1]) / det; r[0][1] = (a[0][2] * a[2][1] - a[0][1] * a[2][2]) / det; r[0][2] = (a[0][1] * a[1][2] - a[0][2] * a[1][1]) / det;
	r[1][0] = (a[1][2] * a[2][0] - a[1][0] * a[2][2]) / det; r[1][1] = (a[0][0] * a[2][2] - a[0][2] * a[2][0]) / det; r[1][2] = (a[0][2] * a[1][0] - a[0][0] * a[1][2]) / det;
	r[2][0] = (a[1][0] * a[2][1] - a[1][1] * a[2][0]) / det; r[2][1] = (a[0][1] * a[2][0] - a[0][0] * a[2][1]) / det; r[2][2] = (a[0][0] * a[1][1] - a[0][1] * a[1][0]) / det;
	return det;
}

static double quad3(const double v[3], const double m[3][3])
{
	double s = 0; int i, j;
	for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) s += v[i] * m[i][j] * v[j];
	return s;
}

/* Cholesky: 1 if positive definite */
static int is_pd(const double *a, int n, int ld)
{
	double l[9][9]; int i, j, k;
	for (i = 0; i < n; i++) for (j = 0; j <= i; j++) {
		double s = a[i * ld + j];
		for (k = 0; k < j; k++) s -= l[i][k] * l[j][k];
		if (i == j) { if (!(s > 0)) return 0; l[i][i] = sqrt(s); } else l[i][j] = s / l[j][j];
	}
	return 1;
}

/* e^(A T) for 3x3 by Taylor series (|A T| small) */
static void expm3(const double a[3][3], double t, double r[3][3])
{
	double term[3][3], nt[3][3]; int i, j, k, n;
	for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) { r[i][j] = (i == j); term[i][j] = (i == j); }
	for (n = 1; n < 60; n++) {
		for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) { double s = 0; for (k = 0; k < 3; k++) s += term[i][k] * a[k][j] * t; nt[i][j] = s / n; }
		for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) { term[i][j] = nt[i][j]; r[i][j] += term[i][j]; }
	}
}

/* Singer q_k by Simpson integration of g g^T, g = third column of f(u) = [tau^2(u/tau + expm1(-u/tau)), -tau expm1(-u/tau), e^(-u/tau)] */
static void qk_numeric(double T, double q, double tau, double out[6])
{
	const int n = 20000; int i, k; double h = T / n, s[6] = { 0 };
	for (i = 0; i <= n; i++) {
		const double u = i * h, w = (i == 0 || i == n) ? 1 : ((i % 2) ? 4 : 2), x = u / tau;
		const double g[3] = { tau * tau * (x + expm1(-x)), -tau * expm1(-x), exp(-x) };
		const double gg[6] = { g[0] * g[0], g[0] * g[1], g[0] * g[2], g[1] * g[1], g[1] * g[2], g[2] * g[2] };
		for (k = 0; k < 6; k++) s[k] += w * gg[k];
	}
	for (k = 0; k < 6; k++) out[k] = (2.0 * q * q / tau) * s[k] * h / 3.0;
}

static void q_elems(const matrix *Q, double out[6])
{
	out[0] = Q->e[0][0]; out[1] = Q->e[0][1]; out[2] = Q->e[0][2]; out[3] = Q->e[1][1]; out[4] = Q->e[1][2]; out[5] = Q->e[2][2];
}

static double rel(double a, double b) { return fabs(a - b) / fmax(fabs(b), 1e-300); }

/* deterministic pseudo random in [-1, 1] for test matrices */
static unsigned long long g_rng = 88172645463325252ULL;
static double rnd(void) { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 7; g_rng ^= g_rng << 17; return ((double)(g_rng >> 11) / 9007199254740992.0) * 2.0 - 1.0; }

/* ---------------------------------------------------------------- unit tests */

static void test_singer(void)
{
	/* 60-digit references (mpmath) of (2q^2/tau) q_k: q11 q12 q13 q22 q23 q33 */
	static const struct { double T, tau, q, v[6]; } ref[] = {
		{ 0.1, 10.0, 0.5, { 2.4861605757240815278e-8, 6.2085063903612020498e-7, 8.2504565334176940638e-6, 0.000016542247922801872993, 0.00024751452104798770985, 0.0049503316733111747169 } },
		{ 0.1, 10.0, 20.0, { 0.000039778569211585304445, 0.00099336102245779232797, 0.013200730453468310502, 0.026467596676482996789, 0.39602323367678033576, 7.920530677297879547 } },
		{ 0.2, 10.0, 0.5, { 7.9117424929114400329e-7, 9.8677707042716059456e-6, 0.000065347885164461809729, 0.00013135186744998612291, 0.0009802313470315126017, 0.0098026402119191981735 } },
		{ 0.1, 2.0, 1.0, { 4.8635569533361292161e-7, 0.00001209187682364712699, 0.0001585580558761037324, 0.00032111986758585285862, 0.0047571380690631104779, 0.095162581964040431859 } },
		{ 0.1, 1000.0, 1.0, { 9.9994444642851615186e-10, 2.4998333402775561165e-8, 3.3330000183326116888e-7, 6.6661666899991678014e-7, 9.9990000583308345295e-6, 0.00019998000133326668043 } },
		{ 1.0, 5.0, 3.0, { 0.16135219861056929786, 0.3946962497268677316, 0.49222186496279005221, 1.0356674121648301354, 1.4786342945854012532, 2.9671195856792462933 } },
		{ 0.5, 1.0, 2.0, { 0.0095702909464966581208, 0.045395125835235591967, 0.1023595964636970192, 0.23297279071636549128, 0.61927248698470189755, 2.5284822353142307136 } },
		{ 0.1, 1.0e6, 1.0, { 9.9999994444444670613e-13, 2.4999998333333408329e-11, 3.3333330000000188884e-10, 6.6666661666666911102e-10, 9.9999990000000594436e-9, 1.9999998000000134444e-7 } } };
	static const char *name[6] = { "q11", "q12", "q13", "q22", "q23", "q33" };
	matrix F, Q; int c, k, i, j, b;

	printf("[unit] Singer F = blockdiag(f, f, f) vs e^(A T), A = [0 1 0; 0 0 1; 0 0 -1/tau]\n");
	{
		static const double cs[][2] = { { 0.1, 10.0 }, { 0.2, 10.0 }, { 1.0, 5.0 }, { 0.1, 1000.0 }, { 0.1, 1.0 } };
		double worst = 0; int okBlock = 1;
		for (c = 0; c < 5; c++) {
			const double A[3][3] = { { 0, 1, 0 }, { 0, 0, 1 }, { 0, 0, -1.0 / cs[c][1] } }; double E[3][3];
			expm3(A, cs[c][0], E); f_Trk_SingerF(&F, cs[c][0], cs[c][1]);
			for (b = 0; b < 3; b++) for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) { double d = fabs(F.e[3 * b + i][3 * b + j] - E[i][j]); if (d > worst) worst = d; }
			for (i = 0; i < 9; i++) for (j = 0; j < 9; j++) if ((i / 3) != (j / 3) && F.e[i][j] != 0.0) okBlock = 0;
		}
		CHECK(worst < 1e-12 && okBlock && F.row == 9 && F.column == 9, "f matches matrix exponential for 5 (T, tau) cases (max |diff| %.1e), off-diagonal blocks 0", worst);
		f_Trk_SingerF(&F, 0.1, 10.0);
		printf("    T 0.1, tau 10: f = [1 %.4f %.10f; 0 1 %.10f; 0 0 %.10f]\n", F.e[0][1], F.e[0][2], F.e[1][2], F.e[2][2]);
	}

	printf("[unit] Singer Q vs 60-digit reference (closed form for T/tau >= %.2f, series below)\n", TRK_SERIES_X);
	for (c = 0; c < (int)(sizeof(ref) / sizeof(ref[0])); c++) {
		double v[6], worst = 0; int wk = 0;
		f_Trk_SingerQ(&Q, ref[c].T, ref[c].q, ref[c].tau); q_elems(&Q, v);
		for (k = 0; k < 6; k++) { double r = rel(v[k], ref[c].v[k]); if (r > worst) { worst = r; wk = k; } }
		CHECK(worst < 1e-8, "T %.1f tau %g q %g (T/tau %.0e, %s): max rel err %.1e (%s)", ref[c].T, ref[c].tau, ref[c].q, ref[c].T / ref[c].tau,
			(ref[c].T / ref[c].tau < TRK_SERIES_X) ? "series" : "closed", worst, name[wk]);
	}

	printf("[unit] Singer Q vs Simpson integral of e^(A u) G G^T e^(A u)^T (independent of the formula)\n");
	{
		static const double cs[][3] = { { 0.1, 10.0, 0.5 }, { 0.1, 10.0, 20.0 }, { 0.3, 2.0, 5.0 }, { 2.0, 1.0, 1.0 } };
		for (c = 0; c < 4; c++) {
			double v[6], n[6], worst = 0;
			f_Trk_SingerQ(&Q, cs[c][0], cs[c][2], cs[c][1]); q_elems(&Q, v); qk_numeric(cs[c][0], cs[c][2], cs[c][1], n);
			for (k = 0; k < 6; k++) { double r = rel(v[k], n[k]); if (r > worst) worst = r; }
			CHECK(worst < 1e-9, "T %.1f tau %g q %g: max rel err %.1e", cs[c][0], cs[c][1], cs[c][2], worst);
		}
	}

	printf("[unit] Q structure, positive definiteness over T/tau = 1e-6 .. 10, branch switch\n");
	{
		int okPd = 1, okSym = 1, nCase = 0; double lx, sw = 0, v1[6], v2[6];
		for (lx = -6.0; lx <= 1.0 + 1e-9; lx += 0.25) {
			const double x = pow(10.0, lx);
			f_Trk_SingerQ(&Q, 0.1, 1.0, 0.1 / x); nCase++;
			for (b = 0; b < 3; b++) {
				double blk[9]; for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) blk[i * 3 + j] = Q.e[3 * b + i][3 * b + j];
				if (!is_pd(blk, 3, 3)) okPd = 0;
			}
			for (i = 0; i < 9; i++) for (j = 0; j < 9; j++) if (Q.e[i][j] != Q.e[j][i] || ((i / 3) != (j / 3) && Q.e[i][j] != 0.0)) okSym = 0;
		}
		CHECK(okPd, "every 3x3 block of Q is positive definite (%d values of T/tau)", nCase);
		CHECK(okSym, "Q symmetric, off-diagonal blocks 0");
		f_Trk_SingerQ(&Q, 0.1, 1.0, 0.1 / (TRK_SERIES_X * (1.0 - 1e-12))); q_elems(&Q, v1);
		f_Trk_SingerQ(&Q, 0.1, 1.0, 0.1 / (TRK_SERIES_X * (1.0 + 1e-12))); q_elems(&Q, v2);
		for (k = 0; k < 6; k++) { double r = rel(v1[k], v2[k]); if (r > sw) sw = r; }
		CHECK(sw < 1e-7, "series and closed form agree at the switch T/tau = %.2f (max rel diff %.1e)", TRK_SERIES_X, sw);
		f_Trk_SingerQ(&Q, 0.1, 0.0, 10.0);
		{ int zero = 1; for (i = 0; i < 9; i++) for (j = 0; j < 9; j++) if (Q.e[i][j] != 0.0) zero = 0; CHECK(zero, "q = 0 -> Q = 0"); }
	}

	printf("[unit] handout q12 with the extra T (2(T/tau)T e^(-T/tau)) is not a covariance\n");
	{
		const double T = 0.1, tau = 10.0, x = T / tau, e1 = exp(-x), e2 = exp(-2 * x);
		const double q11 = pow(tau, 5) / 2 * (1 - e2 + 2 * x + 2 * x * x * x / 3 - 2 * x * x - 4 * x * e1);
		const double q12t = pow(tau, 4) / 2 * (1 + e2 - 2 * e1 + 2 * x * T * e1 - 2 * x + x * x);
		const double q12 = pow(tau, 4) / 2 * (1 + e2 - 2 * e1 + 2 * x * e1 - 2 * x + x * x);
		const double q22 = pow(tau, 3) / 2 * (4 * e1 - 3 - e2 + 2 * x);
		printf("    T 0.1, tau 10: q11 %.4e, q22 %.4e, q12 standard %.4e, q12 with extra T %.4f\n", q11, q22, q12, q12t);
		CHECK(q12t * q12t > q11 * q22 && q12 * q12 < q11 * q22, "extra T breaks q12^2 <= q11 q22 (det of [q11 q12; q12 q22] = %.3e < 0), standard q12 keeps it", q11 * q22 - q12t * q12t);
	}
}

static void make_track(ST_Track *t, const double pos[3], double pVar)
{
	int i;
	memset(t, 0, sizeof(*t));
	Matrix_initialize(&t->mat_EstX, 9, 1); Matrix_initialize(&t->mat_EstP, 9, 9);
	for (i = 0; i < 3; i++) t->mat_EstX.e[3 * i][0] = pos[i];
	for (i = 0; i < 9; i++) t->mat_EstP.e[i][i] = pVar;
	t->TrackState = TRK_STATE_TENTATIVE; t->TrackScore = TRK_SCORE_INIT; t->nis = -1;
}

static void test_kf(void)
{
	ST_TrkParam prm; f_Trk_DefaultParam(&prm);

	printf("[unit] CMKF update vs independent implementation (P - K P[pos,:], K = P[:,pos] S^-1)\n");
	{
		ST_Track t; ST_TrkPlot z; double A[9][9], P[9][9], X[9], S[3][3], Si[3][3], K[9][3], v[3], Xn[9], Pn[9][9], worstX = 0, worstP = 0, nis;
		int i, j, k, okSym = 1, trial, okPd = 1, okDec = 1;
		static const int pos[3] = { 0, 3, 6 };
		for (trial = 0; trial < 50; trial++) {
			memset(&t, 0, sizeof(t)); memset(&z, 0, sizeof(z));
			for (i = 0; i < 9; i++) for (j = 0; j < 9; j++) A[i][j] = rnd() * ((i % 3 == 0) ? 30.0 : ((i % 3 == 1) ? 5.0 : 1.0));
			for (i = 0; i < 9; i++) for (j = 0; j < 9; j++) { double s = (i == j) ? 1.0 : 0.0; for (k = 0; k < 9; k++) s += A[i][k] * A[j][k]; P[i][j] = s; }
			for (i = 0; i < 9; i++) X[i] = rnd() * 1000.0;
			{ double B[3][3]; for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) B[i][j] = rnd() * 20.0;
			  for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) { double s = (i == j) ? 4.0 : 0.0; for (k = 0; k < 3; k++) s += B[i][k] * B[j][k]; z.cov[i][j] = s; } }
			for (i = 0; i < 3; i++) z.pos[i] = X[pos[i]] + rnd() * 100.0;
			Matrix_initialize(&t.mat_EstX, 9, 1); Matrix_initialize(&t.mat_EstP, 9, 9);
			for (i = 0; i < 9; i++) { t.mat_EstX.e[i][0] = X[i]; for (j = 0; j < 9; j++) t.mat_EstP.e[i][j] = P[i][j]; }
			/* reference */
			for (i = 0; i < 3; i++) { v[i] = z.pos[i] - X[pos[i]]; for (j = 0; j < 3; j++) S[i][j] = P[pos[i]][pos[j]] + z.cov[i][j]; }
			inv3(S, Si);
			for (i = 0; i < 9; i++) for (j = 0; j < 3; j++) { double s = 0; for (k = 0; k < 3; k++) s += P[i][pos[k]] * Si[k][j]; K[i][j] = s; }
			for (i = 0; i < 9; i++) { double s = X[i]; for (k = 0; k < 3; k++) s += K[i][k] * v[k]; Xn[i] = s; }
			for (i = 0; i < 9; i++) for (j = 0; j < 9; j++) { double s = P[i][j]; for (k = 0; k < 3; k++) s -= K[i][k] * P[pos[k]][j]; Pn[i][j] = s; }
			nis = quad3(v, Si);
			f_Trk_Update(&t, &z);
			for (i = 0; i < 9; i++) { double d = fabs(t.mat_EstX.e[i][0] - Xn[i]) / (1.0 + fabs(Xn[i])); if (d > worstX) worstX = d;
				for (j = 0; j < 9; j++) { double e = fabs(t.mat_EstP.e[i][j] - 0.5 * (Pn[i][j] + Pn[j][i])) / (1.0 + fabs(Pn[i][i])); if (e > worstP) worstP = e;
					if (t.mat_EstP.e[i][j] != t.mat_EstP.e[j][i]) okSym = 0; } }
			{ double pp[81]; for (i = 0; i < 9; i++) for (j = 0; j < 9; j++) pp[i * 9 + j] = t.mat_EstP.e[i][j]; if (!is_pd(pp, 9, 9)) okPd = 0; }
			for (i = 0; i < 9; i++) if (t.mat_EstP.e[i][i] > P[i][i] * (1 + 1e-12)) okDec = 0;
			if (fabs(t.nis - nis) > 1e-9 * (1 + nis)) okDec = 0;
		}
		CHECK(worstX < 1e-10 && worstP < 1e-10, "50 random cases: X max rel diff %.1e, P max rel diff %.1e", worstX, worstP);
		CHECK(okSym && okPd, "updated P exactly symmetric and positive definite");
		CHECK(okDec, "every diagonal of P does not grow, NIS = v^T S^-1 v");
	}

	printf("[unit] predict X = F X, P = F P F^T + Q\n");
	{
		ST_Track t; double p0[3] = { 1000, 2000, -500 }; matrix F, Q, Ft, FPFt, FX, PP; int i, j, ok = 1;
		make_track(&t, p0, 50.0); t.mat_EstX.e[1][0] = 10; t.mat_EstX.e[2][0] = 2; t.mat_EstX.e[4][0] = -30; t.stateTime = 5.0;
		F = t.mat_EstX; /* keep copy */
		{ ST_Track u = t; f_Trk_Predict(&u, 5.0, &prm); for (i = 0; i < 9; i++) if (u.mat_EstX.e[i][0] != t.mat_EstX.e[i][0]) ok = 0;
		  for (i = 0; i < 9; i++) for (j = 0; j < 9; j++) if (u.mat_EstP.e[i][j] != t.mat_EstP.e[i][j]) ok = 0; }
		CHECK(ok, "dt = 0: X, P unchanged");
		f_Trk_SingerF(&F, 0.1, prm.singerTau); f_Trk_SingerQ(&Q, 0.1, prm.singerQ, prm.singerTau); Ft = Matrix_Transpose(&F);
		FX = Matrix_Product2(&F, &t.mat_EstX); FPFt = Matrix_Product3(&F, &t.mat_EstP, &Ft); PP = Matrix_Add(&FPFt, &Q);
		f_Trk_Predict(&t, 5.1, &prm); ok = 1;
		for (i = 0; i < 9; i++) { if (fabs(t.mat_EstX.e[i][0] - FX.e[i][0]) > 1e-9) ok = 0; for (j = 0; j < 9; j++) if (fabs(t.mat_EstP.e[i][j] - PP.e[i][j]) > 1e-9) ok = 0; }
		CHECK(ok && fabs(t.stateTime - 5.1) < 1e-12, "dt = 0.1: X = F X, P = F P F^T + Q, stateTime 5.1");
		CHECK(fabs(t.mat_EstX.e[0][0] - (1000 + 10 * 0.1 + 2 * 0.0049833749)) < 1e-6, "x = x + T x' + tau^2(-1 + T/tau + e^(-T/tau)) x''  (%.6f)", t.mat_EstX.e[0][0]);
	}

	printf("[unit] gate-out counter and divergence judgement\n");
	{
		ST_Track t, tpl; ST_TrkPlot z; double p0[3] = { 0, 0, 0 }; int i, seqOk = 1;
		/* NIS for a plot at distance d (S = 2 I): d^2 / 2. gate 11.34 -> outside if d > 4.76 */
		static const double dist[] = { 10, 10, 10, 10, 10, 10, 1, 10, 10, 10, 10, 10 };
		static const int expRun[] = { 1, 2, 3, 4, 5, 6, 0, 1, 2, 3, 4, 5 };
		static const int expDiv[] = { 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 2 };
		make_track(&tpl, p0, 1.0); t = tpl; memset(&z, 0, sizeof(z));
		for (i = 0; i < 3; i++) z.cov[i][i] = 1.0;
		for (i = 0; i < 12; i++) {
			t.mat_EstX = tpl.mat_EstX; t.mat_EstP = tpl.mat_EstP; z.pos[0] = dist[i]; z.PlotId = 100 + i;
			f_Trk_Update(&t, &z);
			if (t.nGateOutRun != expRun[i] || t.nDivergeNum != expDiv[i]) seqOk = 0;
		}
		CHECK(seqOk && t.nGateOutSum == 11 && t.nUpdateNum == 12 && t.PlotIdLast == 111, "run 1..6, reset by NIS 0.5, again 1..5; divergence counted when run reaches %d (twice)", TRK_DIVERGE_NUM);
	}

	printf("[unit] association assumption (closest track-plot pair first, no gate)\n");
	{
		static ST_TrkTable tb; ST_TrkPlot p[2]; ST_TrkScanLog lg; double a[3] = { 0, 0, 0 }, b[3] = { 10, 0, 0 }; int i;
		f_Trk_Init(&tb, NULL); memset(p, 0, sizeof(p));
		make_track(&tb.st_Track[0], a, 100.0); make_track(&tb.st_Track[1], b, 100.0);
		tb.st_Track[0].TrackId = 100; tb.st_Track[1].TrackId = 101; tb.st_Track[0].stateTime = tb.st_Track[1].stateTime = 1.0; tb.nTrackNum = 2;
		for (i = 0; i < 2; i++) { p[i].cov[0][0] = p[i].cov[1][1] = p[i].cov[2][2] = 1.0; p[i].time = 1.0; }
		p[0].pos[0] = 9; p[0].PlotId = 1; p[1].pos[0] = 40; p[1].PlotId = 2;
		f_Trk_ProcessScan(&tb, p, 2, 1.0, 10, &lg);
		CHECK(tb.st_Track[1].PlotIdLast == 1 && tb.st_Track[0].PlotIdLast == 2 && tb.st_Track[0].Assigned_Flag == 1 && tb.st_Track[1].Assigned_Flag == 1
			&& lg.nUpdatedNum == 2 && lg.nCreatedNum == 0 && tb.nTrackNum == 2,
			"tracks at 0 / 10 m, plots at 9 / 40 m: track 101 takes 9 m (closest pair), track 100 takes 40 m anyway (no gate), no new track");
		CHECK(tb.st_Track[0].nis > TRK_GATE_CHI2 && lg.nGateOutNum == 1, "40 m plot updates track 100 although NIS %.1f > gate (counted as gate-out)", tb.st_Track[0].nis);
		p[0].pos[0] = 12; p[0].PlotId = 3; p[0].time = 1.1;
		f_Trk_ProcessScan(&tb, p, 1, 1.1, 11, &lg);
		CHECK(tb.st_Track[1].Assigned_Flag == 1 && tb.st_Track[0].Assigned_Flag == 0 && tb.st_Track[1].TrackScore == TRK_SCORE_INIT + 2 && tb.st_Track[0].TrackScore == TRK_SCORE_INIT,
			"one plot at 12 m, tracks now at %.1f / %.1f m: the closer takes it (score +1), the other gets Assigned_Flag 0 (score -1)", tb.st_Track[0].mat_EstX.e[0][0], tb.st_Track[1].mat_EstX.e[0][0]);
	}

	printf("[unit] parameters\n");
	{
		static ST_TrkTable tb; ST_TrkParam bad = { -1.0, NAN, 0.0, -0.1, 0.0 }, z0 = { 5.0, 0.4, 0.6, 0.0, 3.0 }; STRUCT_Coord_Sph s;
		f_Trk_Init(&tb, &bad);
		CHECK(tb.st_Param.sigmaRange == TRK_R_SIGMA_RANGE && tb.st_Param.sigmaAzimuth == TRK_R_SIGMA_AZIMUTH && tb.st_Param.sigmaElevation == TRK_R_SIGMA_ELEVATION
			&& tb.st_Param.singerQ == TRK_SINGER_Q && tb.st_Param.singerTau == TRK_SINGER_TAU, "out of range (sigma <= 0, NaN, q < 0, tau <= 0) -> default");
		f_Trk_Init(&tb, &z0);
		CHECK(tb.st_Param.singerQ == 0.0 && tb.st_Param.singerTau == 3.0 && tb.st_Param.sigmaRange == 5.0, "q = 0 allowed, valid values kept");
		f_Trk_DefaultParam(&z0); f_Trk_GetMeasSigma(&s, &z0);
		CHECK(fabs(s.r - 10.0) < 1e-12 && fabs(f_Rad_To_Deg(s.az) - 0.3) < 1e-12 && fabs(f_Rad_To_Deg(s.el) - 0.3) < 1e-12 && z0.singerQ == 0.5 && z0.singerTau == 10.0,
			"default = 과제 값: R sigma 10 m, 0.3 deg, 0.3 deg, q 0.5, tau 10");
	}
}

static void test_part5_logic(void)
{
	printf("[unit] Part 5 logic kept: score\n");
	{
		ST_Track t; memset(&t, 0, sizeof(t)); t.TrackState = TRK_STATE_TENTATIVE; t.TrackScore = TRK_SCORE_INIT;
		int f[] = { 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0 }; int i, ok = 1;
		const int expState[] = { 1, 1, 2, 3, 3, 2, 3, 3, 3, 3, 4 }, expScore[] = { 3, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5 }, expMem[] = { 0, 0, 0, 1, 2, 0, 1, 2, 3, 4, 5 };
		for (i = 0; i < 11; i++) { t.Assigned_Flag = f[i]; f_Trk_UpdateState(&t); if (t.TrackState != expState[i] || t.TrackScore != expScore[i] || t.MemoryScore != expMem[i]) ok = 0; }
		CHECK(ok, "tentative->confirmed(score>=5)->memory(mem+1)->confirmed(mem=0)->...->deleted(mem>=5)");
	}
	printf("[unit] Part 5 logic kept: 2-point init (+ filter start time)\n");
	{
		ST_TrkPlot a, b; ST_Track t; int i, j, okAcc = 1;
		memset(&a, 0, sizeof(a)); memset(&b, 0, sizeof(b)); memset(&t, 0, sizeof(t));
		a.time = 1.0; b.time = 1.1; b.PlotId = 77; a.pos[0] = 1000; a.pos[1] = 2000; a.pos[2] = -300; b.pos[0] = 1010; b.pos[1] = 1990; b.pos[2] = -300;
		for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) { a.cov[i][j] = (i == j) ? 100.0 : 5.0; b.cov[i][j] = (i == j) ? 400.0 : 7.0; }
		f_Trk_TwoPointInit(&t, &a, &b);
		for (i = 0; i < 9; i++) if ((i % 3) == 2) { if (t.mat_EstX.e[i][0] != 0.0) okAcc = 0; for (j = 0; j < 9; j++) if (t.mat_EstP.e[i][j] != 0.0 || t.mat_EstP.e[j][i] != 0.0) okAcc = 0; }
		CHECK(fabs(t.mat_EstX.e[0][0] - 1010) < 1e-9 && fabs(t.mat_EstX.e[1][0] - 100) < 1e-6 && fabs(t.mat_EstX.e[4][0] + 100) < 1e-6 && okAcc, "X: pos z_k, vel (z_k - z_k-1)/T, acc 0 (P acc rows 0)");
		CHECK(fabs(t.mat_EstP.e[0][0] - 400) < 1e-9 && fabs(t.mat_EstP.e[0][1] - 4000) < 1e-6 && fabs(t.mat_EstP.e[1][1] - 50000) < 1e-3 && fabs(t.mat_EstP.e[1][4] - 1200) < 1e-6,
			"P: Ppp = R_k, Ppv = R_k/T, Pvv = (R_k + R_k-1)/T^2");
		CHECK(fabs(t.stateTime - 1.1) < 1e-12 && t.PlotIdLast == 77 && t.nis == -1.0, "filter state time = current plot time 1.1 s, last plot = current plot");
	}
	printf("[unit] Part 5 logic kept: velocity gate, track ID wrap\n");
	{
		static ST_TrkTable tb; ST_TrkPlot p[3]; ST_TrkScanLog lg; int i;
		f_Trk_Init(&tb, NULL); memset(p, 0, sizeof(p));
		p[0].pos[0] = 0; p[0].PlotId = 10000; f_Trk_ProcessScan(&tb, p, 1, 0.0, 0, &lg);
		p[0].pos[0] = 79.9; p[0].PlotId = 10001; f_Trk_ProcessScan(&tb, p, 1, 0.1, 1, &lg);
		CHECK(lg.nCreatedNum == 1 && tb.st_Track[0].TrackId == 100, "|dz|/T = 799 m/s < 800 -> track created, first ID 100");
		f_Trk_Init(&tb, NULL);
		p[0].pos[0] = 0; f_Trk_ProcessScan(&tb, p, 1, 0.0, 0, &lg);
		p[0].pos[0] = 80.1; f_Trk_ProcessScan(&tb, p, 1, 0.1, 1, &lg);
		CHECK(lg.nCreatedNum == 0 && lg.nPairNum == 1 && lg.nGatePassNum == 0, "|dz|/T = 801 m/s >= 800 -> rejected");
		f_Trk_Init(&tb, NULL); tb.nextTrackId = 9999;
		for (i = 0; i < 3; i++) { p[i].pos[1] = i * 5000.0; p[i].cov[0][0] = p[i].cov[1][1] = p[i].cov[2][2] = 100.0; p[i].PlotId = 20000 + i; }
		f_Trk_ProcessScan(&tb, p, 3, 0.0, 0, &lg);
		for (i = 0; i < 3; i++) { p[i].pos[0] = 10.0; p[i].PlotId = 20010 + i; p[i].time = 0.1; }
		f_Trk_ProcessScan(&tb, p, 3, 0.1, 1, &lg);
		CHECK(tb.nTrackNum == 3 && tb.st_Track[0].TrackId == 9999 && tb.st_Track[1].TrackId == 10000 && tb.st_Track[2].TrackId == 100, "3 targets: IDs 9999 -> 10000 -> 100 (wrap)");
		for (i = 0; i < 3; i++) { p[i].pos[0] = 20.0; p[i].PlotId = 20020 + i; p[i].time = 0.2; }
		f_Trk_ProcessScan(&tb, p, 3, 0.2, 2, &lg);
		CHECK(lg.nUpdatedNum == 3 && lg.nCreatedNum == 0 && lg.nPairNum == 0 && tb.nTrackNum == 3, "next scan: each track takes its own plot, no plot left for 2-point init");
	}
}

/* ---------------------------------------------------------------- scenario runs */

typedef struct { int scn; ST_SimSample *smp; int nSample; ST_PcsScan *pcs; int nScan; } SIM;

static void setobj(const OBJ *o, STRUCT_Coord_Lla *lla, STRUCT_Coord_Attitude *att, double *spd)
{
	lla->Lat = f_Deg_To_Rad(o->lat); lla->Lon = f_Deg_To_Rad(o->lon); lla->Alt = o->alt; *spd = o->speed;
	att->Roll = f_Deg_To_Rad(o->roll); att->Pitch = f_Deg_To_Rad(o->pitch); att->Yaw = f_Deg_To_Rad(o->yaw);
}

static void sim_build(SIM *s, int scn)
{
	static ST_SimConfig cfg; static ST_SimState sim; int i;
	memset(&cfg, 0, sizeof(cfg)); cfg.durationTime = SCN_DURATION_TIME; cfg.stepTime = SCN_STEP_TIME; cfg.nTargetNum = 1;
	setobj(&s_Platform, &cfg.st_Platform.st_InitLla, &cfg.st_Platform.st_InitAtt, &cfg.st_Platform.headingSpeed);
	setobj(&s_Target[scn], &cfg.st_Target[0].st_InitLla, &cfg.st_Target[0].st_InitAtt, &cfg.st_Target[0].headingSpeed);
	for (i = 0; i < 10; i++) if (s_Man[i].tgt == scn) { ST_TargetInit *t = &cfg.st_Target[0]; t->st_Maneuver[t->nManeuverNum++] = s_Man[i].m; }
	f_Tgt_InitSim(&sim, &cfg);
	s->scn = scn; s->nSample = sim.nStepNum + 1; s->nScan = sim.nStepNum;
	s->smp = (ST_SimSample *)malloc(sizeof(ST_SimSample) * (size_t)s->nSample);
	s->pcs = (ST_PcsScan *)malloc(sizeof(ST_PcsScan) * (size_t)s->nScan);
	s->smp[0] = sim.st_Sample;
	for (i = 0; i < sim.nStepNum; i++) { f_Tgt_StepSim(&sim); s->smp[i + 1] = sim.st_Sample; }
}

static int in_turn(int scn, double t)
{
	int i;
	for (i = 0; i < 10; i++) if (s_Man[i].tgt == scn && t >= s_Man[i].m.startTime && t < s_Man[i].m.endTime) return 1;
	return 0;
}

typedef struct {
	int nProc, nLost, nIds, created, deleted, firstInit, firstConfirm, divergeNum, maxRun, nSteady, nNis, nGateOut, nTurn, nStraight;
	double sumSq, maxErr, sumNis, sumNees, sumSqTurn, sumSqStraight, maxErrTurn;
	double rms, rmsTurn, rmsStraight, meanNis, gateOutPct, meanNees, initTime, confirmTime;
} MET;

static void metrics_finish(MET *m)
{
	m->rms = (m->nSteady > 0) ? sqrt(m->sumSq / m->nSteady) : NAN;
	m->rmsTurn = (m->nTurn > 0) ? sqrt(m->sumSqTurn / m->nTurn) : NAN;
	m->rmsStraight = (m->nStraight > 0) ? sqrt(m->sumSqStraight / m->nStraight) : NAN;
	m->meanNis = (m->nNis > 0) ? m->sumNis / m->nNis : NAN;
	m->gateOutPct = (m->nNis > 0) ? 100.0 * m->nGateOut / m->nNis : NAN;
	m->meanNees = (m->nSteady > 0) ? m->sumNees / m->nSteady : NAN;
	m->initTime = m->firstInit * SCN_STEP_TIME; m->confirmTime = m->firstConfirm * SCN_STEP_TIME;
}

static int run_link(const SIM *s, const ST_TrkParam *prm, ST_LnkStatus *st)
{
	if (!f_Lnk_Start(s->pcs, s->smp, s->nScan, 0, prm)) return 0;
	do { usleep(1000); f_Lnk_GetStatus(st); } while (st->isRunning);
	f_Lnk_Stop(); f_Lnk_GetStatus(st);
	return 1;
}

/* collect results of the last link run; dir != NULL writes CSVs */
static void collect(const SIM *s, MET *m, const char *dir)
{
	static ST_TrkView tv[TRK_MAX_TRACK]; static ST_LnkPlot lp[IDD_MAX_PLOT]; ST_LnkScan ls;
	unsigned ids[64]; int nIds = 0, i, k, q;
	FILE *fp = NULL, *ft = NULL, *fl = NULL, *fr = NULL; char path[600];

	memset(m, 0, sizeof(*m)); m->firstInit = -1; m->firstConfirm = -1;
	if (dir) {
		mkdir(dir, 0755);
		snprintf(path, sizeof(path), "%s/truth.csv", dir); fr = fopen(path, "w");
		fprintf(fr, "step,time,north,east,down,lat_deg,lon_deg,alt_m,speed,turn,r_ant,az_ant_deg,el_ant_deg\n");
		for (i = 0; i < s->nSample; i++) {
			const ST_TargetState *rd = &s->smp[i].st_Platform, *tg = &s->smp[i].st_Target[0];
			STRUCT_Coord_Rect n = f_Tgt_EcefToNed(&tg->st_PosEcef, &rd->st_PosEcef, &rd->st_Lla);
			double sp = sqrt(tg->st_VelEcef.x * tg->st_VelEcef.x + tg->st_VelEcef.y * tg->st_VelEcef.y + tg->st_VelEcef.z * tg->st_VelEcef.z);
			fprintf(fr, "%d,%.3f,%.3f,%.3f,%.3f,%.9f,%.9f,%.3f,%.3f,%d,", i, s->smp[i].simTime, n.x, n.y, n.z, f_Rad_To_Deg(tg->st_Lla.Lat), f_Rad_To_Deg(tg->st_Lla.Lon),
				tg->st_Lla.Alt, sp, in_turn(s->scn, s->smp[i].simTime));
			/* 안테나 좌표 참값은 스캔이 있는 표본만 (마지막 표본 116.0 s 는 스캔이 없어 빈 칸) */
			if (i < s->nScan) fprintf(fr, "%.3f,%.5f,%.5f\n", s->pcs[i].st_True[0].r, f_Rad_To_Deg(s->pcs[i].st_True[0].az), f_Rad_To_Deg(s->pcs[i].st_True[0].el));
			else fprintf(fr, ",,\n");
		}
		fclose(fr);
		snprintf(path, sizeof(path), "%s/plots.csv", dir); fp = fopen(path, "w"); fprintf(fp, "scan,time,plot_id,source,north,east,down,lat_deg,lon_deg,alt_m\n");
		snprintf(path, sizeof(path), "%s/tracks.csv", dir); ft = fopen(path, "w");
		fprintf(ft, "scan,time,track_id,state,score,memory,flag,init_scan,plot_last,update_num,north,east,down,vn,ve,vd,an,ae,ad,psn,pse,psd,vsn,vse,vsd,asn,ase,asd,"
			"lat_deg,lon_deg,alt_m,true_north,true_east,true_down,err_n,err_e,err_d,err,nis,nees,gate_run,gate_sum,diverge_num,innov_n,innov_e,innov_d\n");
		snprintf(path, sizeof(path), "%s/scanlog.csv", dir); fl = fopen(path, "w");
		fprintf(fl, "scan,time,plots,updated,gate_out,diverged,cand,pairs,gate_pass,created,confirmed,memory,deleted,tracks,msg_size\n");
	}

	for (i = 0; f_Lnk_CopyScan(i, &ls, tv, lp); i++) {
		const ST_TargetState *rd = &s->smp[ls.ScanNum].st_Platform, *tg = &s->smp[ls.ScanNum].st_Target[0];
		const STRUCT_Coord_Rect tn = f_Tgt_EcefToNed(&tg->st_PosEcef, &rd->st_PosEcef, &rd->st_Lla);
		const double tru[3] = { tn.x, tn.y, tn.z };
		m->nProc++;
		m->divergeNum += ls.st_Log.nDivergedNum;
		if (ls.st_Log.nCreatedNum > 0 && m->firstInit < 0) m->firstInit = (int)ls.ScanNum;
		m->created += ls.st_Log.nCreatedNum; m->deleted += ls.st_Log.nDeletedNum;
		if (fp) for (k = 0; k < ls.nPlotNum; k++) {
			STRUCT_Coord_Rect n = { lp[k].pos[0], lp[k].pos[1], lp[k].pos[2] }; STRUCT_Coord_Lla l = f_Tgt_NedToLla(&n, &rd->st_PosEcef, &rd->st_Lla);
			fprintf(fp, "%u,%.3f,%u,%d,%.3f,%.3f,%.3f,%.9f,%.9f,%.3f\n", ls.ScanNum, ls.time, lp[k].PlotId, f_Lnk_GetPlotSource(lp[k].PlotId), lp[k].pos[0], lp[k].pos[1], lp[k].pos[2],
				f_Rad_To_Deg(l.Lat), f_Rad_To_Deg(l.Lon), l.Alt);
		}
		for (k = 0; k < ls.nTrackNum; k++) {
			const ST_TrkView *v = &tv[k]; double e[3], ci[3][3], err, nees; int seen = 0;
			for (q = 0; q < 3; q++) e[q] = v->pos[q] - tru[q];
			err = sqrt(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
			inv3(v->posCov, ci); nees = quad3(e, ci);
			for (q = 0; q < nIds; q++) if (ids[q] == v->TrackId) seen = 1;
			if (!seen && nIds < 64) ids[nIds++] = v->TrackId;
			if (v->TrackState == TRK_STATE_CONFIRMED && m->firstConfirm < 0) m->firstConfirm = (int)ls.ScanNum;
			if (v->nGateOutRun > m->maxRun) m->maxRun = v->nGateOutRun;
			if (v->nUpdateNum >= STEADY_UPDATES) {
				m->nSteady++; m->sumSq += err * err; m->sumNees += nees; if (err > m->maxErr) m->maxErr = err;
				if (in_turn(s->scn, ls.time)) { m->nTurn++; m->sumSqTurn += err * err; if (err > m->maxErrTurn) m->maxErrTurn = err; }
				else { m->nStraight++; m->sumSqStraight += err * err; }
				if (v->nis >= 0) { m->nNis++; m->sumNis += v->nis; if (v->nis > TRK_GATE_CHI2) m->nGateOut++; }
			}
			if (ft) {
				STRUCT_Coord_Rect n = { v->pos[0], v->pos[1], v->pos[2] }; STRUCT_Coord_Lla l = f_Tgt_NedToLla(&n, &rd->st_PosEcef, &rd->st_Lla);
				fprintf(ft, "%u,%.3f,%u,%d,%d,%d,%d,%d,%u,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.4f,%.4f,%.4f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.4f,%.4f,%.4f,%.9f,%.9f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.4f,%.4f,%d,%d,%d,%.3f,%.3f,%.3f\n",
					ls.ScanNum, ls.time, v->TrackId, v->TrackState, v->TrackScore, v->MemoryScore, v->Assigned_Flag, v->nInitScan, v->PlotIdLast, v->nUpdateNum,
					v->pos[0], v->pos[1], v->pos[2], v->vel[0], v->vel[1], v->vel[2], v->acc[0], v->acc[1], v->acc[2], v->posStd[0], v->posStd[1], v->posStd[2],
					v->velStd[0], v->velStd[1], v->velStd[2], v->accStd[0], v->accStd[1], v->accStd[2], f_Rad_To_Deg(l.Lat), f_Rad_To_Deg(l.Lon), l.Alt,
					tru[0], tru[1], tru[2], e[0], e[1], e[2], err, v->nis, nees, v->nGateOutRun, v->nGateOutSum, v->nDivergeNum, v->innov[0], v->innov[1], v->innov[2]);
			}
		}
		if (fl) fprintf(fl, "%u,%.3f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%u\n", ls.ScanNum, ls.time, ls.st_Log.nPlotNum, ls.st_Log.nUpdatedNum, ls.st_Log.nGateOutNum, ls.st_Log.nDivergedNum,
			ls.st_Log.nCandidateNum, ls.st_Log.nPairNum, ls.st_Log.nGatePassNum, ls.st_Log.nCreatedNum, ls.st_Log.nConfirmedNum, ls.st_Log.nMemoryNum, ls.st_Log.nDeletedNum, ls.nTrackNum, ls.MsgSize);
	}
	m->nIds = nIds;
	if (fp) fclose(fp);
	if (ft) fclose(ft);
	if (fl) fclose(fl);
	metrics_finish(m);
}

static void print_met(const char *name, const MET *m)
{
	printf("  %-12s ids %d (created %d, deleted %d), init %.1f s, confirm %.1f s | RMS %.1f m (turn %.1f, straight %.1f), max %.1f m (turn %.1f) | NIS %.2f, gate-out %.1f %%, max run %d, divergence %d | NEES %.2f\n",
		name, m->nIds, m->created, m->deleted, m->initTime, m->confirmTime, m->rms, m->rmsTurn, m->rmsStraight, m->maxErr, m->maxErrTurn, m->meanNis, m->gateOutPct, m->maxRun, m->divergeNum, m->meanNees);
}

static void runs(const char *out)
{
	static const char *setKey[2] = { "given", "tuned" };
	char dir[600], path[600]; int scn, set; FILE *fr;
	snprintf(path, sizeof(path), "%s/runs.csv", out); fr = fopen(path, "w");
	fprintf(fr, "run,scenario,param,sigma_r,sigma_az_deg,sigma_el_deg,q,tau,proc,lost,ids,created,deleted,init_time,confirm_time,rms,rms_turn,rms_straight,max_err,max_err_turn,mean_nis,gate_out_pct,max_run,divergence,mean_nees\n");
	printf("[runs] 1 target, no clutter, SNR %.0f, over loopback UDP (PlotLink.c)\n", SCN_SNR);
	for (scn = 0; scn < SCN_NUM; scn++) {
		SIM s; sim_build(&s, scn);
		f_Pcs_Measure(s.pcs, s.smp, s.nScan, SCN_SNR, SCN_CLUTTER_NUM);
		{ /* actual plot noise of this realization */
			double sr = 0, sa = 0, se = 0; int i;
			for (i = 0; i < s.nScan; i++) { double dr = s.pcs[i].st_Plot[0].r - s.pcs[i].st_True[0].r, da = s.pcs[i].st_Plot[0].az - s.pcs[i].st_True[0].az, de = s.pcs[i].st_Plot[0].el - s.pcs[i].st_True[0].el;
				sr += dr * dr; sa += da * da; se += de * de; }
			printf("  %s: %d scans, plot noise std r %.2f m, az %.3f deg, el %.3f deg (model %.2f m, %.3f, %.3f)\n", s_ScnKey[scn], s.nScan, sqrt(sr / s.nScan), f_Rad_To_Deg(sqrt(sa / s.nScan)),
				f_Rad_To_Deg(sqrt(se / s.nScan)), f_Pcs_CalcSigma(SCN_SNR).r, f_Rad_To_Deg(f_Pcs_CalcSigma(SCN_SNR).az), f_Rad_To_Deg(f_Pcs_CalcSigma(SCN_SNR).el));
		}
		for (set = 0; set < 2; set++) {
			ST_TrkParam prm; ST_LnkStatus st; MET m; char name[64];
			if (set == 0) f_Trk_DefaultParam(&prm); else prm = s_Tuned[scn];
			snprintf(name, sizeof(name), "%s_%s", s_ScnKey[scn], setKey[set]); snprintf(dir, sizeof(dir), "%s/%s", out, name);
			if (!run_link(&s, &prm, &st)) { CHECK(0, "%s: link start", name); continue; }
			collect(&s, &m, dir);
			CHECK(st.nSentNum == s.nScan && st.nRecvNum == s.nScan && st.nLostNum == 0 && st.nProcNum == s.nScan && st.errorCode == 0, "%s: all %d messages sent / received / processed", name, s.nScan);
			print_met(name, &m);
			fprintf(fr, "%s,%s,%s,%g,%g,%g,%g,%g,%d,%d,%d,%d,%d,%.1f,%.1f,%.3f,%.3f,%.3f,%.3f,%.3f,%.4f,%.3f,%d,%d,%.4f\n", name, s_ScnKey[scn], setKey[set], prm.sigmaRange, prm.sigmaAzimuth,
				prm.sigmaElevation, prm.singerQ, prm.singerTau, m.nProc, st.nLostNum, m.nIds, m.created, m.deleted, m.initTime, m.confirmTime, m.rms, m.rmsTurn, m.rmsStraight, m.maxErr,
				m.maxErrTurn, m.meanNis, m.gateOutPct, m.maxRun, m.divergeNum, m.meanNees);
			if (set == 1) CHECK(m.divergeNum == 0 && m.nIds == 1 && m.deleted == 0, "%s: one track kept to the end, no divergence judgement", name);
			if (set == 0 && scn == SCN_AIR) CHECK(m.divergeNum > 0, "%s: divergence judged with 과제 값 (q 0.5 vs 4 G turns)", name);
		}
		free(s.smp); free(s.pcs);
	}
	fclose(fr);
}

/* ---------------------------------------------------------------- Monte Carlo sweep */

static void sweep(const char *out, int nSeed)
{
	static const double qs[] = { 0.1, 0.25, 0.5, 1, 2, 5, 10, 20, 40, 80 };
	static const double sigs[] = { 0.3, 0.5 };
	static const double taus[] = { 1, 2, 5, 10, 20, 60 };
	char path[600]; FILE *f; int scn, a, b, k, nRun = 0;
	snprintf(path, sizeof(path), "%s/sweep.csv", out); f = fopen(path, "w");
	fprintf(f, "scenario,sweep,sigma_az_deg,q,tau,seeds,rms,rms_sd,rms_turn,rms_straight,max_err,max_err_turn,mean_nis,gate_out_pct,max_run,divergence,diverged_seeds,mean_nees,ids,init_time\n");
	printf("[sweep] %d seeds per setting (seed k = %d + 1000 k, k = 0 is the program's)\n", nSeed, PCS_SEED);
	for (scn = 0; scn < SCN_NUM; scn++) {
		SIM s; sim_build(&s, scn);
		for (a = 0; a < 2; a++) {
			const int nB = (a == 0) ? (int)(sizeof(qs) / sizeof(qs[0])) * 2 : (int)(sizeof(taus) / sizeof(taus[0]));
			for (b = 0; b < nB; b++) {
				ST_TrkParam prm; MET acc; double sumRms = 0, sumRms2 = 0, sumTurn = 0, sumStr = 0, sumMax = 0, sumMaxT = 0, sumNis = 0, sumGo = 0, sumRun = 0, sumNees = 0, sumIds = 0, sumInit = 0;
				int sumDiv = 0, divSeeds = 0;
				f_Trk_DefaultParam(&prm);
				if (a == 0) { prm.sigmaAzimuth = prm.sigmaElevation = sigs[b / (nB / 2)]; prm.singerQ = qs[b % (nB / 2)]; }
				else { prm = s_Tuned[scn]; prm.singerTau = taus[b]; }
				for (k = 0; k < nSeed; k++) {
					ST_LnkStatus st; MET m;
					f_Pcs_MeasureSeed(s.pcs, s.smp, s.nScan, SCN_SNR, SCN_CLUTTER_NUM, (INT64)PCS_SEED + 1000LL * k);
					if (!run_link(&s, &prm, &st) || st.nProcNum != s.nScan) { CHECK(0, "sweep link run"); continue; }
					collect(&s, &m, NULL); nRun++;
					sumRms += m.rms; sumRms2 += m.rms * m.rms; sumTurn += m.rmsTurn; sumStr += m.rmsStraight; sumMax += m.maxErr; sumMaxT += m.maxErrTurn; sumNis += m.meanNis;
					sumGo += m.gateOutPct; sumRun += m.maxRun; sumNees += m.meanNees; sumIds += m.nIds; sumInit += m.initTime; sumDiv += m.divergeNum; divSeeds += (m.divergeNum > 0);
				}
				memset(&acc, 0, sizeof(acc));
				{
					const double n = nSeed, mr = sumRms / n;
					fprintf(f, "%s,%s,%g,%g,%g,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.4f,%.3f,%.2f,%.3f,%d,%.4f,%.3f,%.2f\n", s_ScnKey[scn], (a == 0) ? "q_sigma" : "tau", prm.sigmaAzimuth, prm.singerQ, prm.singerTau,
						nSeed, mr, sqrt(fmax(sumRms2 / n - mr * mr, 0)), sumTurn / n, sumStr / n, sumMax / n, sumMaxT / n, sumNis / n, sumGo / n, sumRun / n, sumDiv / n, divSeeds, sumNees / n, sumIds / n, sumInit / n);
					printf("  %-4s sig %.1f q %5.2f tau %4.0f: RMS %6.1f (turn %6.1f) max %6.1f | NIS %6.2f gate-out %5.1f %% maxrun %6.1f | divergence %.2f/run (%2d/%d seeds) | NEES %6.2f\n",
						s_ScnKey[scn], prm.sigmaAzimuth, prm.singerQ, prm.singerTau, mr, sumTurn / n, sumMax / n, sumNis / n, sumGo / n, sumRun / n, sumDiv / n, divSeeds, nSeed, sumNees / n);
					fflush(stdout);
				}
			}
		}
		free(s.smp); free(s.pcs);
	}
	fclose(f);
	printf("  %d link runs\n", nRun);
}

int main(int argc, char **argv)
{
	const char *out = (argc > 1) ? argv[1] : ".";
	const char *mode = (argc > 2) ? argv[2] : "all";
	const int nSeed = (argc > 3) ? atoi(argv[3]) : SWEEP_SEEDS;
	const int all = (strcmp(mode, "all") == 0);

	mkdir(out, 0755);
	printf("[icd] sizeof(STRUCT_PlotInfo) %zu, head %u, msg with 1 plot %u B\n", sizeof(STRUCT_PlotInfo), (unsigned)IDD_MSG_HEAD_SIZE, (unsigned)(IDD_MSG_HEAD_SIZE + sizeof(STRUCT_PlotInfo)));
	if (all || strcmp(mode, "unit") == 0) { test_singer(); test_kf(); test_part5_logic(); }
	if (all || strcmp(mode, "runs") == 0) runs(out);
	if (all || strcmp(mode, "sweep") == 0) sweep(out, nSeed);
	printf("FAILURES: %d\n", g_fail);
	return g_fail ? 2 : 0;
}
