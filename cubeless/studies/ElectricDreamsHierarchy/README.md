# Electric Dreams 계층형 어셈블리와 Ditch — UE 5.8.3

**현재 상태: 실제 구현·기본 동작·저장·재로드를 확인했으며 사용할 수 있다.** 기존 UE 5.7 Electric Dreams 분석을 이어서, UE 5.8의 기본 PCG 노드로 부모·자식 묶음 복사와 작은 Ditch 구조를 구현했다. [PCGBasics](../PCGBasics/README.md)의 스플라인·밀도·표면 투영 실습 다음 단계이다.

학습 맵은 **`/Game/Study/ElectricDreamsHierarchy/Maps/L_EDHierarchy`**이다. 맵 1개·PCG 그래프 3개·재질 6개, 총 **10개 파일 / 2,130,527 bytes**를 실제 프로젝트에 적용했다. 기본 메시로 구조를 보여 주는 학습 장면이며 Electric Dreams의 완성된 자연환경·원본 아트·물 시뮬레이션을 복제한 결과는 아니다.

## 출처와 기존 학습의 재사용

- [Epic: Procedural Content Generation in Electric Dreams](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-in-electric-dreams)의 Copy Points With Hierarchy / Apply Hierarchy를 기준으로 부모 변환 상속과 부모가 제외된 자식의 제거를 확인했다.
- 원본 저장소 `UnrealMCP5_6_1/Docs/Analysis/ElectricDreams/`의 다음 기록을 읽었다. 저장소 이름은 5.6이지만 `electric_dreams_pcg_extracted_settings_and_cubeless_candidates.md`는 실제 UE 5.7 commandlet 실행을 명시한다.
  - `electric_dreams_pcg_ditch_ground_hierarchy_usage_analysis.md`
  - `electric_dreams_pcg_cubeless_two_level_hierarchy_pass_result.md`
  - `electric_dreams_pcg_cubeless_ditch_riverbank_preset_pass.md`
  - `cubeless_ditch_hierarchy_prototype_result.md`
  - `build_spline_assembly_with_post_copy_offset.py`
- 위 자료의 과거 명령·경로·통신 브릿지는 실행 지시로 사용하지 않았다. 원본 프로젝트와 파일도 수정하지 않았다.

기존 자료에서 Ditch가 계층 복사를 여러 번 활용하는 실무 대상임을 확인했다. 이번에는 작은 12포인트 소스로 그 핵심 관계를 재현하며, 원본 대규모 그래프 전체를 이식했다고 주장하지 않는다.

## 5.8에서 달라진 구현

설치된 UE 5.8.3의 `PCGApplyHierarchy.h/.cpp`, `PCGCopyPoints.h/.cpp`, `PCGCreateAttribute.cpp`와 공식 `PCGToolset.cpp`를 확인했다. 공식 MCP의 실제 노드 스키마도 조회했다.

| 항목 | 과거 학습 경로 | 이번 UE 5.8 구현 |
| --- | --- | --- |
| 복사별 식별 | 별도 `PostCopyPoints-OffsetIndices` Blueprint로 정수 ID 보정 | `Apply Hierarchy`의 복합 키 사용 |
| 포인트 키 | 보정된 ActorIndex | `(AssemblyIndex, ActorIndex)` |
| 부모 키 | 보정된 ParentIndex | `(AssemblyIndex, ParentIndex)` |
| 그룹 번호 | 별도 보정 로직 | 복사 전 대상 포인트의 `$Index`를 Integer32 AssemblyIndex로 변환 |
| 부모 변환 적용 | 샘플의 계층 처리 경로 | 엔진 기본 `Apply Hierarchy` |
| 배치 메시 | 원본 프로젝트 메시 | Cube·Sphere·Cone과 자체 단색 재질 |

`ActorIndex` 단독 값은 그룹마다 0~11로 반복된다. **복합 키가 유일하며 ActorIndex 단독이 전역 고유하다고 해석하면 안 된다.** 각 소비 그래프에는 Copy Points 하나가 있다. 서로 다른 Copy 노드 출력을 같은 데이터로 합치려면 그룹 번호 충돌을 피하도록 추가 batch 키 등을 설계해야 한다. 이번 구현이 원본 보정 Blueprint의 모든 task ID 계약을 대체한 것은 아니다.

계층 노드는 부모 키 중 `-1`이 있으면 루트로 처리한다. 깊이 순서로 부모의 변환과 상대 변환을 합성하며, 유효한 부모·깊이를 잃은 포인트는 제외한다. 이 동작은 설치 소스와 아래 실제 결과로 확인했다. IgnoreParentRotation/Scale 속성에 의존하지 않도록 이번 그래프의 부모 회전·스케일 적용은 `Always`로 명시했다.

새 C++·사용자 PCG Blueprint 클래스·Python 런타임 서비스는 없다. 저장 그래프의 노드는 `/Script/PCG`의 엔진 기본 타입이다. **필수 플러그인은 PCG**이며, 공식 MCP와 Cubeless는 제작·관측에 사용했다.

## 에셋과 그래프 흐름

기준 폴더는 `/Game/Study/ElectricDreamsHierarchy`이다.

| 경로 | 역할 |
| --- | --- |
| `Graphs/SG_ED_DitchAssembly` | 로컬 계층·역할·방향을 가진 재사용 소스 12개 |
| `Graphs/PCG_ED_Assembly` | 서로 다른 위치·회전·스케일의 대상 두 개에 복사 |
| `Graphs/PCG_ED_Ditch` | 소유 스플라인을 샘플링하여 6묶음의 강둑 배치 생성 |
| `Maps/L_EDHierarchy` | 두 실습을 함께 볼 수 있는 저장 맵 |
| `Materials/M_ED_*` | Bank·Silt·Grass·Root·Floor·Water 단색 재질 |

소스 서브그래프:

```text
12개의 Create Points
 → 각 포인트의 ActorIndex / ParentIndex / HierarchyDepth
 → RelativeTransform / Role / SideMask
 → Merge Points → Out
```

포인트의 초기 Transform은 identity이고, 실제 부모 기준 오프셋은 RelativeTransform에 저장한다. Copy만 한 단계에서는 그룹의 포인트가 대상 위치에 모이며, 이후 Apply Hierarchy가 계층을 펼친다. 메시의 표시 크기는 계층 처리 뒤 역할별 Transform Points에서 적용하므로, 장식 메시 크기가 자식의 간격을 잘못 키우지 않는다.

두 소비 그래프의 공통 구조:

```text
SG_ED_DitchAssembly ───────────────────────→ Copy Points.Source
대상 포인트 → Attribute Cast($Index → AssemblyIndex) → Copy Points.Target
 → SideMask 범위 필터 → Density Filter
 → Apply Hierarchy(복합 키)
 → Role별 필터 → 표시 크기 → Static Mesh Spawner
 → Merge Points → Out
```

Copy Points의 Attribute Inheritance는 `SourceFirst`이다. 소스의 계층 정보와 대상의 AssemblyIndex를 함께 유지해야 하므로 SourceOnly로 바꾸지 않는다.

Assembly 예제의 두 번째 대상은 yaw **35°**, uniform scale **1.25**, 높이 오프셋 100 cm를 가진다. Ditch는 PCGVolume 소유 `DitchSpline`의 제어점 5개를 사용하며, NumberOfSamples=6이다. 물 표시는 대상 샘플에서 별도로 생성하는 길이 760 cm의 Cube 6개이다. 곡선을 따라 겹치는 단순 채널 표시이며 Water 플러그인·유체·침식 기능은 아니다.

### 소스 계층

좌표는 **부모 기준 cm**이며, 단위 스케일을 사용한다. Role은 0=루트, 1=강둑, 2=실트, 3=진흙, 4=식생, 5=바위이다.

| ID | 이름 | 부모 | 깊이 | 상대 위치 XYZ | Yaw | Role | Side | Density |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | Root | -1 | 0 | 0, 0, 0 | 0 | 0 | 0 | 1.00 |
| 1 | LeftBank | 0 | 1 | 0, -240, 20 | -12 | 1 | -1 | 0.96 |
| 2 | CenterSilt | 0 | 1 | 0, 0, -15 | 0 | 2 | 0 | 0.93 |
| 3 | RightBank | 0 | 1 | 0, 240, 20 | 12 | 1 | 1 | 0.96 |
| 4 | LeftLowerEdge | 1 | 2 | 0, -65, 20 | 0 | 5 | -1 | 0.86 |
| 5 | LeftUpperEdge | 1 | 2 | 100, -35, 45 | 0 | 5 | -1 | 0.84 |
| 6 | CenterMud | 2 | 2 | 100, 0, 10 | 0 | 3 | 0 | 0.65 |
| 7 | CenterGrass | 2 | 2 | 0, 100, 35 | 0 | 4 | 0 | 0.80 |
| 8 | RightLowerEdge | 3 | 2 | 0, 65, 20 | 0 | 5 | 1 | 0.86 |
| 9 | RightUpperEdge | 3 | 2 | 100, 35, 45 | 0 | 5 | 1 | 0.84 |
| 10 | LeftRockCap | 5 | 3 | 60, 0, 55 | 0 | 5 | -1 | 0.78 |
| 11 | RightRockCap | 9 | 3 | 60, 0, 55 | 0 | 5 | 1 | 0.78 |

SideMask와 밀도 필터는 계층 적용 전에 있다. 같은 쪽의 자식은 부모와 Side를 공유하고, 자식 Density는 부모보다 높지 않게 작성했다. 따라서 정상적인 한쪽 선택·밀도 하한 변경은 남은 자식만 고립시키지 않는다.

## 실제 검증 결과

UE **5.8.3-58210709**, 분리 프로젝트의 공식 MCP에서 실행했다.

| 검사 | 확인한 결과 |
| --- | --- |
| 소스 계층 | 12개, 깊이 0/1/2/3 = 1/3/6/2 |
| Assembly 복사 | 2그룹 × 12 = 24개 포인트·인스턴스 |
| Ditch 복사 | 6그룹 × 12 = 72개 계층 포인트 |
| Ditch 표시 | 계층 메시 72개 + 물 표시 6개 = 78개 |
| 그룹 관계 | 복합 키 중복·부모 누락·다른 그룹 부모 연결 0개 |
| 변환 | 원본 상대 위치·yaw와 대상 변환을 별도 합성해 실제 계층 결과와 대조 |
| 실제 배치 | 최종 PCG 포인트의 위치·회전·스케일과 ISM 월드 변환을 모두 대조 |
| 동일 입력 | 두 예제 재생성의 위치·개수 유지 |
| Ditch SideMask 0~1 | 오른쪽+중앙 48개 계층 + 물 6개 = 54개 |
| Ditch Density 하한 0.9 | 루트·강둑·실트 24개 + 물 6개 = 30개 |
| SideMask 2~3 | 계층 출력 0개, 기존 계층 메시 제거, 물 6개만 유지 |
| ActorIndex 2~11 필터 | 루트가 사라진 계층의 자식이 제외되고 물 6개만 유지 |
| 복원 | 각 변경 후 SideMask -1~1·Density 하한 0으로 원래 배치 복원 |

변환 비교는 부모 위치에 회전·스케일을 적용한 상대 오프셋을 더하는 방식으로 수행했다. 이번 표본의 최대 위치 오차는 약 `1.14e-13 cm`였으며, 이를 일반적인 엔진 정밀도 보장으로 확대하지 않는다. 루트 표시와 물 표시는 의도적으로 같은 위치를 공유하므로 위치 중복만으로 오류를 판정하지 않았다. 실제 비교에는 회전·스케일까지 포함했다.

마지막 물 표시 길이를 370→760 cm로 조정해 채널이 이어지게 했다. 인스턴스 수는 같고, 다른 72개 계층 메시 변환이 그대로임을 확인했다. 위치 변경이나 계층 변형 시험을 다시 실행한 것으로 계산하지 않았다.

세 그래프와 맵을 저장하고 다른 소유 맵으로 전환했다가 다시 열어 24/78개 배치와 스플라인 제어점 5개가 유지되는 것을 확인했다. 이후 최종 저장 파일을 **PCG는 활성화되고 MCP 모듈 5개는 없는 새 프로세스**에서 읽어 최종 물 크기·그래프·복합 키·스플라인·배치를 확인했다. null RHI 저장 로드 검사이며, 화면은 별도 GPU 에디터에서 캡처했다.

저장 의존성은 **19개 패키지**, 금지 참조 0개로 확인했다. 패키징 게임 실행·게임 성능 측정은 이번 결과에 포함하지 않는다.

## 직접 확인하는 짧은 절차

1. 현재 사용자 작업을 필요한 방식으로 저장한 뒤 `L_EDHierarchy`를 연다. 이번 적용에서는 메인 에디터의 dirty Blueprint를 보존하기 위해 월드를 바꾸지 않았다.
2. `ED_Assembly`와 `ED_Ditch`로 시작하는 PCGVolume을 확인한다. 파란 작은 루트·강둑·바위·식생 표시가 있다. 원본 Electric Dreams 아트가 필요한 구성은 아니다.
3. `PCG_ED_Assembly`의 Targets에서 두 번째 대상 회전·스케일과, `SG_ED_DitchAssembly`의 `P1_RelativeTransform` 등을 함께 본다. 소스는 두 소비 그래프가 공유한다.
4. Ditch 그래프의 Side에서 `MinThreshold.AttributeTypes.DoubleValue`를 -1→0으로 바꾸면 오른쪽+중앙만 남는다. -1로 복원한다.
5. Density의 LowerBound를 0→0.9→0으로 바꾸어 78→30→78개를 확인한다. 물 6개는 별도 분기이므로 이 필터로 없어지지 않는다.
6. 진행 중인 자동 생성이 있다면 끝날 때까지 상태를 확인한다. 호출 타임아웃을 생성 취소로 해석하거나 불확실한 수정을 재실행하지 않는다. 그래프 변경을 보관하려면 해당 그래프와 소유 맵을 명시적으로 저장한다.

## 공식 MCP 호출 예제

실제 등록된 `PCGToolset.PCGToolset`을 사용한다. 구조 조회의 `call_tool` 인자는 다음과 같다.

```json
{
  "toolset_name": "PCGToolset.PCGToolset",
  "tool_name": "GetGraphStructure",
  "arguments": {
    "graph": {
      "refPath": "/Game/Study/ElectricDreamsHierarchy/Graphs/PCG_ED_Ditch.PCG_ED_Ditch"
    }
  }
}
```

`ListGraphInstances`는 인자 `{}`로 현재 월드의 인스턴스를 조회한다. 원하는 그래프의 실제 Actor refPath를 선택해 `ExecuteGraphInstance`의 `pcgVolume`으로 전달한다. 선택 상태나 PC별 임의 Actor 접미사를 고정하지 않는다.

- `GetNativeNodeSchema`: Apply Hierarchy·Attribute Cast 등의 실제 입력 계약 확인.
- `UpdateNode`: 확인한 노드 refPath와 `jsonParams`로 설정 변경. 반환 true만으로 생성 완료를 판정하지 않는다.
- `GetNodeDataView`: Targets→Copy→Hierarchy→Final의 개수·속성·변환 비교.
- Cubeless `inspect_pcg_component_outputs`: generating/cleaning_up/refresh_in_progress 상태 확인.
- Cubeless `inspect_pcg_placement`: 실제 ISM의 월드 변환과 개수 확인.

맵 재로드 후에는 inspection 데이터가 초기화될 수 있다. 첫 Data View 요청의 자료 없음은 생성 실패와 구분한다. inspection을 활성화한 뒤, 진행 요청이 없는 상태에서 명시적으로 Generate해야 중간 데이터를 볼 수 있다. 저장된 메시의 유지 여부는 별도 배치 관측으로 확인할 수 있다.

## 시행착오와 범위

- 공식 `ConnectNodePins`는 void/null을 반환한다. 처음 작성한 호출기가 이를 false로 잘못 판정했지만 연결은 이미 있었다. 실제 구조를 읽어 해당 연결을 보존하고 누락된 부분만 이어갔다.
- 그래프·맵 저장 뒤 로그를 만들 때 Unreal Array를 바로 JSON 직렬화해 호출이 실패했다. 실제 저장·dirty 상태를 읽은 뒤 생성·저장을 재실행하지 않고 검증을 이어갔다.
- 공식 연결 API는 Any→Point 연결에 `FilterDataByType_0`를 자동 추가했다. 초기 cold 검수의 노드 수 가정이 틀렸으며, 실제 목록에 맞춰 정정했다. 최종 내부 노드 수는 소스 85개, Assembly 27개, Ditch 30개이고 기본 Input/Output은 별도이다. 이 수치는 기능 진행률이 아니다.
- 물 크기 수정은 정상 반영됐지만, 재로드 뒤 초기화된 inspection 자료를 요구한 후속 관측이 실패했다. 실제 배치에서 물 6개만 크기가 달라지고 나머지 72개가 유지된 것을 확인했으며 수정·생성을 재실행하지 않았다.

이번 검증은 정해진 소스 12개, 단일 Copy 노드, 양의 uniform 대상 스케일과 회전을 다룬다. 여러 Copy 출력을 합치는 키 설계, 음수·비균일 스케일, 원본 assembly exporter, Landscape 접지·레이어·평탄도 검사, Forest·Ground 전체, 대규모·GPU·World Partition·런타임 생성은 이번 완료 범위가 아니다. 모든 기존 5.7 PCG 기능의 이식 완료를 뜻하지 않는다.

기존 에셋 49개와 사용자의 dirty Blueprint를 보존하고 새 폴더에만 적용했다. 자체 검토에서 소유 경로·복합 키·부모 관계·변환·빈 출력·저장·원시 실패 보존·배포 파일 일치를 확인했으며 독립 리뷰라고 주장하지 않는다.

원시 호출·실패·수치·시간·화면은 로컬 `cubeless/records/operations/electric-dreams-hierarchy-study-20261026/report.json`과 같은 폴더에 보존했다. 작성용 Python·검증 프로젝트·raw 자료는 Git 제외이며 이 Markdown과 Study 에셋은 Git 포함 대상이다. **이번 작업에서는 Git stage·commit·push를 하지 않았다.**


## 관련 후속 학습

[PCG Manual Editing](../PCGManualEditing/README.md)에 UE 5.8의 포인트 수동 이동·오버라이드 저장·재생성 연결 조건을 정리했다. 최종 개체 편집과 계층 묶음 편집의 차이를 설명하며, 이번 계층 그래프에 편집 노드를 추가한 것은 아니다.
