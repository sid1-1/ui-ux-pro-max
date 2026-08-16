#include "EspSubsystem.h"

#include "EspTargetComponent.h"

void UEspSubsystem::Register(UEspTargetComponent* Component)
{
	if (Component)
	{
		Targets.AddUnique(Component);
	}
}

void UEspSubsystem::Unregister(UEspTargetComponent* Component)
{
	if (Component)
	{
		Targets.RemoveSingleSwap(Component);
	}
}

void UEspSubsystem::Compact()
{
	// Weak pointers mean a hard-destroyed actor that never ran EndPlay cannot dangle,
	// but it can still leave a null slot -- sweep those out.
	Targets.RemoveAllSwap([](const TWeakObjectPtr<UEspTargetComponent>& Weak)
	{
		const UEspTargetComponent* Component = Weak.Get();
		return Component == nullptr || Component->GetOwner() == nullptr;
	});
}
