# UE 5.8.3 Python closure와 검증 fixture 수명

## 확인한 원인

설치 PythonScriptPlugin의 FPyCFunctionWithClosureObject는 Win64에서 40바이트이며 PyObject_GC_New로 이 크기만 할당한다. PyCFunction_Type를 base로 설정하면서 CPython 3.11.8의 weakref 오프셋 40을 상속한다. GC가 이 슬롯을 읽으면 객체 저장소 밖을 참조할 수 있다. 엔진 소스와 DLL은 수정하지 않았다.

EZ 원 덤프의 type 주소는 PythonScriptPlugin RVA 0x253090이다. 새 격리 세션에서 같은 DLL의 builtin_function_or_method_with_closure 타입이 동일 RVA이고 __basicsize__=40, __weakrefoffset__=40임을 확인했다. Python 3.11.8 공식 PDB의 GUID/age가 설치 DLL과 일치하며 handle_weakrefs의 잘못된 포인터 읽기를 확인했다. 원인 타입은 식별했지만 최초 heap 전체 그래프나 단일 변수 크래시 재현 실험을 수행한 것은 아니다.

## 이 작업의 적용 조건과 회피

원 fixture는 임시 exec 전역 사전에 함수를 정의하고 bound Unreal getter도 남겼다. 함수의 __globals__가 같은 사전을 가리켜 도달 불가능한 순환 참조가 될 수 있다. 이 조건을 SDK 경계 결함을 드러낸 trigger로 분류한다. 모든 Python 순환 참조나 모든 Unreal getter가 반드시 크래시한다는 뜻은 아니다.

현재 사용할 fixture는 ModuleType을 sys.modules의 정확한 소유 키에 유지한 뒤 실행한다. fixture와 callbacks가 사용하는 동안 전역 공간과 bound callable의 수명을 유지한다. 정리가 필요하면 작업·callback 종료와 보고서 저장 후, 모듈이 도달 가능한 동안 bound native callable과 함수 전역 순환을 먼저 해제한다. 그다음 자신이 넣은 정확한 sys.modules 항목만 제거한다. 사용자 모듈과 에셋은 정리하지 않는다.

GC 비활성화, Python 타입 슬롯 패치, engine DLL 변경을 제품 해결책으로 사용하지 않는다. 진단 목적으로 자동 GC 상태를 바꿨다면 기존 상태를 finally에서 복구한다. 새 일반 검증 세션은 기본 GC 설정으로 시작한다. 공개 Toolset API나 엔진 작업 로직을 이 진단 코드와 결합하지 않는다.

## UE 5.8.3 검증 근거와 한계

- REC/debug-symbols/python3118/minidump-weakref-analysis.json: 보존 덤프·정확한 PDB·명령어·레지스터 대조.
- REC/source-candidates/EZ/Review/CrashRegistration/weakref-layout-source-addendum-r2.json: 설치 SDK/CPython 배치 결함.
- REC/source-candidates/EZ/Review/CrashRegistration/weakref-exact-type-evidence-review.json: 실제 타입 RVA와 원 fixture 순환 관계의 독립 검토.
- REC/gc-layout-EZ-recovery2.json: 모듈 소유 fixture에서 explicit Python GC 340개/111개 정상 반환.
- REC/minimal-api-EZ.json: 실제 공식 MCP StartPIE, PIE 속성 쓰기·Tick1·Refresh1·curve0.25, StopPIE, 부분 실패 배치와 저널3 대조.

REC는 cubeless/records/verification/implementation-2026-10-10/이다. recovery2는 진단 시작 시 gc.disable()을 사용했다. explicit Python GC와 UE PIE 진입·종료의 GC 경계는 통과했지만, 기본 자동 GC 조건이나 SDK 전체 안정성 검증으로 확대하지 않는다. 이후 정상 bootstrap은 모듈 수명 관리만 적용하고 기본 자동 GC로 검증한다. UE 5.6에서 확인된 경험으로 분류하지 않는다.

## 후속 기본 GC 확인

FA 정상 bootstrap은 gc.disable()이나 타입 패치를 사용하지 않았다. fixture-normal-GC-FA.json은 기본 자동 GC가 explicit collection 전후 모두 True이고 33개 수집이 반환됐음을 기록한다. 같은 모듈 소유 fixture의 기본 GC 세션에서 실제 공식 MCP SIE 진입·케이스2 Tick/Refresh·종료와 최종 clean/noPIE를 확인했다. 근거는 REC/minimal-api-FA.json이다. 이전 recovery2의 GC-disabled 조건은 원 기록에 보존하며, 이 후속 검증을 SDK 일반 배치 결함의 수리나 단일 변수 원 크래시 재현으로 취급하지 않는다.

## HD 순수 C++ PCG 경로의 한정 확인

HD에서는 Python에 PCG UObject를 반환하지 않는 Editor 전용 C++ 진단으로 임시 `UPCGGraph`와 stock `UPCGCreatePointsSettings` 노드 1개를 생성했다. 모듈이 소유한 Python fixture에서 기본 자동 GC를 유지하고, 생성 후 `gc.collect()` 132개 반환, 다음 게임 스레드 프레임 2→3의 정확한 graph/node/settings 소유·멤버십 관찰, 명시적 strong reference 해제, 해제 후 `gc.collect()` 442개 반환을 확인했다. 저장·생성 작업·컴파일 요청은 0회다. 근거는 `REC/native-canary-HD.json`과 `REC/HD-execution-report.json`이다.

이 결과는 과거 Python wrapper/임시 exec 수명 경로와 이번 순수 C++ 경로의 증거를 구분한다. Python weakref 배치 결함이 고쳐졌거나 PCG의 모든 생성·selector 교체·컴포넌트 실행·Blueprint 컴파일·종료 경계가 안전하다는 뜻은 아니다. 기존 PCG 작업 게이트는 변경하지 않았으며, 향후에는 실제 변경 경로에 맞는 증거를 사용한다. 정상 Editor 종료와 강제 Unreal GC는 이번 진단에서 검증하지 않았다.
