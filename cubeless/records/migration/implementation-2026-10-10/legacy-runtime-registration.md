# UE 5.6 실제 FastMCP 등록 목록 대조

2026-10-10 실행. **정적 정의 436개와 실제 등록 이름 합집합 436개가 정확히 일치했다.** 누락과 추가 이름은 각각 0개다. 이는 기존 서버의 등록 목록 검증이며, 각 기능의 UE 실행·5.8 이식·동등성 검증 완료를 뜻하지 않는다.

| 격리 프로세스 구성 | 실제 `FastMCP.list_tools()` 공개 목록 | 내부 실제 catalog | 정책 registry | 결과 |
| --- | ---: | ---: | ---: | --- |
| `search` (기본 노출 방식) | 3 | 433 | 433 | 통과 |
| `eager` (기존 직접 노출 방식) | 433 | 433 | 433 | 통과 |

`search` 공개 도구는 `call_tool`, `describe_toolset`, `list_toolsets`다. 433개 catalog는 도메인 도구 432개와 관리 도구 1개로 구성된다. 이 catalog와 검색 메타 도구 3개의 합집합이 436개다. `eager`에서는 메타 도구를 등록하지 않고 catalog를 직접 공개한다. 따라서 436개를 한 연결에서 모두 직접 광고하는 도구 수라고 설명하면 부정확하다.

두 프로세스의 catalog 이름과 입력 JSON Schema는 모두 동일했다. 기본 설정에서 433개 registry 항목이 모두 enabled였으며 25개 범주(도메인 24개, 관리 1개)를 확인했다. 기존 조사 파일의 436개 정의 각각에 대한 source 경로·행·해시와 `runtime_catalog`, `search_public`, `eager_public` 대응은 `legacy-runtime-registration.json`에 보존했다.

## 실제 실행과 격리 범위

- 원본 `C:\Git\UnrealMCP5_6_1\Python`의 서버, `tools`, `services`에서 Python 파일 41개를 바이트 그대로 격리 디렉터리에 복사했다. 수정·재구성한 등록 함수, 가짜 FastMCP, 가짜 registry, 도구 decorator 대체는 사용하지 않았다.
- 원본에 설치된 Python **3.11.15**, `mcp` **1.5.0**, `pydantic` **2.10.6**을 사용했다. 각 child는 `-I -B -S`로 실행했으며, 원본 site-packages를 명시적으로 읽어 `.pth` 시작 코드는 실행하지 않았다.
- `UNREAL_MCP_*` 환경 변수는 child에서만 비운 뒤 노출 모드를 지정했다. 호스트 비밀 값은 기록하지 않았다. 따라서 실제 실행 중인 별도 서버의 사용자별 enable/disable 설정을 측정한 것은 아니다.
- 서버 모듈 import 전에 CPython audit hook을 설치했다. 모든 `socket.*` 이벤트, 외부 프로세스 실행, 격리 폴더 밖 쓰기와 파일시스템 변경을 차단하도록 했다. 두 실행에서 이 이벤트의 실제 시도는 모두 **0회**였다.
- 서버 import 시 상대 경로로 만드는 `unreal_mcp.log`는 각 격리 실행 폴더에만 기록됐다. Python bytecode 쓰기는 비활성화했다.
- 실제 SDK의 `FastMCP.list_tools()`는 1.5.0 소스에서 await 없이 완료되는 코루틴이다. 이 실제 코루틴을 `send(None)`으로 완료시켜 SDK가 반환하는 MCP Tool 모델을 수집했다. asyncio의 소켓 초기화, 서버 transport, stdio 세션, 서버 lifespan은 시작하지 않았다.
- 원본 `server_lifespan()`의 UE 연결은 실행하지 않았다. import 후 `server._unreal_connection is None`, 모든 요청 telemetry 카운터 0, 송수신 바이트 0을 확인했다. 도메인 도구 본문은 호출하지 않았다.
- 로드된 legacy 모듈 38개의 파일이 모두 격리 소스 아래에 있는지 확인했다. 설치 SDK class identity와 SDK 소스 해시도 기록했다.
- 41개 원본 파일의 실행 전후 SHA-256은 모두 동일했고, 기존 계획 인벤토리에 기록된 436개 정의의 파일 해시도 현재 원본과 모두 일치했다.

이는 Python 수준 감사·차단을 적용한 실제 등록 실행이다. 운영체제 전체 네트워크 캡처나 강제 보안 sandbox, 다른 프로세스의 동작 관측으로 표현하지 않는다. 또한 MCP wire protocol, UE 5.6 endpoint, UE 5.8 endpoint 호출 검증으로 계산하지 않는다.

## 측정 조건과 결과

Windows 로컬 단일 child 프로세스를 모드별로 순차 실행했다. 시간은 SDK·legacy import 및 등록, 스키마 수집, 모듈 해시 확인을 포함한 `perf_counter` 구간이며, 파일 복사와 프로세스 시작·종료 시간은 제외한다. 검색 모드 **2.867초**, eager 모드 **1.366초**였다. 순서와 파일 캐시가 달라 성능 비교나 도구 처리량 벤치마크로 사용하지 않는다. 두 프로세스 종료 코드는 0이다.

## 증거 파일과 재실행

- `legacy-runtime-registration.py`: 실행한 감사 harness. 격리 폴더가 이미 존재하면 덮어쓰지 않고 중단한다.
- `legacy-runtime-registration.json`: 전체 대조 결과와 정의별 등록 대응.
- `legacy-runtime-registration.sources.json`: 복사한 41개 소스 경로, 크기, SHA-256.
- `legacy-runtime-registration.search.json`, `.eager.json`: 실제 public Tool 모델, catalog 함수 출처·설명·입력 스키마, registry 정책, telemetry, 감사 결과, 실행 명령, SDK 근거.
- `legacy-runtime-registration.search.stdout.log`, `.eager.stdout.log` 및 각각의 `.stderr.log`: 프로세스 출력. stderr는 두 모드 모두 비어 있었다.
- `legacy-runtime-registration.isolated/`: 수정하지 않은 서버 소스 복사본과 모드별 로그·원본 결과.

실행 명령은 다음과 같다. 기존 기록을 보존하므로 같은 폴더에서 이 명령을 재실행하면 명시적으로 중단한다. 다시 측정하려면 harness와 출력 위치를 새 기록 디렉터리에 배치하고 `INVENTORY` 경로를 해당 기존 계획 인벤토리로 지정한다.

```powershell
& 'C:\Git\UnrealMCP5_6_1\Python\.venv\Scripts\python.exe' -I -B -S 'C:\Git\UnrealMCP5_8_3\UnrealMCP5_8_3\cubeless\records\migration\implementation-2026-10-10\legacy-runtime-registration.py'
```

등록 대조로 해소한 항목은 **기존 공개 Python 도구의 실행 시 등록 누락 여부**다. Python→명령→C++→엔진 API 기능 동등성, 보조 스크립트·검수 파이프라인 5.8 실실행, 저장·재로드·패키징 독립성은 별도 검증 대상으로 남는다.
