#if WITH_DEV_AUTOMATION_TESTS
#include "RXSilhouetteSettings.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRXSilhouetteSettingsTest, "RX.SMLODGen.SilhouetteSelections", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRXSilhouetteSettingsTest::RunTest(const FString&)
{
    FRXSectionWeight Default;
    TestFalse(TEXT("New settings do not pin open boundaries"), Default.bPreserveOpenBoundary);
    TestFalse(TEXT("New settings do not pin sharp edges"), Default.bPreserveSharpEdges);
    TestFalse(TEXT("New settings do not pin attribute seams"), Default.bPreserveAttributeSeams);
    for (int32 Mask = 0; Mask < 8; ++Mask)
    {
        FRXSectionWeight Before;
        Before.bPreserveOpenBoundary = (Mask & 1) != 0;
        Before.bPreserveSharpEdges = (Mask & 2) != 0;
        Before.bPreserveAttributeSeams = (Mask & 4) != 0;
        FJsonObject Json; RXSilhouetteSettings::Write(Json, Before);
        FRXSectionWeight After;
        TestTrue(TEXT("Read all independent selections"), RXSilhouetteSettings::Read(Json, 4, After));
        TestTrue(TEXT("All eight combinations round trip"), Before.HasSameProtection(After));
        TestEqual(TEXT("All-off disables protection"), After.HasProtection(), Mask != 0);
    }
    for (bool Enabled : { false, true })
    {
        FJsonObject Old; Old.SetBoolField(TEXT("preserve_silhouette"), Enabled);
        FRXSectionWeight After;
        TestTrue(TEXT("Read v3 aggregate option"), RXSilhouetteSettings::Read(Old, 3, After));
        TestTrue(TEXT("Legacy selection applies equally to all categories"), After.bPreserveOpenBoundary == Enabled && After.bPreserveSharpEdges == Enabled && After.bPreserveAttributeSeams == Enabled);
    }
    FJsonObject Old; FRXSectionWeight After;
    TestTrue(TEXT("Read old v2 with no protection option"), RXSilhouetteSettings::Read(Old, 2, After));
    TestFalse(TEXT("Old saved reduction behavior stays unchanged"), After.HasProtection());
    FJsonObject Invalid; RXSilhouetteSettings::Write(Invalid, Default);
    Invalid.SetStringField(TEXT("preserve_sharp_edges"), TEXT("true"));
    TestFalse(TEXT("V4 rejects string flags"), RXSilhouetteSettings::Read(Invalid, 4, After));
    return true;
}
#endif
