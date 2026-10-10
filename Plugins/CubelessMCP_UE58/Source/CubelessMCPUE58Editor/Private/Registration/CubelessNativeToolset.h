#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "CubelessNativeToolset.generated.h"

/** Cubeless native editor diagnostics, asset verification and compiler operations. Never reference this editor class from game assets. */
UCLASS()
class UCubelessNativeToolset : public UToolsetDefinition
{
    GENERATED_BODY()

public:
    virtual FString GetToolsetVersion() const override { return TEXT("0.1.0"); }

    /** Return JSON with engine/project identity, dirty packages, PIE state and MCP plugin state. Does not save, compile, close, or modify assets. */
    UFUNCTION(BlueprintCallable, Category = "Cubeless|Editor", meta = (AICallable))
    static FString GetEditorDiagnostics();

    /**
     * Audit saved package dependencies for Cubeless, legacy MCP, official MCP, Toolset Registry, Python and EditorScriptingUtilities references.
     * PackageNames are long package names such as /Game/Tests/BP_Test, without object suffixes.
     * Includes hard and soft package references. Optional recursion has a strict MaxPackages budget (1..4096).
     * Returns JSON containing edges, violations, incomplete reasons and dirty packages. Incomplete scans never pass.
     * Saved Asset Registry data only: not proof of unsaved object references, all possible editor-only classes, or plugin-free packaging.
     */
    UFUNCTION(BlueprintCallable, Category = "Cubeless|Editor", meta = (AICallable))
    static FString AuditPackageDependencies(const TArray<FString>& PackageNames, bool bRecursive = true, int32 MaxPackages = 256);

    /**
     * Read imported skeletal mesh sections for one LOD, including enabled state and source section identity.
     * SkeletalMeshPath is an object path such as /Game/Meshes/SK_Test.SK_Test; LODIndex is zero-based.
     * Returns JSON matching the legacy GetSections data fields plus operation status. Loads the mesh but never edits or saves it.
     * If mesh compilation is pending, returns a retryable read error instead of forcing synchronous compilation.
     */
    UFUNCTION(BlueprintCallable, Category = "Cubeless|Editor", meta = (AICallable))
    static FString GetSkeletalMeshSections(const FString& SkeletalMeshPath, int32 LODIndex = 0);

    /**
     * Internal Python helper: compile a Blueprint with actual compiler errors, warnings and tokenized messages.
     * Official MCP callers use CubelessBlueprintTools compile wrappers, which own the durable mutation journal.
     * BlueprintPath is the asset object path. Optional node refresh occurs before compilation; optional save occurs only after compilation passes.
     * This mutates compiler/generated-class state, is not atomic, and never retries or rolls back. Rejects PIE/SIE.
     * A successful compile is not proof of runtime behavior or a reload; see saved and validation_scope separately.
     */
    UFUNCTION(BlueprintCallable, Category = "Cubeless|Editor", meta = (AIIgnore))
    static FString CompileBlueprintValidation(const FString& BlueprintPath, bool bSave = false, bool bRefreshNodes = false);

    /** Python helper: inspect native superclass chain, package flags and existing CDO editor-only state. No asset creation or saving. */
    UFUNCTION(BlueprintCallable, Category = "Cubeless|Editor", meta = (AIIgnore))
    static FString InspectBlueprintParent(const FString& ClassPath);
};
