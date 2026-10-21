// HL: original CDO single-property authoring and shared Blueprint graph metadata.
#include "Compatibility/UE58/CubelessBlueprintPropertyGraphApi.h"
#include "Compatibility/UE58/CubelessBlueprintTopologyApi.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/World.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
namespace Cubeless::UE58::BlueprintPropertyGraph
{
namespace
{
using Variable::FResult;
bool Running=false;
bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
bool GlobalSafe(){return IsInGameThread()&&GEditor&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage();}
bool TextSafe(const FString& S,int32 Bound){if(S.Len()>Bound)return false;for(int32 I=0;I<S.Len();++I)if(S[I]==0)return false;return true;}
FString Json(const TSharedPtr<FJsonObject>& O){FString S;if(O)FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&S));return S;}
bool Finite(const TSharedPtr<FJsonValue>& V,int32 D,int32& Count)
{if(!V.IsValid()||D>64||++Count>262144)return false;if(V->Type==EJson::Number)return FMath::IsFinite(V->AsNumber());if(V->Type==EJson::String)return TextSafe(V->AsString(),1048576);if(V->Type==EJson::Array){for(const auto& X:V->AsArray())if(!Finite(X,D+1,Count))return false;}else if(V->Type==EJson::Object){for(const auto& X:V->AsObject()->Values)if(!TextSafe(FString(X.Key),1048576)||!Finite(X.Value,D+1,Count))return false;}return V->Type!=EJson::None;}
bool Parse(const FString& S,TSharedPtr<FJsonObject>& Out)
{
    if(S.IsEmpty()||!TextSafe(S,1048576))return false;int32 Depth=0;bool In=false,Esc=false;for(TCHAR C:S){if(In){if(Esc)Esc=false;else if(C=='\\')Esc=true;else if(C=='"')In=false;}else if(C=='"')In=true;else if(C=='{'||C=='['){if(++Depth>64)return false;}else if(C=='}'||C==']'){if(--Depth<0)return false;}}if(In||Depth)return false;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S),Out)||!Out.IsValid())return false;int32 Count=0;return Finite(MakeShared<FJsonValueObject>(Out),0,Count);
}
// Applied only to typed object/class paths and actual loaded references, never plain FString payloads.
bool ForbiddenReference(const FString& S)
{FString P=FPackageName::ExportTextPathToObjectPath(S).TrimStartAndEnd();P.TrimQuotesInline();for(const TCHAR* K:{TEXT("UnrealMCP"),TEXT("MCPUnreal"),TEXT("mcp_unreal"),TEXT("CubelessControlRigTools"),TEXT("CubelessMCP"),TEXT("cubeless_mcp"),TEXT("ModelContextProtocol"),TEXT("ToolsetRegistry"),TEXT("/Engine/PythonTypes")})if(P.Contains(K,ESearchCase::IgnoreCase))return true;return false;}
struct FIdentity
{
    TStrongObjectPtr<UObject> Hold;TWeakObjectPtr<UObject> Outer;TWeakObjectPtr<UClass> Class;TWeakObjectPtr<UPackage> Package,Effective;FString Path;
    explicit FIdentity(UObject* O):Hold(O),Outer(O->GetOuter()),Class(O->GetClass()),Package(O->GetOutermost()),Effective(O->GetPackage()),Path(O->GetPathName()){}
    bool Exact()const{auto* O=Hold.Get();return Live(O)&&Live(Class.Get())&&!Class->HasAnyClassFlags(CLASS_NewerVersionExists)&&O->GetClass()==Class.Get()&&O->GetOuter()==Outer.Get()&&O->GetOutermost()==Package.Get()&&O->GetPackage()==Effective.Get()&&O->GetPathName().Equals(Path,ESearchCase::CaseSensitive);}
};
struct FWork
{
    FResult R;TWeakObjectPtr<UEditorEngine> Editor;TWeakObjectPtr<UWorld> World;TWeakObjectPtr<UBlueprint> BP;TWeakObjectPtr<UClass> Generated;TWeakObjectPtr<UObject> CDO;TArray<TUniquePtr<FIdentity>> Objects;bool ContextCaptured=false;
    TSharedRef<FJsonObject> Prefix=MakeShared<FJsonObject>(),Counts=MakeShared<FJsonObject>();TSharedPtr<FJsonObject> Legacy;FProperty* ExpectedProperty=nullptr;FFieldClass* PropertyClass=nullptr;TWeakObjectPtr<UClass> RequiredObjectClass,RequiredMetaClass;TWeakObjectPtr<UEnum> RequiredEnum;FString Name,PropertyType;int32 ArrayDim=0,Size=0;
    explicit FWork(){R.Compiled=false;R.Data->SetObjectField(TEXT("actual_mutation_prefix"),Prefix);R.Data->SetObjectField(TEXT("writer_call_counts"),Counts);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),false);R.Data->SetArrayField(TEXT("actual_compile_prefix"),{});R.Data->SetArrayField(TEXT("actual_save_prefix"),{});}
    bool Fail(const TCHAR* Code,const FString& Message){if(R.Error.IsEmpty()){R.ErrorCode=Code;R.Error=Message;}R.bSuccess=R.bPostconditionPassed=false;return false;}
    void Count(const TCHAR* K){double N=0;Counts->TryGetNumberField(K,N);Counts->SetNumberField(K,N+1);}
    bool Guard(const FString& Stage)
    {
        R.Stage=Stage;if(!R.Error.IsEmpty())return false;if(!GlobalSafe())return Fail(TEXT("blueprint_property_context_unavailable"),TEXT("Safe Editor GT outside GC/exit/save required; no further writer/readback/retry."));
        auto* CW=GEditor->GetEditorWorldContext().World();if(!ContextCaptured){Editor=GEditor;World=CW;ContextCaptured=true;}if(Editor.Get()!=GEditor||!Live(CW)||World.Get()!=CW||CW->bIsTearingDown)return Fail(TEXT("blueprint_property_context_changed"),TEXT("Captured Editor/world changed."));
        for(const auto& O:Objects)if(!O->Exact())return Fail(TEXT("blueprint_property_identity_changed"),TEXT("Captured class/outer/effective package/path changed; no replacement adopted."));
        if(auto* B=BP.Get()){if(B->GeneratedClass!=Generated.Get())return Fail(TEXT("blueprint_property_generated_class_changed"),TEXT("Current GeneratedClass differs across callback."));}
        if(auto* C=Generated.Get()){auto* B=BP.Get();if(!Live(B)||C->ClassGeneratedBy!=B||C->GetOutermost()!=B->GetOutermost()||C->GetPackage()!=B->GetPackage()||C->GetOuter()!=B->GetOutermost())return Fail(TEXT("blueprint_property_class_owner_changed"),TEXT("Current generated class is not the exact selected Blueprint package output."));if(C->HasAnyClassFlags(CLASS_NewerVersionExists))return Fail(TEXT("blueprint_property_class_retired"),TEXT("Captured generated class is retired."));if(CDO.IsValid()&&(C->GetDefaultObject(false)!=CDO.Get()||CDO->GetClass()!=C||!CDO->HasAnyFlags(RF_ClassDefaultObject)||CDO->GetOuter()!=C->GetOuter()||CDO->GetOutermost()!=C->GetOutermost()||CDO->GetPackage()!=C->GetPackage()))return Fail(TEXT("blueprint_property_cdo_changed"),TEXT("Current generated CDO differs across callback."));}
        return true;
    }
    bool Pin(UObject* O){if(!Guard(TEXT("pin_identity"))||!Live(O))return Fail(TEXT("blueprint_property_object_unavailable"),TEXT("Expected live loaded object."));for(const auto& X:Objects)if(X->Hold.Get()==O)return true;if(Objects.Num()>=1024)return Fail(TEXT("blueprint_property_identity_bound"),TEXT("Bounded target/reference identity scope exceeded."));Objects.Add(MakeUnique<FIdentity>(O));return true;}
    bool SDK(const TCHAR* Stage,bool Mutation=false){if(!Guard(Stage))return false;Count(Stage);R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);R.bMutationAttempted|=Mutation;if(Mutation){Prefix->SetStringField(TEXT("last_stage"),Stage);Prefix->SetBoolField(FString(Stage)+TEXT("_attempted"),true);}return true;}
    bool After(const TCHAR* Stage){Prefix->SetBoolField(FString(Stage)+TEXT("_SDK_returned"),true);return Guard(FString(TEXT("after_"))+Stage);}
    FProperty* CurrentProperty()
    {if(!Guard(TEXT("refind_property")))return nullptr;auto* C=Generated.Get();auto* P=C?C->FindPropertyByName(FName(*Name)):nullptr;if(!P||P!=ExpectedProperty||P->GetClass()!=PropertyClass||P->GetCPPType()!=PropertyType||P->ArrayDim!=ArrayDim||P->GetSize()!=Size){Fail(TEXT("blueprint_property_field_changed"),TEXT("Exact inherited property identity/type/layout changed; old FProperty not dereferenced."));return nullptr;}if(auto* HLTypedField66_0=CastField<FObjectPropertyBase>(P)){if(HLTypedField66_0->PropertyClass!=RequiredObjectClass.Get()){Fail(TEXT("blueprint_property_reference_type_changed"),TEXT("Declared object class changed."));return nullptr;}if(auto* CP=CastField<FClassProperty>(P))if(CP->MetaClass!=RequiredMetaClass.Get()){Fail(TEXT("blueprint_property_reference_type_changed"),TEXT("Declared class metaclass changed."));return nullptr;}}UEnum* Enum=nullptr;if(auto* HLTypedField66_1=CastField<FByteProperty>(P))Enum=HLTypedField66_1->GetIntPropertyEnum();else if(auto* HLTypedField66_2=CastField<FEnumProperty>(P))Enum=HLTypedField66_2->GetEnum();if(Enum!=RequiredEnum.Get()){Fail(TEXT("blueprint_property_enum_type_changed"),TEXT("Declared enum identity changed."));return nullptr;}return P;}
    FString Encode(const TCHAR* Op)
    {
        if(!Legacy){Legacy=MakeShared<FJsonObject>();Legacy->SetBoolField(TEXT("success"),false);Legacy->SetStringField(TEXT("error"),R.Error.IsEmpty()?TEXT("Blueprint property operation failed"):R.Error);}R.Data->SetObjectField(TEXT("legacy_result"),Legacy);R.Data->SetStringField(TEXT("operation"),Op);R.Data->SetBoolField(TEXT("operation_complete"),true);R.Data->SetBoolField(TEXT("scoped_target_change_state_known"),R.bSuccess&&R.bPostconditionPassed&&R.bChangeStateKnown);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);if(!R.bSuccess&&R.bMutationAttempted){R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("do_not_retry"),true);}return Operations::EncodeVariableResult(R,Op);
    }
};
bool AuditReference(FWork& W,UObject* Object)
{
    if(!Object)return true;if(!W.Pin(Object))return false;TArray<UClass*> Chain;UClass* C=Cast<UClass>(Object);if(!C)C=Object->GetClass();for(int32 I=0;C;C=C->GetSuperClass(),++I){if(I>=256||Chain.Contains(C)||!W.Pin(C))return W.Fail(TEXT("blueprint_property_reference_class_scope"),TEXT("Reference class chain is invalid/bounded."));Chain.Add(C);if(ForbiddenReference(C->GetPathName())||C->GetOutermost()->HasAnyPackageFlags(PKG_EditorOnly))return W.Fail(TEXT("blueprint_property_runtime_reference_forbidden"),TEXT("Actual referenced class is an MCP/Editor-only dependency."));}
    if(ForbiddenReference(Object->GetPathName())||Object->GetOutermost()->HasAnyPackageFlags(PKG_EditorOnly))return W.Fail(TEXT("blueprint_property_runtime_reference_forbidden"),TEXT("Actual referenced object is an MCP/Editor-only dependency."));
    if(!W.SDK(TEXT("Reference_IsEditorOnly")))return false;const bool EditorOnly=Object->IsEditorOnly();if(!W.After(TEXT("Reference_IsEditorOnly")))return false;return !EditorOnly||W.Fail(TEXT("blueprint_property_runtime_reference_forbidden"),TEXT("Actual referenced object reports Editor-only."));
}
struct FValueBuffer
{
    FProperty* Property;TArray<uint8,TAlignedHeapAllocator<16>> Bytes;
    explicit FValueBuffer(FProperty* P):Property(P){Bytes.SetNumUninitialized(P->GetSize());P->InitializeValue(Bytes.GetData());}
    ~FValueBuffer(){Property->DestroyValue(Bytes.GetData());}
};
FString Canonical(FProperty* P,const void* Address,UObject* Owner)
{auto O=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Rows;const int32 Stride=P->GetSize()/P->ArrayDim;int64 Total=0;for(int32 I=0;I<P->ArrayDim;++I){FString S;P->ExportTextItem_Direct(S,static_cast<const uint8*>(Address)+I*Stride,nullptr,Owner,PPF_None);Total+=S.Len();if(Total>32ll*1024*1024)return {};Rows.Add(MakeShared<FJsonValueString>(S));}O->SetArrayField(TEXT("values"),Rows);return Json(O);}
bool SameValue(FProperty* P,const void* A,const void* B)
{const int32 Stride=P->GetSize()/P->ArrayDim;for(int32 I=0;I<P->ArrayDim;++I)if(!P->Identical(static_cast<const uint8*>(A)+I*Stride,static_cast<const uint8*>(B)+I*Stride,0))return false;return true;}
}
FString SetProperty(const FString& ParamsJson)
{
    constexpr const TCHAR* Operation=TEXT("set_blueprint_property");FWork W;TSharedPtr<FJsonObject> Params;
    if(!GlobalSafe()||Running){W.Fail(TEXT("blueprint_property_context_unavailable"),TEXT("Safe nonreentrant Editor context required."));return W.Encode(Operation);}TGuardValue<bool> Lease(Running,true);
    if(!Parse(ParamsJson,Params)){W.Fail(TEXT("blueprint_property_invalid_params_json"),TEXT("Expected finite bounded JSON object."));return W.Encode(Operation);}
    FString BPName;if(!Params->TryGetStringField(TEXT("blueprint_name"),BPName)){W.Fail(TEXT("blueprint_property_missing_name"),TEXT("Missing 'blueprint_name' parameter"));return W.Encode(Operation);}if(!Params->TryGetStringField(TEXT("property_name"),W.Name)){W.Fail(TEXT("blueprint_property_missing_property"),TEXT("Missing 'property_name' parameter"));return W.Encode(Operation);}
    auto Value=Params->TryGetField(TEXT("property_value"));if(!TextSafe(BPName,NAME_SIZE-1)||!TextSafe(W.Name,NAME_SIZE-1)){W.Fail(TEXT("blueprint_property_name_bound"),TEXT("Raw names must fit NAME_SIZE without embedded NUL."));return W.Encode(Operation);}
    if(!W.SDK(TEXT("ResolveBlueprint")))return W.Encode(Operation);UBlueprint* B=BlueprintTopology::ResolveBlueprintForMetadata(BPName,W.R);W.R.Data->SetBoolField(TEXT("read_only_direct_API"),false);W.Prefix->SetBoolField(TEXT("load_returned_nonnull"),B!=nullptr);if(!W.After(TEXT("ResolveBlueprint"))||!W.Pin(B))return W.Encode(Operation);
    UClass* C=B->GeneratedClass;W.Generated=C;W.BP=B;if(!W.Pin(C))return W.Encode(Operation);
    if(!W.SDK(TEXT("GetDefaultObject")))return W.Encode(Operation);UObject* O=C->GetDefaultObject();W.Prefix->SetBoolField(TEXT("cdo_returned_nonnull"),O!=nullptr);if(!W.After(TEXT("GetDefaultObject"))||!W.Pin(O))return W.Encode(Operation);if(O->GetClass()!=C||!O->HasAnyFlags(RF_ClassDefaultObject)||O->GetOuter()!=C->GetOuter()||O->GetOutermost()!=C->GetOutermost()||O->GetPackage()!=C->GetPackage()||C->GetDefaultObject(false)!=O){W.Fail(TEXT("blueprint_property_cdo_identity"),TEXT("Returned CDO is not the exact current generated class default."));return W.Encode(Operation);}W.CDO=O;
    if(!Value.IsValid()){W.Fail(TEXT("blueprint_property_missing_value"),TEXT("Missing 'property_value' parameter"));return W.Encode(Operation);}
    FProperty* P=C->FindPropertyByName(FName(*W.Name));if(!P){W.Fail(TEXT("blueprint_property_not_found"),FString::Printf(TEXT("Property not found: %s"),*W.Name));return W.Encode(Operation);}if(!W.Pin(P->GetOwnerStruct()))return W.Encode(Operation);W.ExpectedProperty=P;W.PropertyClass=P->GetClass();W.PropertyType=P->GetCPPType();W.ArrayDim=P->ArrayDim;W.Size=P->GetSize();if(W.ArrayDim<1||W.ArrayDim>1024||W.Size<1||W.Size>65536){W.Fail(TEXT("blueprint_property_layout_bound"),TEXT("Property layout exceeds supported single-value capture bound."));return W.Encode(Operation);}
    const bool Supported=CastField<FBoolProperty>(P)||CastField<FIntProperty>(P)||CastField<FFloatProperty>(P)||CastField<FStrProperty>(P)||CastField<FObjectPropertyBase>(P)||CastField<FByteProperty>(P)||CastField<FEnumProperty>(P)||(CastField<FStructProperty>(P)&&CastField<FStructProperty>(P)->Struct==TBaseStructure<FLinearColor>::Get());if(!Supported){W.Fail(TEXT("blueprint_property_type_unsupported"),FString::Printf(TEXT("Unsupported property type: %s for property %s"),*P->GetClass()->GetName(),*W.Name));return W.Encode(Operation);}
    if(auto* HLTypedField102_0=CastField<FObjectPropertyBase>(P)){W.RequiredObjectClass=HLTypedField102_0->PropertyClass;if(HLTypedField102_0->PropertyClass&&!W.Pin(HLTypedField102_0->PropertyClass))return W.Encode(Operation);if(auto* CP=CastField<FClassProperty>(P)){W.RequiredMetaClass=CP->MetaClass;if(CP->MetaClass&&!W.Pin(CP->MetaClass))return W.Encode(Operation);}}if(auto* HLTypedField102_1=CastField<FByteProperty>(P))W.RequiredEnum=HLTypedField102_1->GetIntPropertyEnum();else if(auto* HLTypedField102_2=CastField<FEnumProperty>(P))W.RequiredEnum=HLTypedField102_2->GetEnum();if(W.RequiredEnum.IsValid()&&!W.Pin(W.RequiredEnum.Get()))return W.Encode(Operation);
    // Resolve actual object/class values before allocating scratch storage or taking any final field pointer.
    UObject* RequestedObject=nullptr;
    if(auto* ObjectProp=CastField<FObjectPropertyBase>(P))
    {
        FString Path;if(!Value->TryGetString(Path)){W.Fail(TEXT("blueprint_property_reference_requires_string"),CastField<FClassProperty>(P)?TEXT("Class property requires a class path string"):TEXT("Object property requires an object path string"));return W.Encode(Operation);}FString Expanded=FPackageName::ExportTextPathToObjectPath(Path).TrimStartAndEnd();Expanded.TrimQuotesInline();if(Expanded.StartsWith(TEXT("/"))&&!Expanded.Contains(TEXT(".")))Expanded+=TEXT(".")+FPackageName::GetShortName(Expanded);if(!TextSafe(Path,NAME_SIZE-1)||!TextSafe(Expanded,NAME_SIZE-1)||ForbiddenReference(Path)){W.Fail(TEXT("blueprint_property_runtime_reference_forbidden"),TEXT("Referenced path exceeds NAME_SIZE or introduces an MCP/Editor dependency."));return W.Encode(Operation);}
        UClass* RequiredClass=CastField<FClassProperty>(P)?UClass::StaticClass():ObjectProp->PropertyClass.Get();if(!W.Pin(RequiredClass))return W.Encode(Operation);
        if(!Path.IsEmpty()){if(!W.SDK(TEXT("StaticLoadObject_Reference")))return W.Encode(Operation);RequestedObject=StaticLoadObject(RequiredClass,nullptr,*Path);W.Prefix->SetBoolField(TEXT("reference_load_returned_nonnull"),RequestedObject!=nullptr);if(!W.After(TEXT("StaticLoadObject_Reference"))||!RequestedObject){if(W.R.Error.IsEmpty())W.Fail(TEXT("blueprint_property_reference_load_failed"),TEXT("Could not load the requested class/object path."));return W.Encode(Operation);}if(!AuditReference(W,RequestedObject))return W.Encode(Operation);}
        P=W.CurrentProperty();if(!P)return W.Encode(Operation);ObjectProp=CastField<FObjectPropertyBase>(P);if(auto* CP=CastField<FClassProperty>(P)){if(RequestedObject&&(!Cast<UClass>(RequestedObject)||(CP->MetaClass&&!Cast<UClass>(RequestedObject)->IsChildOf(CP->MetaClass)))){W.Fail(TEXT("blueprint_property_reference_type_mismatch"),TEXT("Class is not a child of the property's required metaclass."));return W.Encode(Operation);}}
        else if(RequestedObject&&!RequestedObject->IsA(ObjectProp->PropertyClass)){W.Fail(TEXT("blueprint_property_reference_type_mismatch"),TEXT("Object is not of the property's required class."));return W.Encode(Operation);}
        const int32 Stride=P->GetSize()/P->ArrayDim;for(int32 I=0;I<P->ArrayDim;++I){auto* Old=ObjectProp->GetObjectPropertyValue(static_cast<uint8*>(P->ContainerPtrToValuePtr<void>(O))+I*Stride);if(Old&&!W.Pin(Old))return W.Encode(Operation);}
    }
    const bool DirtyBefore=B->GetOutermost()->IsDirty(),CachedBefore=B->bCachedDependenciesUpToDate;const EBlueprintStatus StatusBefore=B->Status;
    FString ExpectedCanonical;
    { // No FProperty-backed scratch value survives the MarkBlueprintAsModified callback.
    P=W.CurrentProperty();if(!P)return W.Encode(Operation);FValueBuffer Before(P),Requested(P);const void* Actual=P->ContainerPtrToValuePtr<void>(O);P->CopyCompleteValue(Before.Bytes.GetData(),Actual);P->CopyCompleteValue(Requested.Bytes.GetData(),Actual);
    auto FailValue=[&](const TCHAR* Message){W.Fail(TEXT("blueprint_property_value_invalid"),Message);};void* V=Requested.Bytes.GetData();bool Prepared=true;
    if(auto* HLTypedField119_0=CastField<FBoolProperty>(P))HLTypedField119_0->SetPropertyValue(V,Value->AsBool());
    else if(auto* HLTypedField120_0=CastField<FIntProperty>(P)){double N=Value->AsNumber();if(!FMath::IsFinite(N)||N<MIN_int32||N>MAX_int32){FailValue(TEXT("int32 numeric value out of safe range"));Prepared=false;}else HLTypedField120_0->SetPropertyValue(V,static_cast<int32>(N));}
    else if(auto* HLTypedField121_0=CastField<FFloatProperty>(P)){double N=Value->AsNumber();if(!FMath::IsFinite(N)||FMath::Abs(N)>MAX_flt){FailValue(TEXT("float32 numeric value out of safe range"));Prepared=false;}else HLTypedField121_0->SetPropertyValue(V,static_cast<float>(N));}
    else if(auto* HLTypedField122_0=CastField<FStrProperty>(P))HLTypedField122_0->SetPropertyValue(V,Value->AsString());
    else if(auto* HLTypedField123_0=CastField<FObjectPropertyBase>(P))HLTypedField123_0->SetObjectPropertyValue(V,RequestedObject);
    else if(auto* HLTypedField124_0=CastField<FStructProperty>(P)){const TArray<TSharedPtr<FJsonValue>>* A=nullptr;if(!Value->TryGetArray(A)||!A||A->Num()!=4){FailValue(TEXT("LinearColor requires an array of 4 values"));Prepared=false;}else{double N[4];for(int32 I=0;I<4;++I){N[I]=(*A)[I]->AsNumber();if(!FMath::IsFinite(N[I])||FMath::Abs(N[I])>MAX_flt)Prepared=false;}if(!Prepared)FailValue(TEXT("LinearColor component exceeds safe float32 range"));else{FLinearColor Color(static_cast<float>(N[0]),static_cast<float>(N[1]),static_cast<float>(N[2]),static_cast<float>(N[3]));HLTypedField124_0->CopySingleValue(V,&Color);}}}
    else
    {
        FNumericProperty* Numeric=nullptr;UEnum* Enum=nullptr;bool Byte=false;if(auto* HLTypedField127_0=CastField<FByteProperty>(P)){Numeric=HLTypedField127_0;Enum=HLTypedField127_0->GetIntPropertyEnum();Byte=true;}else if(auto* HLTypedField127_1=CastField<FEnumProperty>(P)){Enum=HLTypedField127_1->GetEnum();Numeric=HLTypedField127_1->GetUnderlyingProperty();}
        int64 N=0;bool Got=false;if(Numeric){if(Value->Type==EJson::Number||(!Enum&&Byte)){double Number=Value->AsNumber();if(FMath::IsFinite(Number)&&(Byte?(Number>=0&&Number<=255):(Number>=-9223372036854775808.0&&Number<9223372036854775808.0))){N=static_cast<int64>(Number);Got=true;}}
        else if(Enum&&Value->Type==EJson::String){FString Text=Value->AsString();if(Text.IsNumeric()){if(Byte){N=static_cast<uint8>(FCString::Atoi(*Text));Got=true;}else{N=FCString::Atoi64(*Text);Got=true;}}else{FString Short=Text;if(Short.Contains(TEXT("::")))Short.Split(TEXT("::"),nullptr,&Short);N=Enum->GetValueByNameString(Short);if(N==INDEX_NONE)N=Enum->GetValueByNameString(Text);Got=N!=INDEX_NONE;}}}
        if(!Got){FailValue(TEXT("Byte/enum numeric or named value invalid or outside safe conversion range"));Prepared=false;}else Numeric->SetIntPropertyValue(V,N);
    }
    if(!Prepared)return W.Encode(Operation);P=W.CurrentProperty();if(!P)return W.Encode(Operation);Actual=P->ContainerPtrToValuePtr<void>(O);if(!SameValue(P,Actual,Before.Bytes.GetData())){W.Fail(TEXT("blueprint_property_current_value_changed"),TEXT("Selected property changed during input preflight; no write dispatched."));return W.Encode(Operation);}
    W.R.Data->SetStringField(TEXT("property"),W.Name);W.R.Data->SetStringField(TEXT("property_type"),W.PropertyType);W.R.Data->SetBoolField(TEXT("inherited_property"),P->GetOwnerStruct()!=C);W.R.Data->SetField(TEXT("requested_value"),Value);W.Prefix->SetStringField(TEXT("blueprint"),B->GetPathName());W.Prefix->SetStringField(TEXT("cdo"),O->GetPathName());W.Prefix->SetStringField(TEXT("property"),W.Name);
    ExpectedCanonical=Canonical(P,Requested.Bytes.GetData(),O);if(ExpectedCanonical.IsEmpty()){W.Fail(TEXT("blueprint_property_capture_bound"),TEXT("Typed selected value capture exceeds32Mi UTF16; no write dispatched."));return W.Encode(Operation);}
    if(!W.SDK(TEXT("FProperty_SetValue"),true))return W.Encode(Operation);const bool WillChange=!SameValue(P,Actual,Requested.Bytes.GetData());P->CopySingleValue(P->ContainerPtrToValuePtr<void>(O),Requested.Bytes.GetData());W.R.bChanged|=WillChange;W.Prefix->SetBoolField(TEXT("property_assignment_returned"),true);
    } // Destroy scratch strings/reference slots while the pre-callback FProperty is still current.
    if(!W.After(TEXT("FProperty_SetValue")))return W.Encode(Operation);
    if(!W.CurrentProperty()||!W.SDK(TEXT("MarkBlueprintAsModified"),true))return W.Encode(Operation);FBlueprintEditorUtils::MarkBlueprintAsModified(B);if(!W.After(TEXT("MarkBlueprintAsModified")))return W.Encode(Operation);
    P=W.CurrentProperty();if(!P)return W.Encode(Operation);if(!Canonical(P,P->ContainerPtrToValuePtr<void>(O),O).Equals(ExpectedCanonical,ESearchCase::CaseSensitive)){W.Fail(TEXT("blueprint_property_readback_mismatch"),TEXT("Typed current property differs after MarkBlueprintAsModified callback; actual mutation retained without retry."));return W.Encode(Operation);}
    W.R.bChanged|=B->GetOutermost()->IsDirty()!=DirtyBefore||B->Status!=StatusBefore||B->bCachedDependenciesUpToDate!=CachedBefore;
    W.R.Data->SetBoolField(TEXT("package_dirty_before"),DirtyBefore);W.R.Data->SetBoolField(TEXT("package_dirty_after"),B->GetOutermost()->IsDirty());
    W.Legacy=MakeShared<FJsonObject>();W.Legacy->SetStringField(TEXT("property"),W.Name);W.Legacy->SetBoolField(TEXT("success"),true);W.R.bSuccess=W.R.bPostconditionPassed=true;W.R.bChangeStateKnown=true;W.R.Stage=TEXT("complete");W.R.Data->SetStringField(TEXT("scope"),TEXT("Exact current Blueprint/GeneratedClass/CDO/inherited field and typed selected-value readback only. Explicit compile/save0; MarkBlueprintAsModified callbacks/cache/derived-class effects are global SDK unknown. No persistence/atomicity/rollback proof."));return W.Encode(Operation);
}
FString ListGraphs(const FString& ParamsJson)
{
    constexpr const TCHAR* Operation=TEXT("list_blueprint_graphs");TSharedPtr<FJsonObject> P;if(!Parse(ParamsJson,P)){FWork W;W.Fail(TEXT("blueprint_metadata_invalid_params_json"),TEXT("Expected finite bounded JSON object."));return W.Encode(Operation);}FString Name,Type;if(!P->TryGetStringField(TEXT("blueprint_name"),Name)){FWork W;W.Fail(TEXT("blueprint_metadata_missing_name"),TEXT("Missing 'blueprint_name' parameter"));return W.Encode(Operation);}P->TryGetStringField(TEXT("graph_type"),Type);auto R=BlueprintTopology::ListGraphMetadata(Name,Type);R.Data->SetStringField(TEXT("operation"),Operation);R.Data->SetBoolField(TEXT("operation_complete"),true);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);if(!R.Data->HasTypedField<EJson::Object>(TEXT("legacy_result"))){auto Error=MakeShared<FJsonObject>();Error->SetBoolField(TEXT("success"),false);Error->SetStringField(TEXT("error"),R.Error);R.Data->SetObjectField(TEXT("legacy_result"),Error);}return Operations::EncodeVariableResult(R,Operation);
}
}
