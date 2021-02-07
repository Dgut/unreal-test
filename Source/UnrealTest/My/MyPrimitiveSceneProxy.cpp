#include "MyPrimitiveSceneProxy.h"
#include "MyMeshComponent.h"

inline FIntVector operator >>(const FIntVector& L, const FIntVector& R)
{
	return FIntVector(L.X >> R.X, L.Y >> R.Y, L.Z >> R.Z);
}

static FIntVector GetCornerVertexOffset(uint8 BrickVertexIndex)
{
	return (FIntVector(BrickVertexIndex) >> FIntVector(2, 1, 0)) & 1;
}
// Maps face index and face vertex index to brick corner indices.
static const uint8 GlobalFaceVertices[6][4] =
{
	{ 2, 3, 1, 0 },		// -X
	{ 4, 5, 7, 6 },		// +X
	{ 0, 1, 5, 4 },		// -Y
	{ 6, 7, 3, 2 },		// +Y
	{ 4, 6, 2, 0 },		// -Z
	{ 1, 3, 7, 5 }		// +Z
};

const FIntVector GlobalFaceNormals[6] =
{
	FIntVector(-1, 0, 0),
	FIntVector(1, 0, 0),
	FIntVector(0, -1, 0),
	FIntVector(0, 1, 0),
	FIntVector(0, 0, -1),
	FIntVector(0, 0, 1),
};

TGlobalResource<FMyTangentBuffer> GlobalTangentBuffer;

void FMyIndexBuffer::InitRHI()
{
	FRHIResourceCreateInfo CreateInfo;
	IndexBufferRHI = RHICreateIndexBuffer(sizeof(uint16), Indices.Num() * sizeof(uint16), BUF_Dynamic, CreateInfo);

	void* IndexBufferData = RHILockIndexBuffer(IndexBufferRHI, 0, Indices.Num() * sizeof(uint16), RLM_WriteOnly);
	FMemory::Memcpy(IndexBufferData, &Indices[0], Indices.Num() * sizeof(uint16));
	RHIUnlockIndexBuffer(IndexBufferRHI);
}

void FMyVertexBuffer::InitRHI()
{
	FRHIResourceCreateInfo CreateInfo;
	VertexBufferRHI = RHICreateVertexBuffer(Vertices.Num() * sizeof(FMyVertex), BUF_Dynamic | BUF_ShaderResource, CreateInfo);

	void* VertexBufferData = RHILockVertexBuffer(VertexBufferRHI, 0, Vertices.Num() * sizeof(FMyVertex), RLM_WriteOnly);
	FMemory::Memcpy(VertexBufferData, &Vertices[0], Vertices.Num() * sizeof(FMyVertex));
	RHIUnlockVertexBuffer(VertexBufferRHI);

	PositionComponentSRV = RHICreateShaderResourceView(VertexBufferRHI, sizeof(uint8), PF_G8);/*RHICreateShaderResourceView(FShaderResourceViewInitializer(VertexBufferRHI, PF_R32_FLOAT));*/
}

void FMyVertexBuffer::ReleaseRHI()
{
	PositionComponentSRV.SafeRelease();
	FVertexBuffer::ReleaseRHI();
}

void FMyTangentBuffer::InitRHI()
{
	FRHIResourceCreateInfo CreateInfo;
	VertexBufferRHI = RHICreateVertexBuffer(12 * sizeof(FPackedNormal), BUF_Dynamic | BUF_ShaderResource, CreateInfo);

	FPackedNormal* TangentBufferData = (FPackedNormal*)RHILockVertexBuffer(VertexBufferRHI, 0, 12 * sizeof(FPackedNormal), RLM_WriteOnly);
	for (int32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
	{
		const FVector UnprojectedTangentX = FVector(+1, -1, 0).GetSafeNormal();
		const FVector UnprojectedTangentY(-1, -1, -1);
		const FVector FaceNormal(GlobalFaceNormals[FaceIndex]);
		const FVector ProjectedFaceTangentX = (UnprojectedTangentX - FaceNormal * (UnprojectedTangentX | FaceNormal)).GetSafeNormal();
		*TangentBufferData++ = ProjectedFaceTangentX;
		*TangentBufferData++ = FVector4(FaceNormal, FMath::Sign(UnprojectedTangentY | (FaceNormal ^ ProjectedFaceTangentX)));
	}
	RHIUnlockVertexBuffer(VertexBufferRHI);

	TangentsSRV = RHICreateShaderResourceView(FShaderResourceViewInitializer(VertexBufferRHI, PF_R8G8B8A8_SNORM));
}

void FMyTangentBuffer::ReleaseRHI()
{
	TangentsSRV.SafeRelease();
	FVertexBuffer::ReleaseRHI();
}

//---------------------------------------------------------------------------

void FMyPrimitiveSceneProxy::FillBuffers()
{
	VertexBuffer.Vertices.Add({ 0, 0, 0 });
	VertexBuffer.Vertices.Add({ 0, 1, 0 });
	VertexBuffer.Vertices.Add({ 0, 1, 1 });
	VertexBuffer.Vertices.Add({ 0, 0, 1 });

	IndexBuffer.Indices.Add(0);
	IndexBuffer.Indices.Add(1);
	IndexBuffer.Indices.Add(2);

	IndexBuffer.Indices.Add(0);
	IndexBuffer.Indices.Add(2);
	IndexBuffer.Indices.Add(3);
}

FMyPrimitiveSceneProxy::FMyPrimitiveSceneProxy(UMyMeshComponent* Component) :
	FPrimitiveSceneProxy(Component),
	Material(nullptr),
	VertexFactory(GetScene().GetFeatureLevel(), "FMyPrimitiveSceneProxy")
{
	FillBuffers();

	BeginInitResource(&IndexBuffer);
	BeginInitResource(&VertexBuffer);

	ENQUEUE_RENDER_COMMAND(FMyPrimitiveSceneProxy_Init)(
		[this](FRHICommandListImmediate& RHICmdList)
		{
			FLocalVertexFactory::FDataType Data;
			
			Data.PositionComponent = FVertexStreamComponent(&VertexBuffer, 0, sizeof(FMyVertex), VET_UByte4N);
			Data.PositionComponentSRV = VertexBuffer.PositionComponentSRV;

			Data.TextureCoordinates.Add(FVertexStreamComponent(&VertexBuffer, 0, sizeof(FMyVertex), VET_UByte4N, EVertexStreamUsage::ManualFetch));
			Data.TextureCoordinatesSRV = VertexBuffer.PositionComponentSRV;

			Data.TangentBasisComponents[0] = FVertexStreamComponent(&GlobalTangentBuffer, sizeof(FPackedNormal) * (2 * 0 + 0), 0, VET_PackedNormal, EVertexStreamUsage::ManualFetch);
			Data.TangentBasisComponents[1] = FVertexStreamComponent(&GlobalTangentBuffer, sizeof(FPackedNormal) * (2 * 0 + 1), 0, VET_PackedNormal, EVertexStreamUsage::ManualFetch);
			Data.TangentsSRV = VertexBuffer.PositionComponentSRV;

			VertexFactory.SetData(Data);
		});

	BeginInitResource(&VertexFactory);

	Material = Component->GetMaterial(0);
	if (Material == NULL)
	{
		Material = UMaterial::GetDefaultMaterial(MD_Surface);
	}
}

FMyPrimitiveSceneProxy::~FMyPrimitiveSceneProxy()
{
	IndexBuffer.ReleaseResource();
	VertexBuffer.ReleaseResource();
	VertexFactory.ReleaseResource();
}

void FMyPrimitiveSceneProxy::SetDynamicData_RenderThread()
{
	check(IsInRenderingThread());
}

FPrimitiveViewRelevance FMyPrimitiveSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bDynamicRelevance = true;

	return Result;
}

void FMyPrimitiveSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;

	auto WireframeMaterialInstance = new FColoredMaterialRenderProxy(
		GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy() : NULL,
		FLinearColor(0, 0.5f, 1.f)
	);

	Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);

	FMaterialRenderProxy* MaterialProxy = NULL;
	if (bWireframe)
	{
		MaterialProxy = WireframeMaterialInstance;
	}
	else
	{
		MaterialProxy = Material->GetRenderProxy();
	}

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];
			// Draw the mesh.
			FMeshBatch& Mesh = Collector.AllocateMesh();
			FMeshBatchElement& BatchElement = Mesh.Elements[0];
			BatchElement.IndexBuffer = &IndexBuffer;
			Mesh.bWireframe = bWireframe;
			Mesh.VertexFactory = &VertexFactory;
			Mesh.MaterialRenderProxy = MaterialProxy;

			bool bHasPrecomputedVolumetricLightmap;
			FMatrix PreviousLocalToWorld;
			int32 SingleCaptureIndex;
			bool bOutputVelocity;
			GetScene().GetPrimitiveUniformShaderParameters_RenderThread(GetPrimitiveSceneInfo(), bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);

			FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
			DynamicPrimitiveUniformBuffer.Set(FScaleMatrix(FVector::OneVector * 255 * 100) * GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, DrawsVelocity(), bOutputVelocity);
			BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

			BatchElement.FirstIndex = 0;
			BatchElement.NumPrimitives = IndexBuffer.Indices.Num() / 3;
			BatchElement.MinVertexIndex = 0;
			BatchElement.MaxVertexIndex = VertexBuffer.Vertices.Num() - 1;
			Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
			Mesh.Type = PT_TriangleList;
			Mesh.DepthPriorityGroup = SDPG_World;
			Mesh.bCanApplyViewModeOverrides = false;
			Collector.AddMesh(ViewIndex, Mesh);
		}
	}
}
