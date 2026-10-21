#include "Registration/CubelessUMGAuthoringLibrary.h"
#include "Compatibility/UE58/CubelessUMGAuthoringApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessUMGAuthoringLibrary::CreateUMGWidgetBlueprint(const FString& WidgetName,const FString& ParentClass,const FString& Path)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::UMGAuthoring::Create(WidgetName,ParentClass,Path),TEXT("create_umg_widget_blueprint"));
}
FString UCubelessUMGAuthoringLibrary::AddTextBlockToWidget(const FString& WidgetName,const FString& TextBlockName,const FString& Text,const FString& PositionJson,const FString& SizeJson,int64 FontSize,const FString& ColorJson)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::UMGAuthoring::AddText(WidgetName,TextBlockName,Text,PositionJson,SizeJson,FontSize,ColorJson),TEXT("add_text_block_to_widget"));
}
FString UCubelessUMGAuthoringLibrary::AddButtonToWidget(const FString& WidgetName,const FString& ButtonName,const FString& Text,const FString& PositionJson,const FString& SizeJson,int64 FontSize,const FString& ColorJson,const FString& BackgroundColorJson)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::UMGAuthoring::AddButton(WidgetName,ButtonName,Text,PositionJson,SizeJson,FontSize,ColorJson,BackgroundColorJson),TEXT("add_button_to_widget"));
}
