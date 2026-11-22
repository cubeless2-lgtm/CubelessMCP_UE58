# 퀴니 모캡 Control Rig 편집

준비한 편집 시퀀스는 `/Game/CubelessMocap/KaXLIZbdnek_236_246_20261115/LS_KaXLIZbdnek_Quinn_EditRig`, 다시 저장한 기본 애니메이션은 같은 폴더의 `AN_KaXLIZbdnek_Quinn_EditRig_Base`입니다. 원본 및 HandFront_v4는 보존했습니다. 애님 블루프린트나 원본 Control Rig Blueprint는 수정하지 않습니다.

표준 엔진 FK Control Rig을 사용합니다. 골반·척추·팔·다리·손가락을 본별 컨트롤로 편집할 수 있습니다. `CR_Mannequin_Body` 세션도 별도 `LS_KaXLIZbdnek_Quinn_ControlRig_v2`에 보관했지만, 기본 IK는 이번 앉은 동작의 무릎을 변경하므로 원본 보존용 기본 편집본으로 채택하지 않았습니다. FK 편집본은 손 위치를 옮길 때 관련 팔 컨트롤도 함께 조정해야 합니다.

편집본은 Quinn 스폰 객체와 원본 참조 트랙(베이크 뒤 비활성), 리그 키 트랙으로 구성됩니다.30fps/300표본,0~299프레임, 길이9.9667초입니다. 소스 트랙은 끝 오프셋을 한 표시 프레임만큼 연장하여 마지막 표본에서 처음 자세로 반복되지 않게 했습니다. 베이크 시 절차적 Post Process Anim BP를 끄고, 내보낼 때 Quinn 리타깃 소스 정보를 설정합니다. 결과는 Quinn 체형에 베이크되므로 원본 RAW 트랙의 완전 동일 복사는 아닙니다.

## Computer Use 없이 편집하기

표준 `unreal.ControlRigSequencerLibrary`와 동봉된 `Tools/control_rig_edit.py`를 사용합니다. PIE/다른 모캡 계산이 없는 상태에서 편집할 시퀀스를 먼저 열고, 다음 Editor tick 이후 컨트롤을 조작합니다. 모캡 창은 그대로 둘 수 있습니다. MCP로 새 에셋 에디터를 열 때는 `CubelessPythonExecutionTools.execute_python(..., transaction_policy='script', interchange_policy='none')`을 사용해 에디터 전환을 감싸는 외부 트랜잭션을 피합니다. `execute_unreal_python`에는 transaction_policy 인수가 없습니다. 불확실한 호출을 자동 반복하지 말고 실제 시퀀스/키/저장 상태를 확인합니다.

새 보정은 편집 시퀀스를 별도 이름으로 복제한 뒤 적용합니다. 예시는 복제본에서3~5초 동안 spine_01_CONTROL을5도 회전하고 양끝0.5초를 이어줍니다. 실제 축과 각도는 원하는 자세에 맞춰 결정합니다.

```python
import sys, unreal
from pathlib import Path
sys.path.insert(0,str(Path(unreal.Paths.project_dir())/'Plugins/CubelessBodyMocap/Tools'))
import control_rig_edit as edit

# 이 복제본이 현재 시퀀서에서 열린 것을 먼저 확인합니다.
sequence = unreal.load_asset('/Game/YourFolder/LS_Quinn_EditCopy')
edit.offset_rotation(sequence,'spine_01_CONTROL',3.,5.,pitch=5.,ease_seconds=.5)
output=edit.export_animation(sequence,
    '/Game/CubelessMocap/KaXLIZbdnek_236_246_20261115/AN_KaXLIZbdnek_Quinn_EditRig_Base',
    '/Game/YourFolder/AN_Quinn_WaistFixed')
print(output.get_path_name())
```

시험본에서는105·120·135프레임에 약5도,0·60·90·150·180·299프레임에0도 차이를 확인했습니다. 원본을 바꾸지 않고 새 AnimSequence에 반영됐습니다. 기본 편집본에는 이 시험 보정을 적용하지 않았습니다.300표본,161개 트랙, 동일 길이/RateScale을 확인했습니다. 재시작 후 helper로 spine_02_CONTROL의6~7초2도 보정과 새 출력도 검증했습니다.

Quinn의 실제 재생과 같은 압축 평가에서161개 본 전체의 최대 위치 차이는0.0306cm 미만, 로컬 회전 차이는0.175도 미만이었습니다. RAW 평가에서는 주요 본 위치 차이가 최대 약0.14cm이고 일부 보조 본·손가락 값도 달랐습니다. Native 베이크가 재생 포즈와 대상 체형을 사용하므로 원본 RAW 키의 동일 복사로 해석하지 않습니다. 다른 영상·캐릭터의 무손실 왕복은 미검증입니다.

이 경로는 Editor 전용이며 표준 ControlRig/LevelSequence/Python Editor Script Plugin이 필요합니다. 저장된 AnimSequence에는 MCP나 편집 리그 실행 의존성을 추가하지 않습니다. 새 런타임 패키징 검증은 이번 작업 범위에서 하지 않았습니다.
