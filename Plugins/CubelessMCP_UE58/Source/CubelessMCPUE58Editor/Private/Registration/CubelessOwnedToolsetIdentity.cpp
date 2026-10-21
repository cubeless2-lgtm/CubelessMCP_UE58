#include "Registration/CubelessOwnedToolsetIdentity.h"
#include "CoreMinimal.h"
#include "ToolsetRegistry/Toolset.h"
#include "ToolsetRegistry/ToolsetRegistry.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace Cubeless::Registration
{
bool HasRetainedToolsetIdentity(UE::ToolsetRegistry::FToolsetRegistry& Registry,
    const TSharedPtr<UE::ToolsetRegistry::FToolset>& Retained)
{
    if (!IsInGameThread() || !Retained.IsValid()) { return false; }
    const bool WasEnabled = Retained->IsEnabled();
    if (!WasEnabled) { Retained->SetEnabled(true); }
    const bool Matches = Registry.Find(Retained->GetToolsetName()) == Retained;
    if (!WasEnabled) { Retained->SetEnabled(false); }
    return Matches;
}

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
class FOwnedIdentityFixture final : public UE::ToolsetRegistry::FToolset
{
public:
    virtual FString GetToolsetName() const override { return TEXT("Cubeless.MCP.OwnerFixture"); }
    virtual FString GetToolsetVersion() const override { return TEXT("1.0"); }
    virtual FString GetToolsetDescription() const override { return TEXT("Local Registry identity fixture; never executes."); }
protected:
    virtual FString GetJsonSchemaInternal() const override
    { return TEXT("{\"tools\":[{\"name\":\"Cubeless.MCP.OwnerFixture.Probe\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}},\"outputSchema\":{\"type\":\"object\",\"properties\":{}}}]}"); }
    virtual TFuture<TValueOrError<FString,FString>> ExecuteToolInternal(const FString&,const FString&) override
    { return MakeFulfilledPromise<TValueOrError<FString,FString>>(MakeError(FString(TEXT("Fixture dispatch is not an operation")))).GetFuture(); }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOwnedToolsetFilteredIdentityTest,
    "Cubeless.MCP.Registry.FilteredOwnerIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOwnedToolsetFilteredIdentityTest::RunTest(const FString&)
{
    UE::ToolsetRegistry::FToolsetRegistry Registry;
    TSharedPtr<UE::ToolsetRegistry::FToolset> Owned = MakeShared<FOwnedIdentityFixture>();
    TestTrue(TEXT("Initial owner registered"), Registry.RegisterToolset(Owned));
    TestTrue(TEXT("Enabled owner identity"), HasRetainedToolsetIdentity(Registry,Owned));
    Registry.AddBlockedName(Owned->GetToolsetName());
    const auto Blocked = Registry.GetBlockedNames(), Allowed = Registry.GetAllowedNames();
    TestFalse(TEXT("Filtered Find hides owner"), Registry.Find(Owned->GetToolsetName()).IsValid());
    TestFalse(TEXT("Owner initially disabled"), Owned->IsEnabled());
    const bool Matches = HasRetainedToolsetIdentity(Registry,Owned);
    TestTrue(TEXT("Filtered owner still identified"), Matches);
    TestFalse(TEXT("Owner enable state restored"), Owned->IsEnabled());
    TestTrue(TEXT("Global filter arrays unchanged"), Registry.GetBlockedNames()==Blocked && Registry.GetAllowedNames()==Allowed);
    if (Matches) { TestTrue(TEXT("Actual filtered owner removal"), Registry.UnregisterToolset(Owned)); }
    TSharedPtr<UE::ToolsetRegistry::FToolset> Foreign = MakeShared<FOwnedIdentityFixture>();
    TestTrue(TEXT("New same-name handler admitted after removal"), Registry.RegisterToolset(Foreign));
    TestFalse(TEXT("Foreign replacement inherits filter"), Foreign->IsEnabled());
    TestFalse(TEXT("Detached retained owner is not replacement"), HasRetainedToolsetIdentity(Registry,Owned));
    TestFalse(TEXT("Foreign filtered state untouched"), Foreign->IsEnabled());
    TestFalse(TEXT("Detached owner state restored"), Owned->IsEnabled());
    TestTrue(TEXT("Replacement probe leaves global filters intact"), Registry.GetBlockedNames()==Blocked && Registry.GetAllowedNames()==Allowed);
    Registry.RemoveBlockedName(Owned->GetToolsetName());
    TestTrue(TEXT("Foreign handler preserved"), Registry.Find(Owned->GetToolsetName())==Foreign);
    TestFalse(TEXT("Enabled foreign replacement is not retained owner"), HasRetainedToolsetIdentity(Registry,Owned));
    TestTrue(TEXT("Local fixture cleanup"), Registry.UnregisterToolset(Foreign));
    return true;
}
#endif
}
