#include "SImGuiOverlay.h"

#include <Framework/Application/SlateApplication.h>

#include "ImGuiContext.h"

FImGuiDrawList::FImGuiDrawList(ImDrawList* Source)
{
	VtxBuffer.swap(Source->VtxBuffer);
	IdxBuffer.swap(Source->IdxBuffer);
	CmdBuffer.swap(Source->CmdBuffer);
	Flags = Source->Flags;
}

FImGuiDrawData::FImGuiDrawData(const ImDrawData* Source)
{
	bValid = Source->Valid;

	TotalIdxCount = Source->TotalIdxCount;
	TotalVtxCount = Source->TotalVtxCount;

	DrawLists.SetNumUninitialized(Source->CmdListsCount);
	ConstructItems<FImGuiDrawList>(DrawLists.GetData(), Source->CmdLists.Data, Source->CmdListsCount);

	DisplayPos = Source->DisplayPos;
	DisplaySize = Source->DisplaySize;
	FrameBufferScale = Source->FramebufferScale;
}

class FImGuiInputProcessor : public IInputProcessor
{
public:
	explicit FImGuiInputProcessor(SImGuiOverlay* InOwner)
	{
		Owner = InOwner;

		FSlateApplication::Get().OnApplicationActivationStateChanged().AddRaw(this, &FImGuiInputProcessor::OnApplicationActivationChanged);
	}

	virtual ~FImGuiInputProcessor() override
	{
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().OnApplicationActivationStateChanged().RemoveAll(this);
		}
	}

	void OnApplicationActivationChanged(bool bIsActive) const
	{
		ImGui::FScopedContext ScopedContext(Owner->GetContext());

		ImGuiIO& IO = ImGui::GetIO();

		IO.AddFocusEvent(bIsActive);
	}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> SlateCursor) override
	{
		ImGui::FScopedContext ScopedContext(Owner->GetContext());

		ImGuiIO& IO = ImGui::GetIO();

		const bool bHasGamepad = (IO.BackendFlags & ImGuiBackendFlags_HasGamepad);
		if (bHasGamepad != SlateApp.IsGamepadAttached())
		{
			IO.BackendFlags ^= ImGuiBackendFlags_HasGamepad;
		}

		if (IO.WantSetMousePos)
		{
			SlateApp.SetCursorPos(IO.MousePos);
		}

#if 0
		// #TODO(Ves): Sometimes inconsistent, something else is changing the cursor later in the frame?
		if (IO.WantCaptureMouse && !(IO.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange))
		{
			const ImGuiMouseCursor CursorType = ImGui::GetMouseCursor();

			if (IO.MouseDrawCursor || CursorType == ImGuiMouseCursor_None)
			{
				SlateCursor->SetType(EMouseCursor::None);
			}
			else if (CursorType == ImGuiMouseCursor_Arrow)
			{
				SlateCursor->SetType(EMouseCursor::Default);
			}
			else if (CursorType == ImGuiMouseCursor_TextInput)
			{
				SlateCursor->SetType(EMouseCursor::TextEditBeam);
			}
			else if (CursorType == ImGuiMouseCursor_ResizeAll)
			{
				SlateCursor->SetType(EMouseCursor::CardinalCross);
			}
			else if (CursorType == ImGuiMouseCursor_ResizeNS)
			{
				SlateCursor->SetType(EMouseCursor::ResizeUpDown);
			}
			else if (CursorType == ImGuiMouseCursor_ResizeEW)
			{
				SlateCursor->SetType(EMouseCursor::ResizeLeftRight);
			}
			else if (CursorType == ImGuiMouseCursor_ResizeNESW)
			{
				SlateCursor->SetType(EMouseCursor::ResizeSouthWest);
			}
			else if (CursorType == ImGuiMouseCursor_ResizeNWSE)
			{
				SlateCursor->SetType(EMouseCursor::ResizeSouthEast);
			}
			else if (CursorType == ImGuiMouseCursor_Hand)
			{
				SlateCursor->SetType(EMouseCursor::Hand);
			}
			else if (CursorType == ImGuiMouseCursor_NotAllowed)
			{
				SlateCursor->SetType(EMouseCursor::SlashedCircle);
			}
		}
#endif

		if (IO.WantTextInput && !Owner->HasKeyboardFocus())
		{
			// No HandleKeyCharEvent so punt focus to the widget for it to receive OnKeyChar events
			SlateApp.SetKeyboardFocus(Owner->AsShared());
		}
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& Event) override
	{
		ImGui::FScopedContext ScopedContext(Owner->GetContext());

		ImGuiIO& IO = ImGui::GetIO();

		IO.AddKeyEvent(ImGui::ConvertKey(Event.GetKey()), true);

		const FModifierKeysState& ModifierKeys = Event.GetModifierKeys();
		IO.AddKeyEvent(ImGuiMod_Ctrl, ModifierKeys.IsControlDown());
		IO.AddKeyEvent(ImGuiMod_Shift, ModifierKeys.IsShiftDown());
		IO.AddKeyEvent(ImGuiMod_Alt, ModifierKeys.IsAltDown());
		IO.AddKeyEvent(ImGuiMod_Super, ModifierKeys.IsCommandDown());

		return IO.WantCaptureKeyboard;
	}

	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& Event) override
	{
		ImGui::FScopedContext ScopedContext(Owner->GetContext());

		ImGuiIO& IO = ImGui::GetIO();

		IO.AddKeyEvent(ImGui::ConvertKey(Event.GetKey()), false);

		const FModifierKeysState& ModifierKeys = Event.GetModifierKeys();
		IO.AddKeyEvent(ImGuiMod_Ctrl, ModifierKeys.IsControlDown());
		IO.AddKeyEvent(ImGuiMod_Shift, ModifierKeys.IsShiftDown());
		IO.AddKeyEvent(ImGuiMod_Alt, ModifierKeys.IsAltDown());
		IO.AddKeyEvent(ImGuiMod_Super, ModifierKeys.IsCommandDown());

		return IO.WantCaptureKeyboard;
	}

	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& Event) override
	{
		ImGui::FScopedContext ScopedContext(Owner->GetContext());

		ImGuiIO& IO = ImGui::GetIO();

		const float Value = Event.GetAnalogValue();
		IO.AddKeyAnalogEvent(ImGui::ConvertKey(Event.GetKey()), FMath::Abs(Value) > 0.1f, Value);

		return IO.WantCaptureKeyboard;
	}

	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override
	{
		ImGui::FScopedContext ScopedContext(Owner->GetContext());

		ImGuiIO& IO = ImGui::GetIO();

		if (SlateApp.HasAnyMouseCaptor())
		{
			IO.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
			return false;
		}

		const FVector2f Position = Event.GetScreenSpacePosition();
		IO.AddMousePosEvent(Position.X, Position.Y);

		return IO.WantCaptureMouse;
	}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override
	{
		ImGui::FScopedContext ScopedContext(Owner->GetContext());

		ImGuiIO& IO = ImGui::GetIO();

		const FKey Button = Event.GetEffectingButton();
		if (Button == EKeys::LeftMouseButton)
		{
			IO.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
		}
		else if (Button == EKeys::RightMouseButton)
		{
			IO.AddMouseButtonEvent(ImGuiMouseButton_Right, true);
		}
		else if (Button == EKeys::MiddleMouseButton)
		{
			IO.AddMouseButtonEvent(ImGuiMouseButton_Middle, true);
		}

		return IO.WantCaptureMouse;
	}

	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override
	{
		ImGui::FScopedContext ScopedContext(Owner->GetContext());

		ImGuiIO& IO = ImGui::GetIO();

		const FKey Button = Event.GetEffectingButton();
		if (Button == EKeys::LeftMouseButton)
		{
			IO.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
		}
		else if (Button == EKeys::RightMouseButton)
		{
			IO.AddMouseButtonEvent(ImGuiMouseButton_Right, false);
		}
		else if (Button == EKeys::MiddleMouseButton)
		{
			IO.AddMouseButtonEvent(ImGuiMouseButton_Middle, false);
		}

		return false;
	}

	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override
	{
		// Treat as mouse down, ImGui handles double click internally
		return HandleMouseButtonDownEvent(SlateApp, Event);
	}

	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& Event, const FPointerEvent* GestureEvent) override
	{
		ImGui::FScopedContext ScopedContext(Owner->GetContext());

		ImGuiIO& IO = ImGui::GetIO();

		IO.AddMouseWheelEvent(0.0f, Event.GetWheelDelta());

		return IO.WantCaptureMouse;
	}

private:
	SImGuiOverlay* Owner = nullptr;
};

void SImGuiOverlay::Construct(const FArguments& Args)
{
	UsedCommands.Reserve(300);
	SetVisibility(EVisibility::HitTestInvisible);

	Context = Args._Context.IsValid() ? Args._Context : FImGuiContext::Create();
	if (Args._HandleInput)
	{
		InputProcessor = MakeShared<FImGuiInputProcessor>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor.ToSharedRef(), 0);
	}
}

SImGuiOverlay::~SImGuiOverlay()
{
	if (FSlateApplication::IsInitialized() && InputProcessor.IsValid())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	}
}

int32 SImGuiOverlay::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (!DrawData.bValid)
	{
		return LayerId;
	}

	const FSlateRenderTransform Transform(AllottedGeometry.GetAccumulatedRenderTransform().GetTranslation() - FVector2d(DrawData.DisplayPos));

	FSlateBrush TextureBrush;
	for (const FImGuiDrawList& DrawList : DrawData.DrawLists)
	{
		TArray<FSlateVertex> Vertices;
		Vertices.SetNumUninitialized(DrawList.VtxBuffer.Size);
		for (int32 BufferIdx = 0; BufferIdx < Vertices.Num(); ++BufferIdx)
		{
			const ImDrawVert& Vtx = DrawList.VtxBuffer.Data[BufferIdx];
			// Instead of calling FSlateVertex::Make() which creates a new vertex which has to be copied,
			// write all data directly, which saves some time for large arrays
			FSlateVertex& Target = Vertices[BufferIdx];
			Target.TexCoords[0] = Vtx.uv.x;
			Target.TexCoords[1] = Vtx.uv.y;
			Target.TexCoords[2] = 1;
			Target.TexCoords[3] = 1;
			Target.Position = TransformPoint(Transform, FVector2f(Vtx.pos));
			// Ideally, make ImGui::ConvertColor inline instead
			Target.Color = FColor((Vtx.col >> IM_COL32_R_SHIFT) & 0xFF,(Vtx.col >> IM_COL32_G_SHIFT) & 0xFF,
				(Vtx.col >> IM_COL32_B_SHIFT) & 0xFF,(Vtx.col >> IM_COL32_A_SHIFT) & 0xFF);
		}

		TArray<SlateIndex> Indices;
		Indices.SetNumUninitialized(DrawList.IdxBuffer.Size);
		for (int32 BufferIdx = 0; BufferIdx < Indices.Num(); ++BufferIdx)
		{
			Indices[BufferIdx] = DrawList.IdxBuffer.Data[BufferIdx];
		}

		UsedCommands.Reset();		
		UsedCommands.AddZeroed(DrawList.CmdBuffer.Size);

		for (int i = 0; i < DrawList.CmdBuffer.Size; ++i)
		{
			if (UsedCommands[i])
			{
				continue;
			}
			
			const ImDrawCmd& DrawCmd = DrawList.CmdBuffer[i];
			TArray VerticesSlice(Vertices.GetData() + DrawCmd.VtxOffset, Vertices.Num() - DrawCmd.VtxOffset);
			TArray<SlateIndex> IndicesSlice;
			IndicesSlice.Reserve(Indices.Num());
			IndicesSlice.Append(Indices.GetData() + DrawCmd.IdxOffset, DrawCmd.ElemCount);

			// Now look ahead if there are other commands in the queue with the same parameters and add their indices to this draw call
			for (int k = i + 1; k < DrawList.CmdBuffer.Size; ++k)
			{
				if (UsedCommands[k])
				{
					continue;
				}
			
				const ImDrawCmd& OtherCmd = DrawList.CmdBuffer[k];
				if (DrawCmd.TextureId == OtherCmd.TextureId &&
					DrawCmd.UserCallback == OtherCmd.UserCallback &&
					DrawCmd.VtxOffset == OtherCmd.VtxOffset &&
					DrawCmd.ClipRect.x == OtherCmd.ClipRect.x &&
					DrawCmd.ClipRect.y == OtherCmd.ClipRect.y &&
					DrawCmd.ClipRect.z == OtherCmd.ClipRect.z &&
					DrawCmd.ClipRect.w == OtherCmd.ClipRect.w)
				{
					IndicesSlice.Append(Indices.GetData() + OtherCmd.IdxOffset, OtherCmd.ElemCount);
					UsedCommands[k] = true;
				}
			}

#if WITH_ENGINE
			UTexture* Texture = DrawCmd.GetTexID();
			if (TextureBrush.GetResourceObject() != Texture)
			{
				TextureBrush.SetResourceObject(Texture);
				if (IsValid(Texture))
				{
					TextureBrush.ImageSize.X = Texture->GetSurfaceWidth();
					TextureBrush.ImageSize.Y = Texture->GetSurfaceHeight();
					TextureBrush.ImageType = ESlateBrushImageType::FullColor;
					TextureBrush.DrawAs = ESlateBrushDrawType::Image;
				}
				else
				{
					TextureBrush.ImageSize.X = 0;
					TextureBrush.ImageSize.Y = 0;
					TextureBrush.ImageType = ESlateBrushImageType::NoImage;
					TextureBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
				}
			}
#else
			FSlateBrush* Texture = DrawCmd.GetTexID();
			if (Texture)
			{
				TextureBrush = *Texture;
			}
			else
			{
				TextureBrush.ImageSize.X = 0;
				TextureBrush.ImageSize.Y = 0;
				TextureBrush.ImageType = ESlateBrushImageType::NoImage;
				TextureBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
			}
#endif

			FSlateRect ClipRect(DrawCmd.ClipRect.x, DrawCmd.ClipRect.y, DrawCmd.ClipRect.z, DrawCmd.ClipRect.w);
			ClipRect = TransformRect(Transform, ClipRect);

			OutDrawElements.PushClip(FSlateClippingZone(ClipRect));
			// There's an unnecessary copy of the vertex and index array happening here,
			// which can be avoided by pulling in https://github.com/EpicGames/UnrealEngine/pull/12023 and moving those arrays instead
			FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId, TextureBrush.GetRenderingResource(), VerticesSlice, IndicesSlice, nullptr, 0, 0);
			OutDrawElements.PopClip();
		}
	}

	return LayerId;
}

FVector2D SImGuiOverlay::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D::ZeroVector;
}

bool SImGuiOverlay::SupportsKeyboardFocus() const
{
	return true;
}

FReply SImGuiOverlay::OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& Event)
{
	ImGui::FScopedContext ScopedContext(Context);

	ImGuiIO& IO = ImGui::GetIO();

	IO.AddInputCharacter(CharCast<ANSICHAR>(Event.GetCharacter()));

	return IO.WantTextInput ? FReply::Handled() : FReply::Unhandled();
}

TSharedPtr<FImGuiContext> SImGuiOverlay::GetContext() const
{
	return Context;
}

void SImGuiOverlay::SetDrawData(const ImDrawData* InDrawData)
{
	DrawData = FImGuiDrawData(InDrawData);
}
