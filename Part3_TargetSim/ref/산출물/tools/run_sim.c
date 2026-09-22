/* 발표자료용 실행 하네스: 명세 / 기동시연 / 다표적 시나리오를 돌려 CSV 로 떨군다. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>
#include "TargetSim.h"

static void set_obj(STRUCT_Coord_Lla *lla, STRUCT_Coord_Attitude *att,
                    double latDeg, double lonDeg, double alt,
                    double rollDeg, double pitchDeg, double yawDeg)
{
    lla->Lat = f_Deg_To_Rad(latDeg);
    lla->Lon = f_Deg_To_Rad(lonDeg);
    lla->Alt = alt;
    att->Roll  = f_Deg_To_Rad(rollDeg);
    att->Pitch = f_Deg_To_Rad(pitchDeg);
    att->Yaw   = f_Deg_To_Rad(yawDeg);
}

static void add_mnv(ST_TargetInit *t, EN_TurnType type, double g, double t0, double t1)
{
    ST_TargetManeuver *m = &t->st_Maneuver[t->nManeuverNum++];
    m->enTurnType = type;
    m->gravityValue = g;
    m->startTime = t0;
    m->endTime = t1;
}

static int run(const char *path, ST_SimConfig *cfg)
{
    ST_SimState sim;
    FILE *fp;
    EN_TgtStatus st;
    int i, n;

    st = f_Tgt_InitSim(&sim, cfg);
    if (st != TGT_OK) { fprintf(stderr, "InitSim: %s\n", f_Tgt_StatusStr(st)); return 1; }

    fp = fopen(path, "w");
    if (!fp) { perror(path); return 1; }

    fprintf(fp, "step,time");
    fprintf(fp, ",pf_lat,pf_lon,pf_alt");
    for (i = 0; i < cfg->nTargetNum; i++)
        fprintf(fp, ",t%d_lat,t%d_lon,t%d_alt,t%d_yaw,t%d_vx,t%d_vy,t%d_vz,t%d_spd", i+1,i+1,i+1,i+1,i+1,i+1,i+1,i+1);
    fprintf(fp, "\n");

    for (n = 0; ; n++) {
        const ST_SimSample *s = &sim.st_Sample;
        fprintf(fp, "%d,%.4f", s->nStepIndex, s->simTime);
        fprintf(fp, ",%.12f,%.12f,%.9f",
                f_Rad_To_Deg(s->st_Platform.st_Lla.Lat),
                f_Rad_To_Deg(s->st_Platform.st_Lla.Lon),
                s->st_Platform.st_Lla.Alt);
        for (i = 0; i < cfg->nTargetNum; i++) {
            const ST_TargetState *g = &s->st_Target[i];
            double spd = sqrt(g->st_VelEcef.x*g->st_VelEcef.x + g->st_VelEcef.y*g->st_VelEcef.y + g->st_VelEcef.z*g->st_VelEcef.z);
            fprintf(fp, ",%.12f,%.12f,%.9f,%.6f,%.9f,%.9f,%.9f,%.12f",
                    f_Rad_To_Deg(g->st_Lla.Lat), f_Rad_To_Deg(g->st_Lla.Lon), g->st_Lla.Alt,
                    f_Rad_To_Deg(g->st_Att.Yaw),
                    g->st_VelEcef.x, g->st_VelEcef.y, g->st_VelEcef.z, spd);
        }
        fprintf(fp, "\n");

        st = f_Tgt_StepSim(&sim);
        if (st == TGT_ERR_SIM_END) break;
        if (st != TGT_OK) { fprintf(stderr, "StepSim @%d: %s\n", n, f_Tgt_StatusStr(st)); fclose(fp); return 1; }
    }
    fclose(fp);
    fprintf(stderr, "%s : %d 표본\n", path, n + 1);
    return 0;
}

int main(void)
{
    ST_SimConfig cfg;

    /* ── 1) 명세 시나리오 (과제 3 조건 그대로, 기동 없음) ── */
    memset(&cfg, 0, sizeof(cfg));
    cfg.durationTime = 60.0;
    cfg.stepTime = 0.1;
    set_obj(&cfg.st_Platform.st_InitLla, &cfg.st_Platform.st_InitAtt, 32.0, 126.0, 0.0, 0, 0, 0);
    cfg.st_Platform.headingSpeed = 0.0;
    cfg.nTargetNum = 2;
    set_obj(&cfg.st_Target[0].st_InitLla, &cfg.st_Target[0].st_InitAtt, 32.125, 126.03, 0.0, 0, 0, 270.0);
    cfg.st_Target[0].headingSpeed = 30.0;
    set_obj(&cfg.st_Target[1].st_InitLla, &cfg.st_Target[1].st_InitAtt, 32.12, 126.0, 300.0, 0, 0, 180.0);
    cfg.st_Target[1].headingSpeed = 200.0;
    if (run("spec.csv", &cfg)) return 1;

    /* ── 2) 기동 시연 (지그재그, 60 s) ── */
    add_mnv(&cfg.st_Target[0], TGT_TURN_YAW, -1.0, 8.0, 10.4);
    add_mnv(&cfg.st_Target[0], TGT_TURN_YAW,  1.0, 15.8, 18.2);
    add_mnv(&cfg.st_Target[0], TGT_TURN_YAW,  1.0, 41.9, 44.3);
    add_mnv(&cfg.st_Target[0], TGT_TURN_YAW, -1.0, 49.7, 52.1);
    add_mnv(&cfg.st_Target[1], TGT_TURN_YAW, -8.205060, 2.8, 6.7);
    add_mnv(&cfg.st_Target[1], TGT_TURN_YAW,  8.205060, 13.3, 17.2);
    add_mnv(&cfg.st_Target[1], TGT_TURN_YAW,  8.205060, 18.6, 22.5);
    add_mnv(&cfg.st_Target[1], TGT_TURN_YAW, -8.205060, 36.2, 40.1);
    add_mnv(&cfg.st_Target[1], TGT_TURN_YAW, -8.205060, 41.5, 45.4);
    add_mnv(&cfg.st_Target[1], TGT_TURN_YAW,  8.205060, 53.5, 60.0);
    if (run("demo.csv", &cfg)) return 1;

    /* ── 3) 단일 표적 Pitch / Roll 비교용 (Yaw / Pitch / Roll 각각 1 G) ── */
    memset(&cfg, 0, sizeof(cfg));
    cfg.durationTime = 60.0;
    cfg.stepTime = 0.1;
    set_obj(&cfg.st_Platform.st_InitLla, &cfg.st_Platform.st_InitAtt, 32.0, 126.0, 0.0, 0, 0, 0);
    cfg.st_Platform.headingSpeed = 0.0;
    cfg.nTargetNum = 3;
    set_obj(&cfg.st_Target[0].st_InitLla, &cfg.st_Target[0].st_InitAtt, 32.12, 126.0, 300.0, 0, 0, 180.0);
    cfg.st_Target[0].headingSpeed = 200.0;
    add_mnv(&cfg.st_Target[0], TGT_TURN_YAW, 2.0, 10.0, 20.0);
    set_obj(&cfg.st_Target[1].st_InitLla, &cfg.st_Target[1].st_InitAtt, 32.12, 126.0, 300.0, 0, 0, 180.0);
    cfg.st_Target[1].headingSpeed = 200.0;
    add_mnv(&cfg.st_Target[1], TGT_TURN_PITCH, 2.0, 10.0, 20.0);
    set_obj(&cfg.st_Target[2].st_InitLla, &cfg.st_Target[2].st_InitAtt, 32.12, 126.0, 300.0, 0, 0, 180.0);
    cfg.st_Target[2].headingSpeed = 200.0;
    add_mnv(&cfg.st_Target[2], TGT_TURN_ROLL, 2.0, 10.0, 20.0);
    if (run("turn.csv", &cfg)) return 1;

    return 0;
}
