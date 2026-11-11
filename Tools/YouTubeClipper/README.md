# YouTube 구간 저장

`YouTubeClipper.exe`를 더블클릭하면 실행됩니다. Python이나 FFmpeg를 따로 설치할 필요가 없습니다. 인터넷 연결은 필요합니다.

## 창에서 사용

1. 유튜브 영상 주소를 붙여 넣습니다.
2. 시작·종료 시간을 초, `분:초`, `시:분:초` 중 하나로 입력합니다. 소수 초도 가능합니다.
3. 저장 폴더를 지정하고 **영상 저장**을 누릅니다.
4. 완료 후 **완성 영상 열기** 또는 **저장 폴더 열기**를 누릅니다.

예를 들어 시작 `1`, 종료 `10`은 9초짜리 영상입니다. 화면 비율과 소리를 유지하며 H.264/AAC MP4로 저장합니다. 최대 높이는 다운로드할 영상의 해상도 상한이며 작은 원본을 확대하지 않습니다. 기본 최대 높이 1920은 세로 쇼츠에 맞춘 값입니다.

원본 전체를 임시 다운로드한 뒤 구간을 잘라냅니다. 긴 원본은 다운로드 시간과 임시 디스크 공간이 더 필요합니다. 완료·일반 오류 시 임시 원본을 정리합니다. 저장 중에는 창을 닫지 못하도록 되어 있습니다.

로그인, 연령·지역 제한, 진행 중인 라이브는 지원하지 않습니다. YouTube 측 변경에 따라 추출 라이브러리 업데이트가 필요할 수 있습니다. AI 요약·자동 자막·핵심 장면 선택 기능은 현재 포함하지 않습니다.

## Codex 또는 명령줄에서 조작

동일한 EXE에 옵션을 전달하면 창을 열지 않고 작업합니다. 진행·완료·오류를 UTF-8 JSON Lines로 표준 출력에 기록하고 성공은 종료 코드 0, 실패는 1을 반환합니다. 창 없이 실행되므로 자동화할 때는 프로세스 완료를 기다리고 출력을 수집하세요.

```powershell
$clipper = 'C:\Git\UnrealMCP5_8_3\UnrealMCP5_8_3\Tools\YouTubeClipper\YouTubeClipper.exe'
& $clipper --url 'https://www.youtube.com/shorts/W-5q1EarSLA' --start 1 --end 10 --max-height 1920 | Out-Host
```

- `--url`: 유튜브 주소. 생략하면 창을 엽니다.
- `--start`, `--end`: 시작·종료 시간. 기본값은 각각 1초, 10초입니다.
- `--output-dir`: 저장 폴더. 기본값은 EXE가 있는 폴더 아래 `work`입니다. EXE를 다른 폴더로 옮기면 그 위치 아래 `work`에 저장합니다.
- `--max-height`: 최대 영상 높이. 720, 1080, 1920, 2160 중 선택합니다.

완성 파일명에는 영상 ID, 구간, 실행 시각, 임의 식별자가 포함됩니다. 기존 파일을 덮어쓰지 않습니다. `complete` 이벤트의 `path`가 결과 파일의 절대 경로입니다.

## 소스 수정과 재빌드

`source/youtube_clipper.py`에 창, 입력 검증, 다운로드, 인코딩, 명령줄 실행 코드가 들어 있습니다. Codex가 이 파일을 수정하고 EXE를 다시 만들 수 있습니다. Unreal 프로젝트 파일은 사용하지 않습니다.

```powershell
$root = 'C:\Git\UnrealMCP5_8_3\UnrealMCP5_8_3\Tools\YouTubeClipper'
& "$root/build-env/Scripts/python.exe" -m pip install -r "$root/source/requirements.txt"
& "$root/build-env/Scripts/python.exe" "$root/source/build.py"
```

Git에는 소스·빌드 스크립트·의존성 목록·이 문서만 포함합니다. EXE, FFmpeg 바이너리, 가상 환경, 빌드 산출물, 로그, 저장 영상은 로컬 파일입니다.

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
