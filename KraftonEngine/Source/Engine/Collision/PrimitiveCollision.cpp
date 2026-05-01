#include "Component/SceneComponent.h"
#include "Collision/PrimitiveCollision.h"
#include "Component/PrimitiveComponent.h"

#include "Component/Collision/CollisionTypes.h"
#include "Component/Collision/BoxComponent.h"
#include "Component/Collision/SphereComponent.h"
#include "Component/Collision/CapsuleComponent.h"
#include "Math/MathUtils.h"

#include <algorithm>
#include <cmath>

namespace
{
	float GetMaxAbs(float A, float B)
	{
		//2개 중 가장 큰 절대값을 반환합니다
		return std::max(std::abs(A), std::abs(B));
	}

	float GetMaxAbs(float A, float B, float C)
	{
		//3개 중 가장 큰 절대값을 반환합니다
		return std::max({ std::abs(A), std::abs(B), std::abs(C) });
	}

	float GetScaledSphereRadius(const USphereComponent* Sphere)
	{
		const FVector Scale = Sphere->GetWorldScale();
		return Sphere->GetSphereRadius() * GetMaxAbs(Scale.X, Scale.Y, Scale.Z);
	}

	float GetScaledCapsuleRadius(const UCapsuleComponent* Capsule)
	{
		//캡슐은 중심축이 z니까 x,y로 반지름을 구함
		const FVector Scale = Capsule->GetWorldScale();
		return Capsule->GetCapsuleRadius() * GetMaxAbs(Scale.X, Scale.Y);
	}

	bool IntersectSegmentAABB(const FVector& Start, const FVector& End, const FBoundingBox& AABB)
	{
		const FVector Dir = End - Start;
		float TMin = 0.0f;
		float TMax = 1.0f;

		const float* StartPtr = &Start.X;
		const float* DirPtr = &Dir.X;
		const float* MinPtr = &AABB.Min.X;
		const float* MaxPtr = &AABB.Max.X;

		for (int Axis = 0; Axis < 3; ++Axis)
		{
			const float D = DirPtr[Axis];

			if (std::abs(D) < 1e-8f)
			{
				if (StartPtr[Axis] < MinPtr[Axis] || StartPtr[Axis] > MaxPtr[Axis])
				{
					return false;
				}
				continue;
			}

			float InvD = 1.0f / D;
			float T1 = (MinPtr[Axis] - StartPtr[Axis]) * InvD;
			float T2 = (MaxPtr[Axis] - StartPtr[Axis]) * InvD;

			if (T1 > T2) std::swap(T1, T2);

			TMin = std::max(TMin, T1);
			TMax = std::min(TMax, T2);

			if (TMin > TMax)
			{
				return false;
			}
		}

		return true;
	}
}

bool FPrimitiveCollision::IntersectBroadPhase(const UPrimitiveComponent* A, const UPrimitiveComponent* B)
{
	//BVH?
	return false;
}

bool FPrimitiveCollision::Intersect(const UPrimitiveComponent* A, const UPrimitiveComponent* B)
{
	//일단 for문을 돌면서 BoradPhase를 미리 돌아 후보군을 추려서 함수 호출 비용을 아낄지
	//아니면 2중 for문을 돌면서 BoradPhase 내에 없으면 바로 return할 지 고민해볼 것.
	//if (!IntersectBroadPhase(A, B))
	//{
	//	return false;
	//}

	const ECollisionShapeType TypeA = A->GetCollisionShapeType();
	const ECollisionShapeType TypeB = B->GetCollisionShapeType();

	// 도형 타입이 3개이므로 순서 있는 조합은 3+9개지만,
	// 충돌 판정은 A-B와 B-A가 사실 동일하므로 실제 narrow phase 함수는 9개만 필요합니다.
	if (TypeA == ECollisionShapeType::Sphere && TypeB == ECollisionShapeType::Sphere)
	{
		return IntersectSphereSphere(static_cast<const USphereComponent*>(A), static_cast<const USphereComponent*>(B));
	}
	if (TypeA == ECollisionShapeType::Box && TypeB == ECollisionShapeType::Box)
	{
		return IntersectBoxBox(static_cast<const UBoxComponent*>(A), static_cast<const UBoxComponent*>(B));
	}
	if (TypeA == ECollisionShapeType::Capsule && TypeB == ECollisionShapeType::Capsule)
	{
		return IntersectCapsuleCapsule(static_cast<const UCapsuleComponent*>(A), static_cast<const UCapsuleComponent*>(B));
	}

	//같은 함수를 인자 순서만 바꿔 호출.
	if (TypeA == ECollisionShapeType::Sphere && TypeB == ECollisionShapeType::Box)
	{
		return IntersectSphereBox(static_cast<const USphereComponent*>(A), static_cast<const UBoxComponent*>(B));
	}
	if (TypeA == ECollisionShapeType::Box && TypeB == ECollisionShapeType::Sphere)
	{
		return IntersectSphereBox(static_cast<const USphereComponent*>(B), static_cast<const UBoxComponent*>(A));
	}

	if (TypeA == ECollisionShapeType::Sphere && TypeB == ECollisionShapeType::Capsule)
	{
		return IntersectSphereCapsule(static_cast<const USphereComponent*>(A), static_cast<const UCapsuleComponent*>(B));
	}
	if (TypeA == ECollisionShapeType::Capsule && TypeB == ECollisionShapeType::Sphere)
	{
		return IntersectSphereCapsule(static_cast<const USphereComponent*>(B), static_cast<const UCapsuleComponent*>(A));
	}

	if (TypeA == ECollisionShapeType::Box && TypeB == ECollisionShapeType::Capsule)
	{
		return IntersectBoxCapsule(static_cast<const UBoxComponent*>(A), static_cast<const UCapsuleComponent*>(B));
	}
	if (TypeA == ECollisionShapeType::Capsule && TypeB == ECollisionShapeType::Box)
	{
		return IntersectBoxCapsule(static_cast<const UBoxComponent*>(B), static_cast<const UCapsuleComponent*>(A));
	}

	return false;
}

bool FPrimitiveCollision::IntersectSphereSphere(const USphereComponent* A, const USphereComponent* B)
{
	const FVector CenterA = A->GetWorldLocation();
	const FVector CenterB = B->GetWorldLocation();
	const float RadiusA = GetScaledSphereRadius(A);
	const float RadiusB = GetScaledSphereRadius(B);
	const float Sum = RadiusA + RadiusB;
	return FVector::DistSquared(CenterA, CenterB) <= Sum * Sum;
}

bool FPrimitiveCollision::IntersectBoxBox(const UBoxComponent* A, const UBoxComponent* B)
{
	//OBB를 고려하지 않았을 때, AABB끼리 충돌하는지로 판정.
	return IntersectAABBAABB(A->GetWorldAABB(), B->GetWorldAABB());
}

bool FPrimitiveCollision::IntersectCapsuleCapsule(const UCapsuleComponent* A, const UCapsuleComponent* B)
{
	FVector A0;
	FVector A1;
	FVector B0;
	FVector B1;

	A->GetSegmentPoints(A0, A1);
	B->GetSegmentPoints(B0, B1);

	const float RadiusSum = GetScaledCapsuleRadius(A) + GetScaledCapsuleRadius(B);
	return SegmentSegmentDistSq(A0, A1, B0, B1) <= RadiusSum * RadiusSum;
}

bool FPrimitiveCollision::IntersectSphereBox(const USphereComponent* Sphere, const UBoxComponent* Box)
{
	const FVector Center = Sphere->GetWorldLocation();
	const float Radius = GetScaledSphereRadius(Sphere);
	const FBoundingBox BoxAABB = Box->GetWorldAABB();
	const FVector Closest = ClosestPointOnAABB(Center, BoxAABB);
	return FVector::DistSquared(Center, Closest) <= Radius * Radius;
}

bool FPrimitiveCollision::IntersectSphereCapsule(const USphereComponent* Sphere, const UCapsuleComponent* Capsule)
{
	const FVector Center = Sphere->GetWorldLocation();
	const float SphereRadius = GetScaledSphereRadius(Sphere);
	const float CapsuleRadius = GetScaledCapsuleRadius(Capsule);

	FVector P0;
	FVector P1;
	Capsule->GetSegmentPoints(P0, P1);

	const FVector Closest = ClosestPointOnSegment(Center, P0, P1);
	const float Sum = SphereRadius + CapsuleRadius;
	return FVector::DistSquared(Center, Closest) <= Sum * Sum;
}

bool FPrimitiveCollision::IntersectBoxCapsule(const UBoxComponent* Box, const UCapsuleComponent* Capsule)
{
	const FBoundingBox BoxAABB = Box->GetWorldAABB();
	const float CapsuleRadius = GetScaledCapsuleRadius(Capsule);

	FVector P0;
	FVector P1;
	Capsule->GetSegmentPoints(P0, P1);

	const FVector Expand(CapsuleRadius, CapsuleRadius, CapsuleRadius);
	const FBoundingBox Expanded(BoxAABB.Min - Expand, BoxAABB.Max + Expand);

	return IntersectSegmentAABB(P0, P1, Expanded);
}

bool FPrimitiveCollision::IntersectAABBAABB(const FBoundingBox& A, const FBoundingBox& B)
{
	return (A.Min.X <= B.Max.X && A.Max.X >= B.Min.X) &&
		(A.Min.Y <= B.Max.Y && A.Max.Y >= B.Min.Y) &&
		(A.Min.Z <= B.Max.Z && A.Max.Z >= B.Min.Z);
}

FVector FPrimitiveCollision::ClosestPointOnAABB(const FVector& Point, const FBoundingBox& AABB)
{
	return FVector(
		Clamp(Point.X, AABB.Min.X, AABB.Max.X),
		Clamp(Point.Y, AABB.Min.Y, AABB.Max.Y),
		Clamp(Point.Z, AABB.Min.Z, AABB.Max.Z)
	);
}

FVector FPrimitiveCollision::ClosestPointOnSegment(const FVector& Point, const FVector& Start, const FVector& End)
{
	const FVector Segment = End - Start;
	const float SegmentLenSq = Segment.Dot(Segment);

	if (SegmentLenSq <= 1e-6f)
	{
		return Start;
	}

	float T = (Point - Start).Dot(Segment) / SegmentLenSq;
	T = Clamp(T, 0.0f, 1.0f);
	return Start + Segment * T;
}

float FPrimitiveCollision::SegmentSegmentDistSq(
	const FVector& P1,
	const FVector& Q1,
	const FVector& P2,
	const FVector& Q2)
{
	const FVector D1 = Q1 - P1;
	const FVector D2 = Q2 - P2;
	const FVector R = P1 - P2;

	const float A = D1.Dot(D1);
	const float E = D2.Dot(D2);
	const float F = D2.Dot(R);

	float S = 0.0f;
	float T = 0.0f;

	const float Epsilon = 1e-6f;

	if (A <= Epsilon && E <= Epsilon)
	{
		return R.Dot(R);
	}

	if (A <= Epsilon)
	{
		S = 0.0f;
		T = Clamp(F / E, 0.0f, 1.0f);
	}
	else
	{
		const float C = D1.Dot(R);

		if (E <= Epsilon)
		{
			T = 0.0f;
			S = Clamp(-C / A, 0.0f, 1.0f);
		}
		else
		{
			const float B = D1.Dot(D2);
			const float Denom = A * E - B * B;

			if (Denom != Epsilon)
			{
				S = Clamp((B * F - C * E) / Denom, 0.0f, 1.0f);
			}
			else
			{
				S = 0.0f;
			}

			const float TNom = B * S + F;

			if (TNom < 0.0f)
			{
				T = 0.0f;
				S = Clamp(-C / A, 0.0f, 1.0f);
			}
			else if (TNom > E)
			{
				T = 1.0f;
				S = Clamp((B - C) / A, 0.0f, 1.0f);
			}
			else
			{
				T = TNom / E;
			}
		}
	}

	const FVector C1 = P1 + D1 * S;
	const FVector C2 = P2 + D2 * T;
	return FVector::DistSquared(C1, C2);
}