#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/EngineTypes.h"
#include "PerQualityLevelProperties.h"
#include "RXSMLODGenLibrary.generated.h"

USTRUCT()
struct FRXLODScreenSizeSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, Category="화면 크기", meta=(DisplayName="화면 크기 디폴트", ClampMin="0", ToolTip="이 LOD로 전환할 기본 화면 점유율입니다. 자동 계산 중에도 편집할 수 있으며, 직접 편집하면 수동 모드로 전환해 입력값을 유지합니다.")) float Default = 1.0f;
    UPROPERTY(EditAnywhere, Category="화면 크기", meta=(DisplayName="플랫폼별 화면 크기", ToolTip="특정 플랫폼에서 사용할 화면 점유율 재정의입니다. 없는 플랫폼은 화면 크기 기본값을 사용합니다.")) TMap<FName, float> PerPlatform;
};

USTRUCT()
struct FRXLODCommonSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, Category="LOD 설정", meta=(DisplayName="최소 LOD", ToolTip="사용할 최소 LOD 번호입니다. 플랫폼별로 다른 값을 지정할 수 있습니다.")) FPerPlatformInt MinLOD;
    UPROPERTY(EditAnywhere, Category="LOD 설정", meta=(DisplayName="품질별 최소 LOD", ToolTip="프로젝트가 품질 수준별 최소 LOD를 사용하는 경우에 적용할 값입니다.")) FPerQualityLevelInt MinQualityLevelLOD;
    UPROPERTY(EditAnywhere, Category="LOD 설정", meta=(DisplayName="레퍼런스 없는 스트리밍 LOD 바이어스", ToolTip="레퍼런스 없는 메시를 미리 로드할 때의 LOD 바이어스입니다. -1은 플랫폼 기본값입니다.")) FPerQualityLevelInt NoRefStreamingLODBias;
    UPROPERTY(EditAnywhere, Category="LOD 설정", meta=(DisplayName="스트리밍되는 LOD 수", ToolTip="스트리밍 가능한 LOD 수입니다. -1은 기본 정책을 사용합니다. 플랫폼별 재정의도 지정할 수 있습니다.")) FPerPlatformInt NumStreamedLODs;
};

/** Editor-only JSON interface. Never referenced by generated meshes/materials. */
UCLASS()
class RX_SMLODGEN_API URXSMLODGenLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** JSON operations: describe, open, get, set, rebuild, apply, close.
        set only edits the transient session; apply requires confirm=true. No implicit disk saves. */
    UFUNCTION(BlueprintCallable, Category="RX SMLODGen")
    static FString ExecuteRequest(const FString& RequestJson);
};
