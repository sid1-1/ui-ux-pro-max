#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "EspHUD.generated.h"

class UEspSubsystem;
class UEspTargetComponent;
struct FEspProjection;

/** One recorded frame for a single target. Flushed to CSV by the EspDump console command. */
struct FEspSample
{
	float Time = 0.f;
	FString Label;
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
};

/**
 * Debug overlay HUD. Set this as your GameMode's HUDClass (or reparent your existing HUD
 * Blueprint to it) and every actor carrying a UEspTargetComponent gets drawn.
 *
 * The entire draw path is compiled out of Shipping builds.
 *
 * Console commands (four-finger tap opens the console on mobile dev builds):
 *   EspToggle      - all overlay drawing on/off
 *   EspRadar       - radar on/off
 *   EspSkeleton    - skeleton lines on/off
 *   EspSnaplines   - snaplines on/off
 *   EspRecord 1|0  - start/stop position recording
 *   EspDump        - write the recording to Saved/Esp/session.csv
 */
UCLASS(Blueprintable)
class ESPDEBUG_API AEspHUD : public AHUD
{
	GENERATED_BODY()

public:
	AEspHUD();

	virtual void DrawHUD() override;

	UFUNCTION(Exec, BlueprintCallable, Category = "ESP")
	void EspToggle();

	UFUNCTION(Exec, BlueprintCallable, Category = "ESP")
	void EspRadar();

	UFUNCTION(Exec, BlueprintCallable, Category = "ESP")
	void EspSkeleton();

	UFUNCTION(Exec, BlueprintCallable, Category = "ESP")
	void EspSnaplines();

	UFUNCTION(Exec, BlueprintCallable, Category = "ESP")
	void EspRecord(bool bEnable);

	UFUNCTION(Exec, BlueprintCallable, Category = "ESP")
	void EspDump();

	/** Targets beyond this distance (cm) are skipped entirely. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESP")
	float MaxDistance = 8000.f;

	/** Skeleton lines only draw inside this fraction of MaxDistance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESP", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SkeletonDistanceFraction = 0.4f;

	/** Labels only draw inside this fraction of MaxDistance. Canvas text is costly on mobile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESP", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LabelDistanceFraction = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESP")
	float LineThickness = 2.f;

	/** Alpha multiplier applied to targets the camera cannot see. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESP", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OccludedAlpha = 0.35f;

	/** Run one visibility trace per target every N frames. 1 = every frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESP|Advanced", meta = (ClampMin = "1", ClampMax = "16"))
	int32 TraceInterval = 3;

	/** World distance (cm) mapped to the radar's outer edge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESP|Radar")
	float RadarWorldRange = 5000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESP|Radar")
	float RadarRadius = 110.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESP")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESP")
	bool bShowRadar = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESP")
	bool bShowSkeletons = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESP")
	bool bShowSnaplines = true;

private:
	void DrawBracketBox(const FVector2D& TopLeft, float Width, float Height, const FLinearColor& Color);
	void DrawHealthBar(const FVector2D& TopLeft, float Height, float Health01);
	void DrawSkeletonLines(const APlayerController* PlayerController, UEspTargetComponent* Target, const FLinearColor& Color);
	void DrawOffscreenArrow(const FEspProjection& Projection, const FLinearColor& Color);
	void DrawRadarWidget(const APlayerController* PlayerController, const UEspSubsystem* Subsystem);

	void Line2D(const FVector2D& A, const FVector2D& B, const FLinearColor& Color, float Thickness);
	void Rect2D(const FVector2D& TopLeft, const FVector2D& Size, const FLinearColor& Color);

	int32 FrameCounter = 0;

	bool bRecording = false;
	TArray<FEspSample> Samples;
};
