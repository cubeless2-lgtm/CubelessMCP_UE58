#include "Compatibility/UE58/CubelessMaterialCommentsApi.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpressionComment.h"
#include "MaterialExpressionIO.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::MaterialComments
{
namespace
{
bool Writing=false;
using FJson=FJsonObject;
FString Encode(const TSharedRef<FJson>& R){FString S;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&S));return S;}
FString Authored(UMaterialExpressionComment* C)
{
 FString S;
 for(TFieldIterator<FProperty> P(C->GetClass());P;++P)
 {
  if(P->HasAnyPropertyFlags(CPF_Transient|CPF_DuplicateTransient|CPF_NonPIEDuplicateTransient|CPF_Deprecated)||P->GetFName()==TEXT("GraphNode"))continue;
  FString V;P->ExportTextItem_Direct(V,P->ContainerPtrToValuePtr<void>(C),nullptr,C,PPF_None);
  S+=P->GetName()+TEXT("=")+FString::FromInt(V.Len())+TEXT(":")+V+TEXT("\n");
 }
 return S;
}
// Keep authored values and physical shader inputs separate from compiler/preview
// caches. FunctionOutput::A has no CPF_Edit flag, so editable properties alone
// cannot establish connection preservation. GetInput is the installed public
// expression API and includes custom/dynamic input arrays through its overrides.
struct FShaderValue
{
 FProperty* Property=nullptr;
 FDefaultConstructedPropertyElement Value;
};
bool ShaderAuthored(UMaterialExpression* E,TMap<FString,FString>& Values,FString& Error,TArray<FShaderValue>* Copies=nullptr)
{
 if(!IsValid(E)){Error=TEXT("Expression became invalid");return false;}
 Values.Add(TEXT("$class"),E->GetClass()->GetPathName());
 Values.Add(TEXT("$path"),E->GetPathName());
 int64 Size=0;
 for(TFieldIterator<FProperty> P(E->GetClass());P;++P)
 {
  if(P->HasAnyPropertyFlags(CPF_Transient|CPF_DuplicateTransient|CPF_NonPIEDuplicateTransient|CPF_Deprecated|CPF_Config))continue;
  const FName N=P->GetFName();
  const auto* S=CastField<FStructProperty>(*P);
  const bool Identity=S&&S->Struct->GetFName()==TEXT("Guid");
  if(!P->HasAnyPropertyFlags(CPF_Edit)&&!Identity&&N!=TEXT("SubgraphExpression")&&N!=TEXT("MaterialExpressionEditorX")&&N!=TEXT("MaterialExpressionEditorY"))continue;
  // Input connection masks are read below without transient attributes caches.
  if(S&&(S->Struct->GetFName()==TEXT("ExpressionInput")||S->Struct->GetFName()==TEXT("MaterialAttributesInput")))continue;
  FString V;P->ExportTextItem_Direct(V,P->ContainerPtrToValuePtr<void>(E),nullptr,E,PPF_None);
  if(const auto* Number=CastField<FNumericProperty>(*P);Number&&Number->IsFloatingPoint())V+=FString::Printf(TEXT(" [%.17g]"),Number->GetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(E)));
  Size+=V.Len()*sizeof(TCHAR);if(Size>8*1024*1024){Error=TEXT("Authored expression snapshot exceeds8MiB");return false;}
  if(Copies)
  {
   FShaderValue Copy;Copy.Property=*P;Copy.Value=FDefaultConstructedPropertyElement(*P);
   P->CopyCompleteValue(Copy.Value.GetObjAddress(),P->ContainerPtrToValuePtr<void>(E));Copies->Add(MoveTemp(Copy));
  }
  Values.Add(P->GetName(),MoveTemp(V));
 }
 for(int32 I=0;I<=16384;++I)
 {
  const FExpressionInput* Input=E->GetInput(I);if(!Input)break;
  if(I==16384){Error=TEXT("Expression input snapshot exceeds16384");return false;}
  const FString Prefix=FString::Printf(TEXT("input[%d]."),I);
  // Pointer identity distinguishes callback replacement at an identical path.
  Values.Add(Prefix+TEXT("expression"),FString::Printf(TEXT("%p|%s"),static_cast<void*>(Input->Expression),*GetPathNameSafe(Input->Expression)));
  Values.Add(Prefix+TEXT("output_index"),FString::FromInt(Input->OutputIndex));
  Values.Add(Prefix+TEXT("input_name"),Input->InputName.ToString());
  Values.Add(Prefix+TEXT("mask"),FString::Printf(TEXT("%d,%d,%d,%d,%d"),Input->Mask,Input->MaskR,Input->MaskG,Input->MaskB,Input->MaskA));
 }
 return true;
}
void RootInputs(UMaterial* M,TMap<FString,FString>& Values)
{
 if(!M)return;
 for(int32 I=0;I<MP_MAX;++I)
 {
  FMaterialInputDescription D;if(!M->GetExpressionInputDescription(static_cast<EMaterialProperty>(I),D)||!D.Input)continue;
  const FString Prefix=FString::Printf(TEXT("root_input[%d]."),I);const auto* Input=D.Input;
  Values.Add(Prefix+TEXT("expression"),FString::Printf(TEXT("%p|%s"),static_cast<void*>(Input->Expression),*GetPathNameSafe(Input->Expression)));
  Values.Add(Prefix+TEXT("output_index"),FString::FromInt(Input->OutputIndex));
  Values.Add(Prefix+TEXT("input_name"),Input->InputName.ToString());
  Values.Add(Prefix+TEXT("mask"),FString::Printf(TEXT("%d,%d,%d,%d,%d"),Input->Mask,Input->MaskR,Input->MaskG,Input->MaskB,Input->MaskA));
 }
}
}
FString Run(UObject* Asset,const FString& Action,const FString& Text,int32 X,int32 Y,int32 Width,int32 Height,bool Execute,bool Save)
{
 auto R=MakeShared<FJson>();
 R->SetStringField(TEXT("operation"),Action==TEXT("create")?TEXT("add_material_comment"):Action==TEXT("repair")?TEXT("repair_material_comments"):TEXT("inspect_material_comments"));
 R->SetBoolField(TEXT("success"),false);R->SetStringField(TEXT("stage"),TEXT("preflight"));
 for(const TCHAR* K:{TEXT("mutation_attempted"),TEXT("changed"),TEXT("compile_attempted"),TEXT("save_attempted"),TEXT("saved"),TEXT("atomic_rollback"),TEXT("automatic_retry")})R->SetBoolField(K,false);
 R->SetBoolField(TEXT("change_state_known"),true);
 auto Fail=[&](const TCHAR* E){R->SetStringField(TEXT("error"),E);bool Known=true;R->TryGetBoolField(TEXT("change_state_known"),Known);if(!Known)R->SetStringField(TEXT("retry_risk"),TEXT("inspect_actual_target"));return Encode(R);};
 auto* M=Cast<UMaterial>(Asset);auto* F=Cast<UMaterialFunction>(Asset);
 if(!IsInGameThread()||!GEditor||GEditor->PlayWorld||GEditor->bIsSimulatingInEditor||IsGarbageCollecting()||UE::IsSavingPackage()||IsEngineExitRequested())return Fail(TEXT("Live Editor game thread outside PIE/save/GC/exit required"));
 if(!IsValid(Asset)||(!M&&!F)||!Asset->IsAsset()||Asset->GetOuter()!=Asset->GetOutermost()||!Asset->GetOutermost()->GetName().StartsWith(TEXT("/Game/")))return Fail(TEXT("Explicit /Game top-level Material or MaterialFunction asset required; no instances"));
 if(Action!=TEXT("inspect")&&Action!=TEXT("create")&&Action!=TEXT("repair"))return Fail(TEXT("Unknown comment operation"));
 if(Save&&(!Execute||Action==TEXT("inspect")))return Fail(TEXT("save=true requires an explicit executed create/repair"));
 if(Action==TEXT("create")&&(Text.Len()>16384||Width<1||Height<1||Width>1000000||Height>1000000))return Fail(TEXT("Comment text<=16384 and dimensions1..1000000 required"));
 if(Writing)return Fail(TEXT("Another material comment writer is active"));
 TGuardValue<bool> WriterGuard(Writing,Action!=TEXT("inspect"));
 TStrongObjectPtr<UObject> KeepAsset(Asset);auto* Package=Asset->GetOutermost();TStrongObjectPtr<UPackage> KeepPackage(Package);
 const FString AssetPath=Asset->GetPathName();R->SetStringField(TEXT("asset"),AssetPath);R->SetBoolField(TEXT("dirty_before"),Package->IsDirty());
 auto Collection=[&]() -> FMaterialExpressionCollection& {return M?M->GetExpressionCollection():F->GetExpressionCollection();};
 auto Context=[&]() {return IsValid(Asset)&&IsValid(Package)&&Asset->GetPathName()==AssetPath&&Asset->GetOutermost()==Package&&GEditor&&!GEditor->PlayWorld&&!GEditor->bIsSimulatingInEditor&&!IsGarbageCollecting()&&!UE::IsSavingPackage()&&!IsEngineExitRequested();};
 auto Closed=[&]() {auto* S=GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();return S&&!S->FindEditorForAsset(Asset,false);};
 // Open material editors own preview copies. Never modify source behind an unsaved preview.
 if(Action!=TEXT("inspect")&&!Closed())return Fail(TEXT("Close this asset's Material Editor first; its preview may otherwise overwrite source changes"));
 auto& Initial=Collection();
 if(Initial.Expressions.Num()>4096||Initial.EditorComments.Num()>4096)return Fail(TEXT("Comment operation inventory exceeds4096 per collection"));
 auto ExpectedExpressions=Initial.Expressions;auto ExpectedComments=Initial.EditorComments;
 TArray<TStrongObjectPtr<UObject>> Held;TMap<UMaterialExpressionComment*,FString> ExpectedAuthored;TArray<UMaterialExpressionComment*> Misplaced;
 TMap<UMaterialExpression*,TMap<FString,FString>> ExpectedShader;
 TMap<UMaterialExpression*,TArray<FShaderValue>> ExpectedShaderValues;
 TMap<FString,FString> ExpectedRoot;RootInputs(M,ExpectedRoot);
 TSet<UObject*> SeenExpressions,SeenComments;int64 Bytes=0;
 auto Remember=[&](UMaterialExpressionComment* C) {
  if(!IsValid(C)||C->GetOuter()!=Asset||C->GetOutermost()!=Package)return false;
  if(!ExpectedAuthored.Contains(C)){FString V=Authored(C);Bytes+=V.Len()*sizeof(TCHAR);if(Bytes>8*1024*1024)return false;ExpectedAuthored.Add(C,MoveTemp(V));}
  return true;
 };
 for(auto P:ExpectedExpressions)
 {
  if(!IsValid(P)||P->GetOuter()!=Asset||SeenExpressions.Contains(P))return Fail(TEXT("Invalid/foreign/duplicate expression; no collection edits"));
  SeenExpressions.Add(P);Held.Emplace(P.Get());
  if(auto* C=Cast<UMaterialExpressionComment>(P.Get())){if(!Remember(C))return Fail(TEXT("Comment ownership/snapshot bound invalid"));Misplaced.Add(C);}
  else
  {
   TMap<FString,FString> Values;FString Error;
   TArray<FShaderValue> Copies;
   if(!ShaderAuthored(P.Get(),Values,Error,&Copies)){R->SetStringField(TEXT("snapshot_error"),Error);return Fail(TEXT("Authored shader snapshot unavailable before collection mutation"));}
   for(const auto& V:Values)Bytes+=(V.Key.Len()+V.Value.Len())*sizeof(TCHAR);
   if(Bytes>8*1024*1024)return Fail(TEXT("Combined authored snapshot exceeds8MiB before collection mutation"));
   ExpectedShader.Add(P.Get(),MoveTemp(Values));
   ExpectedShaderValues.Add(P.Get(),MoveTemp(Copies));
  }
 }
 for(auto P:ExpectedComments)
 {
  if(!Remember(P)||SeenComments.Contains(P))return Fail(TEXT("Invalid/foreign/duplicate editor comment; no collection edits"));
  SeenComments.Add(P);Held.Emplace(P.Get());
 }
 auto Stable=[&]() {
  if(!Context()||Collection().Expressions!=ExpectedExpressions||Collection().EditorComments!=ExpectedComments)return false;
  for(const auto& V:ExpectedAuthored)if(!IsValid(V.Key)||V.Key->GetOuter()!=Asset||Authored(V.Key)!=V.Value)return false;
  auto Mismatch=[&](UObject* E,const FString& Key,const FString& Expected,const FString& Actual) {
   if(!R->HasField(TEXT("first_mismatch")))
   {
    auto J=MakeShared<FJson>();J->SetStringField(TEXT("object"),GetPathNameSafe(E));J->SetStringField(TEXT("property"),Key);
    J->SetStringField(TEXT("expected"),Expected.Left(4096));J->SetStringField(TEXT("actual"),Actual.Left(4096));
    J->SetBoolField(TEXT("expected_truncated"),Expected.Len()>4096);J->SetBoolField(TEXT("actual_truncated"),Actual.Len()>4096);R->SetObjectField(TEXT("first_mismatch"),J);
   }
   R->SetBoolField(TEXT("changed"),true);R->SetBoolField(TEXT("change_state_known"),false);return false;
  };
  for(const auto& E:ExpectedShader)
  {
   if(!IsValid(E.Key)||E.Key->GetOuter()!=Asset)return Mismatch(E.Key,TEXT("$identity"),TEXT("original live owned expression"),TEXT("invalid or owner changed"));
   TMap<FString,FString> Actual;FString Error;
   if(!ShaderAuthored(E.Key,Actual,Error))return Mismatch(E.Key,TEXT("$snapshot"),TEXT("complete authored value/input observation"),Error);
   for(const auto& Cell:ExpectedShaderValues.FindChecked(E.Key))
   {
    // Export text is a diagnostic, not equality: floats/aggregate values may
    // print identically after a real authored change. Use the installed typed comparator.
    for(int32 Index=0;Index<Cell.Property->ArrayDim;++Index)
    {
     const void* Current=Cell.Property->ContainerPtrToValuePtr<void>(E.Key,Index);
     const void* Before=static_cast<const uint8*>(Cell.Value.GetObjAddress())+Index*Cell.Property->GetElementSize();
     if(!Cell.Property->Identical(Current,Before,PPF_None))
     {
      const FString Key=Cell.Property->GetName()+(Cell.Property->ArrayDim>1?FString::Printf(TEXT("[%d]"),Index):FString());
      FString B,A;Cell.Property->ExportTextItem_Direct(B,Before,nullptr,E.Key,PPF_None);Cell.Property->ExportTextItem_Direct(A,Current,nullptr,E.Key,PPF_None);
      if(const auto* Number=CastField<FNumericProperty>(Cell.Property);Number&&Number->IsFloatingPoint())
      {B+=FString::Printf(TEXT(" [%.17g]"),Number->GetFloatingPointPropertyValue(Before));A+=FString::Printf(TEXT(" [%.17g]"),Number->GetFloatingPointPropertyValue(Current));}
      return Mismatch(E.Key,Key,B,A);
     }
    }
   }
   TArray<FString> Keys;E.Value.GetKeys(Keys);for(const auto& V:Actual)Keys.AddUnique(V.Key);Keys.Sort();
   for(const FString& Key:Keys)
   {
    const FString* B=E.Value.Find(Key);const FString* A=Actual.Find(Key);
    if(!B||!A||*A!=*B)return Mismatch(E.Key,Key,B?*B:TEXT("<absent>"),A?*A:TEXT("<absent>"));
   }
  }
  TMap<FString,FString> ActualRoot;RootInputs(M,ActualRoot);
  TArray<FString> RootKeys;ExpectedRoot.GetKeys(RootKeys);for(const auto& V:ActualRoot)RootKeys.AddUnique(V.Key);RootKeys.Sort();
  for(const FString& Key:RootKeys)
  {
   const FString* B=ExpectedRoot.Find(Key);const FString* A=ActualRoot.Find(Key);
   if(!B||!A||*B!=*A)return Mismatch(Asset,Key,B?*B:TEXT("<absent>"),A?*A:TEXT("<absent>"));
  }
  return Context();
 };
 auto Inventory=[&]() {
  if(!Context())return;
  auto& C=Collection();R->SetNumberField(TEXT("expression_count"),C.Expressions.Num());R->SetNumberField(TEXT("editor_comment_count"),C.EditorComments.Num());
  int32 Bad=0;for(auto E:C.Expressions)if(IsValid(E)&&E->IsA<UMaterialExpressionComment>())++Bad;
  R->SetNumberField(TEXT("misplaced_comment_count"),Bad);R->SetBoolField(TEXT("dirty_after"),Package->IsDirty());
  TArray<TSharedPtr<FJsonValue>> Rows;TSet<UMaterialExpressionComment*> Seen;
  auto Add=[&](UMaterialExpressionComment* E){if(!IsValid(E)||Seen.Contains(E))return;Seen.Add(E);auto J=MakeShared<FJson>();J->SetStringField(TEXT("path"),E->GetPathName());J->SetStringField(TEXT("guid"),E->MaterialExpressionGuid.ToString());J->SetStringField(TEXT("text"),E->Text);J->SetNumberField(TEXT("x"),E->MaterialExpressionEditorX);J->SetNumberField(TEXT("y"),E->MaterialExpressionEditorY);J->SetNumberField(TEXT("width"),E->SizeX);J->SetNumberField(TEXT("height"),E->SizeY);J->SetBoolField(TEXT("in_expressions"),C.Expressions.Contains(E));J->SetBoolField(TEXT("in_editor_comments"),C.EditorComments.Contains(E));Rows.Add(MakeShared<FJsonValueObject>(J));};
  for(auto E:C.Expressions)Add(Cast<UMaterialExpressionComment>(E));for(auto E:C.EditorComments)Add(E);R->SetArrayField(TEXT("comments"),Rows);
 };
 if(!Stable())return Fail(TEXT("Source changed during preflight observation; no explicit collection writer"));
 Inventory();R->SetNumberField(TEXT("would_repair_count"),Misplaced.Num());
 R->SetStringField(TEXT("observation_scope"),TEXT("Exact collection membership/order, original comment authored properties, typed shader editable values/identity and physical expression/root Material input connections; excludes transient GraphNode, compiler/preview caches and arbitrary global engine state"));
 if(Action==TEXT("inspect")||!Execute||(Action==TEXT("repair")&&Misplaced.IsEmpty())){R->SetBoolField(TEXT("dry_run"),Action!=TEXT("inspect")&&!Execute);R->SetBoolField(TEXT("success"),true);R->SetStringField(TEXT("stage"),TEXT("observed"));return Encode(R);}
 if(!Stable()||!Closed())return Fail(TEXT("Target/editor state changed before first writer"));
 R->SetStringField(TEXT("stage"),TEXT("modify_target"));R->SetBoolField(TEXT("mutation_attempted"),true);R->SetBoolField(TEXT("change_state_known"),false);
 Asset->Modify(false);
 if(!Stable()||!Closed()){Inventory();return Fail(TEXT("Target/editor changed in Modify callback; inspect source before another request"));}
 int32 Repaired=0;
 if(Action==TEXT("repair"))
 {
  // Public collection operations do not destroy/recreate comments or run shader updates.
  for(auto* C:Misplaced){Collection().RemoveExpression(C);Collection().AddComment(C);ExpectedExpressions.Remove(C);ExpectedComments.AddUnique(C);++Repaired;}
 }
 else
 {
  auto* C=NewObject<UMaterialExpressionComment>(Asset,NAME_None,RF_Transactional);
  if(!IsValid(C)||!Stable()||!Closed()){Inventory();return Fail(TEXT("Target changed during comment allocation; allocation is not rollback"));}
  Held.Emplace(C);C->Text=Text;C->MaterialExpressionEditorX=X;C->MaterialExpressionEditorY=Y;C->SizeX=Width;C->SizeY=Height;C->MaterialExpressionGuid=FGuid::NewGuid();C->Material=M;C->Function=F;
  Collection().AddComment(C);ExpectedComments.Add(C);ExpectedAuthored.Add(C,Authored(C));R->SetStringField(TEXT("created_comment"),C->GetPathName());
 }
 R->SetBoolField(TEXT("changed"),true);R->SetNumberField(TEXT("repaired_count"),Repaired);
 R->SetStringField(TEXT("stage"),TEXT("mark_package_dirty"));Asset->MarkPackageDirty();
 Inventory();if(!Stable()||!Closed())return Fail(TEXT("Target/editor changed after collection commit or dirty notification; partial changes retained"));
 R->SetBoolField(TEXT("change_state_known"),true);R->SetBoolField(TEXT("editor_reopen_required"),true);R->SetBoolField(TEXT("shader_recompile_requested"),false);
 if(Save)
 {
  R->SetStringField(TEXT("stage"),TEXT("save_asset"));R->SetBoolField(TEXT("save_attempted"),true);R->SetBoolField(TEXT("change_state_known"),false);
  const bool Saved=UEditorAssetLibrary::SaveLoadedAsset(Asset,false);R->SetBoolField(TEXT("saved"),Saved);Inventory();
  if(!Stable()||!Closed())return Fail(TEXT("Target/editor changed during save callback; actual save receipt retained"));
  R->SetBoolField(TEXT("change_state_known"),true);
  if(!Saved||Package->IsDirty())return Fail(TEXT("Save failed or package remains dirty; collection changes retained, no retry"));
 }
 R->SetStringField(TEXT("stage"),TEXT("complete"));R->SetBoolField(TEXT("success"),true);return Encode(R);
}
}
