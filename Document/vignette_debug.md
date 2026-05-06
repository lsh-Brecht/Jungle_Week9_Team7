# Vignette 및 Fade 시스템 디버깅 리포트

## 1. 개요
현재 KraftonEngine 내에서 플레이어 사망 시 **순차적 연출(Vignette -> Fade Out)**을 구현하고자 하나, Vignette 효과가 화면에 출력되지 않는 문제 발생. 반면 Fade Out 효과는 정상적으로 작동함.

## 2. 현재까지의 수정 사항

### 2.1 엔진 및 렌더러 최적화
- **`CameraTypes.h`**: `VignetteIntensity` 기본값을 `0.2`에서 `1.0`(효과 없음)으로 변경하여 상시 렌더링 방지.
- **`DrawCommandBuilder.cpp`**: 
    - `VignetteIntensity >= 0.99`인 경우 드로우 콜 생성을 건너뛰도록 최적화.
    - `FadeAlpha <= 0.001`인 경우 드로우 콜 생성을 건너뛰도록 최적화.
- **`GameClientSettings.cpp`**: `ApplyRuntimeDefaults`에서 `bVignette`, `bFade` ShowFlags를 명시적으로 `true`로 설정.

### 2.2 Lua 스크립트 로직 (`Player.lua`)
- **순차 연출 구현**:
    - **Phase 1 (0.5s)**: Vignette 강도를 `1.0`에서 `0.1`로 선형 보간 (붉은색).
    - **Phase 2 (1.5s)**: Vignette 유지 및 `StartFadeIn` 호출을 통한 화면 암전.
- **카메라 인스턴스 동기화**: `Player.camera`를 `PlayerController:GetViewCamera()`를 통해 매 프레임 갱신하여 렌더러와 동일한 객체를 참조하도록 수정.

## 3. 점검 및 진단 결과

### 3.1 로그 분석 결과
- **Lua 로그**: `Intensity` 값이 `1.0 -> 0.1`로 정상적으로 감소함을 확인.
- **Renderer 로그**: 
    - 초기 진단 시 Renderer의 `Intensity`가 `1.00`으로 고정되는 현상 발견.
    - UUID 대조 결과 Lua와 Renderer가 서로 다른 카메라 객체를 보고 있었음 (Mismatch 확인).
    - **수정 후**: UUID가 일치하게 되었으나, 여전히 시각적 변화 없음.

### 3.2 셰이더 및 리소스 체크
- **`Vignette.hlsl`**: 강제로 빨간색을 출력하게 했으나 화면에 나타나지 않음.
- **`PostProcessPass.cpp`**: `StencilCopySRV` 등 필수 리소스 누락 시 로그를 남기도록 했으나, "Skipping pass" 로그는 출력되지 않음 (패스 자체는 실행 중).

## 4. 핵심 미스터리: 왜 Fade는 되고 Vignette는 안 되는가?

| 항목 | Vignette | Fade |
| :--- | :--- | :--- |
| **데이터 위치** | `UCameraComponent` 내부 `PostProcess` 구조체 | `APlayerCameraManager` 전역 상태 |
| **조작 방식** | Lua에서 카메라 속성 직접 수정 | `PlayerController`를 통한 함수 호출 |
| **렌더링 방식** | 풀스크린 삼각형 + AlphaBlend | 풀스크린 삼각형 + AlphaBlend |
| **현재 상태** | **작동 안 함** | **정상 작동** |

### 추정 원인
1. **상수 버퍼 바인딩 슬롯 충돌**: Vignette와 Fade 모두 `b2` 슬롯을 사용하려 할 때, `DrawCommandBuilder`에서 명령 생성 순서나 바인딩 로직에 의해 덮어씌워질 가능성.
2. **카메라 데이터 전달 시점**: `APlayerCameraManager`가 매 프레임 카메라 속성을 업데이트(UpdateCamera)할 때, Lua에서 수정한 값이 엔진 내부의 보간(Lerp) 로직이나 기본값에 의해 다시 `1.0`으로 덮어씌워지고 있을 가능성.
3. **블렌딩 상태**: `PostProcess` 패스의 `AlphaBlend` 설정이 특정 상황에서 Vignette의 마스크 값을 무시하고 있을 가능성.

## 5. 향후 계획
1. `APlayerCameraManager::UpdateCamera` 로직에서 `PostProcess` 설정이 어떻게 병합(Merge)되는지 심층 분석.
2. `DrawCommandBuilder`에서 동일한 상수 버퍼 오브젝트(`VignetteCB`, `FadeCB`)가 안전하게 독립적으로 업데이트되고 바인딩되는지 재검토.
3. 셰이더 바인딩 슬롯(`b2`)을 각기 다른 번호로 할당하여 충돌 가능성 제거 테스트.
