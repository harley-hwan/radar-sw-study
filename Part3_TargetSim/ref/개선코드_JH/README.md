# 개선본 (_JH) 백업

과제 3 본체는 `ref/참고 소스 코드/` 의 좌표변환 코드를 **원본 그대로** 쓰도록
되돌렸다. 이 폴더는 그 전에 쓰던 개선본을 그대로 보관한 것이다.

## 보관 파일

| 파일 | 원본 대응 | 개선한 점 |
|---|---|---|
| `Define_JH.h` | `Define.h` | 코딩룰이 요구하는 형 집합을 `typedef` 로. `VOID` 재정의 방지, `BOOL` 은 정의하지 않아 MFC 와 부딪히지 않음 |
| `matrixCalcLib_JH.h/.c` | `matrixCalcLib.h/.c` | 값 반환 → 출력 인자 + 상태 코드, 차원/특이행렬 검사, LU 분해 기반 역행렬 |
| `CoordinateTransform_JH.h/.c` | `CoordinateTransform.h/.c` | ECEF→LLA 를 5회 반복 근사 대신 Bowring 폐형식으로, DCM 분리, 벡터 전용 회전 함수, WGS84 상수 정정 |
| `TargetSim_JH.h/.c` | 없음 (자체 작성) | 위 개선본 API 를 호출하던 판. 지금 본체의 것은 원본 API 를 호출하도록 고친 판이다 |

## 되돌리는 방법

1. 이 폴더의 7개 파일을 `src/TargetSim/TargetSimCore/include` 와 `src` 로 복사
2. `include` 에서 `Define.h`, `matrixCalcLib.h`, `CoordinateTransform.h` 삭제,
   `src` 에서 `matrixCalcLib.c`, `CoordinateTransform.c` 삭제
3. `TargetSimCore.vcxproj` 와 `.filters` 의 파일 목록을 `_JH` 이름으로 되돌린다
4. UI 쪽은 `git log` 에서 이 교체 커밋을 되돌리면 된다 (`git revert`)

git 이력에도 그대로 남아 있으므로 커밋 하나를 되돌리는 쪽이 더 간단하다.
