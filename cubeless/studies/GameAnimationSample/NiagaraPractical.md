# Niagara 실무 보강: GPU 준비 상태와 타격 이펙트 종료

UE 5.8.4 CL58902052에서 기존 Cubeless Niagara API를 사용해 CPU/GPU 시스템 편집·실제 타격 전달·종료를 확인했다. 필수 런타임 플러그인은 Niagara다. 저작에는 공식 MCP·Toolset Registry·Python·Cubeless Editor 플러그인을 사용하지만 생성하는 게임 Blueprint는 표준 Engine/Niagara만 참조한다.

기존 샘플·ArenaUI를 보존한 소유 복사본으로 학습했다. 검증 맵·Blueprint·이미지·원시 보고서는 개발 checkout의 로컬 자료이며 이 문서와 함께 배포되는 에셋이 아니다. 이 문서는 해당 에셋이 없어도 작업 순서와 판정 기준을 재현할 수 있도록 기록한다. 기존 근접전 시스템 전체의 신규 배포나 패키징을 뜻하지 않는다.

## 편집·호출 순서

1. 공식 `describe_toolset`에서 현재 프로젝트의 Niagara Toolset·입출력을 확인한다.
2. `inspect_niagara_stack`으로 기존 모듈·핀·렌더러를 조회한다.
3. `upsert_niagara_user_parameters`로 `User.HitColor`와 `User.HitSize`를 선언한다. 선언 성공과 모듈의 실제 소비는 다르다.
4. 기존 Scratch/User recipe로 각각 `Particles.Color`와 `Particles.SpriteSize`에 연결하고 `set_niagara_module_inputs_batch`로 필요한 입력을 편집한다.
5. `set_niagara_renderer_material`로 실제 스프라이트 머티리얼을 지정한다. 예제는 `/Niagara/DefaultAssets/DefaultSpriteMaterial.DefaultSpriteMaterial`을 사용했다.
6. GPU 시스템은 `set_niagara_emitter_sim_target`에서 `sim_target="GPU"`, 고정 bounds를 명시한 뒤 컴파일한다.
7. `inspect_niagara_compile_status`의 전체 시스템·필수 스크립트 상태와 아래 GPU shader 필드를 확인한다.
8. 편집은 `save=false`로 묶고 결과를 관측한 뒤 소유 패키지만 명시적으로 저장한다. Blueprint 바인딩·manifest와 바깥 저장 정책도 일치시킨다.

현재 Toolset 이름과 짧은 JSON 예제는 [운영 가이드](../../docs/operations.md#niagara-실무-경로-보강--2026-10-31)에 있다. 외부 `production_niagara` 실행기의 GPU 자동 제작·검수 전체 통합과 직접 Niagara API의 GPU 사용 가능 범위는 구분한다.

## GPU 준비 상태

기존 `is_ready_gpu`는 VM/DI 실행 적합성이다. `ParticleGPUComputeScript`에는 현재 Editor RHI 기준으로 다음을 추가했다.

| 필드 | 의미 |
| --- | --- |
| `gpu_shader_compilation_applicable` | GPU Compute Script에 해당하는지 여부 |
| `gpu_shader_compilation_pending` | GPU shader 컴파일 대기 여부 |
| `gpu_shader_compilation_succeeded` | 현재 shader map의 컴파일 성공 여부 |
| `gpu_ready_to_execute` | 대기 없음·shader 성공·VM/DI 적합·허용된 컴파일 상태의 조합 |

CPU 스크립트의 뒤 세 필드는 `null`이다. 정상 상태 외에 `NCS_UpToDateWithWarnings`와 `NCS_ComputeUpToDateWithWarnings`도 나머지 조건을 만족하면 허용한다. 조회의 `success=true`, idle 또는 `is_ready_gpu=true` 하나로 GPU 준비를 단정하지 않는다. 이 값들은 컴파일 상태의 관측이며 GPU 프레임 완료·실제 출력의 증거가 아니다.

## Blueprint의 전달·수명·결과 계약

실제 체력이 감소한 승인 타격에서만 Burst Actor를 만든다. 중복 Notify나 취소된 공격은 추가 피해·이펙트를 만들지 않는다. 표적과 이펙트 수명을 분리한다.

- `InitializeBurst`는 유효한 self와 `!Started && !Ending && !OutcomeFinal`을 확인한다.
- 자동 활성화하지 않은 Niagara 컴포넌트를 만들고 콜백 이후 self/컴포넌트를 확인한다. self가 사라졌으면 생성된 고아 컴포넌트만 정리한다.
- `OnSystemFinished`를 바인딩하고 typed Color/Vector2 setter를 실행한다. 요청 시각·Armed를 설정한 뒤 마지막으로 Activate한다. Activate 이후 멤버 쓰기는 하지 않는다.
- 공유 경로의 활성화 수락은 `IsActive && !IsComponentTickEnabled`로 관측한다. `IsActive`만으로는 PSO 준비 대기를 구분할 수 없다. 이 관측도 GPU 프레임 진행의 증거는 아니다.
- 방출은 활성화 요청부터 GameTime 0.10초 이상인 다음 Tick에서 중단한다. `EmissionStopped=true`를 Deactivate 전에 확정한다. Solo/관측 대기 상태에서도 이 중단 정책을 유지한다.
- `BurstFinished`는 유효한 self·sender, sender와 FX의 일치, `!Ending`, `!OutcomeFinal`을 검사한다.
- 최종 결과는 `OutcomeFinal=true`를 먼저 기록한 뒤 상태·Completed·Armed를 확정한다. `CancelBurst`도 결과를 먼저 확정하고 컴포넌트를 정리한다. 늦은 콜백과 중복 취소가 이전 결과를 덮어쓰지 않는다.
- EndPlay는 Ending을 먼저 설정하고 미확정 결과를 기록한 뒤 FX를 정리한다.

| 최종 상태 | 판정 |
| --- | --- |
| `normal_complete` | 공유 활성화 수락 관측 → 자체 방출 중단 → 동일 FX 완료 콜백. 이 경우만 `Completed=true` |
| `activation_failed` | invalid System, components 비활성, 컴포넌트 생성 실패 등 확인된 실패 |
| `early_terminated` | 명시적 취소 또는 관측된 실행의 조기 소멸·비활성·owner 종료 |
| `activation_unconfirmed` | 실행 수락을 확인하지 못한 상태에서 종료. Solo/대기 경로를 성공으로 집계하지 않음 |

`normal_complete`는 이 예제의 관리 순서가 완료됐다는 의미이며 원래 Niagara lifetime의 자연 종료나 GPU 실행 완료를 뜻하지 않는다. 취소는 `CancelBurst`를 사용한다. `HitEffectCount`는 Burst 생성·시도 수이며 성공한 방출 수가 아니다. 최종 `EffectState`와 `FailureReason`을 함께 확인한다.

## 검증과 적용 범위

- UE 5.8.4 증분 C++ 빌드가 20.63초에 통과했다. 기존 등록 이름·입력 기본값은 유지하고 새 도구는 추가하지 않았다.
- 공식 MCP의 소유 PIE에서 CPU/GPU 정상 완료, components 비활성 실패, 중단 전 취소, Solo 관측 미확인, 중단 후 취소의 6개 조건이 통과했다. 실패·취소·미확인에서는 `Completed=false`를 유지했고 CVar를 원래 값으로 복원했다.
- 별도의 새 프로세스에서 저장된 Blueprint의 CPU/GPU 정상 경로 2건을 확인했다. 콜드 실행 시간은 2.047초였다.
- 기존 대표 렌더는 Windows D3D12/SM6·RTX 5060 Laptop GPU, 256×256/FOV45 상대 카메라에서 CPU/GPU 색상·크기 반영을 확인했다. 정확한 GPU 파티클 수·좌표·결정성은 주장하지 않는다.
- 실제 `NCS_ComputeUpToDateWithWarnings` shader와 지연 PSO는 재현하지 않았다. 경고 허용 조건은 설치 소스와 대조했고, 별도 경고 유도 시도는 경고 없이 정상 컴파일됐다. Solo 검사는 보수적 미확인 분기의 증거다.
- 의도적인 잘못된 HLSL 수정은 원 실패·부분 변경·미저장·불확실 상태를 보존했다. 후속 관측과 별도 보존 저장을 원 API 성공이나 자동 재시도로 바꾸지 않았다.
- 새 런타임 모듈이나 MCP 게임 의존성은 추가하지 않았다. 이번에는 새 패키징·멀티플레이·전체 플랫폼 검증을 수행하지 않았다.

설치 소스 근거는 `NiagaraScript.cpp`의 `GetLastCompileStatus`, `IsScriptCompilationPending`, `DidScriptCompilationSucceed`, `IsReadyToRun`과 `NiagaraComponent.cpp`의 `ActivateInternal`, `DeactivateInternal`, `OnSystemComplete`다. 재현 도구는 기존 공식 MCP와 Cubeless Niagara/Blueprint Editor API다. 원시 성공·실패·수정·재리뷰·소유 세션 종료 근거는 로컬 `cubeless/records/operations/niagara-practical-20261031/report.json`에 보존하며 Git에는 포함하지 않는다.
