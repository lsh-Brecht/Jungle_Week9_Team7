# 구현 명세서: Pawn 중심 Vignette 효과 및 화면 Fade In/Out 시스템

본 문서는 KraftonEngine 내에서 플레이어(Pawn)를 중심으로 하는 Vignette 효과와 화면 전체의 Fade In/Out 효과를 구현하기 위한 기술적 명세와 단계를 정의합니다.

---

## 1. 개요
- **목표**: 
    1. 게임 내 활성화된 Pawn의 위치를 추적하여 화면에 Vignette 효과(테두리 어두워짐) 적용.
    2. 시각적 연출을 위한 전역 Fade In/Out 기능 구현.
    3. 모든 효과를 Lua Script에서 제어할 수 있도록 바인딩 제공.

---

## 2. 데이터 구조 및 위치 파악

### 2.1 주요 클래스 및 위치
- **Pawn 위치 파악**:
    - `FPrimitiveSceneProxy` (KraftonEngine/Source/Engine/Render/Proxy/PrimitiveSceneProxy.h): 렌더링 데이터 미러. `GetOwner()`를 통해 `APawn` 접근 가능.
    - `APawn` (KraftonEngine/Source/Engine/GameFramework/Pawn.h): 플레이어 캐릭터 클래스.
- **포스트 프로세스**:
    - `FPostProcessPass` (KraftonEngine/Source/Engine/Render/RenderPass/PostProcessPass.h): 포스트 프로세스 렌더 패스 관리.
    - `DrawCommandBuilder.cpp` (KraftonEngine/Source/Engine/Render/Command/DrawCommandBuilder.cpp): 실제 쉐이더 실행 및 렌더 커맨드 생성 로직.
- **카메라 및 페이드 제어**:
    - `FPlayerCameraManager` (KraftonEngine/Source/Engine/Camera/PlayerCameraManager.h): 카메라 상태 관리 및 블렌딩.
    - `UCameraModifier` (KraftonEngine/Source/Engine/Camera/CameraModifier.h): 카메라 속성(Alpha 등) 조작용 기저 클래스.
- **Lua 바인딩**:
    - `LuaCameraComponentBindings.cpp` / `LuaPlayerControllerBindings.cpp` (KraftonEngine/Source/Engine/Scripting/): 스크립트 노출 로직.

---

## 3. 세부 구현 계획

### Phase 1: 렌더링 파이프라인 확장 (C++ & HLSL) — 완료 (2026-05-06)

**구현 결정사항**
- 셰이더는 Vignette과 Fade를 **분리**하여 두 개의 풀스크린 패스로 발행 (드로우콜 2회).
- 상수 버퍼는 `FFrameContext`에 합치지 않고 **패스별 독립 CB 구조체**로 분리. 기존 PostProcess 컨벤션(FogCB, OutlineCB, FXAACB)과 동일하게 `b2` 슬롯에 바인딩.
- 합성 방식은 SceneColor 텍스처를 직접 샘플하지 않고 **하드웨어 AlphaBlend**(`PostProcess` 패스의 RenderState)에 위임 — PS는 `float4(EffectColor, alpha)`만 출력.
- VignetteCenter는 (0, 0)으로 초기화. Pawn 스크린 UV 갱신은 Phase 2에서 처리.
- 활성화 토글로 `FShowFlags::bVignette`, `bFade` 추가 (디폴트 `true`). 에디터 UI는 Phase 4(추후)로 분리.

**구현 항목**
1.  **쉐이더 신규 작성**
    - `KraftonEngine/Shaders/PostProcess/Vignette.hlsl`: `Common/Functions.hlsli`의 `FullscreenTriangleVS` 재사용. PS는 UV와 VignetteCenter의 거리에 `smoothstep(VignetteIntensity, VignetteIntensity + VignetteSmoothness, dist)` 마스크를 알파로 출력.
    - `KraftonEngine/Shaders/PostProcess/Fade.hlsl`: 동일 VS. PS는 `float4(FadeColor, FadeAlpha)` 그대로 출력.
2.  **C++ 상수 구조체 추가** — `KraftonEngine/Source/Engine/Render/Types/RenderConstants.h`
    - `FVignettePostProcessConstants` (32B, b2): `FVector2 VignetteCenter`, `float VignetteIntensity`, `float VignetteSmoothness`, `FVector VignetteColor`, `float _Pad0`.
    - `FFadePostProcessConstants` (16B, b2): `FVector FadeColor`, `float FadeAlpha`.
3.  **DrawCommandBuilder 수정** — `KraftonEngine/Source/Engine/Render/Command/DrawCommandBuilder.{h,cpp}`
    - 멤버 `FConstantBuffer VignetteCB`, `FadeCB` 추가. `Create()`에서 초기화, `Release()`에서 해제.
    - `BuildPostProcessCommands()`에 분기 2개 추가. `ERenderPass::PostProcess`로 발행하며 SortKey는 `5`(Vignette), `6`(Fade) — Fade가 모든 PostProcess 효과 위에 합성됨.
4.  **EShaderPath 등록** — `KraftonEngine/Source/Engine/Render/Shader/ShaderManager.{h,cpp}`
    - `EShaderPath::Vignette`, `EShaderPath::Fade` 경로 상수 추가.
    - `FShaderManager::Initialize`에서 사전 컴파일 등록 (핫 리로드 지원).
5.  **ShowFlags 토글** — `KraftonEngine/Source/Engine/Render/Types/ViewTypes.h`
    - `FShowFlags::bVignette`, `bFade` 추가 (디폴트 `true`).
6.  **빌드 시스템** — `KraftonEngine/KraftonEngine.vcxproj`
    - `<FxCompile Include="Shaders\PostProcess\Vignette.hlsl">`, `<FxCompile Include="Shaders\PostProcess\Fade.hlsl">` 항목 추가 (기존 PostProcess hlsl과 동일한 빌드 구성 패턴: 모든 구성에서 `ExcludedFromBuild=true` — 런타임 동적 컴파일).

**Phase 1 임시 디폴트 값** (Phase 2에서 카메라/Pawn 모디파이어로 덮어씀)
- `VignetteCenter = (0.0, 0.0)`, `VignetteIntensity = 0.5`, `VignetteSmoothness = 0.5`, `VignetteColor = (0,0,0)`
- `FadeColor = (0,0,0)`, `FadeAlpha = 0.0` (Fade는 효과 미적용 상태로 시작)

### Phase 2: 카메라 시스템 연동 (C++ Logic) — 완료 (2026-05-06)

**전제 조건**
- 본 Phase 시작 직전 `feature/delegate`의 `add CameraModifier`, `PlayerCameraManager inherits AActor`, `Camera modifier chain test`, `remove modifier test code` 4개 커밋을 머지하여 `UCameraModifier`/`APlayerCameraManager` modifier chain 인프라를 베이스로 사용.

**구현 결정사항**
- `FCameraView`에 PostProcess 필드를 직접 추가하지 않고 별도 `FCameraPostProcess` 구조체로 분리해 멤버로 포함. `BlendViews`에서 카메라 전환 시 자연스럽게 보간.
- `UCameraModifier::ModifyCamera`를 가상 함수로 전환 (헤더 주석 "나중에 필요시 virtual"의 시점이 Phase 2). 베이스는 no-op.
- `UCameraFadeModifier`를 신규 구현체로 추가. modifier의 `Alpha`(0→1, AlphaIn/OutTime이 곡선 결정)와 `TargetFadeAlpha`(완전 페이드 시 합성 강도)를 곱해 `InOutView.PostProcess.FadeAlpha`에 max-누적.
- VignetteCenter는 modifier가 아닌 `APlayerCameraManager`가 매 프레임 직접 갱신 — `UCameraComponent::GetSubjectActor()`로 추적 대상 획득 후 ViewProj로 NDC→UV 변환.
- Pawn 미존재/화면 밖/카메라 뒤일 때 (0.5, 0.5) 화면 중심으로 폴백.
- 머지된 `PlayerCameraManager`의 skeleton Fade 멤버들(`FadeColor/FadeAmount/FadeAlpha/FadeTime/FadeTimeRemaining`)과 디버그 자동 modifier 추가 코드(`bDebugModifierAdded`)는 제거 — `UCameraFadeModifier`가 대체.
- DrawCommandBuilder의 Phase 1 하드코딩 디폴트값은 `FCameraPostProcess` 구조체 디폴트로 이관, 패스에서는 `FrameContext.PostProcess`만 참조.

**구현 항목**
1.  **`CameraTypes.h`**: `FCameraPostProcess` 구조체 추가 (`VignetteCenter`, `VignetteIntensity`, `VignetteSmoothness`, `VignetteColor`, `FadeColor`, `FadeAlpha`). `FCameraView`에 멤버 포함.
2.  **`Component/CameraComponent.h/cpp`**: `PostProcess` 멤버 + getter/setter 추가. `GetSubjectActor(Controller)` public wrapper 추가 (기존 private `ResolveSubjectActor` 위임). `GetCameraView`/`CalcCameraView`/`ApplyCameraView`가 PostProcess 보존.
3.  **`Camera/CameraModifier.h/cpp`**: `ModifyCamera`를 `virtual`로 전환. 베이스는 `return true`만.
4.  **`Camera/CameraFadeModifier.h/cpp` (신규)**:
    - `UCameraFadeModifier : public UCameraModifier` 구현체.
    - `StartFadeIn(Duration, TargetAlpha, Color)`, `StartFadeOut(Duration)` 공개 API (Phase 3 Lua 노출과 1:1).
    - `ModifyCamera`에서 `Effective = Clamp(Alpha * TargetFadeAlpha, 0, 1)`, `InOutView.PostProcess.FadeAlpha = max(기존, Effective)`.
5.  **`Camera/PlayerCameraManager.h/cpp`**:
    - skeleton Fade 멤버 제거.
    - `bDebugModifierAdded` 디버그 자동 추가 코드 제거.
    - `Initialize`의 임시 `TestModifier` 생성 코드 제거.
    - `BlendViews()`에 PostProcess 보간 추가 (모든 필드 linear lerp).
    - `UpdateVignetteCenter(TargetCamera)` 헬퍼 추가 — Subject 위치 → `OutputCamera->GetViewProjectionMatrix() * world` → NDC → UV. `UpdateCamera`/`SnapToActiveCamera`에서 `ApplyCameraView` 직후 매 프레임 호출.
6.  **`Render/Types/FrameContext.h/cpp`**: `FCameraPostProcess PostProcess` 멤버 추가. `SetCameraInfo()`가 `Camera->GetPostProcess()`를 카피.
7.  **`Render/Command/DrawCommandBuilder.cpp`**: Phase 1 하드코딩 디폴트 제거. Vignette/Fade CB 채울 때 `Frame.PostProcess.*` 사용.
8.  **`KraftonEngine.vcxproj`**: `CameraFadeModifier.{h,cpp}` 등록.

### Phase 3: Lua Script 바인딩 (Scripting) (C++ & HLSL) — 완료 (2026-05-06)
1.  **속성 노출**:
    - `CameraComponent`에 `VignetteIntensity`, `VignetteColor` 속성 바인딩.
2.  **함수 바인딩**:
    - `PlayerController` 또는 `CameraComponent`에 `StartFade(Duration, TargetAlpha)` 함수 바인딩 추가.
    - `sol2`를 이용해 `LuaCameraComponentBindings.cpp` 등에 등록.

### Phase 4: UI 제어 (모든 Phase 구현 완료 후 추가)
모든 렌더/카메라/스크립트 통합이 완료된 뒤 마지막 단계로 진행. Phase 1~3의 구조 변경 없이 기존 토글/매개변수에 GUI를 부착하는 작업이므로 의존성 그래프상 마지막에 위치.
1.  **에디터 ShowFlags 토글 위젯**: `FShowFlags::bVignette`, `bFade`를 디버그/렌더 옵션 패널에 노출.
2.  **매개변수 슬라이더**: `VignetteIntensity`, `VignetteSmoothness`, `VignetteColor`, `FadeColor`, `FadeAlpha`의 실시간 조작 UI.
3.  **콘솔 변수 / 디버그 명령**: 게임플레이 중 단축 명령으로 효과 토글·강제 적용.
4.  **시네마틱/시퀀서 통합 (선택)**: 시네마틱 도구가 있다면 Fade In/Out 구간을 타임라인에서 편집 가능하도록 키프레임 트랙 제공.

---

## 4. 구현 시 주의사항
- **성능**: 매 픽셀마다 Pawn 위치를 계산하는 대신, CPU 단에서 1회 계산 후 쉐이더 상수 버퍼로 전달하여 GPU 연산을 최소화할 것.
- **확장성**: 추후 Vignette 외에 Bloom, Color Grading 등 추가 포스트 프로세스를 쉽게 추가할 수 있도록 `PostProcessSettings` 구조체를 체계화할 것.
- **유연성**: Lua에서 단순히 On/Off만 하는 것이 아니라, 강도와 색상을 실시간으로 조절 가능하게 하여 연출의 폭을 넓힐 것.
