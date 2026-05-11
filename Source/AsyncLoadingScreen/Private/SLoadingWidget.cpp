/************************************************************************************
 *																					*
 * Copyright (C) 2020 Truong Bui.													*
 * Website:	https://github.com/truong-bui/AsyncLoadingScreen						*
 * Licensed under the MIT License. See 'LICENSE' file for full license information. *
 *																					*
 ************************************************************************************/

#include "SLoadingWidget.h"

#include "AsyncLoadingScreen.h"
#include "Widgets/Images/SImage.h"
#include "Slate/DeferredCleanupSlateBrush.h"
#include "Widgets/Layout/SSpacer.h"
#include "Engine/Texture2D.h"
#include "MoviePlayer.h"
#include "Widgets/SCompoundWidget.h"

int32 SLoadingWidget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{		
	TotalDeltaTime += Args.GetDeltaTime();

	if (TotalDeltaTime >= Interval)
	{
		if (CleanupBrushList.Num() > 1)
		{
			if (bPlayReverse)
			{
				ImageIndex--;
			}
			else
			{
				ImageIndex++;
			}

			if (ImageIndex >= CleanupBrushList.Num())
			{
				ImageIndex = 0;
			}
			else if (ImageIndex < 0)
			{
				ImageIndex = CleanupBrushList.Num() - 1;
			}

			StaticCastSharedRef<SImage>(LoadingIcon)->SetImage(CleanupBrushList[ImageIndex].IsValid() ? CleanupBrushList[ImageIndex]->GetSlateBrush() : nullptr);			
		}

		TotalDeltaTime = 0.0f;
	}
	

	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

SThrobber::EAnimation SLoadingWidget::GetThrobberAnimation(const FThrobberSettings& ThrobberSettings) const
{
	const int32 AnimationParams = (ThrobberSettings.bAnimateVertically ? SThrobber::Vertical : 0) |
		(ThrobberSettings.bAnimateHorizontally ? SThrobber::Horizontal : 0) |
		(ThrobberSettings.bAnimateOpacity ? SThrobber::Opacity : 0);

	return static_cast<SThrobber::EAnimation>(AnimationParams);
}

void SLoadingWidget::ConstructLoadingIcon(const FLoadingWidgetSettings& Settings)
{
	WidgetSettings = &Settings;
	if (Settings.LoadingIconType == ELoadingIconType::LIT_ImageSequence)
	{
		// Loading Widget is image sequence
		if (Settings.ImageSequenceSettings.Images.Num() > 0)
		{
			CleanupBrushList.Empty();
			ImageIndex = 0;

			FVector2D Scale = Settings.ImageSequenceSettings.Scale;
			FVector2D DesiredImageSize = Settings.ImageSequenceSettings.DesiredSize;

			for (int i = 0; i < Settings.ImageSequenceSettings.Images.Num(); ++i)
			{
				UTexture2D* LoadingImage = nullptr;
				FAsyncLoadingScreenModule& LoadingScreenModule = FAsyncLoadingScreenModule::Get();
				if (LoadingScreenModule.IsPreloadImagesEnabled())
				{
					// Get Preloaded image
					TArray<UTexture2D*> SequenceImages = LoadingScreenModule.GetSequenceImages();
					if (!SequenceImages.IsEmpty() && SequenceImages.IsValidIndex(i))
					{
						LoadingImage = SequenceImages[i];
					}
				}

				// Not preloaded
				if (!LoadingImage)
				{
					// Load background from settings
					const FSoftObjectPath& ImageAsset = Settings.ImageSequenceSettings.Images[i];
					LoadingImage = Cast<UTexture2D>(ImageAsset.TryLoad());
				}

				if (LoadingImage)
				{
					double Width = (DesiredImageSize.X > 0) ? DesiredImageSize.X : LoadingImage->GetSurfaceWidth() * Scale.X;
					double Height = (DesiredImageSize.Y > 0) ? DesiredImageSize.Y : LoadingImage->GetSurfaceHeight() * Scale.Y;
					//UE_LOG(LogTemp, Warning, TEXT("ImageWidth: %.2f Scale.X: %.2f - ImageHeight: %.2f Scale.Y: %.2f"), LoadingImage->GetSurfaceWidth(), Scale.X, LoadingImage->GetSurfaceHeight(), Scale.Y);
					CleanupBrushList.Add(FDeferredCleanupSlateBrush::CreateBrush(LoadingImage, FVector2D(Width, Height)));
				}
			}
		
			// Create Image slate widget
			LoadingIcon = SNew(SImage)
				.Image(CleanupBrushList[ImageIndex]->GetSlateBrush());

			// Update play animation interval
			Interval = Settings.ImageSequenceSettings.Interval;
		}
		else
		{
			// If there is no image in the array then create a spacer instead
			LoadingIcon = SNew(SSpacer).Size(FVector2D::ZeroVector);
		}

	}
	else if (Settings.LoadingIconType == ELoadingIconType::LIT_CircularThrobber)
	{
		// Loading Widget is SCircularThrobber
		LoadingIcon = SNew(SCircularThrobber)
			.NumPieces(Settings.CircularThrobberSettings.NumberOfPieces)
			.Period(Settings.CircularThrobberSettings.Period)
			.Radius(Settings.CircularThrobberSettings.Radius)
			.PieceImage(&Settings.CircularThrobberSettings.Image);
	}
	else
	{
		// Loading Widget is SThrobber
		LoadingIcon = SNew(SThrobber)
			.NumPieces(Settings.ThrobberSettings.NumberOfPieces)
			.Animate(GetThrobberAnimation(Settings.ThrobberSettings))
			.PieceImage(&Settings.ThrobberSettings.Image);
	}

	// Set Loading Icon render transform
	LoadingIcon.Get().SetRenderTransform(FSlateRenderTransform(FScale2D(Settings.TransformScale), Settings.TransformTranslation));
	LoadingIcon.Get().SetRenderTransformPivot(Settings.TransformPivot);

	// Hide loading widget when level loading is done if bHideLoadingWidgetWhenCompletes is true
	// And Hide it until movies are finish when bHideUntilMoviesFinishPlaying is true
	if (Settings.bHideLoadingWidgetWhenCompletes || Settings.bHideUntilMoviesFinishPlaying)
	{		
		SetVisibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateRaw(this, &SLoadingWidget::GetLoadingWidgetVisibility)));
	}
}

EVisibility SLoadingWidget::GetLoadingWidgetVisibility() const
{
	if (!WidgetSettings) return EVisibility::Visible;

	if (WidgetSettings->bHideUntilMoviesFinishPlaying && GetMoviePlayer()->IsMovieCurrentlyPlaying())
	{
		return EVisibility::Hidden;
	}

	if (WidgetSettings->bHideLoadingWidgetWhenCompletes && GetMoviePlayer()->IsLoadingFinished())
	{
		return EVisibility::Hidden;
	}
	return EVisibility::Visible;
}