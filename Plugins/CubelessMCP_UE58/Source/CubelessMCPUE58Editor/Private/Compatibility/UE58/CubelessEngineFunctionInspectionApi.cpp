#include "Compatibility/UE58/CubelessEngineFunctionInspectionApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "HAL/PlatformProcess.h"
#include "Misc/EngineVersion.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "UObject/StrongObjectPtr.h"

namespace Cubeless::UE58::EngineFunctionInspection
{
namespace
{
struct FTarget
{
    const TCHAR* ClassPath;
    const TCHAR* FunctionName;
    bool bChildActorCatalog = false;
    bool bInterpolationCatalog = false;
    bool bPhysicsAuthoringCatalog = false;
    bool bFacingCatalog = false;
    bool bSceneCameraCatalog = false;
    bool bActorMotionCatalog = false;
    bool bMaterialGraphCatalog = false;
    bool bVector4MakeCatalog = false;
};

constexpr FTarget Targets[] = {
    {TEXT("/Script/Engine.Controller"), TEXT("K2_GetPawn")},
    {TEXT("/Script/Engine.PlayerController"), TEXT("GetMousePosition")},
    {TEXT("/Script/Engine.PlayerController"), TEXT("GetFocalLocation")},
    {TEXT("/Script/Engine.PlayerController"), TEXT("IsInputKeyDown")},
    {TEXT("/Script/Engine.GameplayStatics"), TEXT("DeprojectScreenToWorld")},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("MakeVector2D")},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("BreakVector")},
    {TEXT("/Script/Engine.KismetSystemLibrary"), TEXT("GetObjectName")},
    {TEXT("/Script/Engine.KismetStringLibrary"), TEXT("Contains")},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("MakeVector")},
    {TEXT("/Script/Niagara.NiagaraDataInterfaceArrayFunctionLibrary"), TEXT("SetNiagaraArrayVector")},
    {TEXT("/Script/Niagara.NiagaraDataInterfaceArrayFunctionLibrary"), TEXT("GetNiagaraArrayVector")},
    {TEXT("/Script/Niagara.NiagaraDataInterfaceArrayFunctionLibrary"), TEXT("SetNiagaraArrayPosition")},
    {TEXT("/Script/Niagara.NiagaraDataInterfaceArrayFunctionLibrary"), TEXT("GetNiagaraArrayPosition")},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("MakeRandomStream")},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("RandomFloatInRangeFromStream")},
    {TEXT("/Script/Engine.SplineComponent"), TEXT("SetClosedLoop")},
    {TEXT("/Script/Engine.SplineComponent"), TEXT("SetSplinePoints")},
    {TEXT("/Script/Engine.SplineComponent"), TEXT("GetSplineLength")},
    {TEXT("/Script/Engine.SplineComponent"), TEXT("GetTransformAtDistanceAlongSpline")},
    {TEXT("/Script/Engine.SceneComponent"), TEXT("K2_GetComponentToWorld")},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Subtract_VectorVector")},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Multiply_VectorVector")},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Multiply_VectorFloat")},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("GreaterGreater_VectorRotator")},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("ComposeRotators")},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("GetRightVector")},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("MakeTransform")},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("ComposeTransforms")},
    {TEXT("/Script/Engine.InstancedStaticMeshComponent"), TEXT("ClearInstances")},
    {TEXT("/Script/Engine.InstancedStaticMeshComponent"), TEXT("AddInstance")},
    {TEXT("/Script/Engine.InstancedStaticMeshComponent"), TEXT("UpdateInstanceTransform")},
    {TEXT("/Script/Engine.InstancedStaticMeshComponent"), TEXT("GetInstanceCount")},
    {TEXT("/Script/Engine.InstancedStaticMeshComponent"), TEXT("GetInstanceTransform")},
    {TEXT("/Script/Engine.StaticMeshComponent"), TEXT("SetStaticMesh")},
    {TEXT("/Script/Engine.PrimitiveComponent"), TEXT("SetCollisionProfileName")},
    {TEXT("/Script/Engine.StaticMesh"), TEXT("GetBounds")},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("BreakBoxSphereBounds")},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("SelectVector")},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Vector_GetAbs")},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Vector_GetAbsMin")},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("GetForwardVector")},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("GetUpVector")},
    {TEXT("/Script/Engine.Actor"), TEXT("GetTransform"), true},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("InvertTransform"), true},
    {TEXT("/Script/Engine.Actor"), TEXT("AddComponentByClass"), true},
    {TEXT("/Script/Engine.ChildActorComponent"), TEXT("SetChildActorClass"), true},
    {TEXT("/Script/Engine.Actor"), TEXT("K2_GetComponentsByClass"), true},
    {TEXT("/Script/Engine.SceneComponent"), TEXT("SetMobility"), true},
    {TEXT("/Script/Engine.Actor"), TEXT("SetActorEnableCollision"), true},
    {TEXT("/Script/Engine.ActorComponent"), TEXT("K2_DestroyComponent"), true},
    {TEXT("/Script/Engine.Actor"), TEXT("GetActorBounds"), true},
    {TEXT("/Script/Engine.SceneComponent"), TEXT("K2_SetWorldTransform"), true},
    {TEXT("/Script/Engine.KismetSystemLibrary"), TEXT("IsValidClass"), true},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("ClassIsChildOf"), true},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("VInterpTo"), false, true},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("VInterpTo_Constant"), false, true},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("RInterpTo"), false, true},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("RInterpTo_Constant"), false, true},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("VLerp"), false, true},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("RLerp"), false, true},
    {TEXT("/Script/Engine.PrimitiveComponent"), TEXT("AddForce"), false, false, true},
    {TEXT("/Script/Engine.PrimitiveComponent"), TEXT("AddImpulse"), false, false, true},
    {TEXT("/Script/Engine.PrimitiveComponent"), TEXT("AddTorqueInRadians"), false, false, true},
    {TEXT("/Script/Engine.PrimitiveComponent"), TEXT("SetMassScale"), false, false, true},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("FindLookAtRotation"), false, false, false, true},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("SelectRotator"), false, false, false, true},
    {TEXT("/Script/Engine.SceneComponent"), TEXT("K2_GetComponentLocation"), false, false, false, false, true},
    {TEXT("/Script/Engine.PlayerCameraManager"), TEXT("GetCameraRotation"), false, false, false, false, true},
    {TEXT("/Script/Engine.SceneComponent"), TEXT("K2_SetWorldRotation"), false, false, false, false, true},
    {TEXT("/Script/Engine.GameplayStatics"), TEXT("GetPlayerCameraManager"), false, false, false, false, true},
    {TEXT("/Script/Engine.SceneComponent"), TEXT("K2_SetRelativeLocation"), false, false, false, false, true},
    {TEXT("/Script/Engine.SceneComponent"), TEXT("K2_SetWorldLocation"), false, false, false, false, true},
    {TEXT("/Script/Engine.SceneComponent"), TEXT("SetWorldScale3D"), false, false, false, false, true},
    {TEXT("/Script/Engine.SceneComponent"), TEXT("K2_GetComponentScale"), false, false, false, false, true},
    {TEXT("/Script/Engine.BoxComponent"), TEXT("SetBoxExtent"), false, false, false, false, true},
    {TEXT("/Script/Engine.KismetSystemLibrary"), TEXT("GetComponentBounds"), false, false, false, false, true},
    {TEXT("/Script/Engine.ActorComponent"), TEXT("SetAutoActivate"), false, false, false, false, true},
    {TEXT("/Script/Engine.ActorComponent"), TEXT("SetComponentTickInterval"), false, false, false, false, true},
    {TEXT("/Script/Engine.SceneComponent"), TEXT("SetVisibility"), false, false, false, false, true},
    {TEXT("/Script/Engine.SceneComponent"), TEXT("SetHiddenInGame"), false, false, false, false, true},
    {TEXT("/Script/Engine.SceneComponent"), TEXT("SetAbsolute"), false, false, false, false, true},
    {TEXT("/Script/Engine.PrimitiveComponent"), TEXT("SetCollisionEnabled"), false, false, false, false, true},
    {TEXT("/Script/Engine.PrimitiveComponent"), TEXT("SetGenerateOverlapEvents"), false, false, false, false, true},
    {TEXT("/Script/Engine.PrimitiveComponent"), TEXT("SetTranslucentSortPriority"), false, false, false, false, true},
    {TEXT("/Script/Niagara.NiagaraComponent"), TEXT("SetVariableFloat"), false, false, false, false, true},
    {TEXT("/Script/Niagara.NiagaraComponent"), TEXT("SetVariableInt"), false, false, false, false, true},
    {TEXT("/Script/Niagara.NiagaraComponent"), TEXT("SetVariableBool"), false, false, false, false, true},
    {TEXT("/Script/Niagara.NiagaraComponent"), TEXT("SetVariableVec3"), false, false, false, false, true},
    {TEXT("/Script/Niagara.NiagaraComponent"), TEXT("SetForceSolo"), false, false, false, false, true},
    {TEXT("/Script/Niagara.NiagaraComponent"), TEXT("ReinitializeSystem"), false, false, false, false, true},
    {TEXT("/Script/Engine.Actor"), TEXT("SetActorHiddenInGame"), false, false, false, false, false, true},
    {TEXT("/Script/Engine.Actor"), TEXT("WasRecentlyRendered"), false, false, false, false, false, true},
    {TEXT("/Script/Engine.Actor"), TEXT("K2_SetActorRelativeLocation"), false, false, false, false, false, true},
    {TEXT("/Script/Engine.Actor"), TEXT("K2_SetActorRotation"), false, false, false, false, false, true},
    {TEXT("/Script/Engine.Actor"), TEXT("K2_SetActorRelativeRotation"), false, false, false, false, false, true},
    {TEXT("/Script/Engine.PrimitiveComponent"), TEXT("CreateDynamicMaterialInstance"), false, false, false, false, false, false, true},
    {TEXT("/Script/Engine.MaterialInstanceDynamic"), TEXT("SetTextureParameterValue"), false, false, false, false, false, false, true},
    {TEXT("/Script/Engine.MaterialInstanceDynamic"), TEXT("SetVectorParameterValue"), false, false, false, false, false, false, true},
    {TEXT("/Script/Engine.MaterialInstanceDynamic"), TEXT("SetScalarParameterValue"), false, false, false, false, false, false, true},
    {TEXT("/Script/Engine.MaterialInstanceDynamic"), TEXT("K2_GetVectorParameterValue"), false, false, false, false, false, false, true},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Conv_VectorToLinearColor"), false, false, false, false, false, false, true},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Conv_LinearColorToVector"), false, false, false, false, false, false, true},
    {TEXT("/Script/Engine.PrimitiveComponent"), TEXT("GetMaterial"), false, false, false, false, false, false, true},
    {TEXT("/Script/Engine.PrimitiveComponent"), TEXT("SetMaterial"), false, false, false, false, false, false, true},
    {TEXT("/Script/Engine.MeshComponent"), TEXT("SetScalarParameterValueOnMaterials"), false, false, false, false, false, false, true},
    {TEXT("/Script/Engine.MeshComponent"), TEXT("SetVectorParameterValueOnMaterials"), false, false, false, false, false, false, true},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("MakeVector4"), false, false, false, false, false, false, false, true},
    {TEXT("/Script/Niagara.NiagaraDataInterfaceArrayFunctionLibrary"), TEXT("SetNiagaraArrayVector4")},
    {TEXT("/Script/Engine.KismetRenderingLibrary"), TEXT("CreateRenderTarget2D")},
    {TEXT("/Script/Niagara.NiagaraComponent"), TEXT("SetVariableTextureRenderTarget")},
    {TEXT("/Script/Niagara.NiagaraComponent"), TEXT("SetAsset")},
    {TEXT("/Script/Engine.ActorComponent"), TEXT("Activate")},
    {TEXT("/Script/Engine.ActorComponent"), TEXT("Deactivate")},
    {TEXT("/Script/Engine.PlayerController"), TEXT("GetHitResultUnderFingerByChannel")},
    {TEXT("/Script/Engine.GameplayStatics"), TEXT("BreakHitResult")},
    {TEXT("/Script/Engine.SceneComponent"), TEXT("GetForwardVector"), false, false, false, false, true},
    {TEXT("/Script/Engine.SceneComponent"), TEXT("GetRightVector"), false, false, false, false, true},
    {TEXT("/Script/Engine.SceneComponent"), TEXT("GetUpVector"), false, false, false, false, true},
    {TEXT("/Script/Engine.ActorComponent"), TEXT("AddTickPrerequisiteActor"), false, false, false, false, true},
    {TEXT("/Script/AssetRegistry.AssetRegistryHelpers"), TEXT("CreateAssetData")},
    {TEXT("/Script/AssetRegistry.AssetRegistryHelpers"), TEXT("GetAsset")},
    {TEXT("/Script/Engine.DataTableFunctionLibrary"), TEXT("GetDataTableRowNames")},
    {TEXT("/Script/Engine.GameplayStatics"), TEXT("GetActorOfClass")},
    {TEXT("/Script/Engine.PlayerController"), TEXT("SetViewTargetWithBlend")},
    {TEXT("/Script/Engine.Actor"), TEXT("GetComponentByClass")},
    {TEXT("/Script/Engine.ActorComponent"), TEXT("ComponentHasTag")},
    {TEXT("/Script/Engine.Actor"), TEXT("GetComponentsByTag")},
    {TEXT("/Script/Engine.SceneComponent"), TEXT("DoesSocketExist")},
    {TEXT("/Script/Engine.SceneComponent"), TEXT("GetSocketLocation")},
    {TEXT("/Script/Engine.SkinnedMeshComponent"), TEXT("GetParentBone")},
    {TEXT("/Script/Engine.KismetSystemLibrary"), TEXT("GetPathName")},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("InverseTransformLocation")},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("InverseTransformDirection")},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Divide_VectorFloat")},
    {TEXT("/Script/Engine.BlueprintMapLibrary"), TEXT("Map_Clear")},
    {TEXT("/Script/Engine.BlueprintMapLibrary"), TEXT("Map_Find")},
    {TEXT("/Script/Engine.BlueprintMapLibrary"), TEXT("Map_Add")},
    {TEXT("/Script/AssetRegistry.AssetRegistryHelpers"), TEXT("GetAssetRegistry")},
    {TEXT("/Script/Engine.KismetSystemLibrary"), TEXT("MakeARFilter")},
    {TEXT("/Script/AssetRegistry.AssetRegistry"), TEXT("GetAssets")},
    {TEXT("/Script/AssetRegistry.AssetRegistry"), TEXT("GetAssetsByPackageName")},
    {TEXT("/Script/Engine.KismetStringLibrary"), TEXT("Conv_StringToDouble")},
    {TEXT("/Script/Engine.KismetTextLibrary"), TEXT("Conv_DoubleToText")},
    {TEXT("/Script/Engine.KismetTextLibrary"), TEXT("Conv_TextToString")},
    {TEXT("/Script/Engine.KismetStringLibrary"), TEXT("Replace")},
    {TEXT("/Script/Engine.KismetStringLibrary"), TEXT("Conv_NameToString")},
    {TEXT("/Script/Engine.KismetStringLibrary"), TEXT("Concat_StrStr")},
    {TEXT("/Script/Engine.KismetMathLibrary"), TEXT("EqualEqual_IntInt")},
};

bool Fail(Function::FResult& R, const TCHAR* Code, const TCHAR* Message)
{
    R.ErrorCode = Code; R.Error = Message; return false;
}

FString Text(const TSharedRef<FJsonObject>& Object)
{
    FString Out; FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Out)); return Out;
}

bool IsExactLoadedNativeClass(const UClass* Class, const FString& Path)
{
    return IsValid(Class) && Class->GetPathName() == Path && Class->HasAnyClassFlags(CLASS_Native)
        && !Class->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists) && !Class->IsEditorOnly()
        && (Class->GetOutermost()->GetName() == TEXT("/Script/Engine")
            || ((Path==TEXT("/Script/AssetRegistry.AssetRegistryHelpers")||Path==TEXT("/Script/AssetRegistry.AssetRegistry"))&&Class->GetOutermost()->GetName()==TEXT("/Script/AssetRegistry"))
            || ((Path == TEXT("/Script/Niagara.NiagaraDataInterfaceArrayFunctionLibrary") || Path == TEXT("/Script/Niagara.NiagaraComponent"))
                && Class->GetOutermost()->GetName() == TEXT("/Script/Niagara")))
        && !Class->GetOutermost()->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly);
}

// LX exact4 probe leaf. It observes reflected metadata only, never a registry/interface value.
TSharedRef<FJsonObject> DescribeAssetSearchProperty(const FProperty* P,const UEdGraphSchema_K2* Schema,int32 Depth=0,const FProperty* Container=nullptr)
{
    auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("schema_conversion_success"),false);J->SetBoolField(TEXT("observation_complete"),false);
    const auto Loaded=[](const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);};
    if(!IsEditorGameThread()||IsPlaySessionActive()||GIsImportingT3D||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||!P||Depth>1||!Loaded(Schema))return J;UStruct* Owner=P->GetOwnerStruct();if(!Loaded(Owner))return J;TStrongObjectPtr<UStruct> Held(Owner);TStrongObjectPtr<UEdGraphSchema_K2> HeldSchema(const_cast<UEdGraphSchema_K2*>(Schema));TArray<FProperty*> Fields;for(TFieldIterator<FProperty> It(Owner,EFieldIterationFlags::None);It;++It)Fields.Add(*It);
    const FProperty* Root=Container?Container:P;if(!Fields.Contains(const_cast<FProperty*>(Root)))return J;const auto* A=CastField<FArrayProperty>(Root);const auto* Set=CastField<FSetProperty>(Root);const FProperty* Child=A?A->Inner:Set?Set->ElementProp:nullptr;if(Container&&Child!=P)return J;const auto Flags=P->GetPropertyFlags();const auto Class=P->GetClass();const int32 Size=P->GetSize(),Offset=P->GetOffset_ForInternal(),Dim=P->ArrayDim;const auto* SP=CastField<FStructProperty>(P);const auto* IP=CastField<FInterfaceProperty>(P);const TObjectPtr<UScriptStruct> StructHandle=SP?SP->Struct:TObjectPtr<UScriptStruct>();const TObjectPtr<UClass> InterfaceHandle=IP?IP->InterfaceClass:TObjectPtr<UClass>();
    auto Current=[&](){if(!IsEditorGameThread()||IsPlaySessionActive()||GIsImportingT3D||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||!Loaded(Owner)||Owner!=Held.Get()||!Loaded(Schema)||Schema!=HeldSchema.Get())return false;TArray<FProperty*> Now;for(TFieldIterator<FProperty> It(Owner,EFieldIterationFlags::None);It;++It)Now.Add(*It);if(Now!=Fields)return false;if(A&&A->Inner!=Child||Set&&Set->ElementProp!=Child)return false;return P->GetOwnerStruct()==Owner&&P->GetClass()==Class&&P->GetPropertyFlags()==Flags&&P->GetSize()==Size&&P->GetOffset_ForInternal()==Offset&&P->ArrayDim==Dim&&(!SP||(SP->Struct.IsResolved()&&SP->Struct==StructHandle))&&(!IP||(IP->InterfaceClass.IsResolved()&&IP->InterfaceClass==InterfaceHandle));};
    const FString Cpp=P->GetCPPType(nullptr,0);if(!Current())return J;J->SetStringField(TEXT("name"),P->GetName());J->SetStringField(TEXT("path"),P->GetPathName());J->SetStringField(TEXT("field_class"),Class->GetName());J->SetStringField(TEXT("cpp_type"),Cpp);J->SetStringField(TEXT("property_flags_string"),LexToString(uint64(Flags)));J->SetNumberField(TEXT("array_dim"),Dim);J->SetNumberField(TEXT("offset"),Offset);J->SetNumberField(TEXT("element_size"),P->ElementSize);J->SetNumberField(TEXT("size"),Size);
    J->SetBoolField(TEXT("parm"),P->HasAnyPropertyFlags(CPF_Parm));J->SetBoolField(TEXT("return_parm"),P->HasAnyPropertyFlags(CPF_ReturnParm));J->SetBoolField(TEXT("out_parm"),P->HasAnyPropertyFlags(CPF_OutParm));J->SetBoolField(TEXT("reference_parm"),P->HasAnyPropertyFlags(CPF_ReferenceParm));J->SetBoolField(TEXT("const_parm"),P->HasAnyPropertyFlags(CPF_ConstParm));J->SetBoolField(TEXT("advanced_display"),P->HasAnyPropertyFlags(CPF_AdvancedDisplay));J->SetBoolField(TEXT("editor_only"),P->HasAnyPropertyFlags(CPF_EditorOnly));
    if(IP){const auto H=IP->InterfaceClass;J->SetStringField(TEXT("interface_class_path"),H.IsResolved()?H.GetPathName():FString());J->SetBoolField(TEXT("interface_class_handle_resolved"),H.IsResolved());if(!Current()||!IP->InterfaceClass.IsResolved()||IP->InterfaceClass!=H)return J;}
    if(SP){const auto H=SP->Struct;J->SetStringField(TEXT("struct_path"),H.IsResolved()?H.GetPathName():FString());if(!Current()||!SP->Struct.IsResolved()||SP->Struct!=H)return J;}
    if(!Container&&Child){auto E=DescribeAssetSearchProperty(Child,Schema,Depth+1,P);if(!Current())return J;J->SetObjectField(A?TEXT("array_inner"):TEXT("set_element"),E);J->SetBoolField(A?TEXT("array_inner_observation_complete"):TEXT("set_element_observation_complete"),E->GetBoolField(TEXT("observation_complete")));if(!E->GetBoolField(TEXT("observation_complete")))return J;}
    FEdGraphPinType T;const bool Converted=Schema->ConvertPropertyToPinType(P,T);if(!Current())return J;if(!Converted)return J;FString TypeText;FEdGraphPinType::StaticStruct()->ExportText(TypeText,&T,nullptr,nullptr,PPF_None,nullptr);if(!Current())return J;
    UObject* Primary=T.PinSubCategoryObject.Get();if(!Current()||(Primary&&!Loaded(Primary)))return J;TStrongObjectPtr<UObject> PrimaryHold(Primary);const FString PrimaryPath=Primary?Primary->GetPathName():FString();if(!Current()||T.PinSubCategoryObject.Get()!=Primary)return J;
    UObject* TerminalObject=T.PinValueType.TerminalSubCategoryObject.Get();if(!Current()||(TerminalObject&&!Loaded(TerminalObject)))return J;TStrongObjectPtr<UObject> TerminalHold(TerminalObject);const FString TerminalPath=TerminalObject?TerminalObject->GetPathName():FString();if(!Current()||T.PinValueType.TerminalSubCategoryObject.Get()!=TerminalObject)return J;
    J->SetStringField(TEXT("pin_type_text"),TypeText);J->SetStringField(TEXT("pin_category"),T.PinCategory.ToString());J->SetStringField(TEXT("pin_subcategory"),T.PinSubCategory.ToString());J->SetStringField(TEXT("pin_type_object"),PrimaryPath);J->SetBoolField(TEXT("pin_is_reference"),T.bIsReference);J->SetBoolField(TEXT("pin_is_const"),T.bIsConst);J->SetBoolField(TEXT("pin_is_container"),T.IsContainer());J->SetNumberField(TEXT("pin_container_type"),int32(T.ContainerType));J->SetBoolField(TEXT("pin_is_weak_pointer"),T.bIsWeakPointer);J->SetBoolField(TEXT("pin_is_uobject_wrapper"),T.bIsUObjectWrapper);J->SetBoolField(TEXT("pin_serialize_single_precision"),T.bSerializeAsSinglePrecisionFloat);auto Terminal=MakeShared<FJsonObject>();Terminal->SetStringField(TEXT("category"),T.PinValueType.TerminalCategory.ToString());Terminal->SetStringField(TEXT("subcategory"),T.PinValueType.TerminalSubCategory.ToString());Terminal->SetStringField(TEXT("object"),TerminalPath);Terminal->SetBoolField(TEXT("const"),T.PinValueType.bTerminalIsConst);Terminal->SetBoolField(TEXT("weak"),T.PinValueType.bTerminalIsWeakPointer);Terminal->SetBoolField(TEXT("wrapper"),T.PinValueType.bTerminalIsUObjectWrapper);J->SetObjectField(TEXT("pin_terminal"),Terminal);J->SetBoolField(TEXT("schema_conversion_success"),Current());J->SetBoolField(TEXT("observation_complete"),Current());return J;
}
// LX AssetSearchProbe_End
TSharedRef<FJsonObject> DescribeProperty(const FProperty* Property, const UEdGraphSchema_K2* Schema, int32 Depth = 0, bool TouchTraceCatalog = false, const TFunction<bool()>& Current = TFunction<bool()>())
{
    auto Row = MakeShared<FJsonObject>();
    if(Current){Row->SetBoolField(TEXT("schema_conversion_success"),false);Row->SetBoolField(TEXT("observation_complete"),false);if(!Current())return Row;}
    Row->SetStringField(TEXT("name"), Property->GetName());
    Row->SetStringField(TEXT("path"), Property->GetPathName());
    Row->SetStringField(TEXT("field_class"), Property->GetClass()->GetName());
    const FString CppType=Property->GetCPPType();if(Current&&!Current())return Row;
    Row->SetStringField(TEXT("cpp_type"),CppType);
    Row->SetStringField(TEXT("property_flags_string"), LexToString(static_cast<uint64>(Property->GetPropertyFlags())));
    Row->SetNumberField(TEXT("array_dim"), Property->ArrayDim);
    Row->SetBoolField(TEXT("parm"), Property->HasAnyPropertyFlags(CPF_Parm));
    Row->SetBoolField(TEXT("return_parm"), Property->HasAnyPropertyFlags(CPF_ReturnParm));
    Row->SetBoolField(TEXT("out_parm"), Property->HasAnyPropertyFlags(CPF_OutParm));
    Row->SetBoolField(TEXT("reference_parm"), Property->HasAnyPropertyFlags(CPF_ReferenceParm));
    Row->SetBoolField(TEXT("const_parm"), Property->HasAnyPropertyFlags(CPF_ConstParm));
    Row->SetBoolField(TEXT("required_parm"), Property->HasAnyPropertyFlags(CPF_RequiredParm));
    Row->SetBoolField(TEXT("blueprint_visible"), Property->HasAnyPropertyFlags(CPF_BlueprintVisible));
    Row->SetBoolField(TEXT("blueprint_read_only"), Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly));
    Row->SetBoolField(TEXT("editor_only"), Property->HasAnyPropertyFlags(CPF_EditorOnly));
    if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
    { Row->SetStringField(TEXT("struct_path"), Struct->Struct ? Struct->Struct->GetPathName() : FString()); }
    if (const FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(Property))
    { Row->SetStringField(TEXT("object_class_path"), Object->PropertyClass ? Object->PropertyClass->GetPathName() : FString()); }
    if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
    {
        const bool InnerValid = Depth == 0 && Array->Inner && Array->Inner->ArrayDim == 1
            && !CastField<FArrayProperty>(Array->Inner) && !CastField<FMapProperty>(Array->Inner) && !CastField<FSetProperty>(Array->Inner);
        bool InnerComplete = false;
        if (InnerValid)
        {
            auto Inner = DescribeProperty(Array->Inner, Schema, Depth + 1);
            Inner->TryGetBoolField(TEXT("schema_conversion_success"), InnerComplete);
            Row->SetObjectField(TEXT("array_inner"), Inner);
        }
        Row->SetBoolField(TEXT("array_inner_observation_complete"), InnerValid && InnerComplete);
    }
    if (const FMapProperty* Map = CastField<FMapProperty>(Property))
    {
        const bool ChildrenValid = Depth == 0 && Map->KeyProp && Map->ValueProp
            && Map->KeyProp->ArrayDim == 1 && Map->ValueProp->ArrayDim == 1
            && CastField<FIntProperty>(Map->KeyProp) && CastField<FIntProperty>(Map->ValueProp);
        bool KeyComplete = false, ValueComplete = false;
        if (ChildrenValid)
        {
            auto Key = DescribeProperty(Map->KeyProp, Schema, Depth + 1);
            auto Value = DescribeProperty(Map->ValueProp, Schema, Depth + 1);
            Key->TryGetBoolField(TEXT("schema_conversion_success"), KeyComplete);
            Value->TryGetBoolField(TEXT("schema_conversion_success"), ValueComplete);
            Row->SetObjectField(TEXT("map_key"), Key); Row->SetObjectField(TEXT("map_value"), Value);
        }
        Row->SetBoolField(TEXT("map_children_observation_complete"), ChildrenValid && KeyComplete && ValueComplete);
    }
    FEdGraphPinType Type;
    const bool Converted = Schema->ConvertPropertyToPinType(Property, Type);
    if(Current&&!Current())return Row;
    Row->SetBoolField(TEXT("schema_conversion_success"), Converted);
    if (Converted)
    {
        FString TypeText;
        FEdGraphPinType::StaticStruct()->ExportText(TypeText, &Type, &Type, nullptr, PPF_None, nullptr);
        if(Current&&!Current()){Row->SetBoolField(TEXT("schema_conversion_success"),false);return Row;}
        Row->SetStringField(TEXT("pin_type_text"), TypeText);
        Row->SetStringField(TEXT("pin_category"), Type.PinCategory.ToString());
        Row->SetStringField(TEXT("pin_subcategory"), Type.PinSubCategory.ToString());
        Row->SetStringField(TEXT("pin_type_object"), Type.PinSubCategoryObject.IsValid() ? Type.PinSubCategoryObject->GetPathName() : FString());
        Row->SetBoolField(TEXT("pin_is_reference"), Type.bIsReference);
        Row->SetBoolField(TEXT("pin_is_const"), Type.bIsConst);
        Row->SetBoolField(TEXT("pin_is_container"), Type.IsContainer());
        Row->SetNumberField(TEXT("pin_container_type"), static_cast<int32>(Type.ContainerType));
        Row->SetBoolField(TEXT("pin_is_weak_pointer"), Type.bIsWeakPointer);
        if (Type.IsMap())
        {
            auto Terminal = MakeShared<FJsonObject>();
            Terminal->SetStringField(TEXT("category"), Type.PinValueType.TerminalCategory.ToString());
            Terminal->SetStringField(TEXT("subcategory"), Type.PinValueType.TerminalSubCategory.ToString());
            Terminal->SetStringField(TEXT("object"), Type.PinValueType.TerminalSubCategoryObject.IsValid() ? Type.PinValueType.TerminalSubCategoryObject->GetPathName() : FString());
            Terminal->SetBoolField(TEXT("const"), Type.PinValueType.bTerminalIsConst);
            Terminal->SetBoolField(TEXT("weak"), Type.PinValueType.bTerminalIsWeakPointer);
            Terminal->SetBoolField(TEXT("wrapper"), Type.PinValueType.bTerminalIsUObjectWrapper);
            Row->SetObjectField(TEXT("map_terminal"), Terminal);
        }
        if (const UEnum* Enum = Cast<UEnum>(Type.PinSubCategoryObject.Get()))
        {
            auto EnumRow = MakeShared<FJsonObject>();
            const int32 Count = Enum->NumEnums();
            // LF's exact ETraceTypeQuery currently has 64 authored channels plus MAX.
            // Keep the old bounded catalog policy unchanged for every previous target.
            const bool TouchTraceEnum = TouchTraceCatalog && (Enum->GetPathName()==TEXT("/Script/Engine.ETraceTypeQuery")||Enum->GetPathName()==TEXT("/Script/InputCore.ETouchIndex")) && Count<=128;
            const bool EnumComplete = Count > 0 && (Count <= 32 || TouchTraceEnum);
            EnumRow->SetStringField(TEXT("path"), Enum->GetPathName());
            EnumRow->SetStringField(TEXT("package_path"), Enum->GetOutermost()->GetName());
            EnumRow->SetStringField(TEXT("package_flags_string"), LexToString(static_cast<uint32>(Enum->GetOutermost()->GetPackageFlags())));
            EnumRow->SetBoolField(TEXT("package_dirty"), Enum->GetOutermost()->IsDirty());
            EnumRow->SetBoolField(TEXT("editor_only"), Enum->IsEditorOnly());
            EnumRow->SetNumberField(TEXT("cpp_form"), static_cast<int32>(Enum->GetCppForm()));
            EnumRow->SetNumberField(TEXT("declared_count"), Count);
            EnumRow->SetBoolField(TEXT("observations_complete"), EnumComplete);
            TArray<TSharedPtr<FJsonValue>> Values;
            if (EnumComplete)
            {
                for (int32 Index = 0; Index < Count; ++Index)
                {
                    auto Value = MakeShared<FJsonObject>();
                    Value->SetNumberField(TEXT("ordinal"), Index);
                    Value->SetStringField(TEXT("name"), Enum->GetNameByIndex(Index).ToString());
                    Value->SetStringField(TEXT("value_string"), LexToString(Enum->GetValueByIndex(Index)));
                    Values.Add(MakeShared<FJsonValueObject>(Value));
                }
            }
            EnumRow->SetArrayField(TEXT("values"), Values);
            Row->SetObjectField(TEXT("enum_metadata"), EnumRow);
            Row->SetBoolField(TEXT("enum_observation_complete"), EnumComplete);
        }
    }
    if(Current){Row->SetBoolField(TEXT("observation_complete"),Current());Row->SetBoolField(TEXT("pin_is_uobject_wrapper"),Type.bIsUObjectWrapper);Row->SetBoolField(TEXT("pin_serialize_single_precision"),Type.bSerializeAsSinglePrecisionFloat);}
    return Row;
}

// ME exact4 metadata observation. Values are copied before each callback fence;
// existing 142 targets keep their original path and output.
bool IsNormalizationTarget(const FTarget& T)
{
    return (FString(T.ClassPath)==TEXT("/Script/Engine.KismetMathLibrary")&&FString(T.FunctionName)==TEXT("EqualEqual_IntInt"))||(FString(T.ClassPath)==TEXT("/Script/Engine.KismetStringLibrary")&&(FString(T.FunctionName)==TEXT("Conv_StringToDouble")||FString(T.FunctionName)==TEXT("Replace")))
        ||(FString(T.ClassPath)==TEXT("/Script/Engine.KismetTextLibrary")&&(FString(T.FunctionName)==TEXT("Conv_DoubleToText")||FString(T.FunctionName)==TEXT("Conv_TextToString")));
}
TObjectPtr<UObject> NormalizationProbeCDO(const UClass* C)
{
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    const TObjectPtr<UObject> H=C->ClassDefaultObject;
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
    return H;
}
bool DescribeNormalizationTarget(const FTarget& T,const UEdGraphSchema_K2* Schema,const TSharedRef<FJsonObject>& Row)
{
    const auto Ready=[](){return IsEditorGameThread()&&!IsPlaySessionActive()&&!GIsImportingT3D&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage();};
    const auto Loaded=[](const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);};
    const bool IntegerComparison=FString(T.ClassPath)==TEXT("/Script/Engine.KismetMathLibrary")&&FString(T.FunctionName)==TEXT("EqualEqual_IntInt");
    Row->SetBoolField(TEXT("observation_complete"),false);Row->SetBoolField(IntegerComparison?TEXT("integer_comparison_runtime_executed"):TEXT("normalization_runtime_executed"),false);
    if(!Ready()||!Loaded(Schema))return false;auto* const Editor=GEditor;UWorld* const World=GWorld;auto* const Undo=GUndo;
    TStrongObjectPtr<UEdGraphSchema_K2> SchemaHold(const_cast<UEdGraphSchema_K2*>(Schema));
    UClass* O=FindObject<UClass>(nullptr,T.ClassPath);TStrongObjectPtr<UClass> Owner;if(Ready()&&Loaded(O))Owner.Reset(O);
    if(!Owner.IsValid()||!IsExactLoadedNativeClass(O,T.ClassPath)||!O->ClassGeneratedBy.IsResolved()||O->ClassGeneratedBy!=nullptr)return false;
    TStrongObjectPtr<UPackage> Package(O->GetPackage());if(!Package.IsValid())return false;
    const FString OwnerPath=O->GetPathName(),PackagePath=Package->GetPathName();const EObjectFlags OwnerFlags=O->GetFlags(),SchemaFlags=Schema->GetFlags();const uint32 ClassFlags=uint32(O->GetClassFlags()),PackageFlags=Package->GetPackageFlags();const bool Dirty=Package->IsDirty();const auto CDO=NormalizationProbeCDO(O),SchemaCDO=NormalizationProbeCDO(UEdGraphSchema_K2::StaticClass());
    if(!CDO.IsResolved()||!SchemaCDO.IsResolved()||SchemaCDO!=Schema)return false;
    const auto OwnerCurrent=[&](){return Ready()&&GEditor==Editor&&GWorld==World&&GUndo==Undo&&Loaded(O)&&Loaded(Schema)&&Loaded(Package.Get())&&O==Owner.Get()&&Schema==SchemaHold.Get()&&O->GetPathName().Equals(OwnerPath,ESearchCase::CaseSensitive)&&O->GetFlags()==OwnerFlags&&Schema->GetFlags()==SchemaFlags&&uint32(O->GetClassFlags())==ClassFlags&&O->GetPackage()==Package.Get()&&Package->GetPathName().Equals(PackagePath,ESearchCase::CaseSensitive)&&Package->GetPackageFlags()==PackageFlags&&Package->IsDirty()==Dirty&&O->ClassGeneratedBy.IsResolved()&&O->ClassGeneratedBy==nullptr&&NormalizationProbeCDO(O).IsResolved()&&NormalizationProbeCDO(O)==CDO&&NormalizationProbeCDO(UEdGraphSchema_K2::StaticClass()).IsResolved()&&NormalizationProbeCDO(UEdGraphSchema_K2::StaticClass())==Schema;};
    UFunction* F=O->FindFunctionByName(FName(T.FunctionName));TStrongObjectPtr<UFunction> Function;if(Ready()&&Loaded(F))Function.Reset(F);if(!OwnerCurrent()||!Function.IsValid()||F->GetOuter()!=O||F->GetName()!=T.FunctionName)return false;
    const FString FunctionPath=F->GetPathName();const EObjectFlags FunctionFlags=F->GetFlags();const uint32 Flags=uint32(F->FunctionFlags);const int32 Count=F->NumParms,Size=F->ParmsSize;
    TMap<FName,FString> FunctionMeta;if(const auto* M=FMetaData::GetMapForObject(F))FunctionMeta=*M;
    struct FRole{FProperty* P;FFieldClass* Class;FName Name;uint64 Flags;int32 Offset,Size,Dim;TObjectPtr<UEnum> Enum;TMap<FName,FString> Metadata;};TArray<FProperty*> Inventory;TArray<FRole> Roles;
    for(TFieldIterator<FProperty> It(F,EFieldIterationFlags::None);It;++It){auto* P=*It;Inventory.Add(P);if(!P->HasAnyPropertyFlags(CPF_Parm)||P->HasSetterOrGetter()||P->ArrayDim!=1||CastField<FArrayProperty>(P)||CastField<FMapProperty>(P)||CastField<FStructProperty>(P)||CastField<FObjectPropertyBase>(P))return false;FRole V{P,P->GetClass(),P->GetFName(),uint64(P->GetPropertyFlags()),P->GetOffset_ForInternal(),P->GetSize(),P->ArrayDim,TObjectPtr<UEnum>(),{}};if(auto* B=CastField<FByteProperty>(P))V.Enum=B->Enum;if(!V.Enum.IsResolved())return false;if(const auto* M=P->GetMetaDataMap())V.Metadata=*M;Roles.Add(MoveTemp(V));}
    struct FEnumRole{TStrongObjectPtr<UEnum> E;TStrongObjectPtr<UPackage> Package;FString Path;EObjectFlags Flags;uint32 PackageFlags;TArray<FName> Names;TArray<int64> Values;TMap<FName,FString> Metadata;UEnum::ECppForm Form;};TArray<FEnumRole> Enums;
    const auto Current=[&](){if(!OwnerCurrent()||!Loaded(F)||F!=Function.Get()||F->GetOuter()!=O||!F->GetPathName().Equals(FunctionPath,ESearchCase::CaseSensitive)||F->GetFlags()!=FunctionFlags||uint32(F->FunctionFlags)!=Flags||F->NumParms!=Count||F->ParmsSize!=Size)return false;TArray<FProperty*> Now;for(TFieldIterator<FProperty> It(F,EFieldIterationFlags::None);It;++It)Now.Add(*It);if(Now!=Inventory)return false;if(IntegerComparison){bool Bound=false;for(TFieldIterator<UFunction> It(O,EFieldIteratorFlags::ExcludeSuper);It;++It)if(It->GetFName()==FName(T.FunctionName)){if(*It!=F)return false;Bound=true;}if(!Bound)return false;}const auto SameMeta=[](const TMap<FName,FString>* M,const TMap<FName,FString>& Expected){if((M?M->Num():0)!=Expected.Num())return false;for(const auto& Pair:Expected){const auto* V=M?M->Find(Pair.Key):nullptr;if(!V||!V->Equals(Pair.Value,ESearchCase::CaseSensitive))return false;}return true;};if(!SameMeta(FMetaData::GetMapForObject(F),FunctionMeta))return false;for(const auto& V:Roles){auto* P=V.P;if(P->GetOwnerStruct()!=F||P->GetClass()!=V.Class||P->GetFName()!=V.Name||uint64(P->GetPropertyFlags())!=V.Flags||P->GetOffset_ForInternal()!=V.Offset||P->GetSize()!=V.Size||P->ArrayDim!=V.Dim||P->HasSetterOrGetter()||!SameMeta(P->GetMetaDataMap(),V.Metadata))return false;if(auto* B=CastField<FByteProperty>(P))if(!B->Enum.IsResolved()||B->Enum!=V.Enum)return false;}for(const auto& V:Enums){auto* E=V.E.Get();if(!Loaded(E)||!E->GetPathName().Equals(V.Path,ESearchCase::CaseSensitive)||E->GetFlags()!=V.Flags||E->GetPackage()!=V.Package.Get()||E->GetOuter()!=V.Package.Get()||!Loaded(V.Package.Get())||V.Package->GetPackageFlags()!=V.PackageFlags||V.Package->IsDirty()||E->NumEnums()!=V.Names.Num()||E->GetCppForm()!=V.Form||!SameMeta(FMetaData::GetMapForObject(E),V.Metadata))return false;for(int32 I=0;I<V.Names.Num();++I)if(E->GetNameByIndex(I)!=V.Names[I]||E->GetValueByIndex(I)!=V.Values[I])return false;}return true;};
    for(const auto& V:Roles)if(V.Enum!=nullptr){UEnum* E=V.Enum.Get();TStrongObjectPtr<UEnum> Held;if(Ready()&&Loaded(E))Held.Reset(E);if(!Current()||!Held.IsValid()||E->GetClass()!=UEnum::StaticClass()||(E->GetPathName()!=TEXT("/Script/Engine.ERoundingMode")&&E->GetPathName()!=TEXT("/Script/CoreUObject.ESearchCase"))||E->NumEnums()>32)return false;FEnumRole Q;Q.E=MoveTemp(Held);Q.Package.Reset(E->GetPackage());if(!Q.Package.IsValid())return false;Q.Path=E->GetPathName();Q.Flags=E->GetFlags();Q.PackageFlags=Q.Package->GetPackageFlags();Q.Form=E->GetCppForm();for(int32 I=0;I<E->NumEnums();++I){Q.Names.Add(E->GetNameByIndex(I));Q.Values.Add(E->GetValueByIndex(I));}if(const auto* M=FMetaData::GetMapForObject(E))Q.Metadata=*M;Enums.Add(MoveTemp(Q));if(!Current())return false;}
    if(!Current()||Inventory.Num()!=Count)return false;
    Row->SetBoolField(TEXT("exact_loaded_native_owner"),true);Row->SetBoolField(TEXT("exact_declared_function"),true);Row->SetStringField(TEXT("function_path"),FunctionPath);Row->SetStringField(TEXT("declaring_class_path"),OwnerPath);Row->SetStringField(TEXT("declaring_class_flags_string"),LexToString(ClassFlags));Row->SetStringField(TEXT("declaring_package_flags_string"),LexToString(PackageFlags));Row->SetStringField(TEXT("function_flags_string"),LexToString(Flags));Row->SetBoolField(TEXT("native"),(Flags&FUNC_Native)!=0);Row->SetBoolField(TEXT("public"),(Flags&FUNC_Public)!=0);Row->SetBoolField(TEXT("blueprint_callable"),(Flags&FUNC_BlueprintCallable)!=0);Row->SetBoolField(TEXT("blueprint_pure"),(Flags&FUNC_BlueprintPure)!=0);Row->SetBoolField(TEXT("static"),(Flags&FUNC_Static)!=0);Row->SetBoolField(TEXT("const"),(Flags&FUNC_Const)!=0);Row->SetBoolField(TEXT("editor_only"),(Flags&FUNC_EditorOnly)!=0);Row->SetBoolField(TEXT("package_dirty"),Dirty);
    auto Meta=MakeShared<FJsonObject>();for(const auto& Pair:FunctionMeta)Meta->SetStringField(Pair.Key.ToString(),Pair.Value);Row->SetObjectField(IntegerComparison?TEXT("preset_integer_comparison_catalog_metadata"):TEXT("preset_normalization_catalog_metadata"),Meta);
    TArray<TSharedPtr<FJsonValue>> Parameters;for(const auto& V:Roles){auto P=DescribeProperty(V.P,Schema,0,false,Current);Parameters.Add(MakeShared<FJsonValueObject>(P));Row->SetArrayField(TEXT("parameters"),Parameters);if(!P->GetBoolField(TEXT("observation_complete"))||!Current())return false;const FString Key=TEXT("CPP_Default_")+V.Name.ToString();P->SetNumberField(TEXT("ordinal"),Parameters.Num()-1);P->SetBoolField(TEXT("cpp_default_metadata_present"),F->HasMetaData(*Key));P->SetStringField(TEXT("cpp_default_metadata"),F->GetMetaData(*Key));P->SetBoolField(TEXT("advanced_display"),(V.Flags&CPF_AdvancedDisplay)!=0);P->SetStringField(TEXT("k2_pin_direction"),!(V.Flags&CPF_ReturnParm)&&(!(V.Flags&CPF_OutParm)||(V.Flags&CPF_ReferenceParm))?TEXT("input"):TEXT("output"));P->SetBoolField(TEXT("stock_const_reference_default_ignored"),(V.Flags&(CPF_ConstParm|CPF_ReferenceParm))==(CPF_ConstParm|CPF_ReferenceParm)&&!F->HasMetaData(TEXT("AutoCreateRefTerm")));}
    Row->SetNumberField(TEXT("parameter_count_observed"),Parameters.Num());Row->SetNumberField(TEXT("native_num_parms"),Count);Row->SetBoolField(TEXT("observation_complete"),Current());return Current();
}

TSharedRef<FJsonObject> Observe(const UEdGraphSchema_K2* Schema)
{
    auto State = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Functions;
    bool Complete = true;
    for (const FTarget& Target : Targets)
    {
        const bool AssetSearchCatalog=(FString(Target.ClassPath)==TEXT("/Script/AssetRegistry.AssetRegistryHelpers")&&FString(Target.FunctionName)==TEXT("GetAssetRegistry"))||(FString(Target.ClassPath)==TEXT("/Script/AssetRegistry.AssetRegistry")&&(FString(Target.FunctionName)==TEXT("GetAssets")||FString(Target.FunctionName)==TEXT("GetAssetsByPackageName")))||(FString(Target.ClassPath)==TEXT("/Script/Engine.KismetSystemLibrary")&&FString(Target.FunctionName)==TEXT("MakeARFilter"));
        if(AssetSearchCatalog&&(!IsEditorGameThread()||IsPlaySessionActive()||GIsImportingT3D||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage())){State->SetArrayField(TEXT("functions"),Functions);State->SetBoolField(TEXT("observations_complete"),false);return State;}
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("requested_class_path"), Target.ClassPath);
        Row->SetStringField(TEXT("requested_function_name"), Target.FunctionName);
        if(IsNormalizationTarget(Target))
        {const bool Observed=DescribeNormalizationTarget(Target,Schema,Row);Functions.Add(MakeShared<FJsonValueObject>(Row));if(!Observed){State->SetArrayField(TEXT("functions"),Functions);State->SetBoolField(TEXT("observations_complete"),false);return State;}continue;}
        UClass* Owner = FindObject<UClass>(nullptr, Target.ClassPath);
        const bool OwnerValid = IsExactLoadedNativeClass(Owner, Target.ClassPath);
        Row->SetBoolField(TEXT("exact_loaded_native_owner"), OwnerValid);
        UFunction* Function = OwnerValid ? Owner->FindFunctionByName(FName(Target.FunctionName)) : nullptr;
        const bool FunctionValid = IsValid(Function) && Function->GetOuterUClass() == Owner
            && Function->GetName() == Target.FunctionName && Function->GetOutermost() == Owner->GetOutermost();
        Row->SetBoolField(TEXT("exact_declared_function"), FunctionValid);
        if (!FunctionValid) { Complete = false; Functions.Add(MakeShared<FJsonValueObject>(Row)); continue; }
        TStrongObjectPtr<UClass> AssetSearchOwner(AssetSearchCatalog?Owner:nullptr);TStrongObjectPtr<UFunction> AssetSearchFunction(AssetSearchCatalog?Function:nullptr);
        Row->SetStringField(TEXT("function_path"), Function->GetPathName());
        Row->SetStringField(TEXT("declaring_class_path"), Function->GetOuterUClass()->GetPathName());
        Row->SetStringField(TEXT("declaring_class_flags_string"), LexToString(static_cast<uint32>(Owner->GetClassFlags())));
        if(AssetSearchCatalog){auto Super=Owner->GetSuperStructIterator();++Super;Row->SetStringField(TEXT("declaring_super_class_path"),Super?(*Super)->GetPathName():FString());}
        else Row->SetStringField(TEXT("declaring_super_class_path"), Owner->GetSuperClass() ? Owner->GetSuperClass()->GetPathName() : FString());
        Row->SetStringField(TEXT("declaring_package_flags_string"), LexToString(static_cast<uint32>(Owner->GetOutermost()->GetPackageFlags())));
        const UObject* ExistingCDO=nullptr;TStrongObjectPtr<UObject> AssetSearchCDO;
        if(AssetSearchCatalog){
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
            const auto H=Owner->ClassDefaultObject;
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
            if(H.IsResolved()&&H!=nullptr){UObject* O=StaticFindObject(nullptr,nullptr,*H.GetPathName(),false);if(H==O&&IsValid(O)){AssetSearchCDO.Reset(O);ExistingCDO=O;}}
        }else ExistingCDO = Owner->GetDefaultObject(false);
        Row->SetStringField(TEXT("declaring_existing_cdo_path"), IsValid(ExistingCDO) ? ExistingCDO->GetPathName() : FString());
        Row->SetStringField(TEXT("function_flags_string"), LexToString(static_cast<uint32>(Function->FunctionFlags)));
        Row->SetBoolField(TEXT("native"), Function->HasAnyFunctionFlags(FUNC_Native));
        Row->SetBoolField(TEXT("public"), Function->HasAnyFunctionFlags(FUNC_Public));
        Row->SetBoolField(TEXT("blueprint_callable"), Function->HasAnyFunctionFlags(FUNC_BlueprintCallable));
        Row->SetBoolField(TEXT("blueprint_pure"), Function->HasAnyFunctionFlags(FUNC_BlueprintPure));
        Row->SetBoolField(TEXT("static"), Function->HasAnyFunctionFlags(FUNC_Static));
        Row->SetBoolField(TEXT("const"), Function->HasAnyFunctionFlags(FUNC_Const));
        Row->SetBoolField(TEXT("editor_only"), Function->HasAnyFunctionFlags(FUNC_EditorOnly));
        Row->SetBoolField(TEXT("package_dirty"), Owner->GetOutermost()->IsDirty());
        for (const TCHAR* Key : {TEXT("WorldContext"), TEXT("AutoCreateRefTerm"), TEXT("BlueprintInternalUseOnly"), TEXT("CustomThunk"), TEXT("Latent"), TEXT("DeprecatedFunction"), TEXT("ScriptName")})
        { Row->SetStringField(Key, Function->GetMetaData(Key)); }
        if (FString(Target.ClassPath) == TEXT("/Script/Engine.BlueprintMapLibrary"))
        {
            auto Metadata = MakeShared<FJsonObject>();
            for (const TCHAR* Key : {TEXT("Category"), TEXT("CustomThunk"), TEXT("DisplayName"), TEXT("CompactNodeTitle"), TEXT("MapParam"), TEXT("MapKeyParam"), TEXT("MapValueParam"), TEXT("AutoCreateRefTerm"), TEXT("BlueprintThreadSafe")})
            {
                Metadata->SetBoolField(FString(Key) + TEXT("_present"), Function->HasMetaData(Key));
                Metadata->SetStringField(Key, Function->GetMetaData(Key));
            }
            Metadata->SetBoolField(TEXT("class_BlueprintThreadSafe_present"), Owner->HasMetaData(TEXT("BlueprintThreadSafe")));
            Metadata->SetStringField(TEXT("class_BlueprintThreadSafe"), Owner->GetMetaData(TEXT("BlueprintThreadSafe")));
            Row->SetObjectField(TEXT("map_catalog_metadata"), Metadata);
        }
        if (Target.bChildActorCatalog)
        {
            auto Metadata = MakeShared<FJsonObject>();
            for (const TCHAR* Key : {TEXT("ScriptNoExport"), TEXT("InternalUseParam"), TEXT("DeterminesOutputType"),
                TEXT("ComponentClass"), TEXT("HidePin"), TEXT("DefaultToSelf"), TEXT("ScriptMethod"), TEXT("BlueprintInternalUseOnly")})
            {
                Metadata->SetBoolField(FString(Key) + TEXT("_present"), Function->HasMetaData(Key));
                Metadata->SetStringField(Key, Function->GetMetaData(Key));
            }
            Row->SetObjectField(TEXT("child_actor_catalog_metadata"), Metadata);
        }
        if (FString(Target.ClassPath) == TEXT("/Script/Engine.PrimitiveComponent") &&
            (FString(Target.FunctionName) == TEXT("SetCollisionEnabled") || FString(Target.FunctionName) == TEXT("SetGenerateOverlapEvents") || FString(Target.FunctionName) == TEXT("SetTranslucentSortPriority")))
        {
            auto Metadata = MakeShared<FJsonObject>();
            Metadata->SetBoolField(TEXT("BlueprintSetter_present"), Function->HasMetaData(TEXT("BlueprintSetter")));
            Metadata->SetStringField(TEXT("BlueprintSetter"), Function->GetMetaData(TEXT("BlueprintSetter")));
            Row->SetObjectField(TEXT("primitive_control_catalog_metadata"), Metadata);
        }
        if (FString(Target.ClassPath) == TEXT("/Script/Niagara.NiagaraComponent"))
        {
            auto Metadata = MakeShared<FJsonObject>();
            for (const TCHAR* Key : {TEXT("DisplayName"), TEXT("Keywords"), TEXT("BlueprintSetter")})
            {
                Metadata->SetBoolField(FString(Key) + TEXT("_present"), Function->HasMetaData(Key));
                Metadata->SetStringField(Key, Function->GetMetaData(Key));
            }
            Row->SetObjectField(TEXT("niagara_control_catalog_metadata"), Metadata);
        }
        if (Target.bInterpolationCatalog)
        {
            auto Metadata = MakeShared<FJsonObject>();
            for (const TCHAR* Key : {TEXT("ArrayParm"), TEXT("ArrayTypeDependentParams"), TEXT("ScriptMethod"), TEXT("Category"), TEXT("NativeMakeFunc"), TEXT("NativeBreakFunc")})
            {
                Metadata->SetBoolField(FString(Key) + TEXT("_present"), Function->HasMetaData(Key));
                Metadata->SetStringField(Key, Function->GetMetaData(Key));
            }
            Row->SetObjectField(TEXT("interpolation_catalog_metadata"), Metadata);
        }
        if (FString(Target.ClassPath)==TEXT("/Script/Niagara.NiagaraDataInterfaceArrayFunctionLibrary")
            && (FString(Target.FunctionName)==TEXT("SetNiagaraArrayPosition")||FString(Target.FunctionName)==TEXT("SetNiagaraArrayVector4")))
        {
            auto Metadata=MakeShared<FJsonObject>();
            for(const TCHAR* Key:{TEXT("Category"),TEXT("DisplayName"),TEXT("WorldContext"),TEXT("AutoCreateRefTerm"),TEXT("ArrayParm"),TEXT("ArrayTypeDependentParams"),TEXT("BlueprintThreadSafe"),TEXT("ScriptName"),TEXT("ScriptMethod"),TEXT("CompactNodeTitle"),TEXT("DefaultToSelf"),TEXT("HidePin"),TEXT("NativeMakeFunc"),TEXT("NativeBreakFunc"),TEXT("AdvancedDisplay")})
            {Metadata->SetBoolField(FString(Key)+TEXT("_present"),Function->HasMetaData(Key));Metadata->SetStringField(Key,Function->GetMetaData(Key));}
            Row->SetObjectField(TEXT("niagara_touch_catalog_metadata"),Metadata);
        }
        if ((FString(Target.ClassPath)==TEXT("/Script/Engine.KismetRenderingLibrary")&&FString(Target.FunctionName)==TEXT("CreateRenderTarget2D"))
            ||(FString(Target.ClassPath)==TEXT("/Script/Niagara.NiagaraComponent")&&FString(Target.FunctionName)==TEXT("SetVariableTextureRenderTarget")))
        {
            auto Metadata=MakeShared<FJsonObject>();
            for(const TCHAR* Key:{TEXT("Category"),TEXT("DisplayName"),TEXT("WorldContext"),TEXT("AutoCreateRefTerm"),TEXT("ArrayParm"),TEXT("ArrayTypeDependentParams"),TEXT("BlueprintThreadSafe"),TEXT("ScriptName"),TEXT("ScriptMethod"),TEXT("CompactNodeTitle"),TEXT("DefaultToSelf"),TEXT("HidePin"),TEXT("NativeMakeFunc"),TEXT("NativeBreakFunc"),TEXT("AdvancedDisplay"),TEXT("UnsafeDuringActorConstruction"),TEXT("DeterminesOutputType")})
            {Metadata->SetBoolField(FString(Key)+TEXT("_present"),Function->HasMetaData(Key));Metadata->SetStringField(Key,Function->GetMetaData(Key));}
            Row->SetObjectField(TEXT("render_target_catalog_metadata"),Metadata);
        }
        if ((FString(Target.ClassPath)==TEXT("/Script/Niagara.NiagaraComponent")&&FString(Target.FunctionName)==TEXT("SetAsset"))
            ||(FString(Target.ClassPath)==TEXT("/Script/Engine.ActorComponent")&&(FString(Target.FunctionName)==TEXT("Activate")||FString(Target.FunctionName)==TEXT("Deactivate"))))
        {
            auto Metadata=MakeShared<FJsonObject>();
            for(const TCHAR* Key:{TEXT("Category"),TEXT("DisplayName"),TEXT("WorldContext"),TEXT("AutoCreateRefTerm"),TEXT("ArrayParm"),TEXT("ArrayTypeDependentParams"),TEXT("BlueprintThreadSafe"),TEXT("ScriptName"),TEXT("ScriptMethod"),TEXT("CompactNodeTitle"),TEXT("DefaultToSelf"),TEXT("HidePin"),TEXT("NativeMakeFunc"),TEXT("NativeBreakFunc"),TEXT("AdvancedDisplay"),TEXT("UnsafeDuringActorConstruction"),TEXT("DeterminesOutputType")})
            {Metadata->SetBoolField(FString(Key)+TEXT("_present"),Function->HasMetaData(Key));Metadata->SetStringField(Key,Function->GetMetaData(Key));}
            Row->SetObjectField(TEXT("niagara_lifecycle_catalog_metadata"),Metadata);
        }
        if ((FString(Target.ClassPath)==TEXT("/Script/Engine.PlayerController")&&FString(Target.FunctionName)==TEXT("GetHitResultUnderFingerByChannel"))
            ||(FString(Target.ClassPath)==TEXT("/Script/Engine.GameplayStatics")&&FString(Target.FunctionName)==TEXT("BreakHitResult")))
        {
            auto Metadata=MakeShared<FJsonObject>();
            for(const TCHAR* Key:{TEXT("Category"),TEXT("DisplayName"),TEXT("Keywords"),TEXT("WorldContext"),TEXT("AutoCreateRefTerm"),TEXT("ArrayParm"),TEXT("ArrayTypeDependentParams"),TEXT("BlueprintThreadSafe"),TEXT("ScriptName"),TEXT("ScriptMethod"),TEXT("CompactNodeTitle"),TEXT("DefaultToSelf"),TEXT("HidePin"),TEXT("NativeMakeFunc"),TEXT("NativeBreakFunc"),TEXT("AdvancedDisplay"),TEXT("UnsafeDuringActorConstruction"),TEXT("DeterminesOutputType"),TEXT("bTraceComplex")})
            {Metadata->SetBoolField(FString(Key)+TEXT("_present"),Function->HasMetaData(Key));Metadata->SetStringField(Key,Function->GetMetaData(Key));}
            Row->SetObjectField(TEXT("touch_trace_catalog_metadata"),Metadata);
        }
        if((FString(Target.ClassPath)==TEXT("/Script/AssetRegistry.AssetRegistryHelpers")&&FString(Target.FunctionName)==TEXT("GetAssetRegistry"))||(FString(Target.ClassPath)==TEXT("/Script/AssetRegistry.AssetRegistry")&&(FString(Target.FunctionName)==TEXT("GetAssets")||FString(Target.FunctionName)==TEXT("GetAssetsByPackageName")))||(FString(Target.ClassPath)==TEXT("/Script/Engine.KismetSystemLibrary")&&FString(Target.FunctionName)==TEXT("MakeARFilter")))
        {auto M=MakeShared<FJsonObject>();for(const TCHAR* K:{TEXT("Category"),TEXT("WorldContext"),TEXT("AutoCreateRefTerm"),TEXT("NativeMakeFunc"),TEXT("NativeBreakFunc"),TEXT("BlueprintThreadSafe"),TEXT("AdvancedDisplay"),TEXT("Keywords"),TEXT("CustomThunk"),TEXT("ArrayParm"),TEXT("ArrayTypeDependentParams"),TEXT("BlueprintInternalUseOnly"),TEXT("Latent"),TEXT("DeprecatedFunction"),TEXT("HideSelfPin"),TEXT("CannotImplementInterfaceInBlueprint")}){M->SetBoolField(FString(K)+TEXT("_present"),Function->HasMetaData(K));M->SetStringField(K,Function->GetMetaData(K));}Row->SetObjectField(TEXT("asset_search_catalog_metadata"),M);Row->SetBoolField(TEXT("asset_search_runtime_query_executed"),false);}
        if((FString(Target.ClassPath)==TEXT("/Script/AssetRegistry.AssetRegistryHelpers")&&(FString(Target.FunctionName)==TEXT("CreateAssetData")||FString(Target.FunctionName)==TEXT("GetAsset")))
            ||(FString(Target.ClassPath)==TEXT("/Script/Engine.DataTableFunctionLibrary")&&FString(Target.FunctionName)==TEXT("GetDataTableRowNames")))
        {
            auto Metadata=MakeShared<FJsonObject>();for(const TCHAR* Key:{TEXT("Category"),TEXT("ScriptMethod"),TEXT("DisplayName"),TEXT("Keywords"),TEXT("WorldContext"),TEXT("AutoCreateRefTerm"),TEXT("ArrayParm"),TEXT("ArrayTypeDependentParams"),TEXT("BlueprintThreadSafe"),TEXT("ScriptName"),TEXT("CompactNodeTitle"),TEXT("DefaultToSelf"),TEXT("HidePin"),TEXT("NativeMakeFunc"),TEXT("NativeBreakFunc"),TEXT("AdvancedDisplay"),TEXT("UnsafeDuringActorConstruction"),TEXT("DeterminesOutputType")}){Metadata->SetBoolField(FString(Key)+TEXT("_present"),Function->HasMetaData(Key));Metadata->SetStringField(Key,Function->GetMetaData(Key));}
            Row->SetObjectField(TEXT("asset_data_catalog_metadata"),Metadata);
        }
        if((FString(Target.ClassPath)==TEXT("/Script/Engine.GameplayStatics")&&FString(Target.FunctionName)==TEXT("GetActorOfClass"))
            ||(FString(Target.ClassPath)==TEXT("/Script/Engine.PlayerController")&&FString(Target.FunctionName)==TEXT("SetViewTargetWithBlend")))
        {
            auto Metadata=MakeShared<FJsonObject>();for(const TCHAR* Key:{TEXT("Category"),TEXT("WorldContext"),TEXT("DeterminesOutputType"),TEXT("DynamicOutputParam"),TEXT("Keywords"),TEXT("ActorClass"),TEXT("DisplayName"),TEXT("ScriptMethod"),TEXT("ScriptName"),TEXT("HidePin"),TEXT("DefaultToSelf"),TEXT("AutoCreateRefTerm"),TEXT("AdvancedDisplay"),TEXT("UnsafeDuringActorConstruction")}){Metadata->SetBoolField(FString(Key)+TEXT("_present"),Function->HasMetaData(Key));Metadata->SetStringField(Key,Function->GetMetaData(Key));}Row->SetObjectField(TEXT("camera_view_catalog_metadata"),Metadata);
        }
        if((FString(Target.ClassPath)==TEXT("/Script/Engine.Actor")&&(FString(Target.FunctionName)==TEXT("GetComponentByClass")||FString(Target.FunctionName)==TEXT("GetComponentsByTag")))
            ||(FString(Target.ClassPath)==TEXT("/Script/Engine.ActorComponent")&&FString(Target.FunctionName)==TEXT("ComponentHasTag")))
        {
            auto Metadata=MakeShared<FJsonObject>();for(const TCHAR* Key:{TEXT("Category"),TEXT("ComponentClass"),TEXT("DeterminesOutputType"),TEXT("DynamicOutputParam"),TEXT("WorldContext"),TEXT("DisplayName"),TEXT("Keywords"),TEXT("ScriptMethod"),TEXT("ScriptName"),TEXT("HidePin"),TEXT("DefaultToSelf"),TEXT("AutoCreateRefTerm"),TEXT("ArrayParm"),TEXT("ArrayTypeDependentParams"),TEXT("AdvancedDisplay"),TEXT("UnsafeDuringActorConstruction")}){Metadata->SetBoolField(FString(Key)+TEXT("_present"),Function->HasMetaData(Key));Metadata->SetStringField(Key,Function->GetMetaData(Key));}Row->SetObjectField(TEXT("component_filter_catalog_metadata"),Metadata);
        }
        if((FString(Target.ClassPath)==TEXT("/Script/Engine.SceneComponent")&&(FString(Target.FunctionName)==TEXT("DoesSocketExist")||FString(Target.FunctionName)==TEXT("GetSocketLocation")))
            ||(FString(Target.ClassPath)==TEXT("/Script/Engine.SkinnedMeshComponent")&&FString(Target.FunctionName)==TEXT("GetParentBone")))
        {
            auto Metadata=MakeShared<FJsonObject>();for(const TCHAR* Key:{TEXT("Category"),TEXT("Keywords"),TEXT("WorldContext"),TEXT("DisplayName"),TEXT("ScriptMethod"),TEXT("ScriptName"),TEXT("HidePin"),TEXT("DefaultToSelf"),TEXT("AutoCreateRefTerm"),TEXT("ArrayParm"),TEXT("ArrayTypeDependentParams"),TEXT("DynamicOutputParam"),TEXT("DeterminesOutputType"),TEXT("AdvancedDisplay"),TEXT("UnsafeDuringActorConstruction"),TEXT("NativeMakeFunc"),TEXT("NativeBreakFunc")}){Metadata->SetBoolField(FString(Key)+TEXT("_present"),Function->HasMetaData(Key));Metadata->SetStringField(Key,Function->GetMetaData(Key));}Row->SetObjectField(TEXT("bone_socket_catalog_metadata"),Metadata);
        }
        if((FString(Target.ClassPath)==TEXT("/Script/Engine.KismetSystemLibrary")&&FString(Target.FunctionName)==TEXT("GetPathName"))
            ||(FString(Target.ClassPath)==TEXT("/Script/Engine.KismetMathLibrary")&&(FString(Target.FunctionName)==TEXT("InverseTransformLocation")||FString(Target.FunctionName)==TEXT("InverseTransformDirection")||FString(Target.FunctionName)==TEXT("Divide_VectorFloat"))))
        {
            auto Metadata=MakeShared<FJsonObject>();for(const TCHAR* Key:{TEXT("Category"),TEXT("DisplayName"),TEXT("Keywords"),TEXT("ScriptMethod"),TEXT("ScriptOperator"),TEXT("CompactNodeTitle"),TEXT("BlueprintThreadSafe"),TEXT("WorldContext"),TEXT("AutoCreateRefTerm"),TEXT("DefaultToSelf"),TEXT("HidePin"),TEXT("ArrayParm"),TEXT("ArrayTypeDependentParams"),TEXT("AdvancedDisplay"),TEXT("UnsafeDuringActorConstruction"),TEXT("NativeMakeFunc"),TEXT("NativeBreakFunc"),TEXT("BlueprintInternalUseOnly"),TEXT("CustomThunk"),TEXT("DeprecatedFunction")}){Metadata->SetBoolField(FString(Key)+TEXT("_present"),Function->HasMetaData(Key));Metadata->SetStringField(Key,Function->GetMetaData(Key));}Row->SetObjectField(TEXT("tracker_coordinate_catalog_metadata"),Metadata);
        }
        if (Target.bVector4MakeCatalog)
        {
            auto Metadata=MakeShared<FJsonObject>();
            for(const TCHAR* Key:{TEXT("Category"),TEXT("Keywords"),TEXT("NativeMakeFunc"),TEXT("NativeBreakFunc"),TEXT("ScriptName"),TEXT("ScriptMethod"),TEXT("DisplayName"),TEXT("CompactNodeTitle"),TEXT("BlueprintAutocast"),TEXT("WorldContext"),TEXT("AutoCreateRefTerm"),TEXT("HidePin"),TEXT("DefaultToSelf"),TEXT("AdvancedDisplay"),TEXT("DeterminesOutputType")})
            {Metadata->SetBoolField(FString(Key)+TEXT("_present"),Function->HasMetaData(Key));Metadata->SetStringField(Key,Function->GetMetaData(Key));}
            Row->SetObjectField(TEXT("vector4_make_catalog_metadata"),Metadata);
        }
        if (Target.bMaterialGraphCatalog)
        {
            auto Metadata=MakeShared<FJsonObject>();
            for(const TCHAR* Key:{TEXT("Category"),TEXT("ScriptName"),TEXT("ScriptMethod"),TEXT("DisplayName"),TEXT("Keywords"),TEXT("CompactNodeTitle"),TEXT("BlueprintAutocast"),TEXT("WorldContext"),TEXT("AutoCreateRefTerm"),TEXT("HidePin"),TEXT("DefaultToSelf"),TEXT("AdvancedDisplay"),TEXT("UnsafeDuringActorConstruction"),TEXT("DeterminesOutputType")})
            {
                Metadata->SetBoolField(FString(Key)+TEXT("_present"),Function->HasMetaData(Key));
                Metadata->SetStringField(Key,Function->GetMetaData(Key));
            }
            Row->SetObjectField(TEXT("material_graph_catalog_metadata"),Metadata);
        }
        if (Target.bActorMotionCatalog)
        {
            auto Metadata = MakeShared<FJsonObject>();
            for (const TCHAR* Key : {TEXT("ScriptName"),TEXT("ScriptMethod"),TEXT("Category"),TEXT("DisplayName"),TEXT("AdvancedDisplay"),TEXT("AutoCreateRefTerm"),TEXT("WorldContext"),TEXT("HidePin"),TEXT("DefaultToSelf"),TEXT("UnsafeDuringActorConstruction"),TEXT("DeterminesOutputType")})
            {
                Metadata->SetBoolField(FString(Key) + TEXT("_present"), Function->HasMetaData(Key));
                Metadata->SetStringField(Key, Function->GetMetaData(Key));
            }
            Row->SetObjectField(TEXT("actor_motion_catalog_metadata"), Metadata);
        }
        if ((FString(Target.ClassPath)==TEXT("/Script/Engine.SceneComponent") && (FString(Target.FunctionName)==TEXT("GetForwardVector")||FString(Target.FunctionName)==TEXT("GetRightVector")||FString(Target.FunctionName)==TEXT("GetUpVector")))
            || (FString(Target.ClassPath)==TEXT("/Script/Engine.ActorComponent") && FString(Target.FunctionName)==TEXT("AddTickPrerequisiteActor")))
        {
            auto Metadata=MakeShared<FJsonObject>();
            for(const TCHAR* Key:{TEXT("Category"),TEXT("Keywords"),TEXT("DisplayName"),TEXT("BlueprintSetter"),TEXT("WorldContext"),TEXT("DefaultToSelf"),TEXT("HidePin"),TEXT("ScriptName"),TEXT("ScriptMethod"),TEXT("AdvancedDisplay"),TEXT("CompactNodeTitle"),TEXT("UnsafeDuringActorConstruction")})
            {Metadata->SetBoolField(FString(Key)+TEXT("_present"),Function->HasMetaData(Key));Metadata->SetStringField(Key,Function->GetMetaData(Key));}
            Row->SetObjectField(TEXT("field_frame_catalog_metadata"),Metadata);
        }
        if (Target.bSceneCameraCatalog)
        {
            auto Metadata = MakeShared<FJsonObject>();
            for (const TCHAR* Key : {TEXT("ArrayParm"), TEXT("ArrayTypeDependentParams"), TEXT("ScriptMethod"), TEXT("Category"), TEXT("NativeMakeFunc"), TEXT("NativeBreakFunc"), TEXT("AdvancedDisplay"), TEXT("UnsafeDuringActorConstruction"), TEXT("DeterminesOutputType")})
            {
                Metadata->SetBoolField(FString(Key) + TEXT("_present"), Function->HasMetaData(Key));
                Metadata->SetStringField(Key, Function->GetMetaData(Key));
            }
            Row->SetObjectField(TEXT("scene_camera_catalog_metadata"), Metadata);
        }
        if (Target.bFacingCatalog)
        {
            auto Metadata = MakeShared<FJsonObject>();
            for (const TCHAR* Key : {TEXT("ArrayParm"), TEXT("ArrayTypeDependentParams"), TEXT("ScriptMethod"), TEXT("Category"), TEXT("NativeMakeFunc"), TEXT("NativeBreakFunc")})
            {
                Metadata->SetBoolField(FString(Key) + TEXT("_present"), Function->HasMetaData(Key));
                Metadata->SetStringField(Key, Function->GetMetaData(Key));
            }
            Row->SetObjectField(TEXT("facing_catalog_metadata"), Metadata);
        }
        if (Target.bPhysicsAuthoringCatalog)
        {
            auto Metadata = MakeShared<FJsonObject>();
            for (const TCHAR* Key : {TEXT("ArrayParm"), TEXT("ArrayTypeDependentParams"), TEXT("ScriptMethod"), TEXT("Category"), TEXT("NativeMakeFunc"), TEXT("NativeBreakFunc"), TEXT("UnsafeDuringActorConstruction")})
            {
                Metadata->SetBoolField(FString(Key) + TEXT("_present"), Function->HasMetaData(Key));
                Metadata->SetStringField(Key, Function->GetMetaData(Key));
            }
            Row->SetObjectField(TEXT("physics_authoring_catalog_metadata"), Metadata);
        }
        TArray<TSharedPtr<FJsonValue>> Parameters;
        int32 Count = 0;
        if(AssetSearchCatalog)
        {
            TArray<FProperty*> Fields;for(TFieldIterator<FProperty> It(Function,EFieldIterationFlags::None);It;++It)if(It->HasAnyPropertyFlags(CPF_Parm))Fields.Add(*It);
            if(Fields.Num()>128){State->SetArrayField(TEXT("functions"),Functions);State->SetBoolField(TEXT("observations_complete"),false);return State;}
    struct FParameterRole{FProperty* Property;FProperty* OwnerProperty;FFieldClass* Class;FName Name;FString Path;uint64 Flags;int32 Offset,Size,Dim;bool Accessor;FProperty* Child;FProperty* Value;TObjectPtr<UScriptStruct> Struct;TObjectPtr<UClass> Interface;};TArray<FParameterRole> ParameterRoles;
    auto RememberRole=[&](FProperty* P){if(!P)return false;FProperty* Child=nullptr;FProperty* Value=nullptr;if(auto* A=CastField<FArrayProperty>(P))Child=A->Inner;else if(auto* Set=CastField<FSetProperty>(P))Child=Set->ElementProp;else if(auto* M=CastField<FMapProperty>(P)){Child=M->KeyProp;Value=M->ValueProp;}const auto* SP=CastField<FStructProperty>(P);const auto* IP=CastField<FInterfaceProperty>(P);TObjectPtr<UScriptStruct> SH=SP?SP->Struct:TObjectPtr<UScriptStruct>();TObjectPtr<UClass> IH=IP?IP->InterfaceClass:TObjectPtr<UClass>();if(!SH.IsResolved()||!IH.IsResolved())return false;ParameterRoles.Add({P,P->GetOwnerProperty(),P->GetClass(),P->GetFName(),P->GetPathName(),uint64(P->GetPropertyFlags()),P->GetOffset_ForInternal(),P->GetSize(),P->ArrayDim,P->HasSetterOrGetter(),Child,Value,SH,IH});return true;};
    for(auto* P:Fields){if(!RememberRole(P)){State->SetArrayField(TEXT("functions"),Functions);State->SetBoolField(TEXT("observations_complete"),false);return State;}if(auto* A=CastField<FArrayProperty>(P)){if(!RememberRole(A->Inner)){State->SetArrayField(TEXT("functions"),Functions);State->SetBoolField(TEXT("observations_complete"),false);return State;}}else if(auto* Set=CastField<FSetProperty>(P)){if(!RememberRole(Set->ElementProp)){State->SetArrayField(TEXT("functions"),Functions);State->SetBoolField(TEXT("observations_complete"),false);return State;}}}
            const auto ParametersCurrent=[&](){if(!IsEditorGameThread()||GIsImportingT3D||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||!IsValid(Function)||Function!=AssetSearchFunction.Get())return false;TArray<FProperty*> Now;for(TFieldIterator<FProperty> It(Function,EFieldIterationFlags::None);It;++It)if(It->HasAnyPropertyFlags(CPF_Parm))Now.Add(*It);if(Now!=Fields)return false;for(const auto& V:ParameterRoles){FProperty* P=V.Property;if(P->GetOwnerStruct()!=Function||P->GetOwnerProperty()!=V.OwnerProperty||P->GetClass()!=V.Class||P->GetFName()!=V.Name||!P->GetPathName().Equals(V.Path,ESearchCase::CaseSensitive)||uint64(P->GetPropertyFlags())!=V.Flags||P->GetOffset_ForInternal()!=V.Offset||P->GetSize()!=V.Size||P->ArrayDim!=V.Dim||P->HasSetterOrGetter()!=V.Accessor)return false;if(auto* A=CastField<FArrayProperty>(P)){if(A->Inner!=V.Child)return false;}else if(auto* Set=CastField<FSetProperty>(P)){if(Set->ElementProp!=V.Child)return false;}else if(auto* M=CastField<FMapProperty>(P)){if(M->KeyProp!=V.Child||M->ValueProp!=V.Value)return false;}if(auto* SP=CastField<FStructProperty>(P)){if(!SP->Struct.IsResolved()||SP->Struct!=V.Struct)return false;}if(auto* IP=CastField<FInterfaceProperty>(P)){if(!IP->InterfaceClass.IsResolved()||IP->InterfaceClass!=V.Interface)return false;}}return true;};
            for(FProperty* Field:Fields)
            {
                const bool Input=!Field->HasAnyPropertyFlags(CPF_ReturnParm)&&(!Field->HasAnyPropertyFlags(CPF_OutParm)||Field->HasAnyPropertyFlags(CPF_ReferenceParm));const FString DefaultKey=TEXT("CPP_Default_")+Field->GetName();const bool DefaultPresent=Function->HasMetaData(*DefaultKey);const FString Default=Function->GetMetaData(*DefaultKey);const bool Ignored=Field->HasAllPropertyFlags(CPF_ConstParm|CPF_ReferenceParm)&&(!Function->HasMetaData(TEXT("AutoCreateRefTerm"))||CastField<FArrayProperty>(Field)||CastField<FSetProperty>(Field));
                auto Parameter=DescribeAssetSearchProperty(Field,Schema);Parameter->SetNumberField(TEXT("ordinal"),Count++);Parameter->SetBoolField(TEXT("cpp_default_metadata_present"),DefaultPresent);Parameter->SetStringField(TEXT("cpp_default_metadata"),Default);Parameter->SetStringField(TEXT("k2_pin_direction"),Input?TEXT("input"):TEXT("output"));Parameter->SetBoolField(TEXT("stock_const_reference_default_ignored"),Ignored);Parameters.Add(MakeShared<FJsonValueObject>(Parameter));
                if(!Parameter->GetBoolField(TEXT("observation_complete"))||!ParametersCurrent()){Row->SetNumberField(TEXT("parameter_count_observed"),Count);Row->SetArrayField(TEXT("parameters"),Parameters);Functions.Add(MakeShared<FJsonValueObject>(Row));State->SetArrayField(TEXT("functions"),Functions);State->SetBoolField(TEXT("observations_complete"),false);return State;}
            }
        }
        else
        {
        for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
        {
            if (++Count > 128) { Complete = false; break; }
            const bool TouchTraceCatalog=(FString(Target.ClassPath)==TEXT("/Script/Engine.PlayerController")&&FString(Target.FunctionName)==TEXT("GetHitResultUnderFingerByChannel"))
                ||(FString(Target.ClassPath)==TEXT("/Script/Engine.GameplayStatics")&&FString(Target.FunctionName)==TEXT("BreakHitResult"));
            auto Parameter = DescribeProperty(*It, Schema, 0, TouchTraceCatalog);
            const bool Input = !It->HasAnyPropertyFlags(CPF_ReturnParm)
                && (!It->HasAnyPropertyFlags(CPF_OutParm) || It->HasAnyPropertyFlags(CPF_ReferenceParm));
            Parameter->SetNumberField(TEXT("ordinal"), Count - 1);
            const FString DefaultKey = TEXT("CPP_Default_") + It->GetName();
            Parameter->SetBoolField(TEXT("cpp_default_metadata_present"), Function->HasMetaData(*DefaultKey));
            Parameter->SetStringField(TEXT("cpp_default_metadata"), Function->GetMetaData(*DefaultKey));
            Parameter->SetStringField(TEXT("k2_pin_direction"), Input ? TEXT("input") : TEXT("output"));
            Parameter->SetBoolField(TEXT("const_reference_default_ignored_without_autocreate_ref_term"),
                It->HasAllPropertyFlags(CPF_ConstParm | CPF_ReferenceParm) && !Function->HasMetaData(TEXT("AutoCreateRefTerm")));
            if (Target.bChildActorCatalog)
            {
                auto Metadata = MakeShared<FJsonObject>();
                Metadata->SetBoolField(TEXT("AllowAbstract_present"), It->HasMetaData(TEXT("AllowAbstract")));
                Metadata->SetStringField(TEXT("AllowAbstract"), It->GetMetaData(TEXT("AllowAbstract")));
                if (const FClassProperty* ClassProperty = CastField<FClassProperty>(*It))
                {
                    const UClass* MetaClass = ClassProperty->MetaClass.Get();
                    Complete &= IsValid(MetaClass);
                    Metadata->SetStringField(TEXT("meta_class_path"), IsValid(MetaClass) ? MetaClass->GetPathName() : FString());
                    Metadata->SetBoolField(TEXT("meta_class_valid"), IsValid(MetaClass));
                    if (IsValid(MetaClass))
                    {
                        Metadata->SetStringField(TEXT("meta_class_flags_string"), LexToString(static_cast<uint32>(MetaClass->GetClassFlags())));
                        Metadata->SetStringField(TEXT("meta_class_package_path"), MetaClass->GetOutermost()->GetName());
                        Metadata->SetStringField(TEXT("meta_class_package_flags_string"), LexToString(static_cast<uint32>(MetaClass->GetOutermost()->GetPackageFlags())));
                        Metadata->SetBoolField(TEXT("meta_class_package_dirty"), MetaClass->GetOutermost()->IsDirty());
                        Metadata->SetBoolField(TEXT("meta_class_native"), MetaClass->HasAnyClassFlags(CLASS_Native));
                        Metadata->SetBoolField(TEXT("meta_class_editor_only"), MetaClass->IsEditorOnly());
                    }
                }
                Parameter->SetObjectField(TEXT("child_actor_catalog_metadata"), Metadata);
            }
            bool Converted = false; Parameter->TryGetBoolField(TEXT("schema_conversion_success"), Converted);
            Complete &= Converted;
            if (CastField<FArrayProperty>(*It)) { Complete &= Parameter->GetBoolField(TEXT("array_inner_observation_complete")); }
            bool EnumComplete = true;
            if (Parameter->TryGetBoolField(TEXT("enum_observation_complete"), EnumComplete)) { Complete &= EnumComplete; }
            Parameters.Add(MakeShared<FJsonValueObject>(Parameter));
        }
        }
        Row->SetNumberField(TEXT("parameter_count_observed"), Count);
        Row->SetArrayField(TEXT("parameters"), Parameters);
        Functions.Add(MakeShared<FJsonValueObject>(Row));
    }
    State->SetArrayField(TEXT("functions"), Functions);

    auto PropertyRow = MakeShared<FJsonObject>();
    PropertyRow->SetStringField(TEXT("requested_class_path"), TEXT("/Script/Engine.Pawn"));
    PropertyRow->SetStringField(TEXT("requested_property_name"), TEXT("BaseEyeHeight"));
    UClass* PawnClass = FindObject<UClass>(nullptr, TEXT("/Script/Engine.Pawn"));
    const bool PawnValid = IsExactLoadedNativeClass(PawnClass, TEXT("/Script/Engine.Pawn"));
    FProperty* Eye = PawnValid ? PawnClass->FindPropertyByName(TEXT("BaseEyeHeight")) : nullptr;
    const bool EyeValid = Eye && Eye->GetOwnerClass() == PawnClass && Eye->GetName() == TEXT("BaseEyeHeight");
    PropertyRow->SetBoolField(TEXT("exact_loaded_native_owner"), PawnValid);
    PropertyRow->SetBoolField(TEXT("exact_declared_property"), EyeValid);
    if (EyeValid)
    {
        const auto Property = DescribeProperty(Eye, Schema);
        bool Converted = false; Property->TryGetBoolField(TEXT("schema_conversion_success"), Converted);
        Complete &= Converted;
        PropertyRow->SetObjectField(TEXT("property"), Property);
        PropertyRow->SetBoolField(TEXT("package_dirty"), PawnClass->GetOutermost()->IsDirty());
    }
    else { Complete = false; }
    State->SetObjectField(TEXT("pawn_base_eye_height"), PropertyRow);
    auto ChildActorRow = MakeShared<FJsonObject>();
    ChildActorRow->SetStringField(TEXT("requested_class_path"), TEXT("/Script/Engine.ChildActorComponent"));
    ChildActorRow->SetStringField(TEXT("requested_property_name"), TEXT("ChildActor"));
    UClass* ChildActorClass = FindObject<UClass>(nullptr, TEXT("/Script/Engine.ChildActorComponent"));
    const bool ChildActorOwnerValid = IsExactLoadedNativeClass(ChildActorClass, TEXT("/Script/Engine.ChildActorComponent"));
    FProperty* ChildActor = ChildActorOwnerValid ? ChildActorClass->FindPropertyByName(TEXT("ChildActor")) : nullptr;
    const bool ChildActorValid = ChildActor && ChildActor->GetOwnerClass() == ChildActorClass && ChildActor->GetName() == TEXT("ChildActor");
    ChildActorRow->SetBoolField(TEXT("exact_loaded_native_owner"), ChildActorOwnerValid);
    ChildActorRow->SetBoolField(TEXT("exact_declared_property"), ChildActorValid);
    if (ChildActorValid)
    {
        auto Property = DescribeProperty(ChildActor, Schema);
        bool Converted = false; Property->TryGetBoolField(TEXT("schema_conversion_success"), Converted);
        Complete &= Converted;
        Property->SetBoolField(TEXT("AllowPrivateAccess_present"), ChildActor->HasMetaData(TEXT("AllowPrivateAccess")));
        Property->SetStringField(TEXT("AllowPrivateAccess"), ChildActor->GetMetaData(TEXT("AllowPrivateAccess")));
        Property->SetBoolField(TEXT("native_access_specifier_private"), ChildActor->HasAnyPropertyFlags(CPF_NativeAccessSpecifierPrivate));
        ChildActorRow->SetObjectField(TEXT("property"), Property);
        ChildActorRow->SetStringField(TEXT("declaring_class_path"), ChildActorClass->GetPathName());
        ChildActorRow->SetStringField(TEXT("declaring_class_flags_string"), LexToString(static_cast<uint32>(ChildActorClass->GetClassFlags())));
        ChildActorRow->SetStringField(TEXT("declaring_super_class_path"), ChildActorClass->GetSuperClass() ? ChildActorClass->GetSuperClass()->GetPathName() : FString());
        ChildActorRow->SetStringField(TEXT("declaring_package_flags_string"), LexToString(static_cast<uint32>(ChildActorClass->GetOutermost()->GetPackageFlags())));
        const UObject* ExistingCDO = ChildActorClass->GetDefaultObject(false);
        ChildActorRow->SetStringField(TEXT("declaring_existing_cdo_path"), IsValid(ExistingCDO) ? ExistingCDO->GetPathName() : FString());
        ChildActorRow->SetBoolField(TEXT("package_dirty"), ChildActorClass->GetOutermost()->IsDirty());
    }
    else { Complete = false; }
    State->SetObjectField(TEXT("child_actor_component_child_actor"), ChildActorRow);
    // KN3: fixed already-loaded native declarations only. No value export,
    // getters, setters, CDO creation or class/asset load is requested here.
    TArray<TSharedPtr<FJsonValue>> NativeStorageSchemas;
    for (const TCHAR* ClassPath : {TEXT("/Script/Niagara.NiagaraComponent"), TEXT("/Script/Engine.BoxComponent"), TEXT("/Script/Engine.SceneComponent"), TEXT("/Script/Engine.StaticMeshComponent")})
    {
        auto ClassRow = MakeShared<FJsonObject>(); ClassRow->SetStringField(TEXT("class_path"), ClassPath);
        UClass* StorageClass = FindObject<UClass>(nullptr, ClassPath);
        ClassRow->SetBoolField(TEXT("already_loaded"), IsValid(StorageClass));
        TArray<TSharedPtr<FJsonValue>> StorageFields; int32 StorageCount = 0; bool StorageComplete = IsValid(StorageClass);
        if (StorageComplete) for (TFieldIterator<FProperty> It(StorageClass); It; ++It)
        {
            if (++StorageCount > 2048) { StorageComplete = false; break; }
            const FProperty* Field = *It; auto FieldRow = MakeShared<FJsonObject>();
            FieldRow->SetStringField(TEXT("property"), Field->GetPathName());
            FieldRow->SetStringField(TEXT("type"), Field->GetClass()->GetName());
            FieldRow->SetNumberField(TEXT("ordinal"), StorageCount); FieldRow->SetNumberField(TEXT("array_dim"), Field->ArrayDim);
            FieldRow->SetNumberField(TEXT("offset"), Field->GetOffset_ForInternal()); FieldRow->SetNumberField(TEXT("element_size"), Field->ElementSize);
            FieldRow->SetBoolField(TEXT("has_getter"), Field->HasGetter()); FieldRow->SetBoolField(TEXT("has_setter"), Field->HasSetter());
            FieldRow->SetStringField(TEXT("flags"), LexToString(static_cast<uint64>(Field->GetPropertyFlags())));
            StorageFields.Add(MakeShared<FJsonValueObject>(FieldRow));
        }
        ClassRow->SetBoolField(TEXT("complete"), StorageComplete); ClassRow->SetArrayField(TEXT("fields"), StorageFields);
        NativeStorageSchemas.Add(MakeShared<FJsonValueObject>(ClassRow));
    }
    State->SetArrayField(TEXT("native_component_storage_schemas"), NativeStorageSchemas);
    State->SetBoolField(TEXT("observations_complete"), Complete);
    return State;
}
}

Function::FResult InspectCaptureSignatures()
{
    Function::FResult R;
    const FEngineVersion& Version = FEngineVersion::Current();
    R.Data->SetStringField(TEXT("engine_version"), Version.ToString());
    R.Data->SetStringField(TEXT("engine_directory"), FPaths::ConvertRelativePathToFull(FPaths::EngineDir()));
    R.Data->SetStringField(TEXT("project_file"), FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()));
    R.Data->SetNumberField(TEXT("process_id"), FPlatformProcess::GetCurrentProcessId());
    R.Data->SetStringField(TEXT("scope"), TEXT("one hundred and thirty-five finite exact already-loaded Engine/Niagara/AssetRegistry native functions, Pawn.BaseEyeHeight and ChildActorComponent.ChildActor; prior one hundred and thirty-one target rows retained, exact GetPathName/InverseTransformLocation/InverseTransformDirection/Divide_VectorFloat appended as metadata only; target-specific metadata, depth-one array inner metadata, bounded enum values, CPP defaults, raw flags and public schema type/direction evidence, not invocation or compatibility approval"));
    R.Data->SetBoolField(TEXT("asset_selectors_accepted"), false);
    R.Data->SetBoolField(TEXT("object_or_cdo_creation_requested"), false);
    R.Data->SetBoolField(TEXT("native_function_invocation_requested"), false);
    R.Data->SetBoolField(TEXT("asset_load_requested"), false);
    if (!IsEditorGameThread() || IsPlaySessionActive())
    { Fail(R, TEXT("engine_signature_editor_state_invalid"), TEXT("Requires the Editor game thread outside PIE/SIE.")); return R; }
    if (Version.GetMajor() != 5 || Version.GetMinor() != 8)
    { Fail(R, TEXT("engine_signature_version_unsupported"), TEXT("This evidence adapter is scoped to installed UE 5.8.x.")); return R; }
    UClass* SchemaClass = FindObject<UClass>(nullptr, TEXT("/Script/BlueprintGraph.EdGraphSchema_K2"));
    const UEdGraphSchema_K2* Schema = IsValid(SchemaClass)
        ? Cast<UEdGraphSchema_K2>(SchemaClass->GetDefaultObject(false)) : nullptr;
    if (!Schema || Schema->GetClass() != SchemaClass)
    { Fail(R, TEXT("engine_signature_schema_not_loaded"), TEXT("Requires an already-loaded exact standard K2 schema CDO; this read never creates it.")); return R; }
    const auto Before = Observe(Schema);
    const auto After = Observe(Schema);
    R.Data->SetObjectField(TEXT("before"), Before); R.Data->SetObjectField(TEXT("after"), After);
    R.bChanged = Text(Before) != Text(After);
    R.Data->SetBoolField(TEXT("observed_native_state_preserved"), !R.bChanged);
    if (R.bChanged)
    { R.Stage = TEXT("verification"); Fail(R, TEXT("engine_signature_read_state_changed"), TEXT("Finite native signature/property/package observations changed during the read; no caller modification was requested.")); return R; }
    if (!Before->GetBoolField(TEXT("observations_complete")) || !After->GetBoolField(TEXT("observations_complete")))
    { Fail(R, TEXT("engine_signature_observation_incomplete"), TEXT("At least one exact loaded function/property or bounded schema conversion was unavailable; inspect partial observations.")); return R; }
    R.bSuccess = R.bPostconditionPassed = true; R.Stage = TEXT("complete"); return R;
}
}
