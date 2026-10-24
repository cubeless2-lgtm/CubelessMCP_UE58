#include "Modules/ModuleManager.h"
#include "Registration/CubelessLocalFacadeDefaults.h"
#include "Compatibility/UE58/CubelessStatusWindowApi.h"
#include "CubelessPCGNativeCanaryLibrary.h"
#include "Compatibility/UE58/CubelessPCGComponentGenerationApi.h"
#include "Compatibility/UE58/CubelessEditorLevelSessionApi.h"
#include "Compatibility/UE58/CubelessChaosSolverSettingsApi.h"
#include "Compatibility/UE58/CubelessChaosCacheSessionApi.h"
#include "Compatibility/UE58/CubelessChaosTraceStrainApi.h"
#include "Compatibility/UE58/CubelessChaosLearningApi.h"
#include "Compatibility/UE58/CubelessChaosDestructionPerformanceApi.h"
#include "Compatibility/UE58/CubelessMaterialCollectionSyncApi.h"
#include "Compatibility/UE58/CubelessVolumeTextureApi.h"
#include "Compatibility/UE58/CubelessNiagaraUserParameterAsyncApi.h"
#include "Compatibility/UE58/CubelessNiagaraGraphCleanupAsyncApi.h"
#include "Compatibility/UE58/CubelessNiagaraSpritePresetAsyncApi.h"
#include "Compatibility/UE58/CubelessNiagaraPreviewWorkflowApi.h"
#include "Registration/CubelessJsonConverters.h"
#include "Registration/CubelessMaterialReferenceDefaults.h"
#include "Registration/CubelessNativeToolset.h"
#include "Registration/CubelessNiagaraScratchUserRecipeToolset.h"
#include "Compatibility/UE58/CubelessNiagaraScratchUserRecipeAsyncApi.h"
#include "Compatibility/UE58/CubelessNiagaraFactoryBootstrapApi.h"
#include "Compatibility/UE58/CubelessNiagaraCustomHlslConversionApi.h"
#include "Compatibility/UE58/CubelessNiagaraScratchStackAuthoringApi.h"
#include "Compatibility/UE58/CubelessNiagaraEmitterEventAuthoringAsyncApi.h"
#include "Compatibility/UE58/CubelessNiagaraModuleInputsBatchAsyncApi.h"
#include "Compatibility/UE58/CubelessMaterialPostprocessAsyncApi.h"
#include "Compatibility/UE58/CubelessNiagaraResourceBindingApi.h"
#include "Compatibility/UE58/CubelessNiagaraInputParameterApi.h"
#include "Registration/CubelessBlueprintResolutionConnectionToolset.h"
#include "Registration/CubelessBlueprintPropertyGraphToolset.h"
#include "Registration/CubelessTextureMaterialToolset.h"
#include "Registration/CubelessPythonExecutionToolset.h"
#include "Registration/CubelessFunctionNodeRawToolset.h"
#include "Registration/CubelessBlueprintSelectionRawToolset.h"
#include "Registration/CubelessBlueprintExpressionRawToolset.h"
#include "Registration/CubelessBlueprintAnnotationRawToolset.h"
#include "Registration/CubelessBlueprintDispatcherRawToolset.h"
#include "Registration/CubelessBlueprintEventRawToolset.h"
#include "Registration/CubelessBlueprintBindingRawToolset.h"
#include "Registration/CubelessBlueprintGraphTransferRawToolset.h"
#include "Registration/CubelessBlueprintNetworkingRawToolset.h"
#include "Registration/CubelessBlueprintQueryLayoutRawToolset.h"
#include "Registration/CubelessBlueprintTopologyRawToolset.h"
#include "Registration/CubelessBlueprintComponentReferenceRawToolset.h"
#include "Registration/CubelessAnimGraphInspectionRawToolset.h"
#include "Registration/CubelessAnimGraphAuthoringRawToolset.h"
#include "Registration/CubelessUMGAuthoringRawToolset.h"
#include "Registration/CubelessUMGInteractionRawToolset.h"
#include "Registration/CubelessUMGUtilityRawToolset.h"
#include "Registration/CubelessUMGTextBindingRawToolset.h"
#include "Registration/CubelessBlueprintWidgetFallbackRawToolset.h"
#include "Registration/CubelessLocalVariableRawToolset.h"
#include "Registration/CubelessPCGRawToolset.h"
#include "Registration/CubelessSkeletalPhysicsRawToolset.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

DEFINE_LOG_CATEGORY_STATIC(LogCubelessMCPUE58, Log, All);

class FCubelessMCPUE58EditorModule final : public IModuleInterface
{
public:
    // Queued task callbacks contain this DLL code: use a clean editor restart.
    virtual bool SupportsDynamicReloading() override { return false; }
    virtual void StartupModule() override
    {
        if (!UToolsetRegistry::IsAvailable())
        {
            UE_LOG(LogCubelessMCPUE58, Error, TEXT("Official Toolset Registry is unavailable; Cubeless native toolset was not registered."));
            return;
        }
        // Raw handler owns no UClass/converter/default metadata. Register before
        // the existing native-class duplicate early return; Python is invoked only on calls.
        const bool PythonExecutionRegistered = PythonExecutionRaw.Register();
        UE_LOG(LogCubelessMCPUE58, Display, TEXT("Cubeless original general Python public5/native7 raw2 registration: %s"), PythonExecutionRegistered ? TEXT("confirmed") : TEXT("failed"));
        const bool BlueprintWidgetFallbackRegistered = BlueprintWidgetFallbackRaw.Register();
        UE_LOG(LogCubelessMCPUE58, Display, TEXT("Cubeless original Blueprint-widget fallback analysis3 raw1 registration: %s"), BlueprintWidgetFallbackRegistered ? TEXT("confirmed") : TEXT("failed"));
        const bool UMGTextBindingRegistered = UMGTextBindingRaw.Register();
        UE_LOG(LogCubelessMCPUE58, Display, TEXT("Cubeless original UMG TextBlock binding4 raw1 registration: %s"), UMGTextBindingRegistered ? TEXT("confirmed") : TEXT("failed"));
        const bool UMGUtilityRegistered = UMGUtilityRaw.Register();
        UE_LOG(LogCubelessMCPUE58, Display, TEXT("Cubeless original UMG Checkbox object/Viewport2 raw2 registration: %s"), UMGUtilityRegistered ? TEXT("confirmed") : TEXT("failed"));
        const bool UMGInteractionRegistered = UMGInteractionRaw.Register();
        UE_LOG(LogCubelessMCPUE58, Display, TEXT("Cubeless original UMG Slider2/Bind4 raw2 registration: %s"), UMGInteractionRegistered ? TEXT("confirmed") : TEXT("failed"));
        const bool UMGAuthoringRegistered = UMGAuthoringRaw.Register();
        UE_LOG(LogCubelessMCPUE58, Display, TEXT("Cubeless original UMG create3/text7/button8 raw3 registration: %s"), UMGAuthoringRegistered ? TEXT("confirmed") : TEXT("failed"));
        const bool AnimGraphAuthoringRegistered = AnimGraphAuthoringRaw.Register();
        UE_LOG(LogCubelessMCPUE58, Display, TEXT("Cubeless original AnimGraph RigidBody10/InputPose7 raw2 registration: %s"), AnimGraphAuthoringRegistered ? TEXT("confirmed") : TEXT("failed"));
        const bool AnimGraphInspectionRegistered = AnimGraphInspectionRaw.Register();
        UE_LOG(LogCubelessMCPUE58, Display, TEXT("Cubeless original AnimGraph settings9/protected topology15 raw2 registration: %s"), AnimGraphInspectionRegistered ? TEXT("confirmed") : TEXT("failed"));
        const bool ComponentReferenceRegistered = BlueprintComponentReferenceRaw.Register();
        UE_LOG(LogCubelessMCPUE58, Display, TEXT("Cubeless original blueprint self component reference7 raw1 registration: %s"), ComponentReferenceRegistered ? TEXT("confirmed") : TEXT("failed"));
        const bool TopologyRegistered = BlueprintTopologyRaw.Register();
        UE_LOG(LogCubelessMCPUE58, Display, TEXT("Cubeless original blueprint topology Inspect15 raw1 registration: %s"), TopologyRegistered ? TEXT("confirmed") : TEXT("failed"));
        const bool QueryLayoutRegistered = BlueprintQueryLayoutRaw.Register();
        UE_LOG(LogCubelessMCPUE58, Display, TEXT("Cubeless original blueprint query/layout Find6/Positions8 raw2 registration: %s"), QueryLayoutRegistered ? TEXT("confirmed") : TEXT("failed"));
        const bool NetworkingRegistered = BlueprintNetworkingRaw.Register();
        UE_LOG(LogCubelessMCPUE58, Display, TEXT("Cubeless original blueprint networking inspect/set raw2 registration: %s"), NetworkingRegistered ? TEXT("confirmed") : TEXT("failed"));
        const bool GraphTransferRegistered = BlueprintGraphTransferRaw.Register();
        UE_LOG(LogCubelessMCPUE58, Display, TEXT("Cubeless original blueprint graph transfer/repair raw2 registration: %s"), GraphTransferRegistered ? TEXT("confirmed") : TEXT("failed"));
        const bool BindingRegistered = BlueprintBindingRaw.Register();
        UE_LOG(LogCubelessMCPUE58, Display, TEXT("Cubeless original blueprint binding raw3 registration: %s"), BindingRegistered ? TEXT("confirmed") : TEXT("failed"));
        const bool EventRegistered = BlueprintEventRaw.Register();
        UE_LOG(LogCubelessMCPUE58, Display, TEXT("Cubeless original custom-event raw1 registration: %s"), EventRegistered ? TEXT("confirmed") : TEXT("failed"));
        const bool DispatcherRegistered = BlueprintDispatcherRaw.Register();
        UE_LOG(LogCubelessMCPUE58, Display, TEXT("Cubeless fixed blueprint dispatcher raw6 registration: %s"), DispatcherRegistered ? TEXT("confirmed") : TEXT("failed"));
        const bool AnnotationRegistered = BlueprintAnnotationRaw.Register();
        UE_LOG(LogCubelessMCPUE58, Display, TEXT("Cubeless fixed blueprint annotation raw3 registration: %s"), AnnotationRegistered ? TEXT("confirmed") : TEXT("failed"));
        const bool ExpressionRegistered = BlueprintExpressionRaw.Register();
        UE_LOG(LogCubelessMCPUE58, Display, TEXT("Cubeless fixed Math/Boolean raw2 registration: %s"), ExpressionRegistered ? TEXT("confirmed") : TEXT("failed"));
        const bool SelectionRegistered = BlueprintSelectionRaw.Register();
        UE_LOG(LogCubelessMCPUE58, Display, TEXT("Cubeless fixed typed selection raw2 registration: %s"), SelectionRegistered ? TEXT("confirmed") : TEXT("failed"));
        const bool PhysicsRegistered = SkeletalPhysicsRaw.Register();
        UE_LOG(LogCubelessMCPUE58, Display, TEXT("Cubeless fixed skeletal PhysicsAsset raw registration: %s"), PhysicsRegistered ? TEXT("confirmed") : TEXT("failed"));
        const bool PCGRegistered = PCGRaw.Register();
        UE_LOG(LogCubelessMCPUE58, Display, TEXT("Cubeless fixed PCG raw registration: %s"), PCGRegistered ? TEXT("confirmed") : TEXT("failed"));
        const bool LocalRegistered = LocalVariableRaw.Register();
        UE_LOG(LogCubelessMCPUE58, Display, TEXT("Cubeless named local-variable raw registration: %s"), LocalRegistered ? TEXT("confirmed") : TEXT("failed"));
        const bool RawRegistered = FunctionNodeRaw.Register();
        UE_LOG(LogCubelessMCPUE58, Display, TEXT("Cubeless named function-node raw registration: %s"), RawRegistered ? TEXT("confirmed") : TEXT("failed"));
        // Python init_unreal registers toolsets after this PostEngineInit startup.
        // Their call objects cache schemas: converters must be installed first.
        if (!JsonConverters.Register())
        {
            UE_LOG(LogCubelessMCPUE58, Error, TEXT("Cubeless Material array converter registration failed; a clean editor restart is required after repair."));
            return;
        }
        UE_LOG(LogCubelessMCPUE58, Display, TEXT("Cubeless Material array converter registered before Python toolset initialization (two exact parameters)."));
        // Controller owns only Editor ticker/world handles; no job is started by registration.
        Cubeless::UE58::MaterialCollectionSync::Activate();
        const bool VolumeTextureJobsActive = Cubeless::UE58::VolumeTexture::Activate();
        UE_LOG(LogCubelessMCPUE58, Display, TEXT("Cubeless native VolumeTexture job admission activation: %s (no job submitted)"), VolumeTextureJobsActive ? TEXT("confirmed") : TEXT("failed"));
        const bool ReferenceDefaultsRegistered = MaterialReferenceDefaults.Register();
        UE_LOG(LogCubelessMCPUE58, Display, TEXT("Cubeless exact Material reference empty-source converter: %s"), ReferenceDefaultsRegistered ? TEXT("confirmed") : TEXT("failed"));
        if (UToolsetRegistry::IsToolsetClassRegistered(UCubelessNativeToolset::StaticClass()))
        {
            UE_LOG(LogCubelessMCPUE58, Warning, TEXT("Cubeless native toolset is already registered; existing registration retained."));
            return;
        }
        UToolsetRegistry::RegisterToolsetClass(UCubelessNativeToolset::StaticClass());
        bOwnsRegistration = UToolsetRegistry::IsToolsetClassRegistered(UCubelessNativeToolset::StaticClass());
        UE_LOG(LogCubelessMCPUE58, Display, TEXT("Cubeless native toolset registration: %s"), bOwnsRegistration ? TEXT("confirmed") : TEXT("failed"));
    }

    virtual void ShutdownModule() override
    {
        Cubeless::UE58::PCGComponentGeneration::ShutdownRefreshTracking();
        Cubeless::Registration::ShutdownLocalFacadeDefaults();
        Cubeless::UE58::StatusWindow::Shutdown();
        PythonExecutionRaw.Unregister();
        Cubeless::Registration::ShutdownTextureMaterialToolset();
        Cubeless::Registration::ShutdownBlueprintPropertyGraphToolset();
        Cubeless::Registration::ShutdownBlueprintResolutionConnectionToolset();
        Cubeless::UE58::NiagaraScratchUserRecipeAsync::Shutdown(TEXT("Editor module shutdown"));
        Cubeless::UE58::NiagaraFactoryBootstrapAsync::Shutdown(TEXT("Editor module shutdown"));
        Cubeless::UE58::NiagaraCustomHlslConversionAsync::Shutdown(TEXT("Editor module shutdown"));
        Cubeless::UE58::NiagaraScratchStackAuthoringAsync::Shutdown(TEXT("Editor module shutdown"));
        Cubeless::UE58::NiagaraEmitterEventAuthoringAsync::Shutdown(TEXT("Editor module shutdown"));
        Cubeless::UE58::NiagaraModuleInputsBatchAsync::Shutdown(TEXT("Editor module shutdown"));
        Cubeless::UE58::MaterialPostprocessAsync::Shutdown(TEXT("Editor module shutdown"));
        Cubeless::UE58::NiagaraResourceBindingAsync::Shutdown(TEXT("Editor module shutdown"));
        Cubeless::UE58::NiagaraInputParameterAsync::Shutdown(TEXT("Editor module shutdown"));
        Cubeless::Registration::ShutdownNiagaraScratchUserRecipeToolset();
        Cubeless::UE58::PCGNativeCanary::Shutdown();
        Cubeless::UE58::NiagaraPreviewWorkflows::Shutdown(TEXT("Editor module shutdown"));
        Cubeless::UE58::NiagaraSpritePresetAsync::Shutdown(TEXT("Editor module shutdown"));
        Cubeless::UE58::NiagaraUserParameterAsync::Shutdown(TEXT("Editor module shutdown"));
        Cubeless::UE58::NiagaraGraphCleanupAsync::Shutdown(TEXT("Editor module shutdown"));
        // Native admission is Module-owned; canceled engine texture compilation may continue.
        // This call does not certify global compilation quiescence or permit job replay.
        Cubeless::UE58::VolumeTexture::Deactivate(TEXT("Editor module shutdown"));
        // Remove owned callbacks and strong refs before class/converter teardown and DLL unload.
        Cubeless::UE58::MaterialCollectionSync::Shutdown();
        Cubeless::UE58::EditorLevelSession::Shutdown();
        Cubeless::UE58::ChaosDestructionPerformance::Shutdown();
        Cubeless::UE58::ChaosLearning::Shutdown();
        Cubeless::UE58::ChaosTraceStrain::Shutdown();
        Cubeless::UE58::ChaosCacheSessions::Shutdown();
        Cubeless::UE58::ChaosSolverSettings::Shutdown();
        BlueprintWidgetFallbackRaw.Unregister();
        UMGTextBindingRaw.Unregister();
        UMGUtilityRaw.Unregister();
        UMGInteractionRaw.Unregister();
        UMGAuthoringRaw.Unregister();
        AnimGraphAuthoringRaw.Unregister();
        AnimGraphInspectionRaw.Unregister();
        BlueprintComponentReferenceRaw.Unregister();
        BlueprintTopologyRaw.Unregister();
        BlueprintQueryLayoutRaw.Unregister();
        BlueprintNetworkingRaw.Unregister();
        BlueprintGraphTransferRaw.Unregister();
        BlueprintBindingRaw.Unregister();
        BlueprintEventRaw.Unregister();
        BlueprintDispatcherRaw.Unregister();
        BlueprintAnnotationRaw.Unregister();
        BlueprintExpressionRaw.Unregister();
        BlueprintSelectionRaw.Unregister();
        SkeletalPhysicsRaw.Unregister();
        PCGRaw.Unregister();
        LocalVariableRaw.Unregister();
        FunctionNodeRaw.Unregister();
        if (bOwnsRegistration && UObjectInitialized())
        {
            UToolsetRegistry::UnregisterToolsetClass(UCubelessNativeToolset::StaticClass());
            bOwnsRegistration = false;
        }
        MaterialReferenceDefaults.Unregister();
        JsonConverters.Unregister();
    }

private:
    bool bOwnsRegistration = false;
    Cubeless::Registration::FPythonExecutionRegistration PythonExecutionRaw;
    Cubeless::Registration::FJsonConverters JsonConverters;
    Cubeless::Registration::FMaterialReferenceDefaultsOwner MaterialReferenceDefaults;
    Cubeless::Registration::FFunctionNodeRawRegistration FunctionNodeRaw;
    Cubeless::Registration::FBlueprintSelectionRawRegistration BlueprintSelectionRaw;
    Cubeless::Registration::FBlueprintExpressionRawRegistration BlueprintExpressionRaw;
    Cubeless::Registration::FBlueprintAnnotationRawRegistration BlueprintAnnotationRaw;
    Cubeless::Registration::FBlueprintDispatcherRawRegistration BlueprintDispatcherRaw;
    Cubeless::Registration::FBlueprintEventRawRegistration BlueprintEventRaw;
    Cubeless::Registration::FBlueprintBindingRawRegistration BlueprintBindingRaw;
    Cubeless::Registration::FBlueprintGraphTransferRawRegistration BlueprintGraphTransferRaw;
    Cubeless::Registration::FBlueprintNetworkingRawRegistration BlueprintNetworkingRaw;
    Cubeless::Registration::FBlueprintQueryLayoutRawRegistration BlueprintQueryLayoutRaw;
    Cubeless::Registration::FBlueprintTopologyRawRegistration BlueprintTopologyRaw;
    Cubeless::Registration::FBlueprintComponentReferenceRawRegistration BlueprintComponentReferenceRaw;
    Cubeless::Registration::FAnimGraphInspectionRawRegistration AnimGraphInspectionRaw;
    Cubeless::Registration::FAnimGraphAuthoringRawRegistration AnimGraphAuthoringRaw;
    Cubeless::Registration::FUMGAuthoringRawRegistration UMGAuthoringRaw;
    Cubeless::Registration::FUMGInteractionRawRegistration UMGInteractionRaw;
    Cubeless::Registration::FUMGUtilityRawRegistration UMGUtilityRaw;
    Cubeless::Registration::FUMGTextBindingRawRegistration UMGTextBindingRaw;
    Cubeless::Registration::FBlueprintWidgetFallbackRawRegistration BlueprintWidgetFallbackRaw;
    Cubeless::Registration::FLocalVariableRawRegistration LocalVariableRaw;
    Cubeless::Registration::FPCGRawRegistration PCGRaw;
    Cubeless::Registration::FSkeletalPhysicsRawRegistration SkeletalPhysicsRaw;
};

IMPLEMENT_MODULE(FCubelessMCPUE58EditorModule, CubelessMCPUE58Editor)
