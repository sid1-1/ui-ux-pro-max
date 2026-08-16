#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EspTargetComponent.generated.h"

class USkeletalMeshComponent;

/**
 * Add this to any actor you want the ESP overlay to track.
 *
 * The component does no ticking and no scanning -- it registers itself with UEspSubsystem
 * on BeginPlay and unregisters on EndPlay. The HUD walks that list once per frame.
 */
UCLASS(ClassGroup = (Debug), meta = (BlueprintSpawnableComponent, DisplayName = "ESP Target"))
class ESPDEBUG_API UEspTargetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEspTargetComponent();

	/** Text drawn above the box. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESP")
	FString Label = TEXT("Entity");

	/** Box, snapline and skeleton colour. Alpha is dimmed automatically when occluded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESP")
	FLinearColor Color = FLinearColor(0.f, 1.f, 1.f, 1.f);

	/** Drives the bar on the left edge of the box. Drive this from your own health system. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESP", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Health01 = 1.f;

	/** Per-actor opt-out for skeleton lines. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESP")
	bool bDrawSkeleton = true;

	/** Box width as a fraction of its on-screen height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESP", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float WidthRatio = 0.45f;

	/**
	 * Cache the collision half-height on BeginPlay instead of calling GetActorBounds every
	 * frame. Leave on for characters; turn off for actors that change size at runtime.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESP|Advanced")
	bool bCacheBounds = true;

	/** Feet / head / centre in world space, used to build the screen-space box. */
	void GetExtents(FVector& OutFeet, FVector& OutHead, FVector& OutCenter) const;

	USkeletalMeshComponent* GetMesh() const;

	/** Last occlusion result. The HUD amortises traces across frames and caches here. */
	bool bLastOccluded = false;

	/** Set true once we know this mesh's bone names do not match the link table. */
	bool bSkeletonChecked = false;
	bool bSkeletonUsable = false;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	mutable TWeakObjectPtr<USkeletalMeshComponent> CachedMesh;
	FVector CachedExtent = FVector::ZeroVector;
	bool bBoundsCached = false;
};
