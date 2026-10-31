#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessStudyChooserLibrary.generated.h"

/** Editor authoring only; generated Choosers retain standard engine references. */
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessStudyChooserLibrary : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 /** Remap references only inside a newly duplicated, unsaved owned Study Chooser.
  * Does not save, overwrite, delete, compile or provide atomic rollback. */
 UFUNCTION(BlueprintCallable, Category="Cubeless|Internal", meta=(AIIgnore="true"))
 static FString RemapNewStudyChooser(UObject* Target, const TMap<UObject*, UObject*>& Replacements);
};
