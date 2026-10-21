// Separate the unexecuted pure-C++ allocation path from historical Python-wrapper crashes.
#include "CubelessPCGNativeCanaryLibrary.h"
#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Elements/PCGCreatePoints.h"
#include "Misc/Guid.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGSettings.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace Cubeless::UE58::PCGNativeCanary
{
namespace
{
struct FState
{
    FString Id,Stage=TEXT("never_started"),Error,GraphPath,NodePath,SettingsPath;
    TStrongObjectPtr<UPCGGraph> Graph;
    TStrongObjectPtr<UPCGNode> Node;
    TStrongObjectPtr<UPCGSettings> Settings;
    FTSTicker::FDelegateHandle Ticker;
    FDelegateHandle PreExit;
    uint64 CreatedFrame=0,ObservedFrame=0,InspectedFrame=0,Epoch=0;
    int32 BeforeNodes=0,AfterNodes=0;
    bool NewObjectEntered=false,NewObjectReturned=false,AddNodeEntered=false,AddNodeReturned=false;
    bool Active=false,Observed=false,Released=false,RetainedUnsafe=false,UnownedReturnMayRemain=false;
    bool InspectionAttempted=false,InspectionMembership=false;
    bool InNativeCall=false,Aborted=false,RetainedAfterAbort=false,ExecutionScopePinUsed=false;
};
// Process-lifetime bookkeeping deliberately does not destruct strong refs during exit purge.
FState& State(){static FState* S=new FState();return *S;}
bool Safe(bool AllowExit=false){return IsInGameThread()&&!IsGarbageCollecting()&&!UE::IsSavingPackage()&&!GExitPurge&&(AllowExit||!IsEngineExitRequested());}
FString Encode()
{
    const auto& S=State();auto J=MakeShared<FJsonObject>();
    J->SetStringField(TEXT("operation"),TEXT("isolated_pcg_native_allocation_canary"));
    J->SetStringField(TEXT("id"),S.Id);J->SetStringField(TEXT("stage"),S.Stage);J->SetStringField(TEXT("error"),S.Error);
    J->SetBoolField(TEXT("success"),S.Observed&&S.Error.IsEmpty()&&!S.Aborted);J->SetBoolField(TEXT("active"),S.Active);
    J->SetBoolField(TEXT("new_object_entered"),S.NewObjectEntered);J->SetBoolField(TEXT("new_object_returned"),S.NewObjectReturned);
    J->SetBoolField(TEXT("add_node_entered"),S.AddNodeEntered);J->SetBoolField(TEXT("add_node_returned"),S.AddNodeReturned);
    J->SetBoolField(TEXT("later_GT_frame_membership_observed"),S.Observed);J->SetBoolField(TEXT("strong_refs_released"),S.Released);
    J->SetBoolField(TEXT("retained_due_to_unsafe_lifecycle"),S.RetainedUnsafe);
    J->SetBoolField(TEXT("unowned_return_may_remain"),S.UnownedReturnMayRemain);
    J->SetBoolField(TEXT("aborted"),S.Aborted);J->SetBoolField(TEXT("strong_refs_retained_after_abort"),S.RetainedAfterAbort);
    J->SetBoolField(TEXT("execution_scope_pin_used"),S.ExecutionScopePinUsed);
    J->SetBoolField(TEXT("inspection_attempted"),S.InspectionAttempted);J->SetBoolField(TEXT("inspection_membership"),S.InspectionMembership);
    J->SetNumberField(TEXT("created_frame"),static_cast<double>(S.CreatedFrame));J->SetNumberField(TEXT("observed_frame"),static_cast<double>(S.ObservedFrame));
    J->SetNumberField(TEXT("inspected_frame"),static_cast<double>(S.InspectedFrame));
    J->SetNumberField(TEXT("nodes_before"),S.BeforeNodes);J->SetNumberField(TEXT("nodes_after"),S.AfterNodes);
    J->SetStringField(TEXT("graph_path"),S.GraphPath);J->SetStringField(TEXT("node_path"),S.NodePath);J->SetStringField(TEXT("settings_path"),S.SettingsPath);
    J->SetBoolField(TEXT("Python_UObject_allocation"),false);J->SetBoolField(TEXT("canary_objects_exposed_as_Python_UObject_wrappers"),false);J->SetBoolField(TEXT("GC_policy_changed"),false);
    J->SetNumberField(TEXT("disk_or_asset_saves"),0);J->SetNumberField(TEXT("generation_calls"),0);J->SetNumberField(TEXT("compile_calls"),0);
    J->SetBoolField(TEXT("PCG_gate_changed"),false);J->SetBoolField(TEXT("full_PCG_safety_proven"),false);
    J->SetStringField(TEXT("scope"),TEXT("One Native transient graph and stock CreatePoints node only; no component generation, persistent graph, Blueprint compile, global SDK purity, Python weakref fix, or packaging proof."));
    FString Out;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Out));return Out;
}
bool Membership()
{
    auto& S=State();if(!Safe())return false;
    auto* G=S.Graph.Get();auto* N=S.Node.Get();auto* T=S.Settings.Get();
    return IsValid(G)&&IsValid(N)&&IsValid(T)&&G->GetOutermost()==GetTransientPackage()&&G->HasAnyFlags(RF_Transient)
        &&G->GetPathName()==S.GraphPath&&N->GetPathName()==S.NodePath&&T->GetPathName()==S.SettingsPath
        &&N->GetOuter()==G&&T->GetOuter()==N&&G->GetNodes().Num()==S.BeforeNodes+1
        &&G->GetNodes().Contains(N)&&N->GetSettings()==T&&T->IsA<UPCGCreatePointsSettings>();
}
bool Interrupted(uint64 Expected)
{
    auto& S=State();
    if(S.Epoch==Expected&&!S.Aborted)return false;
    S.Active=false;S.Stage=TEXT("aborted_during_native_dispatch");
    if(S.Error.IsEmpty())S.Error=TEXT("Shutdown interrupted this exact attempt; returned prefix retained, no further pin/observation/ticker");
    S.RetainedAfterAbort=S.Graph.Get()!=nullptr;return true;
}
bool Tick(float)
{
    auto& S=State();
    if(!Safe()){S.Active=false;S.RetainedUnsafe=true;S.Stage=TEXT("lifecycle_unsafe_retained");S.Error=TEXT("No further object reads or release during GC/exit/save/purge");S.Ticker.Reset();return false;}
    if(GFrameCounter<=S.CreatedFrame)return true;
    S.ObservedFrame=GFrameCounter;S.Observed=Membership();S.Active=false;S.Ticker.Reset();
    S.Stage=S.Observed?TEXT("later_frame_observed_awaiting_explicit_release"):TEXT("membership_failed_retained");
    if(!S.Observed)S.Error=TEXT("Exact transient graph/node/settings membership changed; no reconstruction or replay");
    return false;
}
void OnPreExit(){Shutdown();}
}
void Shutdown()
{
    auto& S=State();
    ++S.Epoch;
    if(S.InNativeCall){S.Aborted=true;S.Stage=TEXT("shutdown_during_native_dispatch");S.RetainedAfterAbort=S.Graph.Get()!=nullptr;}
    if(S.Ticker.IsValid()){FTSTicker::GetCoreTicker().RemoveTicker(S.Ticker);S.Ticker.Reset();}
    if(S.PreExit.IsValid()){FEditorDelegates::OnEditorPreExit.Remove(S.PreExit);S.PreExit.Reset();}
    S.Active=false;
    // A synchronous SDK callback may request shutdown. Never remove that call's
    // execution pins, and invalidate its continuation before it can resume.
    if(S.InNativeCall)return;
    if(!Safe(true)){S.RetainedUnsafe=true;return;}
    S.Settings.Reset();S.Node.Reset();S.Graph.Reset();S.Released=true;
    S.RetainedUnsafe=false;S.RetainedAfterAbort=false;
}
}

FString UCubelessPCGNativeCanaryLibrary::BeginPCGNativeAllocationCanary()
{
    using namespace Cubeless::UE58::PCGNativeCanary;
    if(!Safe())return TEXT("{\"success\":false,\"error\":\"Safe Editor GT outside GC/exit/save/purge required\"}");
    auto& S=State();
    if(!S.Id.IsEmpty())return TEXT("{\"success\":false,\"error\":\"One attempt per Editor process; inspect existing evidence, no replay\"}");
    TGuardValue<bool> InCall(S.InNativeCall,true);const uint64 Epoch=++S.Epoch;
    S.Id=FGuid::NewGuid().ToString(EGuidFormats::Digits);S.CreatedFrame=GFrameCounter;
    S.PreExit=FEditorDelegates::OnEditorPreExit.AddStatic(&OnPreExit);
    S.Stage=TEXT("before_native_new_graph");S.NewObjectEntered=true;
    UPCGGraph* G=NewObject<UPCGGraph>(GetTransientPackage(),NAME_None,RF_Transient);
    S.NewObjectReturned=true;
    if(Interrupted(Epoch)){S.UnownedReturnMayRemain=G!=nullptr&&S.Graph.Get()==nullptr;return Encode();}
    if(!Safe()){S.UnownedReturnMayRemain=G!=nullptr;S.Error=TEXT("Lifecycle changed after NewObject; returned object may remain unowned; no further object reads or strong-ref admission");return Encode();}
    if(!IsValid(G)){S.Error=TEXT("NewObject returned no graph");return Encode();}
    S.Graph.Reset(G);S.GraphPath=G->GetPathName();S.BeforeNodes=G->GetNodes().Num();
    const TStrongObjectPtr<UPCGGraph> ExecutionGraph(G);S.ExecutionScopePinUsed=true;
    UPCGSettings* Settings=nullptr;S.Stage=TEXT("before_native_add_stock_node");S.AddNodeEntered=true;
    UPCGNode* N=G->AddNodeOfType(UPCGCreatePointsSettings::StaticClass(),Settings);
    S.AddNodeReturned=true;
    if(Interrupted(Epoch))return Encode();
    if(!Safe()){S.RetainedUnsafe=true;S.Error=TEXT("Lifecycle changed after AddNode; no further object reads");return Encode();}
    if(!IsValid(N)||!IsValid(Settings)){S.Error=TEXT("AddNode returned an incomplete graph/node/settings prefix");return Encode();}
    S.Node.Reset(N);S.Settings.Reset(Settings);S.NodePath=N->GetPathName();S.SettingsPath=Settings->GetPathName();S.AfterNodes=G->GetNodes().Num();
    if(!Membership()){S.Error=TEXT("Initial exact ownership postcondition failed; no delete/reconstruct");return Encode();}
    S.Active=true;S.Stage=TEXT("awaiting_later_GT_frame");S.Ticker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick));
    if(!S.Ticker.IsValid()){S.Active=false;S.Error=TEXT("Ticker admission failed; retained for explicit safe release");}
    return Encode();
}
FString UCubelessPCGNativeCanaryLibrary::InspectPCGNativeAllocationCanary()
{
    using namespace Cubeless::UE58::PCGNativeCanary;
    if(!Safe())return TEXT("{\"success\":false,\"error\":\"Safe GT required; no UObject observation performed\"}");
    auto& S=State();
    if(S.Observed&&!S.Released)
    {
        S.InspectionAttempted=true;S.InspectedFrame=GFrameCounter;S.InspectionMembership=Membership();
        if(!S.InspectionMembership){S.Error=TEXT("Held membership changed at inspect boundary; no reconstruction or replay");S.Stage=TEXT("inspect_membership_failed");}
    }
    return Encode();
}
FString UCubelessPCGNativeCanaryLibrary::ReleasePCGNativeAllocationCanary()
{
    using namespace Cubeless::UE58::PCGNativeCanary;
    if(!Safe())return TEXT("{\"success\":false,\"error\":\"Safe GT required; retained, no unsafe release\"}");
    auto& S=State();if(S.InNativeCall||S.Active||S.Id.IsEmpty())return TEXT("{\"success\":false,\"error\":\"A terminal owned attempt outside SDK dispatch is required before release\"}");
    Shutdown();S.Stage=TEXT("released_after_terminal_canary" );return Encode();
}
