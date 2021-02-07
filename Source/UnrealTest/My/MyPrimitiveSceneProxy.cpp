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

static TArray<int32> GlobalIndices = { 0, 1, 2, 0, 2, 3 };
static TArray<FVector> GlobalVertices = {
	FVector(0, 0, 0),
	FVector(0, 1, 0),
	FVector(0, 1, 1),
	FVector(0, 0, 1),
};

static const FIntVector GlobalFaceNormals[6] =
{
	FIntVector(-1, 0, 0),
	FIntVector(+1, 0, 0),
	FIntVector(0, -1, 0),
	FIntVector(0, +1, 0),
	FIntVector(0, 0, -1),
	FIntVector(0, 0, +1),
};

TGlobalResource<FMyTangentBuffer> GlobalTangentBuffer;

void FMyIndexBuffer::InitRHI()
{
	FRHIResourceCreateInfo CreateInfo;
	IndexBufferRHI = RHICreateIndexBuffer(sizeof(int32), GlobalIndices.Num() * sizeof(int32), BUF_Dynamic, CreateInfo);

	void* IndexBufferData = RHILockIndexBuffer(IndexBufferRHI, 0, GlobalIndices.Num() * sizeof(int32), RLM_WriteOnly);
	FMemory::Memcpy(IndexBufferData, &GlobalIndices[0], GlobalIndices.Num() * sizeof(int32));
	RHIUnlockIndexBuffer(IndexBufferRHI);
}

void FMyVertexBuffer::InitRHI()
{
	FRHIResourceCreateInfo CreateInfo;
	VertexBufferRHI = RHICreateVertexBuffer(GlobalVertices.Num() * sizeof(FVector), BUF_Dynamic | BUF_ShaderResource, CreateInfo);

	void* VertexBufferData = RHILockVertexBuffer(VertexBufferRHI, 0, GlobalVertices.Num() * sizeof(FVector), RLM_WriteOnly);
	FMemory::Memcpy(VertexBufferData, &GlobalVertices[0], GlobalVertices.Num() * sizeof(FVector));
	RHIUnlockVertexBuffer(VertexBufferRHI);

	PositionComponentSRV = RHICreateShaderResourceView(FShaderResourceViewInitializer(VertexBufferRHI, PF_R32_FLOAT));
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

FMyPrimitiveSceneProxy::FMyPrimitiveSceneProxy(UMyMeshComponent* Component) :
	FPrimitiveSceneProxy(Component),
	Material(nullptr),
	VertexFactory(GetScene().GetFeatureLevel(), "FMyPrimitiveSceneProxy")
{
	//VertexBuffers.InitWithDummyData(&VertexFactory, GlobalVertices.Num());

	BeginInitResource(&IndexBuffer);
	BeginInitResource(&VertexBuffer);
	//BeginInitResource(&TangentBuffer);

	ENQUEUE_RENDER_COMMAND(FMyPrimitiveSceneProxy_Init)(
		[this](FRHICommandListImmediate& RHICmdList)
		{
			FLocalVertexFactory::FDataType Data;
			
			Data.PositionComponent = FVertexStreamComponent(&VertexBuffer, 0, sizeof(FVector), VET_Float3);
			Data.PositionComponentSRV = VertexBuffer.PositionComponentSRV;

			Data.TangentBasisComponents[0] = FVertexStreamComponent(&GlobalTangentBuffer, sizeof(FPackedNormal) * (2 * 0 + 0), 0, VET_PackedNormal, EVertexStreamUsage::ManualFetch);
			Data.TangentBasisComponents[1] = FVertexStreamComponent(&GlobalTangentBuffer, sizeof(FPackedNormal) * (2 * 0 + 1), 0, VET_PackedNormal, EVertexStreamUsage::ManualFetch);
			Data.TangentsSRV = VertexBuffer.PositionComponentSRV;

			Data.TextureCoordinates.Add(FVertexStreamComponent(&VertexBuffer, 0, sizeof(FVector), VET_Float2, EVertexStreamUsage::ManualFetch));
			Data.TextureCoordinatesSRV = VertexBuffer.PositionComponentSRV;

			/*Data.LightMapCoordinateComponent = FVertexStreamComponent(&VertexBuffer, 0, sizeof(FVector), VET_Float2, EVertexStreamUsage::ManualFetch);
			Data.TextureCoordinatesSRV = VertexBuffer.PositionComponentSRV;

			Data.ColorComponent = FVertexStreamComponent(&VertexBuffer, 0, sizeof(FVector), VET_Color, EVertexStreamUsage::ManualFetch);
			Data.ColorComponentsSRV = VertexBuffer.PositionComponentSRV;*/

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
	//TangentBuffer.ReleaseResource();
	/*VertexBuffers.PositionVertexBuffer.ReleaseResource();
	VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
	VertexBuffers.ColorVertexBuffer.ReleaseResource();*/
	VertexFactory.ReleaseResource();
}

void FMyPrimitiveSceneProxy::SetDynamicData_RenderThread()
{
	check(IsInRenderingThread());

	/*for (int i = 0; i < GlobalVertices.Num(); i++)
	{
		const FDynamicMeshVertex Vertex = GlobalVertices[i];

		VertexBuffers.PositionVertexBuffer.VertexPosition(i) = Vertex.Position;
		VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents(i, Vertex.TangentX.ToFVector(), Vertex.GetTangentY(), Vertex.TangentZ.ToFVector());
		VertexBuffers.StaticMeshVertexBuffer.SetVertexUV(i, 0, Vertex.TextureCoordinate[0]);
		VertexBuffers.ColorVertexBuffer.VertexColor(i) = Vertex.Color;
	}

	{
		auto& VB = VertexBuffers.PositionVertexBuffer;
		void* VertexBufferData = RHILockVertexBuffer(VB.VertexBufferRHI, 0, VB.GetNumVertices() * VB.GetStride(), RLM_WriteOnly);
		FMemory::Memcpy(VertexBufferData, VB.GetVertexData(), VB.GetNumVertices() * VB.GetStride());
		RHIUnlockVertexBuffer(VB.VertexBufferRHI);
	}

	{
		auto& VB = VertexBuffers.ColorVertexBuffer;
		void* VertexBufferData = RHILockVertexBuffer(VB.VertexBufferRHI, 0, VB.GetNumVertices() * VB.GetStride(), RLM_WriteOnly);
		FMemory::Memcpy(VertexBufferData, VB.GetVertexData(), VB.GetNumVertices() * VB.GetStride());
		RHIUnlockVertexBuffer(VB.VertexBufferRHI);
	}

	{
		auto& VB = VertexBuffers.StaticMeshVertexBuffer;
		void* VertexBufferData = RHILockVertexBuffer(VB.TangentsVertexBuffer.VertexBufferRHI, 0, VB.GetTangentSize(), RLM_WriteOnly);
		FMemory::Memcpy(VertexBufferData, VB.GetTangentData(), VB.GetTangentSize());
		RHIUnlockVertexBuffer(VB.TangentsVertexBuffer.VertexBufferRHI);
	}

	{
		auto& VB = VertexBuffers.StaticMeshVertexBuffer;
		void* VertexBufferData = RHILockVertexBuffer(VB.TexCoordVertexBuffer.VertexBufferRHI, 0, VB.GetTexCoordSize(), RLM_WriteOnly);
		FMemory::Memcpy(VertexBufferData, VB.GetTexCoordData(), VB.GetTexCoordSize());
		RHIUnlockVertexBuffer(VB.TexCoordVertexBuffer.VertexBufferRHI);
	}*/

	/*{
		auto& VB = VertexBuffer;
		void* VertexBufferData = RHILockVertexBuffer(VB.VertexBufferRHI, 0, GlobalVertices.Num() * sizeof(FVector), RLM_WriteOnly);
		FMemory::Memcpy(VertexBufferData, &GlobalVertices[0], GlobalVertices.Num() * sizeof(FVector));
		RHIUnlockVertexBuffer(VB.VertexBufferRHI);
	}*/

	/*void* IndexBufferData = RHILockIndexBuffer(IndexBuffer.IndexBufferRHI, 0, GlobalIndices.Num() * sizeof(int32), RLM_WriteOnly);
	FMemory::Memcpy(IndexBufferData, &GlobalIndices[0], GlobalIndices.Num() * sizeof(int32));
	RHIUnlockIndexBuffer(IndexBuffer.IndexBufferRHI);*/
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
			DynamicPrimitiveUniformBuffer.Set(FScaleMatrix(FVector(255, 255, 255)) * GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, DrawsVelocity(), bOutputVelocity);
			BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

			BatchElement.FirstIndex = 0;
			BatchElement.NumPrimitives = GlobalIndices.Num() / 3;
			BatchElement.MinVertexIndex = 0;
			BatchElement.MaxVertexIndex = GlobalVertices.Num() - 1;
			Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
			Mesh.Type = PT_TriangleList;
			Mesh.DepthPriorityGroup = SDPG_World;
			Mesh.bCanApplyViewModeOverrides = false;
			Collector.AddMesh(ViewIndex, Mesh);
		}
	}














	/*TArray<uint32> Indices;
	TArray<FDynamicMeshVertex> Vertices;

	Vertices.Add(FVector(100, 0, 0));
	Vertices.Add(FVector(0, 100, 0));
	Vertices.Add(FVector(0, 0, 100));
	Indices.Add(0);
	Indices.Add(1);
	Indices.Add(2);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			FDynamicMeshBuilder MeshBuilder(Views[ViewIndex]->GetFeatureLevel());

			MeshBuilder.AddVertices(Vertices);
			MeshBuilder.AddTriangles(Indices);
			
			MeshBuilder.GetMesh(GetLocalToWorld(), new FColoredMaterialRenderProxy(Material->GetRenderProxy(), FLinearColor::Red), GetDepthPriorityGroup(Views[ViewIndex]), true, true, ViewIndex, Collector);

			FMeshBatch& Mesh = Collector.AllocateMesh();
			FMeshBatchElement& BatchElement = Mesh.Elements[0];
		}
	}*/
}
