# YouTube 구간 저장

`YouTubeClipper.exe`를 더블클릭하면 실행됩니다. Python이나 FFmpeg를 따로 설치할 필요가 없습니다. 유튜브 다운로드에는 인터넷 연결이 필요하며, 로컬 영상 변환은 인터넷 없이 사용할 수 있습니다.

## 창에서 사용

1. 유튜브 영상 주소를 붙여 넣습니다.
2. 시작·종료 시간을 초, `분:초`, `시:분:초` 중 하나로 입력합니다. 소수 초도 가능합니다.
3. **최종 출력 해상도**, **전체 회전**, **해상도 맞춤 방식**을 선택합니다.
4. 저장 폴더를 지정하고 **영상 저장**을 누릅니다.
5. 완료 후 **완성 영상 열기** 또는 **저장 폴더 열기**를 누릅니다.

예를 들어 시작 `1`, 종료 `10`은 9초짜리 영상입니다. 소리를 유지하며 H.264/AAC MP4로 저장합니다. **다운로드 최대 높이**는 YouTube에서 받을 원본의 상한이고, **최종 출력 해상도**는 완성 파일의 크기입니다. 서로 별개이며, 지정한 출력 크기가 원본보다 크면 확대됩니다.

- 최종 출력 해상도: 원본 유지, 프리셋 또는 `가로x세로` 직접 입력. 각 변은 2~8192 범위의 짝수입니다.
- 전체 회전: 시계방향 `0`, `90`, `180`, `270`도. 영상 프레임 전체에 적용합니다.
- 회전부터 적용한 뒤 최종 해상도로 맞춥니다. `원본 유지`에서 90/270도는 가로·세로가 바뀝니다.
- **여백 추가**: 비율을 유지하고 남는 공간을 검정으로 채웁니다. 기본값입니다.
- **잘라 채우기**: 비율을 유지하면서 중앙 기준으로 잘라 지정 크기를 채웁니다.
- **늘려 맞추기**: 지정 크기에 맞춰 가로·세로를 늘립니다. 원래 비율이 바뀔 수 있습니다.
- **영상 선택** 버튼으로 로컬 MP4/MOV/MKV/WebM/AVI도 변환할 수 있습니다. 직접 파일 경로를 입력해도 됩니다. 이 경우 다운로드하지 않으며 `다운로드 최대 높이`는 적용하지 않습니다. 로컬 입력은 길이 정보가 있는 파일을 지원합니다.

예: 세로 영상을 가로로 돌려 1280x720으로 만들려면 회전 `90`, 최종 해상도 `1280x720`을 지정합니다. 기존 방향 정보가 있는 영상은 표시 방향을 먼저 반영하고 선택한 추가 회전을 적용합니다. H.264 규격에 맞춰 원본 크기의 홀수 변에는 최대 1픽셀 여백을 추가합니다.

원본 전체를 임시 다운로드한 뒤 구간을 잘라냅니다. 긴 원본은 다운로드 시간과 임시 디스크 공간이 더 필요합니다. 완료·일반 오류 시 임시 원본을 정리합니다. 저장 중에는 창을 닫지 못하도록 되어 있습니다.

로그인, 연령·지역 제한, 진행 중인 라이브는 지원하지 않습니다. YouTube 측 변경에 따라 추출 라이브러리 업데이트가 필요할 수 있습니다. AI 요약·자동 자막·핵심 장면 선택 기능은 현재 포함하지 않습니다.

## Codex 또는 명령줄에서 조작

동일한 EXE에 옵션을 전달하면 창을 열지 않고 작업합니다. 진행·완료·오류를 UTF-8 JSON Lines로 표준 출력에 기록하고 성공은 종료 코드 0, 실패는 1을 반환합니다. 창 없이 실행되므로 자동화할 때는 프로세스 완료를 기다리고 출력을 수집하세요.

```powershell
$clipper = 'C:\Git\UnrealMCP5_8_3\UnrealMCP5_8_3\Tools\YouTubeClipper\YouTubeClipper.exe'
& $clipper --url 'https://www.youtube.com/shorts/W-5q1EarSLA' --start 1 --end 10 --max-height 1920 | Out-Host
```

- `--url`: 유튜브 주소. `--input-file`과 함께 사용할 수 없습니다. 둘 다 생략하면 창을 엽니다.
- `--input-file`: 로컬 동영상 경로. 인터넷 연결 없이 구간·회전·크기를 변환합니다.
- `--start`, `--end`: 시작·종료 시간. 기본값은 각각 1초, 10초입니다.
- `--output-dir`: 저장 폴더. 기본값은 EXE가 있는 폴더 아래 `work`입니다. EXE를 다른 폴더로 옮기면 그 위치 아래 `work`에 저장합니다.
- `--max-height`: 다운로드 원본의 최대 높이. 720, 1080, 1920, 2160 중 선택합니다.
- `--resolution`: 최종 `가로x세로`. 예: `1280x720`, `1080x1920`. 기본값 `original`.
- `--rotation`: 시계방향 0/90/180/270. 기본값 0.
- `--resize-mode`: `fit`(여백), `fill`(중앙 잘라 채우기), `stretch`(늘리기). 기본값 `fit`.

```powershell
& $clipper --url 'https://youtu.be/W-5q1EarSLA' --start 1 --end 10 --rotation 90 --resolution 1280x720 --resize-mode fit | Out-Host
& $clipper --input-file 'C:\Videos\input.mp4' --start 0 --end 5 --rotation 270 --resolution 720x1280 | Out-Host
```

완성 파일명에는 영상 ID, 구간, 실행 시각, 임의 식별자가 포함됩니다. 기존 파일을 덮어쓰지 않습니다. `complete` 이벤트의 `path`가 결과 파일의 절대 경로입니다.

## 사용자와 Codex가 함께 수정하기

- **창에서 조절**: 해상도·회전·맞춤 방식을 바꾸고 **설정 저장**을 누르면 EXE 옆 `settings.json`에 저장됩니다. 다음 GUI 실행 시 불러옵니다.
- **JSON 직접 편집**: 메모장 등으로 `settings.json`을 수정할 수 있습니다. 출력 폴더를 상대 경로로 쓰면 도구 폴더를 기준으로 합니다. 예시는 `source/settings.example.json`에 있습니다. CLI는 JSON 기본값을 읽지 않고 명령줄 옵션을 사용합니다.
- **기능 수정**: `source/youtube_clipper.py`의 `RESOLUTION_PRESETS`(목록), `output_options`(검증), `video_filters`(FFmpeg 변환 순서), `encode_clip`(인코딩), `gui`(화면)를 수정합니다. EXE를 변경하려면 아래 재빌드를 실행합니다.

```json
{
  "output_dir": "work",
  "max_height": 1920,
  "resolution": "1280x720",
  "rotation": 90,
  "resize_mode": "fit"
}
```

설정 파일은 선택 사항입니다. EXE만 복사해도 기본값으로 작동하며, GUI에서 설정 저장 시 새 파일을 만듭니다. 설정 파일이 잘못되면 경고하고 기본값으로 시작하며 원본 설정 파일은 유지합니다.

## 소스 수정과 재빌드

`source/youtube_clipper.py`에 창, 입력 검증, 다운로드, 인코딩, 명령줄 실행 코드가 들어 있습니다. Codex가 이 파일을 수정하고 EXE를 다시 만들 수 있습니다. Unreal 프로젝트 파일은 사용하지 않습니다.

```powershell
$root = 'C:\Git\UnrealMCP5_8_3\UnrealMCP5_8_3\Tools\YouTubeClipper'
& "$root/build-env/Scripts/python.exe" -m pip install -r "$root/source/requirements.txt"
& "$root/build-env/Scripts/python.exe" "$root/source/build.py"
```

Git에는 소스·테스트·설정 예시·빌드 스크립트·의존성 목록·이 문서만 포함합니다. EXE, FFmpeg 바이너리, 가상 환경, 빌드 산출물, 로그, 저장 영상은 로컬 파일입니다.

`build-env`, `build`는 개발용이고, 일상적인 사용에는 EXE만 있으면 됩니다. 새 컴퓨터에서 재빌드하려면 Windows와 Python 3.12 이상이 필요합니다. 저장소를 받은 뒤 아래 명령으로 구성합니다. `$root`는 실제 도구 폴더로 바꿉니다.

```powershell
$root = 'C:\Git\UnrealMCP5_8_3\UnrealMCP5_8_3\Tools\YouTubeClipper'
python -m venv "$root/build-env"
& "$root/build-env/Scripts/python.exe" -m pip install -r "$root/source/requirements.txt"
& "$root/build-env/Scripts/python.exe" "$root/source/build.py"
```

재빌드 시 의존성 패키지에서 FFmpeg 바이너리를 가져와 EXE에 포함합니다. EXE만 다른 폴더로 복사해도 사용할 수 있습니다.

## 구성 요소

- yt-dlp: https://github.com/yt-dlp/yt-dlp (Unlicense; Windows 빌드에 포함되는 종속 라이브러리에는 별도 라이선스가 적용될 수 있음)
- imageio-ffmpeg: https://github.com/imageio/imageio-ffmpeg (BSD-2-Clause)
- FFmpeg: https://ffmpeg.org/ (포함 바이너리의 빌드 구성은 `ffmpeg -version`으로 확인 가능)
- Python / Tcl-Tk / PyInstaller: 각 프로젝트의 라이선스 적용

공개 재배포 시 포함 구성 요소의 라이선스 및 FFmpeg 소스 제공 조건을 확인해야 합니다.

## 오프라인 동작 검증

인터넷 없이 색상 구역과 소리가 있는 테스트 영상을 생성해 네 방향의 실제 픽셀 배치, 출력 크기, 여백/잘라 채우기/늘리기, 회전 정보가 있는 입력, 설정 JSON을 확인합니다.

```powershell
& "$root/build-env/Scripts/python.exe" -m unittest discover -s "$root/source" -p test_output_options.py -v
```
