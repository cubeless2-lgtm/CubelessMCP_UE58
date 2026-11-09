#include "CBMContactEvidence.h"
#include "Misc/AutomationTest.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCBMContactIntervalTest,"Cubeless.BodyMocap.Contact.IntervalEvidence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCBMContactIntervalTest::RunTest(const FString&) {
    using namespace CubelessContact;
    TestTrue(TEXT("Landing does not lock the preceding airborne interval"),Interval(-8.f,8.f)<.001f);
    TestTrue(TEXT("Takeoff releases the interval"),Interval(8.f,-8.f)<.001f);
    TestEqual(TEXT("Time reversal is symmetric"),Interval(-2.f,5.f),Interval(5.f,-2.f));
    TestEqual(TEXT("Sustained stance keeps its original strength"),Interval(5.f,5.f),Probability(5.f));
    TestTrue(TEXT("Toe-only support does not promote the heel"),Interval(-8.f,-8.f)<.001f && Interval(8.f,8.f)>.999f);
    TestEqual(TEXT("Unknown confidence cannot establish contact"),Interval(std::numeric_limits<float>::quiet_NaN(),8.f),0.f);
    TestTrue(TEXT("Extreme logits remain finite"),std::isfinite(Probability(-1000.f)) && std::isfinite(Probability(1000.f)));
    return true;
}
#endif
