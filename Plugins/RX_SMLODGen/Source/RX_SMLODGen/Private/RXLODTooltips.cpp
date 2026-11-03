#include "RXLODTooltips.h"

namespace
{
    const TMap<FName, FString> ManualFields = { { TEXT("PercentTriangles"), TEXT("퍼센트 트라이앵글") }, { TEXT("PercentVertices"), TEXT("퍼센트 버텍스") }, { TEXT("MaxNumOfTriangles"), TEXT("최대 트라이앵글 수") }, { TEXT("MaxNumOfVerts"), TEXT("최대 버텍스 수") }, { TEXT("WeldingThreshold"), TEXT("결합 한계치") }, { TEXT("BaseLODModel"), TEXT("베이스 LOD") }, { TEXT("TerminationCriterion"), TEXT("종료 기준") } };
    const TMap<FString, FString> Labels = {
        { TEXT("LOD"), TEXT("Auto는 화면에서 차지하는 크기에 따라 LOD를 선택합니다. LOD 번호를 고르면 뷰포트에 해당 LOD를 강제로 표시합니다. 원본 에셋의 설정은 바뀌지 않습니다.") },
        { TEXT("커스텀"), TEXT("오른쪽 패널에서 보고 싶은 LOD 항목을 개별적으로 선택합니다. 메시 지오메트리나 LOD 전환 기준을 바꾸는 옵션은 아닙니다.") },
        { TEXT("임포스터 방향"), TEXT("2방향은 정면(+X)과 측면(+Y)의 고정 양면 카드 2장, 4방향은 0·45·90·135도 카드 4장을 사용합니다. 빌보드가 아니며 원본 크기와 위쪽 노멀을 유지합니다. 변경하면 마지막 임포스터 LOD를 교체합니다. 카드끼리 자기 그림자가 생기지 않도록 그림자 생성은 끕니다. 외부 조명과 그림자는 받으며 Lumen·AO에 의한 어두워짐은 남을 수 있습니다.") },
        { TEXT("머티리얼 슬롯"), TEXT("이 LOD의 해당 섹션에 연결할 기존 머티리얼 슬롯을 선택합니다. 다른 섹션의 머티리얼 자체를 바꾸지 않습니다. 미리보기 변경 내용 적용 후 결과를 확인하세요.") },
        { TEXT("강조"), TEXT("해당 LOD를 강제로 표시하고 이 섹션을 강조합니다. 확인을 위한 뷰포트 표시 기능이며 에셋에 저장되지 않습니다.") },
        { TEXT("격리"), TEXT("해당 LOD에서 이 섹션만 표시합니다. 해제하면 전체 섹션을 다시 표시합니다. 원본 지오메트리를 삭제하지 않습니다.") },
        { TEXT("그림자 드리우기"), TEXT("이 섹션이 그림자를 만들도록 설정합니다. 미리보기에 반영한 뒤 원본에 적용해야 실제 에셋에도 반영됩니다.") },
        { TEXT("충돌 활성화"), TEXT("이 섹션을 메시 충돌 계산에 포함합니다. 실제 효과는 메시의 충돌 복잡도 설정과 충돌에 사용하는 LOD에 따라 달라집니다.") },
        { TEXT("레이 트레이싱 표시"), TEXT("이 섹션을 레이 트레이싱 지오메트리에 포함합니다. 레이 트레이싱이 활성화된 렌더링 경로에서 사용됩니다.") },
        { TEXT("디스턴스 필드 라이팅 영향"), TEXT("이 섹션이 메시 디스턴스 필드를 사용하는 라이팅에 영향을 주도록 설정합니다. 해당 렌더링 기능과 디스턴스 필드 데이터가 필요합니다.") },
        { TEXT("불투명 강제"), TEXT("레이 트레이싱에서 이 섹션을 불투명 지오메트리로 처리합니다. 마스크나 투명 재질에서는 보이는 결과가 달라질 수 있습니다.") },
        { TEXT("감소 제외"), TEXT("이 섹션에는 감소를 적용하지 않고 LOD 0의 섹션 지오메트리를 유지합니다. 가중치 1과 같은 동작입니다. LOD 0 자체는 항상 원본을 보존합니다.") },
        { TEXT("열린 외곽 경계 보존"), TEXT("한 면에만 연결된 열린 가장자리의 정점을 고정합니다. 기본값은 꺼짐입니다. 다른 보호 옵션과 독립적으로 선택하며, 윤곽 보존으로 목표보다 삼각형이 많이 남을 수 있습니다. 결합 한계치는 0을 사용하세요.") },
        { TEXT("꺾인 모서리 보존"), TEXT("서로 평면이 아닌 두 면 사이 모서리의 정점을 고정합니다. 기본값은 꺼짐입니다. 켜면 곡면이나 복잡한 형상에서는 거의 줄어들지 않을 수 있습니다. 열린 경계 및 UV·노멀·머티리얼 경계 보호와 독립적으로 선택합니다.") },
        { TEXT("UV·노멀·머티리얼 경계 보존"), TEXT("UV·노멀 분할선과 머티리얼 섹션 경계의 정점을 고정합니다. 기본값은 꺼짐입니다. 열린 외곽 경계와 꺾인 모서리는 각각의 옵션으로 선택하세요. 서로 맞닿은 섹션은 가중치와 세 가지 보호 옵션이 같아야 합니다.") },
        { TEXT("종료 기준"), TEXT("Triangles는 삼각형 잔존 비율, Vertices는 버텍스 잔존 비율을 기준으로 줄입니다. First Percent Satisfied는 둘 중 먼저 만족한 조건에서 멈춥니다. 섹션 가중치는 현재 Triangles 비율 감소에만 적용할 수 있습니다.") },
        { TEXT("퍼센트 트라이앵글"), TEXT("남길 삼각형의 비율입니다. 100은 감소 없음, 50은 절반을 남기는 목표입니다. 실제 결과는 감소기의 제약에 따라 달라집니다. 가중치가 0.5이면 이 값으로 정한 감소량의 절반만 적용합니다.") },
        { TEXT("퍼센트 버텍스"), TEXT("남길 버텍스의 비율입니다. 100은 감소 없음, 50은 절반을 남기는 목표입니다. 종료 기준이 Vertices 또는 First Percent Satisfied일 때 사용합니다. 현재 섹션 가중치와 함께 사용할 수 없습니다.") },
        { TEXT("최대 트라이앵글 수"), TEXT("삼각형 비율 감소에서 남길 삼각형 수의 상한입니다. 4294967295는 사실상 상한을 두지 않는 기본값입니다. 섹션 가중치는 현재 기본 상한을 유지해야 합니다.") },
        { TEXT("최대 버텍스 수"), TEXT("버텍스 비율 감소에서 남길 버텍스 수의 상한입니다. 4294967295는 사실상 상한을 두지 않는 기본값입니다.") },
        { TEXT("결합 한계치"), TEXT("이 거리 안의 정점을 용접할 때 사용하는 임계값입니다. 단위는 메시의 로컬 좌표입니다. 서로 다른 섹션 가중치를 사용할 때는 경계 손상을 막기 위해 0을 사용해야 합니다.") },
        { TEXT("베이스 LOD"), TEXT("감소 계산의 입력으로 사용할 LOD 번호입니다. 현재 섹션 가중치 감소는 LOD 0을 입력으로 사용해야 합니다.") },
        { TEXT("LOD 그룹"), TEXT("엔진에 등록된 LOD 그룹 프리셋을 적용합니다. 그룹이 LOD 설정을 바꿀 수 있습니다. 가중치 또는 임포스터를 원본에 적용하려면 중복 감소를 막기 위해 None을 선택해야 합니다.") },
        { TEXT("LOD 임포트"), TEXT("FBX 또는 OBJ의 메시를 임시 사본의 LOD로 가져옵니다. LOD 1 이상을 선택하면 해당 LOD를 교체하고, Auto 또는 LOD 0에서는 새 LOD를 추가합니다. 원본에는 별도 적용이 필요합니다.") },
        { TEXT("LOD 개수 (임포스터 제외)"), TEXT("편집할 일반 LOD의 총개수입니다. LOD 0을 포함하며 임포스터는 별도로 마지막에 한 개 추가됩니다. 개수를 줄이면 사본에서 뒤쪽 LOD와 해당 가중치 설정을 제거합니다.") },
        { TEXT("LOD 거리 자동 계산"), TEXT("엔진이 LOD별 화면 크기 전환 기준을 자동 계산합니다. 켜면 화면 크기 전환 기준을 자동으로 사용합니다. 화면 크기를 직접 편집하면 자동 계산을 끄고 입력값을 유지합니다.") },
        { TEXT("LOD 스트리밍"), TEXT("이 메시의 LOD 스트리밍 정책을 선택합니다. Default는 프로젝트와 플랫폼의 기본 정책을 따릅니다. 실제 동작은 대상 플랫폼의 스트리밍 지원과 설정에 따라 달라집니다.") },
        { TEXT("마지막 LOD에 임포스터"), TEXT("선택한 2방향 또는 4방향의 고정 양면 평면과 캡처 아틀라스로 임포스터를 만들어 표시 메시의 마지막 LOD에 넣습니다. 빌보드처럼 카메라를 따라 회전하지 않습니다. 다시 생성해도 마지막 임포스터는 하나만 유지됩니다.") },
        { TEXT("임포스터 다시 생성 (마지막 LOD 교체)"), TEXT("현재 사본으로 다시 캡처하여 기존 임포스터 LOD와 표시용 머티리얼을 교체합니다. 일반 LOD는 유지하며 임포스터 LOD를 추가로 쌓지 않습니다.") },
        { TEXT("원본에 적용"), TEXT("현재 미리보기의 LOD, 섹션 가중치 결과와 임포스터를 실제 원본 에셋에 반영합니다. 설정 변경이 있으면 먼저 미리보기를 갱신합니다. 디스크 저장은 하지 않으므로 원본과 생성된 에셋을 확인 후 함께 저장하세요.") },
        { TEXT("변경 내용 적용"), TEXT("편집한 설정으로 임시 사본과 뷰포트만 다시 빌드합니다. 실제 원본 에셋에는 뷰포트의 원본에 적용 버튼을 눌러야 반영됩니다.") },
        { TEXT("미리보기 변경 내용 적용"), TEXT("LOD 설정과 섹션 가중치로 표시 메시를 다시 생성하고, 임포스터가 켜져 있으면 마지막 LOD도 교체합니다. 실제 원본을 변경하거나 저장하지 않습니다.") },
        { TEXT("LOD 제거"), TEXT("이 LOD를 임시 사본에서 제거하고 뒤쪽 LOD 번호를 당깁니다. 원본에는 별도 적용이 필요합니다. LOD 0은 제거할 수 없습니다.") },
        { TEXT("JSON 불러오기"), TEXT("저장된 RX LOD JSON을 읽어 임시 편집 설정을 복원합니다. 에셋과 지오메트리 식별자가 맞아야 하며, 가져온 뒤 미리보기 변경 내용 적용으로 확인합니다. 지오메트리와 텍스처 픽셀은 JSON에 포함되지 않습니다.") },
        { TEXT("저장"), TEXT("현재 RX 편집 설정을 사용 중인 JSON 파일에 저장합니다. 처음 저장할 때는 경로를 지정합니다. 원본 uasset이나 생성된 텍스처를 저장하는 버튼은 아닙니다.") },
        { TEXT("내보내기"), TEXT("현재 RX 편집 설정을 새 경로의 JSON 파일로 저장합니다. LOD 설정, 섹션 가중치, 머티리얼 슬롯과 임포스터 사용 여부를 보관하며 AI 작업에서도 같은 형식을 사용합니다.") },
        { TEXT("고정 노출"), TEXT("뷰포트의 자동 노출 적응을 끄고 EV100 값으로 밝기를 고정합니다. 원본 머티리얼이나 프로젝트 렌더링 설정은 바꾸지 않습니다.") },
        { TEXT("노출 (EV100)"), TEXT("RX 뷰포트의 고정 노출값입니다. 값을 높이면 어두워지고 낮추면 밝아집니다. 미리보기 표시 전용이며 원본 에셋이나 JSON LOD 설정에 저장되지 않습니다.") },
        { TEXT("LOD 선택 툴"), TEXT("뷰포트에 표시할 LOD와 오른쪽 패널에서 편집할 LOD 목록을 선택합니다.") },
        { TEXT("섹션"), TEXT("각 LOD의 머티리얼 섹션과 렌더링·충돌 옵션, 감소 가중치를 편집합니다. 가중치의 그룹 번호는 LOD 0의 소스 Polygon Group을 가리킵니다.") },
        { TEXT("화면 크기"), TEXT("이 LOD로 전환할 화면 점유율입니다. 값이 작을수록 일반적으로 더 멀리서 이 LOD가 사용됩니다. 기본값과 플랫폼별 재정의를 직접 편집할 수 있습니다. 자동 계산 중에 편집하면 수동 모드로 전환하고 다른 LOD의 계산값은 유지합니다.") },
        { TEXT("빌드 세팅"), TEXT("노멀, 탄젠트, UV, 스케일, 디스턴스 필드 등 이 LOD를 빌드하는 방법을 설정합니다. 변경 내용 적용으로 사본을 다시 빌드한 뒤 결과를 확인하세요.") },
        { TEXT("감소 세팅"), TEXT("해당 LOD의 삼각형·버텍스 감소 목표와 감소기의 세부 옵션입니다. 비율은 줄이는 양이 아니라 남기는 양입니다. 섹션 가중치가 있으면 지원되는 조건으로 감소량을 조절합니다.") },
        { TEXT("LOD 설정"), TEXT("전체 메시의 LOD 그룹, LOD 임포트, 최소 LOD, 스트리밍과 LOD 개수 및 화면 크기 자동 계산을 설정합니다.") },
        { TEXT("임포스터"), TEXT("일반 LOD 뒤에 하나의 임포스터 LOD를 생성합니다. 비대칭 모델의 반대면은 같은 촬영 이미지를 재사용하므로 방향별 외형을 확인하세요.") }
    };
    const TMap<FName, FString> Properties = {
        { TEXT("MinLOD"), TEXT("사용할 최소 LOD 번호입니다. 이 번호보다 정밀한 LOD는 사용하지 않습니다. 플랫폼별 재정의도 지정할 수 있습니다.") },
        { TEXT("MinQualityLevelLOD"), TEXT("품질 수준별 최소 LOD 번호입니다. 프로젝트에서 품질별 최소 LOD 기능을 사용하는 경우에 적용됩니다.") },
        { TEXT("NoRefStreamingLODBias"), TEXT("레퍼런스가 없는 메시를 미리 로드할 때의 스트리밍 LOD 바이어스입니다. -1은 플랫폼 기본값을 사용합니다. 품질 수준별로 재정의할 수 있습니다.") },
        { TEXT("NumStreamedLODs"), TEXT("스트리밍할 수 있는 LOD 수입니다. -1은 기본 정책을 따릅니다. 플랫폼별 재정의와 LOD 스트리밍 정책을 함께 확인하세요.") },
        { TEXT("MaxDeviation"), TEXT("감소한 표면이 원래 표면에서 벗어날 수 있는 최대 거리입니다. 단위는 메시 로컬 좌표입니다. 현재 섹션 가중치 경로에서는 0만 허용합니다.") },
        { TEXT("PixelError"), TEXT("이 LOD에 허용할 화면상의 오차를 픽셀 단위로 지정합니다. 지원되는 감소기와 자동 LOD 전환 계산에서 사용됩니다.") },
        { TEXT("HardAngleThreshold"), TEXT("면 사이 각도가 이 값보다 크면 단단한 모서리로 처리하는 기준 각도입니다. 단위는 도입니다. 감소기가 지원할 때 사용합니다.") },
        { TEXT("SilhouetteImportance"), TEXT("감소할 때 외곽선 보존을 얼마나 중요하게 취급할지 지정합니다. 값이 높을수록 외형의 윤곽 보존을 우선합니다. 감소기 지원 여부에 따라 적용됩니다.") },
        { TEXT("TextureImportance"), TEXT("감소할 때 UV와 텍스처 늘어짐 방지를 얼마나 중요하게 취급할지 지정합니다. 감소기가 지원할 때 사용합니다.") },
        { TEXT("ShadingImportance"), TEXT("감소할 때 노멀과 음영의 변화를 얼마나 억제할지 지정합니다. 감소기가 지원할 때 사용합니다.") },
        { TEXT("VertexColorImportance"), TEXT("감소할 때 버텍스 컬러 보존을 얼마나 중요하게 취급할지 지정합니다. 감소기가 지원할 때 사용합니다.") },
        { TEXT("bRecalculateNormals"), TEXT("감소 과정에서 노멀을 다시 계산하도록 요청합니다. 빌드 세팅의 노멀 재계산과는 별도이며 감소기의 지원 여부에 따라 적용됩니다.") },
        { TEXT("bGenerateUniqueLightmapUVs"), TEXT("감소기가 지원하는 경우 고유한 라이트맵 UV 생성을 요청합니다. 빌드 세팅의 라이트맵 UV 생성 옵션과 구분하세요.") },
        { TEXT("bKeepSymmetry"), TEXT("감소 과정에서 모델의 대칭을 보존하도록 요청합니다. 이 기능을 지원하는 감소기에서만 효과가 있습니다.") },
        { TEXT("bVisibilityAided"), TEXT("가시성 정보를 활용한 감소를 요청합니다. 이 기능을 지원하는 감소기에서만 효과가 있습니다.") },
        { TEXT("bCullOccluded"), TEXT("가려진 지오메트리 제거를 요청합니다. 감소기의 가시성 지원과 설정에 따라 적용되므로 외형 손실을 확인하세요.") },
        { TEXT("VisibilityAggressiveness"), TEXT("가시성 기반 감소에서 샘플링의 적극성을 정합니다. 값이 높으면 샘플 수가 줄어듭니다. 해당 기능을 지원하는 감소기에서 사용합니다.") },
        { TEXT("bUseMikkTSpace"), TEXT("MikkTSpace 방식으로 탄젠트 공간을 계산합니다. 노멀맵 제작에 사용한 탄젠트 방식과 일치시키는 데 사용합니다.") },
        { TEXT("bRecomputeNormals"), TEXT("가져온 노멀을 사용하지 않고 메시를 빌드할 때 노멀을 다시 계산합니다. 임포스터의 위쪽 고정 노멀은 보존해야 하므로 생성된 임포스터에서는 이 값을 끕니다.") },
        { TEXT("bRecomputeTangents"), TEXT("가져온 탄젠트를 사용하지 않고 빌드할 때 다시 계산합니다. UV와 노멀에 따라 노멀맵 음영이 달라질 수 있습니다.") },
        { TEXT("bComputeWeightedNormals"), TEXT("노멀을 계산할 때 면적과 코너 각도로 가중치를 줍니다. 큰 면의 방향을 더 잘 반영하는 노멀을 만들 때 사용합니다.") },
        { TEXT("bRemoveDegenerates"), TEXT("면적이 거의 없거나 정점이 겹친 축퇴 삼각형을 빌드 중 제거합니다.") },
        { TEXT("bBuildReversedIndexBuffer"), TEXT("반전된 인덱스 버퍼를 추가 생성해 미러 변환의 렌더링을 최적화합니다. 인덱스 버퍼 메모리가 증가합니다.") },
        { TEXT("bUseHighPrecisionTangentBasis"), TEXT("노멀과 탄젠트 기저를 더 높은 정밀도로 저장합니다. 음영 정밀도가 좋아질 수 있지만 버텍스 데이터 메모리가 증가합니다.") },
        { TEXT("bUseFullPrecisionUVs"), TEXT("UV를 32비트 부동소수점 정밀도로 저장합니다. UV 정밀도 문제를 줄일 수 있지만 메모리가 증가합니다.") },
        { TEXT("bUseBackwardsCompatibleF16TruncUVs"), TEXT("이전 버전 메시와 호환되는 16비트 UV 절삭 변환을 사용합니다. 기존 에셋의 UV 표현을 유지해야 할 때 사용합니다.") },
        { TEXT("bGenerateLightmapUVs"), TEXT("빌드할 때 라이트맵용 UV를 생성합니다. 소스와 목적지 라이트맵 인덱스 및 최소 라이트맵 해상도를 함께 설정하세요.") },
        { TEXT("bGenerateDistanceFieldAsIfTwoSided"), TEXT("모든 삼각형을 양면처럼 취급해 디스턴스 필드를 생성합니다. 열린 메시의 필드 생성에 도움이 되지만 디스턴스 필드 AO 품질이 낮아질 수 있습니다.") },
        { TEXT("bSupportFaceRemap"), TEXT("물리 머티리얼 마스크에 필요한 면 인덱스 매핑 데이터를 빌드합니다. 해당 기능을 사용할 때 활성화합니다.") },
        { TEXT("MinLightmapResolution"), TEXT("라이트맵 UV를 배치할 때 사용하는 최소 해상도입니다. 낮은 해상도에서도 UV 섬 사이에 필요한 패딩을 확보하는 기준입니다.") },
        { TEXT("SrcLightmapIndex"), TEXT("라이트맵 UV 생성의 입력으로 사용할 UV 채널 번호입니다. 채널 번호는 0부터 시작합니다.") },
        { TEXT("DstLightmapIndex"), TEXT("생성한 라이트맵 UV를 기록할 UV 채널 번호입니다. 채널 번호는 0부터 시작합니다.") },
        { TEXT("BuildScale3D"), TEXT("메시를 빌드할 때 로컬 X·Y·Z 축에 적용할 배율입니다. 액터의 배치 스케일과 별도이며 지오메트리와 바운드에 영향을 줍니다.") },
        { TEXT("DistanceFieldResolutionScale"), TEXT("메시 디스턴스 필드 볼륨의 해상도 배율입니다. 1은 기본값이며 높이면 품질과 메모리 비용이 함께 증가할 수 있습니다.") },
        { TEXT("DistanceFieldReplacementMesh"), TEXT("이 메시 대신 디스턴스 필드 생성에 사용할 다른 Static Mesh를 지정합니다. 실제 렌더링 지오메트리를 교체하는 옵션은 아닙니다.") },
        { TEXT("MaxLumenMeshCards"), TEXT("이 메시에서 생성할 Lumen 메시 카드의 최대 수입니다. 많으면 표면 커버리지가 좋아질 수 있지만 런타임 비용이 증가합니다. 0은 카드 생성을 끕니다.") }
    };
}

FText RXLODTooltips::Label(const FString& Name)
{
    if (const FString* Found = Labels.Find(Name)) { return FText::FromString(*Found); }
    if (Name.StartsWith(TEXT("리듀스율"))) { return FText::FromString(TEXT("줄일 비율입니다. 0%는 감소 없음, 50%는 절반을 남기는 목표입니다. 고급 탭의 남길 삼각형/버텍스 비율과 같은 값을 반대로 표시합니다. 종료 기준이 둘 중 먼저 만족이면 삼각형과 버텍스를 각각 편집합니다. 미리보기 변경 내용 적용 후 결과를 확인하세요.")); }
    if (Name == TEXT("스크린 사이즈")) { return FText::FromString(TEXT("이 LOD로 전환할 화면 크기 기본값입니다. 0 이상의 값을 입력하세요. 자동 계산 중에 직접 편집하면 수동 모드로 바뀝니다. 고급 탭의 플랫폼별 재정의는 유지합니다. 생성된 임포스터 LOD는 자동 전환값을 표시합니다.")); }
    if (Name.StartsWith(TEXT("파일에서 LOD"))) { return Label(TEXT("LOD 임포트")); }
    if (Name.StartsWith(TEXT("섹션 가중치"))) { return FText::FromString(TEXT("섹션을 보존할 정도를 나타내는 0~1 가중치입니다. 50%를 남기는 설정에서 0은 50%, 0.5는 75%, 1은 100%를 남기는 목표입니다. 기본값 0은 기존 감소를 그대로 적용하고 1은 완전히 보존합니다. LOD 0은 원본이므로 가중치를 편집하지 않습니다. 서로 다른 가중치의 맞닿은 섹션은 현재 거부됩니다.")); }
    if (Name.StartsWith(TEXT("LOD "))) { return FText::FromString(TEXT("이 LOD의 옵션을 표시합니다. 체크 목록에서는 오른쪽 패널 표시 여부만 바꾸며 뷰포트의 강제 LOD 선택과는 별도입니다. 표시된 삼각형·버텍스 수는 마지막으로 빌드한 미리보기 기준입니다.")); }
    if (Name.StartsWith(TEXT("섹션 "))) { return Labels.Contains(TEXT("섹션")) ? FText::FromString(Labels[TEXT("섹션")]) : FText(); }
    return FText::GetEmpty();
}

bool RXLODTooltips::HasProperty(const FName& Owner, const FName& Name)
{
    if (Owner == TEXT("RXLODScreenSizeSettings")) { return Name == TEXT("Default") || Name == TEXT("PerPlatform"); }
    return Properties.Contains(Name) || ManualFields.Contains(Name) || ((Owner == TEXT("PerPlatformFloat") || Owner == TEXT("PerPlatformInt") || Owner == TEXT("PerQualityLevelInt")) && (Name == TEXT("Default") || Name == TEXT("PerPlatform") || Name == TEXT("PerQuality")));
}

FText RXLODTooltips::Property(const FName& Owner, const FName& Name)
{
    if (Owner == TEXT("RXLODScreenSizeSettings")) { return FText::FromString(Name == TEXT("Default") ? TEXT("이 LOD의 화면 크기 기본값입니다. 자동 계산 중에는 엔진 렌더 데이터의 실제 전환 기준을 표시하며, 자동 계산 중에도 직접 편집할 수 있으며 편집하면 자동 계산을 끄고 입력값을 유지합니다. 모든 LOD의 기본값이 같은 상수인 것은 아닙니다.") : TEXT("플랫폼 이름별 화면 크기 재정의입니다. 해당 플랫폼에 항목이 없으면 화면 크기 기본값을 사용합니다.")); }
    if (const FString* Found = Properties.Find(Name)) { return FText::FromString(*Found); }
    if (const FString* Found = ManualFields.Find(Name)) { return Label(*Found); }
    if (Owner == TEXT("PerPlatformFloat")) { return Label(TEXT("화면 크기")); }
    if (Owner == TEXT("PerPlatformInt") || Owner == TEXT("PerQualityLevelInt")) { return FText::FromString(TEXT("기본값과 플랫폼 또는 품질별 재정의 값입니다. 재정의가 없는 환경에서는 기본값을 사용합니다.")); }
    return FText::GetEmpty();
}
