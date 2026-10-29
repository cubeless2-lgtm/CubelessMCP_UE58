# Material / MaterialFunction 코멘트 박스

공식 Unreal MCP의 Toolset Registry에 등록되는 도구는
`cubeless_mcp_ue58.registration.material_comments_toolset.CubelessMaterialCommentTools`다.
반환값은 상태를 담은 JSON 문자열이며, 필요한 Editor 전용 C++를 빌드하고 에디터를 다시 시작해야 한다.

| 도구 | 사용 |
|---|---|
| `inspect_material_comments(asset_path)` | GUID·텍스트·좌표·영역과 Expressions/EditorComments 소속 조회 |
| `add_material_comment(asset_path, text, x=0, y=0, width=400, height=100, execute=False, save=False)` | EditorComments에 실제 코멘트 박스 생성 |
| `repair_material_comments(asset_path, execute=False, save=False)` | 잘못 Expressions에 들어간 기존 객체를 재생성하지 않고 EditorComments로 이동 |

대상은 명시적인 `/Game` Material 또는 MaterialFunction 에셋이다. 대상 Material Editor를 닫고 실행한 뒤 다시 연다.
열린 편집기는 미저장 preview가 원본을 덮어쓸 수 있으므로 변경 전에 거절한다. 선택 상태나 Actor 이름 추정은 사용하지 않는다.
`execute=False`는 사전 확인이다. 실제 실행 예는
`repair_material_comments(asset_path="/Game/Materials/MF_Example", execute=True, save=True)`다.

복구는 기존 객체·GUID·텍스트·영역·색·서브그래프를 보존하며, 영역이0이면 그대로 보존한다.
생성은 양수 영역과16384 UTF-16 단위 이하의 텍스트를 받는다.
일반 `add_material_node`에 MaterialExpressionComment를 지정하면 전용 도구를 안내하고 변경 전에 거절한다.
엔진 CreateMaterialExpressionEx는 코멘트를 Expressions에 넣지만, 그래프는 EditorComments를 MaterialGraphNode_Comment 박스로 구성한다.

기본값은 저장하지 않는다. `save=True`는 생성/실제 복구가 발생한 대상 패키지 전체를 저장하므로 기존 미저장 편집도 포함한다.
이미 정상인 목록의 repair는 저장을 시도하지 않는다. 전체 월드 저장과 명시적 셰이더 컴파일은 수행하지 않는다.

콜백 이후 원 표현식의 편집 값·GUID·좌표·서브그래프와 실제 입력 연결을 비교한다.
속성은 FProperty::Identical로 비교하며 Material 루트 연결도 검사한다. GraphNode·컴파일러·프리뷰 캐시는 제외한다.
Material 자체 설정 전체나 루트의 UseConstant/Constant 기본값까지 검사하는 것은 아니다.
불일치 시 first_mismatch에 객체·속성·기대값·실제값을 남기고 success=false, changed=true, change_state_known=false로 반환한다.
저장 이후 실패해도 실제 save_attempted/saved 결과는 유지한다. 원자적 롤백·불확실 요청 자동 재시도는 제공하지 않는다.

UE5.8.4에서 공식 MCP 정상 생성·복구·저장, 동일 객체의 저장/새 프로세스 재로드,
실제 함수 편집기의 코멘트 박스 클래스 생성과 엔진 콜백 경계8건을 확인했다.
Editor 전용 기능이며 게임에 Cubeless/MCP 클래스나 모듈 의존성을 추가하지 않는다.
