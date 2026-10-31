#include "CubelessStudyChooserLibrary.h"
#include "Editor.h"
#include "Misc/PackageName.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "UObject/Package.h"

FString UCubelessStudyChooserLibrary::RemapNewStudyChooser(UObject* Target, const TMap<UObject*, UObject*>& Replacements)
{
 auto Reject=[](const TCHAR* Reason){return FString::Printf(TEXT("{\"success\":false,\"mutation_attempted\":false,\"reason\":\"%s\"}"),Reason);};
 if(!IsInGameThread()||!GEditor||GEditor->PlayWorld) return Reject(TEXT("editor_idle_required"));
 if(!IsValid(Target)||Target->GetClass()->GetPathName()!=TEXT("/Script/Chooser.ChooserTable"))return Reject(TEXT("chooser_required"));
 UPackage* Package=Target->GetPackage();
 const FString Name=Package->GetName();
 if(!Name.StartsWith(TEXT("/Game/Study/"))||FPackageName::DoesPackageExist(Name)||!Package->IsDirty())return Reject(TEXT("new_unsaved_owned_study_chooser_required"));
 if(Replacements.IsEmpty()||Replacements.Num()>16)return Reject(TEXT("bounded_replacement_set_required"));
 for(const auto& Pair:Replacements)
 {
  if(!IsValid(Pair.Key)||!IsValid(Pair.Value)||Pair.Key==Pair.Value)return Reject(TEXT("invalid_replacement"));
  const bool Chooser=Pair.Key->GetClass()->GetPathName()==TEXT("/Script/Chooser.ChooserTable")&&Pair.Value->GetClass()==Pair.Key->GetClass();
  const bool Classes=Cast<UClass>(Pair.Key)&&Cast<UClass>(Pair.Value)&&Cast<UClass>(Pair.Key)->GetSuperClass()==Cast<UClass>(Pair.Value)->GetSuperClass();
  if((!Chooser&&!Classes)||!Pair.Value->GetPackage()->GetName().StartsWith(TEXT("/Game/Study/"))||Pair.Key->GetPackage()->IsDirty())return Reject(TEXT("saved_source_and_owned_compatible_destination_required"));
 }
 // All referenced objects are held for the complete synchronous archive operation.
 TArray<TStrongObjectPtr<UObject>> Holds;Holds.Emplace(Target);
 for(const auto& Pair:Replacements){Holds.Emplace(Pair.Key);Holds.Emplace(Pair.Value);}
 TMap<UObject*,UObject*> Mutable=Replacements;
 // No Modify/PreEdit/PostEdit callback is invoked: this narrowly remaps a fresh duplicate.
 FArchiveReplaceObjectRef<UObject> Archive(Target,Mutable,EArchiveReplaceObjectFlags::IgnoreOuterRef|EArchiveReplaceObjectFlags::IgnoreArchetypeRef);
 // Package is already dirty by admission. Do not broadcast a redundant
 // PackageMarkedDirtyEvent here; observers could mutate/save the fresh copy.
 const bool Known=IsValid(Target)&&Target->GetPackage()==Package&&Target->GetClass()->GetPathName()==TEXT("/Script/Chooser.ChooserTable");
 return FString::Printf(TEXT("{\"success\":%s,\"mutation_attempted\":true,\"change_state_known\":%s,\"replacement_count\":%lld,\"save_attempted\":false,\"atomic_rollback\":false}"),Known?TEXT("true"):TEXT("false"),Known?TEXT("true"):TEXT("false"),static_cast<long long>(Archive.GetCount()));
}
