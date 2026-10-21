#include "Registration/CubelessUMGTextBindingLibrary.h"
#include "Compatibility/UE58/CubelessUMGTextBindingApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessUMGTextBindingLibrary::SetTextBlockBinding(const FString& WidgetName,const FString& TextBlockName,const FString& BindingProperty,const FString& BindingType)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::UMGTextBinding::SetTextBinding(WidgetName,TextBlockName,BindingProperty,BindingType),TEXT("set_text_block_binding"));
}
