// Copyright ©2020 Samuel Harrison

//includes
#include "PointSpawner.h"
#include "Components/BillboardComponent.h"
#include "Boid.h"
#include "FlockManager.h"

APointSpawner::APointSpawner()
{
	//disable ticking
	PrimaryActorTick.bCanEverTick = false;

	//setup visual component
	SpawnPointBillboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Spawn Point Billboard Component"));
	RootComponent = SpawnPointBillboard;

	//set default number of boids to spawn
	NumBoidsToSpawn = 0;
}

void APointSpawner::BeginPlay()
{
	Super::BeginPlay();

	//spawn initial boid flock
	SpawnBoids(NumBoidsToSpawn);
}


void APointSpawner::SpawnBoids(int32 NumBoids)
{
	//check for assigned flock manager
	if (AssignedFlockManager == nullptr)
	{
		//log warning to console
		UE_LOG(LogTemp, Warning, TEXT("No FlockManager found for Boid spawner: %s."), *GetName());
		return;
	}

	//check BoidType is set in BP
	if (BoidType == nullptr)
	{
		//log warning to console
		UE_LOG(LogTemp, Warning, TEXT("BoidType not set for Spawner: %s"), *GetName());
		return;
	}

	//spawn boids
	FVector SpawnLocation = this->GetActorLocation();
	FRotator SpawnRotation = FRotator::ZeroRotator;
	FActorSpawnParameters BoidSpawnParams;
	BoidSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	BoidSpawnParams.Owner = AssignedFlockManager.Get();

	for (int32 i = 0; i < NumBoids; ++i)
	{
		//spawn boids at point in random directions
		SpawnRotation = FMath::VRand().ToOrientationRotator();
		AssignedFlockManager->AddBoidToFlock(GetWorld()->SpawnActor<ABoid>(BoidType, SpawnLocation, SpawnRotation, BoidSpawnParams));
	}
}

