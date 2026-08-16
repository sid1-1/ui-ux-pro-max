#include "EspHUD.h"

#include "EspProjection.h"
#include "EspSubsystem.h"
#include "EspTargetComponent.h"

#include "CollisionQueryParams.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	struct FEspBoneLink
	{
		const TCHAR* From;
		const TCHAR* To;
	};

	/**
	 * UE5 Manny / Quinn bone names. If you use the UE4 mannequin, Mixamo, or a custom rig,
	 * edit this table -- links whose bones are missing are silently skipped, so a wrong
	 * table shows up as "no skeleton lines" rather than a crash.
	 */
	const FEspBoneLink GBoneLinks[] =
	{
		{ TEXT("pelvis"),     TEXT("spine_01")   },
		{ TEXT("spine_01"),   TEXT("spine_02")   },
		{ TEXT("spine_02"),   TEXT("spine_03")   },
		{ TEXT("spine_03"),   TEXT("neck_01")    },
		{ TEXT("neck_01"),    TEXT("head")       },

		{ TEXT("spine_03"),   TEXT("clavicle_l") },
		{ TEXT("clavicle_l"), TEXT("upperarm_l") },
		{ TEXT("upperarm_l"), TEXT("lowerarm_l") },
		{ TEXT("lowerarm_l"), TEXT("hand_l")     },

		{ TEXT("spine_03"),   TEXT("clavicle_r") },
		{ TEXT("clavicle_r"), TEXT("upperarm_r") },
		{ TEXT("upperarm_r"), TEXT("lowerarm_r") },
		{ TEXT("lowerarm_r"), TEXT("hand_r")     },

		{ TEXT("pelvis"),     TEXT("thigh_l")    },
		{ TEXT("thigh_l"),    TEXT("calf_l")     },
		{ TEXT("calf_l"),     TEXT("foot_l")     },

		{ TEXT("pelvis"),     TEXT("thigh_r")    },
		{ TEXT("thigh_r"),    TEXT("calf_r")     },
		{ TEXT("calf_r"),     TEXT("foot_r")     },
	};

	/** UE5 vector components are doubles; Canvas draw calls take floats. */
	template <typename T>
	FORCEINLINE float Fx(T Value)
	{
		return static_cast<float>(Value);
	}
}

AEspHUD::AEspHUD()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AEspHUD::EspToggle()    { bEnabled = !bEnabled; }
void AEspHUD::EspRadar()     { bShowRadar = !bShowRadar; }
void AEspHUD::EspSkeleton()  { bShowSkeletons = !bShowSkeletons; }
void AEspHUD::EspSnaplines() { bShowSnaplines = !bShowSnaplines; }

void AEspHUD::EspRecord(bool bEnable)
{
	bRecording = bEnable;
	if (bEnable)
	{
		Samples.Reset();
	}
	UE_LOG(LogTemp, Warning, TEXT("[ESP] recording %s"), bEnable ? TEXT("started") : TEXT("stopped"));
}

void AEspHUD::EspDump()
{
	FString Csv = TEXT("time,label,x,y,z,pitch,yaw,roll\n");
	Csv.Reserve(Samples.Num() * 64 + 64);

	for (const FEspSample& Sample : Samples)
	{
		Csv += FString::Printf(TEXT("%.3f,%s,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f\n"),
			Sample.Time,
			*Sample.Label,
			Sample.Location.X, Sample.Location.Y, Sample.Location.Z,
			Sample.Rotation.Pitch, Sample.Rotation.Yaw, Sample.Rotation.Roll);
	}

	const FString Path = FPaths::ProjectSavedDir() / TEXT("Esp") / TEXT("session.csv");
	if (FFileHelper::SaveStringToFile(Csv, *Path))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ESP] wrote %d samples to %s"), Samples.Num(), *Path);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[ESP] failed to write %s"), *Path);
	}
}

void AEspHUD::DrawHUD()
{
	Super::DrawHUD();

#if !UE_BUILD_SHIPPING
	if (!bEnabled || !Canvas)
	{
		return;
	}

	APlayerController* PlayerController = GetOwningPlayerController();
	UWorld* World = GetWorld();
	if (!PlayerController || !World)
	{
		return;
	}

	UEspSubsystem* Subsystem = World->GetSubsystem<UEspSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	Subsystem->Compact();

	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const float Now = World->GetTimeSeconds();
	const float SkeletonCutoff = MaxDistance * SkeletonDistanceFraction;
	const float LabelCutoff = MaxDistance * LabelDistanceFraction;
	const int32 Interval = FMath::Max(1, TraceInterval);

	const TArray<TWeakObjectPtr<UEspTargetComponent>>& Targets = Subsystem->GetTargets();
	for (int32 Index = 0; Index < Targets.Num(); ++Index)
	{
		UEspTargetComponent* Target = Targets[Index].Get();
		if (!Target || !Target->GetOwner())
		{
			continue;
		}

		FVector Feet, Head, Center;
		Target->GetExtents(Feet, Head, Center);

		const float Distance = Fx(FVector::Dist(ViewLocation, Center));
		if (Distance > MaxDistance)
		{
			continue;
		}

		FEspProjection FeetProjection, HeadProjection;
		if (!EspDebug::ProjectWithDepth(PlayerController, Feet, FeetProjection) ||
			!EspDebug::ProjectWithDepth(PlayerController, Head, HeadProjection))
		{
			continue;
		}

		// Either extent behind the camera means the box would be nonsense. Fall back to an
		// edge arrow pointing at the target instead.
		if (!FeetProjection.IsInFront() || !HeadProjection.IsInFront())
		{
			FEspProjection CenterProjection;
			if (EspDebug::ProjectWithDepth(PlayerController, Center, CenterProjection))
			{
				DrawOffscreenArrow(CenterProjection, Target->Color);
			}
			continue;
		}

		// Visibility traces are the expensive part. Spread them across frames and reuse the
		// previous answer in between -- at 60fps a 3-frame stagger is imperceptible.
		if ((Index % Interval) == (FrameCounter % Interval))
		{
			FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EspVisibility), /*bTraceComplex=*/false, PlayerController->GetPawn());
			QueryParams.AddIgnoredActor(Target->GetOwner());

			FHitResult Hit;
			Target->bLastOccluded = World->LineTraceSingleByChannel(Hit, ViewLocation, Center, ECC_Visibility, QueryParams);
		}

		FLinearColor Color = Target->Color;
		if (Target->bLastOccluded)
		{
			Color.A *= OccludedAlpha;
		}

		const float BoxHeight = FMath::Abs(Fx(FeetProjection.Screen.Y) - Fx(HeadProjection.Screen.Y));
		const float BoxWidth = BoxHeight * Target->WidthRatio;
		const FVector2D TopLeft(
			(FeetProjection.Screen.X + HeadProjection.Screen.X) * 0.5 - BoxWidth * 0.5,
			FMath::Min(FeetProjection.Screen.Y, HeadProjection.Screen.Y));

		DrawBracketBox(TopLeft, BoxWidth, BoxHeight, Color);
		DrawHealthBar(TopLeft, BoxHeight, Target->Health01);

		if (Distance <= LabelCutoff)
		{
			DrawText(
				FString::Printf(TEXT("%s  %.0fm"), *Target->Label, Distance / 100.f),
				Color,
				Fx(TopLeft.X), Fx(TopLeft.Y) - 16.f,
				GEngine ? GEngine->GetSmallFont() : nullptr,
				1.f);
		}

		if (bShowSnaplines)
		{
			Line2D(FVector2D(Canvas->SizeX * 0.5f, Canvas->SizeY), FeetProjection.Screen, Color, LineThickness);
		}

		if (bShowSkeletons && Target->bDrawSkeleton && Distance <= SkeletonCutoff)
		{
			DrawSkeletonLines(PlayerController, Target, Color);
		}

		if (bRecording && Samples.Num() < 200000)
		{
			Samples.Add({ Now, Target->Label, Center, Target->GetOwner()->GetActorRotation() });
		}
	}

	++FrameCounter;

	if (bShowRadar)
	{
		DrawRadarWidget(PlayerController, Subsystem);
	}
#endif // !UE_BUILD_SHIPPING
}

void AEspHUD::DrawBracketBox(const FVector2D& TopLeft, float Width, float Height, const FLinearColor& Color)
{
	// Corner brackets rather than a full rectangle: stays readable when the box is only a
	// few pixels tall, and costs the same eight lines at any distance.
	const float Len = FMath::Max(4.f, Width * 0.25f);
	const float X = Fx(TopLeft.X);
	const float Y = Fx(TopLeft.Y);
	const float R = X + Width;
	const float B = Y + Height;

	Line2D(FVector2D(X, Y), FVector2D(X + Len, Y), Color, LineThickness);
	Line2D(FVector2D(X, Y), FVector2D(X, Y + Len), Color, LineThickness);

	Line2D(FVector2D(R - Len, Y), FVector2D(R, Y), Color, LineThickness);
	Line2D(FVector2D(R, Y), FVector2D(R, Y + Len), Color, LineThickness);

	Line2D(FVector2D(X, B - Len), FVector2D(X, B), Color, LineThickness);
	Line2D(FVector2D(X, B), FVector2D(X + Len, B), Color, LineThickness);

	Line2D(FVector2D(R, B - Len), FVector2D(R, B), Color, LineThickness);
	Line2D(FVector2D(R - Len, B), FVector2D(R, B), Color, LineThickness);
}

void AEspHUD::DrawHealthBar(const FVector2D& TopLeft, float Height, float Health01)
{
	const float Clamped = FMath::Clamp(Health01, 0.f, 1.f);
	const float Filled = Height * Clamped;
	const FVector2D BarTopLeft(TopLeft.X - 8.f, TopLeft.Y);

	Rect2D(BarTopLeft, FVector2D(4.f, Height), FLinearColor(0.f, 0.f, 0.f, 0.6f));
	Rect2D(FVector2D(BarTopLeft.X, BarTopLeft.Y + Height - Filled), FVector2D(4.f, Filled),
		FLinearColor::LerpUsingHSV(FLinearColor::Red, FLinearColor::Green, Clamped));
}

void AEspHUD::DrawSkeletonLines(const APlayerController* PlayerController, UEspTargetComponent* Target, const FLinearColor& Color)
{
	USkeletalMeshComponent* Mesh = Target->GetMesh();
	if (!Mesh)
	{
		return;
	}

	// One-time probe so a rig with different bone names does not cost us 19 failed lookups
	// every frame forever.
	if (!Target->bSkeletonChecked)
	{
		Target->bSkeletonChecked = true;
		Target->bSkeletonUsable = Mesh->GetBoneIndex(FName(GBoneLinks[0].From)) != INDEX_NONE;

		if (!Target->bSkeletonUsable)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[ESP] '%s' has no bone named '%s' -- edit GBoneLinks in EspHUD.cpp to match your rig."),
				*Target->GetOwner()->GetName(), GBoneLinks[0].From);
		}
	}

	if (!Target->bSkeletonUsable)
	{
		return;
	}

	for (const FEspBoneLink& Link : GBoneLinks)
	{
		const FName FromBone(Link.From);
		const FName ToBone(Link.To);

		if (Mesh->GetBoneIndex(FromBone) == INDEX_NONE || Mesh->GetBoneIndex(ToBone) == INDEX_NONE)
		{
			continue;
		}

		FEspProjection From, To;
		if (!EspDebug::ProjectWithDepth(PlayerController, Mesh->GetBoneLocation(FromBone), From) || !From.IsInFront())
		{
			continue;
		}
		if (!EspDebug::ProjectWithDepth(PlayerController, Mesh->GetBoneLocation(ToBone), To) || !To.IsInFront())
		{
			continue;
		}

		Line2D(From.Screen, To.Screen, Color, 1.f);
	}
}

void AEspHUD::DrawOffscreenArrow(const FEspProjection& Projection, const FLinearColor& Color)
{
	FVector2D Screen = Projection.Screen;

	// A point behind the camera projects point-mirrored through the screen centre. Undo that
	// before working out which edge to pin the arrow to -- skipping this is the classic bug
	// where arrows point exactly the wrong way.
	if (!Projection.IsInFront())
	{
		Screen.X = Canvas->SizeX - Screen.X;
		Screen.Y = Canvas->SizeY - Screen.Y;
	}

	const FVector2D Center(Canvas->SizeX * 0.5f, Canvas->SizeY * 0.5f);
	FVector2D Direction = Screen - Center;
	if (Direction.IsNearlyZero())
	{
		return;
	}
	Direction.Normalize();

	const float Radius = FMath::Min(Fx(Center.X), Fx(Center.Y)) - 60.f;
	const FVector2D Tip = Center + Direction * Radius;
	const FVector2D Perpendicular(-Direction.Y, Direction.X);

	const FVector2D BaseA = Tip - Direction * 22.f + Perpendicular * 11.f;
	const FVector2D BaseB = Tip - Direction * 22.f - Perpendicular * 11.f;

	Line2D(Tip, BaseA, Color, 3.f);
	Line2D(Tip, BaseB, Color, 3.f);
	Line2D(BaseA, BaseB, Color, 3.f);
}

void AEspHUD::DrawRadarWidget(const APlayerController* PlayerController, const UEspSubsystem* Subsystem)
{
	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	// Yaw only, so the radar stays a flat top-down map while you look up and down.
	ViewRotation.Pitch = 0.f;
	ViewRotation.Roll = 0.f;

	const FVector2D Origin(Canvas->SizeX - RadarRadius - 30.f, RadarRadius + 30.f);

	Rect2D(FVector2D(Origin.X - RadarRadius, Origin.Y - RadarRadius),
		FVector2D(RadarRadius * 2.f, RadarRadius * 2.f),
		FLinearColor(0.f, 0.f, 0.f, 0.35f));

	// Own marker: a short tick pointing "forward", which on this radar is up.
	Line2D(FVector2D(Origin.X, Origin.Y - 8.f), FVector2D(Origin.X, Origin.Y + 8.f), FLinearColor::White, 1.f);

	const float Range = FMath::Max(1.f, RadarWorldRange);

	for (const TWeakObjectPtr<UEspTargetComponent>& Weak : Subsystem->GetTargets())
	{
		const UEspTargetComponent* Target = Weak.Get();
		if (!Target || !Target->GetOwner())
		{
			continue;
		}

		const FVector Local = ViewRotation.UnrotateVector(Target->GetOwner()->GetActorLocation() - ViewLocation);

		// Local X is forward, Y is right. Radar X is right, radar Y is forward.
		FVector2D Dot(Fx(Local.Y), Fx(Local.X));
		Dot = Dot / Range * RadarRadius;
		if (Dot.Size() > RadarRadius)
		{
			Dot = Dot.GetSafeNormal() * RadarRadius;
		}

		// Screen Y grows downward, so forward has to be subtracted.
		Rect2D(FVector2D(Origin.X + Dot.X - 3.f, Origin.Y - Dot.Y - 3.f), FVector2D(6.f, 6.f), Target->Color);
	}
}

void AEspHUD::Line2D(const FVector2D& A, const FVector2D& B, const FLinearColor& Color, float Thickness)
{
	DrawLine(Fx(A.X), Fx(A.Y), Fx(B.X), Fx(B.Y), Color, Thickness);
}

void AEspHUD::Rect2D(const FVector2D& TopLeft, const FVector2D& Size, const FLinearColor& Color)
{
	DrawRect(Color, Fx(TopLeft.X), Fx(TopLeft.Y), Fx(Size.X), Fx(Size.Y));
}
