# 패키징 독립성 검수기의 증거 범위와 반례 검증

2026-10-10. 구현 파일: [check_package_independence.py](../../../tools/verification/check_package_independence.py). 검수기 자체 테스트: [test_package_independence.py](../../../tools/verification/test_package_independence.py).

이 기록은 외부 Python 검수기 구현 및 합성 반례 테스트 결과다. UnrealPak, Unreal Editor, 패키징된 게임을 이 테스트에서 실행하지 않았다. 합성 입력은 임시 디렉터리에 생성했고 실제 실행 파일로 가장해 실행하지 않았다. 실제 UE 패키징 검증 결과로 해석하면 안 된다.

## 수정한 허위 통과·실패 조건

- 사용자가 제공한 임의 과거 목록으로 모든 pak을 검사했다고 간주하지 않는다. stage의 모든 `.pak`마다 지정한 실제 UnrealPak을 직접 호출하고 명령·종료코드·pak 전후 SHA-256·로그 SHA-256을 남긴다.
- pak 목록의 일부 정상 항목만 발견해 통과시키지 않는다. 설치 UE 5.8 `PakFileUtilities.cpp:3456`, `:3463`의 항목/총계 출력 형식을 사용하며 확장자 필터 없이 모든 항목을 검사하고 총계와 대조한다. 파일명 비공개·실행 실패·타임아웃·내용 변경·목록 일부 누락은 incomplete다. 0개 항목이라는 완전한 총계는 빈 pak으로 인정한다.
- 임의의 비어 있지 않은 폴더를 게임 stage로 인정하지 않는다. Win64 실행 파일과 해당 이름의 Executable build product를 가진 Game/Client/Server `.target` receipt가 필요하다. UAT가 receipt를 stage 밖에 두면 `--receipt`로 실제 파일을 지정한다. 이름 일치 확인이며 암호학적 build binding은 아니다.
- descriptor/receipt는 JSON으로 읽으며 파싱 실패를 incomplete로 보고한다. `.uproject`/`.uplugin`의 명시적 `Enabled: false` 또는 `TargetAllowList: ["Editor"]` 참조는 게임 의존성으로 오인하지 않고 별도 기록한다. 전체 UBT 조건 해석기를 구현한 것은 아니므로 그 외 조건은 보수적으로 검사한다. 실제 금지 플러그인 파일이 stage에 있으면 경로 검사에서 여전히 실패한다.
- `.utoc`/`.ucas`는 지원하지 않는 IoStore 증거로 명시하고 incomplete 처리한다.
- pak 해시 파일 핸들은 context manager로 닫는다. 타임아웃 출력도 증거 파일에 보존하며 기존 증거를 덮어쓰지 않는다. 검사 시작/끝 stage 파일 목록·크기·mtime 변화를 확인한다. 출력과 pak 목록 디렉터리는 stage 밖에 둔다.
- runtime 로그를 제공했다면 marker 부재, 실제 종료코드 비정상, fatal/load error는 최상위 fail이다. 종료코드 미제공은 incomplete다. marker가 실행 명령행에만 나타난 경우에는 실행 증거로 인정하지 않는다. `aqProf.dll` 같은 선택적 프로파일러 DLL 경고는 asset load failure로 오인하지 않는다.

## 상태 계약

| 필드/값 | 의미 |
| --- | --- |
| `static_status: pass` | 구현된 파일·receipt·pak 항목명 검사 범위 통과 |
| `status: static_pass` | 정적 범위 통과. 게임 실행은 이 결과에서 확인하지 않음 |
| `status: fixture_pass` | 정적 범위 통과 및 제공된 fixture 로그 marker·종료코드 관측 |
| `status: incomplete` | 필수 증거 누락·미지원 형식·읽기/목록 실패 등 |
| `status: fail` | 금지 참조 또는 제공된 runtime 검증 실패 |

CLI exit 0은 `static_pass` 또는 `fixture_pass`의 선택된 검사 범위가 통과했다는 뜻이다. 그 외는 exit 1이다. CLI 인수 오류는 argparse의 exit 2다.

`packaging_independence_proven` 및 `full_migration_complete`는 항상 false다. 이 검수기는 바이너리 UObject 내부 참조를 분석하지 않으며 게임을 빌드·cook·실행하지 않는다. 정적 링크된 모듈, 동적 로드 경로, 게임 전체 동작에 대한 완전 증명이 아니다. 실제 빌드 설정·cook 로그·에셋 의존성 감사·플러그인 비활성 상태 재로드와 독립 실행 증거를 별도로 결합해야 한다. runtime 로그도 외부 runner가 제공한 자료이고 현재 stage/process에 암호학적으로 결합하지 않는다.

## 실행 방법

아래의 `<...>` 경로와 marker는 해당 실행에서 생성한 실제 증거로 바꾼다. 이 예제는 사용법이며 실행 결과가 아니다.

```powershell
python -B C:/Git/UnrealMCP5_8_3/UnrealMCP5_8_3/cubeless/tools/verification/check_package_independence.py --stage "<실제 Win64 stage>" --unrealpak "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealPak.exe" --receipt "<실제 Game/Shipping .target>" --output "<stage 밖 신규 report.json>"
```

외부 runner로 실제 게임을 실행한 후 생성된 로그를 함께 검사할 때는 `--runtime-log "<실제 로그>" --runtime-marker "<fixture 고유 marker>" --runtime-exit-code <실제 프로세스 종료코드>`를 추가한다. 타임아웃으로 강제 종료한 실행을 정상 종료코드 0으로 바꾸어 보고하지 않는다.

## 검수기 자체 테스트

실행 디렉터리: `C:/Git/UnrealMCP5_8_3/UnrealMCP5_8_3/cubeless/tools/verification`.

```powershell
python -B -m unittest -v test_package_independence
```

관측 결과: Python 3.12에서 21개 테스트 통과, unittest 측정 0.166초, 프로세스 exit 0. 이는 합성 테스트 수행시간이고 UE 성능 측정값이 아니다. CLI `--help`도 실제 실행으로 확인했다.

반례에는 두 번째 pak만 금지 파일을 포함하는 경우, 다른/과거 listing 대체 거부, 잘린 목록, 목록 실패·타임아웃·pak 변경, 새로운 확장자 `.pyw`, 기존 증거 덮어쓰기 거부, Editor receipt 오용, 실행 파일/receipt 부재, 잘못된 JSON, IoStore, 비활성 plugin 참조와 활성 Game 참조의 구분, runtime fatal, 명령행 marker 오인, 종료코드 누락, 정적 통과와 fixture 통과의 증명 한계가 포함된다.
