// Fill out your copyright notice in the Description page of Project Settings.


#include "DepthPeeling.h"

#include "Engine.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/PostProcessVolume.h"
#include "Components/SceneCaptureComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"

static int32 s_TickCount = 0;

// Sets default values
ADepthPeeling::ADepthPeeling()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = ETickingGroup::TG_PostUpdateWork;
}

// Called when the game starts or when spawned
void ADepthPeeling::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();

	if (!OITActor)
	{
		return;
	}

	for (TActorIterator<APlayerController> It(GetWorld()); It; ++It)
	{
		PlayerCameraManager = (*It)->PlayerCameraManager;
		break;
	}

	for (TActorIterator<ASceneCapture2D> It(World); It; ++It)
	{
		SceneCapture2D = *It;
		break;
	}

	if (!SceneCapture2D)
	{
		return;
	}

	USceneCaptureComponent2D* SceneCaptureComponent2D = SceneCapture2D->GetCaptureComponent2D();
	SceneCaptureComponent2D->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	SceneCaptureComponent2D->CaptureSource = ESceneCaptureSource::SCS_SceneColorSceneDepth;
	SceneCaptureComponent2D->bCaptureEveryFrame = false;

	// Load OITActor's material and create mid
	M_Actor_ColorDepth = LoadObject<UMaterial>(GetTransientPackage(), TEXT("/Game/M_Actor_ColorDepth.M_Actor_ColorDepth"));
	if (M_Actor_ColorDepth)
	{
		MID_Actor_ColorDepth = UMaterialInstanceDynamic::Create(M_Actor_ColorDepth, this, FName("MID_Actor_ColorDepth"));
	}

	OITActor->SetOwner(PlayerCameraManager->GetViewTarget());

	TArray<UMeshComponent*> MeshComponents;
	OITActor->GetComponents<UMeshComponent>(MeshComponents, true);
	for (UMeshComponent* MeshComponent : MeshComponents)
	{
		MeshComponent->bOwnerNoSee = true;

		const TArray<UMaterialInterface*> MaterialInterfaces = MeshComponent->GetMaterials();
		for (int32 i = 0; i < MaterialInterfaces.Num(); ++i)
		{
			// Replace the translucent section
			UMaterialInterface* MaterialInterface = MaterialInterfaces[i];
			if (MaterialInterface->GetBlendMode() == EBlendMode::BLEND_Translucent)
			{
				MeshComponent->SetMaterial(i, MID_Actor_ColorDepth);
			}
		}
	}

	// Add OITActor to SceneCaptureComponent2D
	SceneCaptureComponent2D->ShowOnlyActors.Add(OITActor);

	// Load post process compose material and set to PostProcessVolume
	M_PP_Compose = LoadObject<UMaterial>(GetTransientPackage(), TEXT("/Game/M_PP_Compose.M_PP_Compose"));
	if (M_PP_Compose != nullptr)
	{
		MID_PP_Compose = UMaterialInstanceDynamic::Create(M_PP_Compose, this, FName("MID_PP_Compose"));
	}

	for (TActorIterator<APostProcessVolume> It(GetWorld()); It; ++It)
	{
		PostProcessVolume = (*It);
		break;
	}

	PostProcessVolume = World->PostProcessVolumes.Num() > 0 ? (APostProcessVolume*)World->PostProcessVolumes[0] : nullptr;
}

// Called every frame
void ADepthPeeling::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld* World = GetWorld();

	if (!OITActor)
	{
		return;
	}

	if (!PlayerCameraManager)
	{
		return;
	}

	/* Draw Debug Line
	FVector LineTrace = PlayerCameraManager->GetActorForwardVector() * 10000.0f;
	LineTrace -= PlayerCameraManager->GetCameraLocation();
	DrawDebugLine(World, PlayerCameraManager->GetCameraLocation(), LineTrace, FColor::Red);
	*/

	if (!SceneCapture2D)
	{
		return;
	}

	FVector2D ViewportSize(1, 1);
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}

	// Update SceneCapture2D transform
	USceneCaptureComponent2D* SceneCaptureComponent2D = SceneCapture2D->GetCaptureComponent2D();
	SceneCapture2D->SetActorLocationAndRotation(PlayerCameraManager->GetCameraLocation(), PlayerCameraManager->GetCameraRotation());
	SceneCaptureComponent2D->FOVAngle = PlayerCameraManager->GetFOVAngle();
	SceneCaptureComponent2D->OrthoWidth = PlayerCameraManager->GetOrthoWidth();

	UTextureRenderTarget2D* TextureRenderTarget2D = SceneCaptureComponent2D->TextureTarget;
	if (!TextureRenderTarget2D)
	{
		return;
	}

	TextureRenderTarget2D->InitCustomFormat(ViewportSize.X, ViewportSize.Y, PF_FloatRGBA, false);

	// Capture scene PeelingNum times
	for (int32 Index = 0; Index < PeelingNum; Index++)
	{
		if (!RT_ColorDepths.IsValidIndex(Index))
		{
			// Create RT_ColorDepth
			FName Name = *FString::Format(TEXT("RT_ColorDepth_{0}"), { Index });
			UTextureRenderTarget2D* RT_ColorDepth = NewObject<UTextureRenderTarget2D>(this, Name);
			RT_ColorDepth->ClearColor = FLinearColor::Transparent;
			RT_ColorDepths.Add(RT_ColorDepth);
		}

		RT_ColorDepths[Index]->InitCustomFormat(ViewportSize.X, ViewportSize.Y, PF_FloatRGBA, false);

		// Set last RT_Color and RT_Depth to MID_Mask_Actor except for the first time
		if (Index >= 1)
		{
			MID_Actor_ColorDepth->SetTextureParameterValue(TEXT("RT_Depth"), RT_ColorDepths[Index - 1]);
		}

		// Capture color and copy to RT_Color
		SceneCaptureComponent2D->CaptureScene();

		UTextureRenderTarget2D* RT_TextureTarget = SceneCaptureComponent2D->TextureTarget;
		UTextureRenderTarget2D* RT_ColorDepth = RT_ColorDepths[Index];

		ENQUEUE_RENDER_COMMAND(FCopyToRT_ColorDepth)(
			[RT_TextureTarget, RT_ColorDepth](FRHICommandListImmediate& RHICmdList)
		{
			check(IsInRenderingThread());

			RHICmdList.CopyToResolveTarget(
				RT_TextureTarget->GetRenderTargetResource()->GetRenderTargetTexture(),
				RT_ColorDepth->GetRenderTargetResource()->GetRenderTargetTexture(),
				FResolveParams());

			RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
		});

		FlushRenderingCommands();
	}

	// Reset MID_Actor_Color's T_ColorDepth to default black
	UTexture* RT_Black = nullptr;
	UMaterial* BaseMaterial = MID_Actor_ColorDepth->GetBaseMaterial();
	BaseMaterial->GetTextureParameterValue(TEXT("RT_Depth"), RT_Black);
	MID_Actor_ColorDepth->SetTextureParameterValue(TEXT("RT_Depth"), RT_Black);

	// Set all RT_Color to MID_PP_Compose for composing
	for (int32 Index = 0; Index < RT_ColorDepths.Num(); Index++)
	{
		FName Name = *FString::Format(TEXT("RT_Color_{0}"), { Index });
		if (Index < PeelingNum)
		{
			MID_PP_Compose->SetTextureParameterValue(Name, RT_ColorDepths[Index]);
		}
		else
		{
			MID_PP_Compose->SetTextureParameterValue(Name, RT_Black);
		}
	}

	if (PostProcessVolume)
	{
		PostProcessVolume->AddOrUpdateBlendable(MID_PP_Compose, PP_Compose_Weight_Debug);
	}
}