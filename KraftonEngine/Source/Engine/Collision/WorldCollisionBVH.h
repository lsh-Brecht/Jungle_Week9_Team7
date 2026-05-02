#pragma once

#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"

class AActor;
class UPrimitiveComponent;

class FWorldCollisionBVH
{
public:
	static constexpr int32 INDEX_NONE = -1;

	struct FOverlapCandidatePair
	{
		UPrimitiveComponent* A = nullptr;
		UPrimitiveComponent* B = nullptr;
	};

	void MarkDirty();
	void Reset();
	void BuildNow(const TArray<AActor*>& Actors);
	void EnsureBuilt(const TArray<AActor*>& Actors);

	bool InsertObject(UPrimitiveComponent* Primitive);
	bool RemoveObject(UPrimitiveComponent* Primitive);
	bool UpdateObject(UPrimitiveComponent* Primitive);
	bool ContainsObject(UPrimitiveComponent* Primitive) const;

	void QueryAABB(const FBoundingBox& QueryBounds, TArray<UPrimitiveComponent*>& OutCandidates) const;
	void GeneratePotentialPairs(TArray<FOverlapCandidatePair>& OutPairs) const;

private:
	struct FNode
	{
		FBoundingBox Bounds;
		FBoundingBox FatBounds;
		int32 Parent = INDEX_NONE;
		int32 Left = INDEX_NONE;
		int32 Right = INDEX_NONE;
		int32 Depth = -1;
		UPrimitiveComponent* Primitive = nullptr;

		bool IsLeaf() const { return Left == INDEX_NONE && Right == INDEX_NONE; }
	};

	struct FRotationCandidate
	{
		bool bValid = false;
		bool bRotateLeftChild = false;
		bool bUseFirstGrandChild = false;
		int32 NodeIndex = INDEX_NONE;
		float OldCost = 0.0f;
		float NewCost = 0.0f;
	};

	struct FNodePair
	{
		int32 A = INDEX_NONE;
		int32 B = INDEX_NONE;
	};

	int32 AllocateNode();
	void ReleaseNode(int32 NodeIndex);
	int32 CreateLeafNode(UPrimitiveComponent* Primitive, const FBoundingBox& Bounds);
	int32 FindBestSibling(const FBoundingBox& NewBounds) const;
	int32 FindLeafNodeIndexByObject(UPrimitiveComponent* Primitive) const;

	void InsertLeafNode(int32 LeafNodeIndex);
	void RemoveLeafNode(int32 LeafNodeIndex);
	void RefitNode(int32 NodeIndex);
	void RefitUpwards(int32 NodeIndex);
	void RefitUpwardsAfterStructuralChange(int32 NodeIndex);

	void UpdateDepthsFromNode(int32 NodeIndex, int32 Depth);
	void OptimizeAlongPath(int32 StartNodeIndex);
	FRotationCandidate EvaluateRotateWithLeftChild(int32 NodeIndex) const;
	FRotationCandidate EvaluateRotateWithRightChild(int32 NodeIndex) const;
	bool TryRotateNodeBest(int32 NodeIndex);
	bool ApplyRotation(const FRotationCandidate& Candidate);

	static bool IsCollisionPrimitive(UPrimitiveComponent* Primitive);
	static FBoundingBox UnionBounds(const FBoundingBox& A, const FBoundingBox& B);
	static FBoundingBox MakeFatBounds(const FBoundingBox& Bounds);
	static float GetBoundsSurfaceArea(const FBoundingBox& Bounds);
	static bool IsValidNodeIndex(int32 NodeIndex, int32 NodeCount);

	void ValidateBVH() const;

	bool bDirty = true;
	TArray<FNode> Nodes;
	TArray<int32> FreeNodeIndices;
	TMap<UPrimitiveComponent*, int32> ObjectToLeafNode;
	int32 RootNodeIndex = INDEX_NONE;
	TArray<int32> PathToRootScratch;
};
