#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "EspSubsystem.generated.h"

class UEspTargetComponent;

/**
 * Registry of everything the ESP overlay can draw.
 *
 * Deliberately a push model: components add themselves. Nothing here ever searches the
 * world, so the per-frame cost is proportional to what you tagged, not to level size.
 */
UCLASS()
class ESPDEBUG_API UEspSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void Register(UEspTargetComponent* Component);
	void Unregister(UEspTargetComponent* Component);

	const TArray<TWeakObjectPtr<UEspTargetComponent>>& GetTargets() const { return Targets; }

	/** Drops entries whose component or owning actor has been destroyed. */
	void Compact();

private:
	TArray<TWeakObjectPtr<UEspTargetComponent>> Targets;
};
