# Cubeless SkySystem — Anime Billboard Sky

WebGL 레퍼런스(`traditional.html`, RGBA 방향성 라이트맵 빌보드 + 환경 LUT)를 언리얼로 이식한 서브컬처/일본 애니메이션풍 하늘 시스템.
근·중경은 라이트팩 카드, 원경은 UDS 스태틱 클라우드 방식(폴라 투영)을 자체 구현했다. UDS 원본 에셋은 수정하지 않는다.

## Asset Layout (`/Game/Cubeless/SkySystem`)

| Asset | 역할 |
| --- | --- |
| `Textures/T_SkySystem_EnvLUT` | 256×8 환경 LUT. 행: zenith / horizon / halo / cloudLit / cloudShadow / cloudRim / ambient / sun. X축 = 시간(0~24h). cream 프리셋 포팅 |
| `MPC_SkySystem` | TimeOfDay(0..1), Exposure, Stylize, ToonSoft, RimStrength, WindSpeed, FarCloudOpacity, CloudBob, SunDirOverride(.a>0.5이면 xyz 사용) |
| `Materials/M_SkySystem_SkyDome` | **Opaque** Unlit 돔. LUT 그라디언트 + 태양 디스크/헤일로 + 권운 + 밤(별/달) + **원경 폴라 구름(FarCloud) 합성까지 단일 패스**. 시간에서 태양 방향 유도. 돔 메시는 UDS `Ultra_Dynamic_Sky_Sphere`(×2.007 ≈ 40km) |
| `Materials/M_SkySystem_CloudCard` | 근·중경 카드. `CloudPlaneAtlas_LightPacked_UDSLike_Preview_2048`(4×2 셀) R/G/B=우/상/좌 라이트 응답, A=알파. 방향 가중 블렌드 + 툰 램프 + 백라이트 림 + 거리 헤이즈 |
| `Materials/M_SkySystem_FarClouds` | (레거시) 원경 전용 돔 머티리얼. 현재는 SkyDome에 편입되어 레벨에서 미사용 — 단독 원경 돔이 필요할 때 참고용 |
| `Materials/M_SkySystem_Ground` | 실루엣용 다크 그라운드 (Unlit) |
| `Maps/SkySystem_Showcase` | 쇼케이스 레벨: SkyDome(UDS 스피어) + 카드 120장 + Ground + PPV. 원경 구름은 SkyDome 머티리얼이 직접 그림 |

`Materials/M_SkySystem_Debug`는 검증용 잔여물 — 참조가 풀리면 삭제해도 된다.

## 동작 계약

- 카드 CustomPrimitiveData: `0=CellIndex(0..7)`, `1=FlipSign(±1)`. 셀 UV = `(uv+cell)*(0.25,0.5)`.
- 카드 배치: 평면을 `MathLibrary.make_rot_from_zy(centerward_normal, (0,0,-1))`로 세움(법선=중심 방향, V축=수직). 600m 이내 카드 페이드아웃.
- 빌보드 라이팅 기저는 셰이더에서 카메라 기준으로 유도(HTML과 동일) — 지오메트리는 정적.
- 태양 방향: `ang=(TimeOfDay-0.25)*2π`, `sun=normalize(cos(ang)*0.82, 0.42, sin(ang))` (z-up). `SunDirOverride.a>0.5`면 오버라이드.
- 모든 머티리얼이 셰이더 내부에서 ACES 근사 + 채도 보정. 레벨 PPV: ToneCurveAmount=0, ExpandGamut=0, 수동 노출 + **Exposure Bias 10.0**(이 조합에서 emissive 1.0 ≈ 화면 1.0 캘리브레이션 완료).
- LUT 텍스처: sRGB on / NoMips / TC_VectorDisplacementmap / X=Wrap(시간 순환), Y=Clamp. 하드웨어 sRGB 디코드를 쓰므로 셰이더에서 pow(2.2) 추가 디코드 금지(이중 디코드).

## 시간 프리셋

`MPC_SkySystem.TimeOfDay` — 0.6667=오후 4시(기본), 0.735=골든아워, 0.77=황혼, 0.0~0.2=밤(별/달).

## MCP 작업 시 주의 (이번에 실측한 함정)

- `unreal.Rotator(a,b,c)` 파이썬 위치 인자는 **(roll, pitch, yaw)** 순서. 키워드 인자 사용 권장.
- 텍스처 `AssetImportTask`는 브리지 직접 실행 시 TaskGraph 재진입 크래시 → `defer_to_ticker=True` 필수.
- 참조 중인 에셋에 `create_asset`/`delete_asset` → 모달 다이얼로그가 티커를 블로킹해 deferred 실행이 전부 멈춤. 머티리얼은 제자리 수정으로.
- MPC `CollectionParameter` 노드는 `collection` → `parameter_name` 순서로 set하면 GUID 자동 해석.
- 머티리얼 컴파일은 스크립트 안 `recompile_material` 대신 `compile_and_save_material` 툴로(타임아웃 회피, compile_error_count 확인).
