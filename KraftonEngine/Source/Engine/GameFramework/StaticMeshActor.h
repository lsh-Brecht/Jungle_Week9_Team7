#pragma once

#include "GameFramework/AActor.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class USubUVComponent;
class UBoxComponent;

class AStaticMeshActor : public AActor
{
public:
	DECLARE_CLASS(AStaticMeshActor, AActor)
	AStaticMeshActor() {}

	void InitDefaultComponents(const FString& UStaticMeshFileName = "Data/BasicShape/Cylinder.obj");

private:
	UStaticMeshComponent* StaticMeshComponent = nullptr;
	
	UBoxComponent* BoxComponent = nullptr;
	//UTextRenderComponent* TextRenderComponent = nullptr;
	//USubUVComponent* SubUVComponent = nullptr;
};
