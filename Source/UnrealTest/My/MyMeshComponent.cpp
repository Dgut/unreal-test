// Fill out your copyright notice in the Description page of Project Settings.


#include "MyMeshComponent.h"
#include "MyPrimitiveSceneProxy.h"


UMyMeshComponent::UMyMeshComponent()
{
	/*static ConstructorHelpers::FObjectFinder<UMaterial>	Mat(TEXT("Material'/Engine/BasicShapes/BasicShapeMaterial'"));
	if (Mat.Object != nullptr)
		Material = (UMaterial*)Mat.Object;*/
}

FPrimitiveSceneProxy* UMyMeshComponent::CreateSceneProxy()
{
	FPrimitiveSceneProxy* Proxy = NULL;
	Proxy = new FMyPrimitiveSceneProxy(this);
	return Proxy;
}

FBoxSphereBounds UMyMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	static const FBoxSphereBounds ConstBounds(FBox(FVector::ZeroVector, FVector(16, 16, 16) * 100));
	return ConstBounds.TransformBy(LocalToWorld);
}

void UMyMeshComponent::SendRenderDynamicData_Concurrent()
{
	if (SceneProxy)
	{
		// Enqueue command to send to render thread
		FMyPrimitiveSceneProxy* MySceneProxy = (FMyPrimitiveSceneProxy*)SceneProxy;
		ENQUEUE_RENDER_COMMAND(MySceneProxy_SetDynamicData_RenderThread)(
			[MySceneProxy](FRHICommandListImmediate& RHICmdList)
			{
				MySceneProxy->SetDynamicData_RenderThread();
			});
	}
}

void UMyMeshComponent::CreateRenderState_Concurrent(FRegisterComponentContext* Context)
{
	Super::CreateRenderState_Concurrent(Context);

	SendRenderDynamicData_Concurrent();
}

int32 UMyMeshComponent::GetNumMaterials() const
{
	return 1;
}
