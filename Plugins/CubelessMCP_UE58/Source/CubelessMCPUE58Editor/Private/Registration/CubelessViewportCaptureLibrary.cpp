#include "Registration/CubelessViewportCaptureLibrary.h"
#include "Compatibility/UE58/CubelessViewportCaptureApi.h"
#include "Operations/CubelessVariableOperations.h"

FString UCubelessViewportCaptureLibrary::ListViewportBookmarks()
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::ViewportCapture::ListBookmarks(),TEXT("list_viewport_bookmarks"));
}
FString UCubelessViewportCaptureLibrary::SetActiveViewportRealtime(FString ParamsJson)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::ViewportCapture::SetRealtime(ParamsJson),TEXT("set_active_viewport_realtime"));
}
FString UCubelessViewportCaptureLibrary::CaptureViewportBookmarkScreenshot(FString ParamsJson)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::ViewportCapture::CaptureBookmark(ParamsJson),TEXT("capture_viewport_bookmark_screenshot"));
}
FString UCubelessViewportCaptureLibrary::TakeScreenshot(FString ParamsJson)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::ViewportCapture::Take(ParamsJson),TEXT("take_screenshot"));
}
