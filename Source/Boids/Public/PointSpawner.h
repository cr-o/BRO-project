// Copyright ©2020 Samuel Harrison

//README:~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//An actor you can place in the world and spawn a number of Boids from its location that go in random directions.
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#pragma once

//includes
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PointSpawner.generated.h"

//forward declares
class UBillboardComponent;
class ABoid;
class AFlockManager;

UCLASS()
class BOIDS_API APointSpawner : public AActor
{
	GENERATED_BODY()

public:
	//default constructor
	APointSpawner();

protected:
	//called on level start or when spawned
	virtual void BeginPlay() override;

	//COMPONENTS
protected:
	//scene component
	UPROPERTY(VisibleAnywhere, Category = "Boid|Components")
	UBillboardComponent* SpawnPointBillboard;

	//SPAWNING
protected:
	//number of boids to spawn initially
	UPROPERTY(EditAnywhere, Category = "Boid|Spawn")
	int32 NumBoidsToSpawn;

	//type of boid to spawn
	UPROPERTY(EditAnywhere, Category = "Boid|Spawn")
	TSubclassOf<ABoid> BoidType;

	//spawn flock of boids at point
	void SpawnBoids(int32 NumBoids);

	//flock manager that has been assigned to spawned boids
	UPROPERTY(EditAnywhere, Category = "Boid|Spawn")
	TSoftObjectPtr <AFlockManager> AssignedFlockManager;

	//TODO: add ramped spawning feature, allowing boids to be spawned over duration instead of all at once to reduce processing load on single frame to multiple frames
};
