// UV extends the existing literal guard; it does not execute a preprocessor/compiler.
// Active paths and completed DAG entries belong to this one synchronous admission.
struct FCustomShaderIncludeProof
{
    TSet<FString> Active;
    TMap<FString,FString> ReadyPhysicalPaths;
    TMap<FString,FString> ObservedPhysicalPaths;
    FString ToolBasePath;
    int64 TotalCharacters=0;
    int32 FileCount=0;
};
// Loader callbacks may affect directory mappings: a completed parent is authority
// only while every captured member of its literal closure retains that mapping.
bool CustomShaderMappingsCurrent(const FCustomShaderIncludeProof& Proof,FString& Error)
{
    const TSharedPtr<IPlugin> Plugin=IPluginManager::Get().FindPlugin(TEXT("CubelessMCP_UE58"));
    const FString ToolBase=Plugin.IsValid()?FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir()):FString();
    if(ToolBase.IsEmpty()||!ToolBase.Equals(Proof.ToolBasePath,ESearchCase::CaseSensitive))
    {Error=TEXT("Custom shader include tool plugin directory identity changed; no further dependency admission.");return false;}
    for(const auto& Pair:Proof.ObservedPhysicalPaths)
    {
        TArray<FShaderCompilerError> Errors;const FString Path=GetShaderSourceFilePath(Pair.Key,&Errors);
        const FString Actual=Path.IsEmpty()?FString():FPaths::ConvertRelativePathToFull(Path);
        if(Actual.IsEmpty()||!Actual.Equals(Pair.Value,ESearchCase::CaseSensitive)||FPaths::IsUnderDirectory(Actual,ToolBase))
        {Error=FString::Printf(TEXT("Custom shader include directory_mapping_changed: %s; expected=%s actual=%s"),*Pair.Key,*Pair.Value,*Actual);return false;}
    }
    return true;
}
bool ContainsCustomIncludeDirective(FString Text, bool bAllowFileIndependent = true,
    FString* DependencyError = nullptr, bool bAllowLiteralIncludes = true,
    FCustomShaderIncludeProof* SharedProof = nullptr, const FString& ContainingFile = FString());

bool ValidateCustomShaderFilePath(const FString& VirtualPath, FString& Error,
    FCustomShaderIncludeProof* SharedProof = nullptr)
{
    FCustomShaderIncludeProof LocalProof;
    auto& Proof=SharedProof?*SharedProof:LocalProof;
    auto Fail = [&Error, &VirtualPath](const TCHAR* Stage)
    {
        Error = FString::Printf(TEXT("Custom shader include %s: %s"), Stage, *VirtualPath);
        return false;
    };
    // Literal source paths are not macro expressions or generated compiler environment keys.
    for (TCHAR Ch : VirtualPath)
    {
        if (Ch < 32 || Ch > 126 || Ch == TEXT('"') || Ch == TEXT('\\') || Ch == TEXT('<') || Ch == TEXT('>'))
        { return Fail(TEXT("nonliteral_path_pending")); }
    }
    if (VirtualPath.StartsWith(TEXT("/Engine/Generated/"), ESearchCase::IgnoreCase)
        || VirtualPath.StartsWith(TEXT("/ShaderAutogen/"), ESearchCase::IgnoreCase)
        || VirtualPath.StartsWith(TEXT("/Platform/"), ESearchCase::IgnoreCase))
    { return Fail(TEXT("generated_or_platform_path_pending")); }
    static const TCHAR* ToolShaderRoots[] = {
        TEXT("/Plugin/CubelessMCP_UE58/"), TEXT("/Plugin/CubelessMCP/"), TEXT("/Plugin/UnrealMCP/"),
        TEXT("/Plugin/ModelContextProtocol/"), TEXT("/Plugin/ToolsetRegistry/"), TEXT("/Plugin/CubelessControlRigTools/")
    };
    for (const TCHAR* Prefix : ToolShaderRoots)
    { if (VirtualPath.StartsWith(Prefix, ESearchCase::IgnoreCase)) { return Fail(TEXT("tool_runtime_dependency_forbidden")); } }
    TArray<FShaderCompilerError> Errors;
    if (!CheckVirtualShaderFilePath(VirtualPath, &Errors)) { return Fail(TEXT("virtual_path_invalid")); }
    const FString PhysicalPath = GetShaderSourceFilePath(VirtualPath, &Errors);
    if (PhysicalPath.IsEmpty()) { return Fail(TEXT("directory_mapping_unavailable")); }
    // Also reject an alias mapped into our plugin rather than trusting its virtual spelling.
    const TSharedPtr<IPlugin> OwnPlugin = IPluginManager::Get().FindPlugin(TEXT("CubelessMCP_UE58"));
    if (!OwnPlugin.IsValid()) { return Fail(TEXT("tool_plugin_identity_unavailable")); }
    if (FPaths::IsUnderDirectory(FPaths::ConvertRelativePathToFull(PhysicalPath),
        FPaths::ConvertRelativePathToFull(OwnPlugin->GetBaseDir())))
    { return Fail(TEXT("tool_runtime_dependency_forbidden")); }
    const FString FullPhysical=FPaths::ConvertRelativePathToFull(PhysicalPath);
    const FString ToolBase=FPaths::ConvertRelativePathToFull(OwnPlugin->GetBaseDir());
    if(Proof.ToolBasePath.IsEmpty())Proof.ToolBasePath=ToolBase;
    if(const FString* Before=Proof.ObservedPhysicalPaths.Find(VirtualPath))
    {if(!Before->Equals(FullPhysical,ESearchCase::CaseSensitive))return Fail(TEXT("directory_mapping_changed"));}
    else Proof.ObservedPhysicalPaths.Add(VirtualPath,FullPhysical);
    // Check active ancestry before completed reuse; a guarded/macro cycle is valid wider pending.
    if(Proof.Active.Contains(VirtualPath)) { return Fail(TEXT("cyclic_literal_dependency_pending")); }
    if(!CustomShaderMappingsCurrent(Proof,Error))return false;
    if(const FString* Ready=Proof.ReadyPhysicalPaths.Find(VirtualPath))
    { return Ready->Equals(FullPhysical,ESearchCase::CaseSensitive)||Fail(TEXT("directory_mapping_changed")); }
    if(Proof.Active.Num()>=32||Proof.FileCount>=128) { return Fail(TEXT("literal_dependency_bound_pending")); }
    ++Proof.FileCount;
    FString Contents;
    // This bool-returning overload does not call LoadShaderSourceFileChecked/Fatal.
    if (!LoadShaderSourceFile(*VirtualPath, GMaxRHIShaderPlatform, &Contents, &Errors))
    { return Fail(TEXT("source_load_failed")); }
    if(!CustomShaderMappingsCurrent(Proof,Error))return false;
    if(Contents.Len()>1048576||Proof.TotalCharacters+Contents.Len()>4194304)
    { return Fail(TEXT("literal_dependency_source_bound_pending")); }
    Proof.TotalCharacters+=Contents.Len();Proof.Active.Add(VirtualPath);
    FString NestedError;
    const bool Unsupported=ContainsCustomIncludeDirective(MoveTemp(Contents),true,&NestedError,true,&Proof,VirtualPath);
    Proof.Active.Remove(VirtualPath);
    if(Unsupported)
    {
        if(!NestedError.IsEmpty())Error=MoveTemp(NestedError);
        else Fail(TEXT("unsupported_transitive_source_pending"));
        return false;
    }
    if(!CustomShaderMappingsCurrent(Proof,Error))return false;
    Proof.ReadyPhysicalPaths.Add(VirtualPath,FullPhysical);
    return true;
}

// Shared by the specialized Custom builder and generic Add/Set property paths.
// A typed converted value is inspected before any persistent property assignment.
bool ContainsCustomIncludeDirective(FString Text, bool bAllowFileIndependent,
    FString* DependencyError, bool bAllowLiteralIncludes, FCustomShaderIncludeProof* SharedProof,
    const FString& ContainingFile)
{
    FCustomShaderIncludeProof LocalProof;
    auto* Proof=SharedProof?SharedProof:&LocalProof;
    TSet<FString> VerifiedPaths;
    auto CheckText = [bAllowFileIndependent, bAllowLiteralIncludes, DependencyError, &VerifiedPaths, Proof, &ContainingFile](FString ViewText, bool bSplice) -> bool
    {
        // Inspect both physical and logical lines: SDK conversion/comment removal
        // and STB comment/splice handling do not have interchangeable order.
        if (bSplice)
        {
            ViewText.ReplaceInline(TEXT("\\\r\n"), TEXT(""));
            ViewText.ReplaceInline(TEXT("\\\n\r"), TEXT(""));
            ViewText.ReplaceInline(TEXT("\\\r"), TEXT(""));
            ViewText.ReplaceInline(TEXT("\\\n"), TEXT(""));
        }
        // Identifier fields retain their existing marker admission; they are not source bodies.
        if (!bAllowFileIndependent)
        { return ViewText.Contains(TEXT("#")) || ViewText.Contains(TEXT("%:")) || ViewText.Contains(TEXT("?" "?=")); }
    
        // A bounded lexical dependency guard, not an HLSL syntax validator or evaluator.
        // Remove block comments as copy_line_without_comments does for directive tokens;
        // Line comments become spaces; literals stay available only to the include operand
        // parser and are skipped by the general token scan.
        FString Visible; Visible.Reserve(ViewText.Len());
        bool bLineComment = false, bBlockComment = false; TCHAR Quote = 0;
        for (int32 Index = 0; Index < ViewText.Len(); ++Index)
        {
            const TCHAR Ch = ViewText[Index];
            const TCHAR Next = Index + 1 < ViewText.Len() ? ViewText[Index + 1] : 0;
            if (Quote)
            {
                if (Ch == TEXT('\r') || Ch == TEXT('\n')) { return true; }
                Visible.AppendChar(Ch);
                if (Ch == TEXT('\\'))
                { if (!Next || Next == TEXT('\r') || Next == TEXT('\n')) { return true; } Visible.AppendChar(Next); ++Index; }
                else if (Ch == Quote) { Quote = 0; }
                continue;
            }
            if (bLineComment)
            { if (Ch == TEXT('\r') || Ch == TEXT('\n')) { bLineComment = false; Visible.AppendChar(Ch); } else { Visible.AppendChar(TEXT(' ')); } continue; }
            if (bBlockComment)
            {
                if (Ch == TEXT('*') && Next == TEXT('/')) { bBlockComment = false; ++Index; }
                continue;
            }
            if (Ch == TEXT('/') && Next == TEXT('/')) { bLineComment = true; Visible += TEXT("  "); ++Index; continue; }
            if (Ch == TEXT('/') && Next == TEXT('*')) { bBlockComment = true; ++Index; continue; }
            if (Ch == TEXT('"') || Ch == TEXT('\'')) { Quote = Ch; Visible.AppendChar(Ch); continue; }
            Visible.AppendChar(Ch);
        }
        if (Quote || bBlockComment) { return true; }
        // Alternative directive spelling and pragma operators are outside this first slice.
        // No normalization is applied to the persisted user Code/define text.
        auto IdentifierChar = [](TCHAR Ch) { return (Ch >= TEXT('a') && Ch <= TEXT('z')) || (Ch >= TEXT('A') && Ch <= TEXT('Z')) || (Ch >= TEXT('0') && Ch <= TEXT('9')) || Ch == TEXT('_'); };
        bool bLeading = true;
        for (int32 Index = 0; Index < Visible.Len(); ++Index)
        {
            const TCHAR Ch = Visible[Index];
            if (Ch == TEXT('\r') || Ch == TEXT('\n')) { bLeading = true; continue; }
            if (Ch == TEXT(' ') || Ch == TEXT('\t') || Ch == TEXT('\v') || Ch == TEXT('\f')) { continue; }
            if (Ch == TEXT('"') || Ch == TEXT('\''))
            {
                const TCHAR LiteralQuote = Ch;
                while (++Index < Visible.Len())
                {
                    if (Visible[Index] == TEXT('\\')) { ++Index; }
                    else if (Visible[Index] == LiteralQuote) { break; }
                }
                bLeading = false; continue;
            }
            if ((Ch == TEXT('%') && Index + 1 < Visible.Len() && Visible[Index + 1] == TEXT(':'))
                || (Ch == TEXT('?') && Index + 2 < Visible.Len() && Visible[Index + 1] == TEXT('?') && Visible[Index + 2] == TEXT('=')))
            { return true; }
            if (IdentifierChar(Ch))
            {
                int32 End = Index + 1; while (End < Visible.Len() && IdentifierChar(Visible[End])) { ++End; }
                const FString Word = Visible.Mid(Index, End - Index);
                if (Word == TEXT("_Pragma") || Word == TEXT("__pragma") || Word == TEXT("__has_include") || Word == TEXT("__has_include_next")) { return true; }
                Index = End - 1;
            }
            else if (Ch == TEXT('#'))
            {
                // STB macro-argument rescanning can turn a bare/stringify/paste
                // marker into a leading directive; these computed forms remain pending.
                if (!bLeading) { return true; }
                int32 Start = Index + 1;
                while (Start < Visible.Len() && (Visible[Start] == TEXT(' ') || Visible[Start] == TEXT('\t') || Visible[Start] == TEXT('\v') || Visible[Start] == TEXT('\f'))) { ++Start; }
                int32 End = Start; while (End < Visible.Len() && IdentifierChar(Visible[End])) { ++End; }
                const FString Directive = Visible.Mid(Start, End - Start);
                // Scan every marker, including macro bodies; #pragma can be processed inside a define.
                if (Directive == TEXT("include"))
                {
                    if (!bAllowLiteralIncludes) { return true; }
                    int32 Operand = End;
                    while (Operand < Visible.Len() && (Visible[Operand] == TEXT(' ') || Visible[Operand] == TEXT('\t'))) { ++Operand; }
                    if (Operand >= Visible.Len() || Visible[Operand] != TEXT('"')) { return true; }
                    const int32 First = ++Operand;
                    while (Operand < Visible.Len() && Visible[Operand] != TEXT('"')
                        && Visible[Operand] != TEXT('\r') && Visible[Operand] != TEXT('\n')) { ++Operand; }
                    if (Operand >= Visible.Len() || Visible[Operand] != TEXT('"')) { return true; }
                    const FString IncludePath = Visible.Mid(First, Operand - First);
                    FString ResolvedPath=IncludePath;
                    if(!ResolvedPath.StartsWith(TEXT("/")))
                    {
                        // SDK ShaderCore resolves child literals relative to their containing virtual file.
                        // Root Code still requires an absolute mapped include; persisted text is untouched.
                        if(ContainingFile.IsEmpty())return true;
                        ResolvedPath=FPaths::GetPath(ContainingFile)/ResolvedPath;
                        if(!FPaths::CollapseRelativeDirectories(ResolvedPath))return true;
                    }
                    int32 Tail = Operand + 1;
                    while (Tail < Visible.Len() && (Visible[Tail] == TEXT(' ') || Visible[Tail] == TEXT('\t'))) { ++Tail; }
                    if (Tail < Visible.Len() && Visible[Tail] != TEXT('\r') && Visible[Tail] != TEXT('\n')) { return true; }
                    if (!VerifiedPaths.Contains(ResolvedPath))
                    {
                        FString FileError;
                        if (!ValidateCustomShaderFilePath(ResolvedPath, FileError, Proof))
                        { if (DependencyError) { *DependencyError = MoveTemp(FileError); } return true; }
                        VerifiedPaths.Add(ResolvedPath);
                    }
                    Index = Operand; bLeading = false; continue;
                }
                if(Directive==TEXT("pragma"))
                {
                    // The only admitted file-side pragma is the SDK's ordinary once directive.
                    // Code/define pragmas, macro-produced pragmas and other operands remain pending.
                    if(ContainingFile.IsEmpty()||!bLeading)return true;
                    int32 Word=End;while(Word<Visible.Len()&&(Visible[Word]==TEXT(' ')||Visible[Word]==TEXT('\t')))++Word;
                    int32 Finish=Word;while(Finish<Visible.Len()&&IdentifierChar(Visible[Finish]))++Finish;
                    if(Visible.Mid(Word,Finish-Word)!=TEXT("once"))return true;
                    int32 Tail=Finish;while(Tail<Visible.Len()&&(Visible[Tail]==TEXT(' ')||Visible[Tail]==TEXT('\t')))++Tail;
                    if(Tail<Visible.Len()&&Visible[Tail]!=TEXT('\r')&&Visible[Tail]!=TEXT('\n'))return true;
                    Index=Finish-1;bLeading=false;continue;
                }
                if (Directive == TEXT("include_next") || Directive == TEXT("import") || Directive == TEXT("embed")) { return true; }
                if (bLeading)
                {
                    const bool Empty = Start == Visible.Len() || Visible[Start] == TEXT('\r') || Visible[Start] == TEXT('\n');
                    const bool Allowed = Directive == TEXT("define") || Directive == TEXT("undef") || Directive == TEXT("if") || Directive == TEXT("ifdef") || Directive == TEXT("ifndef") || Directive == TEXT("elif") || Directive == TEXT("else") || Directive == TEXT("endif") || Directive == TEXT("line") || Directive == TEXT("error") || Directive == TEXT("warning");
                    if (!Empty && !Allowed) { return true; }
                }
            }
            bLeading = false;
        }
        return false;
    };
    if (CheckText(Text, false) || CheckText(Text, true)) { return true; }

    // ShaderPreprocessor loads in-memory source through ShaderConvertAndStripComments
    // before STB: physical comments end before splicing, and noncomment TCHARs narrow
    // to ANSICHAR. Check that source view too, without calling a compiler or modifying
    // the persisted value. Low-byte projection is conservative for the SIMD fast path.
    FString ShaderView; ShaderView.Reserve(Text.Len());
    bool bPhysicalLineComment = false, bPhysicalBlockComment = false;
    for (int32 Index = 0; Index < Text.Len(); ++Index)
    {
        const TCHAR Ch = Text[Index];
        const TCHAR Next = Index + 1 < Text.Len() ? Text[Index + 1] : 0;
        if (Ch == TEXT('\r') || Ch == TEXT('\n'))
        {
            ShaderView.AppendChar(TEXT('\n')); bPhysicalLineComment = false;
            if ((Ch == TEXT('\r') && Next == TEXT('\n')) || (Ch == TEXT('\n') && Next == TEXT('\r'))) { ++Index; }
            continue;
        }
        if (bPhysicalLineComment) { continue; }
        if (bPhysicalBlockComment)
        { if (Ch == TEXT('*') && Next == TEXT('/')) { bPhysicalBlockComment = false; ++Index; } continue; }
        if (Ch == TEXT('/') && Next == TEXT('/')) { bPhysicalLineComment = true; ++Index; continue; }
        if (Ch == TEXT('/') && Next == TEXT('*')) { bPhysicalBlockComment = true; ++Index; continue; }
        // The installed comment stripper does not treat quotes as a special state.
        ShaderView.AppendChar(static_cast<TCHAR>(static_cast<uint8>(Ch)));
    }
    if (bPhysicalBlockComment) { return true; }
    return CheckText(ShaderView, false) || CheckText(MoveTemp(ShaderView), true);
}

bool ValidateCustomShaderProperty(UObject* Object, const FProperty* Property, const void* Address, FString& Error)
{
    if (!Object->IsA<UMaterialExpressionCustom>()) { return true; }
    const FName Name = Property->GetFName();
    bool bUnsupported = false;
    if (Name == GET_MEMBER_NAME_CHECKED(UMaterialExpressionCustom, Code))
    {
        bUnsupported = ContainsCustomIncludeDirective(*static_cast<const FString*>(Address), true, &Error);
    }
    else if (Name == GET_MEMBER_NAME_CHECKED(UMaterialExpressionCustom, IncludeFilePaths))
    {
        for (const FString& IncludePath : *static_cast<const TArray<FString>*>(Address))
        { if (!ValidateCustomShaderFilePath(IncludePath, Error)) { return false; } }
    }
    else if (Name == GET_MEMBER_NAME_CHECKED(UMaterialExpressionCustom, AdditionalDefines))
    {
        for (const FCustomDefine& Define : *static_cast<const TArray<FCustomDefine>*>(Address))
        {
            bUnsupported |= ContainsCustomIncludeDirective(Define.DefineName, false) || ContainsCustomIncludeDirective(Define.DefineValue, true, &Error, false);
        }
    }
    else if (Name == GET_MEMBER_NAME_CHECKED(UMaterialExpressionCustom, Inputs))
    {
        for (const FCustomInput& Input : *static_cast<const TArray<FCustomInput>*>(Address))
        {
            bUnsupported |= ContainsCustomIncludeDirective(Input.InputName.ToString(), false);
        }
    }
    else if (Name == GET_MEMBER_NAME_CHECKED(UMaterialExpressionCustom, AdditionalOutputs))
    {
        for (const FCustomOutput& Output : *static_cast<const TArray<FCustomOutput>*>(Address))
        {
            bUnsupported |= ContainsCustomIncludeDirective(Output.OutputName.ToString(), false);
        }
    }
    if (bUnsupported)
    {
        if (Error.IsEmpty())
        { Error = TEXT("Custom shader dependency scope is incomplete: bounded quoted literal include DAGs of existing mapped sources and matching IncludeFilePaths admit file-relative child paths and exact file-side pragma once. Computed/generated/platform/future sources, guarded cycles, alternate directives, import/embed/other pragmas and include-query operators remain valid pending. AdditionalDefines permit file-independent values only. The same guard applies to specialized Add, generic Add and Set."); }
        return false;
    }
    return true;
}
