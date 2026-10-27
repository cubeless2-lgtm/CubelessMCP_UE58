# 공용 프로젝트 설정

공용 Engine 설정은 UE의 표준 플랫폼 계층인 `Windows/WindowsEngine.ini`, `Android/AndroidEngine.ini`, `IOS/IOSEngine.ini`, `Linux/LinuxEngine.ini`, `Mac/MacEngine.ini`에서 관리합니다. 에디터와 UBT/UAT가 같은 설정 계층을 사용합니다. WindowsEditor 등은 해당 부모 플랫폼 설정을 상속합니다.

Android의 방향은 `Portrait`이며 iOS는 정방향 세로만 허용합니다. 기존 PC 렌더러·기본 맵 설정은 유지합니다. 모바일 기기에서의 성능과 패키징은 별도 검증 대상입니다.

UE의 Android File Server는 첫 실행 시 인증값을 `Config/DefaultEngine.ini`에 자동 기록합니다. 이 파일은 Git에서 제외하며 기존 로컬 파일은 삭제하지 않습니다. 공용 플랫폼 파일에는 `SecurityToken`을 넣지 마십시오. 개인 설정인 `User*.ini`, `DefaultEditorPerProjectUserSettings.ini`와 `Saved/`도 공유하지 않습니다.

프로젝트 설정 UI는 `DefaultEngine.ini`에 기록할 수 있습니다. 공용 설정을 변경할 때는 인증값을 제외한 해당 항목만 플랫폼 파일로 반영하십시오. 모든 플랫폼에 적용할 변경은 다섯 파일을 함께 갱신하고, 특정 플랫폼 전용 변경은 해당 파일에서만 관리합니다. 같은 항목이 있으면 플랫폼 계층의 값이 `DefaultEngine.ini`보다 우선합니다. 새 플랫폼을 지원할 때는 해당 플랫폼의 공용 Engine 파일도 추가하고 엔진과 빌드 도구에서 로드를 확인하십시오.

2026-10-27 검증: UE 5.8.3 UBT의 실제 설정 로더에서 다섯 플랫폼의 기본 맵과 Android/iOS 방향 값을 확인했습니다. 로컬 `DefaultEngine.ini`가 없는 게시 대상 검증본을 엔진으로 실행하여 인증값 자동 생성 및 Git 제외, 반복 실행 후 공용 설정 불변을 확인했습니다. 기본 ThirdPerson·Crimson Dawn 맵과 게임 Blueprint 3개의 로드도 통과했습니다. 에디터 플러그인은 같은 소스로 앞서 검증한 바이너리를 재사용했으며 이번 변경으로 C++을 다시 빌드하지 않았습니다. 모바일 패키징·기기 실행은 수행하지 않았습니다.
