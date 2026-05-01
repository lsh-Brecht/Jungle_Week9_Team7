#include "Component/SceneComponent.h"
#include "Collision/PrimitiveCollision.h"
#include "Collision/OBB.h" //SAT
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
	constexpr float CollisionEpsilon = 1e-6f;

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

	FVector GetMatrixAxis(const FMatrix& Matrix, int Axis)
	{
		return FVector(Matrix.M[Axis][0], Matrix.M[Axis][1], Matrix.M[Axis][2]);
	}

	FVector GetSafeNormalizedAxis(const FVector& Axis, const FVector& Fallback)
	{
		const float Length = Axis.Length();
		if (Length <= CollisionEpsilon)
		{
			return Fallback;
		}

		return Axis / Length;
	}

	FOBB MakeBoxOBB(const UBoxComponent* Box)
	{
		const FMatrix& WorldMatrix = Box->GetWorldMatrix();
		const FVector LocalExtent = Box->GetBoxExtent();

		const FVector ScaledAxes[3] = {
			GetMatrixAxis(WorldMatrix, 0),
			GetMatrixAxis(WorldMatrix, 1),
			GetMatrixAxis(WorldMatrix, 2)
		};
		const FVector FallbackAxes[3] = {
			FVector(1.0f, 0.0f, 0.0f),
			FVector(0.0f, 1.0f, 0.0f),
			FVector(0.0f, 0.0f, 1.0f)
		};

		FOBB Result;
		Result.Center = WorldMatrix.GetLocation();
		Result.Extent = FVector(
			LocalExtent.X * ScaledAxes[0].Length(),
			LocalExtent.Y * ScaledAxes[1].Length(),
			LocalExtent.Z * ScaledAxes[2].Length());

		FMatrix RotationMatrix;
		RotationMatrix.SetAxes(
			GetSafeNormalizedAxis(ScaledAxes[0], FallbackAxes[0]),
			GetSafeNormalizedAxis(ScaledAxes[1], FallbackAxes[1]),
			GetSafeNormalizedAxis(ScaledAxes[2], FallbackAxes[2]));
		Result.Rotation = RotationMatrix.ToRotator();
		return Result;
	}

	void GetOBBAxes(const FOBB& OBB, FVector (&OutAxes)[3])
	{
		OutAxes[0] = OBB.Rotation.GetForwardVector().Normalized();
		OutAxes[1] = OBB.Rotation.GetRightVector().Normalized();
		OutAxes[2] = OBB.Rotation.GetUpVector().Normalized();
	}

	FVector WorldToOBBLocal(const FVector& Point, const FOBB& OBB, const FVector (&Axes)[3])
	{
		const FVector Delta = Point - OBB.Center;
		return FVector(
			Delta.Dot(Axes[0]),
			Delta.Dot(Axes[1]),
			Delta.Dot(Axes[2]));
	}

	FVector ClosestPointOnOBB(const FVector& Point, const FOBB& OBB)
	{
		FVector Axes[3];
		GetOBBAxes(OBB, Axes);

		const FVector Local = WorldToOBBLocal(Point, OBB, Axes);
		FVector Closest = OBB.Center;
		for (int Axis = 0; Axis < 3; ++Axis)
		{
			const float Distance = Clamp(Local.Data[Axis], -OBB.Extent.Data[Axis], OBB.Extent.Data[Axis]);
			Closest += Axes[Axis] * Distance;
		}

		return Closest;
	}

	float SegmentOBBDistSq(const FVector& Start, const FVector& End, const FOBB& OBB)
	{
		FVector Axes[3];
		GetOBBAxes(OBB, Axes);

		const FVector LocalStart = WorldToOBBLocal(Start, OBB, Axes);
		const FVector LocalEnd = WorldToOBBLocal(End, OBB, Axes);
		const FBoundingBox LocalBox(OBB.Extent * -1.0f, OBB.Extent);

		auto PointAABBDistSq = [&LocalBox](const FVector& Point)
		{
			const FVector Closest(
				Clamp(Point.X, LocalBox.Min.X, LocalBox.Max.X),
				Clamp(Point.Y, LocalBox.Min.Y, LocalBox.Max.Y),
				Clamp(Point.Z, LocalBox.Min.Z, LocalBox.Max.Z));
			return FVector::DistSquared(Point, Closest);
		};

		if (PointAABBDistSq(LocalStart) <= CollisionEpsilon || PointAABBDistSq(LocalEnd) <= CollisionEpsilon)
		{
			return 0.0f;
		}

		const FVector Segment = LocalEnd - LocalStart;
		if (Segment.Dot(Segment) <= CollisionEpsilon)
		{
			return PointAABBDistSq(LocalStart);
		}

		float Left = 0.0f;
		float Right = 1.0f;
		for (int Iteration = 0; Iteration < 40; ++Iteration)
		{
			const float T1 = Left + (Right - Left) / 3.0f;
			const float T2 = Right - (Right - Left) / 3.0f;
			const float D1 = PointAABBDistSq(LocalStart + Segment * T1);
			const float D2 = PointAABBDistSq(LocalStart + Segment * T2);
			if (D1 < D2)
			{
				Right = T2;
			}
			else
			{
				Left = T1;
			}
		}

		const float T = (Left + Right) * 0.5f;
		return PointAABBDistSq(LocalStart + Segment * T);
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
	// AABB broad phase를 먼저 통과한 뒤 실제 박스 방향은 OBB SAT로 판정합니다.
	if (!IntersectAABBAABB(A->GetWorldAABB(), B->GetWorldAABB()))
	{
		return false;
	}

	return IntersectOBBOBB(MakeBoxOBB(A), MakeBoxOBB(B));
}

bool FPrimitiveCollision::IntersectOBBOBB(const FOBB& A, const FOBB& B)
{
	FVector AxesA[3];
	FVector AxesB[3];
	GetOBBAxes(A, AxesA);
	GetOBBAxes(B, AxesB);

	float R[3][3];
	float AbsR[3][3];
	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			R[i][j] = AxesA[i].Dot(AxesB[j]);
			AbsR[i][j] = std::abs(R[i][j]) + CollisionEpsilon;
		}
	}

	const FVector Translation = B.Center - A.Center;
	const FVector T(
		Translation.Dot(AxesA[0]),
		Translation.Dot(AxesA[1]),
		Translation.Dot(AxesA[2]));

	for (int i = 0; i < 3; ++i)
	{
		const float RA = A.Extent.Data[i];
		const float RB =
			B.Extent.X * AbsR[i][0] +
			B.Extent.Y * AbsR[i][1] +
			B.Extent.Z * AbsR[i][2];
		if (std::abs(T.Data[i]) > RA + RB)
		{
			return false;
		}
	}

	for (int j = 0; j < 3; ++j)
	{
		const float RA =
			A.Extent.X * AbsR[0][j] +
			A.Extent.Y * AbsR[1][j] +
			A.Extent.Z * AbsR[2][j];
		const float RB = B.Extent.Data[j];
		const float Distance = std::abs(
			T.X * R[0][j] +
			T.Y * R[1][j] +
			T.Z * R[2][j]);
		if (Distance > RA + RB)
		{
			return false;
		}
	}

	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			const float RA =
				A.Extent.Data[(i + 1) % 3] * AbsR[(i + 2) % 3][j] +
				A.Extent.Data[(i + 2) % 3] * AbsR[(i + 1) % 3][j];
			const float RB =
				B.Extent.Data[(j + 1) % 3] * AbsR[i][(j + 2) % 3] +
				B.Extent.Data[(j + 2) % 3] * AbsR[i][(j + 1) % 3];
			const float Distance = std::abs(
				T.Data[(i + 2) % 3] * R[(i + 1) % 3][j] -
				T.Data[(i + 1) % 3] * R[(i + 2) % 3][j]);
			if (Distance > RA + RB)
			{
				return false;
			}
		}
	}

	return true;
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
	const FVector Closest = ClosestPointOnOBB(Center, MakeBoxOBB(Box));
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
	const float CapsuleRadius = GetScaledCapsuleRadius(Capsule);

	FVector P0;
	FVector P1;
	Capsule->GetSegmentPoints(P0, P1);

	return SegmentOBBDistSq(P0, P1, MakeBoxOBB(Box)) <= CapsuleRadius * CapsuleRadius;
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

			if (Denom > Epsilon)
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