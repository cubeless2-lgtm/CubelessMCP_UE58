# cubeless 작업 기록

## 2026년 10월 10일 UE 5.8.3 이식 계획

- 사용자 요청에 따라 구현 전에 이식 계획과 읽기 전용 조사를 수행했다.
- 5.6 추가 기능 전량 추적, 5.8 API 변화 수정, 공식 Toolset Registry 확장, 엔진 소스 수정 금지를 계획에 반영했다.
- 게임 런타임 의존성을 남기는 C++를 금지하고, cubelessMCP 내부의 에디터 전용 C++는 사용하도록 확정했다. 생성 에셋의 영속 참조도 검사 대상으로 포함했다.
- MCP 파이프라인과 검수 Python을 도구와 별도의 이식 단위로 추가했다. 에디터 내부·외부 Python 실행, 호출 순서·입출력·검수·실패·보고서 및 실제 UE 5.8 실행을 완료 조건에 포함했다.
- 설치된 UE 5.8.3 CL 58210709를 확인했다. 대상은 Git 저장소가 아니며 기존 1,059개 게임·작업 파일의 SHA-256 기준선을 기록했다.
- 원본 Git HEAD와 dirty 상태를 읽어 기록했다. 일부 운영 문서 손상과 Git 기반 복구 가능성을 조사했다. 원본은 수정하지 않았다.
- 공식 MCP `list_toolsets`는 승인 정책 충돌 오류로 차단되었다. 실제 등록 목록 조회·도구 호출은 통과로 기록하지 않았다.
- 세 하위 에이전트가 기능 목록, 운영 자료·파이프라인, 엔진 등록 API 조사를 나누어 수행했다. 원본 역할 문서를 활성화하거나 과거 지시를 실행한 것은 아니다.
- 생성한 파일은 `cubeless` 아래 계획·조사·검증 자료뿐이다. 플러그인 구현·빌드·에셋 변경·프로젝트 설정 변경·Git 커밋·푸시는 하지 않았다.
- 계획 전후 비교에서 기준선 1,059개 파일의 SHA-256이 모두 같았다. 생성 JSON의 수량·중복·로컬 문서 링크를 검사했다. 이는 계획 자료·파일 보존 검사이며 엔진 기능 검증이 아니다. [검사 결과](records/verification/planning-2026-10-10/planning-preservation-check.json)
- 계획 상호 검토에서 원본 실행을 격리 사본으로 통일하고, 재시작 전 에디터 메모리의 미저장 사용자 작업을 별도로 보호하도록 보강했다.

[실행 계획](records/migration/UE58_MIGRATION_PLAN.md)

## 2026년 10월 10일 Python 배치 계획 변경

이 절은 검토 당시의 기록이다. 아래의 공식 관례 채택 결정으로 대체되었으며 현재 구현 지침이 아니다.

- 사용자가 제안한 프로젝트 루트 `cubeless/Python/cubeless_mcp/`를 에디터 Python 코드 위치로 반영했다.
- UE 5.8.3 설치 소스와 공식 문서에서 Content/Python이 기본 검색 위치일 뿐 필수 위치가 아님을 확인했다. 공개 Python lifecycle·실행 API를 이용하는 에디터 전용 C++ 초기화 경로를 계획했다.
- 기존 프로젝트 `Content/Python/init_unreal.py`를 보존하고, 새 플러그인의 Content/Python 초기화 파일은 만들지 않는 구조로 변경했다.
- 플러그인과 프로젝트 Python 폴더를 함께 배포해야 한다는 제약, 경로 추가·명시 등록·해제·버전 확인·새 위치에서의 등록 검증을 추가했다. 게임 런타임 의존성 금지 조건은 유지한다.
- 계획 문서 3개만 수정했으며, 실제 Python 코드 이동·플러그인 구현·프로젝트 설정 변경·에디터 실행은 하지 않았다.

## 2026년 10월 10일 공식 Python 폴더 관례 채택

- 사용자 최종 선택은 `Plugins/cubelessMCP/Content/Python/cubeless_mcp/`다. 플러그인의 `Content/Python/init_unreal.py`에서 패키지를 import하고 공식 Toolset Registry에 등록한다.
- C++와 에디터 Python을 플러그인 안에 함께 배포한다. 별도 Python 경로를 추가하는 C++ bootstrap 계획을 제거했다. 에디터 기능을 위한 C++ 사용과 게임 런타임 의존성 금지는 유지한다.
- 외부 파이프라인·검수기는 `cubeless/tools/`, 지침·기록은 기존 `cubeless` 구조에 둔다. 기존 프로젝트 Python 시작 스크립트와 사용자 에셋은 보존한다.
- 계획·조사 문서만 갱신했으며 실제 구현·코드 이동은 수행하지 않았다.

## 2026년 10월 10일 작업 계획 보완과 조건부 견적

- 초기 운영 지침, 미저장 사용자 작업 보호, 최소 에셋의 저장·재로드 및 MCP 없는 Win64 패키징을 초기 단계에 배치했다. 예정 대응과 실행 증거가 있는 최종 분류를 분리했다.
- 권한 변경 후 공식 MCP 연결의 목록·스키마 조회와 읽기 호출을 다시 확인했다. Toolset 52개, `EditorAppToolset` 스키마, `IsPIERunning=false` 응답을 기록했다. 앞서 기록한 승인 정책 오류는 이번 조회에서는 재현되지 않았다. cubelessMCP의 등록·호출 성공을 뜻하지 않는다.
- 공개 Python 정의 436개, native 소스 111개 파일, 넓은 운영 자료 범위의 실행 스크립트 3,518개를 견적 근거로 정리했다. 경로·명령·핸들러와 의미상 중복은 독립 기능 수에 합산하지 않았다.
- 숙련 개발자 기준의 미보정 작업량을 낙관 1,156 / 기준 2,456 / 고난도 4,816인시로 산정했다. Codex 실행시간·금액·확정 납기가 아니다. 전체에 포함된 40~80인시 상당의 첫 검증 구간을 실제 수행한 뒤 속도와 잔여 규모를 보정하도록 했다.
- 별도 검토자가 산정표 합계, 정의 수, 실행 증거의 범위와 완료 조건을 읽기 전용으로 교차 검토했다. 실제 플러그인 구현·빌드·에셋 변경·프로젝트 설정 변경·커밋·푸시는 시작하지 않았다.

[보완된 작업 계획](records/migration/UE58_MIGRATION_PLAN.md) · [상세 견적](records/migration/UE58_MIGRATION_ESTIMATE.md)

## 2026년 10월 10일 대상 원격 저장소 지정

- 사용자가 `https://github.com/cubeless2-lgtm/CubelessMCP_UE58.git`을 대상 저장소로 지정했다.
- `git ls-remote` 읽기 조회가 성공했고 공개된 ref가 없었다. GitHub 페이지에서도 빈 저장소 표시를 확인했다. 읽기 접근만 검증했으며 쓰기 권한은 시험하지 않았다.
- 계획에 주소를 기록했다. 로컬 Git 초기화·remote 변경·커밋·푸시와 원격 파일 변경은 수행하지 않았다.

## 2026년 10월 10일 플러그인 이름 확정

- 사용자 요청에 따라 플러그인 이름을 저장소와 같은 `CubelessMCP_UE58`로 확정했다. 이전 절의 `cubelessMCP` 표기는 당시 계획의 역사 기록이며 현재 명명은 이번 결정으로 대체한다.
- 현재 계획·견적·조사 문서의 대상 명칭과 경로를 갱신했다. 플러그인 폴더는 `Plugins/CubelessMCP_UE58/`, descriptor는 `CubelessMCP_UE58.uplugin`, 에디터 모듈은 `CubelessMCPUE58Editor`, Python 패키지는 `Content/Python/cubeless_mcp_ue58/`로 계획한다. 도구 네임스페이스의 `Cubeless` 접두사는 유지한다.
- `Content/Python`은 플러그인 자체 콘텐츠 디렉터리 아래 공식 에디터 Python 검색 위치다. 게임 프로젝트의 기존 Content를 변경하지 않으며, 에디터 전용 활성화와 cook/stage 제외 검증은 별도로 수행한다.
- 실제 플러그인 구현 파일은 아직 없으므로 코드 이동·빌드·등록·프로젝트 설정 변경은 하지 않았다. 과거 원본과 증거 JSON은 명칭 변경 대상으로 일괄 수정하지 않았다.

## 2026년 10월 10일 구현 착수

- 사용자의 "이 작업 진행하자" 요청에 따라 구현·빌드·도구 등록·실행 검증을 시작했다. 커밋·푸시는 승인 범위에 포함하지 않는다.
- 기존 1,059개 파일의 새 기준선을 수집했고 계획 당시 SHA-256과 모두 일치했다. 사용자 에디터 PID 36664와 공식 MCP 연결이 살아 있음을 확인했으며 기존 세션을 종료하지 않았다.
- 루트 AGENTS 진입점, 배포용 .gitignore와 README를 생성했다. Git 초기화와 원격 설정은 수행하지 않았다.
- C++ Toolset, Python Toolset, 현재 운영 지침·과거 자료 보존·외부 검수기를 분담했다. 엔진과 원본 저장소는 읽기 전용이며 에셋 수정 호출은 root에서 직렬 실행한다.
- 원본 기능·내부 명령·반영 API·핸들러·운영 스크립트를 별도 [구현 대장](records/migration/implementation-2026-10-10/migration-ledger.json)으로 연결했다. 중복된 소스 표면을 고유 기능 수로 합산하지 않으며 실행 증거 없이 완료 처리하지 않는다.

## 2026년 10월 10일 첫 Toolset 빌드·등록·Blueprint 검증

- Editor C++ 및 Python Toolset 4개/도구 16개를 구현하고 격리 UE 5.8.3 공식 MCP에서 실제 등록·스키마·호출을 확인했다. RunUAT BuildPlugin 배포 빌드도 성공했다.
- author-D 19단계와 새 에디터 프로세스의 reload-D를 통과했다. 작은 fixture 1회 측정에서 author 0.750446초/26회 호출/컴파일·저장 각1회, reload 0.320406초/11회 읽기 호출이었다. 에디터 기동시간 제외·nullrhi·loopback 조건의 단일표본이며 5.6 대비 성능이나 전체도구 완료율이 아니다.
- 원본 실제 FastMCP 등록 이름 합집합 436개와 정적 정의 436개가 일치했다. 3,518개 스크립트 호출·검수 계약을 정적으로 조사했다. 현재 지침과 과거 문서 2,331개 사본을 보존하고 새 외부 pipeline/dispatcher/독립 검수기를 작성했다.
- 대상 .uproject에 Editor 전용 플러그인 활성화를 적용했다. 사용자 세션을 재시작하지 않았으므로 기존 8000 연결에는 다음 기동 시 적용된다. 1,059개 보호 파일의 정정된 해시 검수에서 승인된 .uproject 외 변경 0개였다.
- MCP 없는 최소 게임 fixture/패키징 검증은 진행 중이며, 나머지 도메인·기존 42 gate·대부분 원본 pipeline은 미완료다. 전체 이식 완료로 표시하지 않는다. 실패와 수정은 [구현 상태](records/migration/implementation-2026-10-10/IMPLEMENTATION_STATUS.md)에 남겼다.

## 2026년 10월 10일 최소 fixture Shipping 실행 검증

- MCP가 없는 별도 프로젝트에서 맵 로드와 Blueprint 세 개 컴파일이 통과했다. 원본 테스트 패키지 네 개의 파일 해시가 유지됐다.
- Win64 Shipping build/cook/stage가 성공했고 pak 1개/entry 1,782개와 receipt 감사에서 MCP 참조 위반 0개를 확인했다. 실제 Shipping 실행은 종료 코드 0이며 실행 중 생성한 SaveGame의 정확한 클래스와 UE 5.8.3 헤더를 검증했다.
- 첫 쿠킹의 에디터 receipt 누락을 임시 Editor target 빌드로 해결했다. UAT가 테스트 Intermediate 아래 생성한 Core-only 게임 stub은 기록했으며 사용자 게임플레이 소스나 MCP 런타임 의존성은 추가하지 않았다.
- 전체 게임 또는 모든 이식 기능의 패키징 통과를 뜻하지 않는다. 플러그인이 설치된 상태의 Game 제외 검증과 추가 기능군의 생성 에셋 검증을 계속한다. [실행 증거와 한계](records/verification/implementation-2026-10-10/runtime-proof-B.md).


## 2026년 10월 10일 Material9 및 MCP 없는 Material Shipping 검증

- 공식 등록 5개 Toolset/25개 도구에서 Material9 author-E3 36단계와 새 프로세스 reload-E3를 통과했다. 기존 실패·부분 변경 기록은 보존했다.
- 플러그인 없는 별도 프로젝트에서 E3 원본과 복사본을 보존하며 MPC 연결을 추가한 새 runtime clone/맵을 작성했다. cold readback, Shipping build/cook/stage, pak 전체 1,786개 항목의 MCP 제외, 실제 게임 SaveGame 생성이 통과했다. shader 완료와 렌더링 품질은 별도 미검증이다. [범위와 증거](records/verification/implementation-2026-10-10/material-runtime-proof-B.md) 참조.
- Material Add/Set native F2 빌드는 통과했고 새 Python 등록 및 batch 실제 호출은 준비 중이다. UE58 MPC 상속 API 차이를 추가 발견해 기존 조회의 정확성부터 교정하고 있다. 전체 이식은 미완료이며 Git 커밋·푸시는 하지 않았다.


## 2026년 10월 10일 중복 기능 최적화 범위 추가

- 사용자가 중복 기능 도구의 최적화도 요청했다. 기존 공개 기능의 이름·기본값·오류·검수 계약을 빠뜨리지 않고 공통 코어를 공유하며, 공식 도구로의 대체는 실제 동등성 증거가 있을 때만 확정한다.
- 기존 3개 Blueprint compile wrapper는 기본 save/refresh 의미가 달라 유지한다. 이번 이식에서 새로 공개한 동일 native compiler 경로만 내부 helper로 정리하는 H 빌드는 성공했다. 실제 등록·호출 회귀는 진행 중이다.
- 공통 mutation journal은 동일 JSON 바이트를 single-write로 출력하도록 최적화했다. exclusive create, ownership, checkpoint, fsync, atomic replace 및 중단 후 재시도 차단을 유지했다. 인코딩 A/B와 실패 반례는 기록했고 실제 engine batch 전체 성능 검증은 별도로 수행한다.

## 2026-10-10 I/J: 등록 호환·중복 최적화 실검증

I C++ 빌드17.91초 및 공식6 Toolset/27도구 등록. 두 Material array default의 정확 converter+own generated metadata 보완, primitive JSON 파서 교정. Material43단계/배치24단계/MPC상속/입력경계의 author와새프로세스cold검수통과. 세 legacy compiler wrapper의 서로다른기본refresh/save 유지검증. 최초검수기문구·잘못된required입력실패는별도조사와함께보존, 불확실변이재시도없음.

J C++ 빌드12.27초. 같은호출의 Before payload를두읽기에재사용하고After신규읽기보존. I/J6사례각warmup1+10측정, DTO/error동일·정상read helper실측2회·변이43단계회귀PASS. 시간측정은tick/cache한계로일반speedup주장안함. material-read-reuse-results-J.md 참조. 당시 다음Custom특화도구는별도소스작업중이었다. 전체이식미완료, 커밋/푸시없음.

## 2026-10-10 K/L: Custom·공식 Codex 연결·입력 경계

K Custom 도구는 공통 Add 코어를 재사용한다. 공식 등록6 Toolset/28도구, author59단계 및 L새프로세스 cold8단계·독립검수 통과. generic Add/Set의 동일 shader 의존성 거부도28단계로 검수했다. include/preprocessor, 모든 output/Function 그래프, 렌더링은 미완료다.

L은 소스에서 확인한 FNAME_Add 긴 이름의 fatal 경로를 사전 거부한다. Add/Set property/directName/nestedImport/큰 정상배열, MPC 필터, dependency 패키지 경계54단계에서 변경 없음과 dirty[]를 독립 확인했다. 큰 정상 name-bearing aggregate도1023 UTF16을 초과하면 미지원이며 숨기지 않는다. 최초 L검수기의 오류문구 substring 불일치는 원본실패로 보존했다. L증분빌드9.65초, 전체 BuildPlugin 약82초 성공. 배포 source는 Actor개발과 분리된 L snapshot이다.

비어 있는8000포트에 격리 L에디터 PID32868을 실행했다. 실제 Codex mcp__unreal_58의 검색/스키마/진단/Custom그래프읽기와 독립결과검수가 통과했다. 사용자 게임 프로젝트 에셋 호출은 하지 않았다.

최신19개 저장에셋을 MCP 없는 UE58CurrentRuntimeProof에 복사해38개 원본/사본해시 검수. 최초 author-A는 비노출 parent_class getter로 변이전실패(changes[],save0). 원인교정과 실패보고 보강 후 새 B실행 진행. 전체 이식이나 최신 패키징 완료로 처리하지 않는다.

## 2026-10-10 M: Actor와 최신 저장 에셋의 Shipping 검증

M 빌드 20.38초, 공식 MCP 7 Toolset/35도구 등록. Actor author 49단계와 별도 프로세스 cold 13단계, 외부 독립 검수 통과. 5개 native 클래스, spawn/create 별칭 공유, 이름과 라벨 구분, 위치 수정·null no-op·삭제·replay·오류 검수와 맵 저장 1회를 포함한다. 실제 Codex 연결에서도 저장된 [40,50,60] 위치를 확인했다. Actor의 Blueprint/attached/streaming/WP 수정과 runtime proof는 남아 있다.

Custom의 정상 긴 FString Code 2,080→3,104자와 Description 2,048자를 수정·저장하고 새 프로세스 M2에서 그대로 읽었다. NAME_SIZE 경계 거부와 별도 계약이다. 검증 맵/에셋은 격리 프로젝트에만 만들었다.

통합 저장 에셋의 MCP 없는 Shipping D 및 Distribution L 설치 상태 Shipping 모두 build/cook/stage/run과 외부 독립 검수를 통과했다. 원본 19개 보존, 저장 23개·Pak 필수 22개·전체 1,818항목, 새 SaveGame 1,423바이트를 확인했다. L 설치본의 Source/Python/descriptor 57개도 보존했다. author A/B 및 IoStore C의 실패·미완료는 별도 기록으로 유지했다. shader/렌더링·Actor M·전체 이식의 완료는 아니다.

보호 파일 1,059개 재검사에서 승인한 `.uproject` 외 변경은 없다. 현재 N은 Blueprint local SCS 읽기 helper를 개발·검토 중이다. 전체 이식은 진행 중이며 Git 초기화·커밋·푸시는 하지 않았다.

## 2026-10-10 N: 건강성 중심 통합

사용자 지침에 따라 검증을 위험 기반으로 조정했다. 빌드·공식 등록·대표 정상 흐름·중요한 실패 상태를 우선하며 공통 구현의 동일 검증과 기능별 패키징 반복을 줄인다. 기능/파이프라인 전수 추적과 미검증 표시는 유지한다. 미사용 Actor 결합 검수기 초안은 작성자가 제거했고 추가 보고서 계층을 만들지 않았다.

Blueprint compile은 SkipSave를 적용해 Always/SuccessOnly 설정에서도 명시적 save 없이 저장하지 않는 것을 세 wrapper 6사례와 cold read로 확인했다. 생성 factory는 공개 SkipSave 옵션이 없어 Never만 허용한다. 생성 뒤 조회 실패에도 실제 생성/암묵 컴파일 상태를 보존하도록 수정했고 Python 테스트 51개, 실제 직접/배치 거부 및 Never 생성/저장 37단계를 통과했다.

Component local SCS 조회 2개를 추가했다. N3 fixture 3개를 한 번에 저장하고 N5 조회 51단계/10속성/오류를 확인했다. N6 새 프로세스에서는 같은 전체 검사 반복 대신 로컬 계층·GUID·FVector·파일 불변·dirty[]를 대표 검수했다. 초기 fixture의 미노출 설정 API/중복 handle, 검수기의 bitfield 기대 타입 오류는 수정했으며 실패 기록은 유지했다. inherited effective/native CDO/orphan 및 Component 수정 기능은 미완료다.

전체 BuildPlugin N은 약106초 성공. N 배포 DLL로 재시작해 실제 공식 Codex 연결에서 8Toolset/37도구 등록·health·대표 cold read를 통과했다. source/build 바이트 출처는 distribution-source-N.json과 registered-schemas-and-diagnostics-N6.json에 있다. 배포 파일에서 생성 Python cache 3개만 제외했다. N5 측정은 nullrhi 격리 UE5.8.3·loopback HTTP·작은 3노드 fixture에서 51호출/59HTTP관측/3.454초 단일 표본이며 동시 빌드 부하가 있었다. 일반 성능 보장이나 이전 버전 대비 개선율이 아니다.

Actor 저장 맵도 MCP 없는 Shipping 38.49초 build/cook/stage, Pak1,780항목·금지참조0, PID42924 실행약1.791초/exit0/새1,423바이트SaveGame을 통과했다. 전체 게임 로직/렌더링 완료는 아니다. 전체 이식은 계속 미완료이며 Git 초기화·remote 변경·커밋·푸시는 하지 않았다.


## 2026-10-10 O: Component 저작·배치와 Blueprint 액터 생성

Component add/set 2개와 spawn_blueprint_actor를 추가했다. O4 증분 빌드 11.24초, 공식 8 Toolset/40개 도구 등록, 실제 Codex 새 PID 50916 대표 조회를 통과했다. 등록 snapshot의 소스/DLL과 로컬 현재본이 일치하고 보호 파일 1,059개는 승인된 uproject 외 변경 0개다. 도메인 Python 계약 75개와 공통 offline 검사 54개도 통과했다.

원본 `test_create_and_spawn_cube_blueprint.py`에 대응한 공식 외부 pipeline의 선택된 신규 저작 흐름은 author O2 29 calls/39 HTTP/1.7321308초, cold O 12 calls/22 HTTP/0.8364492초를 통과했다. DefaultSceneRoot 보존, CubeVisual/Engine Cube, BP compile/save 각 1회, UObjectName·transform을 보존한 액터 생성·맵 저장, 새 프로세스 BP/map readback과 파일 불변을 확인했다. 기존 BP 재사용 및 나머지 mesh 속성은 미완료다.

Component batch 검수는 단독 add 기본 compile=true/save=false와 bool 문자열 거부 후 불변, 5개 child의 compile/save 각 1회를 확인했다. 17 calls/25 HTTP/1.0003873초이며 스크립트 전체 명시 compile 2회/save 1회다. 핵심 1회 batch 0.1730835초(내부 0.145426초)는 warm 작은 fixture/NullRHI/loopback 단일 표본이다. Codex cold 조회에서 4노드 계층·GUID·transform, visibility=true/socket=Grip, 저장 hash/mtime 불변과 dirty=[]도 확인했다. 속도 개선율은 주장하지 않는다.

초기 O의 Cube 의존성 거부와 보고서 ini 최종검사 오류는 원 기록으로 남겼다. 이미 만든 부분 BP는 별도 명시 저장으로 보존했고, 교정 후 새 UUID의 O2를 실행했다. 중요한 SaveOnCompile=Never는 전후 직접 확인하며 최근 맵 bookkeeping에 따른 전체 ini 변화는 관측으로 기록한다. 공개 API를 우선하고 AttachSocketName은 미등록 template의 public SetupAttachment를 사용했다.

저장 BP 2개/map 1개의 재귀 registry 조회 8패키지는 forbidden violations=[]로 통과했으나 packaging_verified=false다. 최신 전체 BuildPlugin 배포 N과 이전 MCP-free runtime 통과는 과거 회귀 근거로 유지한다. O 자체 전체 BuildPlugin·MCP 없는 패키징은 반복하지 않았고 not_run이다. 전체 legacy 동등성은 계속 미완료이며 Git 커밋·푸시는 하지 않았다. 실행 근거는 component-author-O2.json, component-reload-O.json, component-batch-O.json, actual-codex-health-O.json, component-saved-dependencies-O.json과 등록/보존 O 기록이다.


## 2026-10-10 P: named mesh와 Shape 크기

P1 증분 빌드 6.77초와 공식 8 Toolset/41도구 등록을 통과했다. named set_static_mesh_properties를 복구하고 기존 Component add/set에 Sphere/Box/Capsule을 더했다. 공개 shape 크기 setter를 사용하며 Capsule 최종 pair를 사전검사해 companion의 암묵 보정을 허용하지 않는다. 내부 material 옵션·물리 동작·원본 multi-component 시나리오 전체 동등성은 미완료다.

기존 cube pipeline은 P에서 named 기본 Cube 인자를 생략했다. asset-only author 21 calls/29 HTTP/1.0505173초, 새 PID26272 reload 9 calls/17 HTTP/0.3607934초가 통과했다. P에서 spawn/map을 추가 검증하지 않았으며 이전 O2 증거와 구분한다. 실제 Codex의 잘못된 mesh 요청은 preflight에서 변경·컴파일·저장 없이 거부됐다.

기존 batch script의 선택형 --shape-bounds로 세 Shape 작성+Sphere radius setter+최종 compile/save를 한 5-child batch에 묶었다. 최종 Sphere64/Box[20,30,40]/Capsule30·60, CapsuleRadius61>60 거부 후 불변과 compile/save 각1회를 확인했다. 15 calls/23 HTTP/0.9452058초(핵심 batch0.208877초, 내부0.16057초)는 작은 warm NullRHI/loopback 단일 표본이며 개선율이 아니다. 실제 Codex cold 조회로 계층·GUID·transform·공식 크기와 파일 SHA/mtime 불변을 확인했다.

등록 snapshot의 plugin79파일과 root/isolated 현재본 일치, 보호1,059파일의 승인된 uproject 외 차이0을 확인했다. 저장 BP2개/재귀7패키지 의존성 감사도 금지 참조·dirty0이나 packaging_verified=false다. P 전체 BuildPlugin·MCP 없는 패키징은 반복하지 않았다. N 배포 및 이전 runtime 증거, O 보고서/source SHA는 보존한다. 근거: component-author-P.json, shape-bounds-P.json, actual-codex-negative-P.json, component-reload-P.json, actual-codex-cold-P.json, source-and-shape-persistence-P.json, target-preservation-after-P.json. Git 커밋·푸시는 하지 않았다.


## 2026-10-10 Q: 저장 물리 기본값

Q2 증분 빌드 7.29초, 공식 8 Toolset/42개 도구 등록과 새 PID43660의 실제 Codex cold 조회를 통과했다. Q1은 archetype chain 소스 리뷰 교정 전이라 배포하지 않았다. 공개 physics setter5개를 사용한 named set_physics_properties를 복구하고 gravity_enabled 무시 결함을 교정했다. mass [0.001,1e12], damping [0,1e12]의 strict 입력 및 exact StaticMesh/Sphere/Box/Capsule 미등록 template 범위이며 실제 simulation을 실행하지 않는다.

선택형 --physics-defaults는 두 component 추가·기본/명시 physics 설정·최종 compile/save를5-child batch로 묶었다. 전체13 calls/21 HTTP/0.9860521초, 핵심batch0.2482488초(내부0.202811초), 명시compile/save각1회다. 공식BodyInstance7필드의 저장 값과 physicsBlendWeight1/0을 확인했다. mass=-1은 Python preflight/변경0, 새 ID의 동일값 요청은 실제Codex에서 no_op=true/changes[]/native mutationfalse로 끝났다. 작은 warm NullRHI/loopback 단일 표본이며 성능 개선율이 아니다.

cold에서 component GUID·계층·transform·BodyInstance 실제float32저장값이 같고 파일SHA/mtime·dirty[]가 유지됐다. plugin79파일이 등록 snapshot과 일치하며 보호1,059개 중 승인된uproject 외 변경0이다. 저장BP1root/재귀6패키지 감사는 금지참조·dirty0이나 packaging_verified=false다. 전체N배포와 이전P/O/runtime기록은 기존 범위로 보존한다. 원본 physics_variables/bird 파이프라인의 변수·이벤트·inputgraph, 복잡한 상속 조합 실행, 물리·충돌·렌더링·Q패키징은 미검증이다. 근거: physics-defaults-Q.json, actual-codex-physics-noop-Q.json, actual-codex-cold-Q.json, source-and-physics-persistence-Q.json, target-preservation-after-Q.json. Git커밋·푸시는하지않았다.

## 2026-10-10 R: 숨은 material 슬롯 0 계약

기존 set_static_mesh_properties의 공개 인자 순서·Cube 기본값·mesh-only journal 계약을 유지하며 material=None을 마지막에 추가했다. 원본 공개 Python에는 없고 활성 C++에 있던 SetMaterial(0) 분기를 runtime Material/MIC·로컬 exact StaticMeshComponent 범위로 구현했다. 잘못된 참조를 조용히 건너뛰던 원본 동작은 변경 전 명시 오류로 교정했다. 관련 원본 스크립트9개/13참조에는 숨은 material 인자를 보내는 사용을 발견하지 못했으므로 원본 전체 파이프라인 복구로 계수하지 않는다.

R1 빌드의 TObjectPtr 조건식 오류(11.68초)를 교정한 R2가 7.27초에 통과했다. 공식 8 Toolset/42도구 등록 후 기존 pipeline의 --material-overrides로 Plane 유지+Material-only, Cube+MIC와 compile/save 각 1회의 2-child batch, 없는 material 거부 후 불변을 확인했다. 전체 16 calls/24 HTTP/0.8713781초, 핵심 batch 0.1552715초(내부 0.116912초)는 warm NullRHI/loopback 단일 표본이며 개선율이 아니다. 실제 Codex의 새 ID 동일값은 no-op이고 generic mesh-clear는 override 손실을 막는 preflight 오류였다.

새 PID37632(작성 29172)에서 실제 Codex cold의 tree/GUID/transform·Cube/MIC raw override·빈 overlay와 파일 SHA/mtime·dirty=[]가 같았다. 저장 BP 1root/재귀 8패키지 감사의 violations/incomplete/dirty=[]이며 packaging_verified=false다. plugin 79파일이 등록 snapshot과 일치하고 보호 1,059개 중 승인된 uproject 외 변경 0이다. 근거: material-overrides-R.json, actual-codex-material-R.json, actual-codex-cold-R.json, source-and-material-persistence-R.json, target-preservation-after-R.json. 최초 cold 검수 필드 오타는 동일 읽기 응답으로 교정했으며 변이를 재시도하지 않았다.

공유 slot-count guard로 엔진 PostEdit의 override/overlay 꼬리 삭제 위험을 거부하되 다중 slot/상속 조합 실행은 미검증으로 남겼다. 전체 legacy 동등성·R 전체 BuildPlugin·MCP 없는 패키징·shader 완료·렌더링은 미완료다. Python 57/Component 32 테스트 통과는 stdout 관측이다. 엔진 시작 15개 실패는 상세 로그에서 3개 테스트로 식별했고 한국어 번역/영어 기대값 불일치가 유력하나 런타임 비교 값의 직접 증거는 없다(engine-startup-smoke-R.md). 엔진·언어·테스트를 수정하지 않았고 Git 커밋·푸시도 하지 않았다.

## 2026-10-10 S/T: 변수 상태와 member 그래프

S 변수 add/inspect/metadata/default와 T member getter/setter의 6개 named 도구를 추가했다. 기존 connect_graph_pins는 GUID selector와 엔진 schema의 직접 MAKE 데이터 연결을 지원하도록 확대했다. ST1 컴파일 오류 C4456/C2446(16.55초)를 교정한 ST2 증분 빌드가 10.27초에 성공했고 PID30608에서 공식 8 Toolset/48개 도구를 등록했다. Python 계약 75개는 3.054초에 통과했다. 등록 snapshot은 plugin97파일과 로드 DLL을 기록한다.

S는 원본 physics_variables의 네 변수·타입·초기값을 추출하고 hidden GUID/editable OR, metadata, Mass10→25의 stored/CDO 구분을 검증했다. 6-child batch의 full compile/save 각1회 뒤 공식 CDO·정확한 변수/메타데이터를 읽고 중복 거부·dry_run·새 ID 동일값 무변경을 확인했다. 16 calls/24 HTTP, 단계 elapsed 합계0.8903376초·전체1.2777615초, 핵심 batch0.2993754초/내부0.248267초다.

T는 새 Actor BP에서 Source/Target float·Gate bool, getter2/setter1/Branch1을 7-child 배치로 작성하고 GUID의 data2/exec2 연결+compile/save를 5-child 배치로 실행했다. 양방향 전체 edge·노드 identity·공식 CDO 기본값과 파일/dirty 불변을 확인했고 없는 변수·bool→float 직접 연결은 변경 전에 거부됐다. 17 calls/26 HTTP, 단계 elapsed 합계1.1207937초·전체1.6142526초, 작성 batch0.2343425초/내부0.182053초 및 연결 batch0.23915초/내부0.18426초다. 명시 full compile/save 각1회이고 BeginPlay는 실행하지 않았다.

작은 warm NullRHI/loopback 단일 fixture이며 성능 개선율을 주장하지 않는다. 실제 Codex 새 PID3340(작성30608)의 cold 11 checks에서 변수·metadata·전체 그래프·공식 CDO와 저장 파일 SHA/mtime가 같고 dirty=[]였다. 저장 registry2roots/2packages는 edges/violations/incomplete/dirty=[]이며 packaging_verified=false다. 새 ID 변수25·기존 GUID link의 실제 Codex 호출은 무변경이며 replay가 아니다. ST freeze의 root/isolated97파일 일치와 보호1,059개 승인 uproject 외 변경0도 확인했다(후속 U 변경 전 snapshot). 근거는 actual-codex-cold-ST.json, actual-codex-variables-graph-ST.json, source-and-asset-persistence-ST.json, target-preservation-after-ST.json이다. 전체 변수 타입·원본 physics/bird·함수 mouse timer·변환 노드·외부/상속 member·S/T 패키징은 미완료다. 기존 R 이하 증거와 최신 전체 배포 N을 과거 범위로 보존했다. ledger 관련25행의 4,764개 identity/source/기존 disposition/evidence 불변을 확인했다. Git 커밋·푸시는 하지 않았다.

## 2026-10-10 U: 함수 그래프와 timer 저작

resolve_blueprint_graph/add_blueprint_function_parameter/add_blueprint_call_function_node3개 named 도구를 추가했다. U1 증분20.50초·Python81tests/3.092초·공식8Toolset/51도구 등록을 통과했다. 새Actor BP의 float 변수0/DeltaSeconds 입력·정확한 Add_DoubleDouble·setter를3/3/5-child 배치로 구성하고 마지막 명시 compile/save 각1회,4노드/data3+exec1의 양방향 연결과 중복 입력의 선행 거부를 확인했다. EventGraph는 변하지 않았으며 공식 CDO0은 미실행 기본값이다.

blueprint-function-timer-U.json은20calls/29HTTP·단계합1.1892045초·전체1.7205148초, 세 배치0.1411293/0.1301898/0.2678329초(내부0.088888/0.088654/0.213807초)를 기록한다. warm NullRHI/loopback 단일 fixture이며 개선율을 주장하지 않는다. 원본 float/double 폭 차이를 바꾸지 않고 direct MAKE를 확인했으나 함수 실행 효과를 검증한 것은 아니다.

실제 Codex 새PID51508(작성30308)의 cold9checks에서 전체함수/EventGraph/GUID/공식CDO0/파일SHA·mtime와 dirty=[]가 같았다. 새ID graphGUID resolve는 createdfalse/변경0/replayfalse였다. 저장registry1root/1package는 edges/violations/incomplete/dirty=[]이고 packagingfalse다. U108root/isolated파일이 등록snapshot과 일치했다. 근거는 registered-schemas-and-diagnostics-U.json, actual-codex-functions-U.json, actual-codex-cold-U.json, source-and-asset-persistence-U.json이다. target-preservation-after-U.json에서 원래 보호1,059개 승인 uproject 외 변경0도 확인했다. 관련 ledger21행에 증거를 추가했고 전체4,764 identity/source/기존 disposition/evidence와 비대상 행을 보존했다. ActorComponent 원본복제/부모proof·transient 누적실행·caller조건·모든parameter/call옵션·U전체배포/패키징은 미완료다. 다음 구간으로 계속 진행하며 Git 커밋·푸시는 하지 않았다.

## 2026-10-10 V2: ActorComponent 타이머·복제 경계·내부 실행

기존 timer author에 ActorComponent 부모 선택과 공식 parent 조회를 추가하고 U 원문 snapshot을 같은 SHA로 보존했다. source author는21calls/30HTTP·전체1.9764706초·명시 compile/save 각1회로 통과했다. source파일을 MCP 없는 UE58RuntimeProof에 동일SHA로 복사해 PID57380의 ownerless transient component에서 .25/.5/1.25 호출값 .25→.75→2.0, CDO0·dirty[]·파일 불변을 확인했다. runtime reconciliation9checks가 통과했고 본체0.054618초는 시작시간을 제외한다. clone 실행·reset/gate/Shipping 증거는 아니다.

named duplicate를 추가해 V2 공식8Toolset/52도구·plugin111파일을 등록했다. 첫 V dry-run은 Python SourceControlHelpers 비노출로 복제 전 실패했고 source/파일 불변 및 compile/save미시도를 기록했다. ScriptName=SourceControl을 확인해 교정한 새 UUID의 V2는 dry-run→clone/save→독립graph/parent/CDO·원본GUID/SHA 보존을 통과했다. 20calls/28HTTP·단계합0.9012397초·전체1.3527397초, duplicate0.1634635초/내부0.118669초다. 명시 compile0/save1이고 암묵 PostDuplicate compile 상태만 관측했다. V2 계약7tests/0.335초와 실제 Codex 새 ID dry-run도 통과했다. 최초 실패와 모든 기존 raw보고서는 유지한다.

warm NullRHI/loopback 단일 fixture이며 개선율을 주장하지 않는다. clone cold는 다음 배포 재시작과 통합해 아직not_run이다. 전체legacy clone옵션·원본host/reset/gate·clone runtime·V전체BuildPlugin/패키징은 미완료다. DLL은 U1을 재사용하며 source-snapshot-V2.json의 isolated111파일 일치와 W 변경 전 source 보존(바이너리는hash만)을 기록한다. 관련대장12행만 갱신했고 전체4,764 identity/source/기존disposition/evidence와 비대상행 불변을 확인했다. Git 커밋·푸시는 하지 않았다.

## 2026-10-10 W: Sequence·자체 호출·조건별 실행

W1 C++19.01초/Python93tests3.689초를 통과했고 Sequence UUID 비교 교정4tests0.209초 후 W2 공식8Toolset/53도구·batch27capability를 등록했다. 첫 Sequence 실패는 생성 후 후조건 비교였으며 관측·실패원문을 보존하고 소유한 미저장 fixture만 종료 때 폐기했다. W2 author 성공 후 no-MCP 실행은 instance 편집 제한으로 첫 gate setter에서 중단(calls0/preservationPASS/unknowntrue)됐다. 새 W3는 gate3개 is_exposed=True인 테스트 fixture만 다시 작성했고 plugin/verifier는 그대로다.

W3 author21calls/30HTTP·단계합2.1638213초·전체3.0985859초,6/9/12-child batch에서 명시 compile/save 각1회로10nodes/11edges를 저장했다. Sequence는3출력(then_2 미연결)으로 public pin extension을 확인하며 원본 host2출력과 구분했다. 새 no-MCP PID46076의6calls에서 각 false gate는 증가를 막고 모두true만 누적해0/0/0/.25/.75/.75, LastDelta는 매회입력과 같았다. CDO/파일/Config/dirty보존, 명시 compile/save0, 본체0.1494675초다. warm NullRHI 단일 fixture이며 개선율/Shipping 증거가 아니다.

gated-timer-verification-W3.json15checks는 실제 Codex graph·boundary거부·저장참조·clean, runtime source/copy 및 보호1,059파일 승인uproject 외 변경0을 연결한다.116pluginfiles는 W2 snapshot과 같았고 source-snapshot-W2.json에 후속X 이전원문을 보존했다. 기존V2 clone cold도 W배포PID42012에서 actual-codex-cold-duplicate-V2.json11checks로 통과했다. 전체 legacy host/reset/외부component/상속Tick/기존exec교체와 W전체배포·패키징은 미완료다. 이전실패·원본보고서는 수정하지 않았고 Git 커밋/푸시는 하지 않았다.

## 2026-10-11 X: 기대한 기존 실행 연결선 교체

원본 현재 코드에는 없는 과거 파이프라인의 expected-link 계약을 복원했다. 공개 connect_blueprint_graph_pins와 batch를 추가하되 기존 connect_graph_pins의 기본값·저널 식별은 유지하고 공통 코어를 공유한다. 변경 전 정확한 source/old/new 연결과 표준 schema를 검수하며 API 한 번 후 실제 전체 관측을 남긴다. false 부분변경과 응답 유실을 구분하고 자동 재시도/원자 롤백을 제공하지 않는다.

X1 빌드18.65초·Python101검사4.019초, 공식8Toolset/54도구·batch28·121파일 snapshot을 통과했다. author25calls/34HTTP·단계합1.4361454초·전체2.3835839초에서 잘못된기대 거부/정상교체/동일ID replay/새ID stale 거부/원흐름 reconnect 후 compile/save 각1회,11nodes/12edges를 확인했다. 새 no-MCP PID23012에서6/6 실행과 CDO/파일/Config/dirty보존, 본체0.1356369초를 통과했다. guarded-links-verification-X.json11checks에 actual Codex 호출·graph·참조·clean을 연결했다. 단일 NullRHI표본이며 전체 host/상속Tick/연결 compile-save옵션/Shipping은 미완료다. 중간 성공으로 작업을 종료하지 않고 Y generated component 부착·getter·외부호출 묶음으로 이어간다.

## 2026-10-11 Y: 로컬 생성 컴포넌트와 외부 함수 호출

기존 component 추가·member getter·call-node의 좁은 범위를 확장해 saved non-scene ActorComponent를 Actor local SCS MouseButtons로 추가하고 외부 own 함수를 호출했다. Y1 Cast 컴파일 오류14.83초를 교정한 Y2빌드9.89초·Python106검사5.580초·공식8Toolset/54도구와 batch28을 통과했다. 도구 수는 늘지 않았다. skeleton-only 갱신과 마지막 명시 full compile/save 각1회를 구분한다. 에디터 함수 lookup의 skeleton과 serialized member parent/self pin의 compiled generated class도 별도로 기록했다.

blueprint-component-host-Y.json은3/2/4-child 배치·23calls/33HTTP·단계합1.2591456초·전체1.9087676초로3nodes/3edges와 source/root 보존을 확인했다. no-MCP 새 PID24028에서 실제 host를 생성해 component identity/class/owner/default를 확인하고 .25/.5 두 호출의 .25/.75 누적 뒤 소유 Actor를1회 제거했다. 월드0→1→0·기존 Actor집합·BP/CDO/file/Config/선택/dirty 보존, 명시 compile/save0·본체0.1627278초를 통과했다. 단일 NullRHI/loopback 표본이며 개선율/Shipping 증거가 아니다.

component-host-verification-Y.json12checks는 실제 Codex component/graph/dependency/clean과 저장 사본·runtime·source-snapshot-Y.json124파일을 연결한다. 주 에디터는30644, runtime 프로세스는 종료했다. 이후 Z root 소스와 Y 실행 snapshot을 구분하며 이전 증거는 보존했다. 상속 Tick·외부 변수 쓰기·generated SceneComponent·hardware input·원본 전체host·Y전체배포/패키징은 미완료다. 엔진·원본 수정과 Git커밋/푸시는 하지 않았다.

## 2026-10-11 Z3: Blueprint 상속·부모 이벤트와 조건부 외부 호출

저장된 Actor Blueprint 부모에서 자식을 만들고 factory ReceiveTick/parent-call, inherited bool getter, 기존 exec 보호 교체와 외부 MouseButtons timer 호출을 연결했다. event 이름은 기존 이식 대상이고 parent-call은 신규 확장으로 분리했다. 첫 Z의 반환 node identity 수명 결함과 Z2의 유효한 native factory parent 참조 거부를 교정했다. 실패 raw·부분 변경/저장 상태·소유 미저장 fixture 정리 기록과 source snapshot은 보존했다. 엔진 compiler의 가까운 부모 해석에 따라 serialized/native 선언과 effective compiled parent를 구분한다.

Z3 증분 빌드6.63초·공식8Toolset/56도구·batch30을 확인했다. 앞선 Z1빌드31.69초/Python111검사6.273초와 Z2빌드6.52초는 각각의 source 상태 근거로 유지한다. blueprint-inherited-tick-Z3.json은28calls/38HTTP·단계합3.8974209초·전체5.7551638초, 부모/자식 compile/save 각각1회(합계2/2),16nodes/17edges를 검수했다. 실제 Codex 새 ID parent-call 재사용은 no-op이며 엔진 compile 뒤 native 선언 참조가 direct generated parent로 정규화된 상태를 확인했다. 저장3개 참조 감사에 금지 참조/incomplete/dirty가 없었다.

새 no-MCP PID26768에서6회 직접 ReceiveTick의 부모·자식 marker가 항상 delta와 같고 component timer는0/0/0/.25/.75/.75였다. 소유 Actor 제거와 파일/CDO/월드/Config/선택/dirty 보존, 본체0.2259137초·명시 compile/save0을 통과하고 프로세스가 종료됐다. inherited-tick-verification-Z3.json13checks가 실행 근거를 연결하며 source-snapshot-Z3.json의133파일은 실행 당시 root/격리와 일치한다. 이후 AA 개발 source와 구분한다. 단일 NullRHI/loopback 표본이고 성능 개선율을 주장하지 않는다. 실제 게임 Tick 스케줄링·새 event/parent 노드 생성·상속 자식 duplicate·원본 전체 host·Shipping은 미검증이며 최신 전체 BuildPlugin은 N이다. 전체 이식 완료로 선언하지 않고 다음 묶음으로 이어간다. Git 커밋/푸시는 하지 않았다.

## 2026-10-11 사용자 요청으로 AA3에서 일시 중단

AA의 외부 멤버/객체 함수/핀 기본값 구현과 AA3 수정 빌드는 통과했다. AA1 복제 GUID 비교, AA2 null 기본값 비교 회귀를 수정했지만 **AA3 실제 author/runtime 재검증은 아직 실행하지 않았다**. 소유 격리 에디터 PID24080은 dirty=[]/PIE=false로 남겼고 모든 하위 에이전트를 중단했다. 자동 재개/예약과 Git 커밋/푸시는 없다. 상세 상태·실패 기록·배포 DLL·다음 명령 경계는 [재개 체크포인트](records/migration/implementation-2026-10-10/PAUSED-AA3-20261011.md)에 보존했다. 전체 이식 완료가 아니다.
