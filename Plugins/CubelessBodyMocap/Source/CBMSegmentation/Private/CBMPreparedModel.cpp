#include "CBMPreparedModel.h"
#include "NNE.h"
#include "NNEModelData.h"
#include "NNERuntimeCPU.h"
#include "NNERuntimeGPU.h"
#include "HAL/PlatformTime.h"
namespace Cubeless {
FModelFactory PrepareModel(UNNEModelData* Data,bool Shape,bool CPUOnly) {
 check(IsInGameThread());if(!Data)return {};
 if(auto* C=FindObject<UClass>(nullptr,TEXT("/Script/NNERuntimeORT.NNERuntimeORTSettings")))C->GetDefaultObject();
 TSharedPtr<UE::NNE::IModelGPU> GPU;
 if(!CPUOnly){auto R=UE::NNE::GetRuntime<INNERuntimeGPU>(TEXT("NNERuntimeORTDml"));if(R.IsValid())GPU=R->CreateModelGPU(Data);}
 auto R=UE::NNE::GetRuntime<INNERuntimeCPU>(TEXT("NNERuntimeORTCpu"));
 TSharedPtr<UE::NNE::IModelCPU> CPU=(!GPU&&R.IsValid())?R->CreateModelCPU(Data):nullptr;
 const FString Name=Data->GetName();
 return [GPU=MoveTemp(GPU),CPU=MoveTemp(CPU),Shape,Name]() mutable {
  const double Begin=FPlatformTime::Seconds();TSharedPtr<UE::NNE::IModelInstanceRunSync> Instance;
  FString Backend=TEXT("CPU");if(GPU){Instance=GPU->CreateModelInstanceGPU();Backend=TEXT("DML GPU");}if(!Instance&&CPU){Instance=CPU->CreateModelInstanceCPU();Backend=TEXT("CPU fallback");}
  GPU.Reset();CPU.Reset();
  if(Instance&&Shape){TArray<UE::NNE::FTensorShape> Shapes;for(const auto& D:Instance->GetInputTensorDescs()){if(!D.GetShape().IsConcrete())return TSharedPtr<UE::NNE::IModelInstanceRunSync>();Shapes.Add(UE::NNE::FTensorShape::MakeFromSymbolic(D.GetShape()));}if(Instance->SetInputTensorShapes(Shapes)!=UE::NNE::EResultStatus::Ok)Instance.Reset();}
  UE_LOG(LogTemp,Display,TEXT("[CBM] model %s backend=%s init=%.3fs success=%d"),*Name,*Backend,FPlatformTime::Seconds()-Begin,Instance.IsValid());return Instance;
 };
}
}
