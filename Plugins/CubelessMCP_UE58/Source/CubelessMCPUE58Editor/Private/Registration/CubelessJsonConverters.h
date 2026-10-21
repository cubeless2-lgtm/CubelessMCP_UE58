#pragma once

#include "CoreMinimal.h"
#include "Registration/CubelessStateTreeStrictDryRunToolset.h"
#include "Registration/CubelessContentAuditToolset.h"
#include "Registration/CubelessEditorTraceToolset.h"
#include "Registration/CubelessEnumToolset.h"
#include "Registration/CubelessRigVMTopologyToolset.h"
#include "Registration/CubelessRigVMLinkValidationToolset.h"
#include "Registration/CubelessRigVMPinDefaultValidationToolset.h"
#include "Registration/CubelessRigVMLinkWriterToolset.h"
#include "Registration/CubelessRigVMPinDefaultWriterToolset.h"
#include "Registration/CubelessAnimInstanceRuntimeToolset.h"
#include "Registration/CubelessControlRigSolverIntentsToolset.h"
#include "Registration/CubelessControlRigTransientProbeToolset.h"
#include "Registration/CubelessAnimInstanceRuntimePropertyProbeToolset.h"
#include "Registration/CubelessAnimStateRuntimeResponseToolset.h"
#include "Registration/CubelessPostProcessAnimDemoVariantToolset.h"
#include "Registration/CubelessSkeletalBoneSamplingToolset.h"
#include "Registration/CubelessBlendSpaceVariantToolset.h"
#include "Registration/CubelessBlendSpacePoseGridToolset.h"
#include "Registration/CubelessLinkedAnimGraphVariantToolset.h"
#include "Registration/CubelessAnimStateMachineVariantToolset.h"
#include "Registration/CubelessAnimBPLocomotionDriverToolset.h"
#include "Registration/CubelessAnimNodePrePostRuntimePoseToolset.h"
#include "Registration/CubelessBlueprintFunctionExtractionToolset.h"
#include "Registration/CubelessSplineComponentPointsToolset.h"
#include "Registration/CubelessFoliageInstanceToolset.h"
#include "Registration/CubelessLandscapeToolset.h"
#include "Registration/CubelessViewportCaptureToolset.h"
#include "Registration/CubelessEditorLevelSessionToolset.h"
#include "Registration/CubelessActorPropertyFocusToolset.h"
#include "Registration/CubelessChaosSolverSettingsToolset.h"
#include "Registration/CubelessChaosClothComponentToolset.h"
#include "Registration/CubelessChaosClothAuthoringToolset.h"
#include "Registration/CubelessLegacyChaosClothingToolset.h"
#include "Registration/CubelessChaosCinematicSequenceToolset.h"
#include "Registration/CubelessChaosCacheSessionToolset.h"
#include "Registration/CubelessChaosDestructionPerformanceToolset.h"
#include "Registration/CubelessChaosLearningToolset.h"
#include "Registration/CubelessChaosTraceStrainToolset.h"
#include "Registration/CubelessChaosGeometryAuthoringToolset.h"
#include "Registration/CubelessGASAuthoringToolset.h"
#include "Registration/CubelessContentReconstructionToolset.h"
#include "Registration/CubelessNiagaraRendererToolset.h"
#include "Registration/CubelessNiagaraEmitterConfigurationToolset.h"
#include "Registration/CubelessNiagaraParameterCollectionToolset.h"
#include "Registration/CubelessNiagaraAuthoringInspectionToolset.h"
#include "Registration/CubelessPCGInspectionToolset.h"
#include "Registration/CubelessNiagaraCleanupToolset.h"
#include "Registration/CubelessNiagaraInputParameterToolset.h"
#include "Registration/CubelessNiagaraModuleInputPathToolset.h"
#include "Registration/CubelessNiagaraDataChannelAuthoringToolset.h"
#include "Registration/CubelessNiagaraRuntimeInspectionToolset.h"
#include "Registration/CubelessNiagaraFactoryBootstrapToolset.h"
#include "Registration/CubelessNiagaraResourceBindingToolset.h"
#include "Registration/CubelessGeometryCollectionInspectionToolset.h"
#include "Registration/CubelessChaosClothInspectionToolset.h"

class UToolsetRegistrySubsystem;
class FJsonObject;
namespace UE::ToolsetRegistry { class FToolsetJsonConverter; }

namespace Cubeless::Registration
{
    bool VerifySkeletalSocketToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);
    bool PrepareAnimStateMachineInspectionToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);
    bool PreparePhysicsAssetInspectionToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);
    bool VerifySkeletalReadToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);
    bool VerifyPawnPropertiesToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);
    bool VerifyVolumeTextureToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);
    bool VerifyBlueprintCustomEventMetadataToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);
    bool VerifyBlueprintCategorySortingToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);
    bool VerifyBlueprintVariableLifecycleToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);
    // Own only the converter registered by this module instance, on its original subsystem.
    class FJsonConverters final
    {
    public:
        bool Register();
        void Unregister();

    private:
        TWeakObjectPtr<UToolsetRegistrySubsystem> RegistrySubsystem;
        TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> MaterialArrays;
        TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> NiagaraStageElements;
        TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> AIDiscoveryDefaults;
        TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> BlueprintDefaults;
        TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> DataTableDefaults;
        TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> SequencerFloatDefaults;
        TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> SequencerMaintenanceDefaults;
        TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> SequencerCameraDefaults;
        TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> BlueprintMakeStructDefaults;
        TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> BlueprintCategorySortingDefaults;
        TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> BlueprintVariableMetadataNullable;
        TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> SkeletalSocketDefaults;
        TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> SkeletalReadDefaults;
        TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> PawnPropertiesDefaults;
        TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> BlueprintCustomEventMetadataDefaults;
    };
}
