#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Runtime/Launch/Resources/Version.h"
#include "SceneView.h"
#include "UnrealClient.h"

/**
 * Result of a world -> screen projection that keeps the homogeneous W component.
 *
 * UGameplayStatics::ProjectWorldToScreen throws W away and just returns false when the
 * point is behind the camera. Off-screen arrows need those mirrored coordinates, so we
 * project by hand and let the caller decide what to do with a negative W.
 */
struct FEspProjection
{
	/** Viewport pixel coordinates. Only meaningful as-is when IsInFront() is true. */
	FVector2D Screen = FVector2D::ZeroVector;

	/** Homogeneous W. Positive means in front of the camera. */
	float W = 0.f;

	bool IsInFront() const { return W > 0.f; }
};

namespace EspDebug
{
	/**
	 * Projects a world location into viewport pixels.
	 *
	 * @return false only when there is no usable view (no local player / viewport).
	 *         A point behind the camera still returns true -- check Out.IsInFront().
	 */
	inline bool ProjectWithDepth(const APlayerController* PlayerController, const FVector& WorldLocation, FEspProjection& Out)
	{
		const ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
		if (!LocalPlayer || !LocalPlayer->ViewportClient || !LocalPlayer->ViewportClient->Viewport)
		{
			return false;
		}

		FSceneViewProjectionData ProjectionData;
#if ENGINE_MAJOR_VERSION >= 5
		if (!LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ProjectionData))
#else
		if (!LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, eSSP_FULL, ProjectionData))
#endif
		{
			return false;
		}

		const FMatrix ViewProjection = ProjectionData.ComputeViewProjectionMatrix();
		const FVector4 Result = ViewProjection.TransformFVector4(FVector4(WorldLocation, 1.0));

		if (FMath::IsNearlyZero(Result.W))
		{
			return false;
		}

		// Perspective divide. Abs() so points behind the camera produce the point-mirrored
		// position instead of garbage -- DrawOffscreenArrow un-mirrors it.
		const double InvW = 1.0 / FMath::Abs(Result.W);
		const double NormalizedX = Result.X * InvW * 0.5 + 0.5;
		const double NormalizedY = 0.5 - Result.Y * InvW * 0.5;

		const FIntRect ViewRect = ProjectionData.GetConstrainedViewRect();

		Out.W = static_cast<float>(Result.W);
		Out.Screen.X = static_cast<float>(ViewRect.Min.X + NormalizedX * ViewRect.Width());
		Out.Screen.Y = static_cast<float>(ViewRect.Min.Y + NormalizedY * ViewRect.Height());
		return true;
	}
}
