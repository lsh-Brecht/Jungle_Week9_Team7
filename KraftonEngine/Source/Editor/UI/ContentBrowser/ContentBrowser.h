#pragma once
#include "Editor/UI/EditorWidget.h"
#include "ContentItem.h"
#include <d3d11.h>
#include <memory>
#include "ContentBrowserContext.h"
#include "ContentBrowserElement.h"
#include <wrl/client.h>
#include <functional>

class FEditorContentBrowserWidget final : public FEditorWidget
{
	struct FDirNode
	{
		FContentItem Self;
		TArray<FDirNode> Children;
	};

public:
	void Initialize(UEditorEngine* InEditor, ID3D11Device* InDevice);
	void Render(float DeltaTime) override;
	void Refresh();
	void SaveToSettings() const;
	void SetIconSize(float Size);
	float GetIconSize() const { return BrowserContext.ContentSize.x; }
	//void SetAssetDoubleClickHandler(std::function<bool(const FContentItem&)> Handler);

private:
	void LoadFromSettings();
	void RefreshContent();
	void DrawDirNode(FDirNode InNode);
	void DrawContents();

	TArray<FContentItem> ReadDirectory(std::wstring Path);
	FDirNode BuildDirectoryTree(const std::filesystem::path& DirPath);

private:
	ContentBrowserContext BrowserContext;

	FDirNode RootNode;
	TArray<std::shared_ptr<ContentBrowserElement>> CachedBrowserElements;
	TMap<FString, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> ICons;
};
