# RX_SMLODGen JSON API v1 / 설정 문서 v2

에디터 전용 플러그인 `RX_SMLODGen`이 활성화되고 로드된 UE 5.8.4에서 사용한다.
게임 런타임이나 MCP 플러그인에 의존하는 API가 아니다. 기존 MCP Python 실행 도구에서도 이 에디터 API를 호출할 수 있다.

```python
import json, unreal

def rx(op, **kwargs):
    result = json.loads(unreal.RXSMLODGenLibrary.execute_request(
        json.dumps(dict(op=op, **kwargs), ensure_ascii=False)))
    if not result['success']:
        raise RuntimeError(result)
    return result

rx('describe')
rx('open', asset='/Game/Levels/LevelPrototyping/Meshes/bench.bench')
rx('resize', count=3)              # 편집 LOD 0~2. 임포스터는 이 개수에 포함하지 않는다.
state = rx('get')
settings = state['settings']
# settings는 현재 엔진이 반환한 전체 문서를 보존하면서 수정한다.
# reduction.percentTriangles는 0~1 잔존 비율이다. UI에는 0~100%로 표시한다.
settings['lods'][1]['reduction']['percentTriangles'] = 0.5
for section in settings['lods'][1]['sections']:
    section['weight'] = 0.5
    section['excluded'] = False
rx('set', settings=settings)       # 임시 설정만 변경
rx('rebuild')                      # 가중치와 임포스터를 표시 메시의 LOD로 생성
state = rx('get')                  # preview_current, preview_lods의 실제 삼각형 수 확인
rx('snapshot', filename='bench_review_001.png')
rx('save_json', filename='bench_lod.json')
# 원본 적용을 요청받았을 때만 실행. 실제 uasset 저장은 별도 작업이다.
# rx('apply', confirm=True)
```

## 명령

| op | 입력 | 결과와 변경 범위 |
|---|---|---|
| `describe` | 없음 | 명령 및 사용 조건, 세션 유무 |
| `open` | `asset` | `/Game/` Static Mesh로 전용 RX 창과 임시 사본 생성. 기존 세션이 있으면 거부 |
| `get` | 없음 | 전체 `settings`, `preview_current`, 표시 LOD별 실제 `triangles`/`vertices`/`impostor` |
| `set` | 전체 `settings` | 검증 후 사본의 설정만 교체. 원본이나 디스크를 변경하지 않음 |
| `resize` | `count` | 편집 LOD 개수 변경. 임포스터 제외. 이후 `rebuild` 필요 |
| `remove_lod` | `lod` | LOD 1 이상 제거. 이후 `rebuild` 필요 |
| `import_lod` | `lod`, 절대 FBX/OBJ `path` | 사본에 LOD 소스 임포트. 이후 `rebuild` 필요 |
| `view_lod` | `lod` | `-1`은 Auto, `0` 이상은 표시 메시 LOD를 뷰포트에서 강제 표시 |
| `rebuild` | 없음 | 설정으로 표시 메시를 재생성. 실패 시 이전 표시 결과 유지 |
| `apply` | `confirm: true` | 최신 미리보기만 원본에 반영. 실제 uasset 저장은 하지 않음 |
| `save_json` | `filename`, 선택 `overwrite: true` | `Saved/RX_SMLODGen/Presets/`에 UTF-8 JSON 저장 |
| `load_json` | `filename` | 같은 폴더에서 JSON을 읽어 설정만 교체. 이후 `rebuild` 필요 |
| `snapshot` | 새 `filename.png` | RX 창만 Slate로 캡처하여 `Saved/RX_SMLODGen/Previews/`에 저장. 기존 파일 덮어쓰기 거부 |
| `close` | `confirm: true` | 임시 편집 세션 닫기. 유지할 설정은 먼저 JSON으로 저장 |

파일 이름은 경로 없는 이름만 허용한다. UI 파일 대화상자에서는 사용자가 직접 경로를 지정할 수 있다.
`get.viewport`에는 미리보기 월드의 `world_time`/`world_ticks`, `fixed_exposure`/`ev100`, `forced_lod`와 Slate 합성 설정도 반환한다. `forced_lod: -1`은 Auto이며 0 이상은 강제 표시할 LOD 번호다. 뷰포트의 노출은 표시용 옵션이며 저장하는 LOD JSON의 설정에 포함하지 않는다.
`get.preview_lods[].screen_size`는 현재 표시 기준이다. 자동 계산 중에는 렌더 데이터의 값을, 수동 모드에서는 설정된 소스 값을 읽는다. `settings.lods[].screen_size`는 설정용 소스 값이므로 자동 모드의 실제 전환값과 다를 수 있다.
`preview_current: false`이면 표시 메시가 아직 새 설정으로 갱신되지 않은 상태다. 실제 전환 결과를 평가하기 전에 `rebuild`를 호출한다. UI에서 화면 크기를 직접 편집하면 자동 계산을 끄고 입력을 보존한다. UI의 빌드 세팅은 숨겨져 있지만 JSON의 `build` 데이터는 유지한다.
오류/타임아웃 후 변경 명령을 자동 반복하지 않는다. `get`으로 관찰 가능한 상태와 원본 dirty 상태를 확인한다.

## 설정 문서

루트 필드는 `schema: RX_SMLODGen`, `version: 2`, `weight_semantics: preservation`, `asset`, `source_id`, `impostor`, `auto_screen_size`, `lod_group`, `general`, `materials`, `lods`이다. 요청 API 자체의 버전은 1을 유지한다.
`general`에는 플랫폼/품질별 최소 LOD, 스트리밍 바이어스/LOD 수 및 LOD 스트리밍 설정을 보관한다.
각 LOD에는 `index`, `geometry_id`, `import_filename`, 엔진 구조체의 `reduction`/`build`/`screen_size`, `sections`, `section_overrides`가 있다.
`sections`는 LOD 0의 Polygon Group ID를 사용하는 `group`, 0~1 `weight`, `excluded` 배열이다.
`section_overrides`는 섹션별 머티리얼 슬롯, 그림자, 충돌, 레이 트레이싱, 디스턴스 필드 및 불투명 플래그를 보관한다.
`materials`는 슬롯 이름과 머티리얼 에셋 경로를 보관한다. 배열의 순서와 슬롯 식별자를 임의로 바꾸지 않는다.

JSON은 메시의 버텍스나 촬영한 텍스처 자체를 포함하지 않는다. `asset`/`source_id`와 LOD별 `geometry_id`가 현재 사본과 맞아야 한다.
외부에서 가져온 LOD 지오메트리를 재사용하려면 원본에 저장한 에셋에서 세션을 다시 열거나 같은 LOD 소스를 먼저 임포트한다.
`import_filename`은 출처 정보이며 JSON을 읽는 것만으로 외부 파일 임포트를 실행하지 않는다.
엔진 구조체 필드 이름은 `get` 결과를 사용한다. UE 버전마다 달라질 수 있는 필드를 수작업으로 추측하지 않는다.

가중치의 목표 잔존 비율은 `percentTriangles + (1 - percentTriangles) * weight`이다. 기본값 0은 원래 감소를 그대로 적용하고, 1은 섹션을 완전히 보존한다. 예를 들어 삼각형 잔존 비율 0.5에서 가중치 0은 50%, 0.5는 75%, 1은 100%를 남긴다. 제외한 섹션도 100%를 보존한다.
이전 설정 문서 버전 1과 원본 에셋의 버전 1 가중치 메타데이터를 읽을 때는 `새 가중치 = 1 - 이전 가중치`로 자동 변환하여 감소 결과를 유지한다. 숫자를 그대로 둔 채 버전만 2로 고치지 않는다. 가져온 후 저장/내보내기는 버전 2 형식으로 기록한다.
가중치를 다르게 준 맞닿은 섹션, Nanite, 비율 외 감소 조건 등은 현재 감소기의 제한으로 거부될 수 있다.
임포스터는 설정 LOD 개수와 별도로 표시 메시의 마지막에 한 개만 존재한다. 재생성은 같은 마지막 인덱스를 교체한다.
임포스터 버텍스의 셰이딩 노말은 모두 메시 로컬 +Z이다. 머티리얼은 VertexNormalWS를 월드 공간 Normal 입력으로 사용하여 양면의 뒷면에서도 위쪽 노말이 반전되지 않도록 한다. 네 수직 평면의 배치와 삼각형 와인딩은 유지한다.
원본에 다시 적용할 때는 소유권 메타데이터가 있는 기존 임포스터 텍스처·머티리얼을 재사용한다. 미저장 변경이 있는 기존 에셋은 덮어쓰지 않는다.

원본 적용은 미저장 원본, 외부 변경, 원본 에디터 열림, PIE/SIE 등을 검사한다. 부분 실패 시 Undo를 완전한 롤백으로 보장하지 않는다.
적용 후 원본과 생성/갱신된 텍스처·머티리얼을 함께 저장하고, 재적용하려면 RX 창을 다시 연다.
