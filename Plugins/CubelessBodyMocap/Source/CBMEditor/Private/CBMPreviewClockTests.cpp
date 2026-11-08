#include "CBMPreviewClock.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCBMPreviewClockTest,"Cubeless.BodyMocap.Preview.DelayedSeekDuringScrub",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCBMPreviewClockTest::RunTest(const FString&){
 FCBMPreviewClock C;
 C.Scrubbing=true;C.Request(3.2,20.1);C.Observe(0,20.1,.0167);
 TestEqual(TEXT("Dragging evaluates requested pose before decoder catches up"),C.Position,3.2);
 C.Request(7.7,20.1);C.Observe(3.2,20.1,.0167);
 TestEqual(TEXT("Older seek completion cannot replace newest drag"),C.Position,7.7);
 C.Scrubbing=false;C.Observe(3.2,20.1,.0167);
 TestEqual(TEXT("Releasing retains final target while seeking"),C.Position,7.7);
 C.Observe(7.7,20.1,.0167);TestFalse(TEXT("Matching decoder releases pending seek"),C.Pending);
 C.Observe(7.8,20.1,.0167);TestEqual(TEXT("Playback follows media clock"),C.Position,7.8);
 C.Request(.3,20.1);C.Observe(7.8,20.1,.0167);TestEqual(TEXT("Reverse scrubbing also resists stale time"),C.Position,.3);
 C.Request(100,20.1);TestEqual(TEXT("Clamp end"),C.Position,20.1);
 C.Reset();TestFalse(TEXT("New video clears pending seek"),C.Pending);TestEqual(TEXT("New video starts at zero"),C.Position,0.);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCBMContinuousClockTest,"Cubeless.BodyMocap.Preview.ContinuousPlayback",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCBMContinuousClockTest::RunTest(const FString&){
 FCBMPreviewClock C;const double F=1./30.,D=1./60.;
 C.Advance(0,10,F,D,true);C.Advance(0,10,F,D,true);
 TestTrue(TEXT("Repeated decoder timestamp still advances animation"),FMath::IsNearlyEqual(C.Position,D,1e-9));
 C.Advance(F,10,F,D,true);const double Last=C.Position;C.Advance(F,10,F,D,true);
 TestTrue(TEXT("Between-frame pose advances"),C.Position>Last&&C.Position<F*2);
 C.Advance(F,10,F,1,true);TestTrue(TEXT("Stalled decoder lead is bounded by one video frame"),C.Position<=F*2);
 C.Advance(F*2,10,F,D,true);TestTrue(TEXT("Next decoded frame does not rewind"),C.Position>=F*2);
 C.Advance(F*2,10,F,D,false);TestEqual(TEXT("Pause evaluates exact media time"),C.Position,F*2);
 C.Request(7.7,10);C.Advance(1,10,F,D,true);TestEqual(TEXT("Pending seek is never interpolated"),C.Position,7.7);
 C.Advance(7.7,10,F,D,false);TestFalse(TEXT("Matching seek completes"),C.Pending);
 C.Advance(8,10,F,D,true);C.Advance(.1,10,F,D,true);TestEqual(TEXT("Backward seek or loop resynchronizes"),C.Position,.1);
 C.Scrubbing=true;C.Request(3.2,10);C.Advance(8,10,F,D,true);TestEqual(TEXT("Scrub remains immediate"),C.Position,3.2);
 return true;
}
#endif
