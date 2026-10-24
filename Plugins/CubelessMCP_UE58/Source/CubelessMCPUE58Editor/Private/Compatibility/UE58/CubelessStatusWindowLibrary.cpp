#include "Compatibility/UE58/CubelessStatusWindowLibrary.h"
#include "Compatibility/UE58/CubelessStatusWindowApi.h"

FString UCubelessStatusWindowLibrary::ShowStatusWindow(const FString& Text, const FString& Command)
{ return Cubeless::UE58::StatusWindow::Show(Text, Command); }
void UCubelessStatusWindowLibrary::CloseStatusWindow()
{ Cubeless::UE58::StatusWindow::Close(); }
