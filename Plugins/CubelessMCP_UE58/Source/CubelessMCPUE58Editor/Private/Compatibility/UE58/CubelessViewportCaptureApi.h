#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"

namespace Cubeless::UE58::ViewportCapture
{
Variable::FResult ListBookmarks();
Variable::FResult SetRealtime(const FString& ParamsJson);
Variable::FResult CaptureBookmark(const FString& ParamsJson);
Variable::FResult Take(const FString& ParamsJson);
}
