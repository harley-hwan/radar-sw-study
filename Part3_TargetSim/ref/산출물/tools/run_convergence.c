/* 스텝 간격을 바꿔 가며 60 s 뒤 위치를 비교한다 (중점법 차수 확인). */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "TargetSim.h"

static void set_obj(STRUCT_Coord_Lla *lla, STRUCT_Coord_Attitude *att,
                    double latDeg, double lonDeg, double alt, double yawDeg)
{
    lla->Lat = f_Deg_To_Rad(latDeg); lla->Lon = f_Deg_To_Rad(lonDeg); lla->Alt = alt;
    att->Roll = 0.0; att->Pitch = 0.0; att->Yaw = f_Deg_To_Rad(yawDeg);
}

static int final_state(double step, int withTurn, STRUCT_Coord_Rect *pos, STRUCT_Coord_Lla *lla)
{
    ST_SimConfig cfg; ST_SimState sim; EN_TgtStatus st;
    memset(&cfg, 0, sizeof(cfg));
    cfg.durationTime = 60.0; cfg.stepTime = step;
    set_obj(&cfg.st_Platform.st_InitLla, &cfg.st_Platform.st_InitAtt, 32.0, 126.0, 0.0, 0.0);
    cfg.st_Platform.headingSpeed = 0.0;
    cfg.nTargetNum = 1;
    set_obj(&cfg.st_Target[0].st_InitLla, &cfg.st_Target[0].st_InitAtt, 32.12, 126.0, 300.0, 180.0);
    cfg.st_Target[0].headingSpeed = 200.0;
    if (withTurn) {
        cfg.st_Target[0].nManeuverNum = 1;
        cfg.st_Target[0].st_Maneuver[0].enTurnType = TGT_TURN_YAW;
        cfg.st_Target[0].st_Maneuver[0].gravityValue = 2.0;
        cfg.st_Target[0].st_Maneuver[0].startTime = 10.0;
        cfg.st_Target[0].st_Maneuver[0].endTime = 30.0;
    }
    st = f_Tgt_InitSim(&sim, &cfg);
    if (st != TGT_OK) { fprintf(stderr, "init %s\n", f_Tgt_StatusStr(st)); return 1; }
    while ((st = f_Tgt_StepSim(&sim)) == TGT_OK) { }
    if (st != TGT_ERR_SIM_END) { fprintf(stderr, "step %s\n", f_Tgt_StatusStr(st)); return 1; }
    *pos = sim.st_Sample.st_Target[0].st_PosEcef;
    *lla = sim.st_Sample.st_Target[0].st_Lla;
    return 0;
}

int main(void)
{
    const double steps[] = { 1.0, 0.5, 0.25, 0.2, 0.1, 0.05, 0.025, 0.01 };
    STRUCT_Coord_Rect ref, p; STRUCT_Coord_Lla rl, l;
    int turn, i;

    for (turn = 0; turn <= 1; turn++) {
        if (final_state(0.001, turn, &ref, &rl)) return 1;
        printf("# %s  (기준 dt = 0.001 s)\n", turn ? "Yaw 2G 선회 20 s 포함" : "직진");
        printf("dt,err_m,alt_m\n");
        for (i = 0; i < (int)(sizeof(steps)/sizeof(steps[0])); i++) {
            if (final_state(steps[i], turn, &p, &l)) return 1;
            printf("%g,%.6e,%.6f\n", steps[i],
                   sqrt((p.x-ref.x)*(p.x-ref.x) + (p.y-ref.y)*(p.y-ref.y) + (p.z-ref.z)*(p.z-ref.z)),
                   l.Alt);
        }
        printf("\n");
    }
    return 0;
}
