// GX source candidate: Blueprint Niagara local/inherited template inspection and guarded asset swap.
#include "Compatibility/UE58/CubelessBlueprintNiagaraTemplateSwapApi.h"
#include <type_traits>
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraDataInterface.h"
#include "NiagaraParameterStore.h"
#include "NiagaraTypes.h"
#include "NiagaraUserRedirectionParameterStore.h"
#include "BlueprintCompilationManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Modules/ModuleManager.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/ReferencerFinder.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "UObject/GarbageCollection.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/Archive.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "Misc/Crc.h"
#include "HAL/FileManager.h"
namespace Cubeless::UE58::BlueprintNiagaraTemplateSwap
{
namespace
{
using Variable::FResult;
bool Running=false;
bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
bool Safe(){return IsInGameThread()&&!IsEngineExitRequested()&&!GExitPurge&&!IsGarbageCollecting()&&!GIsSavingPackage&&Live(GEditor);}
bool Same(const FString& A,const FString& B){return A.Equals(B,ESearchCase::CaseSensitive);}
bool Forbidden(const FString& S){auto L=S.ToLower();return L.Contains(TEXT("unrealmcp"))||L.Contains(TEXT("cubelessmcp"))||L.Contains(TEXT("modelcontextprotocol"))||L.Contains(TEXT("toolsetregistry"));}
FString Json(const TSharedPtr<FJsonObject>& O){FString T;if(O)FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&T));return T;}
struct FIdentity
{
 TStrongObjectPtr<UObject> Hold{nullptr};TWeakObjectPtr<UObject> Object,Outer;TWeakObjectPtr<UClass> Class;TWeakObjectPtr<UPackage> Package,Effective;FString Path;bool DirtyBefore=false,Retired=false;
 explicit FIdentity(UObject* O):Hold(O),Object(O),Outer(O->GetOuter()),Class(O->GetClass()),Package(O->GetOutermost()),Effective(O->GetPackage()),Path(O->GetPathName()),DirtyBefore(O->GetOutermost()->IsDirty()){}
 bool Exact()const{auto* O=Object.Get();return Retired||(Live(O)&&Live(Class.Get())&&Live(Package.Get())&&O->GetClass()==Class.Get()&&O->GetOuter()==Outer.Get()&&O->GetOutermost()==Package.Get()&&O->GetPackage()==Effective.Get()&&Same(O->GetPathName(),Path));}
};
struct FBlueprintBinding{TWeakObjectPtr<UBlueprint> Blueprint;TWeakObjectPtr<UBlueprintGeneratedClass> Class;TWeakObjectPtr<USimpleConstructionScript> SCS,RootSCS;bool Reinstancing=false;};

struct FSCSBinding{TWeakObjectPtr<USimpleConstructionScript> SCS;TWeakObjectPtr<UBlueprintGeneratedClass> Owner;TArray<TWeakObjectPtr<USCS_Node>> Nodes;bool Retired=false;};
struct FTemplateBinding{TWeakObjectPtr<USCS_Node> Node;TWeakObjectPtr<USimpleConstructionScript> SCS;TWeakObjectPtr<UActorComponent> Direct,Actual;TWeakObjectPtr<UNiagaraSystem> Asset;TWeakObjectPtr<UBlueprintGeneratedClass> ActualClass;FName Variable;bool Retired=false;};
struct FCanaryReplacement;
struct FWork
{
 FCanaryReplacement* ReplacementLease=nullptr;TFunction<bool()> CanaryReplacementCurrent;
 FResult R;TSharedRef<FJsonObject> Counts=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Calls,SaveRows,CreationRows,RetiredRows;TArray<TUniquePtr<FIdentity>> Objects;TArray<FBlueprintBinding> Blueprints;TArray<FSCSBinding> SCSTopology;TArray<FTemplateBinding> Templates;
 TWeakObjectPtr<UWorld> World;TWeakObjectPtr<UEditorEngine> Editor;bool WorldCaptured=false,WorldWasNull=false,Writer=false;TWeakObjectPtr<UBlueprint> Target,Source;TWeakObjectPtr<UNiagaraSystem> SourceSystem,Replacement;bool SourceDirty=false,SourceProtected=false;FString RequestedObject,RequestedPackage,RequestedFilename,SelectedVariable,FailedStage;TWeakObjectPtr<UNiagaraComponent> SelectedTemplate;int64 Slots=0,Chars=0;
 explicit FWork(bool Write):Writer(Write){R.Compiled=false;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),false);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("semantic_restore_available"),false);R.Data->SetBoolField(TEXT("game_runtime_dependency_added"),false);}
 bool Fail(const TCHAR* Code,const FString& Message){if(R.Error.IsEmpty()){R.ErrorCode=Code;R.Error=Message;FailedStage=R.Stage;}R.bSuccess=false;return false;}
 bool Budget(int64 N,int64 Text=0){if(N<0||Text<0||N>262144-Slots||Text>32ll*1024*1024-Chars)return Fail(TEXT("blueprint_niagara_template_budget"),TEXT("Bounded capture/output budget exceeded; actual prefix retained"));Slots+=N;Chars+=Text;return true;}
 bool Admit(const FString& Stage){R.Stage=Stage;if(!R.Error.IsEmpty())return false;if(!Safe()||(WorldCaptured&&Editor.Get()!=GEditor))return Fail(TEXT("blueprint_niagara_template_context_lost"),TEXT("Editor/GC/exit/save context unsafe; no further live observation"));auto* Now=GEditor->GetEditorWorldContext().World();if(Now&&(!Live(Now)||Now->bIsTearingDown))return Fail(TEXT("blueprint_niagara_template_world_unavailable"),TEXT("Editor world tearing down"));if(!WorldCaptured){Editor=GEditor;World=Now;WorldWasNull=Now==nullptr;WorldCaptured=true;}else if((!WorldWasNull&&!Live(World.Get()))||World.Get()!=Now)return Fail(TEXT("blueprint_niagara_template_world_changed"),TEXT("Captured Editor world changed"));if(Writer&&(GEditor->PlayWorld||GEditor->GetPIEWorldContext()))return Fail(TEXT("blueprint_niagara_template_PIE_unsupported"),TEXT("Stop PIE/SIE before template writes"));for(const auto& I:Objects)if(!I->Exact())return Fail(TEXT("blueprint_niagara_template_identity_changed"),TEXT("Exact object/class/outer/outermost/effective package/path changed"));for(const auto& B:Blueprints)if(!B.Reinstancing&&(B.Blueprint->GeneratedClass!=B.Class.Get()||B.Class->ClassGeneratedBy!=B.Blueprint.Get()||B.Class->SimpleConstructionScript!=B.SCS.Get()||B.Blueprint->SimpleConstructionScript!=B.RootSCS.Get()))return Fail(TEXT("blueprint_niagara_template_class_scs_changed"),TEXT("Captured Blueprint class/SCS role changed"));for(const auto& S:SCSTopology)if(!S.Retired){auto* C=S.Owner.Get();auto* T=S.SCS.Get();if(!Live(C)||!Live(T)||C->SimpleConstructionScript!=T)return Fail(TEXT("blueprint_niagara_template_scs_owner_changed"),TEXT("Captured SCS owner changed"));const auto& Nodes=T->GetAllNodes();if(Nodes.Num()!=S.Nodes.Num())return Fail(TEXT("blueprint_niagara_template_scs_topology_changed"),TEXT("Captured physical SCS node count changed"));for(int32 I=0;I<Nodes.Num();++I)if(Nodes[I]!=S.Nodes[I].Get())return Fail(TEXT("blueprint_niagara_template_scs_topology_changed"),TEXT("Captured SCS node membership/order changed"));}for(const auto& T:Templates)if(!T.Retired){auto* N=T.Node.Get();auto* A=T.Actual.Get();if(!Live(N)||!Live(A)||N->GetOuter()!=T.SCS.Get()||N->ComponentTemplate!=T.Direct.Get()||N->GetVariableName()!=T.Variable)return Fail(TEXT("blueprint_niagara_template_node_role_changed"),TEXT("Exact Node/SCS/template/variable role changed"));if(auto* NC=Cast<UNiagaraComponent>(A))if(NC->GetAsset()!=T.Asset.Get())return Fail(TEXT("blueprint_niagara_template_assignment_changed"),TEXT("Captured template asset assignment changed outside owned transition"));}if(SourceProtected&&(!Live(Source.Get())||Source->GetPackage()->IsDirty()!=SourceDirty))return Fail(TEXT("blueprint_niagara_template_source_changed"),TEXT("Canary source dirty projection changed; no source save or replay"));if(CanaryReplacementCurrent&&!CanaryReplacementCurrent())return false;return true;}
 bool Pin(UObject* O){if(!Admit(TEXT("pin_object"))||!Live(O))return Fail(TEXT("blueprint_niagara_template_object_unavailable"),TEXT("Expected live public stock object"));for(const auto& I:Objects)if(I->Object.Get()==O&&!I->Retired)return I->Exact();if(!Budget(1,O->GetPathName().Len()))return false;Objects.Add(MakeUnique<FIdentity>(O));return true;}
 bool SDK(const TCHAR* Stage,bool Mutation=false){if(!Admit(Stage)||!Budget(1))return false;R.bMutationAttempted|=Mutation;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);double N=0;Counts->TryGetNumberField(Stage,N);Counts->SetNumberField(Stage,N+1);auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("stage"),Stage);J->SetBoolField(TEXT("entered"),true);J->SetBoolField(TEXT("returned"),false);J->SetBoolField(TEXT("mutation_attempted"),Mutation);Calls.Add(MakeShared<FJsonValueObject>(J));return true;}
 bool After(const TCHAR* Stage){if(!Calls.IsEmpty())Calls.Last()->AsObject()->SetBoolField(TEXT("returned"),true);return Admit(FString(TEXT("after_"))+Stage);}
 template<class F>bool Read(const TCHAR* Stage,F&& Fn){if(!SDK(Stage))return false;Fn();return After(Stage);}
 template<class F>bool Write(const TCHAR* Stage,F&& Fn){if(!SDK(Stage,true))return false;Fn();return After(Stage);}
 bool Blueprint(UBlueprint* B){if(!Pin(B)||!Live(B->GeneratedClass))return Fail(TEXT("blueprint_niagara_template_generated_class_missing"),TEXT("Current BlueprintGeneratedClass required"));auto* C=Cast<UBlueprintGeneratedClass>(B->GeneratedClass);if(!C||C->ClassGeneratedBy!=B||C->GetPackage()!=B->GetPackage()||!Pin(C))return Fail(TEXT("blueprint_niagara_template_generated_class_owner"),TEXT("Generated class is not the exact Blueprint product"));for(const auto& X:Blueprints)if(X.Blueprint.Get()==B)return true;FBlueprintBinding X;X.Blueprint=B;X.Class=C;X.SCS=C->SimpleConstructionScript;X.RootSCS=B->SimpleConstructionScript;if(X.RootSCS.IsValid()&&!Pin(X.RootSCS.Get()))return false;if(X.SCS.IsValid()&&!Pin(X.SCS.Get()))return false;Blueprints.Add(X);return Admit(TEXT("blueprint_role_captured"));}
 bool Idle(UBlueprint* B){if(!Admit(TEXT("writer_idle_admission")))return false;R.Data->SetBoolField(TEXT("external_transaction_observed"),GEditor->IsTransactionActive());if(B&&(B->bBeingCompiled||B->bQueuedForCompilation||!FBlueprintCompilationManager::IsGeneratedClassLayoutReady()))return Fail(TEXT("blueprint_niagara_template_compile_pending"),TEXT("Blueprint compile is pending; no pump/flush/replay"));for(auto* S:{SourceSystem.Get(),Replacement.Get()})if(S){bool Outstanding=false,Active=false;if(!Read(TEXT("NiagaraSystem_OutstandingPoint"),[&]{Outstanding=S->HasOutstandingCompilationRequests(true);}))return false;if(!Read(TEXT("NiagaraSystem_ActivePoint"),[&]{Active=S->HasActiveCompilations();}))return false;if(Outstanding||Active)return Fail(TEXT("blueprint_niagara_template_Niagara_compile_pending"),TEXT("Niagara compilation pending; cooperative wait is not provided by this original contract"));}return true;}
};
FWork* Current=nullptr;
bool ContinueRead(){return Current&&Current->Admit(TEXT("read_continuation"));}
TSharedPtr<FJsonObject> ErrorResponse(const FString& Message){auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),false);O->SetStringField(TEXT("error"),Message);return O;}
FString GetStringParam(const TSharedPtr<FJsonObject>& P,const TCHAR* K,const FString& Default){FString S=Default;if(P)P->TryGetStringField(K,S);return S;}
bool GetBoolParam(const TSharedPtr<FJsonObject>& P,const TCHAR* K,bool Default){bool B=Default;if(P)P->TryGetBoolField(K,B);return B;}
double GetNumberParam(const TSharedPtr<FJsonObject>& P,const TCHAR* K,double Default){double N=Default;if(P)P->TryGetNumberField(K,N);if(!FMath::IsFinite(N)||N<MIN_int32||N>MAX_int32){Current->Fail(TEXT("blueprint_niagara_template_numeric_range"),FString(K)+TEXT(" exceeds finite int32 storage range before original cast/clamp"));return Default;}return N;}
TArray<TSharedPtr<FJsonValue>> StringArrayToJsonArray(const TArray<FString>& A){TArray<TSharedPtr<FJsonValue>> R;for(const auto& S:A)R.Add(MakeShared<FJsonValueString>(S));return R;}
bool StringArrayContainsExact(const TArray<FString>& A,const FString& B){return A.ContainsByPredicate([&](const FString& X){return Same(X,B);});}
FString NormalizeEditorBlueprintObjectPath(const FString& InputPath);
FString NormalizeNiagaraSystemObjectPath(const FString& InputPath);

bool NoEmbeddedNul(const FString& S){for(int32 I=0;I<S.Len();++I)if(S[I]==0)return false;return true;}
struct FExactKeys:DefaultKeyFuncs<FString>
{static bool Matches(const FString& A,const FString& B){return A.Equals(B,ESearchCase::CaseSensitive);}static uint32 GetKeyHash(const FString& K){return FCrc::StrCrc32(*K);}};
bool Finite(const TSharedPtr<FJsonValue>& V,int32 Depth,int32& Count)
{if(!V.IsValid()||Depth>64||++Count>262144){return false;}if(V->Type==EJson::Number){double N=0;return V->TryGetNumber(N)&&FMath::IsFinite(N);}if(V->Type==EJson::String){return NoEmbeddedNul(V->AsString());}if(V->Type==EJson::Array){for(const auto& X:V->AsArray()){if(!Finite(X,Depth+1,Count)){return false;}}}else if(V->Type==EJson::Object){for(const auto& X:V->AsObject()->Values){if(!NoEmbeddedNul(FString(X.Key))||!Finite(X.Value,Depth+1,Count)){return false;}}}return V->Type!=EJson::None;}
bool Parse(const FString& Text,TSharedPtr<FJsonObject>& Out)
{
    if(Text.IsEmpty()||Text.Len()>1048576||!NoEmbeddedNul(Text)){return false;}
    int32 Depth=0;bool InString=false,Escape=false;for(int32 I=0;I<Text.Len();++I){const TCHAR C=Text[I];if(InString){if(Escape){Escape=false;}else if(C=='\\'){Escape=true;}else if(C=='"'){InString=false;}}else if(C=='"'){InString=true;}else if(C=='{'||C=='['){if(++Depth>64){return false;}}else if(C=='}'||C==']'){if(--Depth<0){return false;}}}if(InString||Depth!=0){return false;}
    auto Reader=TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]"));TArray<TSet<FString,FExactKeys>> Keys;TArray<bool> Objects;EJsonNotation Token;int32 Tokens=0;
    while(Reader->ReadNext(Token)){if(Token==EJsonNotation::Error||++Tokens>262144){return false;}if(Token!=EJsonNotation::ObjectEnd&&Token!=EJsonNotation::ArrayEnd&&!Objects.IsEmpty()&&Objects.Last()){const FString K(Reader->GetIdentifier());if(!NoEmbeddedNul(K)||Keys.Last().Contains(K)){return false;}Keys.Last().Add(K);}if(Token==EJsonNotation::ObjectStart||Token==EJsonNotation::ArrayStart){Objects.Add(Token==EJsonNotation::ObjectStart);Keys.AddDefaulted();}else if(Token==EJsonNotation::ObjectEnd||Token==EJsonNotation::ArrayEnd){if(Objects.IsEmpty()){return false;}Objects.Pop();Keys.Pop();}}
    TArray<TSharedPtr<FJsonValue>> A;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]")),A)||A.Num()!=1||!A[0].IsValid()||A[0]->Type!=EJson::Object){return false;}int32 Count=0;if(!Finite(A[0],0,Count)){return false;}Out=A[0]->AsObject();return Out.IsValid();
}




struct FStoreSnapshot
{
 TArray<FNiagaraVariableWithOffset> Variables;TArray<uint8> Bytes;TArray<UNiagaraDataInterface*> Interfaces;TArray<UObject*> UObjects;
 bool ParametersDirty=false,InterfacesDirty=false,UObjectsDirty=false,PositionDirty=false;uint32 Layout=0;
 TArrayView<const FNiagaraVariableWithOffset> ReadParameterVariables()const{return MakeArrayView(Variables);}
 const TArray<uint8>& GetParameterDataArray()const{return Bytes;}const TArray<UNiagaraDataInterface*>& GetDataInterfaces()const{return Interfaces;}const TArray<UObject*>& GetUObjects()const{return UObjects;}
 bool GetParametersDirty()const{return ParametersDirty;}bool GetInterfacesDirty()const{return InterfacesDirty;}bool GetUObjectsDirty()const{return UObjectsDirty;}bool GetPositionDataDirty()const{return PositionDirty;}uint32 GetLayoutVersion()const{return Layout;}
 bool Valid(const FNiagaraVariableWithOffset& P)const{const bool Good=P.IsDataInterface()?Interfaces.IsValidIndex(P.Offset):P.IsUObject()?UObjects.IsValidIndex(P.Offset):(P.Offset>=0&&P.GetSizeInBytes()>=0&&static_cast<int64>(P.Offset)+P.GetSizeInBytes()<=Bytes.Num());return Good||Current->Fail(TEXT("niagara_runtime_store_layout_invalid"),TEXT("Typed parameter offset is outside captured byte/object/DI snapshot."));}
 UNiagaraDataInterface* GetDataInterface(int32 I)const{return Interfaces.IsValidIndex(I)?Interfaces[I]:nullptr;}UObject* GetUObject(int32 I)const{return UObjects.IsValidIndex(I)?UObjects[I]:nullptr;}
 const uint8* GetParameterData(int32 I,int32 N)const{return I>=0&&N>=0&&static_cast<int64>(I)+N<=Bytes.Num()?Bytes.GetData()+I:nullptr;}
 template<typename T>T GetParameterValueFromOffset(int32 I)const{T V{};if(I<0||static_cast<int64>(I)+sizeof(T)>Bytes.Num()){Current->Fail(TEXT("niagara_runtime_typed_value_size_invalid"),TEXT("Typed value exceeds captured parameter bytes."));return V;}FMemory::Memcpy(&V,Bytes.GetData()+I,sizeof(T));return V;}
};
bool CaptureStore(const FNiagaraParameterStore& S,FStoreSnapshot& Out)
{
 auto& W=*Current;if(!W.SDK(TEXT("ParameterStore_Snapshot")))return false;TArray<FNiagaraVariableWithOffset> Variables;if(!W.Read(TEXT("ParameterStore_ReadVariables"),[&]{auto View=S.ReadParameterVariables();if(View.Num()>65536){W.Fail(TEXT("niagara_runtime_store_snapshot_budget"),TEXT("Declaration inventory exceeds bound."));return;}Variables.Append(View.GetData(),View.Num());}))return false;const int64 Count=Variables.Num(),ByteCount=S.GetParameterDataArray().Num(),DI=S.GetDataInterfaces().Num(),Obj=S.GetUObjects().Num();if(Count>65536||ByteCount>64ll*1024*1024||DI>65536||Obj>65536||!W.Budget(Count+DI+Obj))return W.Fail(TEXT("niagara_runtime_store_snapshot_budget"),TEXT("Store snapshot exceeds declaration/object/64MiB byte budget."));
 Out.Variables=MoveTemp(Variables);Out.Bytes=S.GetParameterDataArray();for(auto* O:S.GetDataInterfaces())Out.Interfaces.Add(O);for(UObject* O:S.GetUObjects())Out.UObjects.Add(O);Out.ParametersDirty=S.GetParametersDirty();Out.InterfacesDirty=S.GetInterfacesDirty();Out.UObjectsDirty=S.GetUObjectsDirty();Out.PositionDirty=S.GetPositionDataDirty();Out.Layout=S.GetLayoutVersion();
 if(!W.After(TEXT("ParameterStore_Snapshot")))return false;for(auto* O:Out.Interfaces)if(O&&!W.Pin(O))return false;for(auto* O:Out.UObjects)if(O&&!W.Pin(O))return false;for(const auto& V:Out.Variables){if(V.GetType().GetStruct()&&!W.Pin(V.GetType().GetStruct()))return false;if(V.GetType().GetClass()&&!W.Pin(V.GetType().GetClass()))return false;if(V.GetType().GetEnum()&&!W.Pin(V.GetType().GetEnum()))return false;}return true;
}

void AddVectorField(TSharedPtr<FJsonObject> Object, const TCHAR* FieldName, const FVector& Value)
    {
        if(!FMath::IsFinite(Value.X)||!FMath::IsFinite(Value.Y)||!FMath::IsFinite(Value.Z)){Current->Fail(TEXT("niagara_runtime_nonfinite_value"),TEXT("Observed vector unavailable; not a numeric zero."));return;}
    TSharedPtr<FJsonObject> VectorObject = MakeShared<FJsonObject>();
        VectorObject->SetNumberField(TEXT("x"), Value.X);
        VectorObject->SetNumberField(TEXT("y"), Value.Y);
        VectorObject->SetNumberField(TEXT("z"), Value.Z);
        Object->SetObjectField(FieldName, VectorObject);
    }

void AddRotatorField(TSharedPtr<FJsonObject> Object, const TCHAR* FieldName, const FRotator& Value)
    {
        if(!FMath::IsFinite(Value.Pitch)||!FMath::IsFinite(Value.Yaw)||!FMath::IsFinite(Value.Roll)){Current->Fail(TEXT("niagara_runtime_nonfinite_value"),TEXT("Observed rotation unavailable; not a numeric zero."));return;}
    TSharedPtr<FJsonObject> RotatorObject = MakeShared<FJsonObject>();
        RotatorObject->SetNumberField(TEXT("pitch"), Value.Pitch);
        RotatorObject->SetNumberField(TEXT("yaw"), Value.Yaw);
        RotatorObject->SetNumberField(TEXT("roll"), Value.Roll);
        Object->SetObjectField(FieldName, RotatorObject);
    }

void AddBoxField(TSharedPtr<FJsonObject> Object, const TCHAR* FieldName, const FBox& Value)
    {
        TSharedPtr<FJsonObject> BoxObject = MakeShared<FJsonObject>();
        BoxObject->SetBoolField(TEXT("is_valid"), Value.IsValid != 0);
        AddVectorField(BoxObject, TEXT("min"), Value.Min);
        AddVectorField(BoxObject, TEXT("max"), Value.Max);
        if (Value.IsValid)
        {
            AddVectorField(BoxObject, TEXT("center"), Value.GetCenter());
            AddVectorField(BoxObject, TEXT("extent"), Value.GetExtent());
            AddVectorField(BoxObject, TEXT("size"), Value.GetSize());
        }
        Object->SetObjectField(FieldName, BoxObject);
    }

TArray<TSharedPtr<FJsonValue>> NumberArrayToJson(const TArray<double>& Values)
    {
        TArray<TSharedPtr<FJsonValue>> Array;
        Array.Reserve(Values.Num());
        for (double Value : Values)
        {
            if(!FMath::IsFinite(Value)){Current->Fail(TEXT("niagara_runtime_nonfinite_value"),TEXT("Non-finite observed value is unavailable, not zero."));return {};}Array.Add(MakeShared<FJsonValueNumber>(Value));
        }
        return Array;
    }


FString NiagaraRuntimeStateObjectPath(const UObject* Object)
    {
        return Object ? Object->GetPathName() : FString();
    }

FString NiagaraRuntimeStateClassName(const UObject* Object)
    {
        return Object && Object->GetClass() ? Object->GetClass()->GetPathName() : FString();
    }

FString NiagaraExecutionStateToString(ENiagaraExecutionState State)
    {
        if (const UEnum* Enum = StaticEnum<ENiagaraExecutionState>())
        {
            return Enum->GetNameStringByValue(static_cast<int64>(State));
        }
        return FString::FromInt(static_cast<int32>(State));
    }

FString NiagaraSimTargetToString(ENiagaraSimTarget Target)
    {
        if (const UEnum* Enum = StaticEnum<ENiagaraSimTarget>())
        {
            return Enum->GetNameStringByValue(static_cast<int64>(Target));
        }
        return FString::FromInt(static_cast<int32>(Target));
    }

bool NiagaraRuntimeStateMatchesFilter(const FString& Value, const FString& Filter)
    {
        return Filter.IsEmpty() || Value.Contains(Filter, ESearchCase::IgnoreCase);
    }

FString NiagaraRuntimeStateBytesToHex(const uint8* Data, int32 ByteCount)
    {
        FString Hex;
        Hex.Reserve(FMath::Max(0, ByteCount) * 2);
        for (int32 ByteIndex = 0; ByteIndex < ByteCount; ++ByteIndex)
        {
            Hex += FString::Printf(TEXT("%02x"), Data[ByteIndex]);
        }
        return Hex;
    }

void AddNiagaraRuntimeStateRawBytes(
        TSharedPtr<FJsonObject> ParameterObject,
        const FStoreSnapshot& Store,
        const FNiagaraVariableWithOffset& Parameter,
        bool bIncludeRawBytes,
        int32 MaxRawBytes)
    {
        const int32 SizeInBytes = Parameter.GetSizeInBytes();
        ParameterObject->SetNumberField(TEXT("size_bytes"), SizeInBytes);
        ParameterObject->SetStringField(TEXT("value_type"), TEXT("raw_bytes"));
        if (!bIncludeRawBytes || SizeInBytes <= 0)
        {
            ParameterObject->SetBoolField(TEXT("raw_bytes_included"), false);
            return;
        }

        const int32 StoreByteCount = Store.GetParameterDataArray().Num();
        if (Parameter.Offset < 0 || static_cast<int64>(Parameter.Offset) + SizeInBytes > StoreByteCount)
        {
            ParameterObject->SetBoolField(TEXT("raw_bytes_included"), false);
            ParameterObject->SetStringField(TEXT("raw_bytes_error"), TEXT("Parameter data offset is outside the store data buffer"));
            return;
        }

        const int32 ClampedByteCount = FMath::Clamp(MaxRawBytes, 0, SizeInBytes);
        if(!Current->Budget(1,static_cast<int64>(ClampedByteCount)*2))return;
    const uint8* RawData = Store.GetParameterData(Parameter.Offset, SizeInBytes);
        ParameterObject->SetBoolField(TEXT("raw_bytes_included"), ClampedByteCount > 0);
        ParameterObject->SetNumberField(TEXT("raw_byte_count"), ClampedByteCount);
        ParameterObject->SetNumberField(TEXT("raw_total_byte_count"), SizeInBytes);
        if (ClampedByteCount > 0)
        {
            ParameterObject->SetStringField(TEXT("raw_bytes_hex"), NiagaraRuntimeStateBytesToHex(RawData, ClampedByteCount));
        }
        ParameterObject->SetBoolField(TEXT("raw_bytes_truncated"), ClampedByteCount < SizeInBytes);
    }

FString TruncateNiagaraRuntimeStateString(const FString& Value, int32 MaxCharacters)
    {
        const int32 ClampedMaxCharacters = FMath::Clamp(MaxCharacters, 0, 16384);
        if (ClampedMaxCharacters <= 0 || Value.Len() <= ClampedMaxCharacters)
        {
            return Value;
        }
        return Value.Left(ClampedMaxCharacters) + TEXT("...");
    }


FString ExportNiagaraRuntimeStatePropertyValue(FProperty* Property,UObject* Owner,int32 MaxCharacters,int32 ArrayIndex=INDEX_NONE,int32 ExpectedArrayCount=0)
{
 auto& W=*Current;FString Out;if(!W.Admit(TEXT("property_export_admission"))||!W.Pin(Owner))return Out;const FName Name=Property->GetFName();auto* CurrentProperty=FindFProperty<FProperty>(Owner->GetClass(),Name);if(CurrentProperty!=Property){W.Fail(TEXT("niagara_runtime_property_replaced"),TEXT("Exact reflected property changed before export."));return Out;}
 const void* Data=Property->ContainerPtrToValuePtr<void>(Owner);FProperty* ExportProperty=Property;
 if(ArrayIndex!=INDEX_NONE){auto* A=CastField<FArrayProperty>(Property);if(!A||!A->Inner){W.Fail(TEXT("niagara_runtime_property_type_changed"),TEXT("Array property changed."));return Out;}FScriptArrayHelper H(A,Data);if(H.Num()!=ExpectedArrayCount||ArrayIndex<0||ArrayIndex>=H.Num()){W.Fail(TEXT("niagara_runtime_array_changed"),TEXT("Array count/index changed across reflection callback."));return Out;}Data=H.GetRawPtr(ArrayIndex);ExportProperty=A->Inner;}
 if(!W.Read(TEXT("Property_ExportText"),[&]{ExportProperty->ExportTextItem_Direct(Out,Data,nullptr,Owner,PPF_None);}))return {};
 if(FindFProperty<FProperty>(Owner->GetClass(),Name)!=Property){W.Fail(TEXT("niagara_runtime_property_replaced"),TEXT("Property changed during export callback."));return {};}
 if(ArrayIndex!=INDEX_NONE){auto* A=CastField<FArrayProperty>(Property);FScriptArrayHelper H(A,Property->ContainerPtrToValuePtr<void>(Owner));if(H.Num()!=ExpectedArrayCount){W.Fail(TEXT("niagara_runtime_array_changed"),TEXT("Array changed during export callback; no stale pointer reused."));return {};}}
 Out=TruncateNiagaraRuntimeStateString(Out,MaxCharacters);if(!W.Budget(1,Out.Len()))return {};return Out;
}
TSharedPtr<FJsonObject> BuildNiagaraRuntimeStateObjectPropertyPreview(UObject* Object,int32 MaxProperties,int32 MaxArrayElements,int32 MaxPropertyValueChars)
{
 auto& W=*Current;auto Preview=MakeShared<FJsonObject>();if(!W.Admit(TEXT("property_preview")))return Preview;Preview->SetStringField(TEXT("object_path"),NiagaraRuntimeStateObjectPath(Object));Preview->SetStringField(TEXT("object_class"),NiagaraRuntimeStateClassName(Object));TArray<TSharedPtr<FJsonValue>> Rows;int32 Total=0;
 if(Object){if(!W.Pin(Object))return Preview;TArray<FName> Names;for(TFieldIterator<FProperty> It(Object->GetClass(),EFieldIteratorFlags::IncludeSuper);It;++It)if(!It->HasAnyPropertyFlags(CPF_Deprecated)){if(!W.Budget(1))return Preview;Names.Add(It->GetFName());}Total=Names.Num();
  for(int32 I=0;I<FMath::Min(Names.Num(),MaxProperties);++I){if(!W.Admit(TEXT("property_preview_row")))return Preview;auto* P=FindFProperty<FProperty>(Object->GetClass(),Names[I]);if(!P){W.Fail(TEXT("niagara_runtime_property_replaced"),TEXT("Requested property disappeared."));return Preview;}auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("name"),P->GetName());J->SetStringField(TEXT("cpp_type"),P->GetCPPType());J->SetStringField(TEXT("class_name"),P->GetClass()->GetName());Rows.Add(MakeShared<FJsonValueObject>(J));
   if(auto* A=CastField<FArrayProperty>(P)){FScriptArrayHelper H(A,A->ContainerPtrToValuePtr<void>(Object));const int32 N=H.Num();if(N<0){W.Fail(TEXT("niagara_runtime_array_shape_invalid"),TEXT("Negative reflected array count."));return Preview;}J->SetBoolField(TEXT("is_array"),true);J->SetStringField(TEXT("inner_cpp_type"),A->Inner?A->Inner->GetCPPType():FString());J->SetNumberField(TEXT("array_count"),N);J->SetBoolField(TEXT("array_truncated"),N>MaxArrayElements);TArray<TSharedPtr<FJsonValue>> Values;for(int32 K=0;K<FMath::Min(N,MaxArrayElements);++K){FString V=ExportNiagaraRuntimeStatePropertyValue(P,Object,MaxPropertyValueChars,K,N);if(!W.Admit(TEXT("property_array_exported")))return Preview;Values.Add(MakeShared<FJsonValueString>(V));}J->SetArrayField(TEXT("array_samples"),Values);}
   else{if(auto* OP=CastField<FObjectPropertyBase>(P)){UObject* V=nullptr;if(!W.Read(TEXT("ObjectProperty_Value"),[&]{V=OP->GetObjectPropertyValue_InContainer(Object);}))return Preview;if(V&&!W.Pin(V))return Preview;J->SetBoolField(TEXT("is_object"),true);J->SetStringField(TEXT("object_path"),NiagaraRuntimeStateObjectPath(V));J->SetStringField(TEXT("object_class"),NiagaraRuntimeStateClassName(V));}else J->SetBoolField(TEXT("is_array"),false);const FString V=ExportNiagaraRuntimeStatePropertyValue(P,Object,MaxPropertyValueChars);if(!W.Admit(TEXT("property_exported")))return Preview;J->SetStringField(TEXT("export_text"),V);}
  }
 }
 Preview->SetNumberField(TEXT("total_property_count"),Total);Preview->SetNumberField(TEXT("returned_property_count"),Rows.Num());Preview->SetBoolField(TEXT("truncated"),Rows.Num()<Total);Preview->SetNumberField(TEXT("max_array_elements"),MaxArrayElements);Preview->SetNumberField(TEXT("max_property_value_chars"),MaxPropertyValueChars);Preview->SetArrayField(TEXT("properties"),Rows);Preview->SetBoolField(TEXT("atomic_object_snapshot_verified"),false);return Preview;
}

void AddNiagaraRuntimeStateParameterValue(
        TSharedPtr<FJsonObject> ParameterObject,
        const FStoreSnapshot& Store,
        const FNiagaraVariableWithOffset& Parameter,
        bool bIncludeRawBytes,
        int32 MaxRawBytes,
        bool bIncludeParameterObjectProperties,
        int32 MaxObjectProperties,
        int32 MaxArrayElements,
        int32 MaxPropertyValueChars)
    {
        if(!ContinueRead()||!Store.Valid(Parameter)){ParameterObject->SetBoolField(TEXT("value_available"),false);return;}
    const FNiagaraTypeDefinition& Type = Parameter.GetType();
        if (Parameter.IsDataInterface())
        {
            UNiagaraDataInterface* DataInterface = Store.GetDataInterface(Parameter.Offset);
            ParameterObject->SetStringField(TEXT("value_type"), TEXT("data_interface"));
            ParameterObject->SetStringField(TEXT("object_path"), NiagaraRuntimeStateObjectPath(DataInterface));
            ParameterObject->SetStringField(TEXT("object_class"), NiagaraRuntimeStateClassName(DataInterface));
            if (bIncludeParameterObjectProperties)
            {
                ParameterObject->SetObjectField(
                    TEXT("object_properties"),
                    BuildNiagaraRuntimeStateObjectPropertyPreview(DataInterface, MaxObjectProperties, MaxArrayElements, MaxPropertyValueChars));
            }
            return;
        }

        if (Parameter.IsUObject())
        {
            UObject* Object = Store.GetUObject(Parameter.Offset);
            ParameterObject->SetStringField(TEXT("value_type"), TEXT("uobject"));
            ParameterObject->SetStringField(TEXT("object_path"), NiagaraRuntimeStateObjectPath(Object));
            ParameterObject->SetStringField(TEXT("object_class"), NiagaraRuntimeStateClassName(Object));
            if (bIncludeParameterObjectProperties)
            {
                ParameterObject->SetObjectField(
                    TEXT("object_properties"),
                    BuildNiagaraRuntimeStateObjectPropertyPreview(Object, MaxObjectProperties, MaxArrayElements, MaxPropertyValueChars));
            }
            return;
        }

        if (Type == FNiagaraTypeDefinition::GetFloatDef())
        {
            ParameterObject->SetStringField(TEXT("value_type"), TEXT("float"));
            const float V=Store.GetParameterValueFromOffset<float>(Parameter.Offset);if(!FMath::IsFinite(V)){Current->Fail(TEXT("niagara_runtime_nonfinite_value"),TEXT("Observed float is unavailable, not zero."));ParameterObject->SetBoolField(TEXT("value_available"),false);return;}ParameterObject->SetNumberField(TEXT("value"),V);
            return;
        }

        if (Type == FNiagaraTypeDefinition::GetIntDef())
        {
            ParameterObject->SetStringField(TEXT("value_type"), TEXT("int"));
            ParameterObject->SetNumberField(TEXT("value"), Store.GetParameterValueFromOffset<int32>(Parameter.Offset));
            return;
        }

        if (Type == FNiagaraTypeDefinition::GetBoolDef())
        {
            const FNiagaraBool Value = Store.GetParameterValueFromOffset<FNiagaraBool>(Parameter.Offset);
            ParameterObject->SetStringField(TEXT("value_type"), TEXT("bool"));
            ParameterObject->SetBoolField(TEXT("value"), Value.GetValue());
            ParameterObject->SetNumberField(TEXT("raw_bool_value"), Value.GetRawValue());
            return;
        }

        if (Type == FNiagaraTypeDefinition::GetVec2Def())
        {
            const FVector2f Value = Store.GetParameterValueFromOffset<FVector2f>(Parameter.Offset);
            ParameterObject->SetStringField(TEXT("value_type"), TEXT("vec2"));
            ParameterObject->SetArrayField(TEXT("value"), NumberArrayToJson(TArray<double>{Value.X, Value.Y}));
            return;
        }

        if (Type == FNiagaraTypeDefinition::GetVec3Def())
        {
            const FVector3f Value = Store.GetParameterValueFromOffset<FVector3f>(Parameter.Offset);
            ParameterObject->SetStringField(TEXT("value_type"), TEXT("vec3"));
            ParameterObject->SetArrayField(TEXT("value"), NumberArrayToJson(TArray<double>{Value.X, Value.Y, Value.Z}));
            return;
        }

        if (Type == FNiagaraTypeDefinition::GetPositionDef())
        {
            const FNiagaraPosition Value = Store.GetParameterValueFromOffset<FNiagaraPosition>(Parameter.Offset);
            ParameterObject->SetStringField(TEXT("value_type"), TEXT("position"));
            ParameterObject->SetArrayField(TEXT("value"), NumberArrayToJson(TArray<double>{Value.X, Value.Y, Value.Z}));
            return;
        }

        if (Type == FNiagaraTypeDefinition::GetVec4Def())
        {
            const FVector4f Value = Store.GetParameterValueFromOffset<FVector4f>(Parameter.Offset);
            ParameterObject->SetStringField(TEXT("value_type"), TEXT("vec4"));
            ParameterObject->SetArrayField(TEXT("value"), NumberArrayToJson(TArray<double>{Value.X, Value.Y, Value.Z, Value.W}));
            return;
        }

        if (Type == FNiagaraTypeDefinition::GetColorDef())
        {
            const FLinearColor Value = Store.GetParameterValueFromOffset<FLinearColor>(Parameter.Offset);
            ParameterObject->SetStringField(TEXT("value_type"), TEXT("linear_color"));
            ParameterObject->SetArrayField(TEXT("value"), NumberArrayToJson(TArray<double>{Value.R, Value.G, Value.B, Value.A}));
            return;
        }

        if (Type == FNiagaraTypeDefinition::GetQuatDef())
        {
            const FQuat4f Value = Store.GetParameterValueFromOffset<FQuat4f>(Parameter.Offset);
            ParameterObject->SetStringField(TEXT("value_type"), TEXT("quat"));
            ParameterObject->SetArrayField(TEXT("value"), NumberArrayToJson(TArray<double>{Value.X, Value.Y, Value.Z, Value.W}));
            return;
        }

        if (Type == FNiagaraTypeDefinition::GetMatrix4Def())
        {
            const FMatrix44f Value = Store.GetParameterValueFromOffset<FMatrix44f>(Parameter.Offset);
            TArray<double> Values;
            Values.Reserve(16);
            for (int32 Row = 0; Row < 4; ++Row)
            {
                for (int32 Column = 0; Column < 4; ++Column)
                {
                    Values.Add(Value.M[Row][Column]);
                }
            }
            ParameterObject->SetStringField(TEXT("value_type"), TEXT("matrix4"));
            ParameterObject->SetArrayField(TEXT("value"), NumberArrayToJson(Values));
            return;
        }

        AddNiagaraRuntimeStateRawBytes(ParameterObject, Store, Parameter, bIncludeRawBytes, MaxRawBytes);
    }
TArray<TSharedPtr<FJsonValue>> BuildNiagaraRuntimeStateParameterArray(
        const FStoreSnapshot& Store,
        const FString& ParameterNameContains,
        int32 MaxParameters,
        bool bIncludeParameterValues,
        bool bIncludeRawBytes,
        int32 MaxRawBytes,
        bool bIncludeParameterObjectProperties,
        int32 MaxObjectProperties,
        int32 MaxArrayElements,
        int32 MaxPropertyValueChars,
        int32& OutTotalParameterCount,
        int32& OutReturnedParameterCount,
        int32& OutFilteredParameterCount,
        bool& bOutTruncated)
    {
        TArray<TSharedPtr<FJsonValue>> Parameters;
        OutTotalParameterCount = 0;
        OutReturnedParameterCount = 0;
        OutFilteredParameterCount = 0;
        bOutTruncated = false;

        const TArrayView<const FNiagaraVariableWithOffset> ParameterVariables = Store.ReadParameterVariables();
        OutTotalParameterCount = ParameterVariables.Num();
        const int32 ClampedMaxParameters = FMath::Clamp(MaxParameters, 0, 4096);
        for (const FNiagaraVariableWithOffset& Parameter : ParameterVariables)
        {
            if(!ContinueRead()||!Current->Budget(1,Parameter.GetName().ToString().Len()))return Parameters;
        const FString ParameterName = Parameter.GetName().ToString();
            if (!NiagaraRuntimeStateMatchesFilter(ParameterName, ParameterNameContains))
            {
                ++OutFilteredParameterCount;
                continue;
            }

            if (OutReturnedParameterCount >= ClampedMaxParameters)
            {
                bOutTruncated = true;
                continue;
            }

            TSharedPtr<FJsonObject> ParameterObject = MakeShared<FJsonObject>();
            const FNiagaraTypeDefinition& Type = Parameter.GetType();
            ParameterObject->SetStringField(TEXT("name"), ParameterName);
            ParameterObject->SetStringField(TEXT("type_name"), Type.GetName());
            ParameterObject->SetStringField(TEXT("type_display_name"), Type.GetNameText().ToString());
            ParameterObject->SetNumberField(TEXT("offset"), Parameter.Offset);
            ParameterObject->SetNumberField(TEXT("size_bytes"), Parameter.GetSizeInBytes());
            ParameterObject->SetBoolField(TEXT("is_data_interface"), Parameter.IsDataInterface());
            ParameterObject->SetBoolField(TEXT("is_uobject"), Parameter.IsUObject());
            ParameterObject->SetBoolField(TEXT("is_enum"), Type.IsEnum());
            ParameterObject->SetBoolField(TEXT("is_static"), Type.IsStatic());
            if (UStruct* Struct = Type.GetStruct())
            {
                ParameterObject->SetStringField(TEXT("struct_path"), Struct->GetPathName());
            }
            if (UClass* Class = Type.GetClass())
            {
                ParameterObject->SetStringField(TEXT("class_path"), Class->GetPathName());
            }
            if (UEnum* Enum = Type.GetEnum())
            {
                ParameterObject->SetStringField(TEXT("enum_path"), Enum->GetPathName());
            }

            if (bIncludeParameterValues)
            {
                AddNiagaraRuntimeStateParameterValue(
                    ParameterObject,
                    Store,
                    Parameter,
                    bIncludeRawBytes,
                    MaxRawBytes,
                    bIncludeParameterObjectProperties,
                    MaxObjectProperties,
                    MaxArrayElements,
                    MaxPropertyValueChars);
            }

            Parameters.Add(MakeShared<FJsonValueObject>(ParameterObject));if(!ContinueRead())return Parameters;
            ++OutReturnedParameterCount;
        }

        return Parameters;
    }
void AddNiagaraRuntimeStateStore(
        TSharedPtr<FJsonObject> Target,
        const TCHAR* FieldName,
        const FNiagaraParameterStore& Store,
        const FString& ParameterNameContains,
        int32 MaxParameters,
        bool bIncludeParameterValues,
        bool bIncludeRawBytes,
        int32 MaxRawBytes,
        bool bIncludeParameterObjectProperties,
        int32 MaxObjectProperties,
        int32 MaxArrayElements,
        int32 MaxPropertyValueChars)
    {
        FStoreSnapshot Snapshot;if(!CaptureStore(Store,Snapshot))return;
    int32 TotalParameterCount = 0;
        int32 ReturnedParameterCount = 0;
        int32 FilteredParameterCount = 0;
        bool bTruncated = false;
        TArray<TSharedPtr<FJsonValue>> Parameters = BuildNiagaraRuntimeStateParameterArray(
            Snapshot,
            ParameterNameContains,
            MaxParameters,
            bIncludeParameterValues,
            bIncludeRawBytes,
            MaxRawBytes,
            bIncludeParameterObjectProperties,
            MaxObjectProperties,
            MaxArrayElements,
            MaxPropertyValueChars,
            TotalParameterCount,
            ReturnedParameterCount,
            FilteredParameterCount,
            bTruncated);

        TSharedPtr<FJsonObject> StoreObject = MakeShared<FJsonObject>();
        StoreObject->SetNumberField(TEXT("total_parameter_count"), TotalParameterCount);
        StoreObject->SetNumberField(TEXT("returned_parameter_count"), ReturnedParameterCount);
        StoreObject->SetNumberField(TEXT("filtered_parameter_count"), FilteredParameterCount);
        StoreObject->SetBoolField(TEXT("truncated"), bTruncated);
        StoreObject->SetBoolField(TEXT("parameters_dirty"), Snapshot.GetParametersDirty());
        StoreObject->SetBoolField(TEXT("interfaces_dirty"), Snapshot.GetInterfacesDirty());
        StoreObject->SetBoolField(TEXT("uobjects_dirty"), Snapshot.GetUObjectsDirty());
        StoreObject->SetBoolField(TEXT("position_data_dirty"), Snapshot.GetPositionDataDirty());
        StoreObject->SetNumberField(TEXT("layout_version"), Snapshot.GetLayoutVersion());
        StoreObject->SetNumberField(TEXT("data_interface_count"), Snapshot.GetDataInterfaces().Num());
        StoreObject->SetNumberField(TEXT("uobject_count"), Snapshot.GetUObjects().Num());
        StoreObject->SetNumberField(TEXT("parameter_data_byte_count"), Snapshot.GetParameterDataArray().Num());
        StoreObject->SetArrayField(TEXT("parameters"), Parameters);
        StoreObject->SetBoolField(TEXT("owned_value_snapshot"),true);StoreObject->SetBoolField(TEXT("atomic_cross_store_frame_snapshot_verified"),false);Target->SetObjectField(FieldName,StoreObject);
    }


FString NormalizeEditorBlueprintObjectPath(const FString& InputPath)
    {
        FString Path = FPackageName::ExportTextPathToObjectPath(InputPath).TrimStartAndEnd();
        Path.TrimQuotesInline();
        Path.ReplaceInline(TEXT("\\"), TEXT("/"));

        if (Path.EndsWith(TEXT(".uasset"), ESearchCase::IgnoreCase))
        {
            FString LongPackageName;
            if (FPackageName::TryConvertFilenameToLongPackageName(Path, LongPackageName))
            {
                Path = LongPackageName;
            }
        }

        if (Path.Contains(TEXT(".")))
        {
            FString PackageName;
            FString ObjectName;
            Path.Split(TEXT("."), &PackageName, &ObjectName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
            if (ObjectName.EndsWith(TEXT("_C")))
            {
                ObjectName.LeftChopInline(2);
                Path = FString::Printf(TEXT("%s.%s"), *PackageName, *ObjectName);
            }
            return Path;
        }

        if (Path.StartsWith(TEXT("/Game/")) || Path.StartsWith(TEXT("/Engine/")))
        {
            const FString AssetName = FPaths::GetBaseFilename(Path);
            if (!AssetName.IsEmpty())
            {
                Path = FString::Printf(TEXT("%s.%s"), *Path, *AssetName);
            }
        }

        return Path;
    }
FString NormalizeNiagaraSystemObjectPath(const FString& InputPath)
    {
        FString Path = InputPath.TrimStartAndEnd();
        Path.TrimQuotesInline();

        if ((Path.StartsWith(TEXT("/Game/")) || Path.StartsWith(TEXT("/Engine/")) || Path.StartsWith(TEXT("/Niagara/"))) && !Path.Contains(TEXT(".")))
        {
            const FString AssetName = FPaths::GetBaseFilename(Path);
            Path = FString::Printf(TEXT("%s.%s"), *Path, *AssetName);
        }

        return Path;
    }

FString DependencyError(const FString& Key,const FString& Value){return Key+TEXT(" contains forbidden runtime tooling reference: ")+Value;}
TArray<FString> GetDirtyPackageNames(){TArray<FString> Names;if(!ContinueRead())return Names;ForEachObjectOfClass(UPackage::StaticClass(),[&](UObject* O){auto* P=Cast<UPackage>(O);if(Live(P)&&P->IsDirty()&&Current->R.Error.IsEmpty()){const FString Name=P->GetName();if(Current->Budget(1,Name.Len()))Names.Add(Name);}},true,RF_ClassDefaultObject);Names.Sort();return Names;}
void AddDirtyPackageCommandDelta(const TSharedPtr<FJsonObject>& ResultObj, const TArray<FString>& DirtyPackagesBefore)
    {
        if(!ContinueRead()){ResultObj->SetBoolField(TEXT("dirty_package_observation_skipped"),true);return;}
        const TArray<FString> DirtyPackagesAfter = GetDirtyPackageNames();

        TSet<FString> BeforeSet;
        for (const FString& PackageName : DirtyPackagesBefore)
        {
            BeforeSet.Add(PackageName);
        }

        TSet<FString> AfterSet;
        for (const FString& PackageName : DirtyPackagesAfter)
        {
            AfterSet.Add(PackageName);
        }

        TArray<FString> AddedPackages;
        for (const FString& PackageName : DirtyPackagesAfter)
        {
            if (!BeforeSet.Contains(PackageName))
            {
                AddedPackages.Add(PackageName);
            }
        }

        TArray<FString> RemovedPackages;
        for (const FString& PackageName : DirtyPackagesBefore)
        {
            if (!AfterSet.Contains(PackageName))
            {
                RemovedPackages.Add(PackageName);
            }
        }

        AddedPackages.Sort();
        RemovedPackages.Sort();

        ResultObj->SetNumberField(TEXT("dirty_package_count_before"), DirtyPackagesBefore.Num());
        ResultObj->SetArrayField(TEXT("dirty_packages_before"), StringArrayToJsonArray(DirtyPackagesBefore));
        ResultObj->SetNumberField(TEXT("dirty_package_count_after"), DirtyPackagesAfter.Num());
        ResultObj->SetArrayField(TEXT("dirty_packages_after"), StringArrayToJsonArray(DirtyPackagesAfter));
        ResultObj->SetNumberField(TEXT("dirty_package_added_count"), AddedPackages.Num());
        ResultObj->SetArrayField(TEXT("dirty_packages_added_by_command"), StringArrayToJsonArray(AddedPackages));
        ResultObj->SetNumberField(TEXT("dirty_package_removed_count"), RemovedPackages.Num());
        ResultObj->SetArrayField(TEXT("dirty_packages_removed_by_command"), StringArrayToJsonArray(RemovedPackages));
    }
template<class T>T* LoadExact(const FString& Path){auto& W=*Current;T* O=nullptr;if(!W.Read(TEXT("LoadObject"),[&]{O=LoadObject<T>(nullptr,*Path);auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("requested_path"),Path);J->SetBoolField(TEXT("nonnull_return"),O!=nullptr);W.R.Data->SetObjectField(TEXT("last_load_return"),J);}))return nullptr;if(!O||!W.Pin(O))return nullptr;if(!Same(O->GetPathName(),Path)||Forbidden(O->GetClass()->GetPathName())){W.Fail(TEXT("blueprint_niagara_template_load_identity"),TEXT("Returned object differs from requested exact path or has forbidden runtime class"));return nullptr;}if constexpr(std::is_same_v<T,UBlueprint>){if(!W.Blueprint(O))return nullptr;}return O;}
TArray<USCS_Node*> CaptureNodes(UBlueprintGeneratedClass* C){auto& W=*Current;TArray<USCS_Node*> Out;if(!W.Pin(C)||!C->SimpleConstructionScript)return Out;auto* S=C->SimpleConstructionScript.Get();if(!W.Pin(S))return Out;for(const auto& B:W.SCSTopology)if(!B.Retired&&B.Owner.Get()==C){for(const auto& N:B.Nodes)Out.Add(N.Get());return Out;}Out=S->GetAllNodes();if(!W.Budget(Out.Num()))return {};FSCSBinding B;B.Owner=C;B.SCS=S;for(auto* N:Out){if(!N){B.Nodes.Add(nullptr);continue;}if(!W.Pin(N)||N->GetOuter()!=S){W.Fail(TEXT("blueprint_niagara_template_foreign_scs_node"),TEXT("SCS node is not owned by exact captured SCS"));return {};}B.Nodes.Add(N);}W.SCSTopology.Add(MoveTemp(B));return Out;}
UActorComponent* ActualTemplate(USCS_Node* N,UBlueprintGeneratedClass* C){auto& W=*Current;if(!N||!W.Pin(N)||!W.Pin(C))return nullptr;auto* S=Cast<USimpleConstructionScript>(N->GetOuter());if(!S||!W.Pin(S)||!S->GetAllNodes().Contains(N)){W.Fail(TEXT("blueprint_niagara_template_node_membership"),TEXT("Node does not belong to captured physical SCS"));return nullptr;}auto* Direct=N->ComponentTemplate.Get();if(Direct&&!W.Pin(Direct))return nullptr;UActorComponent* A=nullptr;if(!W.Read(TEXT("SCS_GetActualComponentTemplate"),[&]{A=N->GetActualComponentTemplate(C);}))return nullptr;if(!A)return nullptr;if(!W.Pin(A))return nullptr;for(const auto& B:W.Templates)if(!B.Retired&&B.Node.Get()==N&&B.ActualClass.Get()==C){if(B.Actual.Get()!=A){W.Fail(TEXT("blueprint_niagara_template_actual_role_changed"),TEXT("GetActualComponentTemplate returned a different role"));return nullptr;}return A;}FTemplateBinding B;B.Node=N;B.SCS=S;B.Direct=Direct;B.Actual=A;B.ActualClass=C;B.Variable=N->GetVariableName();if(auto* NC=Cast<UNiagaraComponent>(A)){B.Asset=NC->GetAsset();if(B.Asset.IsValid()&&!W.Pin(B.Asset.Get()))return nullptr;}W.Templates.Add(B);return A;}
bool SelectLocal(UBlueprint* B,USCS_Node* N,UNiagaraComponent* T){auto& W=*Current;if(!W.Blueprint(B)||!W.Pin(N)||!W.Pin(T)||!W.Idle(B))return false;auto* C=Cast<UBlueprintGeneratedClass>(B->GeneratedClass);if(!C||!C->SimpleConstructionScript||B->SimpleConstructionScript!=C->SimpleConstructionScript||N->GetOuter()!=C->SimpleConstructionScript||!C->SimpleConstructionScript->GetAllNodes().Contains(N)||N->ComponentTemplate!=T||T->GetOuter()!=C||T->GetPackage()!=B->GetPackage()||T->GetOutermost()!=B->GetOutermost()||C->ClassGeneratedBy!=B)return W.Fail(TEXT("blueprint_niagara_template_writer_ownership"),TEXT("Writer requires exact local SCS Node and component template under current BlueprintGeneratedClass in same effective package"));W.Target=B;W.SelectedVariable=N->GetVariableName().ToString();W.SelectedTemplate=T;return true;}
bool SwapLocal(UBlueprint* B,USCS_Node* N,UNiagaraComponent* T,UNiagaraSystem* Replacement){auto& W=*Current;if(!SelectLocal(B,N,T)||!W.Pin(Replacement))return false;if(!W.Write(TEXT("Blueprint_Modify"),[&]{B->Modify();}))return false;if(!W.Write(TEXT("Template_Modify"),[&]{T->Modify();}))return false;if(!W.SDK(TEXT("NiagaraComponent_SetAsset_preserve"),true))return false;for(auto& I:W.Objects)if(I->Object.Get()!=T&&I->Object.IsValid()&&I->Object->IsIn(T)&&I->Object->GetPackage()==T->GetPackage()&&I->Object->GetOutermost()==T->GetOutermost()){I->Retired=true;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path_before"),I->Path);J->SetStringField(TEXT("reason"),TEXT("SetAsset may synchronize owned override DI subtree"));W.RetiredRows.Add(MakeShared<FJsonValueObject>(J));}for(auto& Binding:W.Templates)if(!Binding.Retired&&Binding.Actual.Get()==T)Binding.Asset=Replacement;T->SetAsset(Replacement,false);W.R.Data->SetBoolField(TEXT("set_asset_returned"),true);if(!W.After(TEXT("NiagaraComponent_SetAsset_preserve")))return false;if(T->GetAsset()!=Replacement)return W.Fail(TEXT("blueprint_niagara_template_set_asset_readback"),TEXT("Actual template assignment does not match requested replacement"));W.R.bChanged=true;W.R.Data->SetBoolField(TEXT("actual_template_assignment_changed"),true);W.R.Data->SetStringField(TEXT("override_preservation_scope"),TEXT("SetAsset(reset=false); SDK synchronization may rebuild overrides/DI. Full byte/reference equality is not claimed."));return W.Write(TEXT("MarkBlueprintAsModified"),[&]{FBlueprintEditorUtils::MarkBlueprintAsModified(B);});}
FString BlueprintStatus(UBlueprint* B){return StaticEnum<EBlueprintStatus>()?StaticEnum<EBlueprintStatus>()->GetNameStringByValue(static_cast<int64>(B->Status)):FString::FromInt(static_cast<int32>(B->Status));}
bool CompileOwned(UBlueprint* B,USCS_Node*& Node,UNiagaraComponent*& Template,UBlueprintGeneratedClass*& Class){auto& W=*Current;if(!SelectLocal(B,Node,Template))return false;auto* OldClass=B->GeneratedClass.Get();auto* OldSkeleton=B->SkeletonGeneratedClass.Get();auto* OldSCS=B->SimpleConstructionScript.Get();if(OldSkeleton&&(!Live(OldSkeleton)||OldSkeleton->ClassGeneratedBy!=B||OldSkeleton->GetPackage()!=B->GetPackage()))return W.Fail(TEXT("blueprint_niagara_template_skeleton_owner"),TEXT("Cannot retire foreign Skeleton class role"));const FName Variable=Node->GetVariableName();if(!W.SDK(TEXT("CompileBlueprint"),true))return false;for(auto& I:W.Objects){auto* O=I->Object.Get();if(O&&O!=B&&O->GetPackage()==B->GetPackage()&&O->GetOutermost()==B->GetOutermost()&&(O==OldClass||O==OldSkeleton||O==OldSCS||(OldClass&&O->IsIn(OldClass))||(OldSkeleton&&O->IsIn(OldSkeleton))||(OldSCS&&O->IsIn(OldSCS)))){I->Retired=true;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path_before"),I->Path);J->SetStringField(TEXT("reason"),TEXT("owned_target_compile_role_retirement"));W.RetiredRows.Add(MakeShared<FJsonValueObject>(J));}}
 for(auto& S:W.SCSTopology)if(S.Owner.Get()==OldClass)S.Retired=true;for(auto& T:W.Templates)if(T.ActualClass.Get()==OldClass)T.Retired=true;for(auto& P:W.Blueprints)if(P.Blueprint.Get()==B)P.Reinstancing=true;
 W.R.bCompileAttempted=true;W.R.Compiled.Reset();FKismetEditorUtilities::CompileBlueprint(B);W.R.Data->SetBoolField(TEXT("blueprint_compile_SDK_returned"),true);if(!W.After(TEXT("CompileBlueprint")))return false;if(B->bBeingCompiled||B->bQueuedForCompilation||!FBlueprintCompilationManager::IsGeneratedClassLayoutReady())return W.Fail(TEXT("blueprint_niagara_template_compile_not_idle"),TEXT("Compile SDK returned but Blueprint is not actually idle; no save/replay"));Class=Cast<UBlueprintGeneratedClass>(B->GeneratedClass);if(!Live(Class)||Class->ClassGeneratedBy!=B||Class->GetPackage()!=B->GetPackage()||!Live(Class->SimpleConstructionScript))return W.Fail(TEXT("blueprint_niagara_template_compiled_role_invalid"),TEXT("Current compile product ownership unavailable"));for(auto& P:W.Blueprints)if(P.Blueprint.Get()==B){P.Class=Class;P.SCS=Class->SimpleConstructionScript;P.RootSCS=B->SimpleConstructionScript;P.Reinstancing=false;}if(!W.Pin(Class)||!W.Pin(Class->SimpleConstructionScript))return false;auto Nodes=CaptureNodes(Class);Node=nullptr;Template=nullptr;for(auto* N:Nodes)if(N&&N->GetVariableName()==Variable){if(Node)return W.Fail(TEXT("blueprint_niagara_template_reacquire_ambiguous"),TEXT("Current exact SCS variable is not unique"));Node=N;}if(!Node)return W.Fail(TEXT("blueprint_niagara_template_reacquire_missing"),TEXT("Compiled SCS variable missing"));Template=Cast<UNiagaraComponent>(ActualTemplate(Node,Class));if(!Template||!SelectLocal(B,Node,Template))return false;if(Template->GetAsset()!=W.Replacement.Get())return W.Fail(TEXT("blueprint_niagara_template_compiled_assignment_changed"),TEXT("Reacquired current template lost exact replacement assignment"));const bool Passed=B->Status==BS_UpToDate||B->Status==BS_UpToDateWithWarnings;W.R.Compiled=Passed;W.R.Data->SetBoolField(TEXT("blueprint_compile_idle_verified"),true);W.R.Data->SetBoolField(TEXT("Niagara_VM_GPU_readiness_verified"),false);return Passed||W.Fail(TEXT("blueprint_niagara_template_compile_failed"),TEXT("Current Blueprint compile status failed"));}


bool PackageOwnership(UBlueprint* B){auto& W=*Current;if(!W.Admit(TEXT("package_export_admission"))||!Live(B))return false;auto* P=B->GetPackage();auto* C=Cast<UBlueprintGeneratedClass>(B->GeneratedClass);auto* S=Cast<UBlueprintGeneratedClass>(B->SkeletonGeneratedClass);for(auto* K:{C,S})if(K&&(K->ClassGeneratedBy!=B||K->GetPackage()!=P||K->GetOuter()!=P))return W.Fail(TEXT("blueprint_niagara_template_class_package_owner"),TEXT("Blueprint class product not owned by exact package"));TArray<UObject*> DefaultObjects;for(auto* K:{C,S})if(K){auto* D=K->GetDefaultObject(false);if(D){if(!Live(D)||D->GetClass()!=K||!D->HasAnyFlags(RF_ClassDefaultObject)||D->GetPackage()!=P||D->GetOutermost()!=P)return W.Fail(TEXT("blueprint_niagara_template_CDO_owner_invalid"),TEXT("Current class CDO ownership is not exact; no save"));DefaultObjects.Add(D);}}TArray<UObject*> Exports;GetObjectsWithPackage(P,Exports,true);if(!W.Budget(Exports.Num()))return false;for(auto* O:Exports){if(!Live(O))continue;const bool Metadata=false; /* UE5.8 package FMetaData is not a UObject export; no broad metadata exemption. */const bool Owned=O==B||O->IsIn(B)||(C&&(O==C||O->IsIn(C)))||(S&&(O==S||O->IsIn(S)))||DefaultObjects.ContainsByPredicate([&](const UObject* D){return O==D||O->IsIn(D);})||Metadata;if(!Owned||O->GetPackage()!=P||O->GetOutermost()!=P||Forbidden(O->GetClass()->GetPathName())){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("foreign_export"),O->GetPathName());J->SetStringField(TEXT("class"),O->GetClass()->GetPathName());W.R.Data->SetObjectField(TEXT("package_ownership_failure"),J);return W.Fail(TEXT("blueprint_niagara_template_foreign_package_export"),TEXT("Save would include foreign package export; actual residual retained"));}}return true;}
bool TargetExists(const FString& ObjectPath){auto& W=*Current;if(!W.Admit(TEXT("target_exists")))return true;const FString Package=FPackageName::ObjectPathToPackageName(ObjectPath);bool Exists=FindObject<UObject>(nullptr,*ObjectPath)!=nullptr||FindPackage(nullptr,*Package)!=nullptr||FPackageName::DoesPackageExist(Package);TArray<FAssetData> Assets;if(!W.Read(TEXT("Registry_target_package"),[&]{auto& AR=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();AR.GetAssetsByPackageName(FName(*Package),Assets,false);}))return true;return Exists||!Assets.IsEmpty();}
bool Fresh(const FString& ObjectPath){auto& W=*Current;if(TargetExists(ObjectPath))return W.Fail(TEXT("blueprint_niagara_canary_target_exists"),TEXT("Requested canary object/package/disk/registry already exists; safe new-owned target only"));if(!W.Admit(TEXT("fresh_target_after_registry")))return false;const FString P=FPackageName::ObjectPathToPackageName(ObjectPath);if(FindObject<UObject>(nullptr,*ObjectPath)||FindPackage(nullptr,*P)||FPackageName::DoesPackageExist(P))return W.Fail(TEXT("blueprint_niagara_canary_target_collision"),TEXT("Target collision appeared after registry SDK; no overwrite/replay"));return true;}
UBlueprint* DuplicateCanary(UBlueprint* Source,const FString& ObjectPath){auto& W=*Current;W.RequestedObject=ObjectPath;W.RequestedPackage=FPackageName::ObjectPathToPackageName(ObjectPath);FPackageName::TryConvertLongPackageNameToFilename(W.RequestedPackage,W.RequestedFilename,FPackageName::GetAssetPackageExtension());if(!W.Blueprint(Source)||!W.Idle(Source)||!Fresh(ObjectPath))return nullptr;W.Source=Source;W.SourceDirty=Source->GetPackage()->IsDirty();W.SourceProtected=true;IAssetTools* Tools=nullptr;if(!W.Read(TEXT("AssetTools_Module"),[&]{Tools=&FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();}))return nullptr;if(!Fresh(ObjectPath)||!W.SDK(TEXT("DuplicateAsset"),true))return nullptr;UObject* O=Tools->DuplicateAsset(FPackageName::GetLongPackageAssetName(W.RequestedPackage),FPackageName::GetLongPackagePath(W.RequestedPackage),Source);auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("requested_object_path"),ObjectPath);Row->SetStringField(TEXT("requested_package"),W.RequestedPackage);Row->SetStringField(TEXT("requested_filename"),W.RequestedFilename);Row->SetBoolField(TEXT("nonnull_return"),O!=nullptr);Row->SetBoolField(TEXT("identity_verified"),false);W.CreationRows.Add(MakeShared<FJsonValueObject>(Row));W.R.Data->SetBoolField(TEXT("canary_creation_SDK_returned_nonnull"),O!=nullptr);if(!W.After(TEXT("DuplicateAsset")))return nullptr;auto* B=Cast<UBlueprint>(O);if(!B||!W.Pin(B)||!Same(B->GetPathName(),ObjectPath)||!Same(B->GetPackage()->GetName(),W.RequestedPackage)||B->GetOuter()!=B->GetPackage()||B->GetClass()!=Source->GetClass()){W.Fail(TEXT("blueprint_niagara_canary_duplicate_identity"),TEXT("Duplicate returned null or unexpected class/path/outer/package; residual not automatically deleted"));return nullptr;}Row->SetBoolField(TEXT("identity_verified"),true);Row->SetStringField(TEXT("actual_path"),B->GetPathName());W.Target=B;W.R.bChanged=true;if(!W.Blueprint(B)||!PackageOwnership(B))return nullptr;return B;}
bool SaveOwned(UBlueprint* B){auto& W=*Current;if(!W.Admit(TEXT("before_save"))||!W.Idle(B)||!PackageOwnership(B))return false;auto* P=B->GetPackage();UEditorAssetSubsystem* Assets=nullptr;if(!W.Read(TEXT("EditorAssetSubsystem"),[&]{Assets=GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();})||!Assets||!W.Pin(Assets))return false;if(!PackageOwnership(B))return false;const FString Path=B->GetPathName();const FString Package=P->GetName();FString File;FPackageName::TryConvertLongPackageNameToFilename(Package,File,FPackageName::GetAssetPackageExtension());auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("object_path"),Path);Row->SetStringField(TEXT("package"),Package);Row->SetStringField(TEXT("filename"),File);Row->SetBoolField(TEXT("SDK_returned_true"),false);W.SaveRows.Add(MakeShared<FJsonValueObject>(Row));if(!W.SDK(TEXT("SaveLoadedAsset"),true))return false;W.R.bSaveAttempted=true;const bool Saved=Assets->SaveLoadedAsset(B,false);W.R.bSaved|=Saved;Row->SetBoolField(TEXT("SDK_returned_true"),Saved);if(!W.After(TEXT("SaveLoadedAsset")))return false;if(!PackageOwnership(B))return false;Row->SetBoolField(TEXT("dirty_after"),P->IsDirty());if(!Saved)return W.Fail(TEXT("blueprint_niagara_template_save_failed"),TEXT("SaveLoadedAsset returned false; no automatic rollback or retry"));if(P->IsDirty())return W.Fail(TEXT("blueprint_niagara_template_dirty_after_save"),TEXT("Save SDK returned true but package is still dirty; saved prefix retained"));return true;}
bool ByteArrayContainsPattern(const TArray<uint8>& Bytes,const ANSICHAR* P,int32 N){if(N<=0)return false;for(int32 I=0;I<=Bytes.Num()-N;++I)if(FMemory::Memcmp(Bytes.GetData()+I,P,N)==0)return true;return false;}
TSharedPtr<FJsonObject> ScanPackageFileForMCPDependencyMarkers(const FString& PackageName){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("package_name"),PackageName);FString File;const bool Resolved=FPackageName::TryConvertLongPackageNameToFilename(PackageName,File,FPackageName::GetAssetPackageExtension());J->SetBoolField(TEXT("resolved_filename"),Resolved);J->SetStringField(TEXT("path"),File);TArray<TSharedPtr<FJsonValue>> Hits;TArray<uint8> Bytes;TUniquePtr<FArchive> Reader(Resolved?IFileManager::Get().CreateFileReader(*File,FILEREAD_Silent):nullptr);const int64 Size=Reader?Reader->TotalSize():-1;bool Loaded=false;if(Reader&&Size>=0&&Size<=64ll*1024*1024){Bytes.SetNumUninitialized(static_cast<int32>(Size));if(Size>0)Reader->Serialize(Bytes.GetData(),Size);const bool ReadComplete=!Reader->IsError()&&Reader->Tell()==Size&&Reader->TotalSize()==Size;const bool Closed=Reader->Close();Loaded=ReadComplete&&Closed&&!Reader->IsError()&&Bytes.Num()==Size;}J->SetBoolField(TEXT("exists"),Reader.IsValid());J->SetNumberField(TEXT("size_bytes"),Size<0?0:Size);J->SetBoolField(TEXT("same_handle_bounded_read"),Loaded);if(!Loaded){J->SetStringField(TEXT("error"),TEXT("bounded_package_file_scan_unavailable"));J->SetBoolField(TEXT("passed"),false);}else{const TArray<FString> Markers={TEXT("/Script/UnrealMCP"),TEXT("/UnrealMCP"),TEXT("UnrealMCP"),TEXT("MCPUnreal"),TEXT("mcp_unreal"),TEXT("CubelessMCP"),TEXT("ToolsetRegistry")};for(const auto& M:Markers){FTCHARToUTF8 P(*M);if(ByteArrayContainsPattern(Bytes,P.Get(),P.Length()))Hits.Add(MakeShared<FJsonValueString>(M));}J->SetBoolField(TEXT("passed"),Hits.IsEmpty());}J->SetNumberField(TEXT("hit_count"),Hits.Num());J->SetArrayField(TEXT("hits"),Hits);J->SetStringField(TEXT("scope"),TEXT("Bounded direct package bytes only; not transitive dependency/cooked-package proof"));return J;}

TSharedPtr<FJsonObject> HandleInspect(const TSharedPtr<FJsonObject>& Params)
{
    auto& W=*Current;
    TSharedPtr<FJsonObject> EffectiveParams = Params.IsValid() ? Params : MakeShared<FJsonObject>();

    FString BlueprintPathParam;
    if (!EffectiveParams->TryGetStringField(TEXT("blueprint_path"), BlueprintPathParam) || BlueprintPathParam.TrimStartAndEnd().IsEmpty())
    {
        return ErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    const FString BlueprintObjectPath = NormalizeEditorBlueprintObjectPath(BlueprintPathParam);
    const FString BlueprintPackageName = FPackageName::ObjectPathToPackageName(BlueprintObjectPath);
    if (!FPackageName::IsValidLongPackageName(BlueprintPackageName))
    {
        return ErrorResponse(FString::Printf(TEXT("Invalid blueprint_path: %s"), *BlueprintPathParam));
    }

    const FString ComponentNameFilter = GetStringParam(EffectiveParams, TEXT("component_name"), FString());
    const FString ExpectedSystemPathParam = GetStringParam(EffectiveParams, TEXT("expected_system_path"), FString());
    const FString ExpectedSystemObjectPath = ExpectedSystemPathParam.IsEmpty()
        ? FString()
        : NormalizeNiagaraSystemObjectPath(ExpectedSystemPathParam);
    const FString ParameterNameContains = GetStringParam(EffectiveParams, TEXT("parameter_name_contains"), FString());
    const bool bIncludeInheritedScsTemplates = GetBoolParam(EffectiveParams, TEXT("include_inherited_scs_templates"), true);
    const bool bIncludeParameterValues = GetBoolParam(EffectiveParams, TEXT("include_parameter_values"), true);
    const bool bIncludeRawBytes = GetBoolParam(EffectiveParams, TEXT("include_raw_bytes"), false);
    const bool bIncludeParameterObjectProperties = GetBoolParam(EffectiveParams, TEXT("include_parameter_object_properties"), false);
    const bool bIncludeComponentObjectProperties = GetBoolParam(EffectiveParams, TEXT("include_component_object_properties"), false);
    const bool bRequireExactlyOne = GetBoolParam(EffectiveParams, TEXT("require_exactly_one"), false);
    const bool bRequireExpectedSystemMatch = GetBoolParam(EffectiveParams, TEXT("require_expected_system_match"), false);
    const int32 MaxComponents = FMath::Clamp(static_cast<int32>(GetNumberParam(EffectiveParams, TEXT("max_components"), 64.0)), 1, 512);
    const int32 MaxParameters = FMath::Clamp(static_cast<int32>(GetNumberParam(EffectiveParams, TEXT("max_parameters"), 256.0)), 0, 4096);
    const int32 MaxRawBytes = FMath::Clamp(static_cast<int32>(GetNumberParam(EffectiveParams, TEXT("max_raw_bytes"), 64.0)), 0, 4096);
    const int32 MaxObjectProperties = FMath::Clamp(static_cast<int32>(GetNumberParam(EffectiveParams, TEXT("max_object_properties"), 80.0)), 0, 512);
    const int32 MaxArrayElements = FMath::Clamp(static_cast<int32>(GetNumberParam(EffectiveParams, TEXT("max_array_elements"), 16.0)), 0, 256);
    const int32 MaxPropertyValueChars = FMath::Clamp(static_cast<int32>(GetNumberParam(EffectiveParams, TEXT("max_property_value_chars"), 512.0)), 0, 16384);
    if(!ContinueRead())return ErrorResponse(W.R.Error);

    const TArray<FString> DirtyPackagesBeforeCommand = GetDirtyPackageNames();
    const bool bBlueprintPackageLoadedBefore = FindPackage(nullptr, *BlueprintPackageName) != nullptr;
    const bool bBlueprintPackageDirtyBefore = StringArrayContainsExact(DirtyPackagesBeforeCommand, BlueprintPackageName);

    UBlueprint* Blueprint = LoadExact<UBlueprint>(BlueprintObjectPath);
    if (!Blueprint)
    {
        return ErrorResponse(FString::Printf(TEXT("Failed to load Blueprint asset: %s"), *BlueprintObjectPath));
    }

    UBlueprintGeneratedClass* GeneratedClass = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass);
    if (!GeneratedClass)
    {
        return ErrorResponse(FString::Printf(TEXT("Blueprint does not have a BlueprintGeneratedClass: %s"), *BlueprintObjectPath));
    }

    if(!ContinueRead())return ErrorResponse(W.R.Error);
    TArray<UBlueprintGeneratedClass*> ScsOwnerClasses;
    for (UClass* ClassCursor = GeneratedClass; ClassCursor; ClassCursor = ClassCursor->GetSuperClass())
    {
        UBlueprintGeneratedClass* BlueprintGeneratedClass = Cast<UBlueprintGeneratedClass>(ClassCursor);
        if (!BlueprintGeneratedClass)
        {
            break;
        }
        if (BlueprintGeneratedClass->SimpleConstructionScript)
        {
            if(!W.Pin(BlueprintGeneratedClass)||!W.Pin(BlueprintGeneratedClass->SimpleConstructionScript))return ErrorResponse(W.R.Error);
            ScsOwnerClasses.Add(BlueprintGeneratedClass);
        }
        if (!bIncludeInheritedScsTemplates)
        {
            break;
        }
    }

    int32 ScannedScsNodeCount = 0;
    int32 ScannedNiagaraTemplateCount = 0;
    int32 MatchingComponentTemplateCount = 0;
    int32 ExpectedSystemMatchCount = 0;
    int32 ReturnedComponentTemplateCount = 0;
    bool bTruncated = false;
    TArray<TSharedPtr<FJsonValue>> ComponentResults;

    for (UBlueprintGeneratedClass* ScsOwnerClass : ScsOwnerClasses)
    {
        if (!ScsOwnerClass || !ScsOwnerClass->SimpleConstructionScript)
        {
            continue;
        }

        const TArray<USCS_Node*> Nodes = CaptureNodes(ScsOwnerClass);
        if(!ContinueRead())return ErrorResponse(W.R.Error);
        for (USCS_Node* Node : Nodes)
        {
            if (!Node)
            {
                continue;
            }

            ++ScannedScsNodeCount;
            UActorComponent* ComponentTemplate = ActualTemplate(Node,GeneratedClass);
            if(!ContinueRead())return ErrorResponse(W.R.Error);
            UNiagaraComponent* NiagaraTemplate = Cast<UNiagaraComponent>(ComponentTemplate);
            if (!NiagaraTemplate)
            {
                continue;
            }

            ++ScannedNiagaraTemplateCount;
            const FString ScsVariableName = Node->GetVariableName().ToString();
            const FString ComponentTemplateName = NiagaraTemplate->GetName();
            const FString ComponentTemplatePath = NiagaraTemplate->GetPathName();
            const FString ComponentSearchText = FString::Printf(
                TEXT("%s|%s|%s|%s"),
                *ScsVariableName,
                *ComponentTemplateName,
                *ComponentTemplatePath,
                *NiagaraRuntimeStateClassName(NiagaraTemplate));
            if (!NiagaraRuntimeStateMatchesFilter(ComponentSearchText, ComponentNameFilter))
            {
                continue;
            }

            ++MatchingComponentTemplateCount;
            UNiagaraSystem* AssignedSystem = NiagaraTemplate->GetAsset();
            const FString AssignedSystemPath = NiagaraRuntimeStateObjectPath(AssignedSystem);
            const bool bExpectedSystemProvided = !ExpectedSystemObjectPath.IsEmpty();
            const bool bExpectedSystemMatches = bExpectedSystemProvided &&
                AssignedSystemPath.Equals(ExpectedSystemObjectPath, ESearchCase::IgnoreCase);
            if (bExpectedSystemMatches)
            {
                ++ExpectedSystemMatchCount;
            }

            if (ReturnedComponentTemplateCount >= MaxComponents)
            {
                bTruncated = true;
                continue;
            }

            TSharedPtr<FJsonObject> ComponentObject = MakeShared<FJsonObject>();
            ComponentObject->SetStringField(TEXT("source"), TEXT("simple_construction_script"));
            ComponentObject->SetBoolField(TEXT("read_only"), true);
            ComponentObject->SetStringField(TEXT("scs_variable_name"), ScsVariableName);
            ComponentObject->SetStringField(TEXT("scs_node_path"), Node->GetPathName());
            ComponentObject->SetStringField(TEXT("source_generated_class"), ScsOwnerClass->GetPathName());
            ComponentObject->SetBoolField(TEXT("inherited_scs_template"), ScsOwnerClass != GeneratedClass);
            ComponentObject->SetStringField(TEXT("component_name"), ScsVariableName.IsEmpty() ? ComponentTemplateName : ScsVariableName);
            ComponentObject->SetStringField(TEXT("component_template_name"), ComponentTemplateName);
            ComponentObject->SetStringField(TEXT("component_template_path"), ComponentTemplatePath);
            ComponentObject->SetStringField(TEXT("component_class"), NiagaraRuntimeStateClassName(NiagaraTemplate));
            ComponentObject->SetNumberField(TEXT("creation_method"), static_cast<int32>(NiagaraTemplate->CreationMethod));
            ComponentObject->SetBoolField(TEXT("auto_activate"), NiagaraTemplate->bAutoActivate);
            ComponentObject->SetBoolField(TEXT("visible"), NiagaraTemplate->IsVisible());
            ComponentObject->SetBoolField(TEXT("component_tick_enabled"), NiagaraTemplate->IsComponentTickEnabled());
            ComponentObject->SetStringField(TEXT("asset_path"), AssignedSystemPath);
            ComponentObject->SetStringField(TEXT("asset_class"), NiagaraRuntimeStateClassName(AssignedSystem));
            ComponentObject->SetBoolField(TEXT("expected_system_match_checked"), bExpectedSystemProvided);
            ComponentObject->SetBoolField(TEXT("expected_system_matches"), bExpectedSystemMatches);
            ComponentObject->SetStringField(TEXT("expected_system_path"), ExpectedSystemObjectPath);
            AddBoxField(ComponentObject, TEXT("component_system_fixed_bounds"), NiagaraTemplate->GetSystemFixedBounds());
            if (AssignedSystem)
            {
                ComponentObject->SetBoolField(TEXT("asset_fixed_bounds_enabled"), AssignedSystem->bFixedBounds);
                AddBoxField(ComponentObject, TEXT("asset_fixed_bounds"), AssignedSystem->GetFixedBounds());
            }
            if (USceneComponent* AttachParent = NiagaraTemplate->GetAttachParent())
            {
                ComponentObject->SetStringField(TEXT("attach_parent_template_path"), AttachParent->GetPathName());
            }
            AddVectorField(ComponentObject, TEXT("relative_location"), NiagaraTemplate->GetRelativeLocation());
            AddRotatorField(ComponentObject, TEXT("relative_rotation"), NiagaraTemplate->GetRelativeRotation());
            AddVectorField(ComponentObject, TEXT("relative_scale"), NiagaraTemplate->GetRelativeScale3D());

            W.R.Data->SetObjectField(TEXT("last_component_partial"),ComponentObject);
            AddNiagaraRuntimeStateStore(
                ComponentObject,
                TEXT("override_parameters"),
                NiagaraTemplate->GetOverrideParameters(),
                ParameterNameContains,
                MaxParameters,
                bIncludeParameterValues,
                bIncludeRawBytes,
                MaxRawBytes,
                bIncludeParameterObjectProperties,
                MaxObjectProperties,
                MaxArrayElements,
                MaxPropertyValueChars);
            if(!ContinueRead())return ErrorResponse(W.R.Error);

            if (bIncludeComponentObjectProperties)
            {
                ComponentObject->SetObjectField(
                    TEXT("component_object_properties"),
                    BuildNiagaraRuntimeStateObjectPropertyPreview(
                        NiagaraTemplate,
                        MaxObjectProperties,
                        MaxArrayElements,
                        MaxPropertyValueChars));
                if(!ContinueRead())return ErrorResponse(W.R.Error);
            }

            ComponentResults.Add(MakeShared<FJsonValueObject>(ComponentObject));
            ++ReturnedComponentTemplateCount;
        }
    }

    if(!ContinueRead())return ErrorResponse(W.R.Error);
    UPackage* BlueprintPackage = Blueprint->GetOutermost();
    const bool bBlueprintPackageDirtyAfter = BlueprintPackage ? BlueprintPackage->IsDirty() : StringArrayContainsExact(GetDirtyPackageNames(), BlueprintPackageName);
    const bool bBlueprintPackageDirtyAddedByCommand = !bBlueprintPackageDirtyBefore && bBlueprintPackageDirtyAfter;
    const bool bExpectedSystemProvided = !ExpectedSystemObjectPath.IsEmpty();

    TArray<FString> PreflightFailureReasons;
    if (bRequireExactlyOne && MatchingComponentTemplateCount != 1)
    {
        PreflightFailureReasons.Add(FString::Printf(TEXT("require_exactly_one expected 1 matching Niagara component template but found %d"), MatchingComponentTemplateCount));
    }
    if (bRequireExpectedSystemMatch && !bExpectedSystemProvided)
    {
        PreflightFailureReasons.Add(TEXT("require_expected_system_match needs expected_system_path"));
    }
    if (bRequireExpectedSystemMatch && bExpectedSystemProvided && MatchingComponentTemplateCount == 0)
    {
        PreflightFailureReasons.Add(TEXT("require_expected_system_match found no matching Niagara component templates to verify"));
    }
    if (bRequireExpectedSystemMatch && bExpectedSystemProvided && ExpectedSystemMatchCount != MatchingComponentTemplateCount)
    {
        PreflightFailureReasons.Add(FString::Printf(TEXT("expected_system_path matched %d of %d matching Niagara component templates"), ExpectedSystemMatchCount, MatchingComponentTemplateCount));
    }
    const bool bPreflightPass = PreflightFailureReasons.IsEmpty();

    TSharedPtr<FJsonObject> Filters = MakeShared<FJsonObject>();
    Filters->SetStringField(TEXT("blueprint_path"), BlueprintPathParam);
    Filters->SetStringField(TEXT("component_name"), ComponentNameFilter);
    Filters->SetStringField(TEXT("expected_system_path"), ExpectedSystemPathParam);
    Filters->SetStringField(TEXT("expected_system_object_path"), ExpectedSystemObjectPath);
    Filters->SetStringField(TEXT("parameter_name_contains"), ParameterNameContains);
    Filters->SetStringField(TEXT("component_name_filter_mode"), TEXT("substring_case_insensitive"));

    if(!ContinueRead())return ErrorResponse(W.R.Error);
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetBoolField(TEXT("preflight_pass"), bPreflightPass);
    ResultObj->SetArrayField(TEXT("preflight_failure_reasons"), StringArrayToJsonArray(PreflightFailureReasons));
    ResultObj->SetStringField(TEXT("blueprint_path"), BlueprintPathParam);
    ResultObj->SetStringField(TEXT("blueprint_object_path"), BlueprintObjectPath);
    ResultObj->SetStringField(TEXT("blueprint_package_name"), BlueprintPackageName);
    ResultObj->SetStringField(TEXT("blueprint_class"), GeneratedClass->GetPathName());
    ResultObj->SetBoolField(TEXT("blueprint_package_loaded_before"), bBlueprintPackageLoadedBefore);
    ResultObj->SetBoolField(TEXT("blueprint_package_dirty_before"), bBlueprintPackageDirtyBefore);
    ResultObj->SetBoolField(TEXT("blueprint_package_dirty_after"), bBlueprintPackageDirtyAfter);
    ResultObj->SetBoolField(TEXT("blueprint_package_dirty_added_by_command"), bBlueprintPackageDirtyAddedByCommand);
    ResultObj->SetBoolField(TEXT("include_inherited_scs_templates"), bIncludeInheritedScsTemplates);
    ResultObj->SetNumberField(TEXT("scs_owner_class_count"), ScsOwnerClasses.Num());
    ResultObj->SetNumberField(TEXT("scs_node_count_scanned"), ScannedScsNodeCount);
    ResultObj->SetNumberField(TEXT("niagara_component_template_count_scanned"), ScannedNiagaraTemplateCount);
    ResultObj->SetNumberField(TEXT("matching_component_template_count"), MatchingComponentTemplateCount);
    ResultObj->SetNumberField(TEXT("expected_system_match_count"), ExpectedSystemMatchCount);
    ResultObj->SetNumberField(TEXT("component_template_count"), ReturnedComponentTemplateCount);
    ResultObj->SetNumberField(TEXT("max_components"), MaxComponents);
    ResultObj->SetBoolField(TEXT("truncated"), bTruncated);
    ResultObj->SetNumberField(TEXT("max_parameters"), MaxParameters);
    ResultObj->SetBoolField(TEXT("include_parameter_values"), bIncludeParameterValues);
    ResultObj->SetBoolField(TEXT("include_raw_bytes"), bIncludeRawBytes);
    ResultObj->SetBoolField(TEXT("include_parameter_object_properties"), bIncludeParameterObjectProperties);
    ResultObj->SetBoolField(TEXT("include_component_object_properties"), bIncludeComponentObjectProperties);
    ResultObj->SetNumberField(TEXT("max_object_properties"), MaxObjectProperties);
    ResultObj->SetNumberField(TEXT("max_array_elements"), MaxArrayElements);
    ResultObj->SetNumberField(TEXT("max_property_value_chars"), MaxPropertyValueChars);
            if(!ContinueRead())return ErrorResponse(W.R.Error);
    ResultObj->SetBoolField(TEXT("require_exactly_one"), bRequireExactlyOne);
    ResultObj->SetBoolField(TEXT("require_expected_system_match"), bRequireExpectedSystemMatch);
    ResultObj->SetObjectField(TEXT("filters"), Filters);
    ResultObj->SetArrayField(TEXT("components"), ComponentResults);
    ResultObj->SetBoolField(TEXT("read_only"), true);
    ResultObj->SetBoolField(TEXT("loads_blueprint_asset"), true);
    ResultObj->SetBoolField(TEXT("saves_or_reloads_packages"), false);
    ResultObj->SetBoolField(TEXT("blueprint_was_saved"), false);
    ResultObj->SetBoolField(TEXT("blueprint_was_compiled"), false);
    ResultObj->SetBoolField(TEXT("asset_mutation_admitted"), false);
    ResultObj->SetBoolField(TEXT("blueprint_asset_write_admitted"), false);
    ResultObj->SetStringField(TEXT("safety_note"), TEXT("Loads the Blueprint asset for read-only SCS/template inspection, reports Niagara component template asset assignment and override parameter store, and never saves, reloads, compiles, or mutates the Blueprint."));
    AddDirtyPackageCommandDelta(ResultObj, DirtyPackagesBeforeCommand);
    return ResultObj;
}

TSharedPtr<FJsonObject> HandleDryRun(const TSharedPtr<FJsonObject>& Params)
{
    auto& W=*Current;
    TSharedPtr<FJsonObject> EffectiveParams = Params.IsValid() ? Params : MakeShared<FJsonObject>();

    const FString Operation = GetStringParam(EffectiveParams, TEXT("operation"), FString());
    if (!Operation.IsEmpty() && Operation != TEXT("durable_bp_component_template_asset_swap"))
    {
        return ErrorResponse(TEXT("operation must be durable_bp_component_template_asset_swap for this dry-run preflight"));
    }

    const bool bDryRun = GetBoolParam(EffectiveParams, TEXT("dry_run"), true);
    if (!bDryRun)
    {
        return ErrorResponse(TEXT("dry_run_blueprint_niagara_component_template_asset_swap only admits dry_run=true; no persistent Blueprint asset mutation is performed by this command."));
    }

    const FString ConfirmationToken = GetStringParam(EffectiveParams, TEXT("confirmation_token"), FString());
    if (ConfirmationToken != TEXT("ALLOW_DURABLE_BP_COMPONENT_TEMPLATE_MUTATION"))
    {
        return ErrorResponse(TEXT("confirmation_token must be ALLOW_DURABLE_BP_COMPONENT_TEMPLATE_MUTATION for the reviewed BP component-template mutation dry-run."));
    }

    const bool bPreserveOverrideParameters = GetBoolParam(EffectiveParams, TEXT("preserve_override_parameters"), true);
    if (!bPreserveOverrideParameters)
    {
        return ErrorResponse(TEXT("preserve_override_parameters must remain true; dropping component template override state is not admitted."));
    }

    if (GetBoolParam(EffectiveParams, TEXT("allow_unrealmcp_dependency"), false))
    {
        return ErrorResponse(TEXT("allow_unrealmcp_dependency must remain false; finished assets must not depend on UnrealMCP."));
    }

    FString BlueprintPathParam;
    if (!EffectiveParams->TryGetStringField(TEXT("blueprint_path"), BlueprintPathParam) || BlueprintPathParam.TrimStartAndEnd().IsEmpty())
    {
        return ErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    FString ComponentNameFilter;
    if (!EffectiveParams->TryGetStringField(TEXT("component_name"), ComponentNameFilter) || ComponentNameFilter.TrimStartAndEnd().IsEmpty())
    {
        return ErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    FString SourceSystemPathParam;
    if (!EffectiveParams->TryGetStringField(TEXT("source_system_path"), SourceSystemPathParam) || SourceSystemPathParam.TrimStartAndEnd().IsEmpty())
    {
        return ErrorResponse(TEXT("Missing 'source_system_path' parameter"));
    }

    FString ReplacementSystemPathParam;
    if (!EffectiveParams->TryGetStringField(TEXT("replacement_system_path"), ReplacementSystemPathParam) || ReplacementSystemPathParam.TrimStartAndEnd().IsEmpty())
    {
        return ErrorResponse(TEXT("Missing 'replacement_system_path' parameter"));
    }

    if (Forbidden(BlueprintPathParam))
    {
        return ErrorResponse(DependencyError(TEXT("blueprint_path"), BlueprintPathParam));
    }
    if (Forbidden(SourceSystemPathParam))
    {
        return ErrorResponse(DependencyError(TEXT("source_system_path"), SourceSystemPathParam));
    }
    if (Forbidden(ReplacementSystemPathParam))
    {
        return ErrorResponse(DependencyError(TEXT("replacement_system_path"), ReplacementSystemPathParam));
    }

    const FString BlueprintObjectPath = NormalizeEditorBlueprintObjectPath(BlueprintPathParam);
    const FString BlueprintPackageName = FPackageName::ObjectPathToPackageName(BlueprintObjectPath);
    if (!FPackageName::IsValidLongPackageName(BlueprintPackageName))
    {
        return ErrorResponse(FString::Printf(TEXT("Invalid blueprint_path: %s"), *BlueprintPathParam));
    }
    if (Forbidden(BlueprintObjectPath))
    {
        return ErrorResponse(DependencyError(TEXT("blueprint_path"), BlueprintObjectPath));
    }

    const FString SourceSystemObjectPath = NormalizeNiagaraSystemObjectPath(SourceSystemPathParam);
    const FString ReplacementSystemObjectPath = NormalizeNiagaraSystemObjectPath(ReplacementSystemPathParam);
    if (Forbidden(SourceSystemObjectPath))
    {
        return ErrorResponse(DependencyError(TEXT("source_system_path"), SourceSystemObjectPath));
    }
    if (Forbidden(ReplacementSystemObjectPath))
    {
        return ErrorResponse(DependencyError(TEXT("replacement_system_path"), ReplacementSystemObjectPath));
    }

    UNiagaraSystem* SourceSystem = LoadExact<UNiagaraSystem>(SourceSystemObjectPath);
    if(!ContinueRead())return ErrorResponse(W.R.Error); W.SourceSystem=SourceSystem;
    if (!SourceSystem)
    {
        return ErrorResponse(FString::Printf(TEXT("Failed to load source Niagara system: %s"), *SourceSystemObjectPath));
    }

    UNiagaraSystem* ReplacementSystem = LoadExact<UNiagaraSystem>(ReplacementSystemObjectPath);
    if(!ContinueRead())return ErrorResponse(W.R.Error); W.Replacement=ReplacementSystem;
    if (!ReplacementSystem)
    {
        return ErrorResponse(FString::Printf(TEXT("Failed to load replacement Niagara system: %s"), *ReplacementSystemObjectPath));
    }

    const FString ParameterNameContains = GetStringParam(EffectiveParams, TEXT("parameter_name_contains"), FString());
    const bool bIncludeInheritedScsTemplates = GetBoolParam(EffectiveParams, TEXT("include_inherited_scs_templates"), true);
    const bool bRequireExactlyOne = GetBoolParam(EffectiveParams, TEXT("require_exactly_one"), true);
    const bool bRequireSourceSystemMatch = GetBoolParam(EffectiveParams, TEXT("require_source_system_match"), true);
    const bool bIncludeParameterValues = GetBoolParam(EffectiveParams, TEXT("include_parameter_values"), true);
    const bool bIncludeRawBytes = GetBoolParam(EffectiveParams, TEXT("include_raw_bytes"), false);
    const bool bIncludeParameterObjectProperties = GetBoolParam(EffectiveParams, TEXT("include_parameter_object_properties"), false);
    const bool bIncludeComponentObjectProperties = GetBoolParam(EffectiveParams, TEXT("include_component_object_properties"), false);
    const int32 MaxComponents = FMath::Clamp(static_cast<int32>(GetNumberParam(EffectiveParams, TEXT("max_components"), 8.0)), 1, 64);
    const int32 MaxParameters = FMath::Clamp(static_cast<int32>(GetNumberParam(EffectiveParams, TEXT("max_parameters"), 256.0)), 0, 4096);
    const int32 MaxRawBytes = FMath::Clamp(static_cast<int32>(GetNumberParam(EffectiveParams, TEXT("max_raw_bytes"), 64.0)), 0, 4096);
    const int32 MaxObjectProperties = FMath::Clamp(static_cast<int32>(GetNumberParam(EffectiveParams, TEXT("max_object_properties"), 80.0)), 0, 512);
    const int32 MaxArrayElements = FMath::Clamp(static_cast<int32>(GetNumberParam(EffectiveParams, TEXT("max_array_elements"), 16.0)), 0, 256);
    const int32 MaxPropertyValueChars = FMath::Clamp(static_cast<int32>(GetNumberParam(EffectiveParams, TEXT("max_property_value_chars"), 512.0)), 0, 16384);
    if(!ContinueRead())return ErrorResponse(W.R.Error);

    const TArray<FString> DirtyPackagesBeforeCommand = GetDirtyPackageNames();
    const bool bBlueprintPackageLoadedBefore = FindPackage(nullptr, *BlueprintPackageName) != nullptr;
    const bool bBlueprintPackageDirtyBefore = StringArrayContainsExact(DirtyPackagesBeforeCommand, BlueprintPackageName);

    UBlueprint* Blueprint = LoadExact<UBlueprint>(BlueprintObjectPath);
    if (!Blueprint)
    {
        return ErrorResponse(FString::Printf(TEXT("Failed to load Blueprint asset: %s"), *BlueprintObjectPath));
    }

    UBlueprintGeneratedClass* GeneratedClass = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass);
    if (!GeneratedClass)
    {
        return ErrorResponse(FString::Printf(TEXT("Blueprint does not have a BlueprintGeneratedClass: %s"), *BlueprintObjectPath));
    }

    if(!ContinueRead())return ErrorResponse(W.R.Error);
    TArray<UBlueprintGeneratedClass*> ScsOwnerClasses;
    for (UClass* ClassCursor = GeneratedClass; ClassCursor; ClassCursor = ClassCursor->GetSuperClass())
    {
        UBlueprintGeneratedClass* BlueprintGeneratedClass = Cast<UBlueprintGeneratedClass>(ClassCursor);
        if (!BlueprintGeneratedClass)
        {
            break;
        }
        if (BlueprintGeneratedClass->SimpleConstructionScript)
        {
            if(!W.Pin(BlueprintGeneratedClass)||!W.Pin(BlueprintGeneratedClass->SimpleConstructionScript))return ErrorResponse(W.R.Error);
            ScsOwnerClasses.Add(BlueprintGeneratedClass);
        }
        if (!bIncludeInheritedScsTemplates)
        {
            break;
        }
    }

    int32 ScannedScsNodeCount = 0;
    int32 ScannedNiagaraTemplateCount = 0;
    int32 MatchingComponentTemplateCount = 0;
    int32 SourceSystemMatchCount = 0;
    int32 ReturnedComponentTemplateCount = 0;
    bool bTruncated = false;
    TArray<TSharedPtr<FJsonValue>> ComponentResults;

    for (UBlueprintGeneratedClass* ScsOwnerClass : ScsOwnerClasses)
    {
        if (!ScsOwnerClass || !ScsOwnerClass->SimpleConstructionScript)
        {
            continue;
        }

        const TArray<USCS_Node*> Nodes = CaptureNodes(ScsOwnerClass);
        if(!ContinueRead())return ErrorResponse(W.R.Error);
        for (USCS_Node* Node : Nodes)
        {
            if (!Node)
            {
                continue;
            }

            ++ScannedScsNodeCount;
            UActorComponent* ComponentTemplate = ActualTemplate(Node,GeneratedClass);
            if(!ContinueRead())return ErrorResponse(W.R.Error);
            UNiagaraComponent* NiagaraTemplate = Cast<UNiagaraComponent>(ComponentTemplate);
            if (!NiagaraTemplate)
            {
                continue;
            }

            ++ScannedNiagaraTemplateCount;
            const FString ScsVariableName = Node->GetVariableName().ToString();
            const FString ComponentTemplateName = NiagaraTemplate->GetName();
            const FString ComponentTemplatePath = NiagaraTemplate->GetPathName();
            const FString ComponentSearchText = FString::Printf(
                TEXT("%s|%s|%s|%s"),
                *ScsVariableName,
                *ComponentTemplateName,
                *ComponentTemplatePath,
                *NiagaraRuntimeStateClassName(NiagaraTemplate));
            if (!NiagaraRuntimeStateMatchesFilter(ComponentSearchText, ComponentNameFilter))
            {
                continue;
            }

            ++MatchingComponentTemplateCount;
            UNiagaraSystem* AssignedSystem = NiagaraTemplate->GetAsset();
            const FString AssignedSystemPath = NiagaraRuntimeStateObjectPath(AssignedSystem);
            const bool bSourceSystemMatches = AssignedSystemPath.Equals(SourceSystemObjectPath, ESearchCase::IgnoreCase);
            if (bSourceSystemMatches)
            {
                ++SourceSystemMatchCount;
            }

            if (ReturnedComponentTemplateCount >= MaxComponents)
            {
                bTruncated = true;
                continue;
            }

            TSharedPtr<FJsonObject> BeforeObject = MakeShared<FJsonObject>();
            BeforeObject->SetStringField(TEXT("phase"), TEXT("before_dry_run"));
            BeforeObject->SetStringField(TEXT("asset_path"), AssignedSystemPath);
            BeforeObject->SetStringField(TEXT("asset_class"), NiagaraRuntimeStateClassName(AssignedSystem));
            BeforeObject->SetBoolField(TEXT("source_system_matches"), bSourceSystemMatches);
            BeforeObject->SetBoolField(TEXT("has_component"), true);
            BeforeObject->SetStringField(TEXT("component_name"), ScsVariableName.IsEmpty() ? ComponentTemplateName : ScsVariableName);
            BeforeObject->SetStringField(TEXT("component_template_name"), ComponentTemplateName);
            BeforeObject->SetStringField(TEXT("component_template_path"), ComponentTemplatePath);
            BeforeObject->SetStringField(TEXT("component_class"), NiagaraRuntimeStateClassName(NiagaraTemplate));
            BeforeObject->SetStringField(TEXT("scs_variable_name"), ScsVariableName);
            BeforeObject->SetStringField(TEXT("scs_node_path"), Node->GetPathName());
            BeforeObject->SetStringField(TEXT("source_generated_class"), ScsOwnerClass->GetPathName());
            BeforeObject->SetBoolField(TEXT("inherited_scs_template"), ScsOwnerClass != GeneratedClass);
            BeforeObject->SetBoolField(TEXT("auto_activate"), NiagaraTemplate->bAutoActivate);
            BeforeObject->SetBoolField(TEXT("visible"), NiagaraTemplate->IsVisible());
            BeforeObject->SetBoolField(TEXT("component_tick_enabled"), NiagaraTemplate->IsComponentTickEnabled());
            AddVectorField(BeforeObject, TEXT("relative_location"), NiagaraTemplate->GetRelativeLocation());
            AddRotatorField(BeforeObject, TEXT("relative_rotation"), NiagaraTemplate->GetRelativeRotation());
            AddVectorField(BeforeObject, TEXT("relative_scale"), NiagaraTemplate->GetRelativeScale3D());
            W.R.Data->SetObjectField(TEXT("last_component_partial"),BeforeObject);
            AddNiagaraRuntimeStateStore(
                BeforeObject,
                TEXT("override_parameters"),
                NiagaraTemplate->GetOverrideParameters(),
                ParameterNameContains,
                MaxParameters,
                bIncludeParameterValues,
                bIncludeRawBytes,
                MaxRawBytes,
                bIncludeParameterObjectProperties,
                MaxObjectProperties,
                MaxArrayElements,
                MaxPropertyValueChars);
            if(!ContinueRead())return ErrorResponse(W.R.Error);

            if (bIncludeComponentObjectProperties)
            {
                BeforeObject->SetObjectField(
                    TEXT("component_object_properties"),
                    BuildNiagaraRuntimeStateObjectPropertyPreview(
                        NiagaraTemplate,
                        MaxObjectProperties,
                        MaxArrayElements,
                        MaxPropertyValueChars));
                if(!ContinueRead())return ErrorResponse(W.R.Error);
            }

            TSharedPtr<FJsonObject> PlannedAfterObject = MakeShared<FJsonObject>();
            PlannedAfterObject->SetStringField(TEXT("phase"), TEXT("planned_after_dry_run"));
            PlannedAfterObject->SetStringField(TEXT("asset_path"), ReplacementSystemObjectPath);
            PlannedAfterObject->SetStringField(TEXT("asset_class"), NiagaraRuntimeStateClassName(ReplacementSystem));
            PlannedAfterObject->SetBoolField(TEXT("would_set_asset"), true);
            PlannedAfterObject->SetBoolField(TEXT("would_preserve_override_parameters"), bPreserveOverrideParameters);
            PlannedAfterObject->SetBoolField(TEXT("would_compile_blueprint"), false);
            PlannedAfterObject->SetBoolField(TEXT("would_save_blueprint"), false);
            PlannedAfterObject->SetStringField(TEXT("component_name"), ScsVariableName.IsEmpty() ? ComponentTemplateName : ScsVariableName);
            PlannedAfterObject->SetStringField(TEXT("component_template_path"), ComponentTemplatePath);

            TSharedPtr<FJsonObject> ComponentObject = MakeShared<FJsonObject>();
            ComponentObject->SetStringField(TEXT("source"), TEXT("simple_construction_script"));
            ComponentObject->SetBoolField(TEXT("read_only"), true);
            ComponentObject->SetBoolField(TEXT("dry_run"), true);
            ComponentObject->SetStringField(TEXT("component_name"), ScsVariableName.IsEmpty() ? ComponentTemplateName : ScsVariableName);
            ComponentObject->SetStringField(TEXT("component_template_name"), ComponentTemplateName);
            ComponentObject->SetStringField(TEXT("component_template_path"), ComponentTemplatePath);
            ComponentObject->SetStringField(TEXT("scs_variable_name"), ScsVariableName);
            ComponentObject->SetStringField(TEXT("current_asset_path"), AssignedSystemPath);
            ComponentObject->SetStringField(TEXT("source_system_path"), SourceSystemObjectPath);
            ComponentObject->SetStringField(TEXT("replacement_system_path"), ReplacementSystemObjectPath);
            ComponentObject->SetBoolField(TEXT("source_system_matches"), bSourceSystemMatches);
            ComponentObject->SetBoolField(TEXT("asset_mutation_performed"), false);
            ComponentObject->SetObjectField(TEXT("before"), BeforeObject);
            ComponentObject->SetObjectField(TEXT("planned_after"), PlannedAfterObject);
            ComponentResults.Add(MakeShared<FJsonValueObject>(ComponentObject));
            ++ReturnedComponentTemplateCount;
        }
    }

    TArray<FString> PreflightFailureReasons;
    if (bRequireExactlyOne && MatchingComponentTemplateCount != 1)
    {
        PreflightFailureReasons.Add(FString::Printf(TEXT("require_exactly_one expected 1 matching Niagara component template but found %d"), MatchingComponentTemplateCount));
    }
    if (bRequireSourceSystemMatch && MatchingComponentTemplateCount == 0)
    {
        PreflightFailureReasons.Add(TEXT("require_source_system_match found no matching Niagara component templates to verify"));
    }
    if (bRequireSourceSystemMatch && MatchingComponentTemplateCount > 0 && SourceSystemMatchCount != MatchingComponentTemplateCount)
    {
        PreflightFailureReasons.Add(FString::Printf(TEXT("source_system_path matched %d of %d matching Niagara component templates"), SourceSystemMatchCount, MatchingComponentTemplateCount));
    }
    const bool bPreflightPass = PreflightFailureReasons.IsEmpty();

    if(!ContinueRead())return ErrorResponse(W.R.Error);
    UPackage* BlueprintPackage = Blueprint->GetOutermost();
    const bool bBlueprintPackageDirtyAfter = BlueprintPackage ? BlueprintPackage->IsDirty() : StringArrayContainsExact(GetDirtyPackageNames(), BlueprintPackageName);
    const bool bBlueprintPackageDirtyAddedByCommand = !bBlueprintPackageDirtyBefore && bBlueprintPackageDirtyAfter;

    TSharedPtr<FJsonObject> PlannedMutation = MakeShared<FJsonObject>();
    PlannedMutation->SetStringField(TEXT("operation"), TEXT("durable_bp_component_template_asset_swap"));
    PlannedMutation->SetBoolField(TEXT("dry_run"), true);
    PlannedMutation->SetBoolField(TEXT("preflight_pass"), bPreflightPass);
    PlannedMutation->SetStringField(TEXT("blueprint_path"), BlueprintPathParam);
    PlannedMutation->SetStringField(TEXT("blueprint_object_path"), BlueprintObjectPath);
    PlannedMutation->SetStringField(TEXT("component_name"), ComponentNameFilter);
    PlannedMutation->SetStringField(TEXT("source_system_path"), SourceSystemObjectPath);
    PlannedMutation->SetStringField(TEXT("replacement_system_path"), ReplacementSystemObjectPath);
    PlannedMutation->SetBoolField(TEXT("preserve_override_parameters"), bPreserveOverrideParameters);
    PlannedMutation->SetBoolField(TEXT("would_set_component_template_asset"), bPreflightPass);
    PlannedMutation->SetBoolField(TEXT("would_compile_blueprint"), false);
    PlannedMutation->SetBoolField(TEXT("would_save_blueprint"), false);

    if(!ContinueRead())return ErrorResponse(W.R.Error);
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetBoolField(TEXT("dry_run"), true);
    ResultObj->SetBoolField(TEXT("preflight_pass"), bPreflightPass);
    ResultObj->SetArrayField(TEXT("preflight_failure_reasons"), StringArrayToJsonArray(PreflightFailureReasons));
    ResultObj->SetStringField(TEXT("operation"), TEXT("durable_bp_component_template_asset_swap"));
    ResultObj->SetStringField(TEXT("blueprint_path"), BlueprintPathParam);
    ResultObj->SetStringField(TEXT("blueprint_object_path"), BlueprintObjectPath);
    ResultObj->SetStringField(TEXT("blueprint_package_name"), BlueprintPackageName);
    ResultObj->SetStringField(TEXT("blueprint_class"), GeneratedClass->GetPathName());
    ResultObj->SetBoolField(TEXT("blueprint_package_loaded_before"), bBlueprintPackageLoadedBefore);
    ResultObj->SetBoolField(TEXT("blueprint_package_dirty_before"), bBlueprintPackageDirtyBefore);
    ResultObj->SetBoolField(TEXT("blueprint_package_dirty_after"), bBlueprintPackageDirtyAfter);
    ResultObj->SetBoolField(TEXT("blueprint_package_dirty_added_by_command"), bBlueprintPackageDirtyAddedByCommand);
    ResultObj->SetStringField(TEXT("component_name"), ComponentNameFilter);
    ResultObj->SetStringField(TEXT("source_system_path"), SourceSystemObjectPath);
    ResultObj->SetStringField(TEXT("replacement_system_path"), ReplacementSystemObjectPath);
    ResultObj->SetBoolField(TEXT("source_system_asset_exists"), SourceSystem != nullptr);
    ResultObj->SetBoolField(TEXT("replacement_system_asset_exists"), ReplacementSystem != nullptr);
    ResultObj->SetBoolField(TEXT("preserve_override_parameters"), bPreserveOverrideParameters);
    ResultObj->SetBoolField(TEXT("include_inherited_scs_templates"), bIncludeInheritedScsTemplates);
    ResultObj->SetNumberField(TEXT("scs_owner_class_count"), ScsOwnerClasses.Num());
    ResultObj->SetNumberField(TEXT("scs_node_count_scanned"), ScannedScsNodeCount);
    ResultObj->SetNumberField(TEXT("niagara_component_template_count_scanned"), ScannedNiagaraTemplateCount);
    ResultObj->SetNumberField(TEXT("matching_component_template_count"), MatchingComponentTemplateCount);
    ResultObj->SetNumberField(TEXT("source_system_match_count"), SourceSystemMatchCount);
    ResultObj->SetNumberField(TEXT("component_template_count"), ReturnedComponentTemplateCount);
    ResultObj->SetNumberField(TEXT("max_components"), MaxComponents);
    ResultObj->SetBoolField(TEXT("truncated"), bTruncated);
    ResultObj->SetObjectField(TEXT("planned_mutation"), PlannedMutation);
    ResultObj->SetArrayField(TEXT("components"), ComponentResults);
    ResultObj->SetBoolField(TEXT("read_only"), true);
    ResultObj->SetBoolField(TEXT("loads_blueprint_asset"), true);
    ResultObj->SetBoolField(TEXT("loads_source_system_asset"), true);
    ResultObj->SetBoolField(TEXT("loads_replacement_system_asset"), true);
    ResultObj->SetBoolField(TEXT("saves_or_reloads_packages"), false);
    ResultObj->SetBoolField(TEXT("blueprint_was_saved"), false);
    ResultObj->SetBoolField(TEXT("blueprint_was_compiled"), false);
    ResultObj->SetBoolField(TEXT("asset_mutation_admitted"), false);
    ResultObj->SetBoolField(TEXT("asset_mutation_performed"), false);
    ResultObj->SetBoolField(TEXT("blueprint_asset_write_admitted"), false);
    ResultObj->SetBoolField(TEXT("dry_run_executor_surface_exists"), true);
    ResultObj->SetBoolField(TEXT("actual_bp_asset_write_tool_exists"), false);
    ResultObj->SetBoolField(TEXT("durable_mutation_admitted"), false);
    ResultObj->SetStringField(TEXT("confirmation_token"), ConfirmationToken);
    ResultObj->SetStringField(TEXT("safety_note"), TEXT("Dry-runs a reviewed Blueprint Niagara component-template asset swap by loading the Blueprint and Niagara Systems, resolving exactly one SCS UNiagaraComponent template, and reporting the planned replacement without calling SetAsset, compiling, saving, reloading, or dirtying project content."));
    AddDirtyPackageCommandDelta(ResultObj, DirtyPackagesBeforeCommand);
    return ResultObj;
}

// RG concrete canary replacement lease. Old generation is deliberately never
// enrolled in FWork::Objects: ForceDeleteObjects may unload/GC the whole package.
struct FCanaryReplacement
{
 FWork& W;
 bool Active=false,Retired=false,SourceCaptured=false;
 TStrongObjectPtr<UBlueprint> SourceBlueprint{nullptr};
 TStrongObjectPtr<UBlueprintGeneratedClass> SourceGenerated{nullptr},SourceSkeleton{nullptr};
 TStrongObjectPtr<UObject> SourceGeneratedCDO{nullptr},SourceSkeletonCDO{nullptr};
 TWeakObjectPtr<USimpleConstructionScript> SourceSCS,SourceAuthoredSCS,SourceSkeletonSCS;
 TArray<TUniquePtr<FIdentity>> SourceRoles;
 TStrongObjectPtr<UBlueprint> Old{nullptr};
 TStrongObjectPtr<UPackage> OldPackage{nullptr};
 TArray<TUniquePtr<FIdentity>> Owned;
 TWeakObjectPtr<UBlueprintGeneratedClass> Generated,Skeleton;
 TWeakObjectPtr<USimpleConstructionScript> SCS,AuthoredSCS,SkeletonSCS;
 TWeakObjectPtr<UObject> GeneratedCDO,SkeletonCDO;
 TArray<TWeakObjectPtr<USCS_Node>> Nodes;
 TArray<TWeakObjectPtr<UActorComponent>> Components;
 TArray<TWeakObjectPtr<UNiagaraSystem>> Systems;
 TArray<EObjectFlags> Flags;
 FString ObjectPath,PackageName,Filename;
 TArray<FString> SourceFiles;
 TArray<TArray<uint8>> SourceBytes;
 TArray<uint8> OldBytes;
 TSharedRef<FJsonObject> Log=MakeShared<FJsonObject>();
 explicit FCanaryReplacement(FWork& In):W(In){W.CanaryReplacementCurrent=[this]{return SourceCurrent()&&OldCurrent();};}
 ~FCanaryReplacement(){W.CanaryReplacementCurrent=nullptr;}
 bool Stop(const TCHAR* Code,const FString& Message){return W.Fail(Code,Message);}
 bool SourceCurrent()
 {
  if(!SourceCaptured)return true;
  for(const auto& I:SourceRoles)if(!I->Exact())return Stop(TEXT("blueprint_niagara_canary_source_role_changed"),TEXT("Captured source class/CDO/SCS role changed"));
  auto* B=SourceBlueprint.Get();
  if(!Live(B)||!B->GeneratedClass.GetGCPtr().IsResolved()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||!B->SimpleConstructionScript.IsResolved()
   ||B->GeneratedClass.GetGCPtr().Get()!=SourceGenerated.Get()||B->SkeletonGeneratedClass.GetGCPtr().Get()!=SourceSkeleton.Get()||B->SimpleConstructionScript!=SourceAuthoredSCS.Get())
   return Stop(TEXT("blueprint_niagara_canary_source_role_changed"),TEXT("Captured source Blueprint class/SCS slots changed"));
  for(auto* C:{SourceGenerated.Get(),SourceSkeleton.Get()})if(!Live(C)||!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy!=B||!C->ClassDefaultObject.IsResolved()||!C->SimpleConstructionScript.IsResolved())
   return Stop(TEXT("blueprint_niagara_canary_source_role_changed"),TEXT("Captured source class/CDO/SCS roles unavailable"));
  if(SourceGenerated->ClassDefaultObject.Get()!=SourceGeneratedCDO.Get()||SourceSkeleton->ClassDefaultObject.Get()!=SourceSkeletonCDO.Get()||SourceGenerated->SimpleConstructionScript!=SourceSCS.Get()||SourceSkeleton->SimpleConstructionScript!=SourceSkeletonSCS.Get())
   return Stop(TEXT("blueprint_niagara_canary_source_role_changed"),TEXT("Captured source nullable CDO/SCS slots changed"));
  return true;
 }
 bool CaptureSourceRoles(UBlueprint* B)
 {
  SourceBlueprint.Reset(B);
  if(!Live(B)||!B->GeneratedClass.GetGCPtr().IsResolved()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||!B->SimpleConstructionScript.IsResolved())return Stop(TEXT("blueprint_niagara_canary_source_role_scope"),TEXT("Loaded source class/SCS slots required"));
  auto* G=B->GeneratedClass.GetGCPtr().Get();auto* S=B->SkeletonGeneratedClass.GetGCPtr().Get();
  if(!Live(G)||!Live(S))return Stop(TEXT("blueprint_niagara_canary_source_role_scope"),TEXT("Actual loaded source generated/skeleton required"));
  SourceGenerated.Reset(Cast<UBlueprintGeneratedClass>(G));SourceSkeleton.Reset(Cast<UBlueprintGeneratedClass>(S));
  if(!SourceGenerated.IsValid()||!SourceSkeleton.IsValid())return Stop(TEXT("blueprint_niagara_canary_source_role_scope"),TEXT("Actual source BlueprintGeneratedClass roles required"));
  for(auto* C:{SourceGenerated.Get(),SourceSkeleton.Get()})
  {
   if(!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy!=B||C->GetPackage()!=B->GetPackage()||!C->ClassDefaultObject.IsResolved()||!C->SimpleConstructionScript.IsResolved())return Stop(TEXT("blueprint_niagara_canary_source_role_scope"),TEXT("Source class/CDO ownership required"));
   auto* D=C->ClassDefaultObject.Get();
   if(D&&(!Live(D)||D->GetClass()!=C||D->GetOuter()!=C->GetOuter()||D->GetPackage()!=B->GetPackage()||!D->HasAnyFlags(RF_ClassDefaultObject)))return Stop(TEXT("blueprint_niagara_canary_source_role_scope"),TEXT("Actual nullable source CDO role required"));
  }
  SourceGeneratedCDO.Reset(SourceGenerated->ClassDefaultObject.Get());SourceSkeletonCDO.Reset(SourceSkeleton->ClassDefaultObject.Get());SourceSCS=SourceGenerated->SimpleConstructionScript;SourceAuthoredSCS=B->SimpleConstructionScript;SourceSkeletonSCS=SourceSkeleton->SimpleConstructionScript;
  for(UObject* O:{static_cast<UObject*>(B),static_cast<UObject*>(SourceGenerated.Get()),static_cast<UObject*>(SourceSkeleton.Get()),SourceGeneratedCDO.Get(),SourceSkeletonCDO.Get(),static_cast<UObject*>(SourceSCS.Get()),static_cast<UObject*>(SourceAuthoredSCS.Get()),static_cast<UObject*>(SourceSkeletonSCS.Get())})
   if(O){if(!Live(O))return Stop(TEXT("blueprint_niagara_canary_source_role_scope"),TEXT("Loaded nullable source role required"));SourceRoles.Add(MakeUnique<FIdentity>(O));}
  SourceCaptured=true;return W.Admit(TEXT("replacement_source_roles_captured"));
 }
 // Compiler-cache association uses actual native class/owner/GeneratedBy and
 // compiler state, never a name prefix. Caches remain held/current until the
 // same deletion retirement boundary as their generating selected Blueprint.
 struct FCompilerCache
 {
  TWeakObjectPtr<UBlueprintGeneratedClass> Class;
  TWeakObjectPtr<UClass> Parent;
  TWeakObjectPtr<UObject> CDO;
  TWeakObjectPtr<USimpleConstructionScript> SCS;
  EClassFlags ClassFlags=CLASS_None;
  bool Internal=false;
  FString Path,BeforeRole;
  TArray<TWeakObjectPtr<USCS_Node>> Nodes;
  TArray<TWeakObjectPtr<UActorComponent>> Templates;
  TArray<FName> Variables;
 };
 TArray<TSharedPtr<FJsonValue>> CompilerCacheObservations;
 FString CompilerCachePath(UObject* O)const{return O?(Live(O)?O->GetPathName():TEXT("<unavailable>")):TEXT("<null>");}
 TSharedRef<FJsonObject> CompilerCacheRole(UBlueprintGeneratedClass* K)
 {
  auto J=MakeShared<FJsonObject>();
  if(!Live(K)){J->SetStringField(TEXT("object_state"),TEXT("unavailable"));return J;}
  TStrongObjectPtr<UBlueprintGeneratedClass> Hold(K);
  J->SetStringField(TEXT("path"),K->GetPathName());J->SetStringField(TEXT("class"),CompilerCachePath(K->GetClass()));
  J->SetStringField(TEXT("outer"),CompilerCachePath(K->GetOuter()));J->SetStringField(TEXT("package"),CompilerCachePath(K->GetPackage()));
  J->SetNumberField(TEXT("class_flags"),static_cast<uint32>(K->GetClassFlags()));J->SetBoolField(TEXT("internal"),K->IsInternalClass());J->SetBoolField(TEXT("rooted"),K->IsRooted());J->SetBoolField(TEXT("transient"),K->HasAnyFlags(RF_Transient));
  J->SetStringField(TEXT("parent"),CompilerCachePath(K->GetSuperClass()));
  J->SetBoolField(TEXT("generated_by_resolved"),K->ClassGeneratedBy.IsResolved());
  if(K->ClassGeneratedBy.IsResolved()){TStrongObjectPtr<UObject> By(K->ClassGeneratedBy.Get());J->SetStringField(TEXT("generated_by"),CompilerCachePath(By.Get()));}
  J->SetBoolField(TEXT("CDO_resolved"),K->ClassDefaultObject.IsResolved());
  if(K->ClassDefaultObject.IsResolved())
  {
   TStrongObjectPtr<UObject> D(K->ClassDefaultObject.Get());J->SetStringField(TEXT("CDO"),CompilerCachePath(D.Get()));
   if(Live(D.Get())){J->SetStringField(TEXT("CDO_class"),CompilerCachePath(D->GetClass()));J->SetStringField(TEXT("CDO_outer"),CompilerCachePath(D->GetOuter()));J->SetBoolField(TEXT("CDO_default_flag"),D->HasAnyFlags(RF_ClassDefaultObject));}
  }
  J->SetBoolField(TEXT("SCS_resolved"),K->SimpleConstructionScript.IsResolved());
  if(K->SimpleConstructionScript.IsResolved())
  {TStrongObjectPtr<USimpleConstructionScript> S(K->SimpleConstructionScript.Get());J->SetStringField(TEXT("SCS"),CompilerCachePath(S.Get()));if(Live(S.Get()))J->SetStringField(TEXT("SCS_outer"),CompilerCachePath(S->GetOuter()));}
  J->SetStringField(TEXT("observation_stage"),TEXT("copied prewriter/current role; not execution proof"));return J;
 }
 bool CompilerCacheMismatch(const FString& Path,const FString& Property,const FString& Expected,const FString& Actual)
 {
  if(!Log->HasField(TEXT("first_mismatch")))
  {auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("object"),Path);J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);Log->SetObjectField(TEXT("first_mismatch"),J);}
  return Stop(TEXT("blueprint_niagara_canary_compiler_cache_changed"),TEXT("Captured compiler cache role changed; copied first mismatch retained"));
 }
 TArray<FCompilerCache> CompilerCaches;
 TArray<TUniquePtr<FIdentity>> CompilerCacheOwned;
 bool CompilerCacheOwns(UObject* O)const
 {
  return CompilerCacheOwned.ContainsByPredicate([&](const auto& I){return I->Hold.Get()==O;});
 }
 bool CompilerCacheCurrent()
 {
  if(Retired)return true;
  for(const auto& I:CompilerCacheOwned)if(!I->Exact())return Stop(TEXT("blueprint_niagara_canary_compiler_cache_changed"),TEXT("Captured compiler cache identity changed before selected retirement"));
  for(auto& C:CompilerCaches)
  {
   auto* K=C.Class.Get();
   if(!Live(K)||K->GetClass()!=UBlueprintGeneratedClass::StaticClass()||K->GetOuter()!=GetTransientPackage()
    ||K->GetPackage()!=GetTransientPackage()||K->GetClassFlags()!=C.ClassFlags||K->IsInternalClass()!=C.Internal||K->IsRooted()||(!C.Internal&&!K->HasAnyFlags(RF_Transient))
    ||!K->ClassGeneratedBy.IsResolved()||K->ClassGeneratedBy.Get()!=Old.Get()||K->GetSuperClass()!=C.Parent.Get()
    ||!K->ClassDefaultObject.IsResolved()||K->ClassDefaultObject.Get()!=C.CDO.Get()
    ||!K->SimpleConstructionScript.IsResolved()||K->SimpleConstructionScript.Get()!=C.SCS.Get())
    return CompilerCacheMismatch(C.Path,TEXT("compiler_class_role"),C.BeforeRole,Json(CompilerCacheRole(K)));
   if(auto* D=C.CDO.Get())if(!Live(D)||D->GetClass()!=K||D->GetOuter()!=GetTransientPackage()||D->GetPackage()!=GetTransientPackage()||!D->HasAnyFlags(RF_ClassDefaultObject))
    return CompilerCacheMismatch(C.Path,TEXT("ClassDefaultObject.default_role"),C.BeforeRole,Json(CompilerCacheRole(K)));
   if(auto* S=C.SCS.Get())
   {
    if(!Live(S)||S->GetOuter()!=K)return Stop(TEXT("blueprint_niagara_canary_compiler_cache_changed"),TEXT("Captured compiler SCS owner changed"));
    const auto& Rows=S->GetAllNodes();
    if(Rows.Num()!=C.Nodes.Num())return Stop(TEXT("blueprint_niagara_canary_compiler_cache_changed"),TEXT("Captured compiler SCS membership changed"));
    for(int32 I=0;I<Rows.Num();++I)
    {
     auto* N=C.Nodes[I].Get();
     if(!Live(N)||Rows[I]!=N||N->GetOuter()!=S||!N->ComponentTemplate.IsResolved()
      ||N->ComponentTemplate.Get()!=C.Templates[I].Get()||N->GetVariableName()!=C.Variables[I])
      return Stop(TEXT("blueprint_niagara_canary_compiler_cache_changed"),TEXT("Captured compiler SCS node/template role changed"));
    }
   }
  }
  return true;
 }
 bool CaptureCompilerCache(UObject* Ref)
 {
  UBlueprintGeneratedClass* K=nullptr;
  if(Ref->GetClass()==UBlueprintGeneratedClass::StaticClass())K=static_cast<UBlueprintGeneratedClass*>(Ref);
  else if(Ref->GetClass()==USimpleConstructionScript::StaticClass())
  {
   auto* Outer=Ref->GetOuter();
   if(Live(Outer)&&Outer->GetClass()==UBlueprintGeneratedClass::StaticClass())K=static_cast<UBlueprintGeneratedClass*>(Outer);
  }
  else if(Ref->GetClass()==USCS_Node::StaticClass())
  {
   auto* S=Ref->GetOuter();
   if(Live(S)&&S->GetClass()==USimpleConstructionScript::StaticClass())
   {auto* Outer=S->GetOuter();if(Live(Outer)&&Outer->GetClass()==UBlueprintGeneratedClass::StaticClass())K=static_cast<UBlueprintGeneratedClass*>(Outer);}
  }
  if(!Live(K))return false;
  auto Observation=CompilerCacheRole(K);Observation->SetBoolField(TEXT("admitted"),false);
  CompilerCacheObservations.Add(MakeShared<FJsonValueObject>(Observation));Log->SetArrayField(TEXT("compiler_cache_observations"),CompilerCacheObservations);
  auto Refuse=[&](const TCHAR* Property,const FString& Expected,const FString& Actual)
  {auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("object"),K->GetPathName());J->SetStringField(TEXT("property"),Property);J->SetStringField(TEXT("expected"),Expected);J->SetStringField(TEXT("actual"),Actual);Observation->SetObjectField(TEXT("qualification_mismatch"),J);return false;};
  if(K==Generated.Get()||K==Skeleton.Get())return Refuse(TEXT("class_role"),TEXT("distinct obsolete compiler class"),TEXT("current generated/skeleton"));
  if(K->GetOuter()!=GetTransientPackage()||K->GetPackage()!=GetTransientPackage())return Refuse(TEXT("outer/package"),TEXT("/Engine/Transient"),Json(Observation));
  if(!K->ClassGeneratedBy.IsResolved()||K->ClassGeneratedBy.Get()!=Old.Get())return Refuse(TEXT("ClassGeneratedBy"),ObjectPath,Json(Observation));
  if(K->IsRooted())return Refuse(TEXT("rooted"),TEXT("false (SDK removes compiler class root)"),TEXT("true"));
  if(!K->HasAllClassFlags(CLASS_CompiledFromBlueprint|CLASS_NewerVersionExists)||K->HasAnyClassFlags(CLASS_Native))return Refuse(TEXT("ClassFlags"),TEXT("CompiledFromBlueprint|NewerVersionExists and not Native"),FString::Printf(TEXT("%u"),static_cast<uint32>(K->GetClassFlags())));
  if(!K->ClassDefaultObject.IsResolved()||!K->SimpleConstructionScript.IsResolved())return Refuse(TEXT("CDO/SCS.resolved"),TEXT("true/true"),Json(Observation));
  const bool Reinst=K->IsInternalClass()&&K->GetSuperClass()==AActor::StaticClass();
  const bool Trash=!K->IsInternalClass()&&K->GetSuperClass()==nullptr&&K->ClassDefaultObject.Get()==nullptr&&K->SimpleConstructionScript.Get()==nullptr&&K->HasAnyFlags(RF_Transient);
  if(!Reinst&&!Trash)return Refuse(TEXT("compiler_role"),TEXT("internal+direct Actor REINST; or transient noninternal null-parent/null-CDO/null-SCS trash container"),Json(Observation));
  for(const auto& C:CompilerCaches)if(C.Class.Get()==K){const bool Known=CompilerCacheOwns(Ref);Observation->SetBoolField(TEXT("admitted"),Known);return Known;}
  if(CompilerCaches.Num()>=16)return Stop(TEXT("blueprint_niagara_canary_compiler_cache_budget"),TEXT("Compiler cache class bound exceeded"));
  TStrongObjectPtr<UBlueprintGeneratedClass> ClassHold(K);
  auto* D=K->ClassDefaultObject.Get();auto* S=K->SimpleConstructionScript.Get();
  TStrongObjectPtr<UObject> DefaultHold(D);TStrongObjectPtr<USimpleConstructionScript> SCSHold(S);
  if(D&&(!Live(D)||D->GetClass()!=K||D->GetOuter()!=GetTransientPackage()||D->GetPackage()!=GetTransientPackage()||!D->HasAnyFlags(RF_ClassDefaultObject)))return Refuse(TEXT("ClassDefaultObject.role"),TEXT("nullable actual cache-class CDO in transient package, default flag set"),Json(Observation));
  if(S&&(!Live(S)||S->GetClass()!=USimpleConstructionScript::StaticClass()||S->GetOuter()!=K||S->GetPackage()!=GetTransientPackage()))return Refuse(TEXT("SimpleConstructionScript.role"),TEXT("nullable exact cache-owned native SCS in transient package"),Json(Observation));
  TArray<UObject*> Exports;Exports.Add(K);GetObjectsWithOuter(K,Exports,EGetObjectsFlags::IncludeNestedObjects);
  if(D){Exports.AddUnique(D);GetObjectsWithOuter(D,Exports,EGetObjectsFlags::IncludeNestedObjects);}
  if(Exports.Num()>4096-CompilerCacheOwned.Num())return Stop(TEXT("blueprint_niagara_canary_compiler_cache_budget"),TEXT("Compiler cache export bound exceeded"));
  TArray<TStrongObjectPtr<UObject>> ExportHolds;for(UObject* O:Exports)ExportHolds.Emplace(O);
  static const TSet<FString> NativeCacheClasses={TEXT("/Script/CoreUObject.Function"),TEXT("/Script/CoreUObject.DelegateFunction"),TEXT("/Script/Engine.SimpleConstructionScript"),TEXT("/Script/Engine.SCS_Node"),TEXT("/Script/Engine.SceneComponent"),TEXT("/Script/Niagara.NiagaraComponent")};
  for(const auto& H:ExportHolds)
  {
   auto* O=H.Get();
   if(!Live(O)||!Live(O->GetClass())||O->GetPackage()!=GetTransientPackage())return false;
   if(O!=K&&O!=D&&(!O->IsIn(K)&&!(D&&O->IsIn(D))))return false;
   if(O!=K&&O!=D&&(!O->GetClass()->HasAnyClassFlags(CLASS_Native)||!NativeCacheClasses.Contains(O->GetClass()->GetPathName())))return false;
  }
  FCompilerCache C;C.Path=K->GetPathName();C.BeforeRole=Json(CompilerCacheRole(K));C.Class=K;C.Parent=K->GetSuperClass();C.CDO=D;C.SCS=S;C.ClassFlags=K->GetClassFlags();C.Internal=K->IsInternalClass();
  if(S)for(auto* N:S->GetAllNodes())
  {
   if(!Live(N)||N->GetClass()!=USCS_Node::StaticClass()||N->GetOuter()!=S||!N->ComponentTemplate.IsResolved())return false;
   auto* T=N->ComponentTemplate.Get();
   if(T&&(!Live(T)||!Owned.ContainsByPredicate([&](const auto& I){return I->Hold.Get()==T;})&&!Exports.Contains(T)))return false;
   C.Nodes.Add(N);C.Templates.Add(T);C.Variables.Add(N->GetVariableName());
  }
  if(Ref!=K&&!Exports.Contains(Ref))return false;
  for(const auto& H:ExportHolds)if(!CompilerCacheOwns(H.Get()))CompilerCacheOwned.Add(MakeUnique<FIdentity>(H.Get()));
  CompilerCaches.Add(MoveTemp(C));Observation->SetBoolField(TEXT("admitted"),true);
  return CompilerCacheCurrent();
 }

 bool OldCurrent()
 {
  if(!Active||Retired)return true; // No retired object/package getter, including diagnostics.
  if(!Live(Old.Get())||!Live(OldPackage.Get())||Old->GetPackage()!=OldPackage.Get()||OldPackage->IsDirty())
   return Stop(TEXT("blueprint_niagara_canary_old_target_changed"),TEXT("Unretired clean target/package role changed"));
  for(int32 I=0;I<Owned.Num();++I)if(!Owned[I]->Exact()||Owned[I]->Object->GetFlags()!=Flags[I])
   return Stop(TEXT("blueprint_niagara_canary_old_export_changed"),TEXT("Unretired selected export identity/flags changed"));
  if(!Old->GeneratedClass.GetGCPtr().IsResolved()||!Old->SkeletonGeneratedClass.GetGCPtr().IsResolved()||!Old->SimpleConstructionScript.IsResolved()
   ||!Old->ParentClass.GetGCPtr().IsResolved()||Old->ParentClass.Get()!=AActor::StaticClass()
   ||Old->GeneratedClass!=Generated.Get()||Old->SkeletonGeneratedClass!=Skeleton.Get()||Old->SimpleConstructionScript!=AuthoredSCS.Get())
   return Stop(TEXT("blueprint_niagara_canary_old_class_changed"),TEXT("Unretired target authored/generated/skeleton role changed"));
  for(auto* C:{Generated.Get(),Skeleton.Get()})if(!Live(C)||!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy!=Old.Get()||C->GetSuperClass()!=AActor::StaticClass()
   ||!C->ClassDefaultObject.IsResolved()||!C->SimpleConstructionScript.IsResolved())
   return Stop(TEXT("blueprint_niagara_canary_old_class_changed"),TEXT("Unretired class/CDO/SCS role unavailable"));
  if(Generated->ClassDefaultObject.Get()!=GeneratedCDO.Get()||Skeleton->ClassDefaultObject.Get()!=SkeletonCDO.Get()
   ||Generated->SimpleConstructionScript!=SCS.Get()||Skeleton->SimpleConstructionScript!=SkeletonSCS.Get()||!Live(SCS.Get()))
   return Stop(TEXT("blueprint_niagara_canary_old_class_changed"),TEXT("Unretired nullable CDO/SCS slot changed"));
  const auto& CurrentNodes=SCS->GetAllNodes();
  if(CurrentNodes.Num()!=Nodes.Num())return Stop(TEXT("blueprint_niagara_canary_old_scs_changed"),TEXT("Unretired SCS membership changed"));
  for(int32 I=0;I<Nodes.Num();++I)
  {
   auto* N=Nodes[I].Get();auto* C=Components[I].Get();
   if(!Live(N)||!Live(C)||CurrentNodes[I]!=N||!N->ComponentTemplate.IsResolved()||N->ComponentTemplate!=C)
    return Stop(TEXT("blueprint_niagara_canary_old_scs_changed"),TEXT("Unretired SCS/template role changed"));
   if(auto* Niagara=Cast<UNiagaraComponent>(C))if(Niagara->GetAsset()!=Systems[I].Get())
    return Stop(TEXT("blueprint_niagara_canary_old_scs_changed"),TEXT("Unretired Niagara assignment changed"));
  }
  return CompilerCacheCurrent();
 }
 bool SourceFence(const TCHAR* Stage)
 {
  if(!W.Admit(Stage))return false;
  for(int32 I=0;I<SourceFiles.Num();++I)
  {
   TArray<uint8> Bytes;
   if(!W.Read(TEXT("Replacement_source_file"),[&]{if(!FFileHelper::LoadFileToArray(Bytes,*SourceFiles[I]))Stop(TEXT("blueprint_niagara_canary_source_file_unavailable"),TEXT("Captured source file became unavailable"));}))return false;
   if(Bytes!=SourceBytes[I])return Stop(TEXT("blueprint_niagara_canary_source_file_changed"),SourceFiles[I]);
  }
  Log->SetBoolField(TEXT("source_saved_bytes_match_latest"),true);return true;
 }
 bool CaptureSourceFiles(UBlueprint* Source)
 {
  for(UObject* O:{static_cast<UObject*>(Source),static_cast<UObject*>(W.SourceSystem.Get()),static_cast<UObject*>(W.Replacement.Get())})
  {
   if(!Live(O)||!Live(O->GetPackage()))return Stop(TEXT("blueprint_niagara_canary_source_unavailable"),TEXT("Expected actual source roles"));
   FString File;if(!FPackageName::TryConvertLongPackageNameToFilename(O->GetPackage()->GetName(),File,FPackageName::GetAssetPackageExtension()))
    return Stop(TEXT("blueprint_niagara_canary_source_file_unavailable"),TEXT("Expected saved source asset filename"));
   if(SourceFiles.Contains(File))continue;
   const int64 Size=IFileManager::Get().FileSize(*File);
   if(Size<=0||Size>64ll*1024*1024)return Stop(TEXT("blueprint_niagara_canary_source_file_scope"),TEXT("First replacement scope requires bounded saved source files"));
   TArray<uint8> Bytes;
   if(!W.Read(TEXT("Replacement_source_file_before"),[&]{if(!FFileHelper::LoadFileToArray(Bytes,*File))Stop(TEXT("blueprint_niagara_canary_source_file_unavailable"),File);}))return false;
   SourceFiles.Add(File);SourceBytes.Add(MoveTemp(Bytes));
  }
  Log->SetNumberField(TEXT("source_saved_file_count"),SourceFiles.Num());return true;
 }
 bool CaptureOld(const FString& Path)
 {
  ObjectPath=Path;PackageName=FPackageName::ObjectPathToPackageName(Path);
  W.RequestedObject=Path;W.RequestedPackage=PackageName;
  if(!FPackageName::TryConvertLongPackageNameToFilename(PackageName,Filename,FPackageName::GetAssetPackageExtension()))return false;
  W.RequestedFilename=Filename;W.R.Data->SetObjectField(TEXT("existing_target_replacement"),Log);
  Log->SetStringField(TEXT("target_object_path"),Path);Log->SetBoolField(TEXT("old_generation_retired"),false);
  Log->SetBoolField(TEXT("full_global_effects_verified"),false);Log->SetBoolField(TEXT("automatic_retry"),false);
  if(GEditor->IsTransactionActive())return Stop(TEXT("blueprint_niagara_canary_replace_transaction"),TEXT("Active external transaction is outside isolated replacement"));
  auto* Editors=GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
  if(!Live(Editors)||!W.Pin(Editors))return false;
  if(!W.SDK(TEXT("Replacement_load_old_target")))return false;
  Old.Reset(LoadObject<UBlueprint>(nullptr,*Path)); // Hold return before callback-current inspection.
  if(!W.After(TEXT("Replacement_load_old_target")))return false;
  auto* B=Old.Get();
  if(!Live(B)||B->GetClass()!=UBlueprint::StaticClass()||!Live(B->GetPackage())||B->GetOuter()!=B->GetPackage()
   ||!Same(B->GetPathName(),Path)||!Same(B->GetPackage()->GetName(),PackageName)||B->GetPackage()->IsDirty()
   ||B->BlueprintType!=BPTYPE_Normal||!B->ParentClass.GetGCPtr().IsResolved()||B->ParentClass.Get()!=AActor::StaticClass()
   ||!B->GeneratedClass.GetGCPtr().IsResolved()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||!B->SimpleConstructionScript.IsResolved()
   ||B->bBeingCompiled||B->bQueuedForCompilation||!FBlueprintCompilationManager::IsGeneratedClassLayoutReady())
   return Stop(TEXT("blueprint_niagara_canary_replace_target_scope"),TEXT("First replacement supports clean saved exact normal direct-Actor Blueprint targets only"));
  OldPackage.Reset(B->GetPackage());
  if(!OldPackage->IsFullyLoaded()||OldPackage->HasAnyPackageFlags(PKG_CompiledIn|PKG_PlayInEditor|PKG_NewlyCreated)
   ||IFileManager::Get().FileSize(*Filename)<=0||IFileManager::Get().FileSize(*Filename)>64ll*1024*1024||IFileManager::Get().IsReadOnly(*Filename))
   return Stop(TEXT("blueprint_niagara_canary_replace_target_scope"),TEXT("Writable fully loaded persisted target required"));
  if(!Live(B->GeneratedClass.GetGCPtr().Get())||!Live(B->SkeletonGeneratedClass.GetGCPtr().Get()))return Stop(TEXT("blueprint_niagara_canary_replace_class_scope"),TEXT("Actual loaded generated/skeleton classes required"));
  Generated=Cast<UBlueprintGeneratedClass>(B->GeneratedClass.GetGCPtr().Get());Skeleton=Cast<UBlueprintGeneratedClass>(B->SkeletonGeneratedClass.GetGCPtr().Get());AuthoredSCS=B->SimpleConstructionScript;
  if(!Live(Generated.Get())||!Live(Skeleton.Get()))return Stop(TEXT("blueprint_niagara_canary_replace_class_scope"),TEXT("Actual generated/skeleton classes required"));
  for(auto* C:{Generated.Get(),Skeleton.Get()})
  {
   if(!C->ClassGeneratedBy.IsResolved()||C->ClassGeneratedBy!=B||C->GetSuperClass()!=AActor::StaticClass()||C->GetPackage()!=OldPackage.Get()||C->GetOuter()!=OldPackage.Get()||!C->ClassDefaultObject.IsResolved()||!C->SimpleConstructionScript.IsResolved())
    return Stop(TEXT("blueprint_niagara_canary_replace_class_scope"),TEXT("Actual class/CDO slot roles required; no CDO warming"));
   auto* D=C->ClassDefaultObject.Get();auto* T=C->SimpleConstructionScript.Get();
   if(D&&(!Live(D)||D->GetClass()!=C||D->GetOuter()!=C->GetOuter()||D->GetPackage()!=OldPackage.Get()||!D->HasAnyFlags(RF_ClassDefaultObject)))return Stop(TEXT("blueprint_niagara_canary_replace_cdo_scope"),TEXT("Actual nullable owned CDO required"));
   if(T&&(!Live(T)||T->GetPackage()!=OldPackage.Get()))return Stop(TEXT("blueprint_niagara_canary_replace_scs_scope"),TEXT("Actual nullable owned SCS required"));
  }
  GeneratedCDO=Generated->ClassDefaultObject.Get();SkeletonCDO=Skeleton->ClassDefaultObject.Get();SCS=Generated->SimpleConstructionScript;SkeletonSCS=Skeleton->SimpleConstructionScript;
  if(!Live(SCS.Get())||!Live(AuthoredSCS.Get())||AuthoredSCS->GetPackage()!=OldPackage.Get())return Stop(TEXT("blueprint_niagara_canary_replace_scs_scope"),TEXT("Actual authored/generated SCS required"));
  TArray<UObject*> Exports;GetObjectsWithPackage(OldPackage.Get(),Exports,true);
  if(Exports.IsEmpty()||Exports.Num()>4096)return Stop(TEXT("blueprint_niagara_canary_replace_export_scope"),TEXT("Bounded selected package export inventory required"));
  static const TSet<FString> NativeClasses={TEXT("/Script/Engine.Blueprint"),TEXT("/Script/Engine.BlueprintGeneratedClass"),TEXT("/Script/Engine.SimpleConstructionScript"),TEXT("/Script/Engine.SCS_Node"),TEXT("/Script/Engine.SceneComponent"),TEXT("/Script/Niagara.NiagaraComponent"),TEXT("/Script/Engine.EdGraph"),TEXT("/Script/CoreUObject.Function"),TEXT("/Script/CoreUObject.DelegateFunction"),TEXT("/Script/BlueprintGraph.K2Node_Event"),TEXT("/Script/BlueprintGraph.K2Node_FunctionEntry"),TEXT("/Script/BlueprintGraph.K2Node_FunctionResult"),TEXT("/Script/BlueprintGraph.K2Node_CallFunction"),TEXT("/Script/BlueprintGraph.K2Node_VariableGet"),TEXT("/Script/BlueprintGraph.K2Node_VariableSet"),TEXT("/Script/BlueprintGraph.K2Node_Self"),TEXT("/Script/UnrealEd.EdGraphNode_Comment")};
  for(UObject* O:Exports)
  {
   if(!Live(O)||!Live(O->GetClass())||!Live(O->GetPackage())||O->GetPackage()!=OldPackage.Get())return Stop(TEXT("blueprint_niagara_canary_replace_export_scope"),TEXT("Selected export unavailable or outside selected package"));
   auto* C=O->GetClass();
   const bool Native=NativeClasses.Contains(C->GetPathName())&&C->HasAnyClassFlags(CLASS_Native);
   const bool OwnCDO=(O==GeneratedCDO.Get()&&C==Generated.Get())||(O==SkeletonCDO.Get()&&C==Skeleton.Get());
   if((!Native&&!OwnCDO)||!(O==B||O==Generated.Get()||O==Skeleton.Get()||O->IsIn(B)||O->IsIn(Generated.Get())||O->IsIn(Skeleton.Get())||OwnCDO))
    return Stop(TEXT("blueprint_niagara_canary_replace_export_scope"),TEXT("Custom/foreign exports are outside first isolated replacement scope"));
   if(OwnCDO&&(!O->HasAnyFlags(RF_ClassDefaultObject)||O->GetOuter()!=OldPackage.Get()))return Stop(TEXT("blueprint_niagara_canary_replace_cdo_scope"),TEXT("Actual nullable CDO role required"));
   Owned.Add(MakeUnique<FIdentity>(O));Flags.Add(O->GetFlags());
  }
  for(auto* N:SCS->GetAllNodes())
  {
   if(!Live(N)||!N->ComponentTemplate.IsResolved()||!Live(N->ComponentTemplate.Get()))return Stop(TEXT("blueprint_niagara_canary_replace_scs_scope"),TEXT("Actual SCS template required"));
   auto* C=N->ComponentTemplate.Get();if(C->GetClass()!=USceneComponent::StaticClass()&&C->GetClass()!=UNiagaraComponent::StaticClass())return Stop(TEXT("blueprint_niagara_canary_replace_scs_scope"),TEXT("First old target templates are exact Scene/Niagara components"));
   Nodes.Add(N);Components.Add(C);auto* NC=Cast<UNiagaraComponent>(C);Systems.Add(NC?NC->GetAsset():nullptr);
  }
  Active=true;
  if(!W.Admit(TEXT("replacement_old_inventory_captured")))return false;
  bool Open=false;if(!W.Read(TEXT("Replacement_editor_query"),[&]{Open=Editors->FindEditorForAsset(B,false)!=nullptr;}))return false;
  if(Open)return Stop(TEXT("blueprint_niagara_canary_replace_editor_open"),TEXT("Close selected target editor; no forced editor close"));
  if(!W.Read(TEXT("Replacement_old_file"),[&]{if(!FFileHelper::LoadFileToArray(OldBytes,*Filename))Stop(TEXT("blueprint_niagara_canary_replace_file_unavailable"),Filename);}))return false;
  Log->SetNumberField(TEXT("old_export_count"),Owned.Num());Log->SetNumberField(TEXT("old_file_bytes"),OldBytes.Num());
  W.R.Data->SetBoolField(TEXT("existing_target_force_replace_supported"),true);return true;
 }
 bool References()
 {
  if(!W.SDK(TEXT("Replacement_saved_referencers")))return false;
  auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
  TArray<FName> References;const bool Complete=!Registry.IsLoadingAssets()&&Registry.GetReferencers(FName(*PackageName),References,UE::AssetRegistry::EDependencyCategory::Package);
  if(!W.After(TEXT("Replacement_saved_referencers")))return false;
  TArray<TSharedPtr<FJsonValue>> Foreign;
  if(!Complete||References.Num()>4096)return Stop(TEXT("blueprint_niagara_canary_replace_references_incomplete"),TEXT("Complete bounded saved references required"));
  for(FName N:References)if(!Same(N.ToString(),PackageName))Foreign.Add(MakeShared<FJsonValueString>(N.ToString()));
  TArray<UObject*> Roots;TSet<UObject*> Ignore;Ignore.Add(OldPackage.Get());
  for(const auto& I:Owned){Roots.Add(I->Hold.Get());Ignore.Add(I->Hold.Get());}
  if(!W.SDK(TEXT("Replacement_loaded_referencers")))return false;
  const auto RefObjects=FReferencerFinder::GetAllReferencers(Roots,&Ignore,EReferencerFinderFlags::SkipWeakReferences);
  TArray<TStrongObjectPtr<UObject>> RefHolds;for(UObject* O:RefObjects)RefHolds.Emplace(O);
  if(!W.After(TEXT("Replacement_loaded_referencers")))return false;
  if(RefObjects.Num()>4096)return Stop(TEXT("blueprint_niagara_canary_replace_references_incomplete"),TEXT("Loaded reference bound exceeded"));
  TArray<TSharedPtr<FJsonValue>> Observed,Associated;
  for(const auto& H:RefHolds)
  {
   auto* O=H.Get();if(!Live(O))return Stop(TEXT("blueprint_niagara_canary_replace_references_incomplete"),TEXT("Loaded reference unavailable"));
   const FString Path=O->GetPathName();Observed.Add(MakeShared<FJsonValueString>(Path));
   const bool Cache=CaptureCompilerCache(O);
   if(!W.R.Error.IsEmpty())return false;
   if(Cache)Associated.Add(MakeShared<FJsonValueString>(Path));else Foreign.Add(MakeShared<FJsonValueString>(Path));
  }
  Log->SetArrayField(TEXT("observed_loaded_referencers"),Observed);Log->SetArrayField(TEXT("compiler_cache_referencers"),Associated);
  Log->SetNumberField(TEXT("compiler_cache_class_count"),CompilerCaches.Num());Log->SetNumberField(TEXT("compiler_cache_export_count"),CompilerCacheOwned.Num());
  Log->SetArrayField(TEXT("external_referencers"),Foreign);
  if(!Foreign.IsEmpty())return Stop(TEXT("blueprint_niagara_canary_replace_external_references"),TEXT("Foreign saved/loaded references are outside isolated replacement"));
  if(!W.Admit(TEXT("replacement_cache_roles_captured")))return false;
  // The first scan includes Old Blueprint itself, so every admitted cache class
  // directly identifies its generating Blueprint. Enroll its bounded exact
  // exports/CDO subtree, then close all incoming hard references to that cohort.
  // A real Actor/user UObject referring only to a cache is still foreign.
  if(!CompilerCaches.IsEmpty())
  {
   for(const auto& I:CompilerCacheOwned){Roots.AddUnique(I->Hold.Get());Ignore.Add(I->Hold.Get());}
   if(!W.SDK(TEXT("Replacement_compiler_cache_incoming")))return false;
   const auto Incoming=FReferencerFinder::GetAllReferencers(Roots,&Ignore,EReferencerFinderFlags::SkipWeakReferences);
   TArray<TStrongObjectPtr<UObject>> IncomingHolds;for(UObject* O:Incoming)IncomingHolds.Emplace(O);
   if(!W.After(TEXT("Replacement_compiler_cache_incoming")))return false;
   if(Incoming.Num()>4096)return Stop(TEXT("blueprint_niagara_canary_replace_references_incomplete"),TEXT("Compiler-cache incoming reference bound exceeded"));
   for(const auto& H:IncomingHolds)
   {if(!Live(H.Get()))return Stop(TEXT("blueprint_niagara_canary_replace_references_incomplete"),TEXT("Compiler-cache incoming reference unavailable"));Foreign.Add(MakeShared<FJsonValueString>(H->GetPathName()));}
   Log->SetArrayField(TEXT("external_referencers"),Foreign);Log->SetBoolField(TEXT("compiler_cache_incoming_closed"),Foreign.IsEmpty());
   if(!Foreign.IsEmpty())return Stop(TEXT("blueprint_niagara_canary_replace_external_references"),TEXT("Foreign incoming references to selected compiler caches prevent isolated replacement"));
  }
  return W.Admit(TEXT("replacement_references_complete"));
 }
 UBlueprint* Replace(UBlueprint* Source,const FString& Path,bool& Deleted)
 {
  W.Source=Source;W.SourceDirty=Source->GetPackage()->IsDirty();W.SourceProtected=true;
  if(!W.Blueprint(Source)||!W.Idle(Source)||!CaptureSourceRoles(Source)||!CaptureSourceFiles(Source)||!CaptureOld(Path)||!References()||!SourceFence(TEXT("replacement_before_delete")))return nullptr;
  TArray<uint8> CurrentOld;
  if(!W.Read(TEXT("Replacement_old_file_predelete"),[&]{if(!FFileHelper::LoadFileToArray(CurrentOld,*Filename))Stop(TEXT("blueprint_niagara_canary_replace_file_unavailable"),Filename);})||CurrentOld!=OldBytes)
  {if(W.R.Error.IsEmpty())Stop(TEXT("blueprint_niagara_canary_replace_file_changed"),TEXT("Selected file changed before delete"));return nullptr;}
  auto* Assets=GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();if(!Live(Assets)||!W.Pin(Assets)||!W.SDK(TEXT("DeleteExistingCanary"),true))return nullptr;
  W.R.Data->SetBoolField(TEXT("existing_target_replace_attempted"),true);
  Retired=true;Log->SetBoolField(TEXT("old_generation_retired"),true);
  for(auto& I:CompilerCacheOwned)I->Hold.Reset();
  Log->SetBoolField(TEXT("compiler_cache_generation_retired"),true);
  for(auto& I:Owned)I->Hold.Reset();Old.Reset();OldPackage.Reset();
  // No old-generation getter, SDK, or external observer between release and delete.
  Deleted=Assets->DeleteAsset(PackageName);
  Log->SetBoolField(TEXT("delete_returned"),true);Log->SetBoolField(TEXT("delete_success"),Deleted);
  W.R.Data->SetBoolField(TEXT("target_asset_deleted_before_duplicate"),Deleted);
  if(!W.After(TEXT("DeleteExistingCanary")))return nullptr;
  if(!Deleted){Stop(TEXT("blueprint_niagara_canary_replace_delete_failed"),TEXT("DeleteAsset returned false; actual prefix retained, no duplicate/retry"));return nullptr;}
  bool Registered=true;
  if(!W.Read(TEXT("Replacement_absence_registry"),[&]{TArray<FAssetData> Rows;FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssetsByPackageName(FName(*PackageName),Rows,false);Registered=!Rows.IsEmpty();}))return nullptr;
  const bool Memory=FindObject<UObject>(nullptr,*ObjectPath)!=nullptr,Disk=FPackageName::DoesPackageExist(PackageName);
  Log->SetBoolField(TEXT("absence_observed"),!Registered&&!Memory&&!Disk);
  if(Registered||Memory||Disk){Stop(TEXT("blueprint_niagara_canary_replace_absence_incomplete"),TEXT("Delete true but selected absence incomplete; no duplicate/retry"));return nullptr;}
  // An empty package can survive ForceDelete. It is a newly observed role, not
  // an assertion that the retired package pointer/flags survived unchanged.
  TStrongObjectPtr<UPackage> Remaining(FindPackage(nullptr,*PackageName));
  if(Remaining.IsValid())
  {
   if(!Live(Remaining.Get())){Stop(TEXT("blueprint_niagara_canary_replace_package_unavailable"),TEXT("Remaining package unavailable"));return nullptr;}
   TArray<UObject*> Exports;GetObjectsWithPackage(Remaining.Get(),Exports,true);
   for(auto* O:Exports)if(Live(O)){Stop(TEXT("blueprint_niagara_canary_replace_package_not_empty"),TEXT("Remaining package exports prevent safe duplication"));return nullptr;}
  }
  if(!SourceFence(TEXT("replacement_after_delete")))return nullptr;
  if(!W.SDK(TEXT("Replacement_MakeDirectory"),true))return nullptr;
  const bool Directory=Assets->MakeDirectory(FPackageName::GetLongPackagePath(PackageName));Log->SetBoolField(TEXT("make_directory_return"),Directory);
  if(!W.After(TEXT("Replacement_MakeDirectory"))||!SourceFence(TEXT("replacement_pre_duplicate")))return nullptr;
  if(FindObject<UObject>(nullptr,*ObjectPath)||FPackageName::DoesPackageExist(PackageName)){Stop(TEXT("blueprint_niagara_canary_replace_collision"),TEXT("Target appeared before duplicate; no retry"));return nullptr;}
  if(!W.SDK(TEXT("Replacement_DuplicateAsset"),true))return nullptr;
  TStrongObjectPtr<UObject> Created(Assets->DuplicateAsset(Source->GetPackage()->GetName(),PackageName));
  auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("requested_object_path"),ObjectPath);Row->SetBoolField(TEXT("nonnull_return"),Created.Get()!=nullptr);Row->SetBoolField(TEXT("identity_verified"),false);W.CreationRows.Add(MakeShared<FJsonValueObject>(Row));
  Log->SetBoolField(TEXT("duplicate_returned"),true);Log->SetBoolField(TEXT("duplicate_returned_nonnull"),Created.Get()!=nullptr);
  if(!W.After(TEXT("Replacement_DuplicateAsset")))return nullptr;
  // Original UE56 fallback is one read after a returned duplicate, never a
  // second mutation attempt. Keep a null duplicate receipt even if it resolves.
  if(Created.Get()&&!Live(Created.Get())){Stop(TEXT("blueprint_niagara_canary_duplicate_unavailable"),TEXT("Returned object unavailable; no fallback observation after unknown"));return nullptr;}
  if(!Cast<UBlueprint>(Created.Get()))
  {
   if(!W.SDK(TEXT("Replacement_LoadObjectFallback")))return nullptr;
   Created.Reset(LoadObject<UBlueprint>(nullptr,*ObjectPath));Log->SetBoolField(TEXT("load_fallback_returned"),true);Log->SetBoolField(TEXT("load_fallback_nonnull"),Created.Get()!=nullptr);
   if(!W.After(TEXT("Replacement_LoadObjectFallback")))return nullptr;
  }
  auto* B=Live(Created.Get())?Cast<UBlueprint>(Created.Get()):nullptr;
  if(!Live(B)||B->GetClass()!=Source->GetClass()||!Live(B->GetPackage())||B->GetOuter()!=B->GetPackage()||!Same(B->GetPathName(),ObjectPath)||!Same(B->GetPackage()->GetName(),PackageName))
  {Stop(TEXT("blueprint_niagara_canary_duplicate_identity"),TEXT("Duplicate/fallback returned null or unexpected identity; old deletion retained, no automatic restore"));return nullptr;}
  if(!W.Pin(B))return nullptr;W.Target=B;W.R.bChanged=true;
  if(!W.Blueprint(B)||!PackageOwnership(B)||!SourceFence(TEXT("replacement_after_duplicate")))return nullptr;
  Row->SetBoolField(TEXT("identity_verified"),true);Log->SetBoolField(TEXT("new_generation_identity_verified"),true);return B;
 }
};

TSharedPtr<FJsonObject> HandleCanary(const TSharedPtr<FJsonObject>& Params)
{
    auto& W=*Current;
    TSharedPtr<FJsonObject> EffectiveParams = Params.IsValid() ? Params : MakeShared<FJsonObject>();

    const FString Operation = GetStringParam(EffectiveParams, TEXT("operation"), TEXT("temp_bp_component_template_asset_swap_canary"));
    if (Operation != TEXT("temp_bp_component_template_asset_swap_canary"))
    {
        return ErrorResponse(TEXT("operation must be temp_bp_component_template_asset_swap_canary"));
    }

    const bool bDryRun = GetBoolParam(EffectiveParams, TEXT("dry_run"), true);
    const FString ConfirmationToken = GetStringParam(EffectiveParams, TEXT("confirmation_token"), FString());
    if (ConfirmationToken != TEXT("ALLOW_TEMP_BP_COMPONENT_TEMPLATE_MUTATION"))
    {
        return ErrorResponse(TEXT("confirmation_token must be ALLOW_TEMP_BP_COMPONENT_TEMPLATE_MUTATION for temp-only BP component-template canary mutation."));
    }

    const bool bPreserveOverrideParameters = GetBoolParam(EffectiveParams, TEXT("preserve_override_parameters"), true);
    if (!bPreserveOverrideParameters)
    {
        return ErrorResponse(TEXT("preserve_override_parameters must remain true; dropping component template override state is not admitted."));
    }

    if (GetBoolParam(EffectiveParams, TEXT("allow_unrealmcp_dependency"), false))
    {
        return ErrorResponse(TEXT("allow_unrealmcp_dependency must remain false; finished assets must not depend on UnrealMCP."));
    }

    const bool bIncludeInheritedScsTemplates = GetBoolParam(EffectiveParams, TEXT("include_inherited_scs_templates"), false);
    if (bIncludeInheritedScsTemplates)
    {
        return ErrorResponse(TEXT("execute_blueprint_niagara_component_template_asset_swap_canary only admits include_inherited_scs_templates=false; inherited SCS template mutation could modify another Blueprint."));
    }

    FString SourceBlueprintPathParam;
    if (!EffectiveParams->TryGetStringField(TEXT("source_blueprint_path"), SourceBlueprintPathParam) || SourceBlueprintPathParam.TrimStartAndEnd().IsEmpty())
    {
        return ErrorResponse(TEXT("Missing 'source_blueprint_path' parameter"));
    }

    FString TargetBlueprintPathParam;
    if (!EffectiveParams->TryGetStringField(TEXT("target_blueprint_path"), TargetBlueprintPathParam) || TargetBlueprintPathParam.TrimStartAndEnd().IsEmpty())
    {
        return ErrorResponse(TEXT("Missing 'target_blueprint_path' parameter"));
    }

    FString ComponentNameFilter;
    if (!EffectiveParams->TryGetStringField(TEXT("component_name"), ComponentNameFilter) || ComponentNameFilter.TrimStartAndEnd().IsEmpty())
    {
        return ErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    FString SourceSystemPathParam;
    if (!EffectiveParams->TryGetStringField(TEXT("source_system_path"), SourceSystemPathParam) || SourceSystemPathParam.TrimStartAndEnd().IsEmpty())
    {
        return ErrorResponse(TEXT("Missing 'source_system_path' parameter"));
    }

    FString ReplacementSystemPathParam;
    if (!EffectiveParams->TryGetStringField(TEXT("replacement_system_path"), ReplacementSystemPathParam) || ReplacementSystemPathParam.TrimStartAndEnd().IsEmpty())
    {
        return ErrorResponse(TEXT("Missing 'replacement_system_path' parameter"));
    }

    if (Forbidden(SourceBlueprintPathParam))
    {
        return ErrorResponse(DependencyError(TEXT("source_blueprint_path"), SourceBlueprintPathParam));
    }
    if (Forbidden(TargetBlueprintPathParam))
    {
        return ErrorResponse(DependencyError(TEXT("target_blueprint_path"), TargetBlueprintPathParam));
    }
    if (Forbidden(SourceSystemPathParam))
    {
        return ErrorResponse(DependencyError(TEXT("source_system_path"), SourceSystemPathParam));
    }
    if (Forbidden(ReplacementSystemPathParam))
    {
        return ErrorResponse(DependencyError(TEXT("replacement_system_path"), ReplacementSystemPathParam));
    }

    const FString SourceBlueprintObjectPath = NormalizeEditorBlueprintObjectPath(SourceBlueprintPathParam);
    const FString TargetBlueprintObjectPath = NormalizeEditorBlueprintObjectPath(TargetBlueprintPathParam);
    const FString SourceBlueprintPackageName = FPackageName::ObjectPathToPackageName(SourceBlueprintObjectPath);
    const FString TargetBlueprintPackageName = FPackageName::ObjectPathToPackageName(TargetBlueprintObjectPath);
    if (!FPackageName::IsValidLongPackageName(SourceBlueprintPackageName))
    {
        return ErrorResponse(FString::Printf(TEXT("Invalid source_blueprint_path: %s"), *SourceBlueprintPathParam));
    }
    if (!FPackageName::IsValidLongPackageName(TargetBlueprintPackageName))
    {
        return ErrorResponse(FString::Printf(TEXT("Invalid target_blueprint_path: %s"), *TargetBlueprintPathParam));
    }
    if (!TargetBlueprintPackageName.StartsWith(TEXT("/Game/_MCP_Temp/")))
    {
        return ErrorResponse(FString::Printf(TEXT("target_blueprint_path must be under /Game/_MCP_Temp/: %s"), *TargetBlueprintPackageName));
    }
    if (SourceBlueprintPackageName.Equals(TargetBlueprintPackageName, ESearchCase::IgnoreCase))
    {
        return ErrorResponse(TEXT("source_blueprint_path and target_blueprint_path must not resolve to the same package"));
    }
    if (Forbidden(SourceBlueprintObjectPath))
    {
        return ErrorResponse(DependencyError(TEXT("source_blueprint_path"), SourceBlueprintObjectPath));
    }
    if (Forbidden(TargetBlueprintObjectPath))
    {
        return ErrorResponse(DependencyError(TEXT("target_blueprint_path"), TargetBlueprintObjectPath));
    }

    const FString SourceSystemObjectPath = NormalizeNiagaraSystemObjectPath(SourceSystemPathParam);
    const FString ReplacementSystemObjectPath = NormalizeNiagaraSystemObjectPath(ReplacementSystemPathParam);
    if (Forbidden(SourceSystemObjectPath))
    {
        return ErrorResponse(DependencyError(TEXT("source_system_path"), SourceSystemObjectPath));
    }
    if (Forbidden(ReplacementSystemObjectPath))
    {
        return ErrorResponse(DependencyError(TEXT("replacement_system_path"), ReplacementSystemObjectPath));
    }

    UNiagaraSystem* SourceSystem = LoadExact<UNiagaraSystem>(SourceSystemObjectPath);
    if(!ContinueRead())return ErrorResponse(W.R.Error); W.SourceSystem=SourceSystem;
    if (!SourceSystem)
    {
        return ErrorResponse(FString::Printf(TEXT("Failed to load source Niagara system: %s"), *SourceSystemObjectPath));
    }

    UNiagaraSystem* ReplacementSystem = LoadExact<UNiagaraSystem>(ReplacementSystemObjectPath);
    if(!ContinueRead())return ErrorResponse(W.R.Error); W.Replacement=ReplacementSystem;
    if (!ReplacementSystem)
    {
        return ErrorResponse(FString::Printf(TEXT("Failed to load replacement Niagara system: %s"), *ReplacementSystemObjectPath));
    }

    const FString ParameterNameContains = GetStringParam(EffectiveParams, TEXT("parameter_name_contains"), FString());
    const bool bRequireExactlyOne = GetBoolParam(EffectiveParams, TEXT("require_exactly_one"), true);
    const bool bRequireSourceSystemMatch = GetBoolParam(EffectiveParams, TEXT("require_source_system_match"), true);
    const bool bReplaceExistingTarget = GetBoolParam(EffectiveParams, TEXT("replace_existing_target"), false);
    const bool bAllowDirtySourceBlueprint = GetBoolParam(EffectiveParams, TEXT("allow_dirty_source_blueprint"), false);
    const bool bCompileBlueprint = GetBoolParam(EffectiveParams, TEXT("compile_blueprint"), false);
    const bool bSaveBlueprint = GetBoolParam(EffectiveParams, TEXT("save_blueprint"), false);
    const bool bIncludeParameterValues = GetBoolParam(EffectiveParams, TEXT("include_parameter_values"), true);
    const bool bIncludeRawBytes = GetBoolParam(EffectiveParams, TEXT("include_raw_bytes"), false);
    const bool bIncludeParameterObjectProperties = GetBoolParam(EffectiveParams, TEXT("include_parameter_object_properties"), false);
    const bool bIncludeComponentObjectProperties = GetBoolParam(EffectiveParams, TEXT("include_component_object_properties"), false);
    const int32 MaxComponents = FMath::Clamp(static_cast<int32>(GetNumberParam(EffectiveParams, TEXT("max_components"), 8.0)), 1, 64);
    const int32 MaxParameters = FMath::Clamp(static_cast<int32>(GetNumberParam(EffectiveParams, TEXT("max_parameters"), 256.0)), 0, 4096);
    const int32 MaxRawBytes = FMath::Clamp(static_cast<int32>(GetNumberParam(EffectiveParams, TEXT("max_raw_bytes"), 64.0)), 0, 4096);
    const int32 MaxObjectProperties = FMath::Clamp(static_cast<int32>(GetNumberParam(EffectiveParams, TEXT("max_object_properties"), 80.0)), 0, 512);
    const int32 MaxArrayElements = FMath::Clamp(static_cast<int32>(GetNumberParam(EffectiveParams, TEXT("max_array_elements"), 16.0)), 0, 256);
    const int32 MaxPropertyValueChars = FMath::Clamp(static_cast<int32>(GetNumberParam(EffectiveParams, TEXT("max_property_value_chars"), 512.0)), 0, 16384);
    if(!ContinueRead())return ErrorResponse(W.R.Error);

    const TArray<FString> DirtyPackagesBeforeCommand = GetDirtyPackageNames();
    const bool bSourceBlueprintPackageLoadedBefore = FindPackage(nullptr, *SourceBlueprintPackageName) != nullptr;
    const bool bSourceBlueprintPackageDirtyBefore = StringArrayContainsExact(DirtyPackagesBeforeCommand, SourceBlueprintPackageName);
    const bool bTargetBlueprintPackageLoadedBefore = FindPackage(nullptr, *TargetBlueprintPackageName) != nullptr;
    const bool bTargetBlueprintPackageDirtyBefore = StringArrayContainsExact(DirtyPackagesBeforeCommand, TargetBlueprintPackageName);
    const bool bTargetAssetExistedBefore = TargetExists(TargetBlueprintObjectPath);

    if (!bAllowDirtySourceBlueprint && bSourceBlueprintPackageDirtyBefore)
    {
        return ErrorResponse(FString::Printf(TEXT("Source Blueprint package is already dirty; refusing to duplicate unsaved state: %s"), *SourceBlueprintPackageName));
    }

    UBlueprint* SourceBlueprint = LoadExact<UBlueprint>(SourceBlueprintObjectPath);
    if (!SourceBlueprint)
    {
        return ErrorResponse(FString::Printf(TEXT("Failed to load source Blueprint asset: %s"), *SourceBlueprintObjectPath));
    }

    auto& ReplacementWork=*W.ReplacementLease;
    bool bDeletedExistingTarget=false;
    UBlueprint* TargetBlueprint=SourceBlueprint;
    UObject* DuplicatedObject=nullptr;
    if(!bDryRun)
    {
        if(bTargetAssetExistedBefore)
        {
            if(!bReplaceExistingTarget){W.Fail(TEXT("blueprint_niagara_canary_target_exists"),TEXT("Existing target requires explicit replace_existing_target permission"));return ErrorResponse(W.R.Error);}
            if(bTargetBlueprintPackageDirtyBefore){W.Fail(TEXT("blueprint_niagara_canary_replace_dirty_target"),TEXT("Existing target is dirty; original replacement permission does not permit dirty deletion"));return ErrorResponse(W.R.Error);}
            TargetBlueprint=ReplacementWork.Replace(SourceBlueprint,TargetBlueprintObjectPath,bDeletedExistingTarget);
        }
        else TargetBlueprint=DuplicateCanary(SourceBlueprint,TargetBlueprintObjectPath);
        DuplicatedObject=TargetBlueprint;
        if(!TargetBlueprint)return ErrorResponse(W.R.Error.IsEmpty()?TEXT("DuplicateAsset returned null; residual package/object may remain"):W.R.Error);
    }
    if(!ContinueRead())return ErrorResponse(W.R.Error);
    if(!W.Blueprint(TargetBlueprint))return ErrorResponse(W.R.Error);
    UBlueprintGeneratedClass* GeneratedClass = TargetBlueprint ? Cast<UBlueprintGeneratedClass>(TargetBlueprint->GeneratedClass) : nullptr;
    if (!GeneratedClass || !GeneratedClass->SimpleConstructionScript)
    {
        return ErrorResponse(FString::Printf(TEXT("Target Blueprint does not have a mutable local SimpleConstructionScript: %s"), *(TargetBlueprint ? TargetBlueprint->GetPathName() : TargetBlueprintObjectPath)));
    }

    auto BuildSnapshot = [&](
        const FString& Phase,
        USCS_Node* Node,
        UNiagaraComponent* NiagaraTemplate,
        const FString& AssignedSystemPath,
        bool bSourceSystemMatches,
        bool bReplacementSystemMatches) -> TSharedPtr<FJsonObject>
    {
        TSharedPtr<FJsonObject> Snapshot = MakeShared<FJsonObject>();
        if(!ContinueRead())return Snapshot;
        Snapshot->SetStringField(TEXT("phase"), Phase);
        Snapshot->SetStringField(TEXT("asset_path"), AssignedSystemPath);
        Snapshot->SetStringField(TEXT("asset_class"), NiagaraRuntimeStateClassName(NiagaraTemplate ? NiagaraTemplate->GetAsset() : nullptr));
        Snapshot->SetBoolField(TEXT("source_system_matches"), bSourceSystemMatches);
        Snapshot->SetBoolField(TEXT("replacement_system_matches"), bReplacementSystemMatches);
        Snapshot->SetBoolField(TEXT("has_component"), NiagaraTemplate != nullptr);
        if (Node)
        {
            Snapshot->SetStringField(TEXT("component_name"), Node->GetVariableName().ToString().IsEmpty() ? (NiagaraTemplate ? NiagaraTemplate->GetName() : FString()) : Node->GetVariableName().ToString());
            Snapshot->SetStringField(TEXT("scs_variable_name"), Node->GetVariableName().ToString());
            Snapshot->SetStringField(TEXT("scs_node_path"), Node->GetPathName());
        }
        if (NiagaraTemplate)
        {
            Snapshot->SetStringField(TEXT("component_template_name"), NiagaraTemplate->GetName());
            Snapshot->SetStringField(TEXT("component_template_path"), NiagaraTemplate->GetPathName());
            Snapshot->SetStringField(TEXT("component_class"), NiagaraRuntimeStateClassName(NiagaraTemplate));
            Snapshot->SetNumberField(TEXT("creation_method"), static_cast<int32>(NiagaraTemplate->CreationMethod));
            Snapshot->SetBoolField(TEXT("auto_activate"), NiagaraTemplate->bAutoActivate);
            Snapshot->SetBoolField(TEXT("visible"), NiagaraTemplate->IsVisible());
            Snapshot->SetBoolField(TEXT("component_tick_enabled"), NiagaraTemplate->IsComponentTickEnabled());
            AddVectorField(Snapshot, TEXT("relative_location"), NiagaraTemplate->GetRelativeLocation());
            AddRotatorField(Snapshot, TEXT("relative_rotation"), NiagaraTemplate->GetRelativeRotation());
            AddVectorField(Snapshot, TEXT("relative_scale"), NiagaraTemplate->GetRelativeScale3D());
            W.R.Data->SetObjectField(TEXT("last_component_partial"),Snapshot);
            AddNiagaraRuntimeStateStore(
                Snapshot,
                TEXT("override_parameters"),
                NiagaraTemplate->GetOverrideParameters(),
                ParameterNameContains,
                MaxParameters,
                bIncludeParameterValues,
                bIncludeRawBytes,
                MaxRawBytes,
                bIncludeParameterObjectProperties,
                MaxObjectProperties,
                MaxArrayElements,
                MaxPropertyValueChars);
            if(!ContinueRead())return Snapshot;
            if (bIncludeComponentObjectProperties)
            {
                Snapshot->SetObjectField(
                    TEXT("component_object_properties"),
                    BuildNiagaraRuntimeStateObjectPropertyPreview(
                        NiagaraTemplate,
                        MaxObjectProperties,
                        MaxArrayElements,
                        MaxPropertyValueChars));
                if(!ContinueRead())return Snapshot;
            }
        }
        return Snapshot;
    };

    int32 ScannedScsNodeCount = 0;
    int32 ScannedNiagaraTemplateCount = 0;
    int32 MatchingComponentTemplateCount = 0;
    int32 SourceSystemMatchCount = 0;
    int32 ReturnedComponentTemplateCount = 0;
    bool bTruncated = false;
    UNiagaraComponent* MatchedNiagaraTemplate = nullptr;
    USCS_Node* MatchedNode = nullptr;
    TArray<TSharedPtr<FJsonValue>> ComponentResults;

    const TArray<USCS_Node*> Nodes = CaptureNodes(GeneratedClass);
        if(!ContinueRead())return ErrorResponse(W.R.Error);
    for (USCS_Node* Node : Nodes)
    {
        if (!Node)
        {
            continue;
        }

        ++ScannedScsNodeCount;
        UActorComponent* ComponentTemplate = ActualTemplate(Node,GeneratedClass);
            if(!ContinueRead())return ErrorResponse(W.R.Error);
        UNiagaraComponent* NiagaraTemplate = Cast<UNiagaraComponent>(ComponentTemplate);
        if (!NiagaraTemplate)
        {
            continue;
        }

        ++ScannedNiagaraTemplateCount;
        const FString ScsVariableName = Node->GetVariableName().ToString();
        const FString ComponentTemplateName = NiagaraTemplate->GetName();
        const FString ComponentTemplatePath = NiagaraTemplate->GetPathName();
        const FString ComponentSearchText = FString::Printf(
            TEXT("%s|%s|%s"),
            *ScsVariableName,
            *ComponentTemplateName,
            *NiagaraRuntimeStateClassName(NiagaraTemplate));
        if (!NiagaraRuntimeStateMatchesFilter(ComponentSearchText, ComponentNameFilter))
        {
            continue;
        }

        ++MatchingComponentTemplateCount;
        UNiagaraSystem* AssignedSystem = NiagaraTemplate->GetAsset();
        const FString AssignedSystemPath = NiagaraRuntimeStateObjectPath(AssignedSystem);
        const bool bSourceSystemMatches = AssignedSystemPath.Equals(SourceSystemObjectPath, ESearchCase::IgnoreCase);
        if (bSourceSystemMatches)
        {
            ++SourceSystemMatchCount;
        }
        if (!MatchedNiagaraTemplate)
        {
            MatchedNiagaraTemplate = NiagaraTemplate;
            MatchedNode = Node;
        }

        if (ReturnedComponentTemplateCount >= MaxComponents)
        {
            bTruncated = true;
            continue;
        }

        TSharedPtr<FJsonObject> ComponentObject = MakeShared<FJsonObject>();
        ComponentObject->SetStringField(TEXT("source"), TEXT("simple_construction_script"));
        ComponentObject->SetBoolField(TEXT("dry_run"), bDryRun);
        ComponentObject->SetBoolField(TEXT("canary_temp_target"), !bDryRun);
        ComponentObject->SetStringField(TEXT("component_name"), ScsVariableName.IsEmpty() ? ComponentTemplateName : ScsVariableName);
        ComponentObject->SetStringField(TEXT("component_template_name"), ComponentTemplateName);
        ComponentObject->SetStringField(TEXT("component_template_path"), ComponentTemplatePath);
        ComponentObject->SetStringField(TEXT("scs_variable_name"), ScsVariableName);
        ComponentObject->SetStringField(TEXT("current_asset_path"), AssignedSystemPath);
        ComponentObject->SetStringField(TEXT("source_system_path"), SourceSystemObjectPath);
        ComponentObject->SetStringField(TEXT("replacement_system_path"), ReplacementSystemObjectPath);
        ComponentObject->SetBoolField(TEXT("source_system_matches"), bSourceSystemMatches);
        ComponentObject->SetBoolField(TEXT("asset_mutation_performed"), false);
        ComponentObject->SetObjectField(TEXT("before"), BuildSnapshot(TEXT("before_canary"), Node, NiagaraTemplate, AssignedSystemPath, bSourceSystemMatches, false));
        if(!ContinueRead())return ErrorResponse(W.R.Error);
        ComponentResults.Add(MakeShared<FJsonValueObject>(ComponentObject));
        ++ReturnedComponentTemplateCount;
    }

    TArray<FString> PreflightFailureReasons;
    if (bRequireExactlyOne && MatchingComponentTemplateCount != 1)
    {
        PreflightFailureReasons.Add(FString::Printf(TEXT("require_exactly_one expected 1 matching Niagara component template but found %d"), MatchingComponentTemplateCount));
    }
    if (bRequireSourceSystemMatch && MatchingComponentTemplateCount == 0)
    {
        PreflightFailureReasons.Add(TEXT("require_source_system_match found no matching Niagara component templates to verify"));
    }
    if (bRequireSourceSystemMatch && MatchingComponentTemplateCount > 0 && SourceSystemMatchCount != MatchingComponentTemplateCount)
    {
        PreflightFailureReasons.Add(FString::Printf(TEXT("source_system_path matched %d of %d matching Niagara component templates"), SourceSystemMatchCount, MatchingComponentTemplateCount));
    }
    const bool bPreflightPass = PreflightFailureReasons.IsEmpty();

    bool bAssetMutationPerformed = false;
    bool bReplacementSystemMatchesAfter = false;
    bool bBlueprintWasCompiled = false;
    bool bBlueprintCompilePass = false;
    bool bBlueprintWasSaved = false;
    FString CompileStatusBefore;
    FString CompileStatusAfter;
    FString SaveSkippedReason;
    TSharedPtr<FJsonObject> AfterMutationSnapshot;

    if (!bDryRun && bPreflightPass && MatchedNiagaraTemplate)
    {
        if(!SwapLocal(TargetBlueprint,MatchedNode,MatchedNiagaraTemplate,ReplacementSystem))return ErrorResponse(W.R.Error);
        bAssetMutationPerformed = true;

        const FString AssignedSystemPathAfter = NiagaraRuntimeStateObjectPath(MatchedNiagaraTemplate->GetAsset());
        bReplacementSystemMatchesAfter = AssignedSystemPathAfter.Equals(ReplacementSystemObjectPath, ESearchCase::IgnoreCase);
        AfterMutationSnapshot = BuildSnapshot(
            TEXT("after_canary_set_asset"),
            MatchedNode,
            MatchedNiagaraTemplate,
            AssignedSystemPathAfter,
            AssignedSystemPathAfter.Equals(SourceSystemObjectPath, ESearchCase::IgnoreCase),
            bReplacementSystemMatchesAfter);
        if(!ContinueRead())return ErrorResponse(W.R.Error);

        if (bCompileBlueprint)
        {
            CompileStatusBefore = StaticEnum<EBlueprintStatus>()
                ? StaticEnum<EBlueprintStatus>()->GetNameStringByValue(static_cast<int64>(TargetBlueprint->Status))
                : FString::FromInt(static_cast<int32>(TargetBlueprint->Status));
            if(!CompileOwned(TargetBlueprint,MatchedNode,MatchedNiagaraTemplate,GeneratedClass))return ErrorResponse(W.R.Error);
            bBlueprintWasCompiled = true;
            CompileStatusAfter = StaticEnum<EBlueprintStatus>()
                ? StaticEnum<EBlueprintStatus>()->GetNameStringByValue(static_cast<int64>(TargetBlueprint->Status))
                : FString::FromInt(static_cast<int32>(TargetBlueprint->Status));
            bBlueprintCompilePass = TargetBlueprint->Status == BS_UpToDate || TargetBlueprint->Status == BS_UpToDateWithWarnings;
        }
        else
        {
            bBlueprintCompilePass = true;
        }

        if (bSaveBlueprint)
        {
            if (!bBlueprintCompilePass)
            {
                SaveSkippedReason = TEXT("compile_failed");
            }
            else
            {
                bBlueprintWasSaved = SaveOwned(TargetBlueprint);
            if(!ContinueRead())return ErrorResponse(W.R.Error);
                if (!bBlueprintWasSaved)
                {
                    SaveSkippedReason = TEXT("save_failed");
                }
            }
        }
        else
        {
            SaveSkippedReason = TEXT("save_blueprint_false");
        }
    }
    else if (!bDryRun && !bPreflightPass)
    {
        SaveSkippedReason = TEXT("preflight_failed");
    }
    else if (bDryRun)
    {
        SaveSkippedReason = TEXT("dry_run_true");
    }

    if (ComponentResults.Num() > 0 && AfterMutationSnapshot.IsValid())
    {
        TSharedPtr<FJsonObject> FirstComponent = ComponentResults[0]->AsObject();
        if (FirstComponent.IsValid())
        {
            FirstComponent->SetBoolField(TEXT("asset_mutation_performed"), bAssetMutationPerformed);
            FirstComponent->SetObjectField(TEXT("after"), AfterMutationSnapshot);
            FirstComponent->SetBoolField(TEXT("replacement_system_matches_after"), bReplacementSystemMatchesAfter);
        }
    }

    if(!ContinueRead())return ErrorResponse(W.R.Error);
    UPackage* SourceBlueprintPackage = SourceBlueprint ? SourceBlueprint->GetOutermost() : nullptr;
    UPackage* TargetBlueprintPackage = TargetBlueprint ? TargetBlueprint->GetOutermost() : nullptr;
    const bool bSourceBlueprintPackageDirtyAfter = SourceBlueprintPackage ? SourceBlueprintPackage->IsDirty() : StringArrayContainsExact(GetDirtyPackageNames(), SourceBlueprintPackageName);
    const bool bTargetBlueprintPackageDirtyAfter = TargetBlueprintPackage ? TargetBlueprintPackage->IsDirty() : StringArrayContainsExact(GetDirtyPackageNames(), TargetBlueprintPackageName);
    const bool bSourceBlueprintPackageDirtyAddedByCommand = !bSourceBlueprintPackageDirtyBefore && bSourceBlueprintPackageDirtyAfter;
    const bool bTargetBlueprintPackageDirtyAddedByCommand = !bTargetBlueprintPackageDirtyBefore && bTargetBlueprintPackageDirtyAfter;

    TSharedPtr<FJsonObject> CompileResult = MakeShared<FJsonObject>();
    CompileResult->SetBoolField(TEXT("requested"), bCompileBlueprint);
    CompileResult->SetBoolField(TEXT("compiled"), bBlueprintWasCompiled);
    CompileResult->SetBoolField(TEXT("validation_pass"), bBlueprintCompilePass);
    CompileResult->SetStringField(TEXT("status_before"), CompileStatusBefore);
    CompileResult->SetStringField(TEXT("status_after"), CompileStatusAfter);

    TSharedPtr<FJsonObject> SaveResult = MakeShared<FJsonObject>();
    SaveResult->SetBoolField(TEXT("requested"), bSaveBlueprint);
    SaveResult->SetBoolField(TEXT("saved"), bBlueprintWasSaved);
    SaveResult->SetStringField(TEXT("skipped_reason"), SaveSkippedReason);

    if(!ContinueRead())return ErrorResponse(W.R.Error);
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("operation"), Operation);
    ResultObj->SetBoolField(TEXT("dry_run"), bDryRun);
    ResultObj->SetBoolField(TEXT("preflight_pass"), bPreflightPass);
    ResultObj->SetArrayField(TEXT("preflight_failure_reasons"), StringArrayToJsonArray(PreflightFailureReasons));
    ResultObj->SetStringField(TEXT("source_blueprint_path"), SourceBlueprintPathParam);
    ResultObj->SetStringField(TEXT("source_blueprint_object_path"), SourceBlueprintObjectPath);
    ResultObj->SetStringField(TEXT("source_blueprint_package_name"), SourceBlueprintPackageName);
    ResultObj->SetStringField(TEXT("target_blueprint_path"), TargetBlueprintPathParam);
    ResultObj->SetStringField(TEXT("target_blueprint_object_path"), TargetBlueprintObjectPath);
    ResultObj->SetStringField(TEXT("target_blueprint_package_name"), TargetBlueprintPackageName);
    ResultObj->SetBoolField(TEXT("target_under_temp_root"), TargetBlueprintPackageName.StartsWith(TEXT("/Game/_MCP_Temp/")));
    ResultObj->SetBoolField(TEXT("target_asset_existed_before"), bTargetAssetExistedBefore);
    if(ReplacementWork.Active&&!ReplacementWork.SourceFence(TEXT("replacement_canary_final_source")))return ErrorResponse(W.R.Error);
    ResultObj->SetBoolField(TEXT("target_asset_deleted_before_duplicate"), bDeletedExistingTarget);
    ResultObj->SetBoolField(TEXT("target_asset_created_by_command"), !bDryRun && TargetBlueprint != nullptr);
    ResultObj->SetStringField(TEXT("target_created_object_path"), TargetBlueprint ? TargetBlueprint->GetPathName() : FString());
    ResultObj->SetStringField(TEXT("component_name"), ComponentNameFilter);
    ResultObj->SetStringField(TEXT("source_system_path"), SourceSystemObjectPath);
    ResultObj->SetStringField(TEXT("replacement_system_path"), ReplacementSystemObjectPath);
    ResultObj->SetBoolField(TEXT("source_system_asset_exists"), SourceSystem != nullptr);
    ResultObj->SetBoolField(TEXT("replacement_system_asset_exists"), ReplacementSystem != nullptr);
    ResultObj->SetBoolField(TEXT("preserve_override_parameters"), bPreserveOverrideParameters);
    ResultObj->SetBoolField(TEXT("include_inherited_scs_templates"), false);
    ResultObj->SetNumberField(TEXT("scs_owner_class_count"), GeneratedClass ? 1 : 0);
    ResultObj->SetNumberField(TEXT("scs_node_count_scanned"), ScannedScsNodeCount);
    ResultObj->SetNumberField(TEXT("niagara_component_template_count_scanned"), ScannedNiagaraTemplateCount);
    ResultObj->SetNumberField(TEXT("matching_component_template_count"), MatchingComponentTemplateCount);
    ResultObj->SetNumberField(TEXT("source_system_match_count"), SourceSystemMatchCount);
    ResultObj->SetNumberField(TEXT("component_template_count"), ReturnedComponentTemplateCount);
    ResultObj->SetNumberField(TEXT("max_components"), MaxComponents);
    ResultObj->SetBoolField(TEXT("truncated"), bTruncated);
    ResultObj->SetArrayField(TEXT("components"), ComponentResults);
    ResultObj->SetBoolField(TEXT("read_only"), bDryRun);
    ResultObj->SetBoolField(TEXT("loads_source_blueprint_asset"), true);
    ResultObj->SetBoolField(TEXT("duplicates_source_to_temp_target"), !bDryRun);
    ResultObj->SetBoolField(TEXT("loads_source_system_asset"), true);
    ResultObj->SetBoolField(TEXT("loads_replacement_system_asset"), true);
    ResultObj->SetBoolField(TEXT("saves_or_reloads_packages"), bSaveBlueprint);
    ResultObj->SetBoolField(TEXT("source_blueprint_package_loaded_before"), bSourceBlueprintPackageLoadedBefore);
    ResultObj->SetBoolField(TEXT("source_blueprint_package_dirty_before"), bSourceBlueprintPackageDirtyBefore);
    ResultObj->SetBoolField(TEXT("source_blueprint_package_dirty_after"), bSourceBlueprintPackageDirtyAfter);
    ResultObj->SetBoolField(TEXT("source_blueprint_package_dirty_added_by_command"), bSourceBlueprintPackageDirtyAddedByCommand);
    ResultObj->SetBoolField(TEXT("target_blueprint_package_loaded_before"), bTargetBlueprintPackageLoadedBefore);
    ResultObj->SetBoolField(TEXT("target_blueprint_package_dirty_before"), bTargetBlueprintPackageDirtyBefore);
    ResultObj->SetBoolField(TEXT("target_blueprint_package_dirty_after"), bTargetBlueprintPackageDirtyAfter);
    ResultObj->SetBoolField(TEXT("target_blueprint_package_dirty_added_by_command"), bTargetBlueprintPackageDirtyAddedByCommand);
    ResultObj->SetBoolField(TEXT("blueprint_was_saved"), bBlueprintWasSaved);
    ResultObj->SetBoolField(TEXT("blueprint_was_compiled"), bBlueprintWasCompiled);
    ResultObj->SetObjectField(TEXT("compile_result"), CompileResult);
    ResultObj->SetObjectField(TEXT("save_result"), SaveResult);
    ResultObj->SetBoolField(TEXT("asset_mutation_admitted"), !bDryRun && bPreflightPass);
    ResultObj->SetBoolField(TEXT("asset_mutation_performed"), bAssetMutationPerformed);
    ResultObj->SetBoolField(TEXT("replacement_system_matches_after"), bReplacementSystemMatchesAfter);
    ResultObj->SetBoolField(TEXT("source_blueprint_asset_write_admitted"), false);
    ResultObj->SetBoolField(TEXT("source_blueprint_mutated"), false);
    ResultObj->SetBoolField(TEXT("target_blueprint_asset_write_admitted"), !bDryRun);
    ResultObj->SetBoolField(TEXT("target_blueprint_mutated"), bAssetMutationPerformed);
    ResultObj->SetBoolField(TEXT("blueprint_asset_write_admitted"), !bDryRun);
    ResultObj->SetBoolField(TEXT("durable_mutation_admitted"), false);
    ResultObj->SetBoolField(TEXT("persistent_original_write_admitted"), false);
    ResultObj->SetBoolField(TEXT("actual_bp_asset_write_tool_exists"), false);
    ResultObj->SetBoolField(TEXT("temp_canary_bp_asset_write_tool_exists"), true);
    ResultObj->SetStringField(TEXT("confirmation_token"), ConfirmationToken);
    ResultObj->SetStringField(TEXT("safety_note"), TEXT("Duplicates the source Blueprint to a /Game/_MCP_Temp canary target, mutates only that temp target's local SCS UNiagaraComponent template with override preservation, optionally compiles/saves the temp target, and never writes the original Blueprint or admits durable project-content mutation."));
    AddDirtyPackageCommandDelta(ResultObj, DirtyPackagesBeforeCommand);
    return ResultObj;
}

TSharedPtr<FJsonObject> HandleDurable(const TSharedPtr<FJsonObject>& Params)
{
    auto& W=*Current;
    TSharedPtr<FJsonObject> EffectiveParams = Params.IsValid() ? Params : MakeShared<FJsonObject>();

    const FString Operation = GetStringParam(EffectiveParams, TEXT("operation"), TEXT("durable_bp_component_template_asset_swap"));
    if (Operation != TEXT("durable_bp_component_template_asset_swap"))
    {
        return ErrorResponse(TEXT("operation must be durable_bp_component_template_asset_swap"));
    }

    const bool bDryRun = GetBoolParam(EffectiveParams, TEXT("dry_run"), true);
    const FString ConfirmationToken = GetStringParam(EffectiveParams, TEXT("confirmation_token"), FString());
    if (ConfirmationToken != TEXT("ALLOW_DURABLE_BP_COMPONENT_TEMPLATE_EXECUTE"))
    {
        return ErrorResponse(TEXT("confirmation_token must be ALLOW_DURABLE_BP_COMPONENT_TEMPLATE_EXECUTE for the durable BP component-template executor."));
    }

    const bool bPreserveOverrideParameters = GetBoolParam(EffectiveParams, TEXT("preserve_override_parameters"), true);
    if (!bPreserveOverrideParameters)
    {
        return ErrorResponse(TEXT("preserve_override_parameters must remain true; dropping component template override state is not admitted."));
    }

    if (GetBoolParam(EffectiveParams, TEXT("allow_unrealmcp_dependency"), false))
    {
        return ErrorResponse(TEXT("allow_unrealmcp_dependency must remain false; finished assets must not depend on UnrealMCP."));
    }

    const bool bIncludeInheritedScsTemplates = GetBoolParam(EffectiveParams, TEXT("include_inherited_scs_templates"), false);
    if (bIncludeInheritedScsTemplates)
    {
        return ErrorResponse(TEXT("execute_blueprint_niagara_component_template_asset_swap_durable only admits include_inherited_scs_templates=false; inherited SCS template mutation could modify another Blueprint."));
    }

    const bool bCompileBlueprint = GetBoolParam(EffectiveParams, TEXT("compile_blueprint"), false);
    const bool bSaveBlueprint = GetBoolParam(EffectiveParams, TEXT("save_blueprint"), false);
    if (!bDryRun && !bCompileBlueprint)
    {
        return ErrorResponse(TEXT("durable execution requires compile_blueprint=true; use dry_run=true for preflight."));
    }
    if (!bDryRun && !bSaveBlueprint)
    {
        return ErrorResponse(TEXT("durable execution requires save_blueprint=true; use dry_run=true for preflight."));
    }

    FString BlueprintPathParam;
    if (!EffectiveParams->TryGetStringField(TEXT("blueprint_path"), BlueprintPathParam) || BlueprintPathParam.TrimStartAndEnd().IsEmpty())
    {
        return ErrorResponse(TEXT("Missing 'blueprint_path' parameter"));
    }

    FString ComponentNameFilter;
    if (!EffectiveParams->TryGetStringField(TEXT("component_name"), ComponentNameFilter) || ComponentNameFilter.TrimStartAndEnd().IsEmpty())
    {
        return ErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    FString SourceSystemPathParam;
    if (!EffectiveParams->TryGetStringField(TEXT("source_system_path"), SourceSystemPathParam) || SourceSystemPathParam.TrimStartAndEnd().IsEmpty())
    {
        return ErrorResponse(TEXT("Missing 'source_system_path' parameter"));
    }

    FString ReplacementSystemPathParam;
    if (!EffectiveParams->TryGetStringField(TEXT("replacement_system_path"), ReplacementSystemPathParam) || ReplacementSystemPathParam.TrimStartAndEnd().IsEmpty())
    {
        return ErrorResponse(TEXT("Missing 'replacement_system_path' parameter"));
    }

    if (Forbidden(BlueprintPathParam))
    {
        return ErrorResponse(DependencyError(TEXT("blueprint_path"), BlueprintPathParam));
    }
    if (Forbidden(SourceSystemPathParam))
    {
        return ErrorResponse(DependencyError(TEXT("source_system_path"), SourceSystemPathParam));
    }
    if (Forbidden(ReplacementSystemPathParam))
    {
        return ErrorResponse(DependencyError(TEXT("replacement_system_path"), ReplacementSystemPathParam));
    }

    const FString BlueprintObjectPath = NormalizeEditorBlueprintObjectPath(BlueprintPathParam);
    const FString BlueprintPackageName = FPackageName::ObjectPathToPackageName(BlueprintObjectPath);
    if (!FPackageName::IsValidLongPackageName(BlueprintPackageName))
    {
        return ErrorResponse(FString::Printf(TEXT("Invalid blueprint_path: %s"), *BlueprintPathParam));
    }
    if (Forbidden(BlueprintObjectPath))
    {
        return ErrorResponse(DependencyError(TEXT("blueprint_path"), BlueprintObjectPath));
    }

    const FString SourceSystemObjectPath = NormalizeNiagaraSystemObjectPath(SourceSystemPathParam);
    const FString ReplacementSystemObjectPath = NormalizeNiagaraSystemObjectPath(ReplacementSystemPathParam);
    if (Forbidden(SourceSystemObjectPath))
    {
        return ErrorResponse(DependencyError(TEXT("source_system_path"), SourceSystemObjectPath));
    }
    if (Forbidden(ReplacementSystemObjectPath))
    {
        return ErrorResponse(DependencyError(TEXT("replacement_system_path"), ReplacementSystemObjectPath));
    }

    UNiagaraSystem* SourceSystem = LoadExact<UNiagaraSystem>(SourceSystemObjectPath);
    if(!ContinueRead())return ErrorResponse(W.R.Error); W.SourceSystem=SourceSystem;
    if (!SourceSystem)
    {
        return ErrorResponse(FString::Printf(TEXT("Failed to load source Niagara system: %s"), *SourceSystemObjectPath));
    }

    UNiagaraSystem* ReplacementSystem = LoadExact<UNiagaraSystem>(ReplacementSystemObjectPath);
    if(!ContinueRead())return ErrorResponse(W.R.Error); W.Replacement=ReplacementSystem;
    if (!ReplacementSystem)
    {
        return ErrorResponse(FString::Printf(TEXT("Failed to load replacement Niagara system: %s"), *ReplacementSystemObjectPath));
    }

    const FString ParameterNameContains = GetStringParam(EffectiveParams, TEXT("parameter_name_contains"), FString());
    const bool bRequireExactlyOne = GetBoolParam(EffectiveParams, TEXT("require_exactly_one"), true);
    const bool bRequireSourceSystemMatch = GetBoolParam(EffectiveParams, TEXT("require_source_system_match"), true);
    const bool bAllowDirtyBlueprint = GetBoolParam(EffectiveParams, TEXT("allow_dirty_blueprint"), false);
    const bool bIncludeParameterValues = GetBoolParam(EffectiveParams, TEXT("include_parameter_values"), true);
    const bool bIncludeRawBytes = GetBoolParam(EffectiveParams, TEXT("include_raw_bytes"), false);
    const bool bIncludeParameterObjectProperties = GetBoolParam(EffectiveParams, TEXT("include_parameter_object_properties"), false);
    const bool bIncludeComponentObjectProperties = GetBoolParam(EffectiveParams, TEXT("include_component_object_properties"), false);
    const int32 MaxComponents = FMath::Clamp(static_cast<int32>(GetNumberParam(EffectiveParams, TEXT("max_components"), 8.0)), 1, 64);
    const int32 MaxParameters = FMath::Clamp(static_cast<int32>(GetNumberParam(EffectiveParams, TEXT("max_parameters"), 256.0)), 0, 4096);
    const int32 MaxRawBytes = FMath::Clamp(static_cast<int32>(GetNumberParam(EffectiveParams, TEXT("max_raw_bytes"), 64.0)), 0, 4096);
    const int32 MaxObjectProperties = FMath::Clamp(static_cast<int32>(GetNumberParam(EffectiveParams, TEXT("max_object_properties"), 80.0)), 0, 512);
    const int32 MaxArrayElements = FMath::Clamp(static_cast<int32>(GetNumberParam(EffectiveParams, TEXT("max_array_elements"), 16.0)), 0, 256);
    const int32 MaxPropertyValueChars = FMath::Clamp(static_cast<int32>(GetNumberParam(EffectiveParams, TEXT("max_property_value_chars"), 512.0)), 0, 16384);
    if(!ContinueRead())return ErrorResponse(W.R.Error);

    const TArray<FString> DirtyPackagesBeforeCommand = GetDirtyPackageNames();
    const bool bBlueprintPackageLoadedBefore = FindPackage(nullptr, *BlueprintPackageName) != nullptr;
    const bool bBlueprintPackageDirtyBefore = StringArrayContainsExact(DirtyPackagesBeforeCommand, BlueprintPackageName);
    if (!bAllowDirtyBlueprint && bBlueprintPackageDirtyBefore)
    {
        return ErrorResponse(FString::Printf(TEXT("Blueprint package is already dirty; refusing durable mutation of unsaved state: %s"), *BlueprintPackageName));
    }

    UBlueprint* Blueprint = LoadExact<UBlueprint>(BlueprintObjectPath);
    if (!Blueprint)
    {
        return ErrorResponse(FString::Printf(TEXT("Failed to load Blueprint asset: %s"), *BlueprintObjectPath));
    }

    UBlueprintGeneratedClass* GeneratedClass = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass);
    if (!GeneratedClass || !GeneratedClass->SimpleConstructionScript)
    {
        return ErrorResponse(FString::Printf(TEXT("Blueprint does not have a mutable local SimpleConstructionScript: %s"), *BlueprintObjectPath));
    }

    auto BuildSnapshot = [&](
        const FString& Phase,
        USCS_Node* Node,
        UNiagaraComponent* NiagaraTemplate,
        const FString& AssignedSystemPath,
        bool bSourceSystemMatches,
        bool bReplacementSystemMatches) -> TSharedPtr<FJsonObject>
    {
        TSharedPtr<FJsonObject> Snapshot = MakeShared<FJsonObject>();
        if(!ContinueRead())return Snapshot;
        Snapshot->SetStringField(TEXT("phase"), Phase);
        Snapshot->SetStringField(TEXT("asset_path"), AssignedSystemPath);
        Snapshot->SetStringField(TEXT("asset_class"), NiagaraRuntimeStateClassName(NiagaraTemplate ? NiagaraTemplate->GetAsset() : nullptr));
        Snapshot->SetBoolField(TEXT("source_system_matches"), bSourceSystemMatches);
        Snapshot->SetBoolField(TEXT("replacement_system_matches"), bReplacementSystemMatches);
        Snapshot->SetBoolField(TEXT("has_component"), NiagaraTemplate != nullptr);
        if (Node)
        {
            Snapshot->SetStringField(TEXT("component_name"), Node->GetVariableName().ToString().IsEmpty() ? (NiagaraTemplate ? NiagaraTemplate->GetName() : FString()) : Node->GetVariableName().ToString());
            Snapshot->SetStringField(TEXT("scs_variable_name"), Node->GetVariableName().ToString());
            Snapshot->SetStringField(TEXT("scs_node_path"), Node->GetPathName());
        }
        if (NiagaraTemplate)
        {
            Snapshot->SetStringField(TEXT("component_template_name"), NiagaraTemplate->GetName());
            Snapshot->SetStringField(TEXT("component_template_path"), NiagaraTemplate->GetPathName());
            Snapshot->SetStringField(TEXT("component_class"), NiagaraRuntimeStateClassName(NiagaraTemplate));
            Snapshot->SetNumberField(TEXT("creation_method"), static_cast<int32>(NiagaraTemplate->CreationMethod));
            Snapshot->SetBoolField(TEXT("auto_activate"), NiagaraTemplate->bAutoActivate);
            Snapshot->SetBoolField(TEXT("visible"), NiagaraTemplate->IsVisible());
            Snapshot->SetBoolField(TEXT("component_tick_enabled"), NiagaraTemplate->IsComponentTickEnabled());
            AddVectorField(Snapshot, TEXT("relative_location"), NiagaraTemplate->GetRelativeLocation());
            AddRotatorField(Snapshot, TEXT("relative_rotation"), NiagaraTemplate->GetRelativeRotation());
            AddVectorField(Snapshot, TEXT("relative_scale"), NiagaraTemplate->GetRelativeScale3D());
            W.R.Data->SetObjectField(TEXT("last_component_partial"),Snapshot);
            AddNiagaraRuntimeStateStore(
                Snapshot,
                TEXT("override_parameters"),
                NiagaraTemplate->GetOverrideParameters(),
                ParameterNameContains,
                MaxParameters,
                bIncludeParameterValues,
                bIncludeRawBytes,
                MaxRawBytes,
                bIncludeParameterObjectProperties,
                MaxObjectProperties,
                MaxArrayElements,
                MaxPropertyValueChars);
            if(!ContinueRead())return Snapshot;
            if (bIncludeComponentObjectProperties)
            {
                Snapshot->SetObjectField(
                    TEXT("component_object_properties"),
                    BuildNiagaraRuntimeStateObjectPropertyPreview(
                        NiagaraTemplate,
                        MaxObjectProperties,
                        MaxArrayElements,
                        MaxPropertyValueChars));
                if(!ContinueRead())return Snapshot;
            }
        }
        return Snapshot;
    };

    int32 ScannedScsNodeCount = 0;
    int32 ScannedNiagaraTemplateCount = 0;
    int32 MatchingComponentTemplateCount = 0;
    int32 SourceSystemMatchCount = 0;
    int32 ReturnedComponentTemplateCount = 0;
    bool bTruncated = false;
    UNiagaraComponent* MatchedNiagaraTemplate = nullptr;
    USCS_Node* MatchedNode = nullptr;
    TArray<TSharedPtr<FJsonValue>> ComponentResults;

    const TArray<USCS_Node*> Nodes = CaptureNodes(GeneratedClass);
        if(!ContinueRead())return ErrorResponse(W.R.Error);
    for (USCS_Node* Node : Nodes)
    {
        if (!Node)
        {
            continue;
        }

        ++ScannedScsNodeCount;
        UActorComponent* ComponentTemplate = ActualTemplate(Node,GeneratedClass);
            if(!ContinueRead())return ErrorResponse(W.R.Error);
        UNiagaraComponent* NiagaraTemplate = Cast<UNiagaraComponent>(ComponentTemplate);
        if (!NiagaraTemplate)
        {
            continue;
        }

        ++ScannedNiagaraTemplateCount;
        const FString ScsVariableName = Node->GetVariableName().ToString();
        const FString ComponentTemplateName = NiagaraTemplate->GetName();
        const FString ComponentSearchText = FString::Printf(
            TEXT("%s|%s|%s"),
            *ScsVariableName,
            *ComponentTemplateName,
            *NiagaraRuntimeStateClassName(NiagaraTemplate));
        if (!NiagaraRuntimeStateMatchesFilter(ComponentSearchText, ComponentNameFilter))
        {
            continue;
        }

        ++MatchingComponentTemplateCount;
        UNiagaraSystem* AssignedSystem = NiagaraTemplate->GetAsset();
        const FString AssignedSystemPath = NiagaraRuntimeStateObjectPath(AssignedSystem);
        const bool bSourceSystemMatches = AssignedSystemPath.Equals(SourceSystemObjectPath, ESearchCase::IgnoreCase);
        if (bSourceSystemMatches)
        {
            ++SourceSystemMatchCount;
        }
        if (!MatchedNiagaraTemplate)
        {
            MatchedNiagaraTemplate = NiagaraTemplate;
            MatchedNode = Node;
        }

        if (ReturnedComponentTemplateCount >= MaxComponents)
        {
            bTruncated = true;
            continue;
        }

        TSharedPtr<FJsonObject> ComponentObject = MakeShared<FJsonObject>();
        ComponentObject->SetStringField(TEXT("source"), TEXT("simple_construction_script"));
        ComponentObject->SetBoolField(TEXT("dry_run"), bDryRun);
        ComponentObject->SetBoolField(TEXT("durable_original_target"), true);
        ComponentObject->SetStringField(TEXT("component_name"), ScsVariableName.IsEmpty() ? ComponentTemplateName : ScsVariableName);
        ComponentObject->SetStringField(TEXT("component_template_name"), ComponentTemplateName);
        ComponentObject->SetStringField(TEXT("component_template_path"), NiagaraTemplate->GetPathName());
        ComponentObject->SetStringField(TEXT("scs_variable_name"), ScsVariableName);
        ComponentObject->SetStringField(TEXT("current_asset_path"), AssignedSystemPath);
        ComponentObject->SetStringField(TEXT("source_system_path"), SourceSystemObjectPath);
        ComponentObject->SetStringField(TEXT("replacement_system_path"), ReplacementSystemObjectPath);
        ComponentObject->SetBoolField(TEXT("source_system_matches"), bSourceSystemMatches);
        ComponentObject->SetBoolField(TEXT("asset_mutation_performed"), false);
        ComponentObject->SetObjectField(TEXT("before"), BuildSnapshot(TEXT("before_durable"), Node, NiagaraTemplate, AssignedSystemPath, bSourceSystemMatches, false));
        if(!ContinueRead())return ErrorResponse(W.R.Error);
        ComponentResults.Add(MakeShared<FJsonValueObject>(ComponentObject));
        ++ReturnedComponentTemplateCount;
    }

    TArray<FString> PreflightFailureReasons;
    if (bRequireExactlyOne && MatchingComponentTemplateCount != 1)
    {
        PreflightFailureReasons.Add(FString::Printf(TEXT("require_exactly_one expected 1 matching Niagara component template but found %d"), MatchingComponentTemplateCount));
    }
    if (bRequireSourceSystemMatch && MatchingComponentTemplateCount == 0)
    {
        PreflightFailureReasons.Add(TEXT("require_source_system_match found no matching Niagara component templates to verify"));
    }
    if (bRequireSourceSystemMatch && MatchingComponentTemplateCount > 0 && SourceSystemMatchCount != MatchingComponentTemplateCount)
    {
        PreflightFailureReasons.Add(FString::Printf(TEXT("source_system_path matched %d of %d matching Niagara component templates"), SourceSystemMatchCount, MatchingComponentTemplateCount));
    }
    const bool bPreflightPass = PreflightFailureReasons.IsEmpty();

    bool bAssetMutationPerformed = false;
    bool bReplacementSystemMatchesAfter = false;
    bool bBlueprintWasCompiled = false;
    bool bBlueprintCompilePass = bDryRun;
    bool bBlueprintWasSaved = false;
    FString CompileStatusBefore;
    FString CompileStatusAfter;
    FString SaveSkippedReason;
    TSharedPtr<FJsonObject> AfterMutationSnapshot;
    TSharedPtr<FJsonObject> StaticMarkerScan;

    if (!bDryRun && bPreflightPass && MatchedNiagaraTemplate)
    {
        if(!SwapLocal(Blueprint,MatchedNode,MatchedNiagaraTemplate,ReplacementSystem))return ErrorResponse(W.R.Error);
        bAssetMutationPerformed = true;

        const FString AssignedSystemPathAfter = NiagaraRuntimeStateObjectPath(MatchedNiagaraTemplate->GetAsset());
        bReplacementSystemMatchesAfter = AssignedSystemPathAfter.Equals(ReplacementSystemObjectPath, ESearchCase::IgnoreCase);
        AfterMutationSnapshot = BuildSnapshot(
            TEXT("after_durable_set_asset"),
            MatchedNode,
            MatchedNiagaraTemplate,
            AssignedSystemPathAfter,
            AssignedSystemPathAfter.Equals(SourceSystemObjectPath, ESearchCase::IgnoreCase),
            bReplacementSystemMatchesAfter);
        if(!ContinueRead())return ErrorResponse(W.R.Error);

        CompileStatusBefore = StaticEnum<EBlueprintStatus>()
            ? StaticEnum<EBlueprintStatus>()->GetNameStringByValue(static_cast<int64>(Blueprint->Status))
            : FString::FromInt(static_cast<int32>(Blueprint->Status));
        if(!CompileOwned(Blueprint,MatchedNode,MatchedNiagaraTemplate,GeneratedClass))return ErrorResponse(W.R.Error);
        bBlueprintWasCompiled = true;
        CompileStatusAfter = StaticEnum<EBlueprintStatus>()
            ? StaticEnum<EBlueprintStatus>()->GetNameStringByValue(static_cast<int64>(Blueprint->Status))
            : FString::FromInt(static_cast<int32>(Blueprint->Status));
        bBlueprintCompilePass = Blueprint->Status == BS_UpToDate || Blueprint->Status == BS_UpToDateWithWarnings;

        if (!bBlueprintCompilePass)
        {
            SaveSkippedReason = TEXT("compile_failed");
        }
        else
        {
            bBlueprintWasSaved = SaveOwned(Blueprint);
            if(!ContinueRead())return ErrorResponse(W.R.Error);
            if (bBlueprintWasSaved)
            {
                StaticMarkerScan = ScanPackageFileForMCPDependencyMarkers(BlueprintPackageName);
            }
            else
            {
                SaveSkippedReason = TEXT("save_failed");
            }
        }
    }
    else if (!bDryRun && !bPreflightPass)
    {
        SaveSkippedReason = TEXT("preflight_failed");
        bBlueprintCompilePass = false;
    }
    else if (bDryRun)
    {
        SaveSkippedReason = TEXT("dry_run_true");
    }

    if (ComponentResults.Num() > 0 && AfterMutationSnapshot.IsValid())
    {
        TSharedPtr<FJsonObject> FirstComponent = ComponentResults[0]->AsObject();
        if (FirstComponent.IsValid())
        {
            FirstComponent->SetBoolField(TEXT("asset_mutation_performed"), bAssetMutationPerformed);
            FirstComponent->SetObjectField(TEXT("after"), AfterMutationSnapshot);
            FirstComponent->SetBoolField(TEXT("replacement_system_matches_after"), bReplacementSystemMatchesAfter);
        }
    }

    if(!ContinueRead())return ErrorResponse(W.R.Error);
    UPackage* BlueprintPackage = Blueprint ? Blueprint->GetOutermost() : nullptr;
    const bool bBlueprintPackageDirtyAfter = BlueprintPackage ? BlueprintPackage->IsDirty() : StringArrayContainsExact(GetDirtyPackageNames(), BlueprintPackageName);
    const bool bBlueprintPackageDirtyAddedByCommand = !bBlueprintPackageDirtyBefore && bBlueprintPackageDirtyAfter;

    TSharedPtr<FJsonObject> CompileResult = MakeShared<FJsonObject>();
    CompileResult->SetBoolField(TEXT("requested"), bCompileBlueprint);
    CompileResult->SetBoolField(TEXT("compiled"), bBlueprintWasCompiled);
    CompileResult->SetBoolField(TEXT("validation_pass"), bBlueprintCompilePass);
    CompileResult->SetStringField(TEXT("status_before"), CompileStatusBefore);
    CompileResult->SetStringField(TEXT("status_after"), CompileStatusAfter);

    TSharedPtr<FJsonObject> SaveResult = MakeShared<FJsonObject>();
    SaveResult->SetBoolField(TEXT("requested"), bSaveBlueprint);
    SaveResult->SetBoolField(TEXT("saved"), bBlueprintWasSaved);
    SaveResult->SetStringField(TEXT("skipped_reason"), SaveSkippedReason);

    const bool bStaticMarkerScanPass = !StaticMarkerScan.IsValid() || StaticMarkerScan->GetBoolField(TEXT("passed"));
    const bool bVerificationPass = bDryRun || (
        bPreflightPass
        && bAssetMutationPerformed
        && bReplacementSystemMatchesAfter
        && bBlueprintCompilePass
        && bBlueprintWasSaved
        && bStaticMarkerScanPass);

    if(!ContinueRead())return ErrorResponse(W.R.Error);
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), bVerificationPass);
    ResultObj->SetStringField(TEXT("operation"), Operation);
    ResultObj->SetBoolField(TEXT("dry_run"), bDryRun);
    ResultObj->SetBoolField(TEXT("preflight_pass"), bPreflightPass);
    ResultObj->SetBoolField(TEXT("verification_pass"), bVerificationPass);
    ResultObj->SetArrayField(TEXT("preflight_failure_reasons"), StringArrayToJsonArray(PreflightFailureReasons));
    ResultObj->SetStringField(TEXT("blueprint_path"), BlueprintPathParam);
    ResultObj->SetStringField(TEXT("blueprint_object_path"), BlueprintObjectPath);
    ResultObj->SetStringField(TEXT("blueprint_package_name"), BlueprintPackageName);
    ResultObj->SetStringField(TEXT("component_name"), ComponentNameFilter);
    ResultObj->SetStringField(TEXT("source_system_path"), SourceSystemObjectPath);
    ResultObj->SetStringField(TEXT("replacement_system_path"), ReplacementSystemObjectPath);
    ResultObj->SetBoolField(TEXT("source_system_asset_exists"), SourceSystem != nullptr);
    ResultObj->SetBoolField(TEXT("replacement_system_asset_exists"), ReplacementSystem != nullptr);
    ResultObj->SetBoolField(TEXT("preserve_override_parameters"), bPreserveOverrideParameters);
    ResultObj->SetBoolField(TEXT("include_inherited_scs_templates"), false);
    ResultObj->SetNumberField(TEXT("scs_owner_class_count"), GeneratedClass ? 1 : 0);
    ResultObj->SetNumberField(TEXT("scs_node_count_scanned"), ScannedScsNodeCount);
    ResultObj->SetNumberField(TEXT("niagara_component_template_count_scanned"), ScannedNiagaraTemplateCount);
    ResultObj->SetNumberField(TEXT("matching_component_template_count"), MatchingComponentTemplateCount);
    ResultObj->SetNumberField(TEXT("source_system_match_count"), SourceSystemMatchCount);
    ResultObj->SetNumberField(TEXT("component_template_count"), ReturnedComponentTemplateCount);
    ResultObj->SetNumberField(TEXT("max_components"), MaxComponents);
    ResultObj->SetBoolField(TEXT("truncated"), bTruncated);
    ResultObj->SetArrayField(TEXT("components"), ComponentResults);
    ResultObj->SetBoolField(TEXT("read_only"), bDryRun);
    ResultObj->SetBoolField(TEXT("loads_blueprint_asset"), true);
    ResultObj->SetBoolField(TEXT("loads_source_system_asset"), true);
    ResultObj->SetBoolField(TEXT("loads_replacement_system_asset"), true);
    ResultObj->SetBoolField(TEXT("saves_or_reloads_packages"), !bDryRun);
    ResultObj->SetBoolField(TEXT("blueprint_package_loaded_before"), bBlueprintPackageLoadedBefore);
    ResultObj->SetBoolField(TEXT("blueprint_package_dirty_before"), bBlueprintPackageDirtyBefore);
    ResultObj->SetBoolField(TEXT("blueprint_package_dirty_after"), bBlueprintPackageDirtyAfter);
    ResultObj->SetBoolField(TEXT("blueprint_package_dirty_added_by_command"), bBlueprintPackageDirtyAddedByCommand);
    ResultObj->SetBoolField(TEXT("blueprint_was_saved"), bBlueprintWasSaved);
    ResultObj->SetBoolField(TEXT("blueprint_was_compiled"), bBlueprintWasCompiled);
    ResultObj->SetObjectField(TEXT("compile_result"), CompileResult);
    ResultObj->SetObjectField(TEXT("save_result"), SaveResult);
    if (StaticMarkerScan.IsValid())
    {
        ResultObj->SetObjectField(TEXT("static_marker_scan"), StaticMarkerScan);
    }
    ResultObj->SetBoolField(TEXT("post_save_static_marker_scan_passed"), bStaticMarkerScanPass);
    ResultObj->SetBoolField(TEXT("asset_mutation_admitted"), !bDryRun && bPreflightPass);
    ResultObj->SetBoolField(TEXT("asset_mutation_performed"), bAssetMutationPerformed);
    ResultObj->SetBoolField(TEXT("replacement_system_matches_after"), bReplacementSystemMatchesAfter);
    ResultObj->SetBoolField(TEXT("blueprint_asset_write_admitted"), !bDryRun && bPreflightPass);
    ResultObj->SetBoolField(TEXT("durable_mutation_admitted"), !bDryRun && bPreflightPass);
    ResultObj->SetBoolField(TEXT("persistent_original_write_admitted"), !bDryRun && bPreflightPass);
    ResultObj->SetBoolField(TEXT("actual_bp_asset_write_tool_exists"), true);
    ResultObj->SetBoolField(TEXT("default_dry_run"), true);
    ResultObj->SetStringField(TEXT("confirmation_token"), ConfirmationToken);
    ResultObj->SetStringField(TEXT("safety_note"), TEXT("Durable executor defaults to dry_run=true. Real execution requires an explicit durable token, clean Blueprint package, exactly one local SCS UNiagaraComponent source match, compile_blueprint=true, save_blueprint=true, and a post-save static UnrealMCP marker scan."));
    AddDirtyPackageCommandDelta(ResultObj, DirtyPackagesBeforeCommand);
    return ResultObj;
}

TSharedPtr<FJsonObject> FinalTargetProjection(FWork& W){auto J=MakeShared<FJsonObject>();if(!W.Admit(TEXT("final_target_projection")))return J;auto* B=W.Target.Get();auto* T=W.SelectedTemplate.Get();if(!B||!T)return J;if(!W.Blueprint(B)||!W.Pin(T))return J;J->SetStringField(TEXT("blueprint"),B->GetPathName());J->SetStringField(TEXT("template"),T->GetPathName());J->SetStringField(TEXT("asset"),T->GetAsset()?T->GetAsset()->GetPathName():FString());J->SetStringField(TEXT("SCS_variable"),W.SelectedVariable);J->SetStringField(TEXT("blueprint_status"),BlueprintStatus(B));J->SetBoolField(TEXT("blueprint_being_compiled"),B->bBeingCompiled);J->SetBoolField(TEXT("blueprint_queued_for_compilation"),B->bQueuedForCompilation);J->SetBoolField(TEXT("package_dirty"),B->GetPackage()->IsDirty());J->SetBoolField(TEXT("requested_replacement_exact"),T->GetAsset()==W.Replacement.Get());AddNiagaraRuntimeStateStore(J,TEXT("override_parameters"),T->GetOverrideParameters(),FString(),256,true,false,64,false,0,0,0);if(!W.Admit(TEXT("after_final_target_projection")))return J;J->SetBoolField(TEXT("projection_complete"),true);return J;}
FString Finish(FWork& W,const TSharedPtr<FJsonObject>& Legacy,const TCHAR* Operation,bool DryRun){bool Success=false;if(Legacy)Legacy->TryGetBoolField(TEXT("success"),Success);if(!Success&&W.R.Error.IsEmpty()){FString E;if(Legacy)Legacy->TryGetStringField(TEXT("error"),E);W.Fail(TEXT("blueprint_niagara_template_operation_failed"),E.IsEmpty()?TEXT("Legacy verification failed; actual mutation/compile/save prefix retained"):E);}bool Complete=Success&&W.R.Error.IsEmpty();if(Complete&&W.Writer&&!DryRun){bool Preflight=false,Mutated=false,Match=false;Legacy->TryGetBoolField(TEXT("preflight_pass"),Preflight);Legacy->TryGetBoolField(TEXT("asset_mutation_performed"),Mutated);Legacy->TryGetBoolField(TEXT("replacement_system_matches_after"),Match);if(!Preflight||!Mutated||!Match){Complete=false;W.Fail(TEXT("blueprint_niagara_template_write_postcondition_failed"),TEXT("Legacy success envelope does not establish actual template mutation/assignment"));}else{auto Final=FinalTargetProjection(W);W.R.Data->SetObjectField(TEXT("after"),Final);bool ProjectionComplete=false;Final->TryGetBoolField(TEXT("projection_complete"),ProjectionComplete);Complete=ProjectionComplete&&W.R.Error.IsEmpty()&&W.Admit(TEXT("final_exact_scoped_admission"));}}else if(Complete)Complete=W.Admit(TEXT("final_observer_admission"));W.R.bSuccess=Complete;W.R.bPostconditionPassed=Complete;if(Complete){W.R.bChangeStateKnown=true;if(!W.R.bMutationAttempted)W.R.bChanged=false;}else if(W.R.bMutationAttempted){W.R.bChangeStateKnown=false;W.R.bChanged=false;}W.R.Data->SetObjectField(TEXT("legacy_result"),Legacy?Legacy:MakeShared<FJsonObject>());W.R.Data->SetStringField(TEXT("operation"),Operation);W.R.Data->SetBoolField(TEXT("operation_complete"),true);W.R.Data->SetObjectField(TEXT("native_call_counts"),W.Counts);W.R.Data->SetArrayField(TEXT("actual_call_prefix"),W.Calls);W.R.Data->SetArrayField(TEXT("actual_creation_rows"),W.CreationRows);W.R.Data->SetArrayField(TEXT("actual_save_rows"),W.SaveRows);W.R.Data->SetArrayField(TEXT("owned_compile_role_retirements"),W.RetiredRows);W.R.Data->SetStringField(TEXT("requested_target_object_path"),W.RequestedObject);W.R.Data->SetStringField(TEXT("requested_target_package"),W.RequestedPackage);W.R.Data->SetStringField(TEXT("requested_target_filename"),W.RequestedFilename);W.R.Data->SetBoolField(TEXT("post_failure_live_projection_skipped"),!W.R.Error.IsEmpty());W.R.Data->SetBoolField(TEXT("automatic_cleanup_performed"),false);W.R.Data->SetBoolField(TEXT("residual_target_possible"),(!W.CreationRows.IsEmpty()||W.R.Data->HasField(TEXT("existing_target_replace_attempted")))&&!Complete);if(!W.R.Data->HasField(TEXT("existing_target_force_replace_supported")))W.R.Data->SetBoolField(TEXT("existing_target_force_replace_supported"),false);W.R.Data->SetBoolField(TEXT("atomic_save_or_rollback_verified"),false);W.R.Data->SetBoolField(TEXT("full_override_byte_reference_preservation_verified"),false);W.R.Data->SetBoolField(TEXT("source_RAM_equality_verified"),false);W.R.Data->SetBoolField(TEXT("transitive_packaged_noMCP_verified"),false);W.R.Data->SetStringField(TEXT("scope"),TEXT("Public stock Blueprint/SCS/UNiagaraComponent only. Local owned writer; inherited template read only. Blueprint CompileSynchronously return plus current status/idle is distinct from Niagara VM/GPU readiness. Internal SDK callback/synchronous save cost and full source RAM equality are not atomic guarantees. Only explicitly authorized clean isolated canary replacement deletes its selected old generation; no failure cleanup/restore/replay. DeleteAsset may reset Undo/unload/GC; these global effects are unverified. No private API, runtime Cubeless class, wrapper wait/flush/pump."));W.R.Stage=W.R.Error.IsEmpty()?TEXT("complete"):W.FailedStage;return Operations::EncodeVariableResult(W.R,Operation);}
using FHandler=TSharedPtr<FJsonObject>(*)(const TSharedPtr<FJsonObject>&);
FString Run(const FString& Text,const TCHAR* Operation,FHandler Handler,bool CanWrite){TSharedPtr<FJsonObject> Params;FWork W(false);if(!Parse(Text,Params)){W.Fail(TEXT("blueprint_niagara_template_invalid_params_json"),TEXT("Bounded finite JSON object without duplicate keys or embedded NUL required"));return Finish(W,ErrorResponse(W.R.Error),Operation,true);}const bool DryRun=!CanWrite||GetBoolParam(Params,TEXT("dry_run"),true);W.Writer=CanWrite&&!DryRun;if(CanWrite){W.R.bCompileRequested=GetBoolParam(Params,TEXT("compile_blueprint"),false);W.R.bSaveRequested=GetBoolParam(Params,TEXT("save_blueprint"),false);}W.R.Data->SetObjectField(TEXT("requested_params"),Params);if(!IsInGameThread()){W.Fail(TEXT("blueprint_niagara_template_game_thread_required"),TEXT("Editor game thread required"));return Finish(W,ErrorResponse(W.R.Error),Operation,DryRun);}if(Running){W.Fail(TEXT("blueprint_niagara_template_reentrant_request"),TEXT("Active dedicated request; no reentrant write or replay"));return Finish(W,ErrorResponse(W.R.Error),Operation,DryRun);}TGuardValue<bool> Lease(Running,true);TGuardValue<FWork*> Context(Current,&W);if(!W.Admit(TEXT("preflight")))return Finish(W,ErrorResponse(W.R.Error),Operation,DryRun);TUniquePtr<FCanaryReplacement> CanaryLease;if(Handler==&HandleCanary){CanaryLease=MakeUnique<FCanaryReplacement>(W);W.ReplacementLease=CanaryLease.Get();}return Finish(W,Handler(Params),Operation,DryRun);}
}
FString InspectTemplate(const FString& ParamsJson){return Run(ParamsJson,TEXT("inspect_blueprint_niagara_component_template"),&HandleInspect,false);}
FString DryRunSwap(const FString& ParamsJson){return Run(ParamsJson,TEXT("dry_run_blueprint_niagara_component_template_asset_swap"),&HandleDryRun,false);}
FString ExecuteCanary(const FString& ParamsJson){return Run(ParamsJson,TEXT("execute_blueprint_niagara_component_template_asset_swap_canary"),&HandleCanary,true);}
FString ExecuteDurable(const FString& ParamsJson){return Run(ParamsJson,TEXT("execute_blueprint_niagara_component_template_asset_swap_durable"),&HandleDurable,true);}
}
