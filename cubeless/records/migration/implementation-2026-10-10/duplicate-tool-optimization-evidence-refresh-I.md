# G 조사 이후의 실제 근거 갱신

이 문서는 `duplicate-tool-optimization.*`의 G 시점 분석을 보존하면서, 현재 결론에 재사용하면 오래된 상태가 되는 항목을 구분한다. 원본 기록·기능 대장·status·work-log는 변경하지 않았다. 동명 JSON에 증거 파일 SHA256과 G/I 전체 등록 이름 차이를 저장했다.

| 항목 | 현재 확인된 근거 | 여전히 제한되는 범위 |
| --- | --- | --- |
| 등록 수 28개 | 실제 I 공식 Registry는 6개 Toolset / 27개 도구. G와 비교해 새 native `CompileBlueprintValidation`만 제외되고 Blueprint compile 공개 3종·배치·기존 이름들은 유지됨 | 공개 개수 차이를 기존 기능 누락이나 원본 436개 중복 제거 수로 해석하지 않음 |
| Batch cold pending | 실제 G2 새 프로세스 reload 3단계와 독립 검수 PASS | 실제 Undo·legacy 모든 command·큰 workload 성능은 미검증. 단일 trial에서 batch가 빨랐다는 주장 없음 |
| Add/Set 구현 후 실행 미확인 | I Material v2 author 43단계와 cold reload PASS. I2 edge author 42단계 PASS: node_position 생략/명시, 숫자·문자열 JSON, 잘못된 배열/속성값과 독립 readback | 첫 edges I는 변이 전 검수기의 오류 문구 제한 때문에 실패했고 그대로 보존. null은 상위 converter가 건너뛸 수 있어 강제 거부 증거가 아님 |
| MPC 상속 pending | I author 30단계·실제 cold reload·강화한 독립 검수 PASS. 값·타입·GUID·declared_in·leaf 참조·로컬 기본값을 서로 대조. G2 partial은 계속 실패 | private override map, runtime override, cycle/손상 fixture, nested function/CollectionTransform, 렌더·패키징 범위는 미완료 |
| 반복 조회 최적화 제안만 존재 | J 후보 소스의 GetCollectionValues/ListCollectionNodes 두 읽기에서 현재 호출의 Before payload를 재사용하는 변경을 독립 리뷰함 | 빌드/실행/성능 결과와 별도. After 신규 조회와 dirty 비교, mutation checkpoint를 유지하며 나머지 중복 순회는 남음 |

J 소스 리뷰의 파일 해시와 세부 검사는 `records/verification/implementation-2026-10-10/operations-material-read-reuse-J-source-review.json`에 있다. 공유 JSON 포인터는 현재 response 조립에서 변경되지 않고, 외부 입력이나 다른 호출의 캐시가 아니다. 계측값은 실제 helper 진입·반복문·public getter 호출 지점 수이며 엔진 내부 작업 전체가 아니다. I에는 이 카운터가 없으므로 소스로 예상한 I 횟수를 실측치로 표시하지 않는다.

`operations-batch-pipeline.json`의 `cold_runtime_pending`와 이전 Add/Set·MPC 문서의 pending 상태는 역사적 기록이다. 현재 설명에는 위의 새 증거를 함께 사용한다. 기존 audit의 source line 번호는 G 당시 위치이며 J 소스의 동일 줄 번호를 가리킨다고 간주하지 않는다.

이번 갱신으로 새로 확정한 공식 도구 완전 동등 대체는 0개이고, legacy identity 삭제도 0개다. Material recompile이 현재 오류 없이 반환된 결과는 shader 완료·렌더링 품질 증명이 아니다. 전체 이식 완료도 선언하지 않는다.
