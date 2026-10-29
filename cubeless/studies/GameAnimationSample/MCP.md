# MCP 작업 경로와 실패 경험

2026-10-30 게시·MCP 비활성 패키징의 후속 상태는 [README](README.md#2026-10-30-게시와-mcp-비활성-패키징)를 따른다. 아래 실행 기록의 당시 범위와 구분한다.

## 실제 사용한 연결

공식 UE ModelContextProtocol HTTP 서버 → 공식 Toolset Registry → 아래 Toolset → Editor Python/C++ → 설치 UE 5.8.4 API로 실행했다. 필수 플러그인은 켜지고 로드된 상태가 전제다. 별도 TCP 브릿지나 독립 MCP 서버, 게임플레이 C++를 추가하지 않았다.

| 작업 | 실제 도구/엔진 경로 | 판정 |
|---|---|---|
| PIE 시작·종료 | `EditorToolset.EditorAppToolset.StartPIE` / `StopPIE` | 공식 도구로 충족. 시작 반환과 이후 실제 폰 준비를 구분 |
| 맵 열기 | `CubelessEditorLevelSessionTools.open_editor_level` → `get_editor_level_open_status` | 기존 Cubeless로 충족. scheduled가 실제 open 완료는 아님 |
| AnimGraph 구조·설정 | `CubelessAnimGraphInspectionTools.inspect_anim_graph_protected_topology` | 기존 Cubeless로 충족 |
| 일반 함수 그래프 읽기 | `CubelessBlueprintGraphTools.list_blueprint_nodes`, 공식 `BlueprintTools.list_graphs/find_nodes/get_node_infos` | 공식 우회 경로를 실제 사용했고 Cubeless의 잘못된 pin GUID 조건을 별도로 수정 |
| AnimInstance 상태 | `CubelessAnimInstanceRuntimeTools.inspect_anim_instance_runtime_state` 및 `ToolsetLibrary.get_object_properties` | 기존 도구 조합. 비활성 경로 변수·캐시를 실제 상태와 구분 |
| 입력 | `CubelessPIEInputTools.inject_pie_input_key` | 기존 Cubeless로 충족. controller InputKey 전달 성공과 이후 게임 동작은 별도 관측 |
| Montage 데이터 | `CubelessAnimationAssetMetadataTools.inspect_animation_asset_metadata_guarded` | 기존 Cubeless로 충족. 메타데이터와 Notify 실제 실행은 구분 |
| 생성 노드 매핑 | `CubelessAnimNodePrePostRuntimePoseTools.sample_anim_node_pre_post_runtime_pose`, `mode=compiled_graph_mapping` | 실제 매핑 확인. private node 값·최종 포즈 읽기와 동등하지 않음 |
| 복사·설정·컴파일·저장 | 공식 Python 실행 → `EditorAssetLibrary`, `BlueprintEditorLibrary`, `BlueprintTools` | 조합으로 소유 예제 작성. 전용 일괄 저작 도구의 전체 실패 계약을 갖췄다고 주장하지 않음 |
| Montage 종료, 목표 반응, 본 포즈 | 공식 Python 실행 → AnimInstance delegate / Slate tick / SkeletalMeshComponent | 단일 실행별 bounded 관측. 재사용 가능한 설명은 남겼지만 새 전용 Toolset은 아님 |
| 렌더 화면 | `AutomationLibrary.take_high_res_screenshot` → 파일 생성·이미지 확인 | 잠금 상태에서도 실제 PIE 프레임 캡처 확인 |

완전한 Toolset 이름은 등록 모듈에 따라 다르다. 호출 전에 공식 `list_toolsets`/`describe_toolset`에서 확인한다. Python Toolset은 대체로 `cubeless_mcp_ue58.registration.<module>.<class>`, reflected C++ Toolset은 `CubelessMCPUE58Editor.<class>` 경로다.

## 전투 UMG 저작 확장

[ArenaUI](ArenaUI.md)에서는 기존 도구에 없는 위젯 트리 구성과 표준 WidgetAnimation 저작을 위해 Editor 전용 `CubelessUMGCompositionTools` 4개를 추가했다. 등록부·JSON operations·UE58 compatibility를 분리하고 컴파일·저장을 명시적으로 나눴다. 기존 에셋 덮어쓰기는 제공하지 않으며, 새 위젯/애니메이션의 부분 실패에는 단계·변경·컴파일/저장 상태를 반환한다. 실제 Blueprint/머티리얼은 표준 런타임 타입으로 작성했고, 새 게임 C++나 MCP 런타임 API는 만들지 않았다.

최종 DLL 빌드와 실제 프로젝트 MCP8000의 cold load/등록/스키마/기본 동작을 확인했다. 체력/회피/승패/대상 소멸과 실제 마우스·Enter 재시작, Tab 효과 전환을 검수했다. 기록은 `records/operations/game-animation-arena-ui-20261029/report.json`에 있다. 새 콘텐츠의 직접 MCP 참조는 없지만 이번에 cook/package·독립 게임 실행·독립 QA는 수행하지 않았다.

## 유효 그래프 읽기를 막았던 C++ 검수 조건

샘플의 `SandboxCharacter_Mover_ABP.Update_CVarDrivenVariables`와 `SandboxCharacter_Mover_Ragdoll.ProduceInput` 읽기가 `unique graph pin id` 검사에서 거절됐다. 공식 그래프 API로 같은 실제 노드를 정상 조회했다. 설치 `Engine/Source/Runtime/Engine/Classes/EdGraph/EdGraphPin.h`의 `FEdGraphPinReference::operator==`는 **OwningNode + PinId**를 비교한다. 서로 다른 노드의 동일 PinId를 무조건 그래프 손상으로 처리한 검수 가정이 문제였다.

수정은 `CubelessGraphApi.cpp`의 **읽기 전용 ListBlueprintNodes 경로**에 한정한다.

- 노드 GUID, pin pointer membership, 같은 소유 노드 안의 PinId 유일성, 개수·타입·기본값·연결·소유 관계 보호는 유지한다.
- 공개 제목 조회 등의 콜백 이후 현재 노드에서 살아 있는 핀을 다시 얻고, 포인터·parent/children·연결 endpoint identity를 비교한다. 과거 핀 포인터를 역참조하지 않는다.
- 반환에 `pin_identity_scope=owner_node_and_pin_guid_with_current_pointer_identity`를 명시한다. 핀은 단독 GUID로 식별하지 않고 노드와 함께 식별해야 한다.
- 상속 Blueprint의 skeleton superclass는 부모의 generated class와 다를 수 있다. 설치 `BlueprintCompilationManager.cpp::FastGenerateSkeletonClass`는 부모의 SkeletonGeneratedClass가 있으면 이를 사용한다. Ragdoll 예제에서 부모 generated=`SandboxCharacter_Mover_C`, skeleton parent=`SKEL_SandboxCharacter_Mover_C`를 실제 확인했다. 읽기 경로는 이 정확한 부모 Blueprint/generated/skeleton 바인딩을 강하게 보유하고 콜백 이후 재검사한다. 상속 관계·수명 검사를 제거하지 않았다.
- 삭제 등 writer의 기존 admission은 바꾸지 않았다. 따라서 이 수정으로 모든 그래프 변경 지원이 확대됐다고 표현하지 않는다.

이 소스는 Editor 모듈에만 속하며 입력/출력 스키마나 런타임 모듈 의존성을 추가하지 않는다. 실제 빌드·로딩·등록·행동 결과와 DLL 식별은 로컬 기능군 기록에 보관한다. 빌드 중 기록을 읽는 경우, 소스 수정과 현재 로드 DLL의 적용 상태를 혼동하지 않는다.

UE 5.8.4 전체 빌드 636작업이 2,885.85초에 성공했고 후속 상속 수정은 증분 빌드로 확인했다. 별도 소유 `AnimRead58`/공식 MCP 8002에서 CVar 그래프 29노드·123핀과 상속 ProduceInput 23노드·66핀을 정상 조회했다. 노드 사이에서 재사용되는 PinId는 각각 14개/8개였다. 없는 그래프는 사전 거절됐으며 명시적 변경·컴파일·저장은 없었다. 원래 class_capture 실패와 후속 성공은 각각 보존했다. 테스트 에디터는 dirty 0에서 정상 종료했다.

같은 새 프로세스에서 소유 에셋 9개를 디스크에서 다시 로드하고 AnimBP 상태, Schema 15Hz, DB의 소유 Schema 참조, LookAt 맵의 소유 AnimClass override를 확인했다. `/Game` 의존성을 재귀 추적한 3,162패키지의 registry 경로에서 금지 MCP 참조는 없었고, 보호 원본·소유 에셋 파일 바이트도 유지됐다. 패키징/독립 런타임 실행과 동등한 증거라고 표현하지 않는다.

사용자가 메인 적용과 다음 학습 진행을 지시한 뒤 PID 7428의 dirty/PIE/활성 작업 0·열린 에셋 창 0을 확인하고 정상 종료했다. 이전 DLL/PDB를 복구용으로 보존하고 검증한 새 DLL/PDB를 실제 프로젝트에 적용했다. 새 메인 PID 4240/공식 MCP 8000에서 두 그래프의 29/23노드와 없는 그래프 거절을 확인했다. 처음에는 숨김 창으로 실행했으므로 작업 종료 시 clean 상태에서 정상 종료하고 일반 창의 PID 11208로 다시 열었다. 같은 DLL과 공식 MCP 연결, 소유 Traversal 맵의 저장 결과를 확인한 뒤 원래 맵·카메라로 복원했다. 최종 상태는 PIE false·dirty 0·활성 관측기 없음이다. DLL 적용은 완료됐으며 재시작 승인 대기가 아니다. 복구·해시·그래프 호출 근거는 `main-dll-deployment.json`과 `main-deployed-read-verification.json`, 최종 메인 상태는 `final-main-state.json`에 있다.

## 재현 가능한 호출 예제

공식 서버에 initialize → notifications/initialized를 보내고 반환 `Mcp-Session-Id`를 유지한 상태에서 호출한다. 저장소의 `cubeless/tools/official_mcp_client.py`가 이 연결을 처리한다.

```python
client.call_toolset(
    'cubeless_mcp_ue58.registration.graph_toolset.CubelessBlueprintGraphTools',
    'list_blueprint_nodes', {
        'blueprint_name': '/Game/Blueprints/SandboxCharacter_Mover_ABP',
        'graph_name': 'Update_CVarDrivenVariables', 'graph_type': 'function',
        'node_type': '', 'title_contains': '', 'include_pins': True,
        'graph_id': ''})

client.call_toolset(
    'cubeless_mcp_ue58.registration.pie_input_toolset.CubelessPIEInputTools',
    'inject_pie_input_key', {'key': 'W', 'event': 'pressed'}, mutation=True)
# 다음 프레임의 위치·속도를 확인하고, finally에서 released를 보낸다.
```

위 모듈명은 이번 실제 등록을 기준으로 한다. 결과가 불확실하면 입력을 재주입하지 말고 실제 PIE·controller·held key·Actor 상태를 먼저 확인한다. `partial_mutation_possible` 또는 false 반환을 실제 변경 없음으로 단정하지 않는다.

## 오판·API 차이와 해결

- Native 입력에 `pie_instance=-1`을 직접 전달하면 사전 거절된다. adapter의 기본 선택 의미는 인자 생략으로 처리한다. 알 수 없는 키도 변경 전에 정상 거절됐다.
- M/N 키는 고정 캐릭터 선택 API가 아니다. `GM_Sandbox`는 `DDCvar.PawnClass`/`DDCvar.VisualOverride` 변경과 비동기 로드를 사용한다. 실제 클래스·AnimInstance 준비를 확인한다. 이번 CMC 명시 선택은 PawnClass 0, 복원은 1에서 실제 Mover를 확인했다.
- 구조 schema는 속성 dictionary 자체다. getter 결과 키는 요청한 철자를 따른다. Montage 기록기의 `Speed2D` 철자 오류는 원시 결과의 실제 값을 이용해 검수 오판으로 바로잡았고 재생을 반복하지 않았다.
- Blueprint 상태는 enum으로 비교한다. `str(status).endswith('BS_UP_TO_DATE')`는 `<BlueprintStatus.BS_UP_TO_DATE: 3>`을 잘못 거절했다. 실제 상태를 읽고 소유 LookAt 예제만 별도 저장했다.
- `save_map`에는 파일시스템 `.umap` 경로가 아닌 `/Game/...` asset path를 전달한다. 실패 후 이미 생성된 램프를 재생성하지 않고 관측·별도 저장했다.
- 일반 속성 setter는 원본 LookAt 입력과 Retargeter 쓰기를 거절했다. 제한을 우회하지 않았다. LookAt은 소유 복사본의 공개 편집 계약을 작성했고 Retargeter는 정상 Blueprint 비동기 초기화 완료를 관측했다.
- 공식 `BlueprintTools.find_nodes`의 실제 HTTP 호출에는 `title=''`, `node_class=null`, `entry_points_only=false`를 명시했다. 일부 optional-looking 항목의 생략 오류를 빈 그래프로 해석하지 않는다.
- C++ 함수가 Python에도 그대로 노출된다고 가정하지 않는다. ChildActor는 실제 `Actor.get_all_child_actors`를 사용한다. 실패한 deferred spawn 호출은 Actor 생성 전 메서드 부재였으며 재시도하지 않았다.
- Cubeless viewport capture의 PIE 거절은 명시적 보호다. 공식 CaptureViewport의 transform 기본값 생략 오류와도 구분한다. 실제 렌더 캡처는 공개 Automation API로 수행했다.

## 측정 조건과 남은 공백

실제 관측 비용은 다음과 같다. 표본 간격은 관측 callback의 벽시계 간격이며 게임 스레드 CPU·GPU 시간이 아니다.

| 실행 | 벽시계 시간 / 표본 | 표본 간격 중앙값 |
|---|---|---|
| 기본 Mover | 14.00초 / 127 | 0.109초 |
| Mover MM 경로 | 10.00초 / 30 | 0.328초 |
| 소유 CMC 고정 DB | 7.266초 / 22 | 0.328초 |
| 램프 거리 기준 실행 | 12.328초 / 37 | 0.328초 |

기본 실행 이후 Windows가 잠겼고 백그라운드에서 관측했다. 나중에는 C++ 빌드도 병행했다. 따라서 위 표로 MM과 Blend Stack의 성능 우열을 결론 내리지 않는다. `search Cost`는 매칭 점수다. 진단 도구의 한 실제 호출은 엔진 측 약 0.53ms, HTTP tools/call 벽시계 약 0.334초였으며 이를 애니메이션 평가 시간으로 바꾸지 않는다.

후속 성능 작업은 잠금 해제·같은 전면 장면·캐릭터 수·DB·LOD·업데이트 조건을 고정하고 Rewind Debugger/Pose Search 분석과 Unreal Insights의 실제 animation CPU 측정을 함께 수행한다. 이번에는 비용 점수·선택 결과·실제 본/위치·벽시계 실행 시간을 수집했다. 여러 캐릭터 부하, LOD/update-rate 최적화 우열은 아직 실측 결론이 없다.

전용 guarded Chooser/DB 일괄 재구성, 비동기 retarget-ready 관측, 다중 참가자 전체 취소/소멸, moving platform·solver modifier 전체 수명, 깊은 AnimGraph 쓰기, 모든 노드 writer의 owner-scoped pin 지원은 남은 API/지원 범위다. 단일 캐릭터의 대표 Warp target 생성·갱신·참조 소멸·제거는 아래 후속 학습에서 확인했다. 이번 학습을 전체 애니메이션 API 완성이나 기존 UE 5.6 전체 이식 완료로 집계하지 않는다.


## Traversal 후속 학습

실제 메인 DLL 적용 이후 [Traversal](Traversal.md)의 Hurdle·Mantle, 취소 후 충돌/이동 복원과 Warp target의 생성·갱신·weak 참조 소멸·명시적 제거를 기존 도구 조합으로 확인했다. 새 등록 API와 C++ 변경은 없다. 상태 조회의 바깥 stage와 실제 map load 완료, Montage 중단 이벤트와 물리적 액션 목표 도달, Static 앵커의 이동 거절과 API 결함을 구분했다. 원시 실패는 보존하고 목표 도달이 확인된 정상 실행은 반복하지 않았다. 동적 최종 target transform과 moving platform solver 품질·전체 네트워크 경계는 이 결과에 포함하지 않는다.

## 발 보정·근접전 후속 구현

[FootWarping](FootWarping.md)의 소유 발 보정 예제와 [Melee](Melee.md)의 근접전 예제를 추가했다. 초기 소유 맵 설정·PIE 자동 승인 차단은 사용자의 명시적인 승인 후 해소됐고, 공식 MCP 8002에서 실제 발 보정 전환과 근접전 정상·중요 실패 경계를 확인했다. 표적의 Visibility Ignore 상속과 복사 AnimBP의 Chooser 문맥 클래스 오류는 소유 에셋에서 수리했다. 새 프로세스에서 충돌 기본값·배치 표적·GameMode/폰·Chooser 바인딩·Alpha 기본값·저장 바이트를 확인했다. 클래식 Orientation/Stride/Slope 노드의 새 예제나 전체 보행 품질을 완료했다고 주장하지 않는다. 언어별 노드 ID, DSL 내부 예외 전달과 IsValid macro/Sequence의 실제 핀 계약도 기록했다. 후속 구현의 로컬 근거는 `records/operations/game-animation-warping-melee-20261028/report.json`이다. 기존 실패를 지우거나 후속 수리를 원 요청 성공으로 바꾸지 않았다. 새 도구 등록·C++ 변경·빌드·Git 쓰기는 없다.

## 1대1 전투 후속 구현

[Duel](Duel.md)은 기존 Melee 에셋의 별도 복사본을 공식 BlueprintTools DSL·표준 엔진 API로 작성했다. 공식 MCP의 에셋 복사·함수/변수/이벤트 작성·컴파일/저장·맵 전환·PIE와 기존 Cubeless 입력 주입·Python 실행을 사용했다. 입력 주입과 관측기는 Editor 외부 검수용이며 생성한 게임 그래프에 들어가지 않는다. 새 C++·DLL·API 등록은 없다.

실제 AI의 공격 예고→우클릭 회피→W 접근→좌클릭 반격에서 플레이어 HP100·적 HP80과 적의 피격 회복을 확인했다. 별도 6개 대표 경계와 새 프로세스 저장 결과는 `records/operations/game-animation-duel-20261028/report.json`에 연결했다. Sequence의 기본 출력 2개, 상속된 자기 속성 getter의 외부 self 핀 부재, 벡터 핀 표현과 인스턴스 편집 제한은 실제 SDK 계약에 맞게 저작/검수 코드를 수정했다. 런타임 코드는 일반 Blueprint이며 패키징·MCP 비활성 게임 실행은 이번에 수행하지 않았다.
