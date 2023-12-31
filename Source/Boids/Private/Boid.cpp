// Copyright ©2020 Samuel Harrison

//includes
#include "Boid.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "FlockManager.h"
#include "DrawDebugHelpers.h"
#include "Boids/Boids.h"						//used for project global collision preset "COLLISION_AVOIDANCE"

ABoid::ABoid()
{
	//enable actor ticking
	PrimaryActorTick.bCanEverTick = true;

	//setup boid collision component and set as root
	BoidCollision = CreateDefaultSubobject<USphereComponent>(TEXT("Boid Collision Component"));
	RootComponent = BoidCollision;
	BoidCollision->SetCollisionObjectType(ECC_Pawn);
	BoidCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoidCollision->SetCollisionResponseToAllChannels(ECR_Overlap);

	//setup boid mesh component & attach to root
	BoidMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Boid Mesh Component"));
	BoidMesh->SetupAttachment(RootComponent);
	BoidMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoidMesh->SetCollisionResponseToAllChannels(ECR_Ignore);

	//setup cohesion sensor component
	PerceptionSensor = CreateDefaultSubobject<USphereComponent>(TEXT("Perception Sensor Component"));
	PerceptionSensor->SetupAttachment(RootComponent);
	PerceptionSensor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PerceptionSensor->SetCollisionResponseToAllChannels(ECR_Ignore);
	PerceptionSensor->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PerceptionSensor->SetSphereRadius(300.0f);

	//set default velocity
	BoidVelocity = FVector::ZeroVector;
}

void ABoid::BeginPlay()
{
	Super::BeginPlay();

	//check if boid is owned by a flock manager
	AFlockManager* BoidOwner = Cast<AFlockManager>(this->GetOwner());
	if (BoidOwner)
	{
		//set flock manager
		FlockManager = BoidOwner;

		//set velocity based on spawn rotation and flock speed settings
		BoidVelocity = this->GetActorForwardVector();
		BoidVelocity.Normalize();
		BoidVelocity *= FMath::FRandRange(FlockManager->GetMinSpeed(), FlockManager->GetMaxSpeed());

		//set current mesh rotation
		CurrentRotation = this->GetActorRotation();
	}
	else
	{
		//log warning to console
		UE_LOG(LogTemp, Warning, TEXT("No FlockManager found for Boid: %s."), *GetName());

		//TODO: attempt to find a suitable flock manager or set lifespan of boid since it won't be able to properly move without one
	}
}

void ABoid::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	//steer and move boid
	Steer(DeltaTime);
	
	//update boid mesh rotation
	UpdateMeshRotation();
}

void ABoid::UpdateMeshRotation()
{
	//TODO: need to create more complex and sophisticated system for "banking" that mimic boid's flight mechanics

	//rotate toward current boid heading smoothly
	CurrentRotation = FMath::RInterpTo(CurrentRotation, this->GetActorRotation(), GetWorld()->DeltaTimeSeconds, 7.0f);
	this->BoidMesh->SetWorldRotation(CurrentRotation);
}

FVector ABoid::Separate(TArray<AActor*> Flock)
{
	//check for valid flock manager
	if (FlockManager == nullptr) { return FVector::ZeroVector; }

	FVector Steering = FVector::ZeroVector;
	int32 FlockCount = 0;
	FVector SeparationDirection = FVector::ZeroVector;
	float ProximityFactor = 0.0f;

	//get separation steering force for each of the boid's flockmates
	for (AActor* OverlapActor : Flock)
	{
		ABoid* Flockmate = Cast<ABoid>(OverlapActor);
		if (Flockmate != nullptr && Flockmate != this)
		{
			//check if flockmate is outside perception fov
			//TODO: find way to offload this check to collision based solution, sphere collision that have slices for perception FOV elastic grow/shrinking (), (<, (|, (>, etc.?
			if (FVector::DotProduct(this->GetActorForwardVector(), (Flockmate->GetActorLocation() - this->GetActorLocation()).GetSafeNormal()) <= FlockManager->GetSeparationFOV())
			{
				continue;	//flockmate is outside perception angle, disregard it and continue the loop
			}

			//get normalized direction away from nearby boid
			SeparationDirection = this->GetActorLocation() - Flockmate->GetActorLocation();
			SeparationDirection = SeparationDirection.GetSafeNormal();

			//get scaling factor based off other boid's proximity. 0 = very far away (no separation force) & 1 = very close (full separation force)
			ProximityFactor = 1.0f - (SeparationDirection.Size() / this->PerceptionSensor->GetScaledSphereRadius());

			//check if flockmate's center of mass is outside perception radius (i.e. colliding but not within radius)
			//TODO: add flockmate's collision radius to perception radius so they can be included in calculations
			if (ProximityFactor < 0.0f)
			{
				continue;	//flockmate is outside of perception radius, disregard and continue loop
			}

			//add steering force of flockmate and increase flock count
			Steering += (ProximityFactor * SeparationDirection);
			FlockCount++;
		}
	}

	if (FlockCount > 0)
	{
		//get flock average separation steering force, apply separation steering strength factor and return force
		Steering /= FlockCount;
		Steering.GetSafeNormal() -= this->BoidVelocity.GetSafeNormal();
		Steering *= FlockManager->GetSeparationStrength();
		return Steering;
	}
	else
	{
		return FVector::ZeroVector;
	}
}

FVector ABoid::Align(TArray<AActor*> Flock)
{
	//check for valid flock manager
	if (FlockManager == nullptr) { return FVector::ZeroVector; }

	FVector Steering = FVector::ZeroVector;
	int32 FlockCount = 0.0f;

	//get alignment steering force for each of the boid's flockmates
	for (AActor* OverlapActor : Flock)
	{
		ABoid* Flockmate = Cast<ABoid>(OverlapActor);
		if (Flockmate != nullptr && Flockmate != this)
		{
			//check if flockmate is outside alignment perception fov
			//TODO: find way to offload this check to collision based solution, sphere collision that have slices for perception FOV elastic grow/shrinking (), (<, (|, (>, etc.?
			if (FVector::DotProduct(this->GetActorForwardVector(), (Flockmate->GetActorLocation() - this->GetActorLocation()).GetSafeNormal()) <= FlockManager->GetAlignmentFOV())
			{
				continue;	//flockmate is outside viewing angle, disregard it and continue the loop
			}
			//add flockmate's alignment force
			Steering += Flockmate->BoidVelocity.GetSafeNormal();
			FlockCount++;
		}
	}

	if (FlockCount > 0)
	{
		//get alignment force to average flock direction
		Steering /= FlockCount;
		Steering.GetSafeNormal() -= this->BoidVelocity.GetSafeNormal();
		Steering *= FlockManager->GetAlignmentStrength();
		return Steering;
	}
	else
	{
		return FVector::ZeroVector;
	}
}

FVector ABoid::GroupUp(TArray<AActor*> Flock)
{
	//check for valid flock manager
	if (FlockManager == nullptr) { return FVector::ZeroVector; }

	FVector Steering = FVector::ZeroVector;
	int32 FlockCount = 0.0f;
	FVector AveragePosition = FVector::ZeroVector;

	//get sum of flockmate positions
	for (AActor* OverlapActor : Flock)
	{
		ABoid* Flockmate = Cast<ABoid>(OverlapActor);
		if (Flockmate != nullptr && Flockmate != this)
		{
			//check if flockmate is outside cohesion perception angle
			//TODO: find way to offload this check to collision based solution, sphere collision that have slices for perception FOV elastic grow/shrinking (), (<, (|, (>, etc.?
			if (FVector::DotProduct(this->GetActorForwardVector(), (Flockmate->GetActorLocation() - this->GetActorLocation()).GetSafeNormal()) <= FlockManager->GetCohesionFOV())
			{
				continue;	//flockmate is outside viewing angle, disregard this flockmate and continue the loop
			}
			//get cohesive force to group with flockmate
			AveragePosition += Flockmate->GetActorLocation();
			FlockCount++;
		}
	}

	if (FlockCount > 0)
	{
		//average cohesion force of flock
		AveragePosition /= FlockCount;
		Steering = AveragePosition - this->GetActorLocation();
		Steering.GetSafeNormal() -= this->BoidVelocity.GetSafeNormal();
		Steering *= FlockManager->GetCohesionStrength();
		return Steering;
	}
	else
	{
		return FVector::ZeroVector;
	}
}

void ABoid::Steer(float DeltaTime)
{
	//check for valid flock manager
	if (FlockManager == nullptr) { return; }

	FVector Acceleration = FVector::ZeroVector;

	//update position and rotation
	//AddActorLocalOffset(Velocity * DeltaTime, true);	//TODO: using this prevents boids from rotating to face velocity???
	this->SetActorLocation(this->GetActorLocation() + (BoidVelocity * DeltaTime));
	this->SetActorRotation(BoidVelocity.ToOrientationQuat());

	//apply steering forces to boid acceleration
	//find flockmates in general area to fly with
	TArray<AActor*> Flockmates;
	PerceptionSensor->GetOverlappingActors(Flockmates, TSubclassOf<ABoid>());
	Acceleration += Separate(Flockmates);
	Acceleration += Align(Flockmates);
	Acceleration += GroupUp(Flockmates);

	//TODO: add logic to disregard other steering forces if collision is found. Prioritize avoidance and reduce chance they steer into obstacle due to swarm forces.
	//check if heading for collision
	if (IsObstacleAhead())
	{
		//apply obstacle avoidance force
		Acceleration += AvoidObstacle();
	}

	//apply target forces and remove from stack
	for (FVector TargetForce : TargetForces)
	{
		Acceleration += TargetForce;
		TargetForces.Remove(TargetForce);
	}

	//update velocity
	BoidVelocity += (Acceleration * DeltaTime);
	BoidVelocity = BoidVelocity.GetClampedToSize(FlockManager->GetMinSpeed(), FlockManager->GetMaxSpeed());
}

bool ABoid::IsObstacleAhead()
{
	//check for valid flock manager
	if (FlockManager == nullptr) { return false; }

	if (FlockManager->GetAvoidanceSensors().Num() > 0)
	{
		//check forward sensor for collision
		//TODO: remove sensor array rotation, add simple line trace in forward direction to optimize, not critical that array is aligned with boid's orientation for obstacle check
		//rotate 1st sensor towards forward direction of boid
		FQuat SensorRotation = FQuat::FindBetweenVectors(FlockManager->GetAvoidanceSensors()[0], this->GetActorForwardVector());
		FVector NewSensorDirection = FVector::ZeroVector;
		NewSensorDirection = SensorRotation.RotateVector(FlockManager->GetAvoidanceSensors()[0]);
		//set collision properties and parameters for collision trace
		FCollisionQueryParams TraceParameters;
		FHitResult Hit;
		//run line trace for collision check on 1st sensor
		GetWorld()->LineTraceSingleByChannel(Hit, this->GetActorLocation(), this->GetActorLocation() + NewSensorDirection * FlockManager->GetSensorRadius(), COLLISION_AVOIDANCE, TraceParameters);

		//check hit result for collision and draw collision probe status
		//TODO remove debug lines when not needed. possible toggle?
		/*if (Hit.bBlockingHit)
		{
			DrawDebugLine(GetWorld(), this->GetActorLocation(), this->GetActorLocation() + NewSensorDirection * FlockManager->GetSensorRadius(), FColor::Red, false, -1.0f, 0, 1.5f);
			DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 8.0f, 12, FColor::Red, false, -1.0f, 0, 1.0f);
		}
		else
		{
			DrawDebugLine(GetWorld(), this->GetActorLocation(), this->GetActorLocation() + NewSensorDirection * FlockManager->GetSensorRadius(), FColor::Green, false, -1.0f, 0, 1.5f);
		}*/

		//check if boid is inside object (i.e. no need to avoid/impossible to)
		if (Hit.bBlockingHit)
		{
			TArray<AActor*> OverlapActors;
			BoidCollision->GetOverlappingActors(OverlapActors);
			for (AActor* OverlapActor : OverlapActors)
			{
				if (Hit.GetActor() == OverlapActor)
				{
					return false;
				}
			}
		}

		return Hit.bBlockingHit;


	}

	//there are no sensors to check
	return false;
}

FVector ABoid::AvoidObstacle()
{
	//check for valid flock manager
	if (FlockManager == nullptr) { return FVector::ZeroVector; }

	FVector Steering = FVector::ZeroVector;
	FQuat SensorRotation = FQuat::FindBetweenVectors(FlockManager->GetAvoidanceSensors()[0], this->GetActorForwardVector());
	FVector NewSensorDirection = FVector::ZeroVector;
	FCollisionQueryParams TraceParameters;
	FHitResult Hit;

	for (FVector AvoidanceSensor : FlockManager->GetAvoidanceSensors())
	{
		//rotate avoidance sensor to align with boid orientation and trace for collision
		NewSensorDirection = SensorRotation.RotateVector(AvoidanceSensor);
		GetWorld()->LineTraceSingleByChannel(Hit, this->GetActorLocation(), this->GetActorLocation() + NewSensorDirection * FlockManager->GetSensorRadius(), COLLISION_AVOIDANCE, TraceParameters);
		if (Hit.bBlockingHit)
		{
			/*DrawDebugLine(GetWorld(), this->GetActorLocation(), this->GetActorLocation() + NewSensorDirection * FlockManager->GetSensorRadius(), FColor::Red, false, -1.0f, 0, 1.5f);
			DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 8.0f, 12, FColor::Red, false, -1.0f, 0, 1.0f);*/
		}
		else
		{
			//DrawDebugLine(GetWorld(), this->GetActorLocation(), this->GetActorLocation() + NewSensorDirection * FlockManager->GetSensorRadius(), FColor::Green, false, -1.0f, 0, 1.5f);
			//TODO add proximity factor to avoidance. The closer to collision the stronger the force.
			Steering = NewSensorDirection.GetSafeNormal() - this->BoidVelocity.GetSafeNormal();
			Steering *= FlockManager->GetAvoidanceStrength();
			//DrawDebugDirectionalArrow(GetWorld(), this->GetActorLocation(), this->GetActorLocation() + Steering, 5.0f, FColor::Yellow, false, -1.0f, 0, 3.0f);
			return Steering;
		}
	}

	return FVector::ZeroVector;
}

void ABoid::AddTargetForce(FVector TargetForce)
{
	TargetForces.Add(TargetForce);
}
