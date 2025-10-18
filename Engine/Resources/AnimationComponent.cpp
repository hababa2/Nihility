#include "AnimationComponent.hpp"

#include "World.hpp"

#include "Core/Time.hpp"

Vector<Animation> Animation::components(64, {});
Freelist Animation::freeComponents(64);
bool Animation::initialized = false;

void AnimationClip::Create(const TextureAtlas& atlas, U32 startX, U32 startY, U32 countX, U32 countY, F32 frameTime)
{
	for (U32 y = startY; y < startY + countY; ++y)
	{
		for (U32 x = startX; x < startX + countX; ++x)
		{
			frames.Push({ atlas.texture, { atlas.spriteWidth * x, atlas.spriteHeight * y }, { atlas.spriteWidth, atlas.spriteHeight }, frameTime });
		}
	}
}

bool Animation::Initialize()
{
	if (!initialized)
	{
		World::UpdateFns += Update;
		World::RenderFns += Render;

		initialized = true;
	}

	return false;
}

bool Animation::Shutdown()
{
	if (initialized) { initialized = false; }

	return false;
}

ComponentRef<Animation> Animation::AddTo(const EntityRef& entity, const ComponentRef<Sprite>& sprite)
{
	U32 instanceId;
	Animation& animation = Create(instanceId);
	animation.entityIndex = entity.EntityId();
	animation.sprite = sprite;

	return { entity.EntityId(), instanceId };
}

void Animation::RemoveFrom(const EntityRef& entity)
{
	ComponentRef<Animation> animation = GetRef(entity);
	if (animation)
	{
		animation->sprite = nullptr;
		animation->clips.Destroy();

		Destroy(*animation);
	}
}

bool Animation::Update(Camera& camera, Vector<Entity>& entities)
{
	for (Animation& animation : components)
	{
		if (animation.entityIndex == U32_MAX || animation.clips.Empty()) { continue; }
		Entity& entity = entities[animation.entityIndex];

		AnimationClip& clip = animation.clips[animation.clipIndex];
		AnimationFrame& frame = clip.frames[animation.currentFrame];
		animation.timer -= (F32)Time::DeltaTimeStable();
		if (animation.timer <= 0.0f)
		{
			++animation.currentFrame %= clip.frames.Size();
			animation.timer = frame.duration + animation.timer;

			Vector2 texcoord = frame.texcoord;
			Vector2 texcoordScale = frame.texcoordScale;

			if (animation.flipX)
			{
				texcoord.x += texcoordScale.x;
				texcoordScale.x = -texcoordScale.x;
			}

			if (animation.flipY)
			{
				texcoord.y += texcoordScale.y;
				texcoordScale.y = -texcoordScale.y;
			}

			animation.sprite->SetTexture(frame.texture, texcoord, texcoordScale);
		}
	}

	return false;
}

bool Animation::Render(CommandBuffer commandBuffer)
{
	return false;
}

void Animation::AddClip(const AnimationClip& clip)
{
	clips.Push(clip);
}

void Animation::SetClip(U32 index)
{
	currentFrame = 0;
	timer = 0.0f;
	clipIndex = index;
}

void Animation::SetFlipX(bool flipX)
{
	this->flipX = flipX;
}

void Animation::SetFlipY(bool flipY)
{
	this->flipY = flipY;
}