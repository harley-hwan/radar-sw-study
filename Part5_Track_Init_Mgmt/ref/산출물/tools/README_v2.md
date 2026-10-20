# 과제 5 발표자료 2 판 만드는 순서

Windows (PowerShell), tools 폴더에서:

    # 1) 시뮬레이션 데이터 (data/*.csv). 이미 들어 있으므로 코드를 바꿨을 때만 다시 뽑는다.
    #    Windows 는 dump_fig_data.c 를 Core 소스와 같이 빌드해 실행, Linux/WSL 은 linux/linux_dump.c (UDP 없이 같은 처리)
    #    gcc -O1 -w -DTSCORE_API= -I linux/winstub -I <Core include+src> -o linux_dump linux/linux_dump.c <Core *.c 중 PlotLink.c 뺀 것> -lm
    #    ./linux_dump ../data 20

    # 2) 그림
    python figures.py ..\data ..\figures            # 개념 그림 (글꼴: 맑은 고딕. 없으면 FIG 글꼴을 Noto Sans CJK KR 로)
    python result_figs.py ..\data ..\figures 546    # 결과 화면 (res_view.png), 트랙 수명 띠 (res_life.png)

    # 3) 발표자료 (틀은 과제 4 발표자료, 노트는 notes_v2.py)
    python build_deck_v2.py ..\..\..\..\Part4_Plot_Clutter_Sim\ref\산출물\Chapter4_PlotClutterSim.pptx ..\figures ..\Chapter5_TrackInitMgmt.pptx

- 코드 발췌는 src/TargetSim 의 실제 파일에서 줄을 찾아 읽으므로 줄 번호가 늘 소스와 같다. 소스 위치가 다르면 환경변수 SRC 로 지정.
- 발표 대본 (Chapter5_발표대본.md) 은 notes_v2.py 에서 같이 만들어진다. 대본을 고치려면 notes_v2.py 를 고친다.
- 결과 수치 (트랙 355 개, 거리 구간 표 등) 는 data/*.csv 에서 나온 값이고, Windows 실행 파일 결과와 같다 (난수 생성기가 고정 SEED 의 LCG 라 플랫폼 영향 없음).
- 이전 판 (26 장) 과 그때의 그림 · 도구는 backup_20261020 폴더에 있다.
