// DB original Math7 and Boolean7; mapped public Engine node authoring, no runtime evaluation.
#include "Compatibility/UE58/CubelessBlueprintExpressionApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessFunctionPinApi.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraphNode.h"
#include "Kismet/KismetMathLibrary.h"
#include "ScopedTransaction.h"
#include <cfloat>
#include <initializer_list>
namespace Cubeless::UE58::BlueprintExpression
{
namespace
{
using FResult=Function::FResult;
void Count(FResult& R,const TCHAR* Key) { R.Data->SetNumberField(Key,R.Data->GetNumberField(Key)+1); }
bool Fail(FResult& R,const TCHAR* Code,const FString& Error) { R.bSuccess=R.bPostconditionPassed=false;R.ErrorCode=Code;R.Error=Error;return false; }
bool Loaded(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
bool SafeOperation(const FString& S) { if(S.Len()>512) { return false; }for(int32 I=0;I<S.Len();++I) { if(!S[I]) { return false; } }return true; }
void Init(FResult& R,const FString& Operation,const TCHAR* Kind)
{
    for(const TCHAR* K:{TEXT("operation_mapping_calls"),TEXT("outer_transaction_calls"),TEXT("shared_add_call_calls"),TEXT("shared_add_call_returns"),TEXT("legacy_node_metadata_observation_calls"),TEXT("explicit_compile_calls"),TEXT("explicit_save_calls")}) { R.Data->SetNumberField(K,0); }
    R.Data->SetStringField(TEXT("requested_operation"),Operation);R.Data->SetStringField(TEXT("expression_kind"),Kind);
    R.Data->SetStringField(TEXT("actual_node_family"),TEXT("UK2Node_CallFunction"));
    R.Data->SetStringField(TEXT("position_scope"),TEXT("Original null/default origin, first-two float32 coordinates and checked int32 truncation; bounded legacy CSV/ignored-shape fallback. No unbounded float-to-int conversion."));
    R.Data->SetStringField(TEXT("lifecycle_scope"),TEXT("Original mapped math/Boolean function node, immutable shared AddCall current graph/signature/pin/default proof. Explicit compiler/save zero; class/default/cache/graph notifications and Blueprint refresh are implicit, not zero or runtime/cook proof."));
    R.Data->SetBoolField(TEXT("runtime_value_invocation_verified"),false);R.Data->SetBoolField(TEXT("full_original_or_runtime_verified"),false);R.Data->SetBoolField(TEXT("automatic_retry_performed"),false);
}

FName GetMathFunctionName(const FString& Operation)
{
    if (Operation.Equals(TEXT("add"), ESearchCase::IgnoreCase) || Operation.Equals(TEXT("+"), ESearchCase::IgnoreCase))
    {
        return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Add_DoubleDouble);
    }
    if (Operation.Equals(TEXT("subtract"), ESearchCase::IgnoreCase) || Operation.Equals(TEXT("-"), ESearchCase::IgnoreCase))
    {
        return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Subtract_DoubleDouble);
    }
    if (Operation.Equals(TEXT("multiply"), ESearchCase::IgnoreCase) || Operation.Equals(TEXT("*"), ESearchCase::IgnoreCase))
    {
        return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Multiply_DoubleDouble);
    }
    if (Operation.Equals(TEXT("divide"), ESearchCase::IgnoreCase) || Operation.Equals(TEXT("/"), ESearchCase::IgnoreCase))
    {
        return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Divide_DoubleDouble);
    }
    if (Operation.Equals(TEXT("clamp"), ESearchCase::IgnoreCase))
    {
        return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, FClamp);
    }
    if (Operation.Equals(TEXT("in_range"), ESearchCase::IgnoreCase) || Operation.Equals(TEXT("inrange"), ESearchCase::IgnoreCase))
    {
        return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, InRange_FloatFloat);
    }

    return NAME_None;
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
FName GetBooleanFunctionName(const FString& Operation, FString& OutError)
{
    if (IsOperationAlias(Operation, {TEXT("not"), TEXT("!")}))
    {
        return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Not_PreBool);
    }
    if (IsOperationAlias(Operation, {TEXT("and"), TEXT("&&")}))
    {
        return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BooleanAND);
    }
    if (IsOperationAlias(Operation, {TEXT("nand")}))
    {
        return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BooleanNAND);
    }
    if (IsOperationAlias(Operation, {TEXT("or"), TEXT("||")}))
    {
        return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BooleanOR);
    }
    if (IsOperationAlias(Operation, {TEXT("xor")}))
    {
        return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BooleanXOR);
    }
    if (IsOperationAlias(Operation, {TEXT("nor")}))
    {
        return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BooleanNOR);
    }
    if (IsOperationAlias(Operation, {TEXT("equal"), TEXT("equals"), TEXT("eq"), TEXT("==")}))
    {
        return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, EqualEqual_BoolBool);
    }
    if (IsOperationAlias(Operation, {TEXT("not_equal"), TEXT("notequal"), TEXT("ne"), TEXT("!=")}))
    {
        return GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, NotEqual_BoolBool);
    }

    OutError = FString::Printf(TEXT("Unsupported boolean operation: %s"), *Operation);
    return NAME_None;
}
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
        if(!FMath::IsFinite(Values[I])||FMath::Abs(Values[I])>FLT_MAX) { return Fail(R,TEXT("expression_position_invalid"),TEXT("Finite float32 position required before graph writer.")); }
        const float F=static_cast<float>(Values[I]);const double D=static_cast<double>(F);
        if(!FMath::IsFinite(F)||D<static_cast<double>(MIN_int32)||D>static_cast<double>(MAX_int32)) { return Fail(R,TEXT("expression_position_invalid"),TEXT("Original float32 position exceeds safe int32 assignment budget.")); }
        XY[I]=static_cast<int32>(F);Original.Add(MakeShared<FJsonValueNumber>(Values[I]));FloatRows.Add(MakeShared<FJsonValueNumber>(D));IntRows.Add(MakeShared<FJsonValueNumber>(XY[I]));
    }
    R.Data->SetField(TEXT("requested_position"),V);R.Data->SetArrayField(TEXT("position_numeric_prefix"),Original);R.Data->SetArrayField(TEXT("position_float32"),FloatRows);R.Data->SetArrayField(TEXT("position_int32"),IntRows);
    Projected=FString::Printf(TEXT("[%d,%d]"),XY[0],XY[1]);return true;
}
void LegacyNodeFields(UEdGraphNode* Node,FResult& R)
{
    Count(R,TEXT("legacy_node_metadata_observation_calls"));R.Data->SetStringField(TEXT("name"),Node->GetName());R.Data->SetStringField(TEXT("class"),Node->GetClass()->GetName());
    R.Data->SetStringField(TEXT("title"),Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());R.Data->SetStringField(TEXT("node_comment"),Node->NodeComment);
    R.Data->SetBoolField(TEXT("comment_bubble_visible"),Node->bCommentBubbleVisible);R.Data->SetBoolField(TEXT("comment_bubble_pinned"),Node->bCommentBubblePinned);
}

FResult AddMapped(const FString& Path,const FName& FunctionName,const FString& PositionJson,const FString& Name,const FString& Id,const FString& GraphType,bool Create,FResult Context)
{
    int32 XY[2]={0,0};FString PositionText;if(!Position(PositionJson,XY,PositionText,Context)) { return Context; }
    if(!IsEditorGameThread()||IsPlaySessionActive()) { Fail(Context,TEXT("expression_editor_invalid"),TEXT("Idle Editor game thread required."));return Context; }
    Count(Context,TEXT("outer_transaction_calls"));FScopedTransaction Transaction(NSLOCTEXT("CubelessMCP","AddMappedExpression","Add Blueprint Math or Boolean Expression"));
    Count(Context,TEXT("shared_add_call_calls"));FResult R=Function::AddCall(Path,TEXT("/Script/Engine.KismetMathLibrary"),FunctionName.ToString(),TEXT("{}"),PositionText,Name,Id,GraphType,Create,false,false,false,false,false);Count(Context,TEXT("shared_add_call_returns"));
    for(const auto& P:Context.Data->Values) { R.Data->SetField(P.Key,P.Value); }
    R.Data->SetStringField(TEXT("mapped_function_name"),FunctionName.ToString());R.Data->SetStringField(TEXT("mapped_function_class"),TEXT("/Script/Engine.KismetMathLibrary"));R.Data->SetBoolField(TEXT("shared_add_call_admission_authoritative"),true);
    FString NodePath;if(R.Data->TryGetStringField(TEXT("object_path"),NodePath))
    { auto* Node=FindObject<UEdGraphNode>(nullptr,*NodePath);if(Loaded(Node)) { LegacyNodeFields(Node,R); }
      else if(R.bSuccess) { R.bChangeStateKnown=false;Fail(R,TEXT("expression_node_metadata_incomplete"),TEXT("Returned shared node is not currently loaded; actual creation prefix retained, no retry.")); } }
    else if(R.bSuccess) { R.bChangeStateKnown=false;Fail(R,TEXT("expression_node_metadata_incomplete"),TEXT("Successful shared call omitted node identity; raw prefix retained.")); }
    return R;
}
}
Function::FResult AddMath(const FString& Path,const FString& Operation,const FString& PositionJson,const FString& Name,const FString& Id,const FString& GraphType,bool Create)
{
    FResult Context;Init(Context,Operation,TEXT("math"));if(!SafeOperation(Operation)) { Fail(Context,TEXT("expression_operation_invalid"),TEXT("Bounded operation without interior NUL required."));return Context; }
    Count(Context,TEXT("operation_mapping_calls"));const FName FunctionName=GetMathFunctionName(Operation);
    if(FunctionName.IsNone()) { Fail(Context,TEXT("expression_math_operation_unsupported"),TEXT("Unsupported math operation: ")+Operation);return Context; }
    return AddMapped(Path,FunctionName,PositionJson,Name,Id,GraphType,Create,Context);
}
Function::FResult AddBoolean(const FString& Path,const FString& Operation,const FString& PositionJson,const FString& Name,const FString& Id,const FString& GraphType,bool Create)
{
    FResult Context;Init(Context,Operation,TEXT("boolean"));if(!SafeOperation(Operation)) { Fail(Context,TEXT("expression_operation_invalid"),TEXT("Bounded operation without interior NUL required."));return Context; }
    Count(Context,TEXT("operation_mapping_calls"));FString Error;const FName FunctionName=GetBooleanFunctionName(Operation,Error);
    if(FunctionName.IsNone()) { Fail(Context,TEXT("expression_boolean_operation_unsupported"),Error);return Context; }
    return AddMapped(Path,FunctionName,PositionJson,Name,Id,GraphType,Create,Context);
}
}
