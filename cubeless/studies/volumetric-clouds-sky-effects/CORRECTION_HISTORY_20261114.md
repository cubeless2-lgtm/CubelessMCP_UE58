# Volumetric Clouds & Sky Effects 학습 프로젝트

상태: **재현 보정 진행 중 — 완료 판정 전**. 현재 프로젝트의 UE 5.8.4에서 작업했다.
에셋 루트: `/Game/Study/VolumetricCloudsSkyEffects`.

## 실행 방법과 강좌 순서

콘텐츠 브라우저에서 아래 맵을 순서대로 연 뒤 Play를 누른다. 각 맵에는 관찰 위치의 PlayerStart와 기본 비행 Pawn을 사용하는 GameModeBase가 저장되어 있다. 재생 중 클릭하여 입력을 잡고 WASD/마우스로 이동·회전한다. Esc로 종료한다.

| 강 | 영상 | 맵 | 핵심 에셋 및 확인 상태 |
|---|---|---|---|
| 1 | [Clouds Base Shape](https://www.youtube.com/watch?v=paO-Its5P2Q) | L_01_CloudBase | M_01_CloudBaseNative / MI_01_CloudBase. 기본 볼륨 구름의 실제 PIE 화면 확인. |
| 2 | [Cloud noise details](https://www.youtube.com/watch?v=ND1A-9n6e0Y) | L_02_CloudErosion | M_02_CloudErosionNative / MI_02_CloudErosion. 침식 구름의 실제 PIE 화면 확인. |
| 3 | [Storm flowmap](https://www.youtube.com/watch?v=Zy4GGO7-9ig) | L_03_StormFlow | M_03_StormFlowNative / MI_03_StormFlow. 중앙 개구부 확인. |
| 4 | [Storm eye 01](https://www.youtube.com/watch?v=x36BCa_Rotw) | L_04_EyeBase | HalfSphere / M_04_EyeBase. PIE 확인. MI_04_EyeBase를 실제 맵에 연결하고 PreviewColor(0,0,1)로 반구·가장자리 마스크 가시성을 보정했다. |
| 5 | [Storm eye 02](https://www.youtube.com/watch?v=zg2tYGTznho) | L_05_EyeFlowParallax | M_05_EyeFlowParallax. 흐름 텍스처가 반구에 표시됨. |
| 6 | [Storm eye 03](https://www.youtube.com/watch?v=ypfoC--ABmM) | L_06_StormEye | M_06_EyeStarsAndCore. 별·중심 노이즈·대기광 레이어 표시 확인. |
| 7 | [Improving Sky & Clouds](https://www.youtube.com/watch?v=GwOln8Ckdio) | L_07_StormSky | MI_07_StormEye / MI_07_StormLighting / M_07_SkyDome. 주황색 중심과 구름 발광 PIE 확인. 원본 대비 보정 중. |

[원본 플레이리스트](https://www.youtube.com/playlist?list=PLIxJB8bm2Y11ZX8oCMZQxtX4Nx_0kmwj3).
[저자 배포 자료](https://drive.google.com/file/d/1XNi98Ierc0EkV0dtUwjteLoWXsUvOSCs/view)의 storm_flowmap, stars, HalfSphere, 볼륨 flowmap 함수를 사용했다. T_SoftSmoke와 T_TipNoise는 엔진 텍스처를 복제했다. 함수는 MF_AuthorVolumeFlowmap과 MF_ThunderPulse이다.

## 현재 구현과 교정 근거

- 높이 프로파일은 `1-(1-exp(h))*(1-exp(1-h))`. HeightMaskBias는 Power가 아니라 Multiply이다(1강 568초). 밀도는 `(noiseG-profile*bias)*CloudDensity/100`이며 침식 항을 포함한다.
- 폭풍 바이어스는 `lerp(normal,lerp(storm,center,saturate(pow(flowB*bounds,StormCenterContrast))),stormAlpha)`이다. 3강1200/1269초의 중간 그래프에는 Power가 없지만 1653~1656초에 Power와 Saturate가 추가된다. 앞부분만 보고 대비 조절을 제거했던 판단을 정정했다. 기본 지수1.5, MI03은1.0, MI07은0.9로 시각 조정했다. 흐름 시간에 3D 노이즈 지터를 더했다. TimeNoiseScaleKm40은 근사 설정이다.
- 7강 음수 지수 발광은 밀도를 곱한 Extinction 신호에서 시작한다(7강 280초). 임의의 epsilon Max를 제거했다.
- 눈의 대기광은 별·중심 레이어 전에 합성한다. 별은 vertex G 마스크로 Lerp하며, 별·중심 노이즈 값은 G를 사용한다. 중심 마스크는 vertex G 대신 범위 제한된 flowmap B이다(6강 1588/2046/2340초).
- 중심 노이즈는 `color*(saturate(pow(noiseG*intensity,contrast))+offset)`이다(6강 2170초). 최신 기본값은 tiling2.5, intensity1.25, flow intensity0.75, offset0.05이다. 타일링2.5는 2540초 화면의 근사값이며 마지막 자릿수까지 일치한다고 주장하지 않는다.
- M07 반투명 Opacity에는 연속 마스크를 연결했다. Masked용 DitherTemporalAA 출력은 사용하지 않는다. UE5.8에서 Translucent와 IsSky 조합은 컴파일 거부되어 IsSky=false이다.
- 공통 MF_ThunderPulse는 주기1.86초의 시간 신호와 cos/sin/cos 곱을 Saturate한다. 속도0.1/2/3은 판독 불확실한 원본을 근사한 값이다. 볼륨과 눈에서 같은 함수를 사용한다.
- MI_07_StormEye는 6강의 푸른 기본색과 별개로 마지막 영상의 주황색을 근사한다. 색상·반구 배치·일부 파라미터는 시각 조정값이다.

## 검증한 범위

1~7강 모두 저장 후 개별 맵으로 로드하여 실제 UEDPIE 월드와 DefaultPawn을 확인했다. 각 PIE 화면을 Computer Use로 확인했고 다음 맵으로 넘어가기 전에 종료했다. 최종 IsPIERunning=false를 확인했다. 맵 전환에서 MCP 결과가 unknown일 때 재실행하지 않고 별도 호출로 월드를 확인했다.

공통 번개 함수는 임시 GPU 렌더 타깃으로 10초 구간의 64개 샘플을 측정했다. 출력 범위0..0.877402, 계산식 대비 최대 오차0.00001134였다. 이는 시간 함수 계산 검증이며 원본 번개와 정확히 같은 시각·강도라는 증거는 아니다.

저장된 7개 맵의 시작 위치: 1~2강 (0,0,200), pitch25/yaw0; 3~6강 (0,-1800000,200), pitch20/yaw90; 7강 (0,-1800000,200), pitch30/yaw90. 단위cm. 7강 구름층은5~15km이다.

삭제 대상 내부 이외의 참조가 없는 진단 에셋10개를 에디터의 일반 Delete Assets 창에서 확인하고 삭제했다. 기존 사용자 에셋은 대상에 포함하지 않았다. 삭제 후 10개 모두 부재, dirty content packages=[]를 확인했다. 남은 콘텐츠 파일은29개(맵7, 함수2, 머티리얼/인스턴스15, 소스5)이다.

## 완료 전에 남은 항목

- 복원한 중앙 마스크 contrast를 포함해 최종 색·광도·밀도를 원본과 더 대조한다. 현재 화면은 원본보다 옅고 중심 모양이 다르다.
- 각 강의 구름/흐름/시차/번개 동작을 시점과 시간 조건을 고정하여 대조한다. PIE 진입 성공만으로 모든 동작의 재현을 판정하지 않는다.
- 구름 경계의 시간 누적 전 점무늬와 6~7강 최종 별/노이즈 대비를 재검토한다. 4강 단색 반구 가시성은 인스턴스 연결 수정으로 해결했다.
- 남은 최종 변경 뒤 저장·다시 로드 검증을 수행한다. 29개 에셋의 직접 hard/soft/management/searchable 참조 및 머티리얼/함수 표현식 클래스에서는 MCP·Toolset 참조가 발견되지 않았다. 이는 패키징/플러그인 비활성 상태의 독립 실행 검증은 아니다.
- 원본의 산 지형은 이 강좌의 제작 대상이 아닌 배경이며 현재 맵에는 없다. 배경을 포함한 동일 화면이라고 주장하지 않는다.

## 재현 및 조사 기록

공식 UE MCP로 현재 실행 중인 프로젝트를 확인하고, 에디터 Python으로 엔진 에셋/머티리얼/맵을 작성했다. Python Force Delete 차단은 우회하지 않았고 정리는 에디터 UI의 일반 삭제를 사용했다. 프로젝트 C++나 MCP 런타임 기능은 추가하지 않았다. 커밋/푸시는 하지 않았다.

원본360p 영상·자동 자막·프레임·실행 스크립트·JSON 결과는 로컬 `Saved/CloudTutorialReference/`에 있다. `stage01_runtime_check.json`~`stage07_runtime_check.json`은 PIE 시작 직후의 상태를 기록하며 이후 화면 확인과 종료 호출은 대화 도구 기록에 있다. `thunder_gpu_samples.json`은 번개 계산 검증 자료다.

과거의 잘못된 가설·수정 전 값·실패 기록은 [INVESTIGATION_HISTORY.md](INVESTIGATION_HISTORY.md)에 보존했다. 현재 구현을 설명할 때 과거 기록의 완료·미완료 문장이나 옛 프로세스 ID를 적용하지 않는다.


## 최근 시각 및 동작 확인

3강1653~1656초에 추가되는 StormCenterContrast를 M03/M07에 복원하고 컴파일·저장했다. 현재 인스턴스 값은 MI03=1.0, MI07=0.9이며 원본의 최종 수치와 완전히 같다는 주장은 하지 않는다.

7강의 낮은 태양 고도(pitch=-7°, yaw=-40°, intensity10)와 반구 XY스케일28, Z13.5를 저장했다. 이는 최종 주황색 분위기 및 개구부 경계를 맞추기 위한 장면 조정값이다. 기존 높이650000cm는 유지했다. 저장된 L07을 다시 로드하고 실제 PIE로 확인했다. 같은 Pawn 위치(0,-2500000,200)에서 게임 시간17.0849초와37.3788초의 실제 UI 화면을 비교해 구름 형태가 이동함을 관찰했다. 초기 점무늬는 두 번째 화면에서 크게 줄었으며, 원인이 확정된 엔진 결함으로 기록하지 않는다. StopPIE 이후 false를 확인했다.

의존성 검사 자료: `Saved/CloudTutorialReference/final_asset_dependencies.json`. 검사 당시29개, 금지 참조0개, dirty content packages0개였다. 검사 스크립트는 출력 참조와 노드 클래스를 조사하며 패키징 실행을 대신하지 않는다.

## 5강 시차와 시간 변화 비교

실제 PIE에서 임시 Dynamic Material Instance를 만들어 원본 에셋을 수정하지 않고 비교했다. Pawn 위치(0,-800000,200), 회전pitch35/yaw90, 게임 시간48.771036초를 고정했다. ParallaxHeightRatio를0에서0.2로 바꾸자 소용돌이 텍스처의 내부 위치·형상이 달라졌다. 이후 재생을 재개하여70.550056초 화면에서 흐름이 진행된 것도 관찰했다. 테스트용 인스턴스는 PIE 월드에만 존재했고 StopPIE로 정리되었다.

7강 시작 위치를(0,-1800000,200), pitch30/yaw90로 저장했다. 이전25km 구도보다 중심·별·주변 구름 레이어를 읽기 쉽다. 이전 구도의 관찰 기록은 당시 검증 증거로만 남긴다.

## 별 레이어와 최종 명암 보정

6강43분대의 브라우저1080p 화면에서 StarsContrast2.5, StarsMaskIntensity2.0, StarsFlowIntensity0.5, StarsTiling2.8을 확인하여 M06/M07과 MI07에 저장했다. 이전360p 판독값7.5/2.5/0.5/2.5는 오독이므로 폐기한다. MI07 저장값을 별도 읽기로 확인했다. StarsColor는 원본 색상 선택기에서 확인한(40,9.333334,0)으로 수정했다. 이 최신 변경 후의 PIE 시각 검증은 아직 남아 있다. 기존 mask contrast1에서는 과도한 검은 고리가 생겨 StarsMaskContrast4로 범위를 조정했다. 이 값은 원본 판독값이 아닌 시각 보정값이다. M06/M07 부모와 MI07의 관련 값을 저장하고 화면을 확인했다.

MI_07_StormEye의 CloudContrast3, CloudDarkColor(.015,.012,.01)는 원본처럼 어두운 부분과 주황색 발광 부분이 구분되도록 맞춘 값이다. 변경 후 현재 에디터 화면에서 별이 과도하게 밝던 현상이 줄었고, 눈 내부의 명암이 구분됨을 확인했다. dirty content/map packages 모두0개였다.

4강 가까운 PIE 관찰에서 기본 회청색이 하늘과 비슷한 것을 확인했다. MI04 값을 바꿔도 변화가 없어 실제 컴포넌트를 읽었더니 M_04_EyeBase 부모가 직접 연결되어 있었다. MI_04_EyeBase로 교체하고 PreviewColor(0,0,1)를 저장했다. 에디터 화면에서 파란 반구와 아래쪽의 마스크 경계가 분명히 드러났다. 기존 PIE는 정상 종료했고 변경 후 맵을 저장했다.

## 최근 재로드 확인과 고해상도 원본 확보

MI04 연결 수정 후 L04를 다시 로드하여 PIE에서 파란 반구가 유지됨을 확인했다. 별/중심 노이즈 수정 후 L06도 다시 로드하여 PIE를 확인했다(게임시간40.84998초). 각각 다음 맵 전환 전에 정상 종료했다.

6강2008초 프레임에서 별/중심 UV에 별도 타일링 Multiply가 있음을 재확인했다. 이것만으로 별도 UV를 구름 BumpOffset 출력으로 교체할 근거는 없어 기존 연결을 유지했다.

현재 로컬 원본은360p다. 고해상도 목록 확보를 위해 기존 YouTubeClipper 환경의 yt-dlp를 기본 설정과 Node JS 런타임으로 조회했으나 모두 'This video is not available' 응답을 받았다. 이는 사용자의 브라우저에서도 영상이 없다는 뜻이 아니다. 웹 페이지 수집도 실패했다. 더 높은 해상도 또는 완성 원본 프로젝트 링크가 있는지 사용자에게 비동기 질문을 남겼다. 현재 근사 수치를 확정된 원본 값으로 바꾸어 기록하지 않는다.


브라우저에서 원본6강을 직접 열어1080p60 재생과 전체화면 판독에 성공했다. 앞서 남긴 추가 링크 요청은 현재 작업의 필수 조건이 아니다. 44:14 화면의 Cloud02 최종 조정값은 RGB(0.1,0.035,0), HSV(21,1,0.1)이다. 43:59의 파란색과44:04의 밝은 주황색은 중간 실험값이며 최종값으로 적용하지 않는다. 이 판독과 기존 그래프 파라미터의 대응을 확인한 뒤 반영한다.


## 1080p 색상 및 반구 교정

6강43:54의 Cloud01 RGB(25,2.916667,0),44:14의 Cloud02 RGB(0.1,0.035,0)를 확인했다. 현재 그래프의 CloudLightColor와 CloudDarkColor에 각각 반영하여 M06/M07과 MI07을 컴파일·저장했다. StarsColor(40,9.333334,0)도 두 부모와 MI07에 일치시켰다. 이전의 작은 RGB 근사값은 현행 값이 아니다.

6강 원본 반구 스케일(19.5,19.5,20.75), yaw20도를 L06에 반영했다. 원본 Z772697cm를 그대로 적용한 테스트에서는 이 스터디의 구름 뒤로 대부분 가려졌다. L06 구름은 bottom2km/height5km이고 원본 장면의 위치 기준과 동일하다는 증거가 없어, 스터디 중심 위치(0,0,150000)는 유지했다. 최종 저장값은 원본 스케일/yaw와 스터디 위치의 조합이다. 절대 위치까지 동일하다고 주장하지 않는다.

L06을 다시 로드하고 실제 PIE에서 주황색 반구와 별 레이어를 확인했다. 최종 스케일에서 반구는 개구부 안에 보이지만, 원본보다 밝게 뭉개지고 중심 별 영역이 작게 보인다. 원본의 구름 대비·마스크·조명 조건을 추가 대조해야 한다. 이 결과를 시각 일치 완료로 판정하지 않는다. PIE를 정상 종료하고 IsPIERunning=false를 확인했다. M07 최신 색상 변경의 실제 PIE 검증도 남아 있다.


## 중심 노이즈 고해상도 교정 및 L07 재검증

6강42:22~43:12의 설정 변화를 추적했다. 중간의 FlowIntensity3.0과 Tiling3.156643은 최종값으로 사용하지 않았다. 43:12의 최종 목록은 TipNoiseFlowIntensity1.25, Tiling3.500281, Contrast1.25, Intensity1.25, Offset0.05다. 43:02 색상 선택기 RGB(1.5,0.35,0)를 확인했다. M06/M07 부모와 MI07에 이 값을 저장했고, MI07 읽기로 수치를 확인했다.

L07을 저장된 맵에서 다시 로드하여 실제 PIE를 실행했다. 게임 월드 UEDPIE_0_L_07_StormSky와 시간28.1110758초를 확인했다. UI에서 주황색 구름의 명암과 중심 별 레이어가 구분되는 것을 관찰했다. 최신 구름·별·중심 노이즈 색상 변경의 L07 재로드/PIE 확인은 완료했다. 다만 중심 발광 영역의 크기와 대비는 원본과 다르므로 시각 재현 완료는 아니다. 정상 StopPIE 뒤 false를 확인했다.


## 흐름맵 범위 및 중심 마스크 추가 교정

6강41:21~41:51 고해상도 화면에서 FlowmapRotation1.381258, FlowmapScale0.921031, StarsMaskContrast2.0, 최종 InnerMaskContrast18.101864를 읽었다. M06/M07 부모와 MI07에 적용하고 저장했다. 기존 FlowmapScale0.45와 StarsMaskContrast4는 현행 값이 아니다. InnerMaskIntensity는 마지막 자릿수가 원본 커서에 가려져1.58까지만 판독했으므로 TipMaskIntensity1.58을 근사값으로 사용한다. 이 값은 정확한 전체 소수 판독값이 아니다.

기존 중심 마스크 구현은 flowmap B × UV bounds × TipMaskIntensity → Power(TipMaskContrast) → Saturate 순서다. intensity가 Power 뒤에 있는 것으로 잘못 추정하지 않도록 작성 스크립트와 연결을 확인했다.

최신 변경 후 L07 재로드/PIE에서 넓어진 별 분포와 주황색 구름을 확인했다. 중심 발광이 원본과 일치하지 않는 상태는 남아 있다. 다음에는 UV 변환의 스케일·회전 연산 방향 및 실제 중심 마스크 출력을 대조한다. StopPIE 이후 false를 확인했다.


## 원본 CustomRotator 연결로 수정

5강148~176초 자막과170초 그래프에서 엔진 CustomRotator 사용을 확인했다. M05/M06/M07의 직접 작성 회전식을 `/Engine/Functions/Engine_MaterialFunctions02/Texturing/CustomRotator` 호출로 교체했다. 중심 스케일 계산 `(UV-.5)/Scale+.5` 뒤에 원본 함수의 UVs/Rotation Angle 입력을 연결했고, 원래 흐름맵과 중심 마스크 UV 소비 노드를 새 출력으로 교체했다. M05는1개, M06/M07은 각각2개 연결을 변경했다. 세 머티리얼 모두 컴파일 오류 반환 없이 저장됐다.

이 변경은 원본과 구현 방식을 일치시킨 것이며, 기존 회전식이 잘못된 방향이었다는 판정은 아니다. L07 재로드/실제 PIE에서 구름·별 표시가 유지됨을 확인했다. 중심 발광 차이는 여전히 있어 이 변경만으로 해결됐다고 주장하지 않는다. PIE 정상 종료와 false를 확인했다. M05/M06의 이번 함수 변경 뒤 재로드 시각 확인은 남아 있다.


## 중심 마스크 단독 진단 및 최신 5·6강 확인

L07 실제 PIE의 HalfSphere 컴포넌트에만 임시 Dynamic Material Instance를 적용했다. 구름/별/배경/번개 색상과 태양 테두리 강도를0으로 만들고, TipNoiseIntensity0/Offset1/Color(100,0,0)으로 중심 마스크를 표시했다. 개구부 중앙에 뚜렷한 원형 발광 영역이 나타났다. 따라서 중심 마스크가 연결되지 않았거나 화면 밖으로 이동했다는 가설은 이 조건에서 지지되지 않는다. 이후 비교는 노이즈 값과 주변 레이어의 상대 밝기 및 조명 조건에 집중한다. 에셋에는 이 진단값을 저장하지 않았고 StopPIE로 임시 인스턴스를 정리했다.

CustomRotator 변경 후 L05와 L06을 각각 다시 로드하여 실제 PIE 화면을 확인했다. L05는 파란 흐름 텍스처가, L06은 주황색 구름과 별이 표시됐다. L06은 여전히 원본보다 밝고 낮은 시점에서 중심 영역이 작게 보인다. 재로드/표시 확인과 원본 시각 일치는 구분한다. 각 PIE를 다음 맵 전환 전에 종료했고 마지막 IsPIERunning=false를 확인했다.


## 구름 두 대비 합성 구조 수정

6강 약7:40의1080p 그래프에서 최종 구름 합성식을 확인했다: `Cloud01Color * saturate(pow(G,Cloud01Contrast)) + Cloud02Color * saturate(pow(1-G,Cloud02Contrast))`. 기존 단일 `Lerp(Dark,Light,pow(G,CloudContrast))`는 원본과 다른 구조였다.

M06/M07에 두 번째 대비 CloudDarkContrast, OneMinus, 각각의 Saturate/Multiply와 Add를 작성하고 기존 구름 출력 소비 연결을 새 합성 출력으로 교체했다. 기존 CloudContrast는 밝은 갈래의 대비로 유지한다. CloudDarkContrast=1은 임시 기본값이며 원본 최종 수치 판독이 남아 있다. 기존 Lerp는 연결을 끊고 초안 폐기 노드라는 설명을 달았다. 두 머티리얼 모두 오류 반환 없이 컴파일·저장했다. 이번 구조 수정의 실제 PIE 확인과 최종 대비 수치 적용은 다음 검증 대상이다.

### Part06 최종 대비 값 확인 및 적용
원본 Part06 44:17의 1080p 화면에서 Clouds Tiling=3.1, Clouds01 Contrast=7.648363, Clouds02 Contrast=5.658167, HeightRatioInput=0.25를 확인했다. M06/M07 및 MI07에 대응하는 CloudTiling, CloudContrast, CloudDarkContrast, ParallaxHeightRatio를 적용하고 컴파일·저장했다. 두 대비 분기를 더하는 수정 그래프와 함께 L06/L07을 재로드하여 실제 PIE와 DefaultPawn을 확인했다. L07에서는 밝은 구름 무늬와 어두운 부분 및 별 영역이 구분된다. L06의 주간 조명과 L07 중앙 발광·외곽 구름 조명은 원본과 여전히 차이가 있어 완료 판정은 보류한다. 테스트 후 PIE를 종료했다.

### Part07 HD 볼륨 파라미터 교정
Part07 약 6:20 이후의 HD 영상에서 다음 값을 읽어 MI_07_StormLighting에 적용·저장하고 읽기 결과를 대조했다. 이는 해당 시점에서 확인한 원본 값이며 영상 끝까지 유지되는지에 대한 최종 감사는 남아 있다.

| 학습 에셋 파라미터 | 원본 값 |
|---|---:|
| CloudDensity | 2.052937 |
| FlowSpeed | 0.525 |
| StormBaseTiling | 2.75 |
| StormErosionTiling | 1.25 |
| FlowIntensity | 5.596205 |
| HeightMaskBias | 1.495 |
| HeightContrast | 2.720406 |
| HeightMultiplier | 1.659618 |
| Light01Density | 0.00015 |
| Light01Intensity | 0.027419 |
| Light01StormAlphaContrast | 3 |
| StormCenterContrast | 0.896941 |
| StormHeightBias | 1.175 |
| StormCenterHeightBias | 1.95 |
| TimeNoiseScaleKm | 50 |

L07 재로드 후 실제 PIE/DefaultPawn을 확인했다. 외곽 구름의 밀도와 밝기가 바뀌었으며 별·구름 무늬가 보인다. 중앙 발광과 장면 조명·시점의 원본 일치는 아직 입증되지 않았다. 원본 StormPosition의 절대 좌표는 현재 학습 맵과 원점이 달라 복사하지 않았다. PIE 종료 응답을 받았다. Part07 15:45에는 Layer Bottom Altitude=5, Layer Height=10, Tracing Start Max Distance=350, Tracing Max Distance=50도 확인했다. 해당 추적 거리 두 값의 현재 에셋 일치 여부는 추가 확인 대상이다.

### 별 합성의 추가 배율 제거
Part06 약 26:40~26:50 원본 그래프는 Flowmaps Diffuse → G 마스크 → Power(Stars Contrast) → Multiply(Stars Tint) → Stars Color reroute이다. 초안에만 있던 StarsIntensity=4 및 해당 Multiply를 M06/M07에서 제거하고 Power 출력을 색상 Multiply에 직접 연결했다. 두 머티리얼 컴파일·저장 후 L07/L06 각각 재로드하여 실제 PIE/DefaultPawn과 화면을 확인했고 PIE를 종료했다. L07의 별은 유지되면서 과도한 밝기가 줄었다. L06은 주간 조명 및 낮은 시점으로 중앙부가 작고 흐리게 보이는 문제가 여전히 남는다.

L07 VolumetricCloud의 현재 값을 직접 조회한 결과 layer_bottom_altitude=5, layer_height=10, tracing_start_max_distance=350, tracing_max_distance=50으로 원본 Part07 15:45와 일치했다. 이 네 값에 대한 추가 변경은 없었다.

### L06 시작 시점 교정
기존 (0,-1800000,200), pitch20/yaw90 시점에서는 폭풍의 중앙이 낮고 작게 보였다. PIE에서 카메라만 (0,-800000,200), pitch45/yaw90으로 옮기자 별·구름 무늬·중앙 코어가 크게 보이는 것을 확인했다. 최종 학습 시점은 전체 형태를 함께 보기 위해 (0,-1200000,200), pitch40/yaw90으로 조정했다. 이는 원본 카메라의 정확한 좌표 복사가 아닌 학습용 구도 조정이다.
PlayerStart와 에디터 카메라를 저장하고 검증 도우미의 6강 시작 좌표도 동일하게 수정했다. 재로드 및 새 PIE에서 DefaultPawn과 화면을 다시 확인했다. 중앙 코어는 존재하며, 남은 차이는 주간 조명·반구와 볼륨 구름의 경계·코어의 상대 밝기이다. PIE를 종료했다. 카메라 진단 중 이미 재생 중인 월드에서 set_game_paused(False)의 False 반환을 잘못 assert해 한 차례 진단 오류가 있었으나, 앞서 실행된 위치 변경은 화면으로 확인했으며 해당 호출을 재시도하지 않았다.

### 경계 마스크 및 L06 조명 교정
원본 Part06 4:41 HD 화면에서 읽었던 DitherIntensity=1.184393, DitherMaskContrast=0.921836, FresnelDitherExponent=2.391718, FresnelDitherMultiplier=1.627194를 M06/M07과 MI07에 반영했다. 컴파일·저장 및 두 레벨의 PIE 화면을 확인했다. 반구 경계는 여전히 원본과 다르게 드러나므로 해결됐다고 판정하지 않는다.
L06 태양 pitch를 -25에서 -7로 변경하여 해질녘 색감을 조정했다(yaw=-40, intensity=10 유지). 정확한 원본 수치를 옮긴 것이 아닌 화면 기반 조정이다. PIE에서 Stationary 태양의 회전 시도는 적용되지 않았음을 읽기로 확인했고, PIE 복제 객체만 Movable로 바꾼 후 회전·화면을 확인했다. PIE 종료 뒤 에디터의 원래 Stationary 액터에 -7 각도를 저장했다. 저장한 L06 조명의 재로드 검증은 다음 확인 대상이다. L07의 경계 수정은 재로드 및 실제 PIE로 확인했다.

### 흐름 강도 교정과 남은 벡터 감사
L06에 저장한 태양 pitch=-7 조명을 재로드·PIE 화면에서 확인했다. Part06 32:30 부근의 텍스처 선택 화면은 T_Noise01 계열이며, 현재 중앙 노이즈 소스와 종류가 일치한다. Part06 35:45의 노이즈 곱셈→Power→Saturate→Offset 및 색상 곱셈 구조도 추가 대조했다.
원본 Part06 4:41에서 확인한 Flowmap_intensity=10.348573을 M06/M07의 CloudFlowIntensity 및 MI07에 적용해 저장했다. 두 레벨에서 재로드·PIE를 확인했다. 결과에 강한 나선 무늬가 나타나므로 이 수치 교정만으로 원본의 최종 외형과 일치했다고 판단하지 않는다. Flow Vector의 ConstantBiasScale 변환과 Flowmaps 시간 입력, 원본 후반의 강도 유지 여부를 다음 감사 대상으로 남긴다. L06에서 시간 경과에 따른 구름·내부 패턴 변화도 관찰했다. 두 PIE는 종료했다.

### Flowmaps 기본 시간 경로 복원
Part05 5:30 그래프와 Part06 별 그래프에서 Flowmaps Time 입력이 외부 선에 연결되지 않는 점을 확인했다. 현재 엔진 Flowmaps 함수의 Time 입력은 use_preview_value_as_default=True이고 내부 Add_9를 기본 입력으로 받으며 TimeWithSpeedVariable 호출을 포함한다. 초안의 Time*0.04*0.02 연결은 이 기본 경로를 우회했다.
M05의 2개, M06/M07 각각 4개 Flowmaps Time 연결을 해제해 원본처럼 기본 경로를 사용하도록 저장·컴파일했다. 볼륨 구름의 별도 시간식은 변경하지 않았다. L05/L06/L07 각각 재로드 후 DefaultPawn PIE와 실제 화면을 확인하고 종료했다. L06/L07의 강한 나선 외형은 여전히 남으며, 기본 시간 복원으로 최종 원본 외형 일치가 입증된 것은 아니다. 다음에는 ConstantBiasScale 변환, 범위 마스크의 흐름 벡터 적용, 최종 Flowmap_intensity를 확인해야 한다.

### ConstantBiasScale 오역 교정
Part03 4:55 원본 프레임의 Details는 Bias=-0.5, Scale=0.5이다. 초안은 Bias=-0.5, Scale=2로 원본 대비 네 배의 벡터 크기를 생성했다. Part05가 이 볼륨 흐름 벡터 블록을 복사하는 과정도 자막 0:54~1:08로 확인했다.
M03/M05/M06/M07 Eye/M07 Volume의 단일 ConstantBiasScale을 각각 확인하고 Scale만 0.5로 교정했다. 다섯 머티리얼 모두 컴파일·저장 성공 응답을 받았다. 마지막 볼륨 머티리얼 저장 시 셰이더 컴파일 대기가 발생했고 동일 실행 핸들을 기다려 완료를 확인했다. L07을 재로드하여 실제 PIE/DefaultPawn과 화면을 확인했다. 이전보다 과도한 나선 왜곡이 줄고 중앙 주변의 밝은 무늬가 나타난다. L03~L06의 변경 후 재로드·시각 검증과 원본 최종 외형 대조는 남아 있다. L07 PIE는 종료했다.

### 벡터 배율 교정 이후 L03~L06 재검증
ConstantBiasScale 수정 이후 L03/L04/L05/L06을 순서대로 재로드하고 각 실제 PIE 월드와 DefaultPawn 및 화면을 확인했다. 각 테스트를 종료한 뒤 다음 레벨로 전환했다. L03은 구름층의 폭풍 개구부, L04는 파란 반구, L05는 흐름 텍스처, L06은 별·구름·중앙 발광을 표시한다. 이 검증은 수정 후 표시와 실행 확인이며, 원본과 모든 시각 요소가 일치한다는 판정은 아니다. L06 반구 경계 및 최종 흐름 강도·배치 대조는 남아 있다. 마지막 PIE도 종료했다.

### 반구 흐름 벡터 범위 마스크 복원
Part05 2:04 설명과 5:30 그래프에서 흐름 벡터에 범위 마스크를 곱해 반복 영역을 제거하는 연결을 확인했다. 초안 M05/M06/M07의 CloudFlowIntensity Multiply 출력은 이 범위 제한 없이 Flowmaps 및 별·코어 레이어로 연결돼 있었다. M06/M07은 기존 UV 범위 마스크를 재사용하고, M05에는 동일한 0~1 범위 마스크를 추가하여 흐름 벡터에 곱했다. M05의 두 소비 연결, M06/M07의 네 소비 연결을 각각 교체했다. 세 머티리얼 컴파일·저장 후 L05/L06/L07을 차례로 재로드하여 PIE/DefaultPawn 및 실제 화면을 확인하고 매번 PIE를 종료했다. 중앙 패턴은 유지된다. 범위 밖 벡터 제한을 복원한 것이며 원본과의 최종 외형 일치를 뜻하지 않는다.

### 초안의 추가 배경색 혼합 제거
Part06 10:40 그래프는 두 구름 색상 분기를 더한 결과를 대기광 혼합으로 직접 전달한다. 초안에는 그 사이에 BackgroundColor를 별도 혼합하는 Lerp가 있어 이를 M06/M07에서 제거했다. 3분대 자막의 background blend는 Dithering 그룹으로 이름을 바꾸는 설명이며 별도 배경색 레이어의 근거가 아니다.
첫 수정은 M06 소비 노드 수를 1로 잘못 가정해 재연결 이후 중단됐다. 읽기 검사로 M06은 이미 우회됐고 M07에는 두 연결이 남아 있음을 확인한 뒤, 확인된 상태에 맞춰 삭제·컴파일·저장을 마쳤다. 무조건적인 재실행은 하지 않았다. 이후 L06/L07 각각 재로드·PIE/DefaultPawn과 실제 화면을 확인하고 종료했다. 반구 경계와 최종 원본 외형의 차이는 계속 남아 있다.

### 높이 진단 및 대기광 파라미터
L06 PIE 복제 월드에서만 반구 Z=772697, 구름 하단=5km/두께=10km, 카메라 pitch50으로 비교했다. 밝은 테두리가 계속 남아 배치만으로 설명되지 않음을 확인했다. PIE 종료로 이 높이 변경은 폐기했으며 저장된 레벨은 변경하지 않았다.
원본 Part06 약 14분 전후 HD 화면에서 대기광 마스크 대비=2.335454, 마스크 강도=0.966649, 대기광 강도=0.423483을 확인했다. M06/M07 및 MI07 대응 파라미터에 적용·컴파일·저장하고 읽기로 확인했다. 변경 후 재로드·시각 검증과 원본 후반 값 유지 여부는 아직 확인 대상이다.


### 대기광 노드 종류 교정
대기광 파라미터 수정 후 L06/L07 재로드·PIE에서 테두리가 더 밝아지는 것을 확인했다. 원본 Part06 8:46 노드와 8:38~8:43 설명은 SkyAtmosphereLightIlluminanceOnGround를 사용한다. 초안의 SkyAtmosphereLightIlluminance와 다른 노드이며, 현재 엔진에 OnGround 클래스가 실제 존재함을 확인했다. M06/M07 반구의 일반 Illuminance 노드를 OnGround로 교체하고 같은 LightIndex=0, 기존 소비 연결을 유지했다. 두 머티리얼 컴파일·저장 완료. L07 재로드·PIE 화면 확인 및 종료 완료. L06의 노드 변경 이후 재로드 확인과 M07 스카이돔의 같은 종류 오역 여부는 다음 확인 대상이다.

### 스카이돔 지면 대기광 교정 및 재검증
Part07 23:20 원본 그래프의 노드에 Atmosphere Sun Light Illuminance On Ground가 명시되어 있다. M07 SkyDome도 일반 SkyAtmosphereLightIlluminance를 쓰고 있어 OnGround로 교체했다. LightIndex=0과 기존 소비 연결을 유지하고 컴파일·저장했다. L06/L07을 각각 재로드하여 실제 PIE/DefaultPawn과 화면을 확인했다. L06 반구의 앞선 OnGround 변경도 이번에 재로드 검증했다. 두 PIE를 종료했다. 밝은 고리 경계는 여전히 뚜렷하며, 원본 최종 대기광 마스크 값·반구와 볼륨 구름의 상대 배치에 대한 대조는 남아 있다. 현재 외형을 원본과 일치한다고 판정하지 않는다.

### 강좌 순서 교정: 6강의 번개 연결 제거
현재 M06 Emissive를 추적한 결과 초안 connect_shared_thunder.py가 7강 기능인 ThunderPulse를 M06에도 연결한 것을 확인했다. M06의 대기광 Lerp B 입력에서 번개 Add를 우회하여 두 구름 색 합성 결과를 직접 연결하고, 해당 Add 및 MF_ThunderPulse 호출을 삭제했다. M07의 번개 기능은 변경하지 않았다. M06 컴파일·저장 후 L06 재로드·PIE/DefaultPawn과 화면을 확인하고 종료했다. 출력에 연결되지 않은 번개 관련 일부 파라미터/연산은 정리 대상으로 남아 있지만 현재 M06 출력에는 번개 함수 경로가 없다. 밝은 경계 및 원본 후반 대기광 설정 감사는 미완료다.

### 불투명도 마스크 연결 순서 교정
Part04 22:35 원본 그래프는 saturate(pow(VertexColor.R, DitherMaskContrast) * DitherIntensity)를 DitherTemporalAA에 전달하고, 그 결과에 1 - Fresnel * FresnelDitherMultiplier를 곱한다. 초안은 강도와 대비 순서, Fresnel 강도와 반전 순서, 디더링 위치가 달랐다. M04/M05/M06/M07 Eye에서 이 연결을 수정하고 컴파일·저장했다. M07은 반투명 원본의 연속 불투명도를 유지하여 디더링을 우회한다.
수정 후 4~7강 실행 기록은 각각 2026-11-14 17:21에 갱신됐으며 실제 PIE 월드와 DefaultPawn 확인 기록이 있다. 후속 확인에서 PIE 종료 상태(false)와 L07 편집기 화면을 확인했다. 중간 화면 출력이 잘려 이번 4~6강 화면은 시각 검증했다고 판정하지 않는다. L07에는 밝은 고리 경계가 여전히 남아 있어 원본 최종값과 배치 대조가 필요하다.

### 4강 최종 디더링 값 반영
Part04 23분대 HD 화면에서 DitherIntensity=1.184393, DitherMaskContrast=0.921836, FresnelDitherExponent=2.391718, FresnelDitherMultiplier=1.627194를 판독했다. M04/M05에는 초안 기본값 1/1/5/1이 남아 있어 위 값으로 교정하고 MI04도 같은 값으로 재설정·읽기 확인했다. 두 머티리얼 컴파일 및 저장 후 L04/L05를 차례로 재로드하여 실제 PIE/DefaultPawn과 화면을 확인하고 종료했다. L04 파란 반구와 L05 흐름 무늬가 표시된다. 현재 낮 조명과 카메라 구도는 원본 최종 장면과 다르며, 지평선 아래 검은 영역과 점상 하이라이트도 화면에 남아 있다. 최종 외형 일치 판정은 보류한다.

### 대기광 분기의 누락된 노이즈 제곱 복원
반구 높이만 원본 Z=772697로 바꾼 L06 PIE 비교에서도 균일하게 밝은 고리가 유지됐다. 구름층·카메라는 바꾸지 않은 이 실험은 PIE 종료로 폐기했다.
원본 Part06 9:50 및 10:05 프레임(source06_rim590.png, source06_rim605.png)에서 흐름 노이즈 G를 자기 자신과 곱하고, 그 결과를 SkyAtmosphereLightIlluminanceOnGround에 곱하는 연결을 확인했다. 9:49~9:52 자막도 자기 자신과 곱해 대비를 주는 과정을 설명한다. 초안의 대기광 분기는 이 노이즈를 누락하여 OnGround * AtmosphereSunLightIntensity만 사용했다.
M06/M07 Eye에서 CloudContrast Power에 들어가는 G 채널 마스크를 확인하고, NoiseG * NoiseG * OnGround * AtmosphereSunLightIntensity로 복원했다. 기존 마스크 및 Lerp 입력은 유지했다. 두 머티리얼 컴파일·저장 후 L06/L07을 순서대로 재로드하여 PIE/DefaultPawn과 실제 화면을 확인했다. 균일한 밝은 고리 대신 흐름 노이즈 무늬가 있는 경계가 나타난다. 두 PIE를 종료했다. 반구와 폭풍 개구부의 상대 배치, 최종 번개 식, 원본 최종 구도·조명의 대조는 남아 있다.
읽기 진단 중 ComponentMask.input 보호 속성 조회가 실패했다. 이 진단에는 변경 명령이 없었으며, 후속 작업은 공개 G 채널 속성과 MaterialEditingLibrary의 입력 노드 조회를 사용했다.

### 7강 번개 시간식 원본 대조 및 교정
Part07 8:36~8:40 HD 화면에서 시간 그래프를 확인했다. Time / 4.5 -> Cosine과, Period 2.88인 Time을 제곱한 Sine 및 (1-Time)/3.25의 Cosine을 곱한 항을 Add로 합친 뒤 Saturate한다. 따라서 w = t mod 2.88일 때 엔진 Sine/Cosine의 기본 주기 1을 기준으로 saturate(cos(2πt/4.5) + sin(2πw²) * cos(2π(1-w)/3.25))이다.
초안의 주기 1.86, 느린 배율 0.1, 빠른 배율 2, 반대 배율 3, 마지막 Multiply는 원본과 달랐다. MF_ThunderPulse를 위 원본 식으로 교정하고 두 M07 머티리얼 및 MI07의 대응 값을 저장했다. 함수 변경에 따른 셰이더 대기는 동일 프로세스 핸들로 완료까지 확인했다.
현재 에디터의 transient 재질을 사용해 10초 구간 64개 GPU 샘플을 원본 수식과 비교했다. 출력 범위 0~1, 최대 절대오차 1.7934e-5였다(thunder_source_gpu_samples.json). 기존 thunder_gpu_samples.json은 이전 초안 수식의 결과로서 최신 검증이 아니다. Transient 참조는 검사 후 해제했다. L07을 다시 로드해 실제 PIE/DefaultPawn과 화면을 확인하고 PIE를 종료했다.
시간식 교정은 완료했으나, 원본 반구 번개 분기의 Cloud01 Color 사용과 Emissive Cloud Tip Contrast/Thunder emissive intensity 값은 별도 대조 대상이다. 현재 ThunderColor/ThunderCloudMaskContrast 초안 파라미터가 원본과 일치한다는 증거로 이 시간식 검증을 사용하지 않는다. 원본은 7강 시작 시 반구 시간식을 이미 가진 상태이며 8분대에 볼륨 구름으로 복사한다. 6강 최종 반구에 이 분기가 있었는지는 추가 원본 확인이 필요하다.

### 7강 반구 색상 공유와 코어 설정 교정
Part07 8:40 전후 HD 그래프에서 번개 분기는 별도 번개색이 아니라 Cloud01 Color를 사용한다. M07 Eye의 ThunderColor 소비 연결을 기존 CloudLightColor로 교체하고 불필요한 ThunderColor 노드를 삭제했다. 초안 기본값은 (0.4,0.7,1.5), 공유한 구름색은 (25,2.916667,0)이다. MI에 남은 이전 ThunderColor 오버라이드는 더 이상 연결된 파라미터가 아니다.
같은 영상 약 8:35~8:36 HD 인스턴스 화면에서 TipMaskIntensity=1.581477, TipNoiseFlowIntensity=0.8, TipNoiseIntensity=2.0, TipNoiseTiling=5.123601, TipNoiseOffset=0.1을 읽었다. M07 Eye와 MI_07_StormEye에만 적용·읽기 확인·컴파일·저장했다. 6강 값을 이 7강 값으로 덮어쓰지는 않았다. L07 재로드 후 실제 PIE/DefaultPawn 및 화면을 확인하고 PIE를 종료했다.
이 화면은 Emissive Cloud Tip Contrast=6.0, Thunder emissive intensity=100.0도 보여 준다. 현재 초안의 번개 마스크는 CloudContrast Power 출력을 다시 Power하므로 원본 Power Base를 먼저 대조해야 한다. 강도만 100으로 바꾸지 않았으며 이 두 값 적용과 마스크 배선 교정은 남아 있다. CloudFlowIntensity=10.348573도 7강 화면에서 재확인했다.
첫 설정 스크립트는 존재하지 않는 MI_07_EyeTranslucent 이름을 가정하여 변경 전에 assertion으로 중단됐다. 디스크의 실제 MI_07_StormEye 이름을 확인해 경로를 바로잡은 뒤 실행했다.

### 7강 대기광 및 최종 반구 비율 교정
Part07 약 8:35 HD 인스턴스 화면에서 AtmosphereSunLightMaskContrast=8.614141, AtmosphereSunLightMaskIntensity=0.742638, AtmosphereSunLightIntensity=0.155214를 확인했다. 6강 중간값을 유지하던 M07 Eye/MI07 StormEye만 위 값으로 교정·읽기 확인·컴파일·저장했다.
Part07 최종 반투명 전환 및 종료 구간 HD 화면에서 HalfSphere2 Scale=(22.25,22.25,22.0), Rotation XYZ=(0,0,20), Z 약 660934cm를 확인했다. 초안 L07 반구는 Scale=(28,28,13.5), yaw=0, Z=650000으로 특히 높이 비율이 달랐다. 원본의 스케일과 yaw20 및 Z=660934로 교정했다. Z 소수부는 화면 폭 때문에 확인하지 못해 cm 단위까지만 반영했다. 원본 XY=(-155290,834040 부근)는 원본 프로젝트의 배치이며, 현재 Study 폭풍 중심 XY=(0,0)을 유지한다. 원본 폭풍 중심 대비 상대 XY 오프셋은 별도 확인 대상이다.
L07 저장·재로드 후 실제 PIE/DefaultPawn과 화면을 확인하고 종료했다. 반구 비율이 높아졌으며 화면 내 코어가 위로 이동했다. 카메라는 기존 Study 관찰 위치이므로 원본 최종 구도와 같다고 판정하지 않는다. 번개 마스크/강도와 원본 상대 배치 대조가 남아 있다.


### 6강 최종 코어 값 재확인
Part06 44:45 이후 HD 인스턴스 화면에서 TipNoiseFlowIntensity=0.75, TipMaskIntensity=1.581477을 판독했다. M06의 1.25/1.58을 교정·컴파일·저장하고 L06 재로드·PIE/DefaultPawn 및 실제 화면을 확인한 뒤 종료했다. 이 화면은 Flowmap 다음에 InnerMask가 바로 나타나며 7강의 Global Scalar Parameter Values 번개 그룹은 보이지 않는다. 이것은 6강에 7강 번개 값을 이식하지 않아야 한다는 근거다. 화면 밖 노드까지 모두 없다는 증명으로 확대 해석하지 않는다. 로컬 source06_final2685.png와 source06_final2695.png는 마지막 인스턴스/그래프 근거이며 HD 판독은 브라우저 화면에서 수행했다.


### 최근 교정 후 참조 감사와 미사용 파라미터 정리
29개 에셋의 hard/soft/management/searchable 직접 참조 및 머티리얼/함수 표현식 클래스를 재검사하여 금지 참조 0개, dirty content packages 0개를 확인했다. 이어서 Emissive/Opacity/OpacityMask/WPO에서 역추적해 M06 7개, M07 Eye 4개 미사용 파라미터를 식별하고 제거했다. 공통 PreviewColor/CloudFlowSpeed/CloudColor/BackgroundBlend 및 M06의 이전 번개 파라미터 세 개다. 삭제 전후 실제 출력에 도달하는 105/113개 노드의 입력 연결 fingerprint가 같음을 확인하고 컴파일·저장했다. 출력 연결 변경이 없으므로 이 정리만을 위한 PIE 반복은 하지 않았다. 정리 후 전체 참조 검사를 다시 실행해 금지 참조 및 dirty content가 없음을 확인했다. 이는 독립 패키징 실행 증거가 아니다.


### 7강 반투명 전환의 디더링 우회 정정
기존 기록은 원본이 연속 Opacity로 바꾼다고 단정했지만, Part07 28:20~28:43 화면/자막은 기존 OpacityMask 경로를 Opacity 채널로 옮기는 내용이다. 이 구간에는 DitherTemporalAA를 제거하는 편집이 없다. 원본 4강의 기존 경로를 유지하는 해석에 따라 M07 Eye의 최종 Multiply A를 Saturate 직접 출력에서 기존 DitherTemporalAA 출력으로 복원했다. 이는 기존 마스크 경로를 보존하는 수정이며, 7강 화면 밖 모든 내부 배선까지 새로 판독했다는 뜻은 아니다. 컴파일·저장 후 L07 재로드·실제 PIE/DefaultPawn과 화면을 확인하고 종료했다. UE5.8에서 해당 경로 컴파일은 성공했으며, 이전 연속 마스크 우회를 엔진 필수 제약으로 설명해서는 안 된다. IsSky=false 제약은 별개다.


### 7강 스카이돔 그라디언트 오역 교정
Part07 약25:35 HD 그래프에서 LinearGradient VGradient가 Multiply A/B에 모두 연결되어 V²을 만드는 것을 확인했다. 다음 Multiply에는 Clouds Brightness=0.1, 그 다음에는 OnGround 대기광이 연결된다. 초안은 V를2배하고 Saturate한 뒤 지수2 Power를 적용해 다른 마스크를 만들었다. M07_SkyDome에서 V*V Multiply로 교체하고 이전 Power/ZenithFadeContrast/Saturate/배율 Multiply를 제거했다. 컴파일·저장 후 L07 재로드·실제 PIE/DefaultPawn과 화면을 확인하고 종료했다. 선행 노이즈 대비와 밝기 및 하늘 Add는 유지했다. 이 수정은 전체 조명 일치 판정이 아니다.


### 7강 볼륨 발광 재대조

- 약 16분대 HD 인스턴스에서 Light01 High Clouds Intensity를 50, 0으로 비교한 뒤 2.75로 확정하고 반구 배치로 넘어가는 것을 확인했다. 현재 TipFillIntensity=2.75를 유지한다. 중간 비교값을 최종값으로 적용하지 않았다.
- 약 10:28의 색 분기는 LightColor + LightColor*pulse*ThunderLightIntensity이다. 현재 baseEmission*(1+pulse*ThunderLightIntensity)와 대수적으로 같으므로 합성 방식을 변경하지 않았다.


### 7강 반구 번개 노이즈 입력 교정

- 약 8:38 HD 전체 그래프에서 노이즈 채널 마스크 출력이 번개 Power와 일반 구름 대비 Power로 각각 분기하는 것을 확인했다. 기존 확대 화면은 이 입력이 화면 밖에 있어 판독하지 못했다.
- correct07_thunder_noise_input.py로 M_07_EyeTranslucent의 ThunderCloudMaskContrast Power Base를 CloudContrast Power 출력에서 동일한 원본 노이즈 G ComponentMask로 변경했다. 연결 전후 assertion, recompile 결과 [], 저장 성공을 확인했다.
- L_07_StormSky를 다시 로드하고 UEDPIE 월드/DefaultPawn 및 UI 화면을 확인했다. 이후 StopPIE를 호출했다. 원본과의 색/구도 차이는 여전히 남으며 이 연결 교정만으로 전체 재현 완료를 주장하지 않는다.


### 6강 후반 대기광 값 및 7강 합성 순서

- 7강 약 8:38 HD 전체 그래프와 현재 Emissive 연결 트리를 대조했다. 번개를 일반 구름색에 더한 뒤 대기광, 별, 코어 Lerp를 적용하는 순서가 같아 연결은 유지했다. 읽기 스크립트: inspect07_emission_tree.py.
- 6강 자막에서 29:59 이후 대기광 재조정을 찾았다. source06_atmosphere1815.png와 source06_atmosphere1835.png 및 30:35 이후 브라우저 HD 화면에서 대비8.614141, 마스크강도0.742638, 광도0.155214를 확인했다. 7강의 확인값과 같으며 기존 6강 중간값을 대체한다.
- correct06_late_atmosphere.py로 M_06_EyeStarsAndCore의 세 기본값을 변경했다. 값 readback, recompile [], save 성공 후 L_06_StormEye 재로드·PIE 월드/DefaultPawn·UI 화면을 확인했다. StopPIE 후 종료 상태를 확인했다. 최종 배치/구도와 원본의 외관 차이는 여전히 남는다.


### 6강 반구 원본 높이 복원

- 후반 HD 화면에서 반구 Z=772697, scale19.5/19.5/20.75, yaw20을 재확인했다. 현재 에디터에서 Z150000이 남아 있음을 읽었으며, restore06_reference_height.py로 Z만772697로 복원했다. XY/회전/스케일은 유지했다.
- L_06_StormEye 저장·재로드·PIE 월드/DefaultPawn·실제 화면을 확인했다. 반구는 높아졌으나 원본과 다른 넓은 빈 하늘과 앞쪽 구름 겹침이 남는다. 현재 볼륨층2km/5km의 원본 대조 및 상대 XY/카메라 검증을 다음 작업으로 남긴다. StopPIE 후 종료 상태를 확인했다.


### 볼륨 구름층 두께 근거 확인

- source03_cloud_settings_overview.png에서 3강 초반 설정 패널을 찾고 브라우저 HD로 Layer Height=10.0, Tracing Start Max Distance=350.0, Tracing Max Distance=50.0을 확인했다. Layer Bottom Altitude는 패널 위쪽으로 가려져 판독하지 못했다.
- 3강과 7강 모두 두께10km이므로 6강에도 유지된 것으로 추정했다. correct06_cloud_thickness.py로 L_06_StormEye의 두께5km를10km로 변경하고 하단2km는 유지했다. 이는 6강 직접 판독과 구분한다. 저장·재로드·PIE 월드/DefaultPawn·UI 확인 후 StopPIE. 넓은 개구부와 반구의 상대 배치 차이는 남아 있다.


### 폭풍 위치 부호 교정

- inspect_storm_extent.py에서 MI_03_StormFlow와 MI_07_StormLighting의 StormPositionKm=0, StormScaleKm=50을 읽었다.
- source03_world200.png, source03_world270.png 및 3강 약3~4분 HD 화면에서 WorldPosition에서 StormPosition*1e5를 빼고, StormScale*1e5를 더한 뒤 2*StormScale*1e5로 나누는 연산을 확인했다. 초안 native_storm.py는 첫 연산을 Add로 생성했다.
- correct_storm_position_sign.py로 M_03_StormFlowNative와 M_07_StormLighting의 첫 Add를 Subtract로 교체했다. 기존 WorldPosition/위치스케일 입력과 소비자 연결을 확인하고 recompile [] 및 저장 성공을 확인했다. 현재 위치0이므로 기존 장면 외관에는 변화가 없는 교정이다. 비영 위치의 런타임 이동 방향 검증은 아직 수행하지 않았다.


### 7강 폭풍 중심 판독 및 산란 복원

- 약6:20 이후 HD 인스턴스에서 StormPosition R=-14.242788,G=5.955026,B=0을 확인했다. 반구 X는 입력칸 뒷부분이 잘려 상대 좌표를 아직 확정하지 않았다. 과거 반구 XY의 보이는 숫자만으로 전체 좌표를 추정하지 않는다.
- 같은 화면에서 PhaseG=.5, PhaseG2=-.5, PhaseBlend=.2와 다중산란 Contribution=1, Occlusion=.45, Eccentricity=.1을 읽었다. inspect07_scattering.py에서 현재 위상0/0/0, 다중산란.5/.2/.5 및 입력없음을 확인했다.
- correct07_scattering_parameters.py로 M_07_StormLighting의 여섯 파라미터를 생성하고 VolumetricAdvancedMaterialOutput에 연결했다. recompile [], save 성공. L_07_StormSky 재로드·PIE 월드/DefaultPawn·UI 화면을 확인하고 StopPIE했다. 산란 변경으로 전체 외관 일치를 주장하지 않는다.


### 1강 기본 산란 연결 복원

- 1강 약11:30 HD 인스턴스에서 PhaseG=.5, PhaseG2=-.5, PhaseBlend=.2, MultiScatteringContribution=1, Occlusion=.1, Eccentricity=.1 기본값을 확인했다. 현재 M_01_CloudBaseNative는 이 여섯 입력이 비어 있고 위상0/0/0, 다중산란.5/.5/.5 상수를 사용했다.
- correct01_scattering_parameters.py로 여섯 파라미터를 복원했다. ConservativeDensity 기존 연결은 보존했다. recompile [], save 성공 후 L_01_CloudBase 재로드·PIE 월드/DefaultPawn·UI를 확인했다. 지평선 아래 검은 영역과 원본 배경 차이는 남는다. StopPIE 후 종료를 확인했다.


### 2~3강 산란 연결 복원

- source02_scattering405.png 및 source03_scattering1770.png에서 차폐0.2, 기여도1, 편심도0.1 및 위상0.5/-0.5/0.2를 확인했다. 현재 두 머티리얼은 차폐 상수0.2만 반영하고 나머지 산란 입력이 비어 있었다.
- correct02_scattering_parameters.py 및 correct03_scattering_parameters.py로 여섯 파라미터 연결을 복원했다. 두 recompile [] 및 저장 성공. L_02_CloudErosion과 L_03_StormFlow를 각각 재로드하고 PIE 월드/DefaultPawn/UI를 확인했다.
- 3강 현재 장면은 원본 마지막 화면보다 훨씬 두껍게 덮인 구름을 보인다. 후속 작업은 MI_03_StormFlow의 최종 밀도/마스크 값을 원본과 대조하는 것이다. 지평선 아래 검은 영역도 남는다. 마지막 StopPIE 후 종료 확인.


### 3강 최종 볼륨 인스턴스 복원

- 약29:30 이후 HD 인스턴스의 저장 장면에서 최종 밀도2.698985, 침식스케일4.136317, 흐름속도.525/강도5.596205 및 높이/폭풍 마스크 수치를 읽었다. 전체 현재값은 README에 기록했다.
- correct03_final_instance.py로 MI_03_StormFlow의 13개 값을 readback assertion과 함께 저장했다. 기존 밀도20, 침식스케일50, 흐름속도.04 등은 원본 최종값이 아니었다.
- 3강과 공유 인스턴스를 사용하는6강을 재로드·PIE 월드/DefaultPawn·UI로 확인했다. 큰 원형 개구부가 줄었으나 6강에서 앞쪽 구름이 반구를 가린다. 이후 강좌별 볼륨 변경 여부와 카메라/상대 배치 검증이 필요하다. 4~5강은 이번 변경 후 아직 재검증하지 않았다. StopPIE 후 종료 확인.


### 4강 배치 과정 대조 및 3강 구름층 교정

- source04_overview60.png와 11:40~13:00 자막을 대조했다. 저자는 반구를 대략 폭풍 중심에 수동 배치하고, 위치를 블루프린트로 연결할 수 있다고 제안하지만 구현하지 않는다. 이 구간에서 별도 볼륨 값 변경은 확인하지 못했다.
- L_03_StormFlow의 현재 하단2/두께5km를 읽고, 원본 3강 초반 HD에서 직접 확인한 두께10km로 교정했다. correct03_cloud_thickness.py. 저장·재로드·PIE 월드/DefaultPawn/UI 확인 후 StopPIE. 하단 고도와 상대 중심/시점의 차이는 남는다.

### 2강 최종 인스턴스 값 교정

원본 Part02 약 90% 지점(6:47 전후)의 고해상도 인스턴스 화면에서 CloudDensity 2.989934, ErosionScale 4.136317, HeightContrast 2.720406, HeightMultiplier 1.659618을 확인했다. MI_02_CloudErosion의 초안 값 10/50/3/2를 이 값으로 변경하고 읽기 검증 및 저장했다. L_02_CloudErosion을 다시 로드하여 UEDPIE 월드/DefaultPawn과 실제 화면을 확인했다. 구름 사이 열린 영역이 늘었으며, 현재 낮 조명과 지평선 아래 검은 영역은 원본의 최종 일몰 구도와 여전히 다르다. 스크립트: correct02_final_instance.py. 원본 조명까지 일치했다는 의미는 아니다.

### 공유 구름 값 교정 후 4·5강 재검증

MI_03_StormFlow의 최종 값 교정 이후 L_04_EyeBase와 L_05_EyeFlowParallax를 각각 다시 로드하고 PIE 월드 및 DefaultPawn을 확인했다. 실제 화면에서 두 단계 모두 반구 가장자리의 디더링 띠가 구름 아래에 드러나고, 반구의 상당 부분이 전경 볼륨 구름에 가려진다. 지평선 아래 검은 영역도 남아 있다. 따라서 공유 인스턴스 변경 후 실행 검증은 완료했지만, 배치와 조명 및 최종 외관 일치는 미완료다. 각 PIE를 종료했다.

7강 MI_07_StormLighting 읽기 결과 ErosionScale은 여전히 50이다. 원본 7강 약 5~6분 인스턴스 화면에서는 침식 그룹이 화면 밖에 있어 해당 값을 확정하지 못했다. 추정값으로 덮어쓰지 않았다. 원본 그래프의 Clouds Light01 Color 노드에는 (1,0.173,0)이 표시되며 현재 초안 (1,0.25,0.02)와 다르다. 노드 표시는 반올림될 수 있으므로 색상 편집기 또는 이후 최종 값 확인이 필요하다.

### 7강 볼륨 발광 색상 교정

원본 Part07 약 3:07의 1080p 화면에서 Clouds Light01 Color 선택 상태의 Details 값 R=1.0, G=0.172772, B=0.0, A=0.0을 직접 확인했다. 노드 제목의 0.173은 반올림 표시였다. M_07_StormLighting 및 MI_07_StormLighting의 Light01Color를 이전 (1,0.25,0.02,1)에서 원본 값으로 교정했다. correct07_light_color.py에서 읽기 검증, 컴파일 결과 [], 두 에셋 저장을 확인했다. L_07_StormSky를 재로드하여 UEDPIE 월드/DefaultPawn 및 실제 화면의 발광 구름을 확인하고 PIE를 종료했다. 반구의 강한 띠 모양 및 원본과 다른 중심 형태는 여전히 남아 있다. 이는 색상 한 항목의 교정이며 전체 최종 외관 일치가 아니다.

### 반구 흐름·시차 입력 재대조

현재 MI_07_StormEye의 실제 읽기 값은 CloudTiling=3.1, CloudFlowIntensity=10.348573, ParallaxHeightRatio=0.25, FlowmapRotation=1.381258, FlowmapScale=0.921031로 이전 교정 값과 일치한다. 인스턴스에 이전 음수 타일링이 남았다는 가설은 배제했다.

Part05 580초 프레임(source05_bump580.png)과 498~584초 자막을 대조했다. 첫 Flowmaps의 G 채널을 BumpOffset Height로, 타일링된 UV를 Coordinate로, BumpOffset 결과를 두 번째 Flowmaps UV로 전달하는 구조가 현재 M_07에도 연결되어 있다. 첫·두 번째 Flowmaps는 같은 흐름 벡터와 CloudTexture를 사용한다. 원본은 UV texture 2 offset 입력이 비어 있고 현재는 (0.5,0.5) 상수가 연결되어 있으나, 설치 엔진 Flowmaps 기본값을 직접 읽은 결과도 (0.5,0.5)이고 별도 입력 노드가 없어 수치 차이가 아니다. 따라서 이 입력을 외관 차이의 원인으로 보아 변경하지 않았다. 검사: inspect07_eye_values.py, inspect07_flow_bump.py, inspect_flow_uv2_default.py. 흐름 벡터의 전체 UV 변환과 텍스처 자체의 비교는 계속 필요하다.

### 반구 UV 스케일의 2배 분모 복원

Part05 95초와 104초 프레임에서 TextureCoordinate에서 (0.5,0.5)를 빼고 StormScale을 더한 뒤 StormScale*2로 나누는 원본 연결을 확인했다. 식은 (UV-0.5+Scale)/(2*Scale)이며, 기존 Custom 코드 (UV-0.5)/Scale+0.5에는 분모의 2가 누락되어 있었다. M_05_EyeFlowParallax, M_06_EyeStarsAndCore, M_07_EyeTranslucent의 중심 스케일 코드를 (UV-0.5)/(2*Scale)+0.5로 교정했다. CustomRotator와 기존 파라미터 값은 유지했다. 세 머티리얼 컴파일 결과 [] 및 저장을 확인했다.

교정 후 L_07_StormSky를 재로드하고 PIE 월드/DefaultPawn 및 실제 화면을 확인했다. 이전의 긴 띠 모양보다 넓은 영역의 흐름과 중심 노이즈가 나타났다. 시간 고정 A/B는 아니므로 개선량을 정량화하지 않는다. 최종 원본 외관 일치와 5·6강의 이번 변경 후 재실행 확인은 남아 있다. PIE 종료 확인. 근거 source05_scale95.png/source05_scale104.png, 스크립트 correct_eye_uv_scale_factor.py.

### UV 분모 교정 이후 5·6강 실행 확인

correct_eye_uv_scale_factor.py 적용 이후 L_05_EyeFlowParallax와 L_06_StormEye를 각각 다시 로드하고 PIE 월드/DefaultPawn과 실제 화면을 확인했다. 5강에서는 반구 가장자리가 전경 구름 아래에 드러나며 지평선 아래 검은 영역이 남았다. 6강에서는 폭풍의 발광과 별 일부가 보이지만 중심 상당 부분이 전경 볼륨 구름에 가려진다. 따라서 이번 UV 교정의 적용 및 실행은 확인했고, 상대 배치와 구름층 설정 문제는 해결되지 않았다. 두 PIE를 각각 종료했다. 6강 자동 자막에서 volume/height/density/position 키워드로 직접적인 볼륨 설정 변경 설명은 찾지 못했으나, 이것이 변경 부재의 증거는 아니다.

### 4강 최종 반구 Transform 교정

Part04 약 24:40의 HD 마지막 액터 화면에서 Scale X/Y/Z=19.5, Rotation=0/0/0, Location Z의 정수부 767107을 확인했다. Z 소수부와 XY 전체 문자열은 필드 너비 때문에 확정하지 못했다. L_04_EyeBase의 Part04_EyeBase를 이전 scale(22.5,22.5,13.5), Z150000에서 scale(19.5,19.5,19.5), Z767107로 교정했다. Study XY(0,0)는 유지했으므로 원본 절대 배치의 완전 복제는 아니다. 저장·재로드·PIE 월드/DefaultPawn·화면을 확인했다. 아래쪽에 노출되던 반구 가장자리 띠는 이번 화면에서 보이지 않지만, 반구 대부분을 전경 구름이 가리고 낮 조명/검은 지평선 문제는 남는다. PIE 종료. 스크립트 correct04_final_transform.py. 5강 배치는 후속 원본 확인 후 별도로 교정해야 한다.

### 5강 반구 배치의 이전 초안 제거

Part05 760초 최종 화면(source05_end760.png)의 뒤쪽 Transform은 4강 마지막 배치와 같은 균일 스케일19.5 및 높이767107 계열을 유지하고 있다. 4강에서 확인한 정수 높이를 기준으로 L_05_EyeFlowParallax의 Part05_EyeFlowParallax를 scale(19.5,19.5,19.5), 회전0, Z767107로 맞췄다. 이전 초안은 scale(22.5,22.5,13.5), Z150000이었다. XY는 Study 중심0,0을 유지했고 원본 Z 소수부의 정밀 복제는 아니다. correct05_final_transform.py에서 현재 월드를 확인한 후 저장했다. 재로드·PIE 월드/DefaultPawn·실제 화면을 확인하고 종료했다. 아래쪽 반구 가장자리 노출은 이번 화면에서 보이지 않지만, 원본의 파란 흐름 반구가 현재는 대부분 전경 구름에 가려져 있다. 구름층 설정/개구부 상대 위치 및 조명 차이는 미해결이다.

### 4·5강 구름층 두께 연속성 반영

두 레벨의 실제 VolumetricCloudComponent는 bottom2km/height5km였다. 원본3강의 height10km와 7강의 height10km 사이에서 같은 볼륨을 이어 사용하는 단계로 판단하여 4·5강 height를10km로 맞췄다. 이는 각 강의 해당 필드를 직접 판독한 값이 아니라 강좌 연속성에 따른 추론이다. bottom2km는 아직 유지했다. correct04_cloud_thickness.py/correct05_cloud_thickness.py에서 실제 월드 확인, 이전값 출력, 새 값 검증과 저장을 수행했다. 각각 재로드·PIE 월드/DefaultPawn·화면을 확인한 뒤 종료했다. 두께 변경 후에도 반구의 전경 구름 가림이 남으므로 이 변경을 전체 배치 문제 해결로 판정하지 않는다. bottom altitude 및 개구부 XY의 원본 대조가 필요하다.

### 7강 침식 스케일의 이전 강좌 값 반영

원본7강 600~1200초를 30초 간격으로 검토했지만 인스턴스의 침식 그룹은 확인되지 않았다(source07_mid_parameters.png). MI_07_StormLighting의 ErosionScale=50은 초기 초안 값이었으므로, 2·3강에서 직접 확인한 최종값4.136317이 후속 강좌에 유지된다는 가정으로 교정했다. 7강 해당 필드를 직접 판독한 것은 아니며 최종 대조 대상으로 남긴다. correct07_inherited_erosion_scale.py에서 이전값, 새값 읽기 및 저장을 확인했다. L_07_StormSky 재로드·PIE 월드/DefaultPawn·실제 화면을 확인하고 종료했다. 번개 위상에 따라 밝기가 달라지므로 이전 프레임과 밝기 개선을 단정하지 않는다. 전체 외관과 구름/반구 상대 배치는 여전히 미완료다.

### 최신 의존성 검증

UV 스케일·반구 배치·구름층 두께·7강 발광 색상 및 침식 값 변경 이후 audit_final_dependencies.py를 재실행했다. 소유 에셋29개의 직접 패키지 참조와 머티리얼/함수 표현식 클래스에서 MCP/Toolset 참조0개, Dirty content package0개를 확인했다. 추가로 audit_transitive_dependencies.py는 소유 에셋29개에서 hard/soft 패키지 참조를 재귀 추적해61개 패키지를 방문했고 금지 참조0개였다. /Script/ 참조는 native leaf로 처리했다. 결과 transitive_asset_dependencies.json. 이는 AssetRegistry 정적 검사이며 플러그인 비활성 상태 실행 또는 패키징 검증이 아니다. 외관 일치/배치 문제와 별개로 런타임 의존성 경계를 확인한 것이다.

### 3강 StormScale 인스턴스 오버라이드 발견

Part03 약22분의 storm 필터 화면을 HD로 확인했다. 처음 StormScale=13.872625였고, 약22:30 조정 후19.843781로 바뀌었다. 마스터 기본값50과 달리 인스턴스에서 범위를 줄였다는 직접 증거다. MI_03_StormFlow의 StormScaleKm=50을 확인된 조정값19.843781로 변경하고 읽기 검증·저장했다. 해당 값이 강좌 끝까지 유지되는지는 추가 확인이 필요하며 최종값으로 확정하지 않는다. 공유 영향은3~6강이다. 7강은 별도 인스턴스이므로 변경하지 않았다.

변경 후5강을 재로드해 PIE 월드/DefaultPawn과 화면을 확인했다. 구름 개구부 위치/규모는 달라졌지만 현재 카메라에서는 반구가 더욱 가려져, 상대 배치와 구름층 시작 높이 문제가 해결되지 않았다. PIE 종료. 원본 조정 수치를 따르는 변경이며 외관 개선 완료를 주장하지 않는다. correct03_storm_scale_observed.py. 다른 공유 레벨의 이번 변경 후 재실행 및 최종 StormScale 판독은 남아 있다.

### Part03 종료 직전 스케일 확인
YouTube Zy4GGO7-9ig 약30:03 HD 패널에서 Storm Scale19.843781, Storm Position(-14.242788,6.15991,0)을 판독했다. 이전22:36의 위치(-12.861803,6.15991,0)에서 X가 바뀌었다. 스케일은 동일하다. L_03_StormFlow 최신 스케일 재로드/PIE 및 UI 확인 성공. 재생 종료. 조명과 중심 구도는 불일치하며 완료 판정하지 않는다.

### Part07 Storm Scale25 직접 판독 및 반영
원본 약5:37 HD 패널에서25.0을 판독했다. 3강19.843781과 구별한다. correct07_storm_scale_observed.py로 MI07의50을25로 교정, 저장/readback 성공. verify_stage_runtime.py 7 --keep-playing 성공 후 UI에서 발광 눈 및 주변 볼륨 구름 렌더링 확인. 원본과 시각적 일치 완료는 아니다. PIE 종료.

### Part07 시작 시점과 PlayerStart 일치
inspect07_spatial_bounds.py로 반구 실제 bounds를 확인했다. 원본 카메라 값은 미확정이므로 시점 변경을 원본 재현 완료로 해석하지 않는다. L07 시작 시점(0,-1200000,200),pitch50/yaw90을 저장하고 PIE/UI 확인했다. verify_stage_runtime.py가 기존 PlayerStart를 찾아도 좌표/회전을 갱신하지 않던 부분을 수정해 검증 시작 transform과 저장된 PlayerStart가 일치하도록 했다. PIE 종료.

### Part07 ErosionScale 직접 근거로 교정
약7:22 HD 화면에서 ErosionScale3.544607을 확인했다. 기존4.136317은2·3강 승계 추론이었으며 잘못된 가정이었다. correct07_observed_erosion_scale.py로3.544607 저장/readback 성공. L07 재로드/PIE/UI 확인 및 재생 종료. 중심부 Tip 파라미터 readback은 기존 기록과 일치하지만, 원본과 다른 중심 밝기의 원인까지 입증한 것은 아니다.

### Part07 폭풍 영역 알베도
원본 약7:42의(0.2,0.04,0,0)에서 약7:47의(0.25,0.05,0,0)으로 조정된 후 색상 창이 닫힌다. correct07_observed_storm_albedo.py로 MI07의 초안색을 최종 관측색으로 교정했다. readback/save/reload/PIE/UI 확인, PIE 종료. verify_tip_mask_order.py는 현재 그래프의 intensity Multiply -> contrast Power 순서를 확인했다.

7강 반투명 가림 진단: M_07_EyeTranslucent는 apply_cloud_fogging=False, compute_fog_per_pixel=False, translucency_pass=AfterDOF, disable_depth_test=False였다. cloud fogging만True로 바꿔 컴파일·에디터 화면을 관찰했으나 중심부/주변 연결 차이 해결을 입증하지 못했다. 애니메이션 시간을 고정한 정량 비교는 아니므로 효과가 없다고 단정하지 않는다. 원본 옵션 근거도 미확정이므로 False로 복원·컴파일·저장했다. 원본 자막1700~1764초는 Translucent 전환과 Opacity 연결 및 비용 증가를 설명한다. cloud fogging을 켜라는 설명은 이 구간에서 확인하지 못했다.

### Part04 구름층 하단5km 직접 교정
source04_early_layer_contact.png에서 설정 구간을 찾아 YouTube HD 약1:05~1:10에서 bottom5,height10을 읽었다. correct04_observed_cloud_bottom.py로 L04만2->5 변경. 저장/readback/reload/PIE/UI 확인, PIE 종료. 눈 가림 문제는 완전히 해결되지 않았다. 원본의 비영점 StormPosition과 현재0 위치가 월드 노이즈에 미치는 영향도 다음 조사 대상이다.

### Part07 월드 폭풍 위치 복원
align07_world_storm_origin.py로 확인된 원본 StormPosition을 적용하고 눈/시점에 동일 이동량을 적용했다. verify_stage_runtime.py도7강의 비영점 시작 위치를 사용하도록 갱신했다. readback/save/reload/PIE/UI 성공, PIE 종료. 원본 반구 XY 오프셋과 원본 카메라 위치는 아직 미확정이다. 원점으로 정규화한 기존 배치와 달리 월드 좌표 노이즈를 원본 폭풍 위치에서 평가한다.

### Part04 노을 근사 및 임시 파란 발광 제거
correct04_approximate_sunset.py로 L04의 초안 태양 pitch-25를-3으로 바꾸고 yaw-40을 유지했다. L05와 같은 시각적 근사이며 원본 각도 직접 판독은 아니다. 재로드/PIE/UI에서 노을색을 확인했다. 반구의 파란색은 clarify4_preview_blue.py가 남긴 진단용 PreviewColor였다. 원본 종료 직전 그래프에서 Emissive 미연결을 확인했던 증거와 source04_end1480.png의 어두운 반구를 근거로 remove04_preview_emission.py에서 M_04_EyeBase의 PreviewColor 발광 노드를 제거했다. 컴파일[], 저장 및 Emissive 입력None 확인. 다시 재로드/PIE/UI에서 파란 발광이 사라지고 어두운 중심이 나타나는 것을 확인했다. 재생 종료. 원본 배치와 노출·대기색 일치는 아직 미완료이다.

### Part05 노을 조명 근사 교정
현재 L05 DirectionalLight가 초안 pitch-25/yaw-40,intensity10,흰색임을 읽었다. 원본은 노을 장면이나 정확한 태양 각도는 확인하지 못했다. preview05_low_sun.py로 pitch-3/yaw-40을 비교하고 save05_low_sun.py로 저장했다. 흰 낮 구름이 주황 노을색으로 바뀌어 원본 색감에 가까워졌다는 육안 판단이다. 원본 수치 직접 재현은 아니며 노출·대기색까지 일치한 것은 아니다. L05 재로드/PIE/DefaultPawn/UI 확인 후 재생 종료. L04의 조명은 이번에 바꾸지 않았다.

### Part04 밀도·침식 직접 판독 및 후속 승계
원본 약57초 HD 패널에서 Cloud density2.052937, Erosion Scale3.544607, Erosion Intensity0.1875를 확인했다. correct456_density_erosion.py로 MI04·05·06의 CloudDensity2.698985 및 ErosionScale4.136317을 앞의 직접 판독값으로 교정했다. 4강은 직접 근거이며5·6강은 계속4강 설정 승계 추론이다. 세 인스턴스 저장/readback 후4·5·6강 재로드/PIE 월드/DefaultPawn 검증 성공, 모두 재생 종료. 이번 변경 후 UI 육안 확인은6강만 수행했다. 발광 소용돌이는 보이나 원본과 색감·배치 차이가 남아 있다. ErosionIntensity는 이번에 변경하지 않았다.

### Part05·06 볼륨 인스턴스 분리와 승계 교정
inherit04_volume_to56.py로 MI_04_StormVolume을 각각 MI_05_StormVolume/MI_06_StormVolume에 복제하고 해당 레벨에 배정했다. 4강의 직접 판독 설정을 다음 강좌에 승계한 추론이며, 5·6강 패널에서 모든 값을 직접 읽었다는 뜻은 아니다. 저장/readback 후 각 레벨 재로드/PIE/DefaultPawn/UI 확인 성공, 재생 종료. 5강 중심 푸른 영역과6강 발광 소용돌이가 이전보다 넓게 드러났다. 원본 색감, 정확한 상대 배치 및 강좌별 최종 설정 대조는 남았다. 소유 에셋은 총32개이다.

### Part04 높이 바이어스 교정 후 중심 개구부 확인
원본 약58~59초 HD 전체 패널에서 HeightMaskBias1.495, StormHeightBias1.175, StormCenterHeightBias1.95, TimeNoiseScale50, MultiScatteringOcclusion0.45를 직접 확인했다. correct04_observed_volume_values.py로 MI_04_StormVolume의3강 승계값을 이 값들로 교정했다. save/readback/reload/PIE/DefaultPawn/UI 확인 후 재생 종료. 이전에는 작은 틈만 보이던 중심이 이제 뚜렷한 푸른 개구부로 열렸다. 복수 값을 함께 변경했고 시간 고정 비교가 아니므로 각 파라미터의 개별 기여를 정량적으로 확정하지 않는다. 원본 색감·반구 상대 배치 및 나머지 인스턴스 값은 추가 대조가 필요하다. 5·6강은 아직3강 값을 쓰고 있어 후속 교정 대상이다.

### Part04 별도 볼륨 인스턴스와 StormScale25
원본 x36BCa_Rotw 약56초 HD 인스턴스 패널에서 Storm Scale25.0을 직접 확인했다. 3강 최종19.843781을 모든 후속 강좌에 공유한다는 가정이 틀렸다. split04_observed_volume_scale.py로 MI_03_StormFlow를 MI_04_StormVolume에 복제하고 StormScaleKm25를 설정, L04 VolumetricCloud에 배정했다. 저장/readback, 레벨 재로드/PIE 월드/DefaultPawn/UI 확인 후 재생 종료. 3강 인스턴스는19.843781을 유지한다. 5·6강은 아직 MI03을 사용하며 각 강좌의 볼륨 설정 대조가 남았다. 이번 변경 뒤에도4강 눈은 대부분 구름에 가려져 있으므로 시각적 재현 완료가 아니다. 소유 에셋은 신규 인스턴스로30개가 되었고 이전29개 기준 의존성 감사는 신규 에셋을 포함하지 않는다.

### 볼륨 중심 높이·좌표 실연결 점검
inspect_center_height_live.py와 inspect_storm_uv_live.py로 현재 M_03_StormFlowNative의 입력 노드 및 실제 출력 채널을 읽었다. NormAltitudeInLayer, 중심 B 채널, 폭풍 A 채널이 연결되어 있다. 높이 바이어스는 lerp(HeightMaskBias,lerp(StormHeightBias,StormCenterHeightBias,saturate(pow(B*bounds,StormCenterContrast))),A*bounds)이며 원본3강1269초의 보간 순서와 일치한다. UV는 (WorldPosition-StormPositionKm*100000+StormScaleKm*100000)/(2*StormScaleKm*100000)의 XY이다. 텍스처 sRGB=False, TC_VECTOR_DISPLACEMENTMAP, XY Wrap을 확인했다. 이번 점검에서 연결 오류를 발견하지 않았으므로 그래프를 변경하지 않았다. 이것은 전체 셰이더 또는 외관 일치의 증명이 아니다. 원본4강의 볼륨 인스턴스가3강 최종값을 그대로 유지하는지 및 실제 흐름 노이즈 출력은 추가 대조가 필요하다.

### Part04 최종 반구 Y 위치 및 근거리 시점 진단
원본 x36BCa_Rotw 약24:43 HD 화면에서 반구 Y가663173.x로 보이며 cm 단위 반올림값663174를 적용했다. 기존615991에서47183cm 이동했다. X 표시(-154637...)는 끝이 잘려 있어 전체 좌표로 채택하지 않았다. correct04_observed_y.py로 저장/readback 및4강 재로드/PIE/UI 확인 성공. 눈은 여전히 구름에 크게 가려진다. 진단 목적으로 PIE pawn만(-1424278.8,363174,200),pitch70/yaw90으로 옮겼으나 가까운 하부 시점에서도 가림이 지속됐다. 이 진단 시점을 레벨에 저장하지 않았고 PIE를 종료했다. 원거리 카메라만이 원인이라는 가설은 충분하지 않으며, 볼륨의 중심부 밀도/높이 그래프를 다음 대조 대상으로 삼는다.

### Part03~06 월드 폭풍 중심 복원
align03_06_world_origin.py로 MI_03_StormFlow의 StormPositionKm을 원본3강 종료 직전 직접 확인한(-14.242788,6.15991,0)으로 수정했다. 공유 머티리얼을 사용하는3~6강 시작 시점과4~6강 반구에 동일 이동량을 적용하고 저장했다. 반구의 기존 폭풍 중심 대비 상대 위치는 유지한 것이며, 원본 반구의 정확한 XY 오프셋을 복원한 것은 아니다. 4~6강에도 이 위치가 유지된다는 가정은 추가 대조가 필요하다. verify_stage_runtime.py의 시작 위치도 일치시켰다. 3·6강 재로드/PIE 월드/DefaultPawn/UI 확인 성공 후 재생 종료. 6강 눈의 가림과3강 중심 구름 형태 차이는 여전히 남아 있다. 4·5강은 이번 이동 후 저장까지 확인했으며 재생 재검증은 아직 하지 않았다.

### Part05·06 구름층 연속성 교정과 한계
4강과 7강에서 직접 확인한 구름층 하단5km/높이10km의 연속성을 근거로, 5·6강의 하단을2km에서5km로 변경했다. 5강 후반 및6강 초반 영상에서는 해당 패널을 직접 읽지 못했으므로 두 강좌의5km는 추론한 값이다. correct_layer_continuity.py로 각각 저장/readback 후 레벨 재로드, PIE 월드/DefaultPawn 및 UI를 확인했다. 5강은 작은 푸른 틈만 보이고, 6강은 중앙 발광부 일부가 보이나 주변 눈 대부분이 구름에 가려진다. 구름층 변경만으로 외관 일치를 달성하지 못했다. 6강 PIE 종료; 기존 에디터는 유지했다. 다음 확인 대상은 원본 StormPosition 및 반구의 상대 XY 배치이다.

## 5강 마지막 파라미터 직접 대조 (12:56)

원본 zg2tYGTznho의 1080p 마지막 인스턴스 화면에서 CloudTiling=-2.0, CloudFlowIntensity=8.600579, ParallaxHeightRatio=0.25, FlowmapRotation=0.672를 확인하여 M_05_EyeFlowParallax에 반영했다. 기존 값은 각각 -1.5, 1.0, 0.2, -0.035였다. 디더 관련 네 값은 이미 원본과 같았다. 이 변경은 5강에만 적용했다.

컴파일 오류 목록은 비어 있었고 에셋 저장, 레벨 재로드, 실제 DefaultPawn PIE 실행을 확인했다. Part05_EyeFlowParallax가 수정한 머티리얼을 직접 사용하는 것도 확인했다. UI에서 주황색 구름과 푸른 중심을 확인했으며, 원본 5강도 푸른 중심을 사용하므로 색상은 유지했다. 원본과 구도·가림 정도는 아직 다르다. 검증 후 PIE는 종료했고 기존 에디터는 유지했다. FlowmapScale 및 정확한 원본 X/카메라는 미확정이다.
같은 원본 화면에서 보이는 눈 액터 Y=663173.9 부근을 정수 cm로 반올림하여 5강 Y=663174로 저장했다(기존 615991). X는 화면에서 잘려 확정하지 않았다. 이 배치 변경 후 PIE 화면은 다음 검증 대상이다.

### 5강 배치·관찰 위치 후속 확인

Y=663174cm 배치 변경 뒤 레벨 재로드와 PIE 화면을 확인했다. 시작 관찰 위치를 (-1424278.8,-584009,200)cm, pitch40/yaw90으로 옮기면 기존 18km 남쪽 관찰 위치보다 소용돌이가 크게 드러남을 실제 PIE에서 확인했다. 이 위치를 학습용 구도로 저장하고 재로드/DefaultPawn 실행을 다시 검증했다. 원본 카메라 좌표라고 주장하지 않는다. PIE는 종료했다.

FlowmapScale=0.45는 원본 5강 자막 138~143초의 설명("0.45 for now")과 초반 그래프를 통해 근거를 확인했다. 현재 0.45를 유지한다. 최종 인스턴스 패널에는 이 항목이 보이지 않아 후반 변경 유무까지 확정한 것은 아니다. README의 이전 공유 볼륨/원점/카메라/구름층 서술도 현재 교정 상태에 맞게 갱신했다.


## README에서 이전한 시점별 기록

아래는 이전 시점의 기록이며 현재 값은 README 본문을 따른다.

2강 최종 인스턴스 재대조: CloudDensity 2.989934, ErosionScale 4.136317, HeightContrast 2.720406, HeightMultiplier 1.659618로 교정했다. 저장·재로드·PIE 및 실제 화면을 확인했다. 원본 후반 일몰 조명과 현재 낮 조명의 차이는 남아 있다.

공유 MI_03 설정 교정 후 4·5강도 재로드·PIE·실제 화면을 다시 확인했다. 두 단계에서 전경 구름의 반구 가림과 반구 가장자리 디더링 띠가 확인되어, 상대 배치 및 조명 교정이 계속 필요하다. 7강 ErosionScale 50은 추가 대조 대상이다. 볼륨 Light01Color는 아래 기록처럼 원본 생성 값으로 교정했다.

7강 볼륨 Light01Color는 원본 약 3:07의 Details에서 RGBA (1,0.172772,0,0)을 확인하여 마스터와 인스턴스에 반영했다. 컴파일·저장·재로드·PIE·화면 확인을 마쳤다. 앞선 발광 색상 미확정 기록 중 이 파라미터의 생성 값은 해결됐으며, 이후 최종 장면에서의 전체 외관 대조는 남아 있다.

반구 흐름 UV 스케일 계산에서 누락된 분모의 2를 복원했다. 5~7강은 이제 원본 5강 104초 그래프와 같은 (UV-0.5+Scale)/(2*Scale)를 사용한다. 세 머티리얼 저장·컴파일과 5~7강 재로드·PIE·화면 확인을 마쳤다.

UV 분모 교정 이후 5·6강도 재로드·PIE·실제 화면 확인을 완료했다. 5강 반구 가장자리 노출과 6강 전경 구름의 중심 가림은 여전히 관찰되며, 구름층과 반구 상대 배치 대조가 필요하다.

4강 반구는 원본 마지막 화면에 맞춰 scale(19.5,19.5,19.5), 회전0, 높이 약767107cm로 교정했다. 기존 scale(22.5,22.5,13.5)/Z150000 기록은 4강에 대해서는 이전 상태다. 저장·재로드·PIE·화면을 확인했으며, 구름에 의한 가림과 조명 차이는 남아 있다. 5강에도 이어지는 배치를 반영했으며 아래에 기록했다.

5강도 마지막 화면에서 유지되는 4강 배치를 반영해 균일 스케일19.5, 회전0, 높이 약767107cm로 교정했다. 저장·재로드·PIE·화면 확인을 마쳤다. 반구 가장자리 노출은 줄었으나 전경 구름이 반구를 가리는 문제는 남아 있다.

4·5강 구름층은 bottom2km/height10km로 변경했다. height10km는 원본3강·7강 사이의 연속성에 따른 추론이며 4·5강 화면에서 직접 판독한 값은 아니다. 각각 저장·재로드·PIE·화면을 확인했고 반구 가림 문제는 남아 있다.

7강 MI_07_StormLighting의 ErosionScale은50에서4.136317로 변경했다. 2·3강에서 확인한 값을 이어받는다는 추론이며 7강 직접 판독은 미완료다. 저장·재로드·PIE·화면 확인을 마쳤다.

최신 변경 후 의존성 재검사: 소유 에셋29개 직접 참조/표현식 클래스 및 hard/soft 참조의 재귀 경로61개 패키지에서 MCP/Toolset 참조를 발견하지 못했다. Dirty content package는0개였다. 패키징 또는 플러그인 비활성 실행 검증을 대신하지 않는다.

MI_03_StormFlow의 StormScaleKm은 원본3강 약22:30의 인스턴스 조정값19.843781로 변경했다(기존50). 강좌 끝의 유지 여부는 미확정이다. 공유 대상3~6강 중5강 재로드·PIE·화면을 확인했지만 반구 가림은 해결되지 않았다. 7강 StormScaleKm은 별도 대조 대상이다.

추가 대조: 원본3강 약30:03의 HD 화면에서 Storm Scale19.843781을 다시 확인하여 종료 직전 유지 근거를 확보했다. 같은 화면의 Storm Position은(-14.242788,6.15991,0)이다. Study 위치는 여전히(0,0,0)이며 원본과의 상대 배치 재현은 미완료다. 최신 스케일 적용 상태로3강을 재로드·PIE·화면 확인했다. 낮 조명과 원본의 석양 조명 차이, 폭풍 중심 구도 차이가 남아 있다.

7강 폭풍 크기 교정: 원본 GwOln8Ckdio 약5:37 HD 인스턴스 패널에서 Storm Scale25.0과 Storm Position(-14.242788,5.955026,0)을 직접 확인했다. MI_07_StormLighting.StormScaleKm을50에서25로 변경하고 readback·저장·L_07_StormSky 재로드·PIE·UI 확인을 마쳤다. 눈의 발광 반구와 주변 구름은 표시되지만 원본의 구름과 이어지는 형태·카메라 구도는 아직 일치하지 않는다. 원본 후반의25 유지 여부 및 상대 위치 정합은 미검증이다.

7강 시점 비교: 현재 HalfSphere 로컬 bounds의 Z 범위는 약4700~50000cm이며, 월드 배치 후 약7.64~17.61km에 걸친다. 기존 수평거리18km/pitch30 시점과9km/pitch50 시점을 비교했다. 최종 Study 시작 시점은(0,-1200000,200),pitch50/yaw90으로 정리해 눈 전체가 화면에 들어오도록 저장했다. 이는 원본 카메라 수치 판독이 아닌 비교용 구도다. PlayerStart 위치/회전도 같은 값으로 갱신했고 PIE에서 확인했다. 가까이 보아도 중심 발광과 주변 구름의 연결 차이가 남아 있어 단순 카메라 문제만은 아니다. 원본의 상대 XY 배치는 아직 변경하지 않았다.

7강 ErosionScale의 추정값을 직접 판독값으로 교체했다. 원본 GwOln8Ckdio 약7:22 HD 패널은3.544607을 표시한다. MI_07_StormLighting의4.136317을3.544607로 교정하고 readback/저장/재로드/PIE/UI 확인을 마쳤다. 이전의2·3강 값 승계 가정은 이 관측으로 폐기한다. 원본 후반 유지 여부는 별도 확인 대상이다. 중심 발광 차이는 이 변경으로 해결되지 않았다.

7강 StormAreaAlbedo 교정: 원본 약7:47 HD 색상 창에서 RGBA(0.25,0.05,0,0)을 확인하고 다음 구간에서 창이 닫히는 것을 확인했다. 기존 초안(0.45,0.3,0.2,1)을 해당 값으로 교정·저장·재로드·PIE/UI 검증했다. 원본 후반의 유지 여부는 미확정이다. 중심부 밝기/주변 연결 차이는 계속 남는다. TipMaskIntensity 곱셈이 TipMaskContrast Power 앞에 연결된 것은 현재 그래프를 읽어 확인했으며 이 순서는 변경하지 않았다.

4강 구름층 하단 교정: 원본 x36BCa_Rotw 약1:05~1:10의 VolumetricCloud Details에서 Layer Bottom Altitude5.0, Layer Height10.0, Tracing Start Max Distance350.0, Tracing Max Distance50.0을 직접 판독했다. L_04_EyeBase 하단2km를5km로 교정하고 두께10km는 유지했다. 저장·재로드·PIE/UI 확인 완료. 눈의 파란 부분이 일부 보이나 전경 구름의 가림은 여전히 크다. 5·6강 하단은 아직2km이며 각각의 직접 근거 또는 연속성 판단이 필요하다. 원본4강 height10의 직접 근거도 이번에 확보했다.

7강 월드 기준 교정: MI_07_StormLighting.StormPositionKm을 원본 직접 판독값(-14.242788,5.955026,0)으로 바꿨다. 반구와 PlayerStart/검증 카메라도 같은 XY 이동량(-1424278.8,595502.6)cm만큼 옮겨 기존 상대 구도를 보존했다. 반구는 현재(-1424278.8,595502.6,660934)cm, 시작 시점은(-1424278.8,-604497.4,200)cm/pitch50/yaw90이다. 원본 반구의 상대 XY 오프셋까지 재현한 것은 아니다. 저장·재로드·PIE/UI 확인 완료. 월드 노이즈 배치는 달라졌지만 중심 밝기 및 주변 연결 차이 해결을 입증하지 못했다. 3~6강은 이 변경의 대상이 아니다.


## 6강 최종 색 합성 경로의 번개 여부 대조

원본 ypfoC--ABmM 44:52~44:57을 브라우저 HD 전체 화면으로 직접 확인했다. Cloud01/02 Color의 두 Multiply→Add→대기광 Lerp→별 Lerp 경로이며, Add와 대기광 Lerp 사이에 7강 번개 합산은 없다. 현재 M_06_EyeStarsAndCore를 읽어 Add_3 입력 Multiply_26/27 및 Lerp_3 B=Add_3, Lerp_4 A=Lerp_3을 확인했다. 함수 목록에도 MF_ThunderPulse 호출이 없다. 현재 6강 연결을 유지한다. 7강 495~520초의 시간 함수 복사 설명은 확인했으나 영상 사이에 번개가 추가된 정확한 시점은 알 수 없다. 이 대조는 6강 전체 외관 일치의 증거가 아니다.


## 7강 구름 차폐 비교 및 보정

현재 PIE에서 게임 시간88.282987초와 카메라를 고정하고 M_07_EyeTranslucent의 렌더링 설정을 비교했다. After DOF와 Before DOF만 바꾼 비교는 외관 변화가 작아 결정적 근거가 되지 않았다. Before DOF에서 Apply Cloud Fogging을 false→true로 바꾸자 전경 볼륨 구름이 반구 외곽을 가려, 따로 떠 보이던 원형 경계가 줄었다. 이후 After DOF로 복원해도 차폐 효과가 유지됨을 확인했다. 따라서 최종 변경은 Apply Cloud Fogging=true 한 항목이며, TranslucencyPass=After DOF, ComputeFogPerPixel=false는 유지했다. 원본 설정 패널의 직접 판독값이 아닌 UE5.8 시각 보정이다.

PIE 중 첫 저장은 false를 반환하여 저장 성공으로 취급하지 않았다. PIE 종료 후 현재 값(true/After DOF)을 읽어 확인하고 저장 성공을 확인했다. 이어 레벨 재로드, DefaultPawn PIE 실행, 실제 UI의 전경 구름 차폐를 확인했다. 검증 뒤 PIE를 종료했다. 격자 잔여, 원본 대비 색감과 중심 형태 차이는 남아 있으며 전체 일치를 주장하지 않는다. 이전 cloud fogging 비교가 불확실했던 기록보다 이번 시간 고정 비교를 우선한다.


## 2강 일몰 조명 보정

원본 약7:00 프레임(source02_scattering420.png)의 복숭아색 구름·보랏빛 하늘을 현재 낮 조명과 대조했다. PIE 복제본의 태양을 -3도와 -1도로 비교하고 -1도를 선택했다. 최초 Stationary 액터 회전 시도는 값이 바뀌지 않았으므로 적용으로 취급하지 않았다. 진단 중에만 PIE 복제본을 Movable로 바꿔 비교했다. PIE 종료 후 에디터 원본의 Stationary 상태는 유지한 채 pitch=-1/yaw=-40을 저장했다. 저장·레벨 재로드·DefaultPawn PIE 및 실제 화면 확인 후 PIE를 종료했다. 원본 각도 직접 판독값이 아닌 시각적 근사다. 지형과 지평선 아래 검은 영역, 정확한 구름 위상 차이는 남아 있다.


## 3강 조명 및 구름층 재대조

원본 3강 약0:29 HD 패널에서 LayerHeight=10, TracingStartMaxDistance=350, TracingMaxDistance=50을 확인했다. 현재 3강 값과 같으며 하단 고도는 패널 위로 가려져 직접 판독하지 못했다. 현재2km를 유지했다. 원본 약29:30 프레임의 따뜻한 흰 구름을 기준으로 태양 pitch=-25를 -7로 시각 보정하고 yaw=-40은 유지했다. 저장·재로드·DefaultPawn PIE 및 UI에서 따뜻한 색상 변화를 확인했다. 원본 태양 각도의 직접 판독값이 아니다. 큰 전경 구름의 분포와 지평선 아래 검은 영역은 여전히 원본과 다르다. 검증 후 PIE는 종료했다.


## レンダリング診断 / 렌더링 진단

현재 r.AntiAliasingMethod=4, sg.AntiAliasingQuality=3, ScreenPercentage=100을 읽었다. r.PostProcessAAQuality 조회의0은 비활성 설정이 아니라 존재하지 않는 변수의 실패 반환값이었다. Saved/Logs/UnrealMCP5_8_3.log의 Failed to find console variable 및 Command not recognized로 확인했다. 해당 명령은 변경되지 않았으며 AA 비활성 가설은 폐기한다.

설치 UE5.8 소스 VolumetricRenderTarget.cpp의 모드 설명을 확인했다. 기본0은 저해상도 추적/재구성,3은 전체 해상도 시네마틱이다. 7강 PIE 시간16.789883초와 카메라를 고정하고 모드0→3을 비교했다. 주변 구름 경계에 일부 차이는 있으나 중심 형태의 원본 차이를 해결하지 못했다. 공유 에디터 설정은 원래 모드0으로 복원하고 값 확인 후 PIE를 종료했다. 프로젝트 설정이나 에셋은 이 진단으로 변경하지 않았다.


## 6~7강 중심 노이즈 소스와 흐름 분기 감사

현재 두 머티리얼의 TipNoiseTexture는 Source/T_TipNoise를 가리킨다. 작성 스크립트 eye6_create.py에서 /Engine/MaterialTemplates/Textures/T_Noise01 복제를 확인했고 원본6강32:28의 텍스처 선택 화면의 T_Noise01 경로와 일치한다. 현재 7강 TipFlowIntensity=.8, Tiling=5.123601, Contrast=1.25, Intensity=2, MaskContrast=18.101864, MaskIntensity=1.581477, Offset=.1을 읽었다.

별/중심 흐름 강도 앞에 CloudFlowIntensity가 중복 곱해졌다는 가설을 점검했다. 현재 Multiply_0=(ConstantBiasScale*CloudFlowIntensity), Multiply_28=(Multiply_0*bounds), 별/중심 Multiply_9/15가 Multiply_28을 받아 각 강도를 곱한다. 원본6강1185초에서 Flow Vector named reroute를 마지막 bounds Multiply 출력에서 분기하는 화면을 확인했다(source06_flow_origin1185.png). 따라서 이 상속 연결은 원본과 일치하므로 변경하지 않았다. 이는 중심 외관 차이의 두 후보를 제외한 근거이며 전체 재현 완료 증거는 아니다.


## 7강 원본 반구 Y 좌표 반영

원본7강30:12 HD 전체 화면 Details에서 반구 Y=834040.x, Z=660934.x, scale22.25/22.25/22, yaw20을 확인했다. 원본 X는 -155290...까지만 보이며 끝이 잘려 있어 확정하지 않았다. 이전 기록에서 Y까지 미확정 X와 묶어 보류했던 부분을 분리했다. L07 반구 Y를 폭풍 중심595502.6에서 원본 정수 cm 근사834040으로 바꿨다(차이238537.4cm). X, Z, 볼륨 중심, 카메라는 유지했다. 저장·readback·레벨 재로드·DefaultPawn PIE/UI 확인 후 PIE를 종료했다. 원본 상대 X 및 최종 외관 차이는 여전히 남아 있다.


## 6강 최종 반구 Y 좌표 반영

원본6강44:21 HD 전체 화면에서 HalfSphere2 Y=663173.9 부근, Z=772697.x, scale19.5/19.5/20.75, yaw20을 확인했다. 현재 반구 Y615991을663174cm로 바꾸고 저장했다. X는 -154637... 이후가 잘려 확정하지 않았으며 기존 폭풍 중심 근사값을 유지했다. 레벨 로드의 current_unknown 응답은 재시도하지 않고 별도 호출에서 L_06_StormEye 실제 월드를 확인한 뒤 수정했다. 저장/readback·재로드·DefaultPawn PIE·UI 확인 후 PIE를 종료했다. 원본 대비 색감과 구름 가림 차이는 여전히 남아 있다.


## 6강 주변 구름 조명 보정

PIE 게임 시간16.413941초와 카메라를 고정한 채 태양 pitch=-7과 -3을 비교했다. -3에서 주변 구름이 원본 최종 장면의 따뜻한 색감에 가까워져 채택했다. PIE 복제본에서만 Movable로 비교했고 종료 후 원본 Stationary 상태는 유지하며 pitch=-3/yaw=-40을 저장했다. 저장·레벨 재로드·DefaultPawn PIE·실제 UI 확인 후 재생을 종료했다. 원본 각도 직접 판독이 아닌 시각적 근사이며 중심 가림과 정확한 색상 차이는 아직 남아 있다.


## 현재 콘텐츠 저장·의존성 감사

최근 조명/위치 교정 후 현재 AssetRegistry를 다시 검사했다. 소유 에셋32개: World7, Material9, MaterialInstanceConstant9, MaterialFunction2, Texture2D4, StaticMesh1이다. 직접 hard/soft/management/searchable 참조와 머티리얼/함수 표현식 클래스에 MCP·Toolset 참조가 없었다. 32개 루트에서 hard/soft 참조64개 패키지 재귀 검사도 금지 참조0건이다. 미저장 콘텐츠 패키지와 맵 패키지는 모두0개였다. 기존 직접 검사기의29개 고정 가정을 현재 인스턴스 분리 결과32개로 갱신했다. 결과는 final_asset_dependencies.json, transitive_asset_dependencies.json, current_asset_inventory.json에 기록했다. 패키징/플러그인 비활성 실행 검증은 하지 않았으며 외관 완료 판정도 아니다.


## 6강 누락된 높이 안개 액터 추가

현재6강 액터를 열거하여 VolumetricCloud/DirectionalLight/SkyAtmosphere/SkyLight/반구/PlayerStart만 있고 원본 Outliner의 ExponentialHeightFog와 PostProcessVolume이 없음을 확인했다. Study_ExponentialHeightFog를 원점에 추가했다. FogDensity=.02, FogHeightFalloff=.2, FogMaxOpacity=1은 엔진 기본값이며 원본 직접 수치는 아니다. 생성 뒤 volumetric_fog 속성 조회는 해당 Python 속성이 없어 실패했지만 액터 생성은 이미 완료된 상태였다. 재생성하지 않고 별도 읽기로 액터1개와 밀도를 확인했다. 저장·레벨 재로드·DefaultPawn PIE·UI를 확인하고 PIE를 종료했다. 기본 안개가 원본의 전체 대기 조건과 일치한다는 뜻은 아니며 PostProcessVolume/다른강좌 안개 대조가 남아 있다.
