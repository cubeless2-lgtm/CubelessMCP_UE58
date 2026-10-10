# UE 5.8.3 시작 스모크 테스트 실패 조사

2026-10-10. 읽기 전용 조사이며 엔진·설정·플러그인 소스 변경, 추가 에디터 실행, 테스트 재실행은 하지 않았다.

## 확인된 사실

- 기존 실행 로그 `cubeless/records/verification/implementation-2026-10-10/editor-component-R.log`의 1800–1812, 1839–1843행에 Core 스모크 테스트 3개와 `Condition failed` 15건이 있다: `FUnifiedErrorTest_CreateErrorMessage` 7건, `FUnifiedErrorTest_CreateErrorMessageWithContext` 4건, `FStructuredLogFormatTest` 4건. 단순 로그 잡음으로 삭제하거나 성공으로 처리하지 않는다.
- 같은 로그 458행은 설치 엔진 `5.8.3-58210709+++UE5+Release-5.8`, 864–866행은 OS language/locale `ko-KR` 및 언어 `ko` fallback을 기록한다.
- 실패 시각은 13:12:30.996 UTC이다. Cubeless 네이티브 DLL 로드와 등록은 그 뒤인 13:12:31.711/731(2626–2630행)이다. 이 실패들은 새 R 도구를 호출한 결과가 아니다.
- 엔진 `Source/Runtime/Core/Tests/Experimental/UnifiedError/UnifiedErrorTests.cpp:479–535`의 두 테스트는 각각 정확히 7개와 4개의 영어 문자열 비교를 한다. 예: 485행의 `Empty error`, 489행의 `Error with int -7`.
- `Source/Runtime/Core/Private/Experimental/UnifiedError/UnifiedError.cpp:84–125`는 지역화된 FText/구조화 로그 경로를 사용한다. `Private/Logging/StructuredLog.cpp:162–172`도 `$locformat/$locns/$lockey`를 지역화 템플릿으로 해석한다.
- `Source/Runtime/Core/Public/Misc/LowLevelTestAdapter.h:130`의 `CHECK`는 표현식·실제 값을 출력하지 않고 고정 `Condition failed`만 기록한다. 따라서 현재 로그만으로 실패한 개별 문자열의 실제 출력은 확인할 수 없다.

## 설치 번역 자료 대조

엔진 파일을 수정하지 않고 외부 Python의 메모리 내 바이너리 판독으로 `Engine/Content/Localization/Engine/{en,ko}/Engine.locres`를 비교했다. 판독 형식은 설치 소스 `Private/Internationalization/TextLocalizationResource.cpp:288–403`, `:190–194`, `Private/Internationalization/TextKey.cpp:805`에 근거했다. 양쪽 버전 3, 66,406개 항목·7,451개 namespace, 문자열 테이블 경계·파일 끝·항목 수를 확인했다. 아래 항목들의 영문/한국어 source hash는 서로 같았다.

| Namespace / key | 한국어 리소스 내용 | 테스트와의 관계 |
|---|---|---|
| `UE::UnifiedErrorTest / Empty` | `빈 오류` | 영어 `Empty error` 고정 비교와 다름 |
| `UE::UnifiedErrorTest / WithInt` | `int {IntParam}에 오류가 있음` | 영어 정수 오류 문구와 다름 |
| `UE::UnifiedErrorTest / StringContext` | `통합 오류 스트링 컨텍스트 {StringParam}` | 영어 context 고정 비교와 다름 |
| `StructuredLogFormatTest / EmitterSubheaderText` | `{IntegerPositive}개의 {IntegerPositive}오류가 발견되었습니다!` | 테스트 276행은 `Found 63 errors!` 기대 |
| `StructuredLogFormatTest / ObjectWithNestedLocFormat` | `타깃=({Point}); X={Point/X}` | 테스트 237·314행은 `Target=` 기대 |

UnifiedError의 다른 대상 5개(`WithUint`, `WithIntString`, `WithIntStringFloat`, `WithArray`, `WithStruct`)도 한국어 번역이 존재하며 영어 기대값과 다르다. StructuredLog 테스트의 237행은 TCHAR/UTF-8 두 경로(175–193행)에서 실행되어 2회, 314행과 276행은 각 1회 비교한다.

리소스 SHA256:

- en: `88a8a93a9ccc70ae63612b90f4c779c214ee481198c9e3a45f69ac1fae73b1ef`
- ko: `e71e17c6057590838358f9289b070e4158af9a59c796043fa68947b0327f324b`

## 판단과 한계

가장 유력한 원인은 **영어 결과를 고정 기대하는 설치 엔진 테스트와 실제 한국어 지역화 출력의 불일치**다. 번역 대상과 호출 횟수가 로그의 7 + 4 + 4 실패 수와 정확히 맞는다. 설치 소스와 번역 자료의 이 불일치는 확인했지만, 로그가 실제 비교 값을 숨기므로 15개 런타임 값의 직접 증명 또는 영어 culture에서의 재실행 성공을 주장하지 않는다. 엔진 바이너리/소스 버전 불일치나 파일 손상을 뒷받침하는 증거도 찾지 못했다.

이 엔진 스모크 실패는 Cubeless 기능 빌드·등록·저작 검수와 별도로 기록한다. 이 조사로 엔진 전체 건강성이 통과했다고 선언하지 않는다. 사용자 언어 설정 변경, 테스트 비활성화, 엔진 테스트 수정은 수행하지 않았고 현재 R 작업의 선행 조건으로 요구하지 않는다.
