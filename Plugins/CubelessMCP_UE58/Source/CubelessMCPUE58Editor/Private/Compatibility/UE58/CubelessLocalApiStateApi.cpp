#include "CubelessLocalApiStateApi.h"

#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/EngineVersion.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FString Cubeless::UE58::ReadLocalApiState()
{
    const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    const bool bReady = IsInGameThread() && GIsEditor && GEditor && !IsEngineExitRequested();
    Result->SetBoolField(TEXT("success"), bReady);
    if (!bReady)
    {
        Result->SetStringField(TEXT("error"), TEXT("Local API requires the live Editor game thread"));
    }
    else
    {
        // This labels this loaded Editor module's metadata session, not a TCP server.
        static const FString SessionId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
        Result->SetStringField(TEXT("editor_session_id"), SessionId);
        Result->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());
        Result->SetBoolField(TEXT("module_loaded"), FModuleManager::Get().IsModuleLoaded(TEXT("CubelessMCPUE58Editor")));
        const TSharedRef<FJsonObject> Plugins = MakeShared<FJsonObject>();
        for (const TCHAR* Name : {TEXT("GameplayAbilities"), TEXT("StateTree"), TEXT("GameplayStateTree"), TEXT("Metasound"), TEXT("CubelessMCP_UE58")})
        {
            const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(Name);
            Plugins->SetBoolField(Name, Plugin.IsValid() && Plugin->IsEnabled());
        }
        Result->SetObjectField(TEXT("plugins"), Plugins);
    }
    FString Text;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
    FJsonSerializer::Serialize(Result, Writer);
    return Text;
}
