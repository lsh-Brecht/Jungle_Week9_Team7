# Phase 3: Lua 바인딩 전수조사 및 미비점 분석 (Gap Analysis)

본 문서는 `Vignette_Fade_Implementation_Spec.md`의 Phase 3(Lua 바인딩) 항목이 현재 프로젝트에 어느 정도 반영되었는지 전수조사한 결과를 기록합니다.

---

## 1. 전수조사 요약표

| 항목 | 구분 | 위치 | 상태 | 비고 |
| :--- | :--- | :--- | :---: | :--- |
| **VignetteIntensity** | Property | CameraComponent | **완료** | `sol::property`로 노출됨 |
| **VignetteColor** | Property | CameraComponent | **완료** | `sol::property`로 노출됨 |
| **VignetteCenter** | Property | CameraComponent | **누락** | Pawn 추적 외 수동 설정 기능 필요 |
| **VignetteSmoothness** | Property | CameraComponent | **누락** | 효과의 선명도 조절용 |
| **StartFadeIn** | Function | PlayerController | **완료*** | 호출 경로는 존재하나 내부 로직 점검 필요 |
| **StartFadeOut** | Function | PlayerController | **완료*** | 호출 경로는 존재하나 내부 로직 점검 필요 |
| **FadeColor/Alpha** | Property | CameraComponent | **누락** | 개별 카메라 단위 Fade 제어용 |

---

## 2. 상세 분석 및 문제점

### 2.1 CameraComponent 바인딩 미비 (`LuaCameraComponentBindings.cpp`)
현재 `VignetteIntensity`와 `VignetteColor`는 바인딩되어 있으나, 포스트 프로세스 효과를 정교하게 제어하기 위한 다음 항목들이 누락되었습니다.
- **VignetteCenter (FVector2)**: 특정 지점을 강조하고 싶을 때 필요한 좌표 바인딩.
- **VignetteSmoothness (float)**: Vignette 테두리의 부드러운 정도 조절.
- **FadeAlpha/FadeColor**: `PlayerController`를 통한 전역 페이드 외에, 특정 카메라로 전환 시 개별적으로 적용될 페이드 값 바인딩 누락.

### 2.2 PlayerController 페이드 로직의 불확실성 (`LuaPlayerControllerBindings.cpp`)
- 바인딩된 `StartFadeIn/Out`이 `APlayerCameraManager`의 함수를 호출합니다.
- `APlayerCameraManager`는 내부적으로 `UCameraFadeModifier`를 사용하도록 설계되어 있으나, 현재 `CameraFadeModifier.cpp`가 컴파일 에러 상태이므로 실제 동작이 검증되지 않았습니다.
- **수정 필요 사항**: Lua에서 호출 시 `EnsureFadeModifier()`를 통해 Modifier가 정상적으로 리스트에 등록되고 업데이트되는지 확인해야 합니다.

---

## 3. 향후 조치 사항 (To-Do)

1.  **C++ 컴파일 에러 해결 (최우선)**:
    - `CameraFadeModifier.cpp` 내의 상속 멤버 접근 문제 해결.
    - `ShadowMapVis.hlsl`의 `PS_Input_UV` 타입 정의 오류 해결.
2.  **누락된 바인딩 추가**:
    - `LuaCameraComponentBindings.cpp`에 `VignetteCenter`, `VignetteSmoothness` 추가.
3.  **페이드 동작 검증**:
    - Lua에서 `PlayerController:StartFadeIn(...)` 호출 시 실제 화면이 어두워지는지 렌더링 파이프라인(`DrawCommandBuilder`) 끝단까지 데이터가 전달되는지 확인.
