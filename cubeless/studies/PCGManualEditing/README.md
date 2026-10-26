# PCG Manual Editing — UE 5.8.3 포인트 수동 편집 학습

2026-10-26. **공식 웹 자료·설치 소스 학습과 실제 수동 위치 편집·Undo/Redo·재생성·저장·맵 재로드를 확인했다.** UE 5.8.3의 기본 포인트 수동 편집을 사용할 수 있는 상태다. 별도 소유 프로젝트에서 컴퓨터 유즈로 UI를 조작하고 공식 MCP로 실제 메시 인스턴스 변환을 대조했다. 회전·스케일, PCG 모드의 별도 Manual Edit 도구, GPU 경로까지 이번 결과로 확대하지 않는다.

## 공식 자료와 기능 이름

- [UE 5.8 Release Notes — PCG / Manual Editing](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-8-release-notes): 생성 데이터·결과의 선택, 제외, 수정, 복원과 노드별 수동 편집, Data Overrides 창을 설명한다.
- [Epic: Unreal Engine 5.8 is now available](https://www.unrealengine.com/news/unreal-engine-5-8-is-now-available): 절차적 생성 위에 수동 수정을 얹고 상류 파라미터를 계속 조정하는 제작 방식을 소개한다.
- [UPCGEditPointsSettings](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/PCG/UPCGEditPointsSettings?lang=en-US): 포인트 출력 뒤에 넣는 `Edit Points` 노드의 용도와 선언이다.
- [PCG Editor Mode](https://dev.epicgames.com/documentation/unreal-engine/pcg-editor-mode-in-unreal-engine): 모드 진입과 기존 Paint·Spline 도구의 설명이다. Paint는 충돌 표면에 새 포인트를 찍는 기능이며, 기존 생성 포인트를 옮기는 Manual Editing과 구분해야 한다. 이 개요만으로 새 수동 편집 기능 전체의 사용법을 판단하지 않는다.
- [IPCGDeltaViewportExtension](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/PCGEditor/IPCGDeltaViewportExtension?lang=en-US): 수동 편집 뷰포트 동작과 패널 UI를 연결하는 엔진 인터페이스이다.

릴리스 노트의 Manual Editing, 그래프의 `Edit in Viewport`, 노드 이름 `Edit Points`, PCG 모드의 `Manual Edit`는 관련된 기능군이다. 동일한 버튼·실행 경로라는 뜻은 아니다. 웹 검색에서 5.7 URL이 5.8 제목·내용을 반환하는 사례가 있었으므로 최초 도입 버전을 그 결과로 역추정하지 않았다. 여기서는 설치된 **UE 5.8.3 / CL 58210709**를 기준으로 한다.

YouTube에서는 PolyBoost의 [PCG + Mesh Terrain Workflow in Unreal Engine 5.8](https://www.youtube.com/watch?v=6rJRbsTJAck)를 찾았다. 검색 결과의 소개에 Manual Editing이 포함되어 있지만 영상 본문 접근이 제한됐다. 따라서 영상을 시청·재현했다고 주장하지 않으며, 실행 예제는 위 공식 웹 문서의 포인트 출력 → Edit Points 구성과 설치 소스의 UI 계약을 기준으로 선택했다. Mesh Terrain까지 필요한 큰 장면 대신 기능을 분리한 3포인트 예제다.

## 실제 실습 결과

- 프로젝트: `cubeless/workspaces/PCGManual58/PCGManual58.uproject`. 20자 이하의 별도 소유 프로젝트이며 일반 창 모드로 실행했다. 공식 MCP 포트는 이번 실행에서 8003이다. 기존 메인·다른 검증 에디터는 종료·재시작하지 않았다.
- 소유 에셋 루트: `/Game/_MCP_Temp/PCGManualEditing/CubelessTests/ME_20261026`.
- 맵: `Map_ManualEdit`, 그래프: `Graphs/PCG_ManualPoints`, Actor Label: `ManualEditStudy`.
- 그래프: `Create Points(Source) → Edit Points(Manual) → Static Mesh Spawner(Spawn) → Output`. 기본 Cube 3개를 사용한다.

| 확인 항목 | 실제 관측 |
| --- | --- |
| 기본 생성 | 위치 `(-400,0,100)`, `(0,0,100)`, `(400,0,100)`, 스케일 모두 `(1,1,1)`, 인스턴스 3개 |
| UI 진입 | 그래프 Debug Object Tree의 실제 실행 컴포넌트 선택 → Manual 노드 선택 → `V` → 레벨 뷰포트 편집 패널·포인트 기즈모 표시 |
| 수동 이동 | 뷰포트 패널의 Point Transform 위치 Z를 100에서 300으로 입력·확정. 실제 가운데 메시만 `(0,0,300)`으로 변경 |
| Undo / Redo | Edit 메뉴의 `PCG 수동 편집: 포인트 트랜스폼 값 업데이트` Undo로 Z=100, Redo로 Z=300. 다른 두 인스턴스는 동일 |
| 재생성 | 완료 상태를 별도 관측하고 가운데 Z=300 유지 확인 |
| 원본 생성 규칙 | Source의 authored 높이 세 개는 모두 Z=100. 수동 오버라이드가 원본 생성 포인트 값을 덮어쓰지 않음 |
| 저장·재로드 | 그래프와 소유 맵 저장 → 다른 엔진 템플릿 맵으로 전환 → 저장 맵 재로드 → 재생성 후 3개와 가운데 Z=300 유지 |
| 상류 위치 변경 경계 | 소유 Source의 가운데 Z를 150으로 변경하고 캐시를 비워 재생성하면 결과도 Z=150. 원래 Z=100으로 복원하고 같은 절차를 수행하면 기존 오버라이드 Z=300이 다시 적용됨 |
| 최종 상태 | Source Z=100/100/100, 실제 메시 Z=100/300/100로 복원·저장. 테스트 에디터 dirty 0, PIE/SIE false. 저장 의존성 검사 통과 |
| 사용자 작업 보존 | 메인 프로젝트의 dirty `/Game/Study/LyraBasics/Blueprints/BP_StudyCoordinator` 유지. 실제 프로젝트의 기존 에셋·월드·에디터에 수정 요청을 보내지 않음 |

**이번 이동 성공 근거는 패널의 숫자 입력이다.** 기즈모 선택은 확인했으나 자동화 드래그 시도에서는 실제 좌표 변경이 관측되지 않았다. 이를 기즈모 드래그 성공이나 엔진 결함으로 기록하지 않는다. 드래그 시 생긴 변경 없는 transaction과 선택 변경이 Undo 위에 쌓였으므로, Undo 메뉴의 실제 작업 이름을 확인한 뒤 좌표 변경 취소를 검증했다.

`generate(True)`만으로 캐시가 모두 새로 계산됐다고 가정하면 안 됐다. Source를 Python으로 바꾼 뒤 초기 재생성에서는 이전 결과가 보였고, 이 소유 프로젝트에서 `pcg.FlushCache` 후 재생성한 결과로 상류 변경 경계를 판정했다. 이 명령을 사용자 프로젝트에서 자동 실행하는 운영 절차로 추가하지 않았다.

원시 결과는 `cubeless/records/operations/pcg-manual-edit-study-20261026/`에 보존했다. 주요 증거는 `scene.json`, `observe_numeric_move.json`, `observe_undo_transform.json`, `observe_redo.json`, `observe_regenerated_after_reload.json`, `observe_uncached_boundary.json`, `observe_restored_binding.json`, `save_final.json`, `finish.json`이다. 최종 뷰포트 이미지는 같은 폴더의 `manual-points-final.png`이다. 로컬 테스트 프로젝트·보조 Python·원시 자료는 Git 제외이며 이 Markdown은 Git에 포함할 수 있다. stage·commit·push는 하지 않았다.

이번에 확인하고 보존한 실패·무변경 기록:

- 초기 읽기 보조 코드가 존재하지 않는 `PCGComponent.is_generating`, 저장 전 코드가 `PCGGraph.get_nodes`를 호출해 실패했다. 해당 요청의 에셋 writer에 도달하지 않았으며 실제 상태를 확인한 뒤 별도 관측·저장으로 이어갔다. 원 실패는 성공으로 바꾸지 않았다.
- Python `load_map`은 `python_map_transition_blocked`로 실행 전에 거절됐다. 공식 Registry의 `SceneTools.load_level` 경로를 사용하고 반환값만으로 판정하지 않고 실제 월드·재생성 결과를 확인했다.
- 처음 Source 포인트 배열의 원소 필드만 수정한 코드는 복사본을 바꿔 실제 높이가 100으로 유지됐다. 실제 readback 후 배열 원소를 다시 대입하는 보조 코드를 사용했다. 이 무변경 시도를 성공으로 집계하지 않았다.
- 처음 창이 보이지 않는다는 이유로 UI 사용 불가를 추론한 것은 잘못이었다. 새 프로젝트를 `WindowStyle Normal`로 실행한 뒤 사용자가 정렬한 두 Unreal 창에서 컴퓨터 유즈를 수행했다. 창 조회·캡처만으로 헤드리스 여부를 단정하지 않는다.

## 세 가지 진입 경로

아래 절차는 설치 소스의 명령·처리를 대조한 안내다. 그래프의 `Edit in Viewport` → 수동 위치 입력 경로는 위 실습에서 확인했다. 별도 PCG 모드 Manual Edit의 Apply/Cancel 경로는 소스 학습 범위다. PCG와 PCGEditor가 활성·로드된 에디터가 전제이다.

| 경로 | 대상과 역할 | 소스에서 확인한 차이 |
| --- | --- | --- |
| 그래프 노드 → `Edit in Viewport` | 검사 중인 실행 소스의 특정 노드를 편집 | 기본 단축키 `V`. 하나의 임시 편집 노드를 활성화하며, 검사 소스가 PCGComponent이면 소유 Actor·컴포넌트를 선택하고 화면을 맞춘다. |
| 노드 → `Mark for Edit` | 계속 편집할 노드를 표시 | 내부 명령 이름은 `Mark for Viewport Editing`. 표시 플래그가 그래프에 저장된다. 임시 표시와 달리 세션 사이에 유지하도록 선언되어 있다. |
| Modes → PCG → `Manual Edit` | 선택 Actor의 PCG Static Mesh Spawner 결과 편집 | 그래프가 있는 PCGComponent를 가진 Actor 하나가 필요하다. 하위 그래프까지 Static Mesh Spawner를 찾아 임시 편집을 켠다. 이 도구 클래스는 Experimental이다. |

그래프 중간의 명시적인 편집 지점이 필요하면 다음처럼 배치한다.

```text
포인트 생성 / 필터
 → Edit Points
 → 후속 처리
 → Static Mesh Spawner
```

실행 소스를 검사 대상으로 지정하고 포인트가 생성된 상태에서 `Edit Points` 또는 적합한 노드의 `Edit in Viewport`로 진입한다. 포인트를 선택하고 기즈모로 조정한 뒤, 후속 배치와 오버라이드를 확인한다. 노드 선택만으로 항상 편집 가능한 데이터가 준비된다고 가정하면 안 된다.

PCG 모드의 Manual Edit 팔레트 표시 변수는 `pcg.Editor.EnableManualEditPalette`이며 설치 기본값은 `true`이다. 이는 **팔레트 표시 조건**이다. 이 변수 하나가 모든 그래프·데이터를 편집 가능하게 만들지는 않는다.

## 실제 실행 구조와 저장 위치

`FPCGEditPointsElement::ExecuteInternal`은 입력을 출력에 전달한다. 이동 로직은 이 함수 내부가 아니라 공통 Data Override 실행 경로에 있다.

```text
뷰포트 선택·기즈모 편집
 → 원래 변환 + 편집 변환 등을 Delta로 기록
 → 실행 소스의 FPCGSourceDataContainer
 → 노드/실행 스택/핀에 대응하는 DeltaCollection 검색
 → 생성 포인트에 Override 적용
 → 후속 노드 또는 메시 배치
```

- `IPCGElement`는 오버라이드 적용 단계를 처리한다. 보통 출력의 PostExecute 단계이며, Static Mesh Spawner는 입력 준비 단계인 PrepareData를 사용한다. 따라서 스포너 결과를 조정하는 경로와 그래프 중간 데이터를 편집하는 경로를 구분해야 한다.
- `Point Transform`은 켜진 위치·회전·스케일 항목을 지정 변환으로 덮어쓴다. `Point Transform Offset`은 위치 가산·회전 합성·스케일 곱을 적용한다. 모든 수동 이동이 상대 오프셋이라고 설명하면 잘못이다.
- `Point Deletion`, `Point Insertion` 구현도 있다. 다만 이번 조사로 임의 데이터·GPU 경로·모든 스포너에 대해 동일한 편집 지원을 확인한 것은 아니다.
- PCGComponent의 `SourceDataContainer`는 직렬화 대상 UPROPERTY이며, 컨테이너에는 사용자 정의 직렬화가 있다. 컴포넌트 시각화 편집은 `PCGComponent->Modify()`를 호출한다. **노드의 편집 표시를 저장하는 것과, 해당 실행 소스의 오버라이드를 저장하는 것은 별개다.** 일반적인 레벨 배치 컴포넌트라면 변경 소유 패키지·외부 Actor 패키지를 확인해야 한다. 그래프 에셋만 저장했다고 수동 이동까지 보존됐다고 판단하지 않는다.
- 컨테이너는 런타임 읽기 전용으로 선언되어 있다. 이것을 게임 실행 중 사용자가 편집하는 도구로 해석하지 않는다. 이번에 게임 패키징 동작을 검증한 것은 아니다.

## 재생성 시 편집이 유지되는 조건

수동 편집은 원본 생성 규칙을 없애지 않는다는 의미에서 비파괴적이다. **상류를 어떻게 바꿔도 동일 개체에 영구 연결된다는 보장은 아니다.**

설치 경로의 기본 컬렉션은 `Spatial / Position / SpatialTolerance=10.0`을 사용한다. `ComputePositionHash`는 좌표를 tolerance로 나눈 후 반올림한 격자 좌표를 해시한다. 따라서 10이라는 값은 단순한 “반경 10 cm 안에서 가장 가까운 점을 찾는다”는 뜻이 아니다. 격자 경계와 후보 bounds 검색도 영향을 준다. 엔진 자체 주석에도 큰 위치·bounds 변경이 오버라이드를 고립시킬 수 있다고 명시되어 있다.

| 변경 | 학습 결과와 판단 |
| --- | --- |
| 같은 입력으로 재생성 | 이번 3포인트 예제에서 수동 이동·맵 재로드 이후 재생성 지속성을 확인했다. |
| Seed·샘플 간격·스플라인 변경 | 원래 위치가 바뀌면 대응을 잃을 수 있다. 이번에는 Source Z=100→150으로 공간 대응 해제, 100 복원으로 재적용을 확인했다. Seed·스플라인 변형까지 실행한 것은 아니다. |
| 같은 위치에 여러 포인트 | 위치 키가 충돌할 수 있다. 공통 ApplyDataOverrides에는 collision handling TODO가 있으므로 완전한 충돌 해결을 가정하지 않는다. |
| 노드 교체·그래프 이동·루프 구조 변경 | 저장 키가 실행 스택의 경로·루프 인덱스·노드·핀과 관련되므로 대응을 다시 확인해야 한다. 일반적인 무손실 재연결을 보장하지 않는다. |
| 사용자 ID 속성 부여 | `ByIndex`·`ByAttribute` enum 존재만으로 안정적인 ID 재연결이 구현됐다고 판단하지 않는다. 확인한 point Resolve/ComputeKey 경로는 공간 정보로 키를 계산한다. |

`ApplyDataOverrides`가 내부 `Apply`의 bool 반환을 이용해 모든 변경 성공을 보장하는 구조는 아니므로, 패널의 연결 표시만으로 실제 이동·저장 성공을 확정하지 않는다. 최종 포인트·인스턴스 변환과 저장 후 결과를 확인하는 것이 필요한 이유다. 엔진 결함을 실제 재현했다고 주장하는 것은 아니다.

## 편집 취소와 복원

PCG 모드의 `UPCGManualEditTool`은 Accept/Cancel을 제공하고, Cancel에서는 세션 transaction 시작 지점까지 Undo를 요청한다. 반면 별도 `UPCGEditPointsInteractiveTool`의 소스에는 현재 UI가 자동 Accept이며 Accept/Cancel UI가 없다는 TODO가 있다. **서로 다른 경로의 취소 동작을 하나로 설명하면 안 된다.**

시각화 경로에는 선택 오버라이드 제거·전체 오버라이드 제거와 복원 동작이 있다. 오버라이드 제거는 수동 편집을 지우고 절차적 결과로 돌아가는 작업이며, 원본 생성 노드를 삭제하는 것과 다르다. Undo·Cancel도 원자적 롤백이나 디스크 파일 복구를 뜻하지 않는다. 이번에는 좌표 입력의 Undo/Redo와 실제 인스턴스를 대조했다. 별도 Manual Edit 모드의 Cancel이나 삭제·삽입 동작까지 확인한 것은 아니다.

## 현재 Electric Dreams / Ditch 학습에 적용할 기준

[기존 학습](../ElectricDreamsHierarchy/README.md)의 그래프는 부모·자식 계층과 어셈블리 대상 포인트를 분리한다. 아래는 그 구조와 엔진 구현으로부터 도출한 적용 제안이며 아직 그래프를 변경하지 않았다.

| 하고 싶은 편집 | 적합한 위치와 이유 |
| --- | --- |
| 바위·풀 한 개만 미세 조정 | Apply Hierarchy 이후 역할별 스포너 이전에 편집 지점을 두거나, 해당 스포너의 Manual Edit을 이용한다. 마지막 Transform Points가 시각적 크기를 덮어쓰는지도 확인한다. |
| 강둑 묶음 전체를 이동·회전 | `Copy Points`의 대상 포인트를 복사·계층 계산 전에 편집하는 편이 구조에 맞는다. 그러면 그 대상에서 파생되는 묶음의 배치에 영향을 줄 수 있다. |
| 계층 부모와 자식 전체 편집 | 최종 부모 포인트 하나를 옮기는 것만으로는 이미 계산된 자식이 자동 갱신되지 않는다. 현재 그래프는 authored `RelativeTransform`을 계층 합성에 사용하므로 그 값이나 합성 전 입력을 편집해야 한다. 기본 Point Transform 도구가 RelativeTransform 속성까지 수정한다고 가정하지 않는다. |
| 물 표시와 강둑을 함께 조정 | 현재 물은 별도 Target 분기다. 공통 대상 포인트가 분기되기 전에 편집하는 구조가 적합한 후보이다. |

우리 MCP가 노드를 추가·설정할 수 있다는 사실과 이 **실행 소스별 Delta 편집 계약**을 공개 도구로 지원한다는 사실은 별개다. 이번에는 도구 등록·스키마를 새로 조사하거나 MCP API를 추가하지 않았다. 현재 지원·미지원 여부를 과거 등록 수로 단정하지 않는다.

## 직접 확인하는 짧은 절차

위 소유 프로젝트의 두 Unreal 창을 열어 둔 상태다. 새로 열 때는 해당 `.uproject`에서 위 맵·그래프를 선택한다. 이미 실행 중인 동일 프로젝트를 중복 실행하지 않는다.

1. 그래프 아래 Debug Object Tree에서 `ManualEditStudy`를 펼치고 실제 컴포넌트 행을 선택한다.
2. `Manual` 노드를 선택하고 `V`를 누른다. 레벨 창에 Viewport Editing 패널이 나타나는지 확인한다.
3. 가운데 포인트를 선택하고 Point Transform 위치 Z를 변경해 Enter로 확정한다. 현재 저장 결과는 Z=300이며 원래 생성 값은 100이다.
4. Edit 메뉴에서 작업 이름을 확인해 Undo·Redo하고 가운데 메시만 달라지는지 확인한다. 선택·변경 없는 기즈모 transaction이 있으면 Undo 한 번이 좌표 변경에 해당하지 않을 수 있다.
5. 그래프 재생성 후 유지 여부를 확인한다. 편집을 보존할 때는 그래프뿐 아니라 소유 맵도 저장한다.

회전·스케일, 실제 기즈모 드래그 성공, 삭제·삽입, PCG 모드 Apply/Cancel, GPU, 계층 자식 전파와 게임 패키징은 이번 기본 위치 실습과 별도 범위다. Ditch에 적용할 때는 복사본에서 개별 개체와 어셈블리 대상 편집을 구분한다.

새 C++·엔진 수정·MCP API 추가 없이 기존 공식 MCP 클라이언트와 로컬 학습 보조 Python을 사용했다. 독립 서버나 제품용 외부 Python 파이프라인은 추가하지 않았다. Git stage·commit·push도 수행하지 않았다.

## 설치 소스 대조 위치

기준 루트: `C:/Program Files/Epic Games/UE_5.8/Engine/Plugins/PCG/Source/`. 아래 경로는 설치 소스 내 상대 위치이며 모두 읽기만 했다.

| 파일·함수 | 확인 내용 |
| --- | --- |
| `PCG/Public/Elements/PCGEditPoints.h`, `PCG/Private/Elements/PCGEditPoints.cpp` | 노드 이름·입출력과 pass-through 실행 |
| `PCGEditor/Private/PCGEditorCommands.cpp:52`, `PCGEditor.cpp:2888` 부근, `Nodes/PCGEditorGraphNodeBase.cpp:509` | Edit in Viewport / V, 임시·지속 표시, 메뉴 이름 |
| `PCG/Public/PCGSettings.h:240` 부근 | 임시 플래그와 그래프에 저장되는 표시 플래그 |
| `PCGEditor/Private/EditorMode/Tools/ManualEdit/PCGManualEditTool.h/.cpp` | Experimental, Actor 선택 조건, 스포너 탐색, Accept/Cancel·Undo |
| `PCGEditor/Private/EditorMode/PCGEdModeToolkit.cpp:35` | Manual Edit 팔레트 변수·기본값 |
| `PCG/Private/PCGElement.cpp:616,690`, `PCG/Private/Elements/PCGStaticMeshSpawner.cpp:907` | 공통 적용 단계와 스포너 PrepareData |
| `PCG/Public/Graph/DataOverride/PCGDataOverride.h`, `PCG/Private/Graph/DataOverride/PCGDataOverride.cpp:124` | 설정·enum과 실제 공간 키 계산 |
| `PCG/Private/Graph/DataOverride/PCGDataOverridePoints.cpp:9,33,74,117` 부근 | 후보·Resolve, 절대 변환·상대 변환 |
| `PCG/Private/Graph/DataOverride/PCGDataOverrideHelpers.cpp:23,173,342` 부근 | 실행 스택·공간 해시·공통 적용과 TODO |
| `PCG/Public/PCGComponent.h:864`, `PCG/Public/Graph/PCGSourceDataContainer.h` | 실행 소스별 저장·직렬화·런타임 읽기 전용 |
| `PCGEditor/Private/PCGComponentVisualizer.cpp:1271` 부근 | 기즈모 완료 시 transaction·Modify·Delta 반영 |
| `PCGEditor/Private/AssetEditorMode/Tools/PCGEditPointsInteractiveTool.cpp:425,598` 부근 | 별도 도구의 Accept/Cancel TODO·컬렉션 초기값 |

공식 웹 자료는 기능 범위와 용어의 근거이고, 세부 저장·키·취소 분석은 위 설치 소스의 근거다. 실제 UI·재생성·저장 증거는 위 3포인트 위치 실습 범위이며 패키징·성능까지 실행 검증한 것으로 확대하지 않는다.
