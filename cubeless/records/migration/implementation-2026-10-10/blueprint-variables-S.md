# S: Blueprint 변수 상태와 다음 그래프 구간

S/T 통합 ST2 증분 빌드와 공식 8 Toolset/48도구 등록, S 변수 대표 파이프라인 실행을 통과했다. 실제 Codex 연결의 새 프로세스 재조회도 통과했다. 변수 상태를 원본 물리 예제에서 추출했으며 전체 물리·이벤트 그래프 파이프라인을 이식했다고 처리하지 않는다.

원본 `Python/scripts/node/test_physics_variables.py`(SHA256 `d67c35ca2eb6f6a6652091f07614a05bcf65abc293caae1e04831fe52605d9da`)의 순서는 Actor BP 생성 → Float Mass10/RotationSpeed100/BounceFactor.8·Boolean IsTrigger=false 추가 → StaticMesh 추가 → physics 설정 → BeginPlay/Tick → SetMassScale/AddTorqueInRadians → exec 연결 → compile → 액터4개 생성이다. 변수와 함수 핀을 실제 연결하지 않고 주석으로만 후속 동적 연결을 예고한다. 이벤트 입력도 event_type인데 현재 원본 handler는 event_name을 요구한다. 응답 상태 검사·stdout 외 저장/재로드·독립 값 검수는 없으며 실패 branch의 return이 프로세스 실패 코드를 보장하지 않는다.

S 대표 흐름은 이 네 변수의 이름·타입·기본값을 보존한다. Mass는 hidden variable_guid를 명시하고 is_exposed=false/is_editable=true의 OR 노출 규칙을 확인한다. category/tooltip/friendly_name와 작은 metadata를 추가한 뒤 exact 변수·메타데이터를 조회한다. Mass 기본값을25로 변경할 때 compile=false/save=false를 명시하고 마지막 컴파일·저장 각1회로 묶는다. 생성 클래스 CDO의 실제 경로로 공식 ObjectTools 값을 읽고 새 프로세스에서 저장 상태를 확인한다. 이름 중복의 자동 변경 금지, dry_run과 동일값 변경 없음만 대표 실패/무변경 경계로 선택한다. 새 범용 검수 프레임워크는 만들지 않는다.

설치 엔진 `KismetCompiler.cpp:1290–1297`은 full compile에서 선언의 DefaultValue를 CDO로 옮긴 뒤 문자열을 비운다. 따라서 컴파일 전 add/set 응답의 stored default10→25와 컴파일 후 공식 CDO 값25를 구분한다. 컴파일 후 stored_default="" 자체를 손실로 처리하지 않는다. 실제 공식 `editor_toolset.toolsets.blueprint.BlueprintTools.get_default_object`의 반환 refPath로 ObjectTools를 호출하며 CDO 경로를 조합해 추측하지 않는다(`official-blueprint-cdo-schema-S.json`). Float/single은 real/float, double/real은 real/double로 구분해 넓이도 확인한다.

원본 공개 `Python/tools/node_tools.py`의 add(:1018)는 is_exposed/default/category/tooltip/friendly_name/type_object/is_array/metadata를 전달한다. active native `UnrealMCPBlueprintNodeCommands_Part1.cpp:2094`는 hidden is_editable과 variable_guid도 처리하며, is_exposed OR is_editable일 때 CPF_Edit/CPF_BlueprintVisible을 켜고 CPF_DisableEditOnInstance를 지운다(:2216). 이 마지막 flag 교정은 `test_cpp_blueprint_variable_exposure_contract.py`에 보존된 운영 경험이다. raw 스크립트가 보내는 is_transient는 이 handler에서 읽지 않으므로 실제 지원으로 취급하지 않는다. 배열·object/class·enum/struct와 hidden GUID 옵션의 구현·검증 범위는 각각 추적한다.

inspect_blueprint_variables(:2289)는 new/generated 변수, category sorting, counts를 반환한다. inspect_blueprint_variable_metadata(:2326)는 정확한 scope/index/GUID와 순서 있는 metadata_entries·편의 map 및 dirty 전후를 반환한다. set_blueprint_variable_default(:2440)는 stored default 전후와 검증 변수를 반환하고 compile/save/dry_run을 제어한다. 공개 Python 기본 compile=true/save=true와 raw C++ 기본 false/false는 같지 않다. 새 공개 JSON 문자열 adapter와 공통 변경/컴파일/저장 결과 계약으로 이 차이를 명시하고, 기존 오류를 숨기거나 불확실한 변이를 자동 재시도하지 않는다.

다음 함수 그래프 후보는 `build_cube_fluid_ninja_mouse_timer.py`(SHA256 `696ea2d51014152d55cde30a3f7e7720ad4b230a1153ea6e9be6e8036319ae29`)의 37줄 author다. 함수 graph 생성 → DeltaSeconds 입력 → topology 검수 → TimeSinceLastClick get → KismetMathLibrary.Add_DoubleDouble → 변수 set → 데이터3/exec1 연결로 변수·typed 핀·함수 그래프를 묶는다. 기존 builder의 원본 복제/proof 의존은 격리 fixture로 치환해야 하므로 전체 builder 이전과 구분한다. 그 builder는 단계별 원응답 보고서, compile validation/error count, graph truncation/금지 참조 검사, 보호 SHA, 실패 do_not_retry를 보존한다. Bird 전체의 입력 설정·Pawn·카메라와 새 함수 graph 묶음을 한 번에 완료했다고 선언하지 않는다.

## 실제 대표 실행

`blueprint-variables-S.json`은 PID30608의 격리 UE5.8.3에서 4add+Mass25 setter+최종 compile/save를 6-child 배치로 실행했다. 명시 full compile/save 각1회, Mass GUID/노출 OR·메타데이터·정확한 float/bool 타입과 공식 CDO25/100/0.800000011920929/false를 확인했다. 중복 변수는 variable_name_collision/preflight로 거부했고 dry_run99·새 ID 동일값25 이후 선언/CDO/dirty/디스크는 변하지 않았다. full compile 후 stored_default=""도 실제로 관측했다.

16 named calls/24 HTTP 관측, named 단계 elapsed 합계0.8903376초, 전체 파이프라인1.2777615초다. 6-child 배치 호출0.2993754초/내부0.248267초는 warm NullRHI·loopback의 작은 단일 fixture 측정이며 속도 개선율이 아니다. 보고서의 source SHA는 `63591ee09c0ed4c31c104249488b4c6a6b4c7e173d548122a963e6147a7ae15f`다. `registered-schemas-and-diagnostics-ST.json`, `build-variables-graph-ST2.log`, `python-contract-tests-ST.log`를 함께 참조한다. ST1 빌드 실패와 교정은 보존했고 ST2만 로드했다. S 전체 변수 타입·generated/inherited 조합·원본 전체 파이프라인·MCP 없는 패키징을 이 대표 결과로 완료 처리하지 않는다.

## 실제 Codex와 cold 확인

`actual-codex-cold-ST.json`의 새 PID3340(작성30608)에서 S 변수 rows·순서 있는 metadata entries·공식 CDO25/100/.8/false가 작성 보고서와 일치했다. 두 저장 BP의 SHA/mtime와 dirty=[]도 유지됐다. 저장 registry2roots/2packages의 edges/violations/incomplete/dirty=[]는 저장 참조 계층의 증거이며 packaging_verified=false다. `actual-codex-variables-graph-ST.json`의 새 operation ID 변수25·기존 GUID link 호출은 무변경이고 replay가 아니다. `source-and-asset-persistence-ST.json`은 후속 U 변경 전 ST freeze의 root/isolated97파일 일치를, `target-preservation-after-ST.json`은 보호1,059개 승인 uproject 외 변경0을 기록한다. BeginPlay 실행·ST 패키징·전체 원본 계약 동등성은 계속 미검증이다.
