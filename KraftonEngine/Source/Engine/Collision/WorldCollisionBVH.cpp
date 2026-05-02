#include "Collision/WorldCollisionBVH.h"

#include "Component/PrimitiveComponent.h"
#include "Component/Collision/CollisionTypes.h"
#include "GameFramework/AActor.h"

#include <algorithm>
#include <cassert>

namespace
{
	constexpr float WorldCollisionBVHFatBoundsScale = 0.15f;
	constexpr float WorldCollisionBVHFatBoundsMinPadding = 1.0f;
}

void FWorldCollisionBVH::MarkDirty()
{
	bDirty = true;
}

void FWorldCollisionBVH::Reset()
{
	Nodes.clear();
	FreeNodeIndices.clear();
	ObjectToLeafNode.clear();
	PathToRootScratch.clear();
	RootNodeIndex = INDEX_NONE;
	bDirty = false;
}

void FWorldCollisionBVH::BuildNow(const TArray<AActor*>& Actors)
{
	Reset();

	for (AActor* Actor : Actors)
	{
		if (!Actor)
		{
			continue;
		}

		for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
		{
			InsertObject(Primitive);
		}
	}

	bDirty = false;
	ValidateBVH();
}

void FWorldCollisionBVH::EnsureBuilt(const TArray<AActor*>& Actors)
{
	if (bDirty)
	{
		BuildNow(Actors);
	}
}

bool FWorldCollisionBVH::InsertObject(UPrimitiveComponent* Primitive)
{
	if (!IsCollisionPrimitive(Primitive) || ContainsObject(Primitive))
	{
		return false;
	}

	const FBoundingBox Bounds = Primitive->GetWorldAABB();
	if (!Bounds.IsValid())
	{
		return false;
	}

	const int32 LeafNodeIndex = CreateLeafNode(Primitive, Bounds);
	if (LeafNodeIndex == INDEX_NONE)
	{
		return false;
	}

	InsertLeafNode(LeafNodeIndex);
	ObjectToLeafNode[Primitive] = LeafNodeIndex;
	bDirty = false;
	ValidateBVH();
	return true;
}

bool FWorldCollisionBVH::RemoveObject(UPrimitiveComponent* Primitive)
{
	const int32 LeafNodeIndex = FindLeafNodeIndexByObject(Primitive);
	if (LeafNodeIndex == INDEX_NONE)
	{
		return false;
	}

	RemoveLeafNode(LeafNodeIndex);
	ObjectToLeafNode.erase(Primitive);
	bDirty = false;
	ValidateBVH();
	return true;
}

bool FWorldCollisionBVH::UpdateObject(UPrimitiveComponent* Primitive)
{
	const int32 LeafNodeIndex = FindLeafNodeIndexByObject(Primitive);
	if (LeafNodeIndex == INDEX_NONE)
	{
		return InsertObject(Primitive);
	}

	if (!IsCollisionPrimitive(Primitive))
	{
		return RemoveObject(Primitive);
	}

	const FBoundingBox NewBounds = Primitive->GetWorldAABB();
	if (!NewBounds.IsValid())
	{
		return RemoveObject(Primitive);
	}

	FNode& Leaf = Nodes[LeafNodeIndex];
	if (!Leaf.FatBounds.IsContains(NewBounds))
	{
		RemoveObject(Primitive);
		return InsertObject(Primitive);
	}

	Leaf.Bounds = NewBounds;
	Leaf.Primitive = Primitive;
	RefitUpwards(LeafNodeIndex);
	OptimizeAlongPath(LeafNodeIndex);
	bDirty = false;
	ValidateBVH();
	return true;
}

bool FWorldCollisionBVH::ContainsObject(UPrimitiveComponent* Primitive) const
{
	return FindLeafNodeIndexByObject(Primitive) != INDEX_NONE;
}

void FWorldCollisionBVH::QueryAABB(const FBoundingBox& QueryBounds, TArray<UPrimitiveComponent*>& OutCandidates) const
{
	if (!QueryBounds.IsValid() || RootNodeIndex == INDEX_NONE ||
		!IsValidNodeIndex(RootNodeIndex, static_cast<int32>(Nodes.size())))
	{
		return;
	}

	TArray<int32> Stack;
	Stack.reserve(Nodes.size());
	Stack.push_back(RootNodeIndex);

	while (!Stack.empty())
	{
		const int32 NodeIndex = Stack.back();
		Stack.pop_back();
		if (!IsValidNodeIndex(NodeIndex, static_cast<int32>(Nodes.size())))
		{
			continue;
		}

		const FNode& Node = Nodes[NodeIndex];
		if (!Node.Bounds.IsIntersected(QueryBounds))
		{
			continue;
		}

		if (Node.IsLeaf())
		{
			if (IsCollisionPrimitive(Node.Primitive))
			{
				OutCandidates.push_back(Node.Primitive);
			}
			continue;
		}

		Stack.push_back(Node.Right);
		Stack.push_back(Node.Left);
	}
}

void FWorldCollisionBVH::GeneratePotentialPairs(TArray<FOverlapCandidatePair>& OutPairs) const
{
	if (RootNodeIndex == INDEX_NONE || !IsValidNodeIndex(RootNodeIndex, static_cast<int32>(Nodes.size())))
	{
		return;
	}

	TArray<FNodePair> Stack;
	Stack.reserve(Nodes.size());
	Stack.push_back({ RootNodeIndex, RootNodeIndex });

	while (!Stack.empty())
	{
		const FNodePair Pair = Stack.back();
		Stack.pop_back();

		if (!IsValidNodeIndex(Pair.A, static_cast<int32>(Nodes.size())) ||
			!IsValidNodeIndex(Pair.B, static_cast<int32>(Nodes.size())))
		{
			continue;
		}

		const FNode& A = Nodes[Pair.A];
		const FNode& B = Nodes[Pair.B];
		if (Pair.A == Pair.B)
		{
			if (!A.IsLeaf())
			{
				Stack.push_back({ A.Left, A.Left });
				Stack.push_back({ A.Left, A.Right });
				Stack.push_back({ A.Right, A.Right });
			}
			continue;
		}

		if (!A.Bounds.IsIntersected(B.Bounds))
		{
			continue;
		}

		if (A.IsLeaf() && B.IsLeaf())
		{
			if (IsCollisionPrimitive(A.Primitive) && IsCollisionPrimitive(B.Primitive))
			{
				OutPairs.push_back({ A.Primitive, B.Primitive });
			}
			continue;
		}

		if (A.IsLeaf())
		{
			Stack.push_back({ Pair.A, B.Left });
			Stack.push_back({ Pair.A, B.Right });
		}
		else if (B.IsLeaf())
		{
			Stack.push_back({ A.Left, Pair.B });
			Stack.push_back({ A.Right, Pair.B });
		}
		else
		{
			Stack.push_back({ A.Left, B.Left });
			Stack.push_back({ A.Left, B.Right });
			Stack.push_back({ A.Right, B.Left });
			Stack.push_back({ A.Right, B.Right });
		}
	}
}

int32 FWorldCollisionBVH::AllocateNode()
{
	if (!FreeNodeIndices.empty())
	{
		const int32 NodeIndex = FreeNodeIndices.back();
		FreeNodeIndices.pop_back();
		Nodes[NodeIndex] = FNode{};
		return NodeIndex;
	}

	const int32 NodeIndex = static_cast<int32>(Nodes.size());
	Nodes.emplace_back();
	return NodeIndex;
}

void FWorldCollisionBVH::ReleaseNode(int32 NodeIndex)
{
	if (!IsValidNodeIndex(NodeIndex, static_cast<int32>(Nodes.size())))
	{
		return;
	}

	Nodes[NodeIndex] = FNode{};
	FreeNodeIndices.push_back(NodeIndex);
}

int32 FWorldCollisionBVH::CreateLeafNode(UPrimitiveComponent* Primitive, const FBoundingBox& Bounds)
{
	if (!IsCollisionPrimitive(Primitive) || !Bounds.IsValid())
	{
		return INDEX_NONE;
	}

	const int32 LeafNodeIndex = AllocateNode();
	FNode& Leaf = Nodes[LeafNodeIndex];
	Leaf.Bounds = Bounds;
	Leaf.FatBounds = MakeFatBounds(Bounds);
	Leaf.Parent = INDEX_NONE;
	Leaf.Left = INDEX_NONE;
	Leaf.Right = INDEX_NONE;
	Leaf.Depth = 0;
	Leaf.Primitive = Primitive;
	return LeafNodeIndex;
}

int32 FWorldCollisionBVH::FindBestSibling(const FBoundingBox& NewBounds) const
{
	if (RootNodeIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	int32 Current = RootNodeIndex;
	while (IsValidNodeIndex(Current, static_cast<int32>(Nodes.size())) && !Nodes[Current].IsLeaf())
	{
		const FNode& Node = Nodes[Current];
		if (!IsValidNodeIndex(Node.Left, static_cast<int32>(Nodes.size())) ||
			!IsValidNodeIndex(Node.Right, static_cast<int32>(Nodes.size())))
		{
			return Current;
		}

		const FNode& Left = Nodes[Node.Left];
		const FNode& Right = Nodes[Node.Right];
		const float NodeArea = GetBoundsSurfaceArea(Node.Bounds);
		const FBoundingBox CombinedBounds = UnionBounds(Node.Bounds, NewBounds);
		const float CombinedArea = GetBoundsSurfaceArea(CombinedBounds);
		const float CostHere = 2.0f * CombinedArea;
		const float InheritanceCost = 2.0f * (CombinedArea - NodeArea);

		auto ComputeDescendCost = [&](const FNode& Child) -> float
		{
			const FBoundingBox CombinedChildBounds = UnionBounds(Child.Bounds, NewBounds);
			const float CombinedChildArea = GetBoundsSurfaceArea(CombinedChildBounds);
			if (Child.IsLeaf())
			{
				return CombinedChildArea + InheritanceCost;
			}
			return (CombinedChildArea - GetBoundsSurfaceArea(Child.Bounds)) + InheritanceCost;
		};

		const float CostLeft = ComputeDescendCost(Left);
		const float CostRight = ComputeDescendCost(Right);
		if (CostHere <= CostLeft && CostHere <= CostRight)
		{
			break;
		}

		Current = (CostLeft <= CostRight) ? Node.Left : Node.Right;
	}

	return Current;
}

int32 FWorldCollisionBVH::FindLeafNodeIndexByObject(UPrimitiveComponent* Primitive) const
{
	if (!Primitive)
	{
		return INDEX_NONE;
	}

	const auto It = ObjectToLeafNode.find(Primitive);
	if (It == ObjectToLeafNode.end())
	{
		return INDEX_NONE;
	}

	const int32 LeafNodeIndex = It->second;
	if (!IsValidNodeIndex(LeafNodeIndex, static_cast<int32>(Nodes.size())))
	{
		return INDEX_NONE;
	}

	const FNode& Leaf = Nodes[LeafNodeIndex];
	return Leaf.IsLeaf() && Leaf.Primitive == Primitive ? LeafNodeIndex : INDEX_NONE;
}

void FWorldCollisionBVH::InsertLeafNode(int32 LeafNodeIndex)
{
	if (!IsValidNodeIndex(LeafNodeIndex, static_cast<int32>(Nodes.size())))
	{
		return;
	}

	if (RootNodeIndex == INDEX_NONE)
	{
		RootNodeIndex = LeafNodeIndex;
		Nodes[LeafNodeIndex].Parent = INDEX_NONE;
		Nodes[LeafNodeIndex].Depth = 0;
		return;
	}

	const int32 SiblingIndex = FindBestSibling(Nodes[LeafNodeIndex].FatBounds);
	if (SiblingIndex == INDEX_NONE)
	{
		return;
	}

	const int32 OldParentIndex = Nodes[SiblingIndex].Parent;
	const int32 NewParentIndex = AllocateNode();

	FNode& NewParent = Nodes[NewParentIndex];
	NewParent.Parent = OldParentIndex;
	NewParent.Left = SiblingIndex;
	NewParent.Right = LeafNodeIndex;
	NewParent.Bounds = UnionBounds(Nodes[SiblingIndex].Bounds, Nodes[LeafNodeIndex].Bounds);
	NewParent.FatBounds = UnionBounds(Nodes[SiblingIndex].FatBounds, Nodes[LeafNodeIndex].FatBounds);
	NewParent.Depth = (OldParentIndex == INDEX_NONE) ? 0 : Nodes[OldParentIndex].Depth + 1;

	if (OldParentIndex == INDEX_NONE)
	{
		RootNodeIndex = NewParentIndex;
	}
	else if (Nodes[OldParentIndex].Left == SiblingIndex)
	{
		Nodes[OldParentIndex].Left = NewParentIndex;
	}
	else
	{
		Nodes[OldParentIndex].Right = NewParentIndex;
	}

	Nodes[SiblingIndex].Parent = NewParentIndex;
	Nodes[LeafNodeIndex].Parent = NewParentIndex;
	UpdateDepthsFromNode(NewParentIndex, NewParent.Depth);
	RefitUpwardsAfterStructuralChange(NewParentIndex);
	OptimizeAlongPath(NewParentIndex);
}

void FWorldCollisionBVH::RemoveLeafNode(int32 LeafNodeIndex)
{
	if (!IsValidNodeIndex(LeafNodeIndex, static_cast<int32>(Nodes.size())))
	{
		return;
	}

	const int32 ParentIndex = Nodes[LeafNodeIndex].Parent;
	if (ParentIndex == INDEX_NONE)
	{
		RootNodeIndex = INDEX_NONE;
		ReleaseNode(LeafNodeIndex);
		return;
	}

	const int32 GrandParentIndex = Nodes[ParentIndex].Parent;
	const int32 SiblingIndex = (Nodes[ParentIndex].Left == LeafNodeIndex) ? Nodes[ParentIndex].Right : Nodes[ParentIndex].Left;

	if (GrandParentIndex == INDEX_NONE)
	{
		RootNodeIndex = SiblingIndex;
		Nodes[SiblingIndex].Parent = INDEX_NONE;
		UpdateDepthsFromNode(SiblingIndex, 0);
	}
	else
	{
		if (Nodes[GrandParentIndex].Left == ParentIndex)
		{
			Nodes[GrandParentIndex].Left = SiblingIndex;
		}
		else
		{
			Nodes[GrandParentIndex].Right = SiblingIndex;
		}

		Nodes[SiblingIndex].Parent = GrandParentIndex;
		UpdateDepthsFromNode(SiblingIndex, Nodes[GrandParentIndex].Depth + 1);
	}

	const int32 RefitStart = Nodes[SiblingIndex].Parent;
	ReleaseNode(LeafNodeIndex);
	ReleaseNode(ParentIndex);

	if (RefitStart != INDEX_NONE)
	{
		RefitUpwardsAfterStructuralChange(RefitStart);
		OptimizeAlongPath(RefitStart);
	}
}

void FWorldCollisionBVH::RefitNode(int32 NodeIndex)
{
	if (!IsValidNodeIndex(NodeIndex, static_cast<int32>(Nodes.size())))
	{
		return;
	}

	FNode& Node = Nodes[NodeIndex];
	if (Node.IsLeaf())
	{
		if (Node.Primitive)
		{
			Node.Bounds = Node.Primitive->GetWorldAABB();
		}
		return;
	}

	Node.Bounds = UnionBounds(Nodes[Node.Left].Bounds, Nodes[Node.Right].Bounds);
	Node.FatBounds = UnionBounds(Nodes[Node.Left].FatBounds, Nodes[Node.Right].FatBounds);
	Node.Primitive = nullptr;
}

void FWorldCollisionBVH::RefitUpwards(int32 NodeIndex)
{
	int32 Current = NodeIndex;
	while (Current != INDEX_NONE)
	{
		RefitNode(Current);
		Current = Nodes[Current].Parent;
	}
}

void FWorldCollisionBVH::RefitUpwardsAfterStructuralChange(int32 NodeIndex)
{
	int32 Current = NodeIndex;
	while (Current != INDEX_NONE)
	{
		RefitNode(Current);
		Current = Nodes[Current].Parent;
	}
}

void FWorldCollisionBVH::UpdateDepthsFromNode(int32 NodeIndex, int32 Depth)
{
	if (!IsValidNodeIndex(NodeIndex, static_cast<int32>(Nodes.size())))
	{
		return;
	}

	Nodes[NodeIndex].Depth = Depth;
	if (Nodes[NodeIndex].Left != INDEX_NONE)
	{
		UpdateDepthsFromNode(Nodes[NodeIndex].Left, Depth + 1);
	}
	if (Nodes[NodeIndex].Right != INDEX_NONE)
	{
		UpdateDepthsFromNode(Nodes[NodeIndex].Right, Depth + 1);
	}
}

void FWorldCollisionBVH::OptimizeAlongPath(int32 StartNodeIndex)
{
	PathToRootScratch.clear();
	PathToRootScratch.reserve(32);

	int32 Current = StartNodeIndex;
	while (Current != INDEX_NONE)
	{
		PathToRootScratch.push_back(Current);
		Current = Nodes[Current].Parent;
	}

	for (int32 NodeIndex : PathToRootScratch)
	{
		while (TryRotateNodeBest(NodeIndex))
		{
		}
	}
}

FWorldCollisionBVH::FRotationCandidate FWorldCollisionBVH::EvaluateRotateWithLeftChild(int32 NodeIndex) const
{
	FRotationCandidate Best{};
	const FNode& Node = Nodes[NodeIndex];
	if (Node.IsLeaf())
	{
		return Best;
	}

	const int32 AIndex = Node.Left;
	const int32 BIndex = Node.Right;
	if (!IsValidNodeIndex(AIndex, static_cast<int32>(Nodes.size())) ||
		!IsValidNodeIndex(BIndex, static_cast<int32>(Nodes.size())) ||
		Nodes[AIndex].IsLeaf())
	{
		return Best;
	}

	const FNode& A = Nodes[AIndex];
	const float OldCost = GetBoundsSurfaceArea(Node.Bounds) + GetBoundsSurfaceArea(A.Bounds);

	auto Consider = [&](int32 GrandChildIndex, int32 OtherGrandChildIndex, bool bUseFirstGrandChild)
	{
		const FBoundingBox NewA = UnionBounds(Nodes[BIndex].Bounds, Nodes[OtherGrandChildIndex].Bounds);
		const FBoundingBox NewNode = UnionBounds(NewA, Nodes[GrandChildIndex].Bounds);
		const float NewCost = GetBoundsSurfaceArea(NewNode) + GetBoundsSurfaceArea(NewA);
		if (!Best.bValid || NewCost < Best.NewCost)
		{
			Best.bValid = true;
			Best.bRotateLeftChild = true;
			Best.bUseFirstGrandChild = bUseFirstGrandChild;
			Best.NodeIndex = NodeIndex;
			Best.OldCost = OldCost;
			Best.NewCost = NewCost;
		}
	};

	Consider(A.Left, A.Right, true);
	Consider(A.Right, A.Left, false);

	if (Best.bValid && Best.NewCost >= Best.OldCost)
	{
		Best.bValid = false;
	}

	return Best;
}

FWorldCollisionBVH::FRotationCandidate FWorldCollisionBVH::EvaluateRotateWithRightChild(int32 NodeIndex) const
{
	FRotationCandidate Best{};
	const FNode& Node = Nodes[NodeIndex];
	if (Node.IsLeaf())
	{
		return Best;
	}

	const int32 AIndex = Node.Left;
	const int32 BIndex = Node.Right;
	if (!IsValidNodeIndex(AIndex, static_cast<int32>(Nodes.size())) ||
		!IsValidNodeIndex(BIndex, static_cast<int32>(Nodes.size())) ||
		Nodes[BIndex].IsLeaf())
	{
		return Best;
	}

	const FNode& B = Nodes[BIndex];
	const float OldCost = GetBoundsSurfaceArea(Node.Bounds) + GetBoundsSurfaceArea(B.Bounds);

	auto Consider = [&](int32 GrandChildIndex, int32 OtherGrandChildIndex, bool bUseFirstGrandChild)
	{
		const FBoundingBox NewB = UnionBounds(Nodes[AIndex].Bounds, Nodes[OtherGrandChildIndex].Bounds);
		const FBoundingBox NewNode = UnionBounds(Nodes[GrandChildIndex].Bounds, NewB);
		const float NewCost = GetBoundsSurfaceArea(NewNode) + GetBoundsSurfaceArea(NewB);
		if (!Best.bValid || NewCost < Best.NewCost)
		{
			Best.bValid = true;
			Best.bRotateLeftChild = false;
			Best.bUseFirstGrandChild = bUseFirstGrandChild;
			Best.NodeIndex = NodeIndex;
			Best.OldCost = OldCost;
			Best.NewCost = NewCost;
		}
	};

	Consider(B.Left, B.Right, true);
	Consider(B.Right, B.Left, false);

	if (Best.bValid && Best.NewCost >= Best.OldCost)
	{
		Best.bValid = false;
	}

	return Best;
}

bool FWorldCollisionBVH::TryRotateNodeBest(int32 NodeIndex)
{
	if (!IsValidNodeIndex(NodeIndex, static_cast<int32>(Nodes.size())) || Nodes[NodeIndex].IsLeaf())
	{
		return false;
	}

	const FRotationCandidate LeftCandidate = EvaluateRotateWithLeftChild(NodeIndex);
	const FRotationCandidate RightCandidate = EvaluateRotateWithRightChild(NodeIndex);
	FRotationCandidate Best{};

	if (LeftCandidate.bValid)
	{
		Best = LeftCandidate;
	}
	if (RightCandidate.bValid && (!Best.bValid || RightCandidate.NewCost < Best.NewCost))
	{
		Best = RightCandidate;
	}

	return Best.bValid && ApplyRotation(Best);
}

bool FWorldCollisionBVH::ApplyRotation(const FRotationCandidate& Candidate)
{
	if (!Candidate.bValid || !IsValidNodeIndex(Candidate.NodeIndex, static_cast<int32>(Nodes.size())))
	{
		return false;
	}

	FNode& Node = Nodes[Candidate.NodeIndex];
	if (Candidate.bRotateLeftChild)
	{
		const int32 AIndex = Node.Left;
		const int32 BIndex = Node.Right;
		FNode& A = Nodes[AIndex];

		if (Candidate.bUseFirstGrandChild)
		{
			Node.Right = A.Left;
			A.Left = BIndex;
		}
		else
		{
			Node.Right = A.Right;
			A.Right = BIndex;
		}

		Nodes[Node.Right].Parent = Candidate.NodeIndex;
		Nodes[BIndex].Parent = AIndex;
		A.Parent = Candidate.NodeIndex;
		UpdateDepthsFromNode(Candidate.NodeIndex, Nodes[Candidate.NodeIndex].Depth);
		RefitUpwards(AIndex);
	}
	else
	{
		const int32 AIndex = Node.Left;
		const int32 BIndex = Node.Right;
		FNode& B = Nodes[BIndex];

		if (Candidate.bUseFirstGrandChild)
		{
			Node.Left = B.Left;
			B.Left = AIndex;
		}
		else
		{
			Node.Left = B.Right;
			B.Right = AIndex;
		}

		Nodes[Node.Left].Parent = Candidate.NodeIndex;
		Nodes[AIndex].Parent = BIndex;
		B.Parent = Candidate.NodeIndex;
		UpdateDepthsFromNode(Candidate.NodeIndex, Nodes[Candidate.NodeIndex].Depth);
		RefitUpwards(BIndex);
	}

	return true;
}

bool FWorldCollisionBVH::IsCollisionPrimitive(UPrimitiveComponent* Primitive)
{
	return Primitive && Primitive->GetCollisionShapeType() != ECollisionShapeType::None;
}

FBoundingBox FWorldCollisionBVH::UnionBounds(const FBoundingBox& A, const FBoundingBox& B)
{
	FBoundingBox Result;
	if (A.IsValid())
	{
		Result.Expand(A.Min);
		Result.Expand(A.Max);
	}
	if (B.IsValid())
	{
		Result.Expand(B.Min);
		Result.Expand(B.Max);
	}
	return Result;
}

FBoundingBox FWorldCollisionBVH::MakeFatBounds(const FBoundingBox& Bounds)
{
	const FVector Extent = Bounds.GetExtent();
	const FVector Padding(
		std::max(Extent.X * WorldCollisionBVHFatBoundsScale, WorldCollisionBVHFatBoundsMinPadding),
		std::max(Extent.Y * WorldCollisionBVHFatBoundsScale, WorldCollisionBVHFatBoundsMinPadding),
		std::max(Extent.Z * WorldCollisionBVHFatBoundsScale, WorldCollisionBVHFatBoundsMinPadding));

	return FBoundingBox(Bounds.Min - Padding, Bounds.Max + Padding);
}

float FWorldCollisionBVH::GetBoundsSurfaceArea(const FBoundingBox& Bounds)
{
	if (!Bounds.IsValid())
	{
		return 0.0f;
	}

	const FVector Extent = Bounds.GetExtent();
	const float Width = std::max(Extent.X * 2.0f, 0.0f);
	const float Height = std::max(Extent.Y * 2.0f, 0.0f);
	const float Depth = std::max(Extent.Z * 2.0f, 0.0f);
	return 2.0f * ((Width * Height) + (Width * Depth) + (Height * Depth));
}

bool FWorldCollisionBVH::IsValidNodeIndex(int32 NodeIndex, int32 NodeCount)
{
	return NodeIndex >= 0 && NodeIndex < NodeCount;
}

void FWorldCollisionBVH::ValidateBVH() const
{
#ifdef _DEBUG
	if (RootNodeIndex == INDEX_NONE)
	{
		assert(ObjectToLeafNode.empty());
		return;
	}

	assert(IsValidNodeIndex(RootNodeIndex, static_cast<int32>(Nodes.size())));
	assert(Nodes[RootNodeIndex].Parent == INDEX_NONE);

	TArray<uint8> bReachable;
	bReachable.resize(Nodes.size(), 0u);

	TArray<int32> Stack;
	Stack.reserve(Nodes.size());
	Stack.push_back(RootNodeIndex);
	bReachable[RootNodeIndex] = 1u;

	while (!Stack.empty())
	{
		const int32 NodeIndex = Stack.back();
		Stack.pop_back();
		const FNode& Node = Nodes[NodeIndex];

		if (Node.IsLeaf())
		{
			assert(IsCollisionPrimitive(Node.Primitive));
			const auto It = ObjectToLeafNode.find(Node.Primitive);
			assert(It != ObjectToLeafNode.end());
			assert(It->second == NodeIndex);
			continue;
		}

		assert(IsValidNodeIndex(Node.Left, static_cast<int32>(Nodes.size())));
		assert(IsValidNodeIndex(Node.Right, static_cast<int32>(Nodes.size())));
		assert(Node.Left != Node.Right);
		assert(Nodes[Node.Left].Parent == NodeIndex);
		assert(Nodes[Node.Right].Parent == NodeIndex);

		if (!bReachable[Node.Left])
		{
			bReachable[Node.Left] = 1u;
			Stack.push_back(Node.Left);
		}
		if (!bReachable[Node.Right])
		{
			bReachable[Node.Right] = 1u;
			Stack.push_back(Node.Right);
		}
	}

	for (const auto& Pair : ObjectToLeafNode)
	{
		assert(IsCollisionPrimitive(Pair.first));
		assert(IsValidNodeIndex(Pair.second, static_cast<int32>(Nodes.size())));
		assert(bReachable[Pair.second]);
		assert(Nodes[Pair.second].IsLeaf());
		assert(Nodes[Pair.second].Primitive == Pair.first);
	}
#endif
}
