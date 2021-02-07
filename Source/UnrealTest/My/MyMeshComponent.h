// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/MeshComponent.h"
#include "MyMeshComponent.generated.h"

/**
 * 
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class UNREALTEST_API UMyMeshComponent : public UMeshComponent
{
	GENERATED_BODY()
public:
	UMyMeshComponent();

	/*UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Materials)
	UMaterial* Material;*/

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

	virtual void SendRenderDynamicData_Concurrent() override;

	virtual void CreateRenderState_Concurrent(FRegisterComponentContext* Context) override;

	virtual int32 GetNumMaterials() const override;
};
