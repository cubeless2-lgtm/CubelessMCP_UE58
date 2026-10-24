#include "Registration/CubelessLocalFacadeDefaults.h"
#include "Registration/CubelessOwnedToolsetIdentity.h"
#include "Async/Future.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/CoreMisc.h"
#include "Templates/Atomic.h"
#include "Serialization/JsonSerializer.h"
#include "ToolsetRegistry/Toolset.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "ToolsetRegistry/ToolsetRegistrySubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace Cubeless::Registration
{
namespace
{
using FToolset=UE::ToolsetRegistry::FToolset;
using FOutput=TValueOrError<FString,FString>;
constexpr const TCHAR* Names[] = {
    TEXT("cubeless_mcp_ue58.registration.discovery_toolset.CubelessDiscoveryTools"),
    TEXT("cubeless_mcp_ue58.registration.toolsets.CubelessPythonDiagnostics")};
constexpr const TCHAR* Paths[] = {
    TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/discovery_toolset_PY.CubelessDiscoveryTools"),
    TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/toolsets_PY.CubelessPythonDiagnostics")};
const TArray<FString> Methods[] = {
    {TEXT("list_toolsets"),TEXT("describe_toolset"),TEXT("call_tool"),TEXT("manage_tools")},
    {TEXT("health"),TEXT("get_operation"),TEXT("execute_command_json"),TEXT("get_python_api_info_json"),TEXT("get_mcp_capabilities")}};
struct FState
{
    FState(int32 InRole,UClass* InClass):Role(InRole),Class(InClass){}
    int32 Role;
    TStrongObjectPtr<UClass> Class;
    TArray<TStrongObjectPtr<UFunction>> Functions;
    TWeakObjectPtr<UToolsetRegistrySubsystem> Subsystem;
    TSharedPtr<FToolset> Original,Wrapper;
    FString Schema,Version,Description;
    TAtomic<bool> Active{false};
    TAtomic<int32> InFlight{0};
    bool Preparing=false;
};
TSharedPtr<FState> Owned[2];
bool Busy=false;
struct FBusy {FBusy(){Busy=true;}~FBusy(){Busy=false;}};

FString Json(const TSharedRef<FJsonObject>& Object)
{FString Text;FJsonSerializer::Serialize(Object,TJsonWriterFactory<>::Create(&Text));return Text;}
bool Live(const UObject* Object)
{return IsValid(Object)&&!Object->HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed);}
int32 RoleOf(UClass* Class)
{
    if(!Live(Class)){return INDEX_NONE;}
    for(int32 Role=0;Role<2;++Role){if(Class->GetPathName().Equals(Paths[Role],ESearchCase::CaseSensitive)){return Role;}}
    return INDEX_NONE;
}
bool Current(const TSharedRef<FState>& S)
{
    UClass* C=S->Class.Get();
    if(!Live(C)||C->HasAnyClassFlags(CLASS_NewerVersionExists)||RoleOf(C)!=S->Role
        ||FindObject<UClass>(nullptr,Paths[S->Role])!=C||!C->IsChildOf(UToolsetDefinition::StaticClass())
        ||S->Functions.Num()!=Methods[S->Role].Num()||!S->Original.IsValid()||S->Original->GetToolsetClass()!=C){return false;}
    for(int32 I=0;I<S->Functions.Num();++I)
    {
        UFunction* F=S->Functions[I].Get();
        if(!Live(F)||F->GetOuter()!=C||C->FindFunctionByName(FName(*Methods[S->Role][I]))!=F){return false;}
    }
    return true;
}
bool CapturedOwner(const TSharedRef<FState>& S,const TSharedPtr<FToolset>& Handler)
{
    auto Now=UToolsetRegistrySubsystem::Get();
    return Now.HasValue()&&Now.GetValue().Get()==S->Subsystem.Get()
        &&HasRetainedToolsetIdentity(Now.GetValue()->ToolsetRegistry,Handler)&&Current(S);
}

// JSON is parsed into a private copy. Only the explicitly named defaults/null branches change.
bool PatchSchema(int32 Role,const FString& Physical,FString& Logical,FString& Error)
{
    TSharedPtr<FJsonObject> Root;const TArray<TSharedPtr<FJsonValue>>* Tools=nullptr;FString Name;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Physical),Root)||!Root.IsValid()
        ||!Root->TryGetStringField(TEXT("name"),Name)||!Name.Equals(Names[Role],ESearchCase::CaseSensitive)
        ||!Root->TryGetArrayField(TEXT("tools"),Tools)||Tools->Num()!=Methods[Role].Num())
    {Error=TEXT("Exact original local facade schema required");return false;}
    TSet<FString> Seen;
    for(const auto& Value:*Tools)
    {
        if(!Value.IsValid()||Value->Type!=EJson::Object){Error=TEXT("Invalid facade tool schema");return false;}
        const auto Tool=Value->AsObject();const FString Prefix=FString(Names[Role])+TEXT(".");
        if(!Tool->TryGetStringField(TEXT("name"),Name)||!Name.StartsWith(Prefix,ESearchCase::CaseSensitive))
        {Error=TEXT("Foreign facade method schema");return false;}
        const FString Method=Name.Mid(Prefix.Len());
        if(!Methods[Role].Contains(Method)||Seen.Contains(Method)){Error=TEXT("Unexpected/duplicate facade method");return false;}Seen.Add(Method);
        TArray<FString> Empty;
        if(Role==0&&(Method==TEXT("list_toolsets")||Method==TEXT("describe_toolset"))){Empty.Add(TEXT("query"));}
        if(Role==0&&Method==TEXT("manage_tools")){Empty={TEXT("tool_name"),TEXT("category")};}
        if(Role==1&&Method==TEXT("execute_command_json")){Empty.Add(TEXT("params_json"));}
        const FString Nullable=Role==0?(Method==TEXT("call_tool")?TEXT("arguments"):(Method==TEXT("manage_tools")?TEXT("enabled"):TEXT(""))):TEXT("");
        if(Empty.IsEmpty()&&Nullable.IsEmpty()){continue;}
        const TSharedPtr<FJsonObject>* Input=nullptr;const TSharedPtr<FJsonObject>* Props=nullptr;
        if(!Tool->TryGetObjectField(TEXT("inputSchema"),Input)||!(*Input)->TryGetObjectField(TEXT("properties"),Props))
        {Error=TEXT("Facade input properties unavailable");return false;}
        for(const FString& Key:Empty)
        {
            const TSharedPtr<FJsonObject>* Field=nullptr;FString Type;
            if(!(*Props)->TryGetObjectField(Key,Field)||!(*Field)->TryGetStringField(TEXT("type"),Type)||Type!=TEXT("string"))
            {Error=TEXT("Expected physical FString default carrier unavailable");return false;}
            (*Field)->SetStringField(TEXT("default"),TEXT(""));
        }
        if(!Nullable.IsEmpty())
        {
            const TSharedPtr<FJsonObject>* Field=nullptr;FString Type;
            const FString Expected=Nullable==TEXT("arguments")?TEXT("object"):TEXT("boolean");
            if(!(*Props)->TryGetObjectField(Nullable,Field)||!(*Field)->TryGetStringField(TEXT("type"),Type)||Type!=Expected)
            {Error=TEXT("Expected physical nullable carrier unavailable");return false;}
            auto OriginalBranch=MakeShared<FJsonObject>();OriginalBranch->Values=(*Field)->Values;
            OriginalBranch->RemoveField(TEXT("default"));
            auto NullBranch=MakeShared<FJsonObject>();NullBranch->SetStringField(TEXT("type"),TEXT("null"));
            auto Union=MakeShared<FJsonObject>();
            Union->SetArrayField(TEXT("anyOf"),{MakeShared<FJsonValueObject>(OriginalBranch),MakeShared<FJsonValueObject>(NullBranch)});
            Union->SetField(TEXT("default"),MakeShared<FJsonValueNull>());
            (*Props)->SetObjectField(Nullable,Union);
        }
        const TArray<TSharedPtr<FJsonValue>>* Required=nullptr;
        if((*Input)->TryGetArrayField(TEXT("required"),Required))
        {
            TArray<TSharedPtr<FJsonValue>> Kept;
            for(const auto& V:*Required)
            {if(!V.IsValid()||V->Type!=EJson::String){Error=TEXT("Malformed required list");return false;}
             if(!Empty.Contains(V->AsString())&&!V->AsString().Equals(Nullable,ESearchCase::CaseSensitive)){Kept.Add(V);}}
            if(Kept.IsEmpty()){(*Input)->RemoveField(TEXT("required"));}else{(*Input)->SetArrayField(TEXT("required"),Kept);}
        }
    }
    Logical=Json(Root.ToSharedRef());return true;
}

bool Normalize(int32 Role,const FString& Method,const FString& Input,FString& Output,FString& Error)
{
    TSharedPtr<FJsonObject> Args;
    // Preserve logical objects/booleans/numeric lexemes; never quote FString public argument contents.
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Input),Args,FJsonSerializer::EFlags::StoreNumbersAsStrings)||!Args.IsValid())
    {Error=TEXT("Local facade arguments require a JSON object");return false;}
    TArray<FString> Empty;
    if(Role==0&&(Method==TEXT("list_toolsets")||Method==TEXT("describe_toolset"))){Empty.Add(TEXT("query"));}
    if(Role==0&&Method==TEXT("manage_tools")){Empty={TEXT("tool_name"),TEXT("category")};}
    if(Role==1&&Method==TEXT("execute_command_json")){Empty.Add(TEXT("params_json"));}
    for(const FString& Key:Empty)
    {
        const auto V=Args->TryGetField(Key);
        if(V.IsValid()&&V->Type!=EJson::String){Error=TEXT("Empty-default string fields require strings, not null");return false;}
        if(!V.IsValid()){Args->SetStringField(Key,TEXT(""));}
    }
    const FString Nullable=Role==0?(Method==TEXT("call_tool")?TEXT("arguments"):(Method==TEXT("manage_tools")?TEXT("enabled"):TEXT(""))):TEXT("");
    if(!Nullable.IsEmpty())
    {
        const auto V=Args->TryGetField(Nullable);
        const EJson Type=Nullable==TEXT("arguments")?EJson::Object:EJson::Boolean;
        if(V.IsValid()&&V->Type!=EJson::Null&&V->Type!=Type){Error=TEXT("Nullable facade argument has the wrong JSON type");return false;}
        // The installed generated optional function initializes omitted values to its Python None.
        if(V.IsValid()&&V->Type==EJson::Null){Args->RemoveField(Nullable);}
    }
    Output=Json(Args.ToSharedRef());return true;
}

class FLocalFacadeDefaults final:public FToolset
{
public:
    explicit FLocalFacadeDefaults(const TSharedRef<FState>& InState):State(InState){}
    FString GetToolsetName() const override{return Names[State->Role];}
    FString GetToolsetVersion() const override{return State->Version;}
    FString GetToolsetDescription() const override{return State->Description;}
    UClass* GetToolsetClass() const override{return State->Class.Get();}
protected:
    FString GetJsonSchemaInternal() const override{return State->Schema;}
    TFuture<FOutput> ExecuteToolInternal(const FString& Method,const FString& Input) override
    {
        auto Reject=[](const FString& Error){return MakeFulfilledPromise<FOutput>(MakeError(Error)).GetFuture();};
        if(!IsInGameThread()||IsEngineExitRequested()||Busy||State->Preparing||!State->Active.Load()
            ||!Methods[State->Role].Contains(Method)||!CapturedOwner(State,State->Wrapper))
        {return Reject(TEXT("Local facade defaults owner/class unavailable; no function dispatched"));}
        FString Packed,Error;if(!Normalize(State->Role,Method,Input,Packed,Error)){return Reject(Error);}
        const auto CallState=State;const auto Original=State->Original;const auto Wrapper=State->Wrapper;
        ++State->InFlight;State->Preparing=true;
        auto* Subsystem=State->Subsystem.Get();
        Original->SetNameFilters(Subsystem->ToolsetRegistry.GetBlockedNames(),Subsystem->ToolsetRegistry.GetAllowedNames());
        State->Preparing=false;
        if(!State->Active.Load()||!CapturedOwner(State,Wrapper)||!Wrapper->IsToolEnabled(FString(Names[State->Role])+TEXT(".")+Method)
            ||!Original->IsToolEnabled(FString(Names[State->Role])+TEXT(".")+Method))
        {Original->SetEnabled(false);--State->InFlight;return Reject(TEXT("Local facade owner/filter/class changed before dispatch; no function dispatched"));}
        // Exactly one public handler delegation. No RPC, new dispatch table, wait, retry or cancellation.
        Original->SetEnabled(true);
        auto Future=Original->ExecuteTool(Method,Packed);
        // A nested different discovery method is valid. Never reject it just because another future exists.
        Original->SetEnabled(false);
        return Future.Next([CallState,Original,Wrapper](FOutput&& Result)->FOutput
        {--CallState->InFlight;return MoveTemp(Result);});
    }
private:TSharedRef<FState> State;
};

FString Report(const TSharedPtr<FState>& S,bool Ok,const FString& Error,bool Restored=false,bool Removed=false)
{
    auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("ready"),Ok);R->SetBoolField(TEXT("verified"),Ok);
    R->SetStringField(TEXT("error"),Error);R->SetBoolField(TEXT("asset_mutation"),false);
    R->SetBoolField(TEXT("actual_named_wire_verified"),false);R->SetBoolField(TEXT("original_handler_restored"),Restored);
    R->SetBoolField(TEXT("cleanup_required"),S.IsValid());
    // Python must never perform name-only generic class removal after this attachment.
    R->SetBoolField(TEXT("class_unregister_allowed"),false);
    R->SetBoolField(TEXT("owned_handler_removed"),Removed);
    R->SetNumberField(TEXT("delegated_calls_in_flight"),S.IsValid()?S->InFlight.Load():0);
    R->SetStringField(TEXT("scope"),TEXT("Fixed Discovery/Diagnostics default/null logical admission only; same original method/future/output. Physical class schema remains physical."));
    if(S.IsValid()){R->SetStringField(TEXT("logical_schema_json"),S->Schema);R->SetBoolField(TEXT("active"),S->Active.Load());}
    return Json(R);
}
bool Remove(int32 Role,bool RestoreOriginal,FString& Error,bool& Restored,bool& Removed)
{
    if(!Owned[Role].IsValid()){return true;}
    const auto S=Owned[Role];S->Active.Store(false);
    if(S->InFlight.Load()!=0){Error=TEXT("Local facade future ownership retained; no cancellation or class removal");return false;}
    auto* Subsystem=UObjectInitialized()?S->Subsystem.Get():nullptr;
    if(Subsystem)
    {
        if(S->Wrapper.IsValid()&&HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry,S->Wrapper))
        {if(!Subsystem->ToolsetRegistry.UnregisterToolset(S->Wrapper)){Error=TEXT("Owned wrapper removal failed");return false;}}
        if(S->Wrapper.IsValid()&&HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry,S->Wrapper))
        {Error=TEXT("Owned wrapper remains after removal callback");return false;}
        if(RestoreOriginal&&Current(S.ToSharedRef()))
        {
            if(HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry,S->Original))
            {
                // Initial installation may have failed before detaching the disabled original.
                // Reapply actual user filters, then close callbacks on that exact retained identity.
                S->Original->SetNameFilters(Subsystem->ToolsetRegistry.GetBlockedNames(),Subsystem->ToolsetRegistry.GetAllowedNames());
                Restored=HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry,S->Original)&&Current(S.ToSharedRef());
            }
            else
            {
                S->Original->SetEnabled(true);
                // Name-only SDK registration refuses foreign entries instead of replacing them.
                const bool Registered=Subsystem->ToolsetRegistry.RegisterToolset(S->Original);
                Restored=Registered&&HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry,S->Original)&&Current(S.ToSharedRef());
            }
            if(!Restored){S->Original->SetEnabled(false);Error=TEXT("Original handler restore not confirmed; foreign entry preserved and owner retained");return false;}
        }
        else
        {
            // Python unregister requests exact owned cleanup, never a generic name-only removal.
            // Stale classes also cannot be restored. Foreign replacements remain untouched.
            if(HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry,S->Original))
            {if(!Subsystem->ToolsetRegistry.UnregisterToolset(S->Original)){Error=TEXT("Exact owned original removal failed");return false;}}
            if((S->Wrapper.IsValid()&&HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry,S->Wrapper))
                ||HasRetainedToolsetIdentity(Subsystem->ToolsetRegistry,S->Original))
            {Error=TEXT("Owned facade entry remains after removal callback");return false;}
            Removed=true;
        }
    }
    else {Removed=true;} // Subsystem already gone: no registry entry is removed by name.
    S->Wrapper.Reset();S->Original.Reset();Owned[Role].Reset();return true;
}
}

FString InstallLocalFacadeDefaults(UClass* ToolsetClass)
{
    const int32 Role=RoleOf(ToolsetClass);FString Error;
    if(!IsInGameThread()||IsEngineExitRequested()||Busy||Role==INDEX_NONE){return Report(nullptr,false,TEXT("Idle GT and exact fixed facade class required"));}
    FBusy Lock;
    if(Owned[Role].IsValid())
    {
        const bool Ok=Owned[Role]->Class.Get()==ToolsetClass&&Owned[Role]->Active.Load()&&CapturedOwner(Owned[Role].ToSharedRef(),Owned[Role]->Wrapper);
        return Report(Owned[Role],Ok,Ok?TEXT(""):TEXT("Existing owner unavailable; explicit cleanup required"));
    }
    // Capture class/function identities before any original SDK schema converter callback.
    const auto S=MakeShared<FState>(Role,ToolsetClass);
    for(const FString& Method:Methods[Role])
    {
        UFunction* F=ToolsetClass->FindFunctionByName(FName(*Method));
        if(!Live(F)||F->GetOuter()!=ToolsetClass){return Report(nullptr,false,TEXT("Exact owned facade functions required"));}
        S->Functions.Emplace(F);
    }
    auto Now=UToolsetRegistrySubsystem::Get();
    if(!Now.HasValue()){return Report(nullptr,false,TEXT("Official registry unavailable"));}
    S->Subsystem=Now.GetValue().Get();S->Original=Now.GetValue()->ToolsetRegistry.Find(Names[Role]);
    if(!S->Original.IsValid()||!Current(S)){return Report(nullptr,false,TEXT("Enabled exact existing facade handler required"));}
    Owned[Role]=S;
    S->Version=S->Original->GetToolsetVersion();S->Description=S->Original->GetToolsetDescription();
    S->Original->SetNameFilters({},{});
    const FString Physical=S->Original->GetJsonSchema();
    S->Original->SetNameFilters(Now.GetValue()->ToolsetRegistry.GetBlockedNames(),Now.GetValue()->ToolsetRegistry.GetAllowedNames());
    S->Original->SetEnabled(false);
    if(!CapturedOwner(S,S->Original)||!PatchSchema(Role,Physical,S->Schema,Error))
    {return Report(S,false,Error.IsEmpty()?TEXT("Facade definition/owner changed during schema callback"):Error);}
    S->Wrapper=MakeShared<FLocalFacadeDefaults>(S);
    if(!CapturedOwner(S,S->Original)||!Now.GetValue()->ToolsetRegistry.UnregisterToolset(S->Original))
    {return Report(S,false,TEXT("Exact original detach failed; explicit cleanup required"));}
    if(!Current(S)||!Now.GetValue()->ToolsetRegistry.RegisterToolset(S->Wrapper)||!CapturedOwner(S,S->Wrapper))
    {return Report(S,false,TEXT("Wrapper registration/current confirmation failed; explicit cleanup required"));}
    S->Active.Store(true);return Report(S,true,TEXT(""));
}

FString VerifyLocalFacadeDefaults(UClass* ToolsetClass)
{
    const int32 Role=RoleOf(ToolsetClass);
    if(!IsInGameThread()||Busy||Role==INDEX_NONE){return Report(nullptr,false,TEXT("Idle GT and exact fixed facade class required"));}
    FBusy Lock;const auto S=Owned[Role];
    const bool Ok=S.IsValid()&&S->Class.Get()==ToolsetClass&&S->Active.Load()&&CapturedOwner(S.ToSharedRef(),S->Wrapper);
    return Report(S,Ok,Ok?TEXT(""):TEXT("Exact active facade default attachment unavailable; verification never installs"));
}

FString RemoveLocalFacadeDefaults(UClass* ToolsetClass,bool RestoreOriginal)
{
    const int32 Role=RoleOf(ToolsetClass);
    if(!IsInGameThread()||Busy||Role==INDEX_NONE){return Report(nullptr,false,TEXT("Idle GT and exact fixed facade class required"));}
    FBusy Lock;FString Error;bool Restored=false,Removed=false;
    if(!Owned[Role].IsValid())
    {return Report(nullptr,false,TEXT("Uncaptured facade owner retained; no name-only class removal is authorized"));}
    if(Owned[Role].IsValid()&&Owned[Role]->Class.Get()!=ToolsetClass){return Report(Owned[Role],false,TEXT("Retained class identity differs"));}
    const bool Ok=Remove(Role,RestoreOriginal,Error,Restored,Removed);return Report(Owned[Role],Ok,Error,Restored,Removed);
}

void ShutdownLocalFacadeDefaults()
{
    for(auto& S:Owned){if(S.IsValid()){S->Active.Store(false);}}
    if(!IsInGameThread()||Busy){return;}
    FBusy Lock;for(int32 Role=0;Role<2;++Role){FString Error;bool Restored=false,Removed=false;Remove(Role,true,Error,Restored,Removed);}
}
}
