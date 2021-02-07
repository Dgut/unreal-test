#pragma once

#include "CoreMinimal.h"
#include "Components/MeshComponent.h"

class FMyIndexBuffer : public FIndexBuffer
{
public:
	virtual void InitRHI() override;
};

class FMyVertexBuffer : public FVertexBuffer
{
public:
	FShaderResourceViewRHIRef PositionComponentSRV;

	virtual void InitRHI() override;
	virtual void ReleaseRHI() override;
};

class FMyTangentBuffer : public FVertexBuffer
{
public:
	FShaderResourceViewRHIRef TangentsSRV;

	virtual void InitRHI() override;
	virtual void ReleaseRHI() override;
};

class UMyMeshComponent;

class FMyPrimitiveSceneProxy : public FPrimitiveSceneProxy
{
	/*UPROPERTY()
	UMaterial* Material;*/
	UMaterialInterface* Material;

	FMyIndexBuffer IndexBuffer;
	FMyVertexBuffer VertexBuffer;
	//FMyTangentBuffer TangentBuffer;
	//FStaticMeshVertexBuffers VertexBuffers;
	FLocalVertexFactory VertexFactory;
public:
	FMyPrimitiveSceneProxy(UMyMeshComponent* Component);

	virtual ~FMyPrimitiveSceneProxy();

	void SetDynamicData_RenderThread();

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	virtual uint32 GetMemoryFootprint(void) const override { return(sizeof(*this) + GetAllocatedSize()); }

	uint32 GetAllocatedSize(void) const { return(FPrimitiveSceneProxy::GetAllocatedSize()); }
};
