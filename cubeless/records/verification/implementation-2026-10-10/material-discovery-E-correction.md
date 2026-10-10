# 초기 검색 기록 E의 판정 정정

`material-discovery-E.json`은 짧은 Toolset 이름을 사용해서 공식 서버가 `isError=true`와 정확한 전체 등록 이름 목록을 반환한 **실패 기록**이다. 당시 CLI가 HTTP/JSON-RPC 응답 성공만 확인해서 바깥 `status=pass`를 기록한 검수기 오류가 있었다. 이 바깥 상태를 도구 검색 성공 증거로 사용하지 않는다. 에셋 수정은 없었다.

`official_mcp_client.py` CLI는 이제 list_toolsets/describe_toolset 응답에 `tool_payload` 오류 검사를 적용한다. 실제 전체 이름 `cubeless_mcp_ue58.registration.material_toolset.CubelessMaterialTools`로 조회한 **material-discovery-E2.json**에는 정상 Toolset 정의와 도구 9개가 있다. E2가 현재 등록 증거이다. 원래 E의 raw 응답은 그대로 보존한다.
