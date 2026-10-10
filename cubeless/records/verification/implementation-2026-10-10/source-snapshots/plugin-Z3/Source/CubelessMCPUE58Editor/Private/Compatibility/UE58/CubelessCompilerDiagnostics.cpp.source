#include "Compatibility/UE58/CubelessCompilerDiagnostics.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/UObjectToken.h"

namespace
{
    void GraphContext(const TSharedRef<FJsonObject>& Result, const UEdGraph* Graph)
    {
        if (!Graph) { return; }
        Result->SetStringField(TEXT("graph_id"), Graph->GraphGuid.ToString());
        Result->SetStringField(TEXT("graph_name"), Graph->GetName());
        Result->SetStringField(TEXT("graph_path"), Graph->GetPathName());
    }

    void NodeContext(const TSharedRef<FJsonObject>& Result, const UEdGraphNode* Node)
    {
        if (!Node) { return; }
        GraphContext(Result, Node->GetGraph());
        Result->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
        Result->SetStringField(TEXT("node_name"), Node->GetName());
        Result->SetStringField(TEXT("node_title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
        Result->SetStringField(TEXT("node_class"), Node->GetClass()->GetName());
    }

    void ObjectContext(const TSharedRef<FJsonObject>& Result, const UObject* Object)
    {
        if (const UEdGraphNode* Node = Cast<UEdGraphNode>(Object)) { NodeContext(Result, Node); }
        else if (const UEdGraph* Graph = Cast<UEdGraph>(Object)) { GraphContext(Result, Graph); }
    }

    void TokenContext(const TSharedRef<FJsonObject>& Result, const IMessageToken& Token, const FCompilerResultsLog& Log)
    {
        if (Token.GetType() == EMessageToken::EdGraph)
        {
            const FEdGraphToken& GraphToken = static_cast<const FEdGraphToken&>(Token);
            if (const UEdGraphPin* Pin = Log.FindSourcePin(GraphToken.GetPin()))
            {
                NodeContext(Result, Pin->GetOwningNodeUnchecked());
                Result->SetStringField(TEXT("pin_id"), Pin->PinId.ToString());
                Result->SetStringField(TEXT("pin_name"), Pin->PinName.ToString());
                Result->SetStringField(TEXT("pin_direction"), Pin->Direction == EGPD_Output ? TEXT("output") : TEXT("input"));
                Result->SetStringField(TEXT("pin_category"), Pin->PinType.PinCategory.ToString());
                Result->SetStringField(TEXT("pin_subcategory"), Pin->PinType.PinSubCategory.ToString());
            }
            if (const UObject* Object = GraphToken.GetGraphObject()) { ObjectContext(Result, Log.FindSourceObject(Object)); }
        }
        else if (Token.GetType() == EMessageToken::Object)
        {
            const FUObjectToken& ObjectToken = static_cast<const FUObjectToken&>(Token);
            if (const UObject* Object = ObjectToken.GetObject().Get()) { ObjectContext(Result, Log.FindSourceObject(Object)); }
        }
    }
}

namespace Cubeless::UE58
{
    TSharedRef<FJsonObject> CompilerDiagnostic(const TSharedRef<FTokenizedMessage>& Message, const FCompilerResultsLog& Log)
    {
        TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        const EMessageSeverity::Type Severity = Message->GetSeverity();
        const TCHAR* Name = static_cast<int32>(Severity) <= static_cast<int32>(EMessageSeverity::Error) ? TEXT("error") :
            (Severity == EMessageSeverity::PerformanceWarning ? TEXT("performance_warning") : (Severity == EMessageSeverity::Warning ? TEXT("warning") : TEXT("info")));
        Result->SetStringField(TEXT("severity"), Name);
        Result->SetStringField(TEXT("message"), Message->ToText().ToString());
        if (!Message->GetIdentifier().IsNone()) { Result->SetStringField(TEXT("message_id"), Message->GetIdentifier().ToString()); }
        for (const TSharedRef<IMessageToken>& Token : Message->GetMessageTokens()) { TokenContext(Result, Token.Get(), Log); }
        if (const TSharedPtr<IMessageToken> Link = Message->GetMessageLink()) { TokenContext(Result, *Link, Log); }
        return Result;
    }
}
