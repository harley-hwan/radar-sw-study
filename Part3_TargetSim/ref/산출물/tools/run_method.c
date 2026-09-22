/* 적분 방법만 바꿔 60 s 뒤 위치를 비교한다.
 *
 * 같은 파일을 두 번 빌드한다.
 *   - 중점법판 : Core 의 TargetSim.c 원본을 그대로 링크
 *   - 오일러판 : f_Tgt_Propagate 에서 위치 갱신에 쓰는 속도만 스텝 시작 속도로
 *                되돌린 사본을 링크 (README 의 sed 한 줄)
 *
 * 출력은 dt 별 60 s 뒤 ECEF 좌표다. 두 판의 오차는 중점법 dt = 0.001 s 결과를
 * 기준으로 삼아 make_figures.py 에서 계산한다.
 */
#include <stdio.h>
#include <string.h>
#include "TargetSim.h"

static int final_pos(double step, STRUCT_Coord_Rect *pos)
{
	ST_SimConfig	cfg;
	ST_SimState		sim;
	EN_TgtStatus	st;

	memset(&cfg, 0, sizeof(cfg));
	cfg.durationTime = 60.0;
	cfg.stepTime = step;

	cfg.st_Platform.st_InitLla.Lat = f_Deg_To_Rad(32.0);
	cfg.st_Platform.st_InitLla.Lon = f_Deg_To_Rad(126.0);
	cfg.st_Platform.st_InitLla.Alt = 0.0;
	cfg.st_Platform.headingSpeed = 0.0;

	cfg.nTargetNum = 1;
	cfg.st_Target[0].st_InitLla.Lat = f_Deg_To_Rad(32.12);
	cfg.st_Target[0].st_InitLla.Lon = f_Deg_To_Rad(126.0);
	cfg.st_Target[0].st_InitLla.Alt = 300.0;
	cfg.st_Target[0].st_InitAtt.Yaw = f_Deg_To_Rad(180.0);
	cfg.st_Target[0].headingSpeed = 200.0;
	cfg.st_Target[0].nManeuverNum = 1;
	cfg.st_Target[0].st_Maneuver[0].enTurnType = TGT_TURN_YAW;
	cfg.st_Target[0].st_Maneuver[0].gravityValue = 2.0;
	cfg.st_Target[0].st_Maneuver[0].startTime = 10.0;
	cfg.st_Target[0].st_Maneuver[0].endTime = 30.0;

	st = f_Tgt_InitSim(&sim, &cfg);
	if (st != TGT_OK) { fprintf(stderr, "init %s\n", f_Tgt_StatusStr(st)); return 1; }
	while ((st = f_Tgt_StepSim(&sim)) == TGT_OK) { }
	if (st != TGT_ERR_SIM_END) { fprintf(stderr, "step %s\n", f_Tgt_StatusStr(st)); return 1; }

	*pos = sim.st_Sample.st_Target[0].st_PosEcef;
	return 0;
}

int main(void)
{
	const double	steps[] = { 1.0, 0.5, 0.25, 0.2, 0.1, 0.05, 0.025, 0.01, 0.001 };
	STRUCT_Coord_Rect	p;
	int				i;

	printf("dt,x,y,z\n");
	for (i = 0; i < (int)(sizeof(steps) / sizeof(steps[0])); i++) {
		if (final_pos(steps[i], &p)) return 1;
		printf("%g,%.9f,%.9f,%.9f\n", steps[i], p.x, p.y, p.z);
	}
	return 0;
}
