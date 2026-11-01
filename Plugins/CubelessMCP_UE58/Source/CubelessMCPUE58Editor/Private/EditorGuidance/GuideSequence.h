#pragma once
#include "CoreMinimal.h"
#include "Layout/SlateRect.h"
class SWindow;

namespace Cubeless::EditorGuidance::Sequence
{
    // Certificates are issued only by an actual guidance paint, never by a
    // caller's assertion. The callback holds weak semantic owners only.
    bool Certify(const FString& Token, const FString& Action, const FString& Arguments,
        TWeakPtr<SWindow> Window, TFunction<bool()> IsCurrent);
    bool HasCertificate(const FString& Token);
    void ObservePaint(const FString& Token, TWeakPtr<SWindow> Window, const FSlateRect& Bounds);
    bool OwnsActiveWindowFor(const TSharedPtr<SWindow>& TargetWindow, const TSharedPtr<SWindow>& ActiveWindow);
    FString Request(const FString& Action, const FString& Arguments);
    void Shutdown();
}
