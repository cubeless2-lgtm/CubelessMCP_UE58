#include "Registration/CubelessUMGUtilityLibrary.h"
#include "Compatibility/UE58/CubelessUMGUtilityApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessUMGUtilityLibrary::AddCheckboxToWidget(const FString& ParamsJson)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::UMGUtility::AddCheckbox(ParamsJson),TEXT("add_checkbox_to_widget"));
}
FString UCubelessUMGUtilityLibrary::AddWidgetToViewport(const FString& WidgetName,int64 ZOrder)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::UMGUtility::ViewportReadiness(WidgetName,ZOrder),TEXT("add_widget_to_viewport"));
}
