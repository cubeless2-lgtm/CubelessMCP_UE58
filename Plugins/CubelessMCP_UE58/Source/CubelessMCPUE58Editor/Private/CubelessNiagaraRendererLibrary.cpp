#include "CubelessNiagaraRendererLibrary.h"
#include "Compatibility/UE58/CubelessNiagaraRendererApi.h"

FString UCubelessNiagaraRendererLibrary::InspectNiagaraRenderers(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraRenderers::Inspect(ParamsJson);
}

FString UCubelessNiagaraRendererLibrary::SetNiagaraRendererMaterial(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraRenderers::SetMaterial(ParamsJson);
}

FString UCubelessNiagaraRendererLibrary::SetNiagaraSpriteRendererSettings(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraRenderers::SetSpriteSettings(ParamsJson);
}

FString UCubelessNiagaraRendererLibrary::AddNiagaraSpriteRenderer(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraRenderers::AddSprite(ParamsJson);
}

FString UCubelessNiagaraRendererLibrary::AddNiagaraMeshRenderer(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraRenderers::AddMesh(ParamsJson);
}

FString UCubelessNiagaraRendererLibrary::SetNiagaraRendererEnabled(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraRenderers::SetEnabled(ParamsJson);
}

FString UCubelessNiagaraRendererLibrary::CleanupNiagaraRenderers(const FString& ParamsJson)
{
    return Cubeless::UE58::NiagaraRenderers::Cleanup(ParamsJson);
}

