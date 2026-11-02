# UE 5.8 MCP 블루프린트 작성 범위 조사

최신 메인 프로젝트 구현 상태는 문서 마지막의 **2026-11-02 고정 캡처·카메라 연동 보강** 절을 따른다. 앞선 실험의 미완료·미해결 표기는 각 실험 시점의 기록이다.

2026-11-01, GuideFull58, UE 5.8.4. 이 문서는 엔진의 모든 블루프린트 노드 목록이 아니라 실행 중인 Cubeless MCP의 함수 호출·이벤트·그래프 가져오기 경로를 조사한 기록이다. 소스에 존재하는 프로파일은 실제 호출 성공이나 모든 파라미터 조합의 지원을 뜻하지 않는다.

## 실제 관찰

- 공식 ModelContextProtocol, ToolsetRegistry, CubelessMCP_UE58은 GuideFull58 프로젝트에서 Editor 대상으로 활성화돼 있으며, PID 18220의 `127.0.0.1:8018/mcp`에서 초기화와 도구셋 조회에 응답했다.
- `add_blueprint_input_key_node`는 직접 Actor를 부모로 둔 임시 Blueprint에 P 이벤트를 생성했고, `GetPlayerCharacter`, `PrintString` 호출과 연결했다. PIE에서 입력 주입 후 Blueprint 로그가 출력됐다.
- 동일 작성기의 `/Script/Engine.Character:Jump` 요청은 `function_callable_unsupported`로 생성 전에 거부됐다. 반면 `import_blueprint_graph_nodes_from_text`는 동일 엔진 함수를 참조하는 표준 CallFunction 노드를 생성했고, 기존 캐릭터에 연결한 P 입력 시험에서 Z 92.15 → 204.37 cm와 양의 수직 속도를 관찰했다. 이는 별도 경로의 단일 동작 확인이지 일반 함수 작성기의 성공이 아니다.
- 같은 일반 함수 작성기에 `Character.StopJumping`, `Character.LaunchCharacter`, `Actor.GetActorForwardVector`를 각각 요청했을 때에도 `function_callable_unsupported`가 preflight에서 반환됐다. `GetActorForwardVector`는 별도 텍스트 가져오기 경로로 노드 생성에 성공했다. 이 결과는 다른 호출 경로 전체의 미지원을 의미하지 않는다.
- `GameplayStatics.SpawnDecalAtLocation` 노드, 별도 검은 decal 머티리얼 및 위치 계산을 연결해 PIE에서 캐릭터 앞의 사각형을 시각적으로 확인했다. 별도 후속 시험에서는 0.5초 Timeline에 따라 평면 배율 약 0.38과 1.0을 관찰했고, 그 후 Z 상승을 확인했다. 임시 생존 시간은 3초이며 하강 합성, 진입 후 1초 대기와 닫힘은 아직 구현하지 않았다.
- 공식 `BlueprintGraphEditor.list_available_nodes([])`는 이 테스트 Actor의 EventGraph에서 액션 후보 45,639개(고유 문자열 44,323개)를 반환했다. 원본 목록은 `cubeless/workspaces/GuideFull58/Saved/SquareHole_EditorAvailableNodes_20261101_v2.json`이다. 중복·지역화된 이름을 포함하고, 후보 열거는 해당 노드의 Cubeless 작성 허용이나 생성·컴파일·실행을 증명하지 않는다. 최초 JSON 기록은 Unreal Array 직렬화 오류로 불완전하게 남았으며 v2는 Python list 변환 후 기록했다.
- 원본 서드퍼슨 캐릭터 복제 사전 검사에서 더티 `CR_Mannequin_Body`가 의존성 그래프에 포함돼 `duplicate_dependency_audit_failed`가 반환됐다. 관찰된 메시→리그 간선은 editor-only soft reference였으며 금지 접두사 위반은 없었다. 더티 메모리 상태가 디스크 의존성 조사에 반영되지 않아 검사가 불완전하다는 뜻이다.

## 작성기별 범위와 제약

| 경로 | 확인한 지원 또는 소스상 프로파일 | 거부·미검증 범위 |
| --- | --- | --- |
| `add_blueprint_call_function_node` | 엔진의 public static 함수 중 허용된 시그니처, 자기 Blueprint의 일부 함수, 명시적으로 감사한 Engine 인스턴스 메서드. 실제 `GetPlayerCharacter`, `PrintString` 생성 성공. | 임의 인스턴스 함수 전체는 허용하지 않는다. 기존 구현의 `Character.Jump`는 목록에 없어서 거부됐다. 실제 엔진 기능의 부재가 아니다. |
| 같은 경로의 특수 분기 | `Delay`, `RetriggerableDelay`는 기존 그래프에서 `allow_latent`를 켠 전용 경로가 있다. `AddComponentByClass`는 별도 `allow_internal` 경로가 있다. Map/Set 등 일부 wildcard 함수도 전용 경로다. | 일반 경로는 latent, deprecated, CustomThunk, BlueprintInternalUseOnly 및 wildcard를 거부한다. 위 전용 분기의 존재를 다른 함수 전반의 지원으로 일반화할 수 없다. |
| 입력·이벤트 | 유효한 디지털 Blueprint 키를 입력 가능한 Actor 이벤트 그래프에 추가한다. P는 독립 Actor에서 실행까지 확인했다. | 아날로그·폐기·바인딩 불가 키와 비호환 그래프는 거부한다. 실험한 서드퍼슨 자식 Blueprint는 `event_parent_unsupported`였다. 입력 노드 생성만으로 매핑, 소유, 실제 키 동작을 보장하지 않는다. |
| 핀 기본값 | 미연결 기본형 핀 및 한정된 타입별 특수 경로를 지원한다. | 일반 경로는 임의 에셋 오브젝트, 클래스, 연결된 핀, 컨테이너 및 임의 구조체 리터럴을 폭넓게 지원하지 않는다. 별도로 감사된 material 리소스 변수 기본값과 getter 연결은 이번 실험에서 성공했다. |
| 텍스트 그래프 가져오기 | 로드된 런타임 객체·멤버를 사전 확인한 표준 노드 텍스트를 제한된 조건에서 붙인다. `Character.Jump`와 `GameplayStatics.SpawnDecalAtLocation` 노드 생성은 성공했다. | 임의 직렬화 텍스트나 모든 노드가 허용되는 것은 아니다. 정확한 런타임 참조·소유자·K2 그래프 및 유효한 텍스트가 필요하며, 미지원 프로파일과 불확실한 결과에는 자동 재시도·원자적 롤백이 없다. |

함수 소스에는 예를 들어 `KismetMathLibrary`의 `MakeVector`, `Add_VectorVector`, `Multiply_VectorFloat`, `VLerp`, `RLerp`, `FindLookAtRotation`, `BreakTransform`; `SceneComponent`의 `K2_SetWorldLocation`, `SetWorldScale3D`, `SetVisibility`; `Actor`의 `GetActorBounds`, `SetActorHiddenInGame`, `SetActorEnableCollision`; `ActorComponent`의 `Activate`, `Deactivate`에 대한 한정 프로파일이 있다. 이 열거는 소스 조사이며 호출별 조건과 실행 결과는 별도 확인해야 한다.

검사 코드는 대상이 public BlueprintCallable 런타임 함수인지, 허용된 소유자·플래그·메타데이터·핀 타입인지 검증한다. 생성·연결 중 에디터 객체 상태와 부분 변경을 보호하기 위한 제한이 코드에 구현돼 있다. `Jump`를 특별히 금지한 이유는 확인되지 않았다. Git blame은 이 감사 목록을 2026-10-25의 `645ae5f`에 귀속하며, 2026-10-10 `AB-native-api.md`는 `SetIgnoreLookInput`만 추가하고 다른 Controller 인스턴스 함수를 일반적으로 허용하지 않는다고 설명한다. 이는 범위를 좁힌 설계의 근거이며 `Jump` 미포함의 개별 동기를 증명하지는 않는다.

## 근거 및 한계

- `Plugins/CubelessMCP_UE58/Source/CubelessMCPUE58Editor/Private/Compatibility/UE58/CubelessFunctionApi.cpp`: 인스턴스 분류 및 callable 검사, 특수 분기와 파라미터 검사.
- 같은 디렉터리 `CubelessEventApi.cpp`: 입력 키와 그래프/부모 검사.
- 같은 디렉터리 `CubelessBlueprintGraphTransferApi.cpp`: 텍스트 구조와 로드된 런타임 참조 검사.
- `cubeless/records/migration/implementation-2026-10-10/AB-native-api.md`: 제한된 인스턴스 함수 지원 기록. 역사 자료이며 현재 승인이나 전체 동작의 증명은 아니다.
- 플러그인 확장, 재빌드, 재연결, 냉시작·패키징 및 전체 스킬 테스트는 각각 별도 상태로 기록해야 한다. 전체 엔진 함수/노드의 자동 열거, 모든 경로의 거부 시험 및 성능 측정은 수행하지 않았다.

## 후속 구현과 검증 상태

- `Character.Jump`의 인자 없는 엔진 인스턴스 함수를 일반 작성기의 명시적 허용 목록에 추가했다. 별도 검증 프로젝트 `C:/CBJump01`에서 플러그인을 빌드하고 MCP 일반 작성기로 Jump 노드를 생성·컴파일·저장했다. `StopJumping`은 계속 거부됐다. 원래 GuideFull58 에디터는 재시작하지 않았으므로 실행 중인 기존 DLL에 이 변경이 반영된 것은 아니다.
- GuideFull58의 `/Game/_MCP_Temp/SquareHole/CubelessTests/Run20261101a/`에 시제품을 저장했다. 구멍 확대, 점프, 바닥 아래 하강과 캐릭터 메시 상단이 바닥 아래로 들어간 뒤 약 1초 대기 후 닫힘을 PIE에서 관찰했다. 샘플 간격 때문에 측정된 약 1.08초에는 샘플링 오차가 있다. 위 초기 실험의 미구현 표기는 해당 실험 시점에 한정된다.
- Scene Capture로 캐릭터를 구멍 내부에 합성했으나 캐릭터가 원래보다 어둡고, 바닥 위에 그림자가 남는다. 캡처의 Dynamic Shadows, Ambient Occlusion, Contact Shadows를 꺼도 색상 보존이 확인되지 않았다. 효과 캡처, 다양한 지형과 시점, 성능, 패키징 및 MCP 없이 독립 실행은 검증하지 않았다.
- 현재 원래 맵의 기본 게임 모드에 자동 연결된 상태가 아니다. PIE에서 테스트 캐릭터와 컨트롤러를 명시적으로 생성해 검증했다. 기존 더티 Control Rig는 저장하거나 버리지 않았다.

## 2026-11-02 합성 순서 추가 시험

- 캡처 소스는 HDR SceneColor였고 포스트 프로세스 머티리얼은 톤 매핑 이후에 합성했다. 머티리얼의 Blendable Location을 `BL_SCENE_COLOR_BEFORE_BLOOM`으로 바꾸어 컴파일·저장하고 동일 PIE 장면을 다시 촬영했다. 캐릭터가 이전 시험보다 시각적으로 밝아졌지만, 원본과의 정량적인 색상 일치 및 노출 변화는 검증하지 않았다. 테스트 이미지: `C:/CBJump01/SquareHoleMid02.png`, `C:/CBJump01/SquareHoleMid03.png`.
- 바닥 위에 캐릭터 그림자가 남는다. 기본 게임 모드 자동 연결, 재사용, 이펙트 캡처, MCP 없는 패키징 및 메인 프로젝트 이관은 아직 완료하지 않았다. 원본 및 검증 프로젝트의 ThirdPerson 캐릭터·게임 모드·플레이어 컨트롤러 uasset은 SHA-256이 각각 같았으나, 이것만으로 전체 의존성이 동일함을 보증하지 않는다.

## 메인 프로젝트 이관 및 후속 시험

- 검증 프로젝트에서 하강 시작 후 메시 그림자를 비활성화해 바닥 위의 잔여 그림자가 사라지는 것을 PIE 스크린샷으로 확인했다. 구멍이 닫히면 저장한 시작 위치로 캐릭터를 되돌리고 충돌, 그림자와 스프링 암 연결을 복구한다. 진행 중 입력 잠금을 추가했다.
- 시제품 캐릭터에 제어 액터를 Child Actor Component로 연결했다. 검증 프로젝트에서 `/Engine/Maps/Templates/Template_Default?game=/Game/_MCP_Temp/SquareHole/CubelessTests/Run20261101a/BP_SquareHoleGameMode.BP_SquareHoleGameMode_C`로 PIE 이동 후 게임모드가 캐릭터와 제어 액터 하나를 자동 생성했고, 연속 두 번의 하강 및 위치·충돌·그림자·카메라 부모 복구를 확인했다.
- Asset Registry의 재귀 게임 패키지 의존성 65개 중 시제품 7개를 제외한 58개 파일은 메인과 SHA-256이 일치했다. 메인 프로젝트에 기존 파일을 덮어쓰지 않고 7개를 복사한 뒤 `/Game/Study/SquareHole/`로 이동했다. 새 에셋 간 참조는 새 경로를 가리켰다. 에디터 Python 래퍼는 에셋 이동 후 트랜잭션 소유 상태 변경으로 결과를 unknown으로 표시했으므로 재시도하지 않았다. 후속 독립 조회에서 7개 새 에셋, 블루프린트 up-to-date, 더티 패키지 및 맵 없음, 새 참조를 확인했다.
- 메인 프로젝트의 `/Engine/Maps/Templates/Template_Default` PIE에서 새 게임모드로 자동 생성과 2회 동작·복귀를 확인했다. 단, 메인 화면의 캐릭터는 구멍 안에서 원래보다 어둡고 푸르게 보인다. Lumen 캡처 표시 비활성화, 캡처 조명 비활성화, EyeAdaptation 보정은 해결하지 못해 실험 설정을 원복했다. 스크린샷은 `C:/CBJump01/MainSquareHoleBefore.png`, `C:/CBJump01/MainSquareHoleMid.png`, `C:/CBJump01/MainSquareHoleNoLumen.png`, `C:/CBJump01/MainSquareHoleUnlit.png`, `C:/CBJump01/MainSquareHoleExposure.png`이다. 따라서 색상 보존은 미완료다.
- 메인 프로젝트의 기존 기본 맵/게임모드는 변경하지 않았다. 테스트하려면 테스트 맵 또는 PIE에서 새 게임모드를 지정해야 한다. 다양한 지형, 이펙트 캡처, 성능, MCP 플러그인 비활성화 후 로드, cook/package 및 독립 실행은 미검증이다.

## 2026-11-02 밝기 분석 중단 시점

- 메인 PIE에서 구멍에 진입할 때 바닥 위의 밝은 부분과 캡처 합성된 어두운 몸통이 동시에 보였고, 아래쪽에 밝은 부분이 나타나는 프레임도 관찰했다. 캡처 원본을 같은 프레임에 읽어 알파를 적용해 표시했을 때도 캐릭터가 어둡게 보였다. 예시 raw center RGB는 약 `(0.0023, 0.0040, 0.0080)`이었고 이는 특정 프레임·픽셀의 값이다. 특정 광원이나 노출이 원인이라는 증거는 아니다.
- 캐릭터의 Custom Depth와 Scene Depth를 비교해 가려진 부분만 합성하는 실험, Scene Capture `Final Color LDR`, 8배 색상 이득, PIE에서 바닥 그림자 비활성화를 시험했으나 원본과 일치하는 캐릭터 조명을 얻지 못했다. 해당 실험은 전부 원복했다. 저장된 캡처 소스는 `SceneColor HDR`, Blendable Location은 `Before Bloom`이며 명시적 Show Flags는 `DynamicShadows`, `AmbientOcclusion`, `ContactShadows` 세 항목이 꺼져 있다. 그 외 Show Flags의 최종 비트 값은 읽어 확인하지 못했다.
- 현 시제품의 진입 중 밝기 및 색상 차이는 미해결이다. 고정 시점 및 선택한 평면에서의 PIE 동작은 확인했지만, 다양한 노출·장면, 최종 흰색 변화의 정확한 원인, 성능, 독립 패키징 실행은 검증하지 않았다. 별도 저장된 캡처·화면 증거는 `C:/CBJump01/CaptureRaw.png`, `C:/CBJump01/CaptureRawComposite.png`, `C:/CBJump01/DepthGatePaused.png`, `C:/CBJump01/DepthGateLate2.png`이다.

## 2026-11-02 고정 캡처·카메라 연동 보강

대상은 메인 저장소의 `UnrealMCP5_8_3.uproject`, 실행 엔진은 UE 5.8.4 CL 58902052다. GuideFull58 또는 별도 프로젝트에서 수정한 결과가 아니다. 변경한 게임 에셋은 `/Game/Study/SquareHole/`의 `BP_SquareHoleController`, `M_SquareHolePostProcess`, `RT_CharacterCapture` 세 개다. 프로젝트 C++와 MCP 런타임 API는 추가하지 않았다.

### 적용한 계획과 설정

1. RT를 **768×768, RGBA16F 고정**으로 바꿨다. 구멍 크기, 카메라 FOV, 화면 크기에 따라 RT를 재생성하거나 리사이즈하는 노드는 없다. 최대 구멍 반크기는 **200cm**, 기본 목표 반크기는 **100cm**다. 실제 구멍 반크기는 기존 열림·닫힘 Timeline이 0에서 목표값까지 바꾼다. `PostMID`의 `HoleTargetHalfSize`를 설정하면 목표 크기를 바꿀 수 있으며, BP와 머티리얼 모두 0~200cm로 제한한다. 최대값을 바꾸려면 이 두 제한과 캡처 시야 검증을 함께 갱신해야 한다.
2. 제어 Actor를 `TG_POST_UPDATE_WORK`에서 갱신한다. `PlayerCameraManager`의 최종 카메라 위치와 FOV를 읽으며 특정 CameraComponent의 고정 이름에 의존하지 않는다. 캡처는 같은 카메라 원점에서 고정 구멍 중심을 향하고, 캡처 수평 FOV는 `max(메인 FOV×0.8, 70°)`다. 이 시야는 최대 반크기 200cm의 평면을 포함하도록 현재 서드퍼슨 카메라에 맞췄다. 현재 구멍 크기는 캡처 위치·회전·FOV 계산에 들어가지 않는다.
3. PP에 캡처의 Forward/Right/Up과 `tan(FOV/2)`를 전달한다. 메인 화면 픽셀의 월드 광선을 캡처 UV로 다시 투영하므로 전체 화면 UV를 정사각형 RT에 그대로 늘이지 않는다. 캡처 UV 밖은 검은색으로 처리해 가장자리 픽셀을 늘여 붙이지 않는다. 메인 카메라의 화면비는 메인 뷰의 월드 광선에 반영된다. 이번 PIE 화면비 외 별도 세로 화면 시험은 하지 않았다.
4. 메인 픽셀의 Scene Depth에 해당하는 월드 위치와 수평 구멍 평면의 광선 교차 거리를 비교한다. 평면보다 앞의 불투명 물체는 원래 화면을 유지한다. 구멍 안쪽만 검은 바탕과 캐릭터 캡처로 대체한다. 기본 깊이 허용치는 `DepthBias=1cm`다. 이 범위에서는 **Custom Depth/Stencil을 추가하지 않았다**. 임의 경사면, 깊이를 쓰지 않는 반투명 전경, 정사영·VR·분할 화면은 검증 범위 밖이다.
5. 캡처는 `Use Show Only List`를 유지하고, 스킬 실행 중 매 프레임 플레이어와 Child Actor의 컴포넌트를 `ShowOnlyActorComponents(..., true)`로 등록한다. 새로 생긴 해당 컴포넌트도 다음 캡처에 포함된다. 별도 Actor로 생성하는 이펙트는 별도 Show Only 등록이 필요하다. 단순 반투명·가산 재질은 아래 후속 리뷰에서 확인했으며, 실제 Niagara 이펙트 전체의 색·알파 합성은 아직 검증하지 않았다.
6. `Capture Every Frame`과 `Capture on Movement`를 끄고, MID와 스킬 상태를 확인한 뒤 카메라·투영 파라미터 갱신 → Show Only 갱신 → `CaptureScene` 순서로 실행한다. 대기 상태에서는 캡처하지 않는다. 렌더 상태는 유지한다.

### 검은 캐릭터 현상의 원인과 수정

같은 위치에서 플레이어 메시의 `Receives Decals`만 일시적으로 끈 대조 시험을 수행했다. 시간 배율을 매우 작게 한 상태로 한 프레임 이상 캡처를 갱신했을 때 검은 몸통이 원래의 흰색으로 바뀌었다. **검은 구멍 데칼이 캐릭터에 투영되는 것이 검게 변하던 주요 원인임을 확인했다.** 증거는 `Saved/SquareHoleUpgrade20261102/decal-before.png`, `decal-after.png`다.

최종 구현은 캐릭터의 모든 데칼 수신을 끄지 않는다. 생성한 구멍 데칼만 숨기고, 기존 위치·높이 판단용 컴포넌트로 사용한다. 검은 구멍은 PP가 그린다. 구멍 내부의 흰 캐릭터를 확인했지만 모든 장면의 조명·노출에서 원본과 정량적으로 색이 일치한다고 검증한 것은 아니다. 기존 HDR `SceneColor`와 `Before Bloom` 순서를 유지했다.

명시적으로 끈 Show Flags는 `DynamicShadows`, `AmbientOcclusion`, `ContactShadows`, `Decals`, `MotionBlur`, `TemporalAA`, `Bloom`, `Tonemapper`, `EyeAdaptation`, `Fog`, `Atmosphere`, `VolumetricFog`다. 나머지 플래그는 엔진 기본값을 따른다. 위 배열을 최종 엔진 비트 전체를 읽은 결과로 해석하지 않는다.

### 실행 결과와 재현

- 메인 프로젝트 PIE에서 기본 목표 반크기 100cm의 하강과 흰 캐릭터 합성을 확인했다: `Saved/SquareHoleUpgrade20261102/upgrade-first.png`.
- 별도 `SkeletalMeshActor`를 구멍 앞에 겹쳐 놓으면 전경에 남았다. 같은 메시를 바닥 아래에 놓았을 때 캡처에 섞이지 않았다: `foreground-character.png`, `max-hole-fov60.png`.
- 목표/현재 반크기 200cm와 메인 FOV 60°에서는 캡처 FOV 70°, 반크기 50cm와 메인 FOV 110°에서는 캡처 FOV 88°를 관찰했다. 두 경우 RT는 768×768이었다: `max-hole-fov60.png`, `small-hole-fov110.png`.
- 연속 두 번 실행 후 시작 위치 `(0,0,91.65)`, 충돌 활성화, 그림자 활성화, SpringArm의 Capsule 부모 복구와 스킬 잠금 해제를 확인했다.
- Show Only 갱신 보강 후 최종 저장된 BP를 다시 PIE로 실행했다. 최대 반크기 200cm의 네 모서리가 모두 캡처 UV 범위 안에 있었고, 최종 복귀도 통과했다. UV는 대략 `(0.168,0.688)`, `(0.832,0.688)`, `(0.315,0.395)`, `(0.685,0.395)`다. 이는 시험한 카메라 거리·각도의 증거이며 모든 근접·비스듬한 시야의 보증은 아니다. `Saved/SquareHoleUpgrade20261102/final-pie.json`에 기록했다.
- BP 컴파일은 오류 0·경고 0이었다. 세 에셋을 저장했고 PIE 종료 후 더티 콘텐츠 패키지는 없었다. 직접 Asset Registry 의존성에는 MCP 패키지가 없었다. 디스크 냉시작, 플러그인 비활성화 후 로드, cook/package, MCP 없는 독립 실행, GPU 시간 측정은 이번 작업에서 수행하지 않았다.

기존 기본 맵/게임모드는 변경하지 않았다. `BP_SquareHoleGameMode`로 테스트하거나 PIE 콘솔에서 다음 명령을 실행한 뒤 **P**를 누른다.

```text
open /Engine/Maps/Templates/Template_Default?game=/Game/Study/SquareHole/BP_SquareHoleGameMode.BP_SquareHoleGameMode_C
```

### 작성 경로와 실패 기록

- 표준 엔진 CallFunction/VariableSet/Branch 노드는 기존 native 텍스트 가져오기를 사전 검사한 뒤 생성했다. 일반 핀 연결·기본값·컴파일 도구와 공식 `BlueprintGraphPin.try_create_connection`을 사용했다. 직접 공식 핀 연결의 전후 전체 간선 차이가 지정한 교체 한 쌍뿐인지 검사했다. 머티리얼은 `MaterialEditingLibrary`로 작성했다. 저장된 게임 그래프에 작성용 MCP 함수를 연결하지 않았다.
- 새 BP 멤버 변수 추가는 외부 ChildActor 템플릿 인스턴스 보호 검사에서 작성 전에 거부됐다. 이어 템플릿의 ChildActor 클래스를 일시 해제하는 시도 중 에디터가 종료됐다. 직전 더티 패키지와 PIE는 없었고 저장된 파일 변경도 없었다. 같은 해제 조작은 반복하지 않고 메인 프로젝트를 재실행한 뒤 기존 MID 파라미터와 표준 엔진 노드를 사용하는 경로로 완료했다. 별도 프로젝트로 전환하지 않았다.
- 구체 핀 교체 경로는 자체 멤버·Timeline이 있는 BP 프로파일을 거부했다. exec 교체의 기존 대상 입력에 여러 간선이 생긴 경우에도 별도 보호 검사에서 거부됐다. 상태를 재조회한 뒤 범위가 정해진 공식 핀 연결을 사용했다. float 연결로 PromotableOperator 타입이 정상 변경된 한 호출은 도구의 보존 검사에서 실패로 반환됐지만, 후속 조회에서 양방향 연결이 존재했고 컴파일·PIE는 통과했다. 그 실패 요청을 자동 재시도하지 않았다.

관련 설치 엔진 근거는 `Engine/Source/Runtime/Engine/Classes/Components/SceneCaptureComponent2D.h`, `SceneCaptureComponent.h`, `Camera/PlayerCameraManager.h`, `Engine/Shaders/Private/SceneCapturePixelShader.usf`다. 실험 클라이언트·그래프 스냅샷·원시 로그는 무시되는 `Saved/SquareHoleUpgrade20261102/`에 두었다.

### 후속 자체 리뷰와 알파 합성 수정

사용자의 검수 후 리뷰·수정·메인 게시 요청에 따라 실제 118개 노드와 저장된 머티리얼을 검토했다. 새 실행 노드들의 연결과 고정 해상도·대기 상태 캡처 차단을 확인했고, 그래프의 호출 소유자는 표준 `/Script/Engine` 클래스였다. 별도 검토자의 독립 리뷰는 수행하지 않았다.

**수정한 P2 문제:** 기존 `captured.rgb * (1 - captured.a)`는 검은 배경에 이미 합성된 캡처 RGB에 불투명도를 다시 곱했다. 캡처의 알파는 inverse opacity이며 RGB를 원래 색으로 역산한 값이 아니다. 이 중복 곱은 반투명 색을 다시 어둡게 만들고 알파를 유지하는 가산 색을 없앴다. 검은 배경과 Show Only·Fog/Atmosphere 비활성 설정을 유지한 채, 최종 합성을 `captured.rgb * inside`로 바꿨다. 캡처 RGB는 별도의 불투명도 곱 없이 구멍의 검은 바탕 위에 사용한다.

- 별도 소유 테스트 경로 `/Game/_MCP_Temp/SquareHole/CubelessTests/Review20261102/`의 unlit 빨간 재질로 확인했다. opacity 0.5인 Translucent의 중앙 RT는 약 `(0.49976,0,0,0.5)`였고 기존 식의 빨간 값은 약 `0.24988`이었다. 같은 opacity의 Additive RT는 약 `(0.49976,0,0,1)`이었으며 기존 식은 빨간 값을 0으로 만들었다. 빈 배경의 RGB는 두 경우 모두 0이었다. `Saved/SquareHoleUpgrade20261102/alpha-review.json`에 수치를 기록했다.
- 수정 후 메인 스킬의 정상 실행 중 두 테스트 재질을 별도 Show Only 등록한 메시로 바닥 아래에 배치했다. RT 빨간 값은 두 경우 모두 0.5였고, 실제 최종 구멍 화면에서도 두 재질이 빨간색으로 보였다. 스크린샷의 빨간 영역은 각각 25,846·25,691픽셀이었다. 증거는 `M_HalfOpacity-integrated-fixed.png`, `M_Additive-integrated-fixed.png`, `alpha-composite-result.json`이다. 이 픽셀 수는 출력 존재 확인이며 GPU 성능 수치나 선형 색상 일치 측정이 아니다.
- 수정 후 최대 반크기 200cm의 캡처 모서리 범위와 스킬 종료 복귀도 다시 통과했다. 테스트는 PIE 종료로 정리했고 테스트 재질과 클라이언트는 게시 파일에 포함하지 않는다. 메인 에디터는 유지한다.
- 최초 probe 시험은 StaticMeshActor의 Static 모빌리티 때문에 이동·메시 설정이 거부되어 빈 RT를 읽었다. Movable로 고친 새 시험에서 위 수치를 확인했다. 별도 수동 카메라 합성 시험도 캡처 위치·축이 0인 상태라 빈 화면을 내어 통과로 기록하지 않았다. 최종 출력 확인은 정상 스킬의 카메라·Tick 경로를 이용한 위 통합 시험으로 수행했다.

설치 엔진의 `Engine/Shaders/Private/SceneCapturePixelShader.usf`, `SceneTexturesCommon.ush`와 `Engine/Source/Runtime/Renderer/Private/TranslucentRendering.cpp`의 SceneColor 복사 및 Translucent/Additive 블렌드 설정을 대조했다. 반투명 재질은 `Before DOF` 시험에 한정되며, 다른 translucency pass나 실제 Niagara 구성 전체, 노출 변화, 패키징·독립 실행은 여전히 미검증이다.
