// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DepthPeeling.generated.h"

class AStaticMeshActor;
class APlayerCameraManager;
class ASceneCapture2D;
class APostProcessVolume;
class UTextureRenderTarget2D;

UCLASS()
class OITDEMO_API ADepthPeeling : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ADepthPeeling();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<AActor*> OITActors;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UMaterial* M_Actor_ColorDepth;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UMaterialInstanceDynamic* MID_Actor_ColorDepth;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	APlayerCameraManager* PlayerCameraManager;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	ASceneCapture2D* SceneCapture2D;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (UIMin = '1', UIMax = '5'))
	int32 PeelingNum = 5;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<UTextureRenderTarget2D*> RT_ColorDepths;

	UPROPERTY(VisibleAnywhere)
	UMaterial* M_PP_Compose;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UMaterialInstanceDynamic* MID_PP_Compose;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	APostProcessVolume* PostProcessVolume;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 PP_Compose_Weight_Debug = 1;
};
