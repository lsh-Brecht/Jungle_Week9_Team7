#pragma once

//FBoundingBox — AABB (Axis-Aligned Bounding Box)
#include "Core/EngineTypes.h"
#include "Math/Vector.h"

class UPrimitiveComponent;
class AActor;
class UBoxComponent;
class USphereComponent;
class UCapsuleComponent;
struct FOBB;

struct FHitResult;
struct FOverlapInfo;

struct FPrimitiveCollision
{
public:
	static bool Intersect(const UPrimitiveComponent* A, const UPrimitiveComponent* B);
	static bool IntersectBroadPhase(const UPrimitiveComponent* A, const UPrimitiveComponent* B);

private:
	/**
	 * 6가지 충돌 경우.
	 * sphere-sphere, sphere-box, sphere-capsule, box-box, box-capsule, capsule-capsule
	 */
	static bool IntersectSphereSphere(const USphereComponent* A, const USphereComponent* B);
	static bool IntersectSphereBox(const USphereComponent* Sphere, const UBoxComponent* Box);
	static bool IntersectSphereCapsule(const USphereComponent* Sphere, const UCapsuleComponent* Capsule);
	static bool IntersectBoxBox(const UBoxComponent* A, const UBoxComponent* B);
	static bool IntersectOBBOBB(const FOBB& A, const FOBB& B);
	static bool IntersectBoxCapsule(const UBoxComponent* Box, const UCapsuleComponent* Capsule);
	static bool IntersectCapsuleCapsule(const UCapsuleComponent* A, const UCapsuleComponent* B);

private:
	//Box 충돌 관련해서 내부적으로 SAT가 호출됨
	static bool IntersectAABBAABB(const FBoundingBox& A, const FBoundingBox& B);
	
	static FVector ClosestPointOnAABB(const FVector& Point, const FBoundingBox& AABB);
	static FVector ClosestPointOnSegment(const FVector& Point, const FVector& Start, const FVector& End);

	static float SegmentSegmentDistSq(
		const FVector& P1,
		const FVector& Q1,
		const FVector& P2,
		const FVector& Q2
	);
};