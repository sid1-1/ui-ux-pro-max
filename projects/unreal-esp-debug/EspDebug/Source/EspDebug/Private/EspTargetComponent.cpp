#include "EspTargetComponent.h"

#include "EspSubsystem.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UEspTargetComponent::UEspTargetComponent()
{
	// Purely passive: the HUD pulls from us, we never tick.
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = false;
}

void UEspTargetComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const AActor* Owner = GetOwner())
	{
		FVector Origin, Extent;
		Owner->GetActorBounds(/*bOnlyCollidingComponents=*/true, Origin, Extent);
		CachedExtent = Extent;
		bBoundsCached = !Extent.IsNearlyZero();
	}

	if (UWorld* World = GetWorld())
	{
		if (UEspSubsystem* Subsystem = World->GetSubsystem<UEspSubsystem>())
		{
			Subsystem->Register(this);
		}
	}
}

void UEspTargetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UEspSubsystem* Subsystem = World->GetSubsystem<UEspSubsystem>())
		{
			Subsystem->Unregister(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UEspTargetComponent::GetExtents(FVector& OutFeet, FVector& OutHead, FVector& OutCenter) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		OutFeet = OutHead = OutCenter = FVector::ZeroVector;
		return;
	}

	if (bCacheBounds && bBoundsCached)
	{
		// For characters the capsule centre is the actor location, so the cached half-height
		// is all we need and we skip walking every component's bounds each frame.
		OutCenter = Owner->GetActorLocation();
		OutFeet = OutCenter - FVector(0.f, 0.f, CachedExtent.Z);
		OutHead = OutCenter + FVector(0.f, 0.f, CachedExtent.Z);
		return;
	}

	FVector Origin, Extent;
	Owner->GetActorBounds(/*bOnlyCollidingComponents=*/true, Origin, Extent);
	OutCenter = Origin;
	OutFeet = Origin - FVector(0.f, 0.f, Extent.Z);
	OutHead = Origin + FVector(0.f, 0.f, Extent.Z);
}

USkeletalMeshComponent* UEspTargetComponent::GetMesh() const
{
	if (CachedMesh.IsValid())
	{
		return CachedMesh.Get();
	}

	if (const AActor* Owner = GetOwner())
	{
		USkeletalMeshComponent* Mesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
		CachedMesh = Mesh;
		return Mesh;
	}

	return nullptr;
}
