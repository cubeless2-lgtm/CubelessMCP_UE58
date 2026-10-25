# Python 후보의 구문 검증

FE 연결 후보에서 같은 update 호출에 boolean_raw_guard_ready 키워드가 두 번 들어간 결함을 발견했다. ast.parse는 AST를 만들었지만, compile(source, filename, 'exec')는 중복 키워드로 SyntaxError를 발생시켰다. 원 후보와 첫 실패는 source-candidates/FE/Review/RegistrationRoot/source-compile-first-failure.json에 보존했다. 엔진이나 Main에는 적용되지 않았다.

생성하거나 조합한 Python 후보는 import하지 않는 compile 검사를 거친다. AST 비교는 기존 계약과 구조를 대조하는 데 사용하고, 실행 가능한 구문 검증과 구분한다. 이 검사는 Unreal을 불러오거나 에셋을 변경하지 않는다. 이미 통과한 엔진 행동 검증이나 배치 계약 검사를 이 이유만으로 반복할 필요는 없다.

이 경험은 UE 5.8.3 작업에서 확인한 Python 소스 검증의 한계다. Native 컴파일이나 실제 공식 MCP 호출 성공을 대신하지 않는다.
