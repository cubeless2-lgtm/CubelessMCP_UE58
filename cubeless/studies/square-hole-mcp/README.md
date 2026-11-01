# UE 5.8 MCP 블루프린트 작성 범위 조사

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
