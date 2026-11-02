# RX_SMLODGen — 구현 설계 및 조사

상태: UE 5.8.4에서 RX 전용 LOD 편집 창과 JSON 인터페이스를 구현했다. 합성 테스트 에셋의 적용·저장·별도 프로세스 재로드를 확인했다. 실제 사용 에셋의 시각 품질과 독립 검토, cook/package 실행은 아직 미검증이다.

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
