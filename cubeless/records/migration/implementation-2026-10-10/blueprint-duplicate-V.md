# V: ActorComponent 타이머·보호된 복제·내부 실행

ActorComponent 부모의 함수 저작과 MCP 없는 에디터 내부 누적 실행, 교정한 V2의 named 복제·명시 저장·원본 보존이 통과했다. 공식 V2 등록은8Toolset/52도구다. 최초 dry-run 실패는 보존했다. V2 clone cold는 후속 W 배포 재시작의 실제 Codex 조회로 통과했다(아래 추가 기록). source 함수 실행을 V2 clone 실행이나 전체 mouse pipeline 이전으로 확대하지 않는다.

## 원본 연결과 이식 범위

- 공개 `Python/tools/blueprint_tools.py:363`의 `duplicate_blueprint_to_temp` → active `UnrealMCPBridge.cpp:851` → `UnrealMCPBlueprintCommands.cpp:1510`이 named 복제 기능이다. 기본 required_root=/Game/_MCP_Temp, dry_run=true, overwrite_existing=false, allow_dirty_source=false, compile=false, save=true를 유지한다. 기존 target 덮어쓰기·실제 dirty source는 이번 범위에서 명시 거부한다.
- `build_cube_fluid_ninja_mouse_button_component.py:70`은 `/Script/Engine.ActorComponent` 부모로 생성한다. V는 기존 timer author에 `--parent-class Actor|ActorComponent`만 추가하고 기본 Actor를 유지했다. 공식 BlueprintTools.get_parent로 정확한 부모 경로를 확인한다. 원본 mouse 변수·입력·reset 함수 전체를 이 한 변경으로 이전한 것은 아니다.
- `build_cube_fluid_ninja_mouse_timer.py`는 `build_cube_fluid_ninja_mouse_buttons_endplay.py`의 builder를 공유한다. builder는 부모 보고서 success/asset SHA, 보호 파일, 에디터 PID/project/dirty와 대상 부재를 확인하고 EditorAssetLibrary.duplicate_asset을 실행한다. 이 원본 builder는 named duplicate_blueprint_to_temp를 직접 쓰지 않는다. 새 외부 `blueprint_duplicate.py`는 성공한 ActorComponent timer 보고서·파일 fingerprint를 입력받아 같은 보존 경계를 named 도구로 구성한다. 원본 모든 보호 에셋·EndPlay/host 수정 흐름은 여전히 미완료다.
- `verify_cube_fluid_ninja_mouse_buttons.py:114–130`의 editor-internal timer 검수에서 transient component와 세 누적 호출만 이식했다. 이후 MousePressed/MouseReleased reset은 포함하지 않았으며 원본 자체도 caller_gate_verified=false를 반환한다.

새 복제 도구는 registration/UE58 compatibility/operations 및 durable journal을 재사용한다. public EditorAssetSubsystem 복제·저장 API를 사용하며 컴파일 옵션은 기존 코어로 연결한다. 엔진 PostDuplicate의 암묵 컴파일을 explicit compile과 분리하고 상태 관측만으로 모든 compiler diagnostic을 확보했다고 하지 않는다. SaveOnCompile=Never와 SourceControl 비활성을 확인하고 설정을 바꾸지 않는다. 원본/대상 K2 graph·변수/CDO·파일·부모 관측은 제공하지만 comment 내용, 임의 callback 부작용, 모든 component template 속성, live BlueprintGuid 전체를 검증한다고 주장하지 않는다.

외부 복제 pipeline은 dry-run→실제 clone(compile=false/save=true)→공식 부모/CDO와 graph readback을 수행한다. 복제된 GUID는 재생성 가능하므로 node class/name/상대 경로·함수 참조·핀·양방향 edge를 비교하고, 원본 graph/GUID는 전체 동일성을 요구한다. 실제 reference field만 경계 정규화하며 literal 문자열·기본값을 경로로 치환하지 않는다. 고유 `/Game/CubelessMCPTests/Duplicate_<UUID>/` 외에는 수정하지 않고 자동 재시도·원자적 rollback을 제공하지 않는다.

## 실제 ActorComponent 저작과 원문 보존

`blueprint-component-timer-V.json`은 기존 U51 서버에서 ActorComponent 부모의 fresh source를 작성했다. 21 named calls/30 HTTP, 단계 elapsed 합계1.3476237초·전체1.9764706초이며 warm NullRHI/loopback 한 fixture 측정이다. 명시 compile/save 각1회, 공식 부모 `/Script/Engine.ActorComponent`, 함수 topology와 CDO0을 검수했다. U의 Actor 결과를 고치지 않았으며 U script 원문 SHA `b3359e5ac7fb7c849ef044c11f2c4b935fd6232cf2a5c88c7193e425f63d4c7e`는 `../../verification/implementation-2026-10-10/source-snapshots/blueprint_function_timer-U.py.source`에 보존했다. V author source SHA는 `838f97cababccb4fa9983bbe1611726eefbe838d41a885132a34336b876ab914`다.

## MCP 없는 에디터 내부 실행

`timer-runtime-copy-V.json`의 source/사본 SHA와 author asset SHA가 모두 `bf97de438589de1155166eba36d99c05a98ea7398d918bd5fb018a71ddb5c3b1`로 일치한다. 대상은 플러그인 없는 `UE58RuntimeProof/CubelessRuntimeProof.uproject`이며 실행 PID57380은 별도 launch 기록과 대조했다.

`timer-runtime-V.json`에서 owner가 없는 `/Engine/Transient` ActorComponent 하나에 CFNL_AdvanceMouseTime을 .25/.5/1.25로 호출했다. 실제 인스턴스 값은 .25→.75→2.0이었고 각 호출 후 CDO는0이었다. source/사본 파일 SHA와 source mtime, Blueprint 상태, dirty=[] 및 MCP/Toolset 비활성도 유지됐다. 명시 compile/save는0회이며 load 중 암묵 compile 여부는 측정하지 않았다. 임시 패키지 outer를 확인한 것이며 객체에 RF_Transient flag가 있다고 주장하지 않는다. 객체 정리는 소유한 임시 프로세스 종료에 맡겼다.

`timer-runtime-verification-V.json`의9checks는 author·copy·launch·실행 보고서를 연결한다. 본체0.054618초는 에디터 시작 시간을 포함하지 않는다. 이 증거는 **원본 source fixture의 직접 함수 실행**이며 named clone 실행·press/release reset·하드웨어 입력·Tick/caller gate·Shipping/패키징은 검증하지 않았다. 공개 Python API 근거와 실행 한계는 [timer-runtime-api-V.md](timer-runtime-api-V.md)에 있다.

## 최초 복제 실패와 다음 확인

`blueprint-duplicate-V.json`은 PID61456에서 dry_run/preflight의 `unreal.SourceControlHelpers` 비노출 오류로 실패했다. changes=[], duplicate_not_called, explicit compile/save 미시도, outcome_unknown=false, source 파일 SHA/mtime 불변, target 파일 없음이다. 실제 복제를 시도하거나 저장하지 않았다. 원본 실패 보고서는 보존한다.

설치 `Engine/Source/Developer/SourceControl/Public/SourceControlHelpers.h:244`의 `ScriptName="SourceControl"`에 맞춰 `unreal.SourceControl.is_enabled()`로 교정했다. 교정 후 새 UUID/operation ID·V2 보고서로 실행해 성공했으며 최초 실패를 pass로 덮어쓰지 않았다. V2 실행 시점의 미검수 clone cold는 아래 W 시점 추가 기록으로 구분하고, 직접 clone 런타임은 완료로 표시하지 않는다.

## V2 실제 복제·저장

`registered-schemas-and-diagnostics-V2.json`은 PID12256의8Toolset/52도구·plugin111파일을 기록한다. V2는 Python 수정이며 로드 DLL은 U1의 SHA `87e5d18c7434d0687894dcb16b0c16c8d7ebaeab837041396deeb3d12089a16a`를 재사용한다. `python-contract-tests-V2.log`의7tests/0.335초가 통과했다. `source-snapshot-V2.json`은 isolated111파일의 등록 snapshot 일치와 후속 W 변경 전 source 바이트 보존을 기록한다. 바이너리는 해시만 보존하며 현재 root의 후속 코드를 V2 실행본으로 표시하지 않는다. 별도 V 전체 BuildPlugin·Shipping은 수행하지 않았다.

`blueprint-duplicate-V2.json`은 부모 report/SHA 검증, dry-run 무변경, 새 clone 생성과 save, 원본 전체 graph/GUID·파일 불변, clone의 경계 정규화 구조·공식 parent/CDO0 및 dirty=[]를 통과했다. 총20calls/28HTTP, 단계 elapsed 합계0.9012397초·전체1.3527397초다. dry-run0.0712883초/내부0.02304초, 실제 복제0.1634635초/내부0.118669초는 warm NullRHI·loopback 단일 fixture이며 개선율을 주장하지 않는다. 명시 compile0/save1이고 엔진 PostDuplicate compile은 attempted=true/UpToDate 상태를 관측했지만 compiler diagnostics_captured=false다.

새 clone은 `/Game/CubelessMCPTests/Duplicate_5910184abab144bb94994acfec226454/BP_FunctionTimerClone`이고 저장 SHA `54c7802153a60cf9b05afb40a509f0586ba13c90a2065a596f2e1f76efd23ca0`다. `actual-codex-duplicate-V2.json`은 실제 Codex 공식 연결의 schema 검색과 새 ID dry-run changes=[]를 확인한다. 복제 pipeline SHA는 `7fc3b1df3682cd6971c28f14c99afc544191e2f3a1ca0620fad7d7ebab0f5e06`이다. source runtime 누적 결과는 위 별도 증거이며 clone 실행/재로드 결과가 아니다.

## 대장 연결 대상

기존 identity를 유지해 named duplicate의 public/native route/native handler3행, create_blueprint의 ActorComponent 대표 경계, batch의 신규 capability 등록 범위, 위4개 원본 pipeline/verifier의 추출 부분에 증거를 연결한다. 이12개 행은 중복 source 표면이며 고유 기능 수나 완료율이 아니다. 기존 U 함수 저작/실행 관련 행은 해당 숫자 timer 한 사례의 실행 증거만 추가할 수 있다. 모든 최종 disposition과 전체 legacy parity를 자동 완료로 바꾸지 않으며 원본 host의 instance-call/component attach/inherited getter/Sequence/guarded exec replacement는 후속이다.

## 후속 W 재시작에서 확인한 V2 clone cold

`actual-codex-cold-duplicate-V2.json`은 W1 DLL 배포의 새 공식 MCP PID42012에서11checks PASS다. source/clone CFNL_AdvanceMouseTime 및 EventGraph의 전체 저장 identity/topology, 공식 CDO0, source/clone 파일 SHA·mtime, saved dependency와 dirty=[]를 확인했다. 원본 V2 보고서는 수정하지 않았다. 이는 실제 Codex 공식 연결의 새 프로세스 조회이며 V2 clone의 직접 함수 실행·Shipping 검증은 아니다. W3의 자체 gate 실행 fixture와도 구분한다.
