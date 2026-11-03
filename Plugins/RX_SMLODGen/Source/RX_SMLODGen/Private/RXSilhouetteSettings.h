#pragma once
#include "RXSectionReduction.h"
#include "Dom/JsonObject.h"

namespace RXSilhouetteSettings
{
    inline void Write(FJsonObject& Row, const FRXSectionWeight& Settings)
    {
        Row.SetBoolField(TEXT("preserve_open_boundary"), Settings.bPreserveOpenBoundary);
        Row.SetBoolField(TEXT("preserve_sharp_edges"), Settings.bPreserveSharpEdges);
        Row.SetBoolField(TEXT("preserve_attribute_seams"), Settings.bPreserveAttributeSeams);
    }
    inline bool Read(const FJsonObject& Row, int32 Version, FRXSectionWeight& Settings)
    {
        if (Version < 4)
        {
            // An old saved Boolean enabled/disabled all protections together.
            bool Legacy = false;
            if ((Version >= 3 || Row.HasField(TEXT("preserve_silhouette"))) &&
                (!Row.HasTypedField<EJson::Boolean>(TEXT("preserve_silhouette")) || !Row.TryGetBoolField(TEXT("preserve_silhouette"), Legacy))) { return false; }
            Settings.bPreserveOpenBoundary = Settings.bPreserveSharpEdges = Settings.bPreserveAttributeSeams = Legacy;
            return true;
        }
        return Row.HasTypedField<EJson::Boolean>(TEXT("preserve_open_boundary")) && Row.TryGetBoolField(TEXT("preserve_open_boundary"), Settings.bPreserveOpenBoundary) &&
            Row.HasTypedField<EJson::Boolean>(TEXT("preserve_sharp_edges")) && Row.TryGetBoolField(TEXT("preserve_sharp_edges"), Settings.bPreserveSharpEdges) &&
            Row.HasTypedField<EJson::Boolean>(TEXT("preserve_attribute_seams")) && Row.TryGetBoolField(TEXT("preserve_attribute_seams"), Settings.bPreserveAttributeSeams);
    }
}
