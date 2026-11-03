# RX_SMLODGen — 구현 설계 및 조사

## 임포스터 방향 선택 및 자기 그림자 억제 (2026-11-03)

- 게시 전 자체 코드 리뷰에서 심플/고급 상태 공유, 방향 수 전파, v1~4 이관, 원본 변경 전 검증, 기존 생성 머티리얼 교체 경로를 확인했다. 추가 기능 오류는 발견하지 않았고 문서 줄바꿈 형식만 기존 LF로 정리했다. 위의 두 실행 보고서에서 동일 항목의 마지막 결과를 취하면 관련 13개 검사가 모두 성공이다. DLL 해시는 기록과 일치하며 이번 리뷰에서 기능 코드는 변경하지 않았으므로 빌드/에디터 검사를 반복하지 않았다.

- 심플 탭 마지막에도 임포스터 활성화·방향 선택·재생성을 넣었다. 두 탭이 같은 세션을 편집한다.
- 기본 4방향(0·45·90·135도)과 2방향(정면 +X / 측면 +Y)을 선택한다. 캡처·메시·UV·아틀라스가 같은 방향 수를 사용한다. 원본 크기·피벗·로컬 +Z 노멀·고정 양면 카드를 유지하며 마지막 임포스터 하나를 교체한다.
- 설정 JSON과 에셋의 RX 메타데이터는 v5로 방향 수를 저장한다. v1~4는 기존 4방향으로 복원하며 잘못된 방향 수는 사본을 변경하지 않고 거절한다.
- 생성 LOD는 CastShadow=false, 머티리얼은 ShadowPassSwitch로 그림자 패스의 마스크를 0으로 만든다. 기존 소유 머티리얼을 재사용할 때도 이 연결을 추가한다. Lit 조명과 외부 그림자 수신은 유지한다. 임포스터가 바닥에 만드는 그림자도 함께 없어진다. Lumen/AO/컨택트 그림자 및 기존 LOD 0 기반 라이트맵·디스턴스 필드의 어두워짐은 별도 렌더링 경로이므로 완전 제거를 보장하지 않는다.
- UE 5.8.4 CL58902052 / D3D12에서 `Saved/RX_SMLODGen/directions_20261103/report/index.json`의 관련 12개 중 11개 성공, 1개 실패(구버전 변환 테스트가 버전 번호 4를 하드코딩하여 새 v5를 v1으로 변환하지 못함). 검사 입력을 JSON 객체로 수정한 뒤 `directions_final_20261103/report/index.json`에서 WeightMetadata, KoreanTooltips, Persistence.Reload 3개 모두 성공, 실패/경고 0이다. 실제 2·4방향 캡처/지오메트리/UV/아틀라스, 마지막 LOD 및 머티리얼 경로 교체, v5 방향 복원/구버전 기본값, 심플 UI, 원본 적용 및 저장·재시작 후 그래프/픽셀/메타데이터 복원을 확인했다.
- 라이브 에디터에서 심플 탭의 2방향 선택과 실제 마지막 LOD(64 삼각형/50 버텍스)를 확인하고 `Saved/RX_SMLODGen/Previews/simple_two_directions_20261103.png`를 Slate로 캡처했다. 이후 기존 v4 JSON 설정을 v5 기본 방향 4로 이관한 값을 제외하고 정확히 복원했으며 dirty content/maps 및 PIE 없음이 `directions_live_20261103/restored.json`에 기록되었다. PID 31704는 사용자가 확인할 에디터로 유지한다.
- 첫 컴파일은 새 테스트의 int64/int32 `TestEqual` 타입 불일치로 실패했고 타입을 맞춘 뒤 빌드 성공했다. 최종 DLL SHA256 `9045FC59E1ACD61779FC0FED39BDA6270BE28FF46C4D1B9A4F06DA0A5244C81D`. 테스트 PID 31344와 8360은 TestExit 후 종료했다. 패키징/모바일, 실제 복잡한 에셋과 모든 조명 경로의 시각적 자기 그림자 제거 비교는 이번 검사 범위가 아니다.


상태: UE 5.8.4에서 RX 전용 LOD 편집 창과 JSON 인터페이스를 구현했다. 합성 테스트 에셋의 적용·저장·별도 프로세스 재로드를 확인했다. 실제 사용 에셋의 시각 품질과 독립 검토, cook/package 실행은 아직 미검증이다.

## 2026-11-03 심플·고급 설정 탭

- 오른쪽 설정 영역에 `심플`/`고급` 탭을 추가하고 처음 열 때 심플을 표시한다. JSON 버튼은 공통이다. 고급은 기존 전체 패널을 유지하고, 심플은 맨 위 LOD 개수와 LOD별 리듀스율·스크린 사이즈·섹션 옵션 전부만 표시한다. 심플은 뷰포트의 강제 LOD 선택과 별개로 전체 LOD 카드를 표시한다. LOD 0의 섹션은 처음 접힌 상태이며 펼쳐서 볼 수 있다.
- 두 탭은 같은 `FRXLODSession`을 편집한다. 섹션 편집 위젯과 LOD 개수 입력을 재사용하며 별도 설정 사본/프리셋을 만들지 않는다. 탭을 바꾸는 것만으로 재빌드하거나 설정을 초기화하지 않는다. 모드별 펼침 상태를 구분하고 섹션 강조·격리 체크는 실제 뷰포트 상태를 읽는다.
- 심플 리듀스율은 줄이는 양(0%=감소 없음)이며 고급의 남길 비율과 반대로 표시한다. 버텍스 종료 기준을 쓰면 버텍스 비율을 편집하며 Any 기준에서는 삼각형/버텍스를 각각 표시한다. 표시 소수점은 최대 2자리로 정리하여 60.000004와 같은 부동소수점 잔여값을 숨긴다. 내부 설정 값은 그대로 유지한다.
- 스크린 사이즈는 기본값만 간단하게 편집하고 플랫폼별 재정의는 유지한다. 직접 입력하면 기존 동작대로 자동 계산을 끄며 미리보기 적용으로 확인한다. 임포스터의 생성 설정은 두 탭의 마지막에 함께 표시하고 심플의 생성 LOD에는 자동 화면 크기·섹션을 표시한다. 미리보기 적용/원본 적용 동작은 기존 버튼을 사용한다.
- AI 인터페이스에 `panel_mode`와 `get.panel_mode`를 추가했다. UI 표시 상태만 바꾸므로 설정 JSON 버전 4는 유지한다. 사용법은 [JSON_API.md](JSON_API.md)를 따른다.
- `Saved/RX_SMLODGen/simple_tabs_20261103/report/index.json`: SimpleAdvancedPanels, WindowJSONAPI, DefaultAssetAndDrop, ScreenSizeValues, KoreanTooltips 5개 성공, 실패/경고 0. 두 탭의 공유 설정, 모드 전환 전후 전체 JSON 일치, 리듀스율 60%↔잔존 비율 40%, Any의 버텍스 비율, 화면 크기 직접 입력/자동 해제, Windows 재정의 유지, 잘못된 입력과 알 수 없는 탭 거부, 사용자 원본 보존을 확인했다. 테스트 PID 39248 종료 확인. 이후 표시 정밀도만 보완했다.
- 표시 보완 후 `Saved/RX_SMLODGen/simple_tabs_final_20261103/report/index.json`의 SimpleAdvancedPanels/WindowJSONAPI 2개 성공, 실패/경고 0과 PID 37728 종료를 확인했다. 캡처 `Saved/RX_SMLODGen/Previews/simple_panel_B4F95C5B401EEDFDE419D8A672EBCA9F.png`에서 60.0/30.0으로 정리된 입력 표시와 요청한 패널 구성을 확인했다. UE 5.8.4 CL 58902052 Win64 Development Editor 모듈 빌드 성공. 최종 DLL SHA-256: `DC230BEB9224F580DA11AE2EB8F6B3AD49B131E54EEF5FF37B84577142BC7E48`.
- 재시작 전 PID 36444의 dirty 패키지/PIE 없음과 RX 설정 백업을 확인하고 정상 종료했다. 같은 프로젝트의 최종 사용자용 PID 28088에서 기존 전체 설정 JSON이 같은 것을 확인하고 강제 LOD 선택을 복원했다. `Saved/RX_SMLODGen/simple_tabs_final_launch_20261103/restored.json`에 기록했으며 dirty 패키지/PIE는 없다. 최종 창은 심플 탭으로 열어 둔다. 실제 탭 캡처는 `Saved/RX_SMLODGen/Previews/simple_live_20261103.png`, `advanced_live_20261103.png`다. 원본 Apply/저장과 Computer Use는 사용하지 않았다.

## 2026-11-03 선택형 보호 리뷰 수정

- 단독 소스 리뷰에서 `FAttrMeshSimplification`의 `MeshBoundaryConstraint`, `GroupBoundaryConstraint`, `MaterialBoundaryConstraint`가 초기화되지 않은 것을 확인했다. 설치된 엔진의 `MeshSimplification.cpp`는 에지 축소 성공 후 이 값을 다시 사용한다. RX에서 열린 경계/머티리얼 경계 선택을 명시적으로 설정하고, 선택 대상이 아닌 폴리곤 그룹 경계는 NoConstraint로 초기화했다. 예기치 않은 제약 재적용 가능성을 제거한 수정이며 독립 리뷰를 수행했다고 주장하지 않는다.
- UV/노멀 분할이 없는 평면에서도 머티리얼만 다른 두 섹션의 경계를 유지하는 회귀 검사를 보강했다. 새 세 옵션, 기본값 모두 false, v1~3 이관, 분리 섹션의 ID 정리와 원본 Group ID 보존도 검토했다.
- `Saved/RX_SMLODGen/selection_review_20261103/report/index.json`: 관련 7개 성공, 실패/경고 0. AttributeBoundaryProtection, SectionReduction, SilhouettePreservation, SilhouetteSelections, WeightedPreview, SessionJSON, WeightMetadata를 실행했다. 이전 전체 21개 성공 자료는 아래 기록을 유지하며, 이번 초기화 수정 후 전체 21개를 다시 실행했다고 주장하지 않는다. 테스트 PID 39576 종료 확인.
- UE 5.8.4 CL 58902052 Win64 Development Editor 모듈 빌드 성공. DLL SHA-256: `3EB33CB1D74C21AB6391BB86C18CC685D54902B27E29D9AF51B45AA71EC33A49`. 원래 PID 14604의 미저장 패키지/PIE 없음과 RX 설정 백업을 확인한 뒤 정상 종료했고 `LogExit: Exiting`을 확인했다. 복구 정보는 `Saved/RX_SMLODGen/selection_review_20261103/before_restart.json`이다. 원본 Apply/저장 및 Computer Use는 사용하지 않았다.
- 같은 프로젝트를 사용자 검토용 PID 36444로 다시 열고, 백업한 RX 설정과 강제 표시 LOD를 복원했다. 전체 설정 JSON의 일치와 dirty 패키지/PIE 없음, 전역 최상위 false·에디터 소유 관계 true를 확인했다. 증거는 `Saved/RX_SMLODGen/selection_review_launch_20261103/restored.json`이며 이 에디터는 사용자 검토용으로 열어 둔다.

## 2026-11-03 선택형 외곽 보호

- 섹션 가중치 아래의 통합 체크를 `열린 외곽 경계 보존`, `꺾인 모서리 보존`, `UV·노멀·머티리얼 경계 보존`으로 나눴다. 사용자의 최종 지시에 따라 새 설정의 기본값은 세 옵션 모두 false다. LOD 0에서는 세 항목을 체크 해제 상태로 비활성화하며 원본 보존은 별도의 감소 제외 표시로 유지한다.
- 세 보호 종류는 독립적으로 제약을 구성한다. 열린 경계 옵션은 한 면에만 연결된 가장자리를 고정한다. 꺾인 모서리 옵션은 두 면의 비평면 모서리를 고정한다. 속성 경계 옵션은 내부 UV·노멀 분할선과 머티리얼 섹션 경계를 고정한다. 선택하지 않은 열린 경계가 전역 seam 설정 때문에 암묵적으로 고정되지 않도록 감소기의 전역 seam 잠금을 사용하지 않고 선택된 에지와 정점만 명시적으로 제약한다.
- 기존 가져오기/베이크 LOD와 Nanite는 창 열기만으로 원본 지오메트리를 다시 생성하거나 기존 동작을 거부하지 않도록 보호 감소를 자동 활성화하지 않는다. 신규/기존 LOD 모두 필요한 옵션을 명시적으로 선택한다. 기존 JSON/메타데이터에 명시적으로 저장한 선택은 기본값 변경으로 덮어쓰지 않는다.
- JSON/에셋 메타데이터 버전은 4다. `preserve_open_boundary`, `preserve_sharp_edges`, `preserve_attribute_seams` 세 필수 Boolean을 보관한다. 기존 v3의 `preserve_silhouette` true/false는 세 옵션 모두 true/false로 복원한다. 플래그가 없는 v1·2 저장본은 모두 false로 가져온다. UI·미리보기·원본 적용·메타데이터 복원은 같은 선택을 사용한다. [JSON_API.md](JSON_API.md)에 현재 형식을 기록했다.
- 최초 `Saved/RX_SMLODGen/selections_20261103/report/index.json`: 21개 중 20개 성공/1개 실패. 서로 다른 가중치의 섹션을 분리한 후 정점 ID에 빈 구간이 남으면 DynamicMesh 변환에서 삼각형이 누락되는 문제를 확인했다. 변환 전에 MeshDescription ID를 정리하고, Polygon Group→원본 Group ID 매핑을 전달해 섹션 식별자는 보존하도록 수정했다. 원본 실패 자료는 보존한다.
- 그 수정 후 `Saved/RX_SMLODGen/selections_fixed_20261103/report/index.json`의 관련 5개가 성공했다. 이후 사용자의 최종 지시로 기본값을 모두 false로 변경하고 `Saved/RX_SMLODGen/selections_all_off_20261103/report/index.json`에서 전체 21개 성공, 실패/경고 0을 확인했다. 공통 JSON 직렬화의 8가지 선택 조합, 메타데이터 왕복, v3 통합 플래그 이관, 열린 경계 정점 고정, UV 경계만 켠 경우, 비평면 꼭짓점 고정, 가져온 LOD 소스 유지와 기존 적용·임포스터·창/JSON 동작을 포함한다. 테스트 PID 36040, 29048, 28560 종료를 확인했다.
- UE 5.8.4 CL 58902052 Win64 Development Editor 모듈 빌드 성공. 최종 DLL SHA-256: `B0EBBDD0C58457B88D79C202864A6D8D7EDC10DE4AE4E966ED6416EF7A1E9EBC`. 재시작 전 PID 38276의 dirty 패키지/PIE가 없고 RX 편집 세션이 닫힌 것을 확인했다. 보존할 활성 RX 설정이 없어 이전 bench 설정을 자동으로 다시 열거나 덮어쓰지 않았다. 전제 조건은 `Saved/RX_SMLODGen/before_selections_editor_state.json`에 기록했다. 원본 Apply/저장 및 Computer Use는 사용하지 않았다.
- 같은 프로젝트를 사용자 검토용 PID 14604로 다시 열고 기본 박스의 임시 LOD 1에서 세 체크가 모두 꺼진 것을 확인했다. `Saved/RX_SMLODGen/selections_launch_20261103/all_off_live.json`은 v4와 세 false 값, 메인 창 소유 관계, 전역 최상위 false 및 dirty 패키지/PIE 없음을 기록한다. `Saved/RX_SMLODGen/Previews/selections_all_off_20261103.png`에서 실제 UI도 확인했다. 사용자 검토용 에디터와 임시 미리보기만 열어 둔다.

## 2026-11-03 리뷰 수정 및 외곽 실루엣 보존 (이전 통합 옵션)

- 아래 전역 최상위 창 설정은 사용자의 후속 요구에 따라 폐기했다. 현재 RX 창은 시스템 전역 최상위가 아니며 언리얼 메인 창의 네이티브 소유 창으로만 유지한다. 다른 프로그램을 가리지 않는다. `get.window_topmost`는 false, `window_owned_by_editor`는 true다.
- 각 감소 LOD의 섹션 가중치 바로 아래에 `외곽 실루엣 보존` 체크와 한국어 툴팁을 추가했다. 기본값은 false이며 LOD 0은 원본 보존 상태로 비활성화한다. 가중치 0에서도 독립적으로 작동한다. 기존 `SilhouetteImportance` 값만 바꾸는 방식은 설치된 Quadric 감소기에서 해당 값이 사용되지 않아 채택하지 않았다.
- 표준 GeometryProcessing의 속성 기반 QEM 감소기를 사용한다. 열린 경계, 비평면 모서리, UV/노멀·머티리얼 경계의 정점/에지를 고정하고 평면 내부를 줄인다. 비평면 판단의 법선 내적 오차는 1e-8이다. 곡면이나 경계가 많은 경우 목표보다 삼각형이 많이 남거나 감소하지 않을 수 있다. 섹션의 원본 Group ID, 머티리얼 슬롯 이름과 선형 버텍스 컬러를 보존한다. 결합 한계치는 0이어야 하며 서로 다른 가중치/실루엣 옵션의 맞닿은 섹션은 기존 보수적 제약에 따라 거부한다. Epic 소스 수정이나 비공개 감소기 헤더 포함은 하지 않았다.
- JSON과 에셋 메타데이터 버전을 3으로 올려 섹션별 `preserve_silhouette` Boolean을 보관한다. 버전 1·2를 계속 읽고 없는 플래그는 false로 가져온다. 원본 적용 시에도 이 옵션을 저장하고 편집 세션을 다시 열면 복원한다. 생성된 메시에는 표준 MeshDescription/StaticMesh 데이터만 기록하며 새 런타임 클래스를 만들지 않았다. GeometryProcessing은 에디터 도구 의존성으로 명시했다.
- 단독 소스 리뷰에서 잘못된 asset JSON 타입이 기본 박스 열기로 처리되는 경로와, 거부한 비 Static Mesh 드롭을 로드하던 경로를 수정했다. 새 실루엣 플래그는 문자열 "true"가 아니라 실제 Boolean만 받도록 검증한다. 스키마 및 통계·드롭·창 우선순위 변경도 함께 검토했다. 별도 독립 리뷰를 수행했다고 주장하지 않으며 cook/package 검증은 이번 범위에 포함하지 않는다.
- 최초 전체 실행 `Saved/RX_SMLODGen/review_silhouette_20261103/report/index.json`은 18개 중 16개 성공/2개 실패였다. JSON의 문자열→Boolean 자동 변환 문제를 수정했다. 평면 격자에서 경계 32개를 고정해도 삼각형 32개 목표에 도달할 수 있는데 더 많이 남아야 한다고 가정한 검증 기준도 수정했다. 외곽 정점·비평면 꼭짓점 고정, 섹션 ID/이름 및 선형 컬러 검사는 이 실행에서도 통과했다. 원본 실패 자료는 보존한다.
- 수정 후 `Saved/RX_SMLODGen/review_silhouette_fixed_20261103/report/index.json`의 SessionJSON, SilhouettePreservation, WeightMetadata 3개 성공/실패·경고 0을 확인했다. 전체 실행의 기존 통과 항목과 합쳐 18개 고유 검사의 성공을 확인했으며 마지막 빌드로 전체 18개를 다시 실행했다고 주장하지 않는다. 테스트 PID 34064와 21008은 모두 종료됐다. UE 5.8.4 CL 58902052 Win64 Development Editor 모듈 빌드 성공. 최종 DLL SHA-256: `B983429E6DB5B2646086550157676943AEE04779CFF0250C36092A3F37371FF0`.
- 실제 창에서 기본 박스의 가중치 0/실루엣 보존 true/삼각형 목표 50%를 설정하고 LOD 1의 36개 삼각형·48개 렌더 버텍스를 확인했다(LOD 0은 48개/54개). 목표 24개보다 보존 제약이 우선한 결과다. `Saved/RX_SMLODGen/Previews/silhouette_ui_review_20261103.png`에서 새 체크와 통계를 확인했고 잘못된 `open.asset: 42` 요청도 거부됐다.
- 기존 사용자 설정은 `recovery_review_P39048.json`에 보존한 뒤 메인 PID 39048을 정상 종료했다. 같은 프로젝트의 사용자 검토용 PID 38276에서 `bench` 설정을 버전 3으로 이관·복원했고, 버전/새 기본 플래그를 제외한 전체 설정의 일치를 확인했다. `Saved/RX_SMLODGen/review_launch_20261103/native_window.json`의 Win32 전역 최상위 비트는 false이며 소유 창은 같은 PID의 언리얼 메인 창이다. `restored_live.json`의 dirty 패키지/PIE는 없으며 사용자 원본 Apply/저장은 호출하지 않았다. 이 사용자 검토용 에디터만 열어 둔다. Computer Use는 사용하지 않았다.

## 2026-11-03 창 우선순위 (이전 전역 최상위 구현)

- RX 창에 `IsTopmostWindow(true)`를 지정하고 `FGlobalTabmanager`의 메인 창에 `AddWindowAsNativeChild`로 연결했다. 메인 에디터를 클릭해도 RX 창이 뒤로 내려가지 않게 하며 비모달 상태로 콘텐츠 브라우저의 드래그를 받는다. 메인 창이 없는 특수 실행은 소유 관계 없이 최상위 창으로 연다.
- 창 구조를 바꾼 뒤의 `Saved/RX_SMLODGen/topmost_20261103/report/index.json`: 관련 검사 3개 성공, 실패/경고 0. DefaultAssetAndDrop에 최상위 설정과 메인 창 소유 관계 검증을 추가했고, 자식 창에서의 드롭·통계 및 기존 WindowJSONAPI가 성공했다. 테스트 PID 36500 종료 확인. DLL SHA-256: `0DB9152D7EFBD04EF6FE30A0F4686E3E9F8C8644A79145DD15A5EE765E0246FF`.
- 메시 교체 후 통계 텍스트의 앞부분이 잘리는 캡처를 발견했다. 고정 너비/왼쪽 정렬만으로는 해소되지 않아 갱신되는 통계 텍스트에 `SimpleTextMode(true)`를 적용했다. 통계 영역은 320 Slate 단위이며 섹션이 많으면 세로로 스크롤한다. 한국어·수치·여러 줄 표시를 실제 벤치 캡처에서 확인했다.
- 재시작 전에 최신 `bench` 세션을 `recovery_topmost_P30180.json`과 `before_topmost_state.json`에 보존했다. 이전 통계 검증 후 창의 대상이 기본 박스에서 bench로 바뀐 것을 관찰했으므로 그 당시의 1개 LOD 세션을 유지하며 이전 3개 LOD 복구본으로 덮어쓰지 않았다. 이전 복구 JSON도 보존한다. dirty 패키지/PIE가 없는 것을 확인하고 PID 30180을 정상 종료했다.
- 같은 프로젝트를 사용자 검토용 PID 28612로 다시 열어 직전 설정을 복원했고 전체 설정 JSON이 일치하는 것을 확인했다. `Saved/RX_SMLODGen/topmost_launch_20261103/native_window.json`에서 Win32 `WS_EX_TOPMOST`와 소유 창의 PID/제목이 언리얼 메인 창인 것을 별도로 확인했다. `current_live.json`의 dirty 패키지/PIE는 없으며 원본 Apply/저장은 호출하지 않았다. 캡처는 `Saved/RX_SMLODGen/Previews/bench_topmost_stats_20261103_0229.png`이고 사용자 검토용 에디터와 RX 창은 열어 둔다.
- 마지막 텍스트 표시 보완 후 모듈 빌드 성공. 최종 DLL SHA-256: `275B415F7FCDC884C6709E7B40E8649CD81537F1C5EDD60CC77D78B0284F6BAF`. `recovery_statstext_P28612.json`으로 최신 설정을 보존하고 PID 28612의 `LogExit: Exiting`을 확인한 뒤 같은 프로젝트를 PID 39048로 다시 열었다. 전체 설정 JSON 복원 일치, 최상위/소유 창 플래그, dirty 패키지/PIE 없음을 확인했다. 최종 화면 `Saved/RX_SMLODGen/Previews/bench_statstext_20261103_0232.png`에서 현재 LOD·스크린 크기·전체/섹션 통계 문구가 잘리지 않는 것을 확인했다. 이 마지막 텍스트 렌더링 변경 후 자동화 3개를 다시 돌린 것은 아니며 실제 창으로 검증했다. 사용자 검토용 PID 39048만 열어 둔다.

## 2026-11-03 뷰포트 통계와 기본 박스·메시 드롭

- 뷰포트 왼쪽 위에 현재 LOD(자동/고정), 현재 카메라의 스크린 크기, 폴리곤(삼각형)·렌더 버텍스 수와 섹션별 수치를 표시한다. `FEditorViewportClient::DrawCanvas`의 실제 `FSceneView`를 사용하며 설치된 Static Mesh Editor와 같은 `ComputeBoundsScreenSize` / `ComputeStaticMeshLOD` 계산을 따른다. 전환 임계값과 현재 투영 크기를 구분한다. Nanite 메시에서는 기본 LOD 지오메트리 통계임을 표시한다.
- 섹션 헤더에도 폴리곤·버텍스 수를 추가했다. 각 섹션의 인덱스 버퍼에서 실제로 참조하는 고유 렌더 버텍스를 센다. UV/노멀 분할을 포함하며 섹션 간 공유 버텍스는 각 섹션에 포함된다. 통계는 메시 교체/재빌드 때 계산해 캐시한다. 많은 섹션은 통계 영역을 스크롤해서 확인할 수 있다.
- 플러그인의 `CanContainContent`를 활성화하고 [SM_DefaultBox.uasset](../../../Plugins/RX_SMLODGen/Content/Meshes/SM_DefaultBox.uasset)를 추가했다. 엔진 기본 Cube를 복제한 100cm 정육면체이며 경로는 `/RX_SMLODGen/Meshes/SM_DefaultBox.SM_DefaultBox`다. 선택한 메시/단일 메시 액터가 없으면 이 박스로 창을 연다. `open` API의 `asset`도 생략할 수 있다.
- 콘텐츠 브라우저에서 스태틱 메시 하나를 뷰포트로 드롭하면 새 임시 사본을 초기화한 뒤 기존 창의 대상을 교체하고 새 바운드에 카메라를 맞춘다. 통계 오버레이 위에 드롭해도 뷰포트로 전달하며 디테일 패널 영역의 드롭은 처리하지 않는다. 새 메시 초기화 또는 복구 JSON 저장 실패 시 기존 세션을 유지한다. 교체 전 설정은 `Saved/RX_SMLODGen/Presets/autoswap_<GUID>.json`으로 보존한다. 원본 적용은 기존처럼 명시적인 버튼/API로만 수행한다.
- JSON API에 `load_asset`과 `get.viewport.current_lod/current_screen_size/triangles/vertices/sections/statistics_text`를 추가했다. 자세한 의미와 복원 순서는 [JSON_API.md](JSON_API.md)를 따른다.
- UE 5.8.4 CL 58902052 Win64 Development Editor의 RX_SMLODGen 모듈 빌드 성공. 이 단계 안내/툴팁 문구를 포함한 DLL SHA-256: `A87E9169967488C5FC379EB229D6CE4A1C898E9CAC81FEEC7CB496DA44BF9BE6`. 이후 창 우선순위 빌드는 위 항목을 따른다.
- `Saved/RX_SMLODGen/stats_drop_final_20261103/report/index.json`: DefaultAssetAndDrop, SectionStatistics, WindowJSONAPI 3개 성공, 실패/경고 0. 기본 박스의 별도 프로세스 재로드, 선택 없는 open, 실제 Slate asset drag/drop 이벤트, 무효 드롭 시 기존 세션 유지, 미저장 설정의 자동 복구 JSON, 원본 보존, 강제 LOD와 전체/섹션 통계 일치 및 기존 JSON 동작을 검사했다. 테스트 PID 33456은 테스트 큐 완료 후 엔진의 TestExit 상태 0으로 종료됐다. 전체 기능 검사를 다시 수행한 것은 아니다. 이후 안내/툴팁 문자열만 정리하고 모듈을 다시 빌드했다.
- 최초 실행 `Saved/RX_SMLODGen/stats_drop_20261103/tests.log`에서는 DefaultAssetAndDrop은 성공했으나 합성 SectionStatistics 테스트의 스택 `FStaticMeshLODResources` 참조 카운트 때문에 검증 프로세스가 종료됐다. 테스트 픽스처를 `bAddRef=false`로 고친 뒤 위 후속 실행이 성공했다. 이 오류는 통계 집계 로직이나 사용자 메시에서 발생한 것이 아니다. 최초 로그와 캡처는 보존한다.
- Computer Use를 사용하지 않았다. 원래 PID 38716의 dirty 패키지/PIE 없음과 설정 백업 `recovery_before_stats_drop_20261103.json`을 확인한 뒤 RX 창과 메인 창을 정상 종료했다. 사용자의 실제 에셋에 Apply/저장을 호출하지 않았다. 독립 검토 및 cook/package 실행은 이번 변경의 검증 범위가 아니다.

## 2026-11-03 패널 정리와 화면 크기 직접 편집

- LOD별 빌드 세팅 카테고리와 해당 입력 위젯을 UI에서 제거했다. 기존 빌드 설정 값과 JSON 입출력은 유지한다.
- 화면 크기 입력의 자동 모드 비활성화를 제거했다. 직접 편집하면 입력을 검증하고 자동 계산을 끈 뒤 지정한 LOD의 기본값/플랫폼별 값을 기록한다. 다른 감소 LOD의 자동 계산값은 수동 설정으로 복사한다. LOD 0은 기존 엔진의 자동→수동 전환과 동일하게 1로 초기화한 뒤 편집 대상이면 입력값으로 덮어쓴다. 입력 후 미리보기 변경 내용 적용으로 표시 결과를 갱신한다.
- 카테고리 본문마다 Slate 단위 24px의 왼쪽 패딩을 넣어 하위 단계가 누적 들여쓰기 되게 했다. LOD별/섹션별 펼침 상태를 별도로 유지하고 화면 크기 입력 후 다음 틱에 패널을 갱신하여 편집 이벤트 중 위젯 트리를 교체하지 않는다.
- DLL SHA-256: `E76C086ACC102770653B7A49936003621706A0FC18C58790365A791346F17539`.
- `Saved/RX_SMLODGen/layout_screen_edit_20261103_015319/editor.log`: 변경에 관련된 ScreenSizeValues, WindowJSONAPI, KoreanTooltips 3개 성공. 자동 모드에서 직접 입력, 자동 계산 해제, 나머지 감소 LOD 값 보존, 재빌드 후 입력 유지, 잘못된 입력 거부와 기존 JSON 왕복을 포함한다. PID 39384 정상 종료와 `LogExit: Exiting` 확인. 전체 검사 16개를 다시 수행했다고 주장하지 않는다.
- `Saved/RX_SMLODGen/Previews/window_3BB9BF084111ABB2ECD37084484582A9.png`에서 단계별 들여쓰기, 빌드 세팅 숨김, 활성화된 화면 크기 입력을 확인했다.
- 사용자 세션의 설정을 `Saved/RX_SMLODGen/Presets/recovery_layout_P15200.json`으로 보존하고, dirty 패키지/PIE가 없는 상태에서 RX 창과 정확한 메인 에디터 PID 15200을 정상 종료했다. 강제 종료/Computer Use는 사용하지 않았다.
- 같은 프로젝트를 사용자용 PID 38716으로 다시 열어 백업의 `bench` 세션(일반 LOD 3개)을 복원하고 미리보기를 갱신했다. 복원 후 전체 설정 JSON이 백업과 같은 것을 확인했다. 진단 결과 dirty 패키지/PIE가 없으며 원본 적용은 호출하지 않았다. 이 에디터는 검토용으로 열어 둔다. 실제 메시 캡처: `Saved/RX_SMLODGen/Previews/bench_layout_20261103_0200.png`.

## 2026-11-03 보존 가중치·LOD 선택·화면 크기·위쪽 노말

- 가중치를 보존 강도로 반전했다. 0은 기본 감소, 1은 전체 보존이며 목표 잔존 비율은 `p + (1-p)*w`이다. 기본값은 0, 감소 제외는 전체 보존이다. JSON과 에셋 메타데이터의 본문 버전은 2이며, 버전 1을 읽을 때 `1-w`로 변환하여 이전 감소 결과를 유지한다. 기존 에셋에서 읽는 메타데이터 키 이름은 호환성을 위해 유지한다. 요청 API 버전은 1이다.
- 뷰포트 위에 Auto/LOD별 강제 표시 선택을 추가하고 오른쪽 LOD 선택과 동기화했다. API `view_lod`와 실제 컴포넌트의 강제 표시 번호도 검사한다.
- 기본 Static Mesh Editor의 `GetLODScreenSize`와 대조해 자동 계산 모드의 표시 오류를 수정했다. 자동 계산일 때는 `RenderData.ScreenSize`를 읽으며, 수동일 때는 소스 모델 값을 읽는다. 자동 계산을 끌 때는 현재 계산값을 수동 설정으로 복사한다. 전 LOD를 0.25로 초기화하거나 원본 값을 임의로 덮어쓰지 않는다. 화면 크기 위젯의 항목은 `화면 크기 디폴트`와 `플랫폼별 화면 크기`로 표시한다.
- 임포스터의 셰이딩 노말은 모든 버텍스에서 메시 로컬 +Z로 고정했다. 네 수직 평면의 배치와 삼각형 와인딩은 유지한다. 머티리얼은 tangent-space normal을 끄고 VertexNormalWS를 월드 공간 Normal에 연결하여 양면의 뒷면 반전을 방지하도록 구성한다. 기존 생성 머티리얼 재사용 시에도 해당 노말 연결을 갱신한다.
- DLL SHA-256: `EFB7E3F80F4FA54C3E87235FC47DAC1C6D94D6DE23B7C4339BB394B836ABC80C`.
- `Saved/RX_SMLODGen/weight_screen_up_20261103_013115/editor.log`: 기본 검사 15개 성공 및 PID 18208 정상 종료. 보존 가중치 0/0.5/1, 버전 1 JSON/메타데이터 변환, 자동 화면 크기와 렌더 데이터 일치, 수동 전환 시 값 유지, LOD별 1/0.63/0.17과 Windows 재정의의 JSON 복원, 뷰포트 강제 LOD 및 Auto 복귀, 메시/렌더 버퍼의 위쪽 노말과 머티리얼 연결을 포함한다.
- 현재 코드로 소유한 고유 테스트 에셋 `/Game/_MCP_Temp/RX_SMLODGen/CubelessTests/RXTest_136312144204B7D70388028940146985/`의 메시·머티리얼·아틀라스만 저장했다. `Saved/RX_SMLODGen/up_normal_reload_20261103_013250/editor.log`: 별도 에디터 프로세스 PID 10588의 Reload 검사 성공 및 정상 종료. 저장한 노말의 +Z 방향, 월드 공간 Normal 연결, 마지막 LOD와 아틀라스 픽셀 해시 및 메타데이터 복원을 확인했다. 두 실행을 합해 검사 16개가 성공했다.
- `Saved/RX_SMLODGen/Previews/window_22F36EEE48B4E093350E18BD101C58C2.png`에서 뷰포트 선택, 기본 가중치 0, 화면 크기 디폴트의 실제 계산값 0.75를 확인했다. 모든 에셋에서 0.75라는 의미는 아니며 엔진이 해당 입력에 계산한 값이다.
- 미저장 패키지/PIE가 없고 RX 세션이 닫힌 것을 확인한 뒤, 사용자가 승인한 정상 재시작 범위에서 정확한 메인 PID 28200에 `CloseMainWindow`를 요청했고 프로세스 종료를 확인했다. 강제 종료와 Computer Use는 사용하지 않았다. 같은 프로젝트를 PID 15200으로 다시 열고 `bench` RX 창을 표시했다. 실제 Python API에서 설정 버전 2/보존 의미와 해당 메시 LOD 0의 화면 크기 2.0을 확인했다. 진단 결과 dirty 패키지와 PIE/SIE는 없었고 원본 적용은 호출하지 않았다. 사용자 검토용 에디터는 열어 둔다.

## 2026-11-03 뷰포트 갱신 및 한국어 툴팁

- 기본 Static Mesh Editor의 클라이언트와 비교하여 RX에 미리보기 월드 Tick이 빠져 있음을 확인했다. 전용 `FRXLODViewportClient::Tick`에서 기본 클라이언트 갱신 후 전용 미리보기 월드만 갱신한다. 사용자의 메인 에디터 월드를 중복 갱신하지 않는다.
- Slate 오버레이가 포함된 RX 창의 뷰포트는 직접 백버퍼 경로 대신 Slate 렌더 타깃에 합성하도록 명시했다. RX 클라이언트의 기본 노출을 고정하고, 뷰포트 위에 고정 노출 체크와 EV100 조절을 추가했다. 프로젝트 전역 렌더링 설정이나 CVar는 변경하지 않는다.
- 파라미터 행, 카테고리, 섹션 가중치, 임포스터, JSON 및 원본/미리보기 적용 버튼에 한국어 설명을 추가했다. 엔진 구조체 입력은 이 창의 property handle tooltip override로 설명을 붙인다. 엔진 전역 reflection metadata를 수정하지 않는다.
- 최신 DLL SHA-256: `CF26AB93800F39EC5DF0477552623891EB7F21B5B0F5A007A197481866225958`.
- `Saved/RX_SMLODGen/flicker_tooltips_20261103_005433/editor.log`: 자동화 15개 성공. 새 KoreanTooltips 검사는 현재 엔진의 편집 가능한 빌드·감소·화면 크기·공통 설정에 한국어 설명이 있는지 확인한다. WindowJSONAPI에서는 창의 월드 시간/틱 진행과 고정 노출, 연속 8프레임 캡처를 검사한다. PID 32256 정상 종료 및 `LogExit: Exiting` 확인.
- 정지한 합성 큐브의 캡처 구간에서 월드 시간은 1.3842~2.7573초, 틱 수는 70~140으로 증가했다. 컨트롤·상태 텍스트·축 표시를 제외한 뷰포트 영역의 인접 프레임 RGB 절대 차이는 7쌍 모두 0이었다. 비교 자료: `Saved/RX_SMLODGen/Previews/window_8E9EA55B468CDB72FB603AB1FB2CD95D_frames_comparison.json`. 실제 사용자 모니터의 모든 상황이나 움직이는 머티리얼까지 검증한 결과는 아니다. 수정 전 연속 프레임을 확보하지 못했으므로 사용자 제보의 단일 원인을 확정한 것으로 해석하지 않는다.
- 같은 프로젝트를 사용자용 에디터 PID 28200으로 다시 열고 `bench`의 RX 창을 표시했다. 현재 `get.viewport`에서 월드 시간/틱 진행, 고정 노출 및 Slate 합성을 확인했다. 진단 결과 PIE/SIE와 dirty 패키지는 없었고 원본 적용은 호출하지 않았다. 사용자 검토용으로 열어 둔다. 캡처: `Saved/RX_SMLODGen/Previews/bench_stable_20261103_0100.png`.

## 2026-11-03 전용 UI 및 JSON

- 전체 Static Mesh Editor 재사용을 제거하고, 전용 Slate 창의 왼쪽에 미리보기 뷰포트, 오른쪽에 LOD 전용 패널을 구현했다. 원본 적용 버튼은 뷰포트 위에 있다.
- LOD Auto/강제 선택, 커스텀 LOD 표시 목록, LOD별 통계·섹션·화면 크기·빌드/감소 설정·제거, 공통 그룹/임포트/최소 LOD/스트리밍/LOD 개수/자동 거리 옵션을 제공한다. 속성 입력에는 엔진 PropertyEditor 위젯을 사용하지만 전체 Static Mesh 디테일 패널을 열지 않는다.
- 섹션 안에 머티리얼 슬롯, 강조/격리, 그림자·충돌·레이 트레이싱·디스턴스 필드·불투명 옵션과 그룹별 가중치/감소 제외를 배치했다. LOD 0의 가중치는 원본 보존 표시로 비활성화한다.
- 임포스터는 표시 메시의 실제 마지막 LOD로 빌드된다. 설정용 사본의 LOD 수와 분리하여 반복 생성 시 같은 마지막 인덱스와 재질 슬롯을 교체한다. 원본 재적용은 소유권 메타데이터가 있는 기존 아틀라스와 머티리얼 경로를 재사용한다. 미저장 변경이나 불명확한 소유권은 덮어쓰지 않는다.
- UI의 JSON 불러오기/저장/내보내기와 에디터 전용 `unreal.RXSMLODGenLibrary.execute_request`를 구현했다. 스키마·버전·에셋/지오메트리 식별자, 머티리얼 슬롯, LOD 빌드/감소/화면 크기, 공통 설정, 섹션 가중치/렌더링 플래그를 보관한다. 실제 지오메트리나 텍스처 픽셀을 JSON에 넣지는 않는다. 사용 순서와 필드는 [JSON_API.md](JSON_API.md)를 따른다.
- 현재 프로젝트에서 Win64 Development Editor 빌드 성공. DLL SHA-256: `C5CDABD9DE84935D44168665C177747A2EABED400FA07AA1A718AF93CCE96625`.
- `Saved/RX_SMLODGen/final_reload_20261103_003415/editor.log`: 자동화 14개 성공 및 PID 20952의 `LogExit: Exiting` 확인. SessionJSON, SessionImpostorReplacement, WindowJSONAPI와 기존 감소·캡처·적용 검사, RX.SMLODGenPersistence.Reload를 포함한다.
- 디스크 저장은 고유 `/Game/_MCP_Temp/RX_SMLODGen/CubelessTests/RXTest_9FC9B80D4F612AFF55DF5CA60D602816/`의 소유한 메시·머티리얼·텍스처만 사용했다. `Saved/RX_SMLODGen/persistence_manifest.txt`의 경로를 별도 프로세스에서 로드하여 LOD 수, 마지막 LOD 소스, 머티리얼 그래프와 텍스처 연결, 양면 마스크 설정, 아틀라스 픽셀 MD5, 메타데이터 복원을 확인했다. 테스트 에셋은 증거와 함께 로컬에 보존했다.
- LOD 개수 증가 후 UI 갱신에서 아직 없는 표시 LOD의 화면 크기를 읽어 실패했다. 설정용 LOD를 대체값으로 사용하는 수정 후 동일한 증가·감소 API 검사와 전체 검사가 성공했다. 최초 실패는 `Saved/RX_SMLODGen/persist_write_20261103_003139/editor.log`에 보존한다. 해당 실행의 저장 단계는 성공했지만 전체 실행은 실패였으며, 후속 실행과 구분한다.
- UI 캡처는 Slate `TakeScreenshot`으로 생성했다. Computer Use는 사용하지 않았다. `Saved/RX_SMLODGen/Previews/window_E2B64FE440A448F49F4A328B803E2158.png`에서 뷰포트·LOD 선택·인라인 가중치·종료 기준 등을 확인했다.
- 현재 프로젝트를 사용자용 에디터 PID 29152로 다시 열고 실제 `/Game/Levels/LevelPrototyping/Meshes/bench.bench`의 RX 창을 표시했다. Python에 노출된 `RXSMLODGenLibrary.execute_request`의 open/get/snapshot 호출이 성공했고 LOD 0의 14,721 삼각형/9,607 버텍스를 확인했다. 원본 적용을 호출하지 않았으며 진단 결과 dirty 패키지는 없었다. 이 에디터는 사용자가 검토할 세션으로 열어 둔다. 실제 메시의 캡처는 `Saved/RX_SMLODGen/Previews/bench_workspace_20261103_0038.png`에 있다.
- 기존 가중치 감소기의 제한은 유지한다: 비율 기반/LOD 0 기준, 서로 다른 가중치를 준 맞닿은 섹션 및 일부 감소 조건은 거부한다. 원본 변이의 독립 검토, MCP/플러그인을 끈 재로드·cook/package 실행, 다양한 실제 에셋 품질은 미실시다. FBX/OBJ LOD 임포트 입력과 모든 플랫폼별 설정 조합도 이번 자동화의 검증 범위가 아니다.

아래 내용은 이전 구현과 조사 기록이다. 현재 UI와 검증 상태는 위 항목을 따른다.

## 2026-11-02 현재 프로젝트에서의 개발

- 현재 `.uproject`에 Editor 전용 `RX_SMLODGen`을 등록하고 같은 프로젝트를 대상으로 Win64 Development Editor 모듈을 빌드했다. 기존 사용자 Editor에는 재시작 없이 새 모듈을 로드하지 않았다.
- 원본의 임시 복제본을 엔진 Static Mesh Editor에서 열고, 임시 메시 저장을 차단하는 검증기를 추가했다. 툴바에서 임포스터를 표시하고 다시 편집 메시로 바꾸는 경로를 추가했다. 가중치 창은 LOD별 그룹과 0–1 값, 감소 제외를 받으며 결과를 별도의 임시 표시 메시로 빌드한다. 사용자 화면에서 창 배치와 입력 조작은 아직 미검증이다.
- 가중치 감소는 현재 LOD 0을 기준으로 한 삼각형 비율 조건만 처리한다. Nanite, 다른 base LOD, 최대 삼각형 수/편차 조건은 거부한다. 모든 섹션의 가중치가 같으면 엔진의 전체 메시 감소기를 사용한다. 서로 다른 가중치를 적용할 때 같은 위치를 공유하는 섹션과 0보다 큰 용접 임계값은 경계 균열 위험 때문에 거부한다. 모든 섹션이 0이거나 제외되면 원본 지오메트리를 그대로 보존한다. 목표 삼각형 수는 감소기의 제약 때문에 정확히 달성된다고 보장하지 않는다.
- 임포스터 미리보기는 원본 바운드와 피벗을 따르는 고정 평면 4개, 반구형 노멀, 4개 타일 아틀라스와 여백, Base Color 및 알파 캡처, 임시 양면 마스크 머티리얼과 임시 메시를 생성한다. 비대칭 모델의 반대면은 같은 이미지를 재사용한다. 길쭉한 바운드의 대각 평면은 크기를 맞추기 위해 촬영 이미지 일부가 잘릴 수 있다. 투명·복잡한 Substrate 머티리얼, 모바일 및 그림자 품질은 미검증이다.
- `Saved/RX_SMLODGen/all_tests_20261102_current/editor.log`에 기록된 UE 5.8.4 D3D12 오프스크린 검사 8개가 성공했다: ImpostorAtlas, ImpostorCapture, ImpostorGeometry, ImpostorMaterial, ImpostorPreview, PreviewSession, SectionReduction, WeightedPreview. 큐브 및 합성 격자 입력에 대한 자동화 결과이며 실제 사용 에셋의 시각 품질이나 저장·재로드를 증명하지 않는다. 소유한 테스트 PID 35740은 종료했고 `LogExit: Exiting`을 확인했다.
- 이후 같은 가중치의 맞닿은 섹션에 엔진 전체 메시 감소기를 적용하는 경로를 추가했다. `Saved/RX_SMLODGen/uniform_weight_20261102/editor.log`의 SectionReduction 테스트가 성공했고 PID 15180의 정상 종료를 확인했다.
- 전체 메시 목표와 섹션별 목표를 구분하는 결과 표시를 추가했다. 당시 `Saved/RX_SMLODGen/result_labels_20261102/editor.log`의 SectionReduction 테스트가 성공했고 PID 33492의 정상 종료를 확인했다.
- `Saved/RX_SMLODGen/impostor_apply_retry_20261102/editor.log`에서 별도 테스트용 패키지에 임포스터 LOD와 두 에셋을 생성하고 미리보기 옵션을 복원하는 메모리 검사가 성공했다. 최초 실행은 테스트용 복제 메시가 컴파일 중이어서 실패했고 종료 중 엔진 ensure 및 배열 오류가 발생했다. 컴파일 완료를 기다리도록 테스트를 수정한 후 재실행했다.
- 생성 머티리얼 내부 객체의 transient 플래그를 제거한 후 `Saved/RX_SMLODGen/material_serialization_20261102/editor.log`에서 머티리얼 노드와 아틀라스 연결 검사 및 정상 종료를 확인했다. 이 검사는 실제 패키지를 저장하거나 재로드하지 않는다.
- 최초 감소 테스트는 미등록 출력 메시 속성 때문에 접근 위반이 발생했고 후속 수정 후 통과했다. 최초 RHI 캡처는 배경 검사 실패 및 종료 중 엔진 배열 오류가 발생했다. 같은 RHI의 비캡처 비교 실행은 정상 종료했고, 캡처 씬 정리 및 촬영 여백 수정 후 후속 캡처와 통합 실행이 정상 종료했다. 최초 실패 기록은 각각 `Saved/RX_SMLODGen/section_test_20261102/`과 `Saved/RX_SMLODGen/capture_test_20261102/`에 보존한다. 최초 종료 오류의 근본 원인은 확정하지 않았다.
- 원본 적용 버튼은 가중치 감소 지오메트리를 LOD 소스로 기록하고 기본 감소 비율을 100%로 설정한다. 요청한 설정과 섹션 가중치는 패키지 메타데이터에 보관하여 다음 미리보기에서 복원한다. 임포스터 선택 시 기존 LOD 뒤에 추가하고 별도의 텍스처·머티리얼 패키지를 생성한다. 자동 저장하지 않으며 실패 시 생성된 패키지가 남을 수 있다.
- `Saved/RX_SMLODGen/finalcheck_20261102/editor.log`에서 가중치 적용과 메타데이터 복원을 포함한 자동화 10개가 성공했고 테스트 프로세스의 정상 종료를 확인했다. 이는 메모리상의 합성 입력 검증이다. 실제 에셋의 저장·재로드, 시각적 품질, MCP 없이 패키징·실행 및 원본 변이의 독립 검토는 아직 미실시다.

## 2026-11-02 개발 및 검증 현황

아래는 현재 프로젝트 기준 개발 이전의 기록이다.

- `Plugins/RX_SMLODGen/`에 Editor 모듈과 Window 메뉴를 구현했다. 선택한 Static Mesh의 임시 복제본을 엔진의 Static Mesh Editor로 열도록 작성했으나, 실제 메뉴 호출·복제본 편집·닫기·원본 보호는 아직 시험하지 않았다. 원본 적용 버튼, 섹션 가중치 및 임포스터는 미구현이다. 기본 활성화는 꺼져 있다.
- UE 5.8.4 CL 58902052에서 최종 소스의 Win64 Development Editor `BuildPlugin` 컴파일·링크가 성공했다. 패키지 결과는 로컬 `%LOCALAPPDATA%/Temp/RXSMLODGenBuildEditor2_20261102/`에 있다. 원본 프로젝트의 플러그인 디렉터리에 실행 바이너리를 설치하거나 `.uproject`를 변경하지 않았다.
- 격리 프로젝트 `cubeless/workspaces/RXLODVerify/RXLODVerify.uproject`에서 빌드된 모듈 DLL 로딩을 로그로 확인했다. 엔진 기본 Window 메뉴와 Cube 에셋 로드도 확인했으나 메뉴 항목은 Python에서 protected 속성 접근이 거부되어 확인하지 못했다. 로그의 `LogAutomationTest: Error: Condition failed` 메시지의 원인은 확인하지 못했다. 로그는 `cubeless/workspaces/RXLODVerify/verify_editor_2.log`, 검증 결과는 `startup_result.json`에 있다.
- 일회성 검증 프로세스 PID 16012, 38420의 종료를 확인했다. 두 번째 실행 로그에는 `LogExit: Exiting`이 기록되어 있다. 최초 실행은 로그 경로 지정 오류로 종료 로그를 확보하지 못했다. 사용자의 기존 에디터는 건드리지 않았다.
- 고위험 원본 변이의 독립 검토, 저장·재로드 및 MCP 없이 cook/package와 실행은 모두 미실시다.

## 목표와 편집 경계

- 독립적인 Editor 모듈 플러그인 `Plugins/RX_SMLODGen/`을 사용한다. 게임 런타임 모듈이나 MCP 의존성을 만들지 않는다.
- 콘텐츠 브라우저의 단일 Static Mesh를 선택해 열고, 원본과 분리된 임시 복제본을 편집한다. 뷰포트, LOD 설정 및 선택 UI는 가능하면 엔진의 Static Mesh Editor 자체를 사용한다.
- 엔진 패널의 적용 버튼은 복제본만 변경한다. 별도의 원본 적용 버튼은 유효성 검사와 변경 내역 확인 후 복제본의 결과를 원본에 반영한다. 원본 저장은 명시적인 별도 동작으로 다루며 기존 dirty 상태를 보존한다.
- 임시 객체와 생성 중 텍스처는 원본 패키지를 더럽히지 않아야 한다. 취소·닫기·실패 후 잔여 패키지와 참조를 검사한다. 원본 변경은 실패 시 부분 변경 가능성을 보고하며 Undo를 완전한 롤백으로 보장하지 않는다.
- 원본이 미리보기 시작 이후 변경되었거나, 빌드 중이거나, 사용할 수 없는 LOD 소스가 있으면 적용을 막고 다시 미리보기하도록 한다. 기존 임포트 LOD, 충돌, 소켓, 머티리얼, Nanite, 플랫폼별 설정의 보존 범위를 검증한다.

## 섹션별 감소

- UI는 LOD별·섹션별 가중치 0–1과 감소 제외를 제공한다. 제외는 가중치 0과 같은 보존 동작이다. 감소 목표를 `r`(0–1), 가중치를 `w`라 하면 해당 섹션의 잔존 목표 비율은 `1 - r*w`이다. 예를 들어 50% 감소에 1이면 50% 잔존, 0이면 100% 잔존, 0.5면 75% 잔존이다.
- Unreal의 Percent Triangles는 **감소율이 아니라 잔존 비율**이다. 가중치 계산은 이 차이를 UI와 구현에서 명확히 처리한다. 이 공식은 삼각형 비율 기준에 적용한다. 최대 삼각형 수, 정점 기준 및 최대 편차 등 다른 종료 조건과의 결합은 별도 의미 정의와 검증이 필요하며, 임의로 동일한 보장을 표시하지 않는다.
- 엔진 감소 인터페이스에는 섹션별 가중치 매개변수가 없으므로 Polygon Group별 분리·감소·병합 등의 경로를 실험해야 한다. 분리 감소는 공유 경계의 균열과 노멀/UV 이음새를 만들 수 있다. 경계 보존, 재결합, 재질 슬롯 매핑과 제한된 삼각형 수 때문에 실제 결과가 목표와 다를 수 있으므로 각 섹션의 목표와 실제 수치를 표시한다. 품질을 보장할 수 없는 경우 적용을 막는다.
- 섹션의 정체성은 LOD/Polygon Group 및 재질 슬롯을 기준으로 추적하고, 동일 머티리얼을 쓰는 여러 섹션을 하나로 합치지 않는다. 소스 모델 및 재임포트 이후의 매핑 변화는 재검사한다.

## 임포스터

- 선택 시 마지막 LOD에 고정 교차 평면 4장(0°, 45°, 90°, 135°)을 넣고 양면 머티리얼을 사용한다. 카메라를 따라 회전하는 빌보드는 사용하지 않는다.
- 원본 로컬 바운드와 피벗을 기준으로 중심·높이·방향별 폭을 계산하고 미리보기에서 원본과 겹쳐 확인한다. 평면의 노멀은 수직으로만 서 있는 쿼드의 기본 노멀이 아니라 지정한 반구형 분포로 생성하고, 양면 셰이딩 및 탄젠트 공간에서 확인한다. 평면 메시로 원본의 모든 축 바운드를 정확히 재현할 수 있는지는 대상별 검증이 필요하다.
- 독립된 프리뷰 씬에서 4방향을 직교 투영으로 캡처하고 알파와 아틀라스를 만든다. 양면 평면 4장에는 총 8개 면 방향이 있으므로 비대칭 에셋의 반대편은 4장의 이미지 재사용만으로 정확하게 표현할 수 없다. 기본 4방향 모드에서는 반대면 재사용의 한계를 표시하고, 필요 시 8방향 캡처 선택지를 제공하는 것을 권장한다. 수평 회전·UV 뒤집기와 앞뒤 이미지 연결은 실제 미리보기로 검증한다.
- 색상/알파뿐 아니라 라이팅, 노멀, 마스크 경계, 밉맵, 투명 머티리얼, 그림자 및 여러 재질의 캡처 결과를 확인해야 한다. 표준 엔진 텍스처와 머티리얼만 저장하고 플러그인 런타임 클래스 참조는 남기지 않는다. 생성 에셋의 경로·이름 충돌은 사전에 확인한다.

## 검증 기준

1. 단일 및 다중 섹션의 미리보기에서 원본 디스크·메모리 상태가 보존되는지 확인한다. 0, 0.5, 1 가중치와 제외를 시험하고 실제 삼각형 수 및 경계 품질을 확인한다.
2. 엔진의 각 LOD 설정, 가져온 LOD, 화면 크기, 재질·소켓·충돌 및 Nanite에 대해 유지 또는 명시적인 차단을 확인한다. 중간에 원본을 변경한 경우 적용이 차단되는지 시험한다.
3. 임포스터의 피벗·크기·네 방향 및 반대 방향, 노멀, 알파와 거리 전환을 비대칭 메시에서 확인한다. 생성 에셋의 저장·재로드와 실패 시 잔여물을 검사한다.
4. 원본 적용 후 저장·재로드를 확인하고, 생성 결과의 직렬화 참조를 검사한다. 편집 플러그인과 MCP를 끈 상태에서 cook/package 및 실행을 확인한다. 독립 검토가 필요한 공유 변경은 별도로 검토받는다.

## 조사 근거

- 설치된 엔진: UE 5.8.4, CL 58902052 (`Engine/Build/Build.version`).
- `Engine/Source/Editor/StaticMeshEditor/Private/StaticMeshEditorTools.h`: `FStaticMeshDetails`, `FLevelOfDetailSettingsLayout`, `FMeshReductionSettingsLayout`은 비공개 구현이다.
- `Engine/Source/Editor/StaticMeshEditor/Public/StaticMeshEditorModule.h`: `CreateStaticMeshEditor`와 툴바 확장 지점을 제공한다. 임시 복제본에 대한 동작과 수명은 아직 검증하지 않았다.
- `Engine/Source/Editor/StaticMeshEditor/Public/StaticMeshEditorSubsystem.h`: LOD 설정 및 다른 Static Mesh의 LOD 복사 API를 제공한다. 호출 부작용과 모든 설정의 보존은 아직 검증하지 않았다.
- `Engine/Source/Developer/MeshReductionInterface/Public/IMeshReductionInterfaces.h`: `ReduceMeshDescription`은 메시와 전체 감소 설정을 받으며 섹션별 가중치 인자가 없다.
- `Engine/Source/Runtime/Engine/Public/MeshReductionSettings.h`와 `StaticMeshEditor/Private/StaticMeshEditorTools.cpp`: Percent Triangles의 내부 값은 잔존 비율이다.
