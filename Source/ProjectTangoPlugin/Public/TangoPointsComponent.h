/*Copyright 2016 Opaque Media Group

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.*/
#pragma once
#include "Components/MeshComponent.h"
#include "DynamicMeshBuilder.h"
#include "TangoPointsComponent.generated.h"

UCLASS(ClassGroup = Tango, meta = (BlueprintSpawnableComponent))
class PROJECTTANGOPLUGIN_API UTangoPointsComponent : public UMeshComponent
{
	GENERATED_BODY()

public:

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, meta = (ToolTip = "The current timestamp of points component"))
		float Timestamp;

	UPROPERTY(Category = "Point Rendering", EditAnywhere, BlueprintReadWrite, meta = (ToolTip = "Material used to color the points"))
		UMaterialInterface * Material;

public:
	UTangoPointsComponent();

	//UActorComponent interface
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	// Begin UPrimitiveComponent interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	// End UPrimitiveComponent interface.
};

/** This class is the container inside the renderer that holds onto our array of vertices. */
class FTangoPointVertexResourceArray : public FResourceArrayInterface
{
public:
	FTangoPointVertexResourceArray(void* InData, uint32 InSize)
		: Data(InData)
		, Size(InSize)
	{
	}

	virtual const void* GetResourceData() const override 
	{ 
		return Data; 
	}

	virtual uint32 GetResourceDataSize() const override 
	{ 
		return Size; 
	}

	virtual void Discard() override { }

	virtual bool IsStatic() const override 
	{ 
		return false; 
	}

	virtual bool GetAllowCPUAccess() const override 
	{ 
		return false; 
	}

	virtual void SetAllowCPUAccess(bool bInNeedsCPUAccess) override { }

private:
	void* Data;
	uint32 Size;
};

/** This class is responsible for managing the vertex buffer object of the RHI implementation in use. */
class FTangoPointVertexBuffer : public FVertexBuffer
{
public:
	TArray<FDynamicMeshVertex> Vertices;

	virtual void InitRHI() override
	{
		const uint32 SizeInBytes = Vertices.Num() * sizeof(FDynamicMeshVertex);

		FTangoPointVertexResourceArray ResourceArray(Vertices.GetData(), SizeInBytes);
		FRHIResourceCreateInfo CreateInfo(&ResourceArray);
		VertexBufferRHI = RHICreateVertexBuffer(SizeInBytes, BUF_Dynamic, CreateInfo);
	}

};

/** This class is responsible for managing the Index Buffer Object of the RHI implementation in use. */
class FTangoPointIndexBuffer : public FIndexBuffer
{
public:
	TArray<int32> Indices;

	virtual void InitRHI() override
	{
		FRHIResourceCreateInfo CreateInfo;
		void* Buffer = nullptr;
		IndexBufferRHI = RHICreateAndLockIndexBuffer(sizeof(int32), Indices.Num() * sizeof(int32), BUF_Dynamic, CreateInfo, Buffer);

		// Write the indices to the index buffer.		
		FMemory::Memcpy(Buffer, Indices.GetData(), Indices.Num() * sizeof(int32));
		RHIUnlockIndexBuffer(IndexBufferRHI);
	}
};

/** The Vertex Factory tells the RHI about the vertex data we are uploading to the graphics card. */
class FTangoPointVertexFactory : public FLocalVertexFactory
{
public:

	FTangoPointVertexFactory()
	{}

	/** Init function that should only be called on render thread. */
	void Init_RenderThread(const FTangoPointVertexBuffer* VertexBuffer)
	{
		check(IsInRenderingThread());

		// Initialize the vertex factory's stream components.
		DataType NewData;
		NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FDynamicMeshVertex, Position, VET_Float3);
		NewData.ColorComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FDynamicMeshVertex, Color, VET_Color);
		SetData(NewData);


	}

	/** Init function that can be called on any thread, and will do the right thing (enqueue command if called on main thread) */
	void Init(const FTangoPointVertexBuffer* VertexBuffer)
	{
		if (IsInRenderingThread())
		{
			Init_RenderThread(VertexBuffer);
		}
		else
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
					InitTangoPointVertexFactory,
					FTangoPointVertexFactory*, VertexFactory, this,
					const FTangoPointVertexBuffer*, VertexBuffer, VertexBuffer,
				{
					VertexFactory->Init_RenderThread(VertexBuffer);
				});
		}
	}
};

/** The SceneProxy tells the renderer what our object looks like. This shouldn't be interacted with
* on the game thread.
*/
class FTangoPointCloudSceneProxy : public FPrimitiveSceneProxy
{
public:
	UMaterialInterface * Material;
	FTangoPointVertexBuffer VertexBuffer;
	FTangoPointIndexBuffer IndexBuffer;
	FTangoPointVertexFactory VertexFactory;
	FPrimitiveViewRelevance ViewRelevance;
	FLinearColor Color;
	float PointSize;
	uint8 DepthPriority;

public:

	FTangoPointCloudSceneProxy(const UTangoPointsComponent* InComponent);
	virtual ~FTangoPointCloudSceneProxy();

	void UpdatePoints_RenderThread();

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) override
	{
		return ViewRelevance;
	}
	virtual uint32 GetMemoryFootprint(void) const override { 
		return(sizeof(*this) + GetAllocatedSize()); 
	}
	uint32 GetAllocatedSize(void) const
	{
		return FPrimitiveSceneProxy::GetAllocatedSize();
	}
};