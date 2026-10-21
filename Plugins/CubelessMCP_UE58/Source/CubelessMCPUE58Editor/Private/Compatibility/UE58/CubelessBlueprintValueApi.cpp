// DC original Compare8 and Literal8, mapped Engine CallFunction nodes with original late literal cleanup.
#include "Compatibility/UE58/CubelessBlueprintValueApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessFunctionPinApi.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "String/LexFromString.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include <cfloat>
#include <initializer_list>
namespace Cubeless::UE58::BlueprintValue
{
namespace
{
using FResult=Function::FResult;
void Count(FResult& R,const TCHAR* Key) { R.Data->SetNumberField(Key,R.Data->GetNumberField(Key)+1); }
bool Fail(FResult& R,const TCHAR* Code,const FString& Error) { R.bSuccess=R.bPostconditionPassed=false;R.ErrorCode=Code;R.Error=Error;return false; }
bool Loaded(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
bool SafeToken(const FString& S) { if(S.Len()>512) { return false; }for(int32 I=0;I<S.Len();++I) { if(!S[I]) { return false; } }return true; }
void Init(FResult& R)
{
    for(const TCHAR* K:{TEXT("outer_transaction_calls"),TEXT("operation_mapping_calls"),TEXT("shared_graph_resolve_calls"),TEXT("shared_graph_resolve_returns"),TEXT("external_scope_observation_calls"),TEXT("mapped_function_lookup_calls"),TEXT("mapped_function_lookup_returns"),TEXT("signature_observation_calls"),TEXT("signature_parameter_observation_calls"),TEXT("node_allocation_calls"),TEXT("set_from_function_calls"),TEXT("graph_add_node_calls"),TEXT("new_guid_calls"),TEXT("post_placed_calls"),TEXT("allocate_pins_calls"),TEXT("literal_default_conversion_calls"),TEXT("try_set_default_calls"),TEXT("try_set_default_returns"),TEXT("default_validation_calls"),TEXT("default_match_calls"),TEXT("pin_default_changed_calls"),TEXT("pin_default_changed_returns"),TEXT("destroy_node_calls"),TEXT("destroy_node_returns"),TEXT("mark_blueprint_modified_calls"),TEXT("mark_blueprint_modified_returns"),TEXT("legacy_node_metadata_observation_calls"),TEXT("explicit_modify_calls"),TEXT("explicit_compile_calls"),TEXT("explicit_save_calls"),TEXT("float_fallback_lookup_calls"),TEXT("float_fallback_lookup_returns"),TEXT("float_default_observation_calls"),TEXT("float_default_parse_calls")}) { R.Data->SetNumberField(K,0); }
    R.Data->SetStringField(TEXT("actual_node_family"),TEXT("UK2Node_CallFunction"));
    R.Data->SetStringField(TEXT("lifecycle_scope"),TEXT("Original direct mapped Engine node creation and late literal value validation/only-new-node retirement. Shared owned graph resolution may create a requested function graph and regenerate skeleton; no explicit full compile/save here. MarkModified cache/status/dirty/child/editor effects are not a zero-effect or atomic rollback proof."));
    R.Data->SetStringField(TEXT("position_scope"),TEXT("Original bounded Any origin/first-two/CSV fallback through float32 and checked int32 assignment."));
    R.Data->SetBoolField(TEXT("full_original_or_runtime_verified"),false);R.Data->SetBoolField(TEXT("runtime_value_invocation_verified"),false);R.Data->SetBoolField(TEXT("automatic_retry_performed"),false);
}
FString NormalizeTypeName(const FString& TypeName)
{
    FString Normalized = TypeName.ToLower();
    Normalized.TrimStartAndEndInline();
    Normalized.ReplaceInline(TEXT(" "), TEXT(""));
    Normalized.ReplaceInline(TEXT("_"), TEXT(""));
    Normalized.ReplaceInline(TEXT("-"), TEXT(""));
    return Normalized;
}

bool IsOperationAlias(const FString& Operation, std::initializer_list<const TCHAR*> Aliases)
{
    FString NormalizedOperation = Operation.ToLower();
    NormalizedOperation.TrimStartAndEndInline();
    NormalizedOperation.ReplaceInline(TEXT(" "), TEXT("_"));
    NormalizedOperation.ReplaceInline(TEXT("-"), TEXT("_"));

    for (const TCHAR* Alias : Aliases)
    {
        FString NormalizedAlias(Alias);
        NormalizedAlias.ToLowerInline();
        NormalizedAlias.ReplaceInline(TEXT(" "), TEXT("_"));
        NormalizedAlias.ReplaceInline(TEXT("-"), TEXT("_"));
        if (NormalizedOperation == NormalizedAlias)
        {
            return true;
        }
    }

    return false;
}

FName GetCompareFunctionName(const FString& Operation, const FString& ValueType, FString& OutError)
{
    const FString NormalizedType = NormalizeTypeName(ValueType);
    const bool bLess = IsOperationAlias(Operation, {TEXT("less"), TEXT("lt"), TEXT("<")});
    const bool bGreater = IsOperationAlias(Operation, {TEXT("greater"), TEXT("gt"), TEXT(">")});
    const bool bLessEqual = IsOperationAlias(Operation, {TEXT("less_equal"), TEXT("lessequal"), TEXT("le"), TEXT("<=")});
    const bool bGreaterEqual = IsOperationAlias(Operation, {TEXT("greater_equal"), TEXT("greaterequal"), TEXT("ge"), TEXT(">=")});
    const bool bEqual = IsOperationAlias(Operation, {TEXT("equal"), TEXT("equals"), TEXT("eq"), TEXT("==")});
    const bool bNotEqual = IsOperationAlias(Operation, {TEXT("not_equal"), TEXT("notequal"), TEXT("ne"), TEXT("!=")});

    if (!bLess && !bGreater && !bLessEqual && !bGreaterEqual && !bEqual && !bNotEqual)
    {
        OutError = FString::Printf(TEXT("Unsupported compare operation: %s"), *Operation);
        return NAME_None;
    }

    if (NormalizedType == TEXT("int") || NormalizedType == TEXT("integer"))
    {
        if (bLess) { return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Less_IntInt); }
        if (bGreater) { return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Greater_IntInt); }
        if (bLessEqual) { return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, LessEqual_IntInt); }
        if (bGreaterEqual) { return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, GreaterEqual_IntInt); }
        if (bEqual) { return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, EqualEqual_IntInt); }
        return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, NotEqual_IntInt);
    }
    if (NormalizedType == TEXT("int64") || NormalizedType == TEXT("integer64"))
    {
        if (bLess) { return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Less_Int64Int64); }
        if (bGreater) { return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Greater_Int64Int64); }
        if (bLessEqual) { return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, LessEqual_Int64Int64); }
        if (bGreaterEqual) { return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, GreaterEqual_Int64Int64); }
        if (bEqual) { return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, EqualEqual_Int64Int64); }
        return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, NotEqual_Int64Int64);
    }
    if (NormalizedType == TEXT("float") || NormalizedType == TEXT("double") || NormalizedType == TEXT("real"))
    {
        if (bLess) { return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Less_DoubleDouble); }
        if (bGreater) { return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Greater_DoubleDouble); }
        if (bLessEqual) { return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, LessEqual_DoubleDouble); }
        if (bGreaterEqual) { return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, GreaterEqual_DoubleDouble); }
        if (bEqual) { return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, EqualEqual_DoubleDouble); }
        return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, NotEqual_DoubleDouble);
    }
    if ((NormalizedType == TEXT("byte") || NormalizedType == TEXT("enum") || NormalizedType == TEXT("byteenum")) && (bEqual || bNotEqual))
    {
        return bEqual
            ? GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, EqualEqual_ByteByte)
            : GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, NotEqual_ByteByte);
    }

    if (bEqual || bNotEqual)
    {
        if (NormalizedType == TEXT("bool") || NormalizedType == TEXT("boolean"))
        {
            return bEqual
                ? GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, EqualEqual_BoolBool)
                : GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, NotEqual_BoolBool);
        }
        if (NormalizedType == TEXT("object"))
        {
            return bEqual
                ? GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, EqualEqual_ObjectObject)
                : GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, NotEqual_ObjectObject);
        }
        if (NormalizedType == TEXT("class"))
        {
            return bEqual
                ? GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, EqualEqual_ClassClass)
                : GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, NotEqual_ClassClass);
        }
        if (NormalizedType == TEXT("name"))
        {
            return bEqual
                ? GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, EqualEqual_NameName)
                : GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, NotEqual_NameName);
        }
    }

    OutError = FString::Printf(TEXT("Unsupported compare type/operation combination: type=%s, operation=%s"), *ValueType, *Operation);
    return NAME_None;
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FName GetLiteralFunctionName(const FString& LiteralType, FString& OutError)
{
    const FString NormalizedType = NormalizeTypeName(LiteralType);
    if (NormalizedType == TEXT("int") || NormalizedType == TEXT("integer"))
    {
        return GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, MakeLiteralInt);
    }
    if (NormalizedType == TEXT("int64") || NormalizedType == TEXT("integer64"))
    {
        return GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, MakeLiteralInt64);
    }
    if (NormalizedType == TEXT("float") || NormalizedType == TEXT("single"))
    {
        return GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, MakeLiteralFloat);
    }
    if (NormalizedType == TEXT("double") || NormalizedType == TEXT("real"))
    {
        return GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, MakeLiteralDouble);
    }
    if (NormalizedType == TEXT("bool") || NormalizedType == TEXT("boolean"))
    {
        return GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, MakeLiteralBool);
    }
    if (NormalizedType == TEXT("name"))
    {
        return GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, MakeLiteralName);
    }
    if (NormalizedType == TEXT("byte"))
    {
        return GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, MakeLiteralByte);
    }
    if (NormalizedType == TEXT("string"))
    {
        return GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, MakeLiteralString);
    }
    if (NormalizedType == TEXT("text"))
    {
        return GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, MakeLiteralText);
    }

    OutError = FString::Printf(TEXT("Unsupported literal type: %s"), *LiteralType);
    return NAME_None;
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

bool Position(const FString& Json,int32 (&XY)[2],FString& Projected,FResult& R)
{
    TSharedPtr<FJsonValue> V;if(!Function::Pin::ParseJson(Json,V,R)) { return false; }
    double Values[2]={0,0};
    if(V->Type==EJson::Array&&V->AsArray().Num()>=2)
    { for(int32 I=0;I<2;++I) { V->AsArray()[I]->TryGetNumber(Values[I]); } }
    else if(V->Type==EJson::String)
    {
        FString S=V->AsString();S.TrimStartAndEndInline();S.RemoveFromStart(TEXT("["));S.RemoveFromEnd(TEXT("]"));S.RemoveFromStart(TEXT("("));S.RemoveFromEnd(TEXT(")"));
        TArray<FString> Parts;S.ParseIntoArray(Parts,TEXT(","),true);if(Parts.Num()>=2) { Values[0]=FCString::Atof(*Parts[0].TrimStartAndEnd());Values[1]=FCString::Atof(*Parts[1].TrimStartAndEnd()); }
    }
    TArray<TSharedPtr<FJsonValue>> Original,FloatRows,IntRows;
    for(int32 I=0;I<2;++I)
    {
        if(!FMath::IsFinite(Values[I])||FMath::Abs(Values[I])>FLT_MAX) { return Fail(R,TEXT("value_position_invalid"),TEXT("Finite float32 position required before graph writer.")); }
        const float F=static_cast<float>(Values[I]);const double D=static_cast<double>(F);
        if(!FMath::IsFinite(F)||D<static_cast<double>(MIN_int32)||D>static_cast<double>(MAX_int32)) { return Fail(R,TEXT("value_position_invalid"),TEXT("Original float32 position exceeds safe int32 assignment budget.")); }
        XY[I]=static_cast<int32>(F);Original.Add(MakeShared<FJsonValueNumber>(Values[I]));FloatRows.Add(MakeShared<FJsonValueNumber>(D));IntRows.Add(MakeShared<FJsonValueNumber>(XY[I]));
    }
    R.Data->SetField(TEXT("requested_position"),V);R.Data->SetArrayField(TEXT("position_numeric_prefix"),Original);R.Data->SetArrayField(TEXT("position_float32"),FloatRows);R.Data->SetArrayField(TEXT("position_int32"),IntRows);
    Projected=FString::Printf(TEXT("[%d,%d]"),XY[0],XY[1]);return true;
}

TSharedRef<FJsonObject> ResultJson(const FResult& R)
{
    auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),R.bSuccess);J->SetBoolField(TEXT("mutation_attempted"),R.bMutationAttempted);
    if(R.bChangeStateKnown) { J->SetBoolField(TEXT("changed"),R.bChanged); }else { J->SetField(TEXT("changed"),MakeShared<FJsonValueNull>()); }
    J->SetBoolField(TEXT("change_state_known"),R.bChangeStateKnown);J->SetBoolField(TEXT("postcondition_passed"),R.bPostconditionPassed);J->SetStringField(TEXT("stage"),R.Stage);J->SetStringField(TEXT("error_code"),R.ErrorCode);J->SetStringField(TEXT("error"),R.Error);J->SetObjectField(TEXT("data"),R.Data);return J;
}

bool Equal(const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B)
{
    if(!A.IsValid()||!B.IsValid()||A->Type!=B->Type) { return false; }
    switch(A->Type)
    {
    case EJson::Null:return true;case EJson::Boolean:return A->AsBool()==B->AsBool();case EJson::Number:return A->AsNumber()==B->AsNumber();case EJson::String:return A->AsString().Equals(B->AsString(),ESearchCase::CaseSensitive);
    case EJson::Array:{const auto& X=A->AsArray();const auto& Y=B->AsArray();if(X.Num()!=Y.Num()) { return false; }for(int32 I=0;I<X.Num();++I) { if(!Equal(X[I],Y[I])) { return false; } }return true;}
    case EJson::Object:{const auto X=A->AsObject(),Y=B->AsObject();if(!X.IsValid()||!Y.IsValid()||X->Values.Num()!=Y->Values.Num()) { return false; }for(const auto& P:X->Values) { const auto* V=Y->Values.Find(P.Key);if(!V||!Equal(P.Value,*V)) { return false; } }return true;}
    default:return false;
    }
}

bool ExistingNodesPreserved(const TSharedPtr<FJsonObject>& Before,const TSharedPtr<FJsonObject>& After,int32 Delta)
{
    const auto& A=Before->GetArrayField(TEXT("nodes"));const auto& B=After->GetArrayField(TEXT("nodes"));
    if(B.Num()!=A.Num()+Delta) { return false; }for(int32 I=0;I<A.Num();++I) { if(!Equal(A[I],B[I])) { return false; } }return true;
}

bool GraphRowsPreserved(const TSharedPtr<FJsonObject>& Before,const TSharedPtr<FJsonObject>& After,const FString& Selected,int32 Delta)
{
    const auto& A=Before->GetArrayField(TEXT("graphs"));const auto& B=After->GetArrayField(TEXT("graphs"));if(A.Num()!=B.Num()) { return false; }int32 SelectedCount=0;
    for(int32 I=0;I<A.Num();++I)
    { const auto Expected=MakeShared<FJsonObject>(*A[I]->AsObject());if(Expected->GetStringField(TEXT("object_path")).Equals(Selected,ESearchCase::CaseSensitive))
      { ++SelectedCount;Expected->SetNumberField(TEXT("node_count"),Expected->GetNumberField(TEXT("node_count"))+Delta); }
      if(!Equal(MakeShared<FJsonValueObject>(Expected),B[I])) { return false; } }return SelectedCount==1;
}

void LegacyNodeFields(UEdGraphNode* Node,FResult& R)
{
    Count(R,TEXT("legacy_node_metadata_observation_calls"));R.Data->SetStringField(TEXT("name"),Node->GetName());R.Data->SetStringField(TEXT("class"),Node->GetClass()->GetName());
    R.Data->SetStringField(TEXT("title"),Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());R.Data->SetStringField(TEXT("node_comment"),Node->NodeComment);
    R.Data->SetBoolField(TEXT("comment_bubble_visible"),Node->bCommentBubbleVisible);R.Data->SetBoolField(TEXT("comment_bubble_pinned"),Node->bCommentBubblePinned);
}
struct FSignature
{
    TMap<FName,FEdGraphPinType> Inputs,Outputs;
    TSharedRef<FJsonObject> Json=MakeShared<FJsonObject>();
};
bool PropertyFits(const FProperty* P,const FString& Type)
{
    const FString T=NormalizeTypeName(Type);
    if(T==TEXT("bool")||T==TEXT("boolean")) { return CastField<FBoolProperty>(P)!=nullptr; }
    if(T==TEXT("int")||T==TEXT("integer")) { return CastField<FIntProperty>(P)!=nullptr; }
    if(T==TEXT("int64")||T==TEXT("integer64")) { return CastField<FInt64Property>(P)!=nullptr; }
    if(T==TEXT("double")||T==TEXT("real")) { return CastField<FDoubleProperty>(P)!=nullptr; }
    if(T==TEXT("float")||T==TEXT("single")) { return CastField<FFloatProperty>(P)!=nullptr; }
    if(T==TEXT("byte")||T==TEXT("enum")||T==TEXT("byteenum"))
    { const auto* B=CastField<FByteProperty>(P);return B&&B->GetIntPropertyEnum()==nullptr; }
    if(T==TEXT("name")) { return CastField<FNameProperty>(P)!=nullptr; }
    if(T==TEXT("string")) { return CastField<FStrProperty>(P)!=nullptr; }
    if(T==TEXT("text")) { return CastField<FTextProperty>(P)!=nullptr; }
    if(T==TEXT("class"))
    { const auto* C=CastField<FClassProperty>(P);return C&&C->MetaClass==UObject::StaticClass()&&C->PropertyClass==UClass::StaticClass(); }
    if(T==TEXT("object"))
    { const auto* O=CastField<FObjectProperty>(P);return O&&!CastField<FClassProperty>(P)&&O->PropertyClass==UObject::StaticClass(); }
    return false;
}
bool Signature(UClass* Owner,UFunction* F,const UEdGraphSchema_K2* Schema,const FString& Type,bool Literal,bool FloatAdapted,FSignature& Out,FResult& R,const TCHAR* Key)
{
    Count(R,TEXT("signature_observation_calls"));R.Data->SetObjectField(Key,Out.Json);
    Out.Json->SetStringField(TEXT("owner"),Owner->GetPathName());Out.Json->SetStringField(TEXT("function"),F->GetPathName());
    Out.Json->SetStringField(TEXT("function_flags"),LexToString(static_cast<uint32>(F->FunctionFlags)));
    Out.Json->SetStringField(TEXT("owner_class_flags"),LexToString(static_cast<uint32>(Owner->GetClassFlags())));
    Out.Json->SetBoolField(TEXT("package_dirty"),Owner->GetOutermost()->IsDirty());
    TArray<TSharedPtr<FJsonValue>> Rows;Out.Json->SetArrayField(TEXT("parameters"),Rows);
    if(!Loaded(Owner)||!Loaded(F)||Owner->GetOutermost()->GetName()!=TEXT("/Script/Engine")||Owner->ClassGeneratedBy!=nullptr
        ||!Owner->HasAnyClassFlags(CLASS_Native)||Owner->IsEditorOnly()||F->GetOuter()!=Owner
        ||!F->HasAllFunctionFlags(FUNC_Public|FUNC_Static|FUNC_BlueprintCallable|FUNC_BlueprintPure)
        ||F->HasAnyFunctionFlags(FUNC_Private|FUNC_Protected|FUNC_EditorOnly|FUNC_Delegate|FUNC_Net)
        ||F->HasMetaData(TEXT("Latent"))||F->HasMetaData(TEXT("CustomThunk"))||F->HasMetaData(TEXT("DeprecatedFunction"))
        ||F->GetBoolMetaData(TEXT("BlueprintInternalUseOnly"))||UEdGraphSchema_K2::HasWildcardParams(F))
    { return Fail(R,TEXT("value_function_signature_pending"),TEXT("Exact public static pure mapped native Engine function required.")); }
    int32 Inputs=0,Outputs=0;
    for(TFieldIterator<FProperty> It(F);It&&It->HasAnyPropertyFlags(CPF_Parm);++It)
    {
        Count(R,TEXT("signature_parameter_observation_calls"));const bool Return=It->HasAnyPropertyFlags(CPF_ReturnParm);
        auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),It->GetName());Row->SetBoolField(TEXT("return_parameter"),Return);
        Row->SetStringField(TEXT("property_class"),It->GetClass()->GetName());Row->SetStringField(TEXT("property_flags"),LexToString(static_cast<uint64>(It->GetPropertyFlags())));
        const FName DefaultKey(*FString::Printf(TEXT("CPP_Default_%s"),*It->GetName()));Row->SetBoolField(TEXT("default_metadata_present"),F->HasMetaData(DefaultKey));Row->SetStringField(TEXT("default_metadata"),F->GetMetaData(DefaultKey));
        Rows.Add(MakeShared<FJsonValueObject>(Row));Out.Json->SetArrayField(TEXT("parameters"),Rows);
        FEdGraphPinType T;const FName N=It->GetFName();const FString Expected=Return&&!Literal?TEXT("bool"):Type;
        if(It->ArrayDim!=1||It->GetOwnerStruct()!=F||It->HasAnyPropertyFlags(CPF_ReferenceParm)
            ||(!Return&&It->HasAnyPropertyFlags(CPF_OutParm))||!PropertyFits(*It,Expected)||!Schema->ConvertPropertyToPinType(*It,T)
            ||T.IsContainer()||T.bIsReference||T.bIsWeakPointer
            ||(Return&&N!=TEXT("ReturnValue"))||(!Return&&((FloatAdapted&&N!=TEXT("CurrentSample")&&N!=TEXT("PreviousSample")&&N!=TEXT("Weight"))
                ||(Literal&&!FloatAdapted&&N!=TEXT("Value"))||(!Literal&&N!=TEXT("A")&&N!=TEXT("B")))))
        { return Fail(R,TEXT("value_function_signature_pending"),TEXT("Mapped parameter name/type/ownership/layout differs from the installed exact contract.")); }
        Row->SetObjectField(TEXT("pin_type"),Function::Pin::Describe(T));
        if(Return) { ++Outputs;Out.Outputs.Add(N,T); }else { ++Inputs;Out.Inputs.Add(N,T); }
    }
    const bool OK=Outputs==1&&Inputs==(FloatAdapted?3:Literal?1:2);Out.Json->SetBoolField(TEXT("verified"),OK);
    return OK||Fail(R,TEXT("value_function_signature_pending"),TEXT("Mapped function parameter cardinality differs from its exact literal/comparison/adapted-float contract."));
}
bool ActorScope(UBlueprint* BP,UEdGraph* G,const TSharedPtr<FJsonObject>& Graph,FResult& R)
{
    if(!Loaded(BP)||!Loaded(G)||FBlueprintEditorUtils::FindBlueprintForGraph(G)!=BP||BP->ParentClass!=AActor::StaticClass()
        ||BP->BlueprintType!=BPTYPE_Normal||BP->bBeingCompiled||BP->bQueuedForCompilation||BP->bSuppressStructurallyModified
        ||BP->bIsRegeneratingOnLoad||BP->Status==BS_BeingCreated||!G->bEditable||!G->GetSchema()
        ||G->GetSchema()->GetClass()!=UEdGraphSchema_K2::StaticClass()
        ||(Graph->GetStringField(TEXT("graph_type"))!=TEXT("function")&&Graph->GetStringField(TEXT("graph_type"))!=TEXT("event"))||G->Nodes.Num()>=2048)
    { return Fail(R,TEXT("value_blueprint_scope_pending"),TEXT("Finite idle direct native Actor Blueprint with an owned editable function/event K2 graph required; wider original owners remain residual.")); }
    Count(R,TEXT("external_scope_observation_calls"));TArray<TSharedPtr<FJsonValue>> External;
    for(TObjectIterator<UBlueprint> It;It;++It)
    { if(*It!=BP&&It->ParentClass&&BP->GeneratedClass&&It->ParentClass->IsChildOf(BP->GeneratedClass)) { External.Add(MakeShared<FJsonValueString>(It->GetPathName())); } }
    if(BP->GeneratedClass)
    { TArray<UObject*> Instances;GetObjectsOfClass(BP->GeneratedClass,Instances,true,RF_NoFlags);for(UObject* O:Instances) { if(IsValid(O)&&O->GetOutermost()!=BP->GetOutermost()) { External.Add(MakeShared<FJsonValueString>(O->GetPathName())); } } }
    R.Data->SetArrayField(TEXT("external_instances_or_derived_blueprints"),External);
    return External.IsEmpty()||Fail(R,TEXT("value_external_instances_pending"),TEXT("Loaded external instances/derived Blueprints can receive MarkModified effects; finite scope refused."));
}
bool LiteralDefault(const TSharedPtr<FJsonValue>& V,const FEdGraphPinType& Type,FString& Text,FResult& R)
{
    Count(R,TEXT("literal_default_conversion_calls"));Text.Reset();
    if(Type.PinCategory==UEdGraphSchema_K2::PC_Boolean&&V->Type!=EJson::Boolean)
    { return Fail(R,TEXT("value_literal_invalid"),TEXT("Boolean default values must be JSON booleans.")); }
    if(V->Type==EJson::String) { Text=V->AsString();return true; }
    if(V->Type==EJson::Boolean) { Text=V->AsBool()?TEXT("true"):TEXT("false");return true; }
    if(V->Type==EJson::Number)
    {
        const double N=V->AsNumber();if(!FMath::IsFinite(N)) { return Fail(R,TEXT("value_literal_invalid"),TEXT("Finite numeric literal required.")); }
        if(Type.PinCategory==UEdGraphSchema_K2::PC_Int||Type.PinCategory==UEdGraphSchema_K2::PC_Int64||Type.PinCategory==UEdGraphSchema_K2::PC_Byte)
        { if(FMath::Abs(N)>9007199254740991.0) { return Fail(R,TEXT("value_literal_numeric_precision_pending"),TEXT("Numeric integer carrier exceeds exact safe double budget; original unsafe/lossy larger variants remain incomplete. Exact decimal string values remain schema-authoritative.")); }Text=FString::Printf(TEXT("%lld"),static_cast<int64>(N)); }
        else { Text=FString::SanitizeFloat(N); }
        return true;
    }
    if(V->Type==EJson::Array||V->Type==EJson::Object)
    { return FJsonSerializer::Serialize(V,TEXT(""),TJsonWriterFactory<>::Create(&Text))||Fail(R,TEXT("value_literal_invalid"),TEXT("Literal JSON serialization failed.")); }
    return true;
}
bool Pins(UK2Node_CallFunction* Node,UClass* Owner,const FSignature& Sig,const UEdGraphSchema_K2* Schema)
{
    if(!Loaded(Node)||!Node->IsNodePure()||Node->Pins.Num()!=Sig.Inputs.Num()+Sig.Outputs.Num()+1) { return false; }
    TSet<FGuid> Ids;
    for(const UEdGraphPin* P:Node->Pins)
    { if(!P||P->WasTrashed()||!P->PinId.IsValid()||Ids.Contains(P->PinId)||P->GetOwningNodeUnchecked()!=Node||P->bOrphanedPin||P->ParentPin||!P->SubPins.IsEmpty()||!P->LinkedTo.IsEmpty()) { return false; }Ids.Add(P->PinId); }
    for(const auto& Pair:Sig.Inputs)
    { const auto* P=Node->FindPin(Pair.Key,EGPD_Input);if(!P||P->PinType!=Pair.Value||P->bHidden||P->bNotConnectable||P->bDefaultValueIsReadOnly||P->bDefaultValueIsIgnored||!Schema->IsCurrentPinDefaultValid(P).IsEmpty()) { return false; } }
    for(const auto& Pair:Sig.Outputs)
    { const auto* P=Node->FindPin(Pair.Key,EGPD_Output);if(!P||P->PinType!=Pair.Value||P->bHidden||P->bNotConnectable||P->bDefaultValueIsIgnored) { return false; } }
    const auto* Self=Node->FindPin(UEdGraphSchema_K2::PN_Self,EGPD_Input);
    return Self&&Self->bHidden&&Self->PinType.PinCategory==UEdGraphSchema_K2::PC_Object&&Self->PinType.PinSubCategoryObject.Get()==Owner
        &&!Self->PinType.IsContainer()&&!Self->PinType.bIsReference&&(!Self->DefaultObject||Self->DefaultObject->GetClass()==Owner);
}
bool FloatDefaults(const UK2Node_CallFunction* Node,FResult& R,const TCHAR* Key)
{
    Count(R,TEXT("float_default_observation_calls"));auto Values=MakeShared<FJsonObject>();R.Data->SetObjectField(Key,Values);float Samples[3]={0,0,0};int32 I=0;
    for(const TCHAR* Name:{TEXT("CurrentSample"),TEXT("PreviousSample"),TEXT("Weight")})
    {
        const auto* P=Node->FindPin(Name,EGPD_Input);if(!P) { return Fail(R,TEXT("value_float_default_invalid"),TEXT("Adapted float input pin is missing.")); }
        Values->SetStringField(Name,P->GetDefaultAsString());Count(R,TEXT("float_default_parse_calls"));
        if(!LexTryParseString(Samples[I],*P->GetDefaultAsString())||!FMath::IsFinite(Samples[I]))
        { return Fail(R,TEXT("value_float_default_invalid"),TEXT("Adapted float actual pin defaults must parse to finite float32, including string/exponent values.")); }
        Values->SetNumberField(FString(Name)+TEXT("_float32"),static_cast<double>(Samples[I]));++I;
    }
    uint32 A=0,B=0;FMemory::Memcpy(&A,&Samples[0],sizeof(float));FMemory::Memcpy(&B,&Samples[1],sizeof(float));Values->SetNumberField(TEXT("CurrentSample_bits"),A);Values->SetNumberField(TEXT("PreviousSample_bits"),B);
    const bool OK=A==B&&Samples[2]==1.0f;Values->SetBoolField(TEXT("finite_same_samples_weight_one"),OK);
    return OK||Fail(R,TEXT("value_float_default_invalid"),TEXT("Adapted float requires bit-identical finite samples and Weight exactly float32 one."));
}
bool ApplyFloatLiteral(UK2Node_CallFunction* Node,const TSharedPtr<FJsonValue>& Value,const UEdGraphSchema_K2* Schema,FString& Wanted,FResult& R)
{
    auto* Current=Node->FindPin(TEXT("CurrentSample"),EGPD_Input);auto* Previous=Node->FindPin(TEXT("PreviousSample"),EGPD_Input);auto* Weight=Node->FindPin(TEXT("Weight"),EGPD_Input);
    if(!Current||!Previous||!Weight) { return Fail(R,TEXT("value_float_default_invalid"),TEXT("Exact adapted float input pins are missing.")); }
    if(Value->Type==EJson::Null) { Wanted=Current->GetDefaultAsString(); }
    else if(!LiteralDefault(Value,Current->PinType,Wanted,R)) { return false; }
    R.Data->SetStringField(TEXT("literal_requested_default"),Wanted);float FiniteValue=0;Count(R,TEXT("float_default_parse_calls"));
    if(!LexTryParseString(FiniteValue,*Wanted)||!FMath::IsFinite(FiniteValue)) { return Fail(R,TEXT("value_float_default_invalid"),TEXT("Supplied/adapted-default literal must resolve to finite float32 after allocation; no NaN/Inf/overflow acceptance.")); }
    R.Data->SetNumberField(TEXT("adapted_requested_float32"),static_cast<double>(FiniteValue));TArray<TSharedPtr<FJsonValue>> Attempts;R.Data->SetArrayField(TEXT("adapted_literal_default_attempts"),Attempts);
    for(UEdGraphPin* P:{Current,Previous,Weight})
    {
        const FString Text=P==Weight?TEXT("1.0"):Wanted;auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("pin_name"),P->PinName.ToString());Row->SetStringField(TEXT("requested_default"),Text);Attempts.Add(MakeShared<FJsonValueObject>(Row));R.Data->SetArrayField(TEXT("adapted_literal_default_attempts"),Attempts);
        Count(R,TEXT("try_set_default_calls"));Schema->TrySetDefaultValue(*P,Text);Count(R,TEXT("try_set_default_returns"));Row->SetStringField(TEXT("returned_default"),P->GetDefaultAsString());
        Count(R,TEXT("default_validation_calls"));const FString Error=Schema->IsCurrentPinDefaultValid(P);Count(R,TEXT("default_match_calls"));const bool Match=Schema->DoesDefaultValueMatch(*P,Text);Row->SetStringField(TEXT("validation_error"),Error);Row->SetBoolField(TEXT("matched"),Match);
        if(!Error.IsEmpty()||!Match) { return Fail(R,TEXT("value_float_default_invalid"),Error.IsEmpty()?TEXT("Adapted float default was not accepted."):Error); }
        Count(R,TEXT("pin_default_changed_calls"));Node->PinDefaultValueChanged(P);Count(R,TEXT("pin_default_changed_returns"));
    }
    return FloatDefaults(Node,R,TEXT("adapted_float_defaults_returned"));
}
FResult CreateMapped(const FString& Path,const FString& Type,const FName& FunctionName,bool Literal,const TSharedPtr<FJsonValue>& Value,const FString& PositionJson,const FString& Name,const FString& Id,const FString& GraphType,bool Create,FResult R)
{
    int32 XY[2]={0,0};FString Projected;if(!Position(PositionJson,XY,Projected,R)) { return R; }
    R.Stage=TEXT("resolve_graph");Count(R,TEXT("shared_graph_resolve_calls"));const auto Resolved=Function::Resolve(Path,Name,Id,GraphType,Create);Count(R,TEXT("shared_graph_resolve_returns"));R.Data->SetObjectField(TEXT("graph_resolution"),ResultJson(Resolved));
    R.bMutationAttempted=Resolved.bMutationAttempted;R.bChanged=Resolved.bChanged;R.bChangeStateKnown=Resolved.bChangeStateKnown;
    if(!Resolved.bSuccess||!Resolved.bPostconditionPassed) { Fail(R,Resolved.ErrorCode.IsEmpty()?TEXT("value_graph_unverified"):*Resolved.ErrorCode,Resolved.Error);return R; }
    auto* BP=FindObject<UBlueprint>(nullptr,*Resolved.Data->GetStringField(TEXT("blueprint")));const auto Graph=Resolved.Data->GetObjectField(TEXT("graph"));auto* G=FindObject<UEdGraph>(nullptr,*Graph->GetStringField(TEXT("object_path")));
    if(!ActorScope(BP,G,Graph,R)) { return R; }
    const TStrongObjectPtr<UBlueprint> KeepBP(BP);const TStrongObjectPtr<UEdGraph> KeepGraph(G);const FGuid GraphGuid=G->GraphGuid;
    const auto* Schema=CastChecked<UEdGraphSchema_K2>(G->GetSchema());UClass* Owner=Literal?UKismetSystemLibrary::StaticClass():UKismetMathLibrary::StaticClass();const TStrongObjectPtr<UClass> KeepOriginalOwner(Owner);
    Count(R,TEXT("mapped_function_lookup_calls"));UFunction* F=Owner->FindFunctionByName(FunctionName);Count(R,TEXT("mapped_function_lookup_returns"));
    R.Data->SetStringField(TEXT("original_mapped_function_name"),FunctionName.ToString());R.Data->SetStringField(TEXT("original_mapped_function_class"),Owner->GetPathName());R.Data->SetBoolField(TEXT("original_mapped_function_available"),Loaded(F));
    R.Data->SetBoolField(TEXT("native_only_float_name_requested"),Literal&&FunctionName==FName(TEXT("MakeLiteralFloat")));
    bool FloatAdapted=false;FName ActualName=FunctionName;
    if(F==nullptr&&Literal&&FunctionName==FName(TEXT("MakeLiteralFloat")))
    {
        R.Data->SetStringField(TEXT("float_source_scope"),TEXT("Installed stock UE5.6.1 and UE5.8 have native-only deprecated MakeLiteralFloat; original exact lookup was missing. UE58 compatibility uses public MathLibrary WeightedMovingAverage_Float with equal finite float32 samples and Weight1. Different class/function/three pins, no synthetic Value pin or full original/runtime parity."));
        R.Data->SetBoolField(TEXT("float_fallback_attempted"),true);Count(R,TEXT("float_fallback_lookup_calls"));Owner=UKismetMathLibrary::StaticClass();ActualName=GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary,WeightedMovingAverage_Float);F=Owner->FindFunctionByName(ActualName);Count(R,TEXT("float_fallback_lookup_returns"));FloatAdapted=Loaded(F);R.Data->SetBoolField(TEXT("float_compatibility_adaptation_selected"),FloatAdapted);
    }
    R.Data->SetStringField(TEXT("mapped_function_name"),ActualName.ToString());R.Data->SetStringField(TEXT("mapped_function_class"),Owner->GetPathName());R.Data->SetBoolField(TEXT("mapped_function_available"),Loaded(F));
    if(!Loaded(F)) { Fail(R,TEXT("value_function_unavailable"),FString::Printf(TEXT("Mapped UFunction is not available: %s. Actual original/fallback resolver/load/lookup prefix retained; no runtime wrapper or retry."),*ActualName.ToString()));return R; }
    const TStrongObjectPtr<UClass> KeepOwner(Owner);const TStrongObjectPtr<UFunction> KeepFunction(F);FSignature Sig;FString SemanticType=Type;
    if(!Literal&&(NormalizeTypeName(Type)==TEXT("float")||NormalizeTypeName(Type)==TEXT("real"))) { SemanticType=TEXT("double"); }
    if(!Signature(Owner,F,Schema,SemanticType,Literal,FloatAdapted,Sig,R,TEXT("native_signature_before"))) { return R; }
    FText Reason;if(!Schema->CanFunctionBeUsedInGraph(Owner,F,G,UEdGraphSchema_K2::FT_Pure|UEdGraphSchema_K2::FT_Imperative|UEdGraphSchema_K2::FT_Const,false,&Reason))
    { Fail(R,TEXT("value_graph_call_rejected"),Reason.ToString());return R; }
    const auto Before=Resolved.Data->GetObjectField(TEXT("after"));R.Data->SetObjectField(TEXT("before"),Before);
    R.Stage=TEXT("create_value_node");R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("node_allocation_calls"));auto* Node=NewObject<UK2Node_CallFunction>(G);
    if(!Node) { Fail(R,TEXT("value_allocation_failed"),TEXT("Native call-node allocation returned null; resolver/attempt prefix retained."));return R; }
    const TStrongObjectPtr<UK2Node_CallFunction> KeepNode(Node);R.Data->SetStringField(TEXT("created_node_path"),Node->GetPathName());
    Count(R,TEXT("set_from_function_calls"));Node->SetFromFunction(F);Node->NodePosX=XY[0];Node->NodePosY=XY[1];
    Count(R,TEXT("graph_add_node_calls"));G->AddNode(Node,true);Count(R,TEXT("new_guid_calls"));Node->CreateNewGuid();Count(R,TEXT("post_placed_calls"));Node->PostPlacedNewNode();Count(R,TEXT("allocate_pins_calls"));Node->AllocateDefaultPins();
    bool Accepted=true;FString Wanted;
    if(FloatAdapted) { R.Stage=TEXT("apply_adapted_float_literal");Accepted=ApplyFloatLiteral(Node,Value,Schema,Wanted,R); }
    else if(Literal&&Value->Type!=EJson::Null)
    {
        R.Stage=TEXT("apply_literal_value");auto* P=Node->FindPin(TEXT("Value"),EGPD_Input);R.Data->SetBoolField(TEXT("literal_value_pin_found"),P!=nullptr);
        if(!P) { Accepted=Fail(R,TEXT("value_literal_pin_missing"),TEXT("Failed to resolve literal Value input pin.")); }
        else if(!LiteralDefault(Value,P->PinType,Wanted,R)) { Accepted=false; }
        else
        {
            R.Data->SetStringField(TEXT("literal_requested_default"),Wanted);Count(R,TEXT("try_set_default_calls"));Schema->TrySetDefaultValue(*P,Wanted);Count(R,TEXT("try_set_default_returns"));
            R.Data->SetStringField(TEXT("literal_returned_default"),P->GetDefaultAsString());Count(R,TEXT("default_validation_calls"));const FString Error=Schema->IsCurrentPinDefaultValid(P);
            Count(R,TEXT("default_match_calls"));const bool Match=Schema->DoesDefaultValueMatch(*P,Wanted);R.Data->SetStringField(TEXT("literal_default_validation_error"),Error);R.Data->SetBoolField(TEXT("literal_default_match"),Match);
            if(!Error.IsEmpty()||!Match) { Accepted=Fail(R,TEXT("value_literal_invalid"),Error.IsEmpty()?TEXT("Requested literal default was not accepted."):Error); }
            else { Count(R,TEXT("pin_default_changed_calls"));Node->PinDefaultValueChanged(P);Count(R,TEXT("pin_default_changed_returns")); }
        }
    }
    R.Data->SetBoolField(TEXT("literal_value_accepted"),Accepted);R.Data->SetStringField(TEXT("created_node_guid"),Node->NodeGuid.ToString(EGuidFormats::Digits));
    if(!Accepted)
    { R.Stage=TEXT("retire_invalid_literal");Count(R,TEXT("destroy_node_calls"));Node->DestroyNode();Count(R,TEXT("destroy_node_returns"));R.Data->SetBoolField(TEXT("created_node_retired"),!G->Nodes.Contains(Node)); }
    else { R.Stage=TEXT("mark_modified");Count(R,TEXT("mark_blueprint_modified_calls"));FBlueprintEditorUtils::MarkBlueprintAsModified(BP);Count(R,TEXT("mark_blueprint_modified_returns")); }
    const FString PriorCode=R.ErrorCode,PriorError=R.Error;
    if(!Accepted) { R.Data->SetStringField(TEXT("original_literal_error_code"),PriorCode);R.Data->SetStringField(TEXT("original_literal_error"),PriorError); }
    R.Stage=TEXT("after_observation");Count(R,TEXT("shared_graph_resolve_calls"));const auto Observed=Function::Resolve(BP->GetPathName(),TEXT(""),GraphGuid.ToString(EGuidFormats::Digits),TEXT(""),false);Count(R,TEXT("shared_graph_resolve_returns"));R.Data->SetObjectField(TEXT("graph_post_observation"),ResultJson(Observed));
    FSignature SigAfter;const bool SignatureOK=Signature(Owner,F,Schema,SemanticType,Literal,FloatAdapted,SigAfter,R,TEXT("native_signature_after"));
    const bool SourceSame=SignatureOK&&Equal(MakeShared<FJsonValueObject>(Sig.Json),MakeShared<FJsonValueObject>(SigAfter.Json));R.Data->SetBoolField(TEXT("native_signature_preserved"),SourceSame);
    if(!Observed.bSuccess||!Observed.bChangeStateKnown||!Observed.bPostconditionPassed)
    { Fail(R,TEXT("value_after_incomplete"),TEXT("Current graph read did not close; actual allocation/default/retirement/mark prefix retained, no retry."));return R; }
    const auto After=Observed.Data->GetObjectField(TEXT("after"));R.Data->SetObjectField(TEXT("after"),After);R.bChanged=Resolved.bChanged||!Equal(MakeShared<FJsonValueObject>(Before),MakeShared<FJsonValueObject>(After))||!SourceSame;R.bChangeStateKnown=SignatureOK;
    const int32 Delta=Accepted?1:0;const bool OldNodes=ExistingNodesPreserved(Before,After,Delta),OldGraphs=GraphRowsPreserved(Before,After,G->GetPathName(),Delta);
    R.Data->SetBoolField(TEXT("existing_nodes_preserved"),OldNodes);R.Data->SetBoolField(TEXT("graph_rows_preserved"),OldGraphs);R.Data->SetStringField(TEXT("change_observation_scope"),TEXT("Shared owned graph identities/selected nodes/pins/defaults/links/dirty/status plus mapped native signature. Arbitrary CDO/child/editor/cache/callback state is not fully observed; cleanup is not atomic rollback."));
    if(!Accepted)
    {
        R.Data->SetBoolField(TEXT("retired_node_graph_postcondition"),!G->Nodes.Contains(Node)&&OldNodes&&OldGraphs&&SourceSame);
        if(!R.Data->GetBoolField(TEXT("retired_node_graph_postcondition"))) { R.bChangeStateKnown=false;Fail(R,TEXT("value_literal_cleanup_incomplete"),TEXT("Original literal rejection occurred, but new-node retirement/source/preservation observation did not close.")); }
        else { Fail(R,PriorCode.IsEmpty()?TEXT("value_literal_invalid"):*PriorCode,PriorError); }
        return R;
    }
    const TSharedPtr<FJsonValue>* Current=After->GetArrayField(TEXT("nodes")).FindByPredicate([&](const auto& J){return J->AsObject()->GetStringField(TEXT("object_path")).Equals(Node->GetPathName(),ESearchCase::CaseSensitive);});
    if(Current) { R.Data->SetObjectField(TEXT("node"),(*Current)->AsObject());for(const auto& Pair:(*Current)->AsObject()->Values) { R.Data->SetField(Pair.Key,Pair.Value); }LegacyNodeFields(Node,R); }
    const bool Identity=Loaded(Node)&&Node->GetClass()==UK2Node_CallFunction::StaticClass()&&Node->GetTargetFunction()==F&&Node->FunctionReference.GetMemberName()==F->GetFName()
        &&Node->FunctionReference.GetMemberParentClass()==Owner&&!Node->FunctionReference.IsSelfContext()&&!Node->FunctionReference.GetMemberGuid().IsValid()
        &&G->Nodes.Contains(Node)&&Node->NodeGuid.IsValid()&&G->GraphGuid==GraphGuid&&Node->NodePosX==XY[0]&&Node->NodePosY==XY[1];
    bool DefaultOK=true;
    if(FloatAdapted)
    {
        DefaultOK=FloatDefaults(Node,R,TEXT("adapted_float_defaults_current"));
        for(const TCHAR* PinName:{TEXT("CurrentSample"),TEXT("PreviousSample"),TEXT("Weight")}) { const auto* P=Node->FindPin(PinName,EGPD_Input);const FString Expected=P&&P->PinName==TEXT("Weight")?TEXT("1.0"):Wanted;DefaultOK&=P&&Schema->IsCurrentPinDefaultValid(P).IsEmpty()&&Schema->DoesDefaultValueMatch(*P,Expected); }
    }
    else if(Literal&&Value->Type!=EJson::Null) { const auto* P=Node->FindPin(TEXT("Value"),EGPD_Input);DefaultOK=P&&Schema->IsCurrentPinDefaultValid(P).IsEmpty()&&Schema->DoesDefaultValueMatch(*P,Wanted); }
    R.Data->SetBoolField(TEXT("function_member_identity_verified"),Identity);R.Data->SetBoolField(TEXT("current_literal_default_verified"),DefaultOK);R.Data->SetBoolField(TEXT("current_pin_shape_verified"),Pins(Node,Owner,Sig,Schema));
    if(!Current||!Identity||!DefaultOK||!R.Data->GetBoolField(TEXT("current_pin_shape_verified"))||!OldNodes||!OldGraphs||!SourceSame)
    { Fail(R,TEXT("value_node_postcondition_failed"),TEXT("Current node/function/pin/default/position/source/preservation proof failed; accepted writer prefix retained without cleanup/retry."));return R; }
    R.Data->SetObjectField(TEXT("graph"),Observed.Data->GetObjectField(TEXT("graph")));R.Data->SetStringField(TEXT("blueprint"),BP->GetPathName());R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
}
Function::FResult AddCompare(const FString& Path,const FString& Operation,const FString& Type,const FString& PositionJson,const FString& Name,const FString& Id,const FString& GraphType,bool Create)
{
    FResult R;Init(R);R.Data->SetStringField(TEXT("requested_operation"),Operation);R.Data->SetStringField(TEXT("value_type"),Type);R.Data->SetStringField(TEXT("legacy_node_kind"),TEXT("compare"));
    if(!IsEditorGameThread()||IsPlaySessionActive()||!SafeToken(Operation)||!SafeToken(Type)) { Fail(R,TEXT("value_input_invalid"),TEXT("Idle Editor and bounded literal labels without interior NUL required."));return R; }
    Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","AddBlueprintCompare","Add Blueprint Compare Node"));Count(R,TEXT("operation_mapping_calls"));FString Error;const FName Fn=GetCompareFunctionName(Operation,Type,Error);
    if(Fn.IsNone()) { Fail(R,TEXT("value_compare_mapping_unsupported"),Error);return R; }
    return CreateMapped(Path,Type,Fn,false,nullptr,PositionJson,Name,Id,GraphType,Create,R);
}
Function::FResult AddLiteral(const FString& Path,const FString& Type,const FString& ValueJson,const FString& PositionJson,const FString& Name,const FString& Id,const FString& GraphType,bool Create)
{
    FResult R;Init(R);R.Data->SetStringField(TEXT("literal_type"),Type);R.Data->SetStringField(TEXT("legacy_node_kind"),TEXT("literal"));
    if(!IsEditorGameThread()||IsPlaySessionActive()||!SafeToken(Type)) { Fail(R,TEXT("value_input_invalid"),TEXT("Idle Editor and bounded literal type without interior NUL required."));return R; }
    Count(R,TEXT("outer_transaction_calls"));FScopedTransaction Tx(NSLOCTEXT("CubelessMCP","AddBlueprintLiteral","Add Blueprint Literal Node"));Count(R,TEXT("operation_mapping_calls"));FString Error;const FName Fn=GetLiteralFunctionName(Type,Error);
    if(Fn.IsNone()) { Fail(R,TEXT("value_literal_mapping_unsupported"),Error);return R; }
    TSharedPtr<FJsonValue> Value;if(!Function::Pin::ParseJson(ValueJson,Value,R)) { return R; }R.Data->SetField(TEXT("requested_literal_value"),Value);R.Data->SetBoolField(TEXT("literal_value_supplied"),Value->Type!=EJson::Null);
    return CreateMapped(Path,Type,Fn,true,Value,PositionJson,Name,Id,GraphType,Create,R);
}
}
