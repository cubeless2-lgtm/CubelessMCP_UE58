#include "Registration/CubelessUMGInteractionLibrary.h"
#include "Compatibility/UE58/CubelessUMGInteractionApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessUMGInteractionLibrary::AddSlidersToWidget(const FString& WidgetName,const FString& SlidersJson)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::UMGInteraction::AddSliders(WidgetName,SlidersJson),TEXT("add_sliders_to_widget"));
}
FString UCubelessUMGInteractionLibrary::BindWidgetEvent(const FString& WidgetName,const FString& WidgetComponentName,const FString& EventName,const FString& FunctionName)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::UMGInteraction::BindEvent(WidgetName,WidgetComponentName,EventName,FunctionName),TEXT("bind_widget_event"));
}

FString UCubelessUMGInteractionLibrary::BindWidgetEventRaw(const FString& WidgetName,const FString& WidgetComponentName,const FString& EventName,const FString& FunctionName,const FString& NodePositionJson)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::UMGInteraction::BindEventRaw(WidgetName,WidgetComponentName,EventName,FunctionName,NodePositionJson),TEXT("bind_widget_event"));
}
