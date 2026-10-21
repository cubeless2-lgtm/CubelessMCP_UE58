#include "CubelessMaterialPostprocessAsyncLibrary.h"
#include "Compatibility/UE58/CubelessMaterialPostprocessAsyncApi.h"
UCubelessMaterialPostprocessJob* UCubelessMaterialPostprocessAsyncLibrary::StartCompileAndSaveMaterialJob(const FString& ParamsJson)
{ return Cubeless::UE58::MaterialPostprocessAsync::StartCompileAndSave(ParamsJson); }
UCubelessMaterialPostprocessJob* UCubelessMaterialPostprocessAsyncLibrary::StartRefreshMaterialCachedExpressionDataJob(const FString& ParamsJson)
{ return Cubeless::UE58::MaterialPostprocessAsync::StartCacheRefresh(ParamsJson); }

UCubelessMaterialPostprocessJob* UCubelessMaterialPostprocessAsyncLibrary::StartReplaceMaterialCollectionReferencesJob(const FString& ParamsJson)
{ return Cubeless::UE58::MaterialPostprocessAsync::StartReplaceCollections(ParamsJson); }
UCubelessMaterialPostprocessJob* UCubelessMaterialPostprocessAsyncLibrary::StartReplaceMaterialTextureReferencesJob(const FString& ParamsJson)
{ return Cubeless::UE58::MaterialPostprocessAsync::StartReplaceTextures(ParamsJson); }
UCubelessMaterialPostprocessJob* UCubelessMaterialPostprocessAsyncLibrary::StartReplaceMaterialCollectionParametersJob(const FString& ParamsJson)
{ return Cubeless::UE58::MaterialPostprocessAsync::StartReplaceCollectionParameters(ParamsJson); }
