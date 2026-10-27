# 공용 프로젝트 설정

공용 Engine 설정은 UE의 표준 플랫폼 계층인 `Windows/WindowsEngine.ini`, `Android/AndroidEngine.ini`, `IOS/IOSEngine.ini`, `Linux/LinuxEngine.ini`, `Mac/MacEngine.ini`에서 관리합니다. 에디터와 UBT/UAT가 같은 설정 계층을 사용합니다. WindowsEditor 등은 해당 부모 플랫폼 설정을 상속합니다.

Android의 방향은 `Portrait`이며 iOS는 정방향 세로만 허용합니다. 기존 PC 렌더러·기본 맵 설정은 유지합니다. 모바일 기기에서의 성능과 패키징은 별도 검증 대상입니다.

UE의 Android File Server는 첫 실행 시 인증값을 `Config/DefaultEngine.ini`에 자동 기록합니다. 이 파일은 Git에서 제외하며 기존 로컬 파일은 삭제하지 않습니다. 공용 플랫폼 파일에는 `SecurityToken`을 넣지 마십시오. 개인 설정인 `User*.ini`, `DefaultEditorPerProjectUserSettings.ini`와 `Saved/`도 공유하지 않습니다.

프로젝트 설정 UI는 `DefaultEngine.ini`에 기록할 수 있습니다. 공용 설정을 변경할 때는 인증값을 제외한 해당 항목만 플랫폼 파일로 반영하십시오. 모든 플랫폼에 적용할 변경은 다섯 파일을 함께 갱신하고, 특정 플랫폼 전용 변경은 해당 파일에서만 관리합니다. 같은 항목이 있으면 플랫폼 계층의 값이 `DefaultEngine.ini`보다 우선합니다. 새 플랫폼을 지원할 때는 해당 플랫폼의 공용 Engine 파일도 추가하고 엔진과 빌드 도구에서 로드를 확인하십시오.

2026-10-27 검증: UE 5.8.3 UBT의 실제 설정 로더에서 다섯 플랫폼의 기본 맵과 Android/iOS 방향 값을 확인했습니다. 로컬 `DefaultEngine.ini`가 없는 게시 대상 검증본을 엔진으로 실행하여 인증값 자동 생성 및 Git 제외, 반복 실행 후 공용 설정 불변을 확인했습니다. 기본 ThirdPerson·Crimson Dawn 맵과 게임 Blueprint 3개의 로드도 통과했습니다. 에디터 플러그인은 같은 소스로 앞서 검증한 바이너리를 재사용했으며 이번 변경으로 C++을 다시 빌드하지 않았습니다. 모바일 패키징·기기 실행은 수행하지 않았습니다.

## Game Animation Sample 통합

2026-10-28에 기존 `UnrealMCP5_8_3.uproject`에 샘플의 애니메이션·PoseSearch·Chooser·Mover 등 필수 엔진 플러그인을 병합했습니다. 별도 `GameAnimationSample.uproject`는 로딩·공식 MCP 확인 뒤 삭제했으며 사용하지 않습니다. 기존 프로젝트 ID, 기본 ThirdPerson 맵·게임 모드, 모바일 세로 방향과 기본 터치 인터페이스는 유지합니다.

샘플은 `/Game/Levels/DefaultLevel`을 직접 열어 사용합니다. 이 맵의 `GM_Sandbox` 오버라이드는 기존 전역 게임 모드와 별개입니다. CMC·Mover 캐릭터와 각 AnimBP, 기존 Crimson Dawn 핵심 클래스의 로딩을 UE 5.8.4에서 확인했습니다. 샘플 모바일 터치 조작·게임플레이·패키징의 검증 완료를 뜻하지 않습니다.

공용 플랫폼 INI에는 샘플의 스키닝·물리·충돌 채널·Data Driven CVar·PoseSearch 설정을 추가했습니다. `DefaultGameplayTags.ini`와 `DefaultNetworkPrediction.ini`는 샘플 기능에 사용합니다. `DefaultInput.ini`에는 충돌하지 않는 FlyCam 축 입력만 추가하고 기존 터치 설정을 보존했습니다. 샘플의 전역 cook 맵 제한과 프로젝트 에셋 참조 제한은 적용하지 않았습니다.

`DefaultDeviceProfiles.ini`의 샘플 텍스처 품질은 `GameAnimationSample_Android` / `GameAnimationSample_IOS` 선택형 프로파일입니다. 기존 게임의 Android·iOS 기본 품질에 자동 적용하지 않습니다.

### 렌더러 정책과 모바일 프로파일 등록

렌더러의 공용 원본은 다섯 플랫폼 INI의 `[/Script/Engine.RendererSettings]`입니다. 로컬에만 있던 18개 값을 현재 유효값 그대로 공용 설정에 통합했고, 로컬 `DefaultEngine.ini`의 중복 렌더러 섹션은 제거했습니다. 기존 플랫폼 값이 우선하는 항목도 그대로 유지합니다. 인증값 등 로컬의 다른 섹션은 변경하지 않았습니다. 스킨 캐시는 기존의 `r.SkinCache.DefaultBehavior=0`(메시별 opt-in), 기본 모션 블러는 기존의 `r.DefaultFeature.MotionBlur=False`를 명시적으로 유지합니다.

두 선택형 모바일 프로파일은 `[DeviceProfiles]`의 `DeviceProfileNameAndTypes`에 이름·타입을 등록합니다. `GameAnimationSample_Android`는 Android 타입/부모, `GameAnimationSample_IOS`는 IOS 타입/부모를 사용하며, 등록 자체로 기본 프로파일을 바꾸지 않습니다. 이미 실행 중인 메인 에디터는 보존했으므로 새 목록은 다음 재시작 시 반영됩니다.

2026-10-28 최소 검증: 로컬 `DefaultEngine.ini` 없이 시작한 UE 5.8.4의 설정 전용 프로세스에서 두 렌더러 값과 두 프로파일의 실제 로드·타입·부모를 확인했습니다. 활성 프로파일은 WindowsEditor였습니다. 이 검증은 NullRHI 설정 읽기이며 새 화면·모바일 기기·패키징 검증을 뜻하지 않습니다. 메인 MCP에서도 기존 두 값과 dirty 0을 확인했습니다.
