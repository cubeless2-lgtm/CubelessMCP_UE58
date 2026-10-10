# Material 조회 중복 제거: I/J 실제 비교

UE 5.8.3 CL58210709, 격리 CubelessMCPVerification, 공식 MCP HTTP 8001, `-nullrhi`. I PID20944와 J PID63880에 같은 저장 fixture를 읽었다. 두 버전 모두 명시적 컴파일·저장·에셋 변경은 0회다.

J는 `GetCollectionValues`와 `ListCollectionNodes` 두 읽기만 같은 호출의 Before 결과를 재사용한다. 독립 After Capture와 snapshot/dirty 비교를 보존한다. 기존 소스의 read helper 3회가 J에서는 실제 계수 2회이고, capture_state 2회/reuse 1회/Execute 0회다. 내부 엔진 재귀 전체 횟수로 해석하지 않는다. mutation의 Execute 경로는 유지하며 J Material author 43단계도 통과했다.

각 사례 warmup 1회 후 10회 연속 측정했다. 응답 전체에서 매회 달라지는 operation_id와 새 native 계측/출처 필드 2개만 제외했으며, 나머지 데이터·Before/After·실패 결과가 I/J 사이 및 각 프로세스의 반복 호출에서 일치했다. 잘못된 패키지와 타입의 오류도 포함했다.

| 사례 | I HTTP 중앙값 ms | J HTTP 중앙값 ms | J 주요 관찰 |
| --- | ---: | ---: | --- |
| Leaf 전체 값 | 32.507 | 16.611 | 값 read helper 2, base chain 방문 6 |
| 상속 scalar 필터 | 17.091 | 16.547 | 값 read helper 2, default lookup 2 |
| default 제외 | 24.814 | 16.700 | 값 read helper 2, default lookup 없음 |
| CollectionParameter 노드 | 32.390 | 24.452 | 노드 read helper 2, 독립 graph capture 2 |
| 없는 collection | 17.110 | 24.970 | preflight capture 1, 같은 오류 |
| Material을 collection으로 입력 | 16.737 | 16.790 | preflight capture 1, 같은 오류 |

이 수치는 서로 다른 프로세스, 고정 I→J 순서, 작은 3단계 MPC/2노드 fixture, 캐시·디스크 효과 미보정 조건이다. HTTP 시간은 약 16ms 에디터 tick의 영향을 강하게 받으며, 일반적인 속도 향상률이나 대형 그래프 성능을 증명하지 않는다. 확정된 개선은 동일한 계약과 마지막 검증을 유지하면서 중복 read helper 실행을 줄인 것이다.

근거:
- `material-read-reuse-baseline-I.json`: 6사례×11회, 모든 읽기 성공/예상 오류와 clean 상태.
- `material-read-reuse-comparison-J.json`: baseline hash, 전체 계약 일치, 실제 계수와 시간.
- `registered-schemas-and-diagnostics-J.json`: 6 Toolset/27개 도구, Source/DLL/Python 해시.
- `build-material-J.log`: 성공, 총 12.27초.
- `material-author-J.json`: 기존 mutation 경로 43단계 회귀.
- `operations-material-read-reuse-J-source-review.json`: 독립 소스 검토.

최적화 전후 검증 중 원본 5.6 저장소·엔진 소스·사용자 에셋은 수정하지 않았다. 전체 legacy 기능이나 MPC runtime override, render, private override-map 동등성의 완료 증거는 아니다.
