#include "Compatibility/UE58/CubelessChaosPerformanceBreakListener.h"
#include "UObject/StrongObjectPtr.h"
void UCubelessChaosPerformanceBreakListener::HandleBreak(const FChaosBreakEvent& Event)
{
    if (IsInGameThread() && bAcceptEvents && Callback)
    {
        TStrongObjectPtr<UCubelessChaosPerformanceBreakListener> Self(this);
        TFunction<void(const FChaosBreakEvent&)> LocalCallback = Callback;
        LocalCallback(Event);
    }
}
