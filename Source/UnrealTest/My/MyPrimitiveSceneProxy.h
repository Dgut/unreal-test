#pragma once

#include "CoreMinimal.h"
#include "Components/MeshComponent.h"

struct FMyVertex
{
	uint8 X;
	uint8 Y;
	uint8 Z;
	uint8 AO;
	/*float X;
	float Y;
	float Z;
	float A;*/
};

class FMyIndexBuffer : public FIndexBuffer
{
public:
	TArray<uint16> Indices;

	virtual void InitRHI() override;
};

class FMyVertexBuffer : public FVertexBuffer
{
public:
	TArray<FMyVertex> Vertices;

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

	void FillBuffers();
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
