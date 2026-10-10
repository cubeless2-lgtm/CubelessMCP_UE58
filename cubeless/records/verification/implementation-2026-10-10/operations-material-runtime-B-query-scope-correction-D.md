# 과거 Material runtime B의 의존성 조회 범위 정정

원본 기록은 수정하지 않았다. `material-runtime-proof-B.md`의 실패·한계 6번 및 `operations-material-runtime-proof-B-independent.json`의 두 번째 limitations에서 editor-only 참조를 제외한다고 해석한 부분을 정정한다.

설치 UE 5.8.3 `AssetRegistry.cpp` 10355–10373행의 `FAssetRegistryDependencyOptions::GetPackageQuery`를 확인했다. 소스 SHA256: `0200285704dd44d2a43e4143d9cb78c5918c9a8f5381bc8dc946aca1534aeafe`. Game/EditorOnly 중 하나라도 true일 때에만 Game 속성 필터를 설정한다. 실제 과거 raw query_options는 둘 다 false이므로 해당 분기를 실행하지 않으며, Hard/Soft로 선택한 package query에는 Game 필터가 없다. 따라서 게임 참조와 editor-only 참조 모두 포함된다.

false인 옵션 하나만 보고 제외로 해석한 것이 원인이었다. 재실행이나 새 엔진 검증을 주장하지 않으며, 이미 관찰된 조회의 범위를 엔진 소스 근거로 바로잡는다. 기존 Pak의 정확한 에셋 포함·이전 맵 부재·fresh SaveGame 결과에는 변경이 없다. 동적 문자열 로드·native 모듈 바이너리 분석·렌더링 품질은 여전히 범위 밖이다. 각 과거 파일의 보존 해시와 실제 옵션은 같은 이름의 JSON에 기록했다.
