# Vignette Intensity 전파 결함 추가 진단 계획

## Context (업데이트)

이전 패치 — `PlayerCameraManager.cpp:280` 의 smoothing 브랜치에 `CurrentView.PostProcess = DesiredView.PostProcess;` 추가 — 적용 후에도 사망 Phase 1 동안 렌더러 측 `Frame.PostProcess.VignetteIntensity` 가 **1.00 으로 고정**됨이 확인됨 ([DrawCommandBuilder.cpp:466-470](KraftonEngine/Source/Engine/Render/Command/DrawCommandBuilder.cpp:466) 의 진단 로그로 검증).

부수 관측:
- Player.camera (Lua) UUID = **14716**
- OutputCamera (렌더러가 읽는 카메라) UUID = **13536**
- 두 UUID 의 차이는 설계상 정상 (Lua 는 ActiveCamera, 렌더러는 OutputCamera). 본 결함과 직접 무관.
- Vignette.hlsl:32 의 디버그 코드 `float4(float3(165, 42, 42), mask)` 는 raw sRGB 과포화로 해석상 무의미. 시각 부재의 직접 원인이 아님.

이전 패치만으로는 부족함이 입증됐으므로, Lua → 렌더러 사이의 PostProcess 전파 체인 어디에서 값이 1.00 으로 유지/리셋되는지 정확히 짚는 결정적 진단이 필요.

## 가능한 잔여 결함 위치

1. **빌드 캐시** — `PlayerCameraManager.obj` 가 갱신되지 않아 smoothing 패치가 실행 코드에 반영되지 않음.
2. **카메라 식별 불일치** — Lua 의 `Player.camera` (14716) 가 `Manager::ActiveCameraCached` 와 다른 객체 (예: Pawn 내부 별도 CameraComponent). Lua 의 `VignetteIntensity = 0.1` 은 14716 에 쓰이지만 `CalcCameraView` 는 다른 카메라의 PostProcess 를 읽음.
3. **smoothing 가 아닌 분기 진입** — `bIsBlending == true` 가 사망 사이클 동안 지속되면 `BlendViews` (line 537-543) 가 `BlendFromView.PostProcess` (구 카메라의 디폴트 1.00) 와 `DesiredView.PostProcess` (Lua 의 0.10) 사이를 lerp 하며, blend duration 이 길면 alpha 가 작아 결과가 거의 1.00 에 머물 수 있음.
4. **OutputCamera.PostProcess 외부 리셋** — `ApplyCameraView` 직후 `UpdateVignetteCenter` 가 `OutputCameraComponent->GetMutablePostProcess()` 를 호출. 코드 검토상 VignetteCenter 만 수정하지만, 다른 호출 경로에서 PostProcess 가 디폴트로 리셋될 가능성을 배제할 필요.
5. **ApplyCameraModifiers 부작용** — 어떤 modifier 가 InOutView.PostProcess 를 디폴트 구조체로 덮어씀. 현재 `UCameraFadeModifier` (FadeAlpha 만), `UCameraShakeModifier` (Vignette 무수정), 베이스 `UCameraModifier::ModifyCamera` (no-op) 모두 무관 — 검증 완료.

## Critical Files

- [PlayerCameraManager.cpp](KraftonEngine/Source/Engine/Camera/PlayerCameraManager.cpp) — 진단 로그 추가 위치 (UpdateCamera 끝).
- [LuaCameraComponentBindings.cpp](KraftonEngine/Source/Engine/Scripting/LuaCameraComponentBindings.cpp) — Lua VignetteIntensity setter 가 어떤 카메라 UUID 를 수정하는지 추가 로그.
- [DrawCommandBuilder.cpp:466-470](KraftonEngine/Source/Engine/Render/Command/DrawCommandBuilder.cpp:466) — 이미 활성화된 진단 로그 (수정 불요).

## 변경

### 변경 1 — PlayerCameraManager.cpp UpdateCamera 끝에 결정적 진단 로그 추가

`UpdateCamera` 함수의 마지막 (line 294 `UpdateVignetteCenter(TargetCamera);` 직전) 에 단 한 개의 로그.

```cpp
// [DEBUG] 매 30프레임마다 PostProcess 전파 체인의 각 단계 값을 출력.
// 진단 종료 후 제거.
static uint32 ChainLogCounter = 0;
if (ChainLogCounter++ % 30 == 0)
{
    UE_LOG("[CameraMgr] Tgt=%u TgtPP=%.2f Desired=%.2f Current=%.2f Final=%.2f Blend=%d Smooth=%d Out=%u OutPP=%.2f",
        TargetCamera ? TargetCamera->GetUUID() : 0,
        TargetCamera ? TargetCamera->GetPostProcess().VignetteIntensity : -1.0f,
        DesiredView.PostProcess.VignetteIntensity,
        CurrentView.PostProcess.VignetteIntensity,
        FinalView.PostProcess.VignetteIntensity,
        bIsBlending ? 1 : 0,
        Smoothing.bEnableSmoothing ? 1 : 0,
        OutputCameraComponent ? OutputCameraComponent->GetUUID() : 0,
        OutputCameraComponent ? OutputCameraComponent->GetPostProcess().VignetteIntensity : -1.0f);
}
```

이 로그 하나로 이전 가설 1~5 모두를 한 번에 검증 가능.

### 변경 2 — LuaCameraComponentBindings.cpp setter 에 짧은 검증 로그 추가

Lua 가 실제로 어떤 카메라 UUID 에 쓰는지 한 줄로 확인.

```cpp
[](const FLuaCameraComponentHandle& Self, float Value)
{
    UCameraComponent* C = Self.Resolve();
    if (!C) { UE_LOG("[Lua] Invalid CameraComponent.VignetteIntensity Access."); return; }
    static uint32 LuaSetCounter = 0;
    if (LuaSetCounter++ % 30 == 0)
    {
        UE_LOG("[Lua->Cam] UUID=%u VignetteIntensity := %.2f", C->GetUUID(), Value);
    }
    C->GetMutablePostProcess().VignetteIntensity = Value;
}
```

## 결과 해석 가이드 (사망 Phase 1, deathTimer ∈ (1.5, 2.0] 동안)

[Lua->Cam] 의 UUID 와 [CameraMgr] 의 Tgt UUID 비교, 각 값의 진행을 다음 표로 매칭:

| [Lua->Cam] UUID | [CameraMgr] Tgt UUID | TgtPP | Desired | Current | Final | OutPP | [DrawCmdBuilder] Intensity | 결론 |
|---|---|---|---|---|---|---|---|---|
| 14716 | 14716 | 1.0→0.1 | 1.0→0.1 | 1.0→0.1 | 1.0→0.1 | 1.0→0.1 | 1.00 (고정) | ApplyCameraView 후 OutputCamera 가 외부 경로에서 1.0 으로 리셋 — 추가 grep 필요 |
| 14716 | 14716 | 1.0→0.1 | 1.0→0.1 | 1.0 (고정) | 1.0 (고정) | 1.0 (고정) | 1.00 | smoothing 패치 미반영 → 클린 리빌드 |
| 14716 | 14716 | 1.0→0.1 | 1.0 (고정) | 1.0 | 1.0 | 1.0 | 1.00 | CalcCameraView 가 PostProcess 를 채우지 못함 (코드상 불가능, 그러면 다른 분기) |
| 14716 | 14716 | 1.0 (고정) | 1.0 | 1.0 | 1.0 | 1.0 | 1.00 | Lua setter 가 TargetCamera 에 도달 못함 — 별도 PostProcess 객체 (예: 인스턴스 복사본) |
| 14716 | **다른 UUID** | 1.0 (고정) | 1.0 | 1.0 | 1.0 | 1.0 | 1.00 | **가장 유력** — Lua 가 14716 에 쓰지만 ActiveCameraCached 는 다른 카메라. 해결: Lua 에서 `Player.controller:GetActiveCamera()` 사용해 Player.camera 동기화 |
| 14716 | 14716 | 1.0→0.1 | 1.0→0.1 | 1.0→0.1 | 1.0→0.1 | 1.0→0.1 | 0.10 | smoothing 패치 정상, 시각 부재는 Vignette.hlsl 의 디버그 코드만 원인 — 셰이더 복원 |
| Bend=1 (사망 동안 지속) | — | — | — | — | — | — | — | blend 가 끝나지 않음. BlendTime/Pending 상태 확인 |

## Verification

1. 위 두 로그를 추가하고 `Build/Debug` 또는 `Build/Release` 에서 `PlayerCameraManager.obj`, `LuaCameraComponentBindings.obj`, `DrawCommandBuilder.obj` 를 삭제 후 클린 리빌드.
2. 게임 실행 → 차량과 충돌해 사망 트리거.
3. 사망 후 첫 1초 분량의 콘솔 로그 캡처:
   - `[Lua->Cam] UUID=... VignetteIntensity := 0.10` 라인
   - `[CameraMgr] Tgt=... TgtPP=... Desired=... Current=... Final=... Out=... OutPP=...` 라인
   - `[DrawCommandBuilder] CamUUID=13536 ... Intensity=...` 라인
4. 위 표와 매칭해 결함 위치 특정 후 로그 제거 + 본격 수정 (별도 plan 으로 진행).

## 사후 정리 (이번 plan 의 verification 완료 후)

- 진단 로그 두 개 모두 제거.
- [DrawCommandBuilder.cpp:466-470](KraftonEngine/Source/Engine/Render/Command/DrawCommandBuilder.cpp:466) 의 활성화된 로그도 다시 주석 처리 또는 제거.
- [Vignette.hlsl:32](KraftonEngine/Shaders/PostProcess/Vignette.hlsl:32) 의 `float3(165, 42, 42)` 를 원본 `VignetteColor` 로 복원.
- 결함이 가설 5 (Lua 가 잘못된 카메라에 씀) 로 판명되면 Player.lua 의 `Player.camera` 갱신 로직 (1041-1044 의 dead code 또는 매 프레임 `GetActiveCamera()` 동기) 추가.