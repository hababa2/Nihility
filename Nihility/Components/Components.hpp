#pragma once

#include "Defines.hpp"

#include "Platform/Audio.hpp"

#include <glm/glm.hpp>

struct NH_API Transform2D
{
	glm::vec2 position{ 0.0f, 0.0f };
	glm::vec2 scale{ 1.0f, 1.0f };
	F32 rotation{ 0.0f };
};

struct NH_API SpriteComponent
{
	U32 textureId{ 0 };
	glm::vec4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
	U32 zIndex{ 0 };
};

struct NH_API PlayerController
{
	F32 moveSpeed = 10.0f;
	F32 jumpForce = -400.0f;
	F32 coyoteTime = 0.075f;
	F32 coyoteTimer = 0.0f;
	F32 jumpBufferTime = 0.075f;
	F32 jumpBufferTimer = 0.0f;

	glm::vec2 spawnPosition{ 0.0f, 0.0f };
};

struct NH_API Camera
{
	glm::mat4 projection;
	glm::mat4 view;
};

struct NH_API CameraTarget
{
	U32 targetEntity = U32_MAX;
	glm::vec2 offset{ 0.0f, 0.0f };
	F32 smoothSpeed = 15.0f;
};

struct NH_API AudioEmitter
{
	PlaybackHandle handle;
	glm::vec2 prevPosition{ 0.0f, 0.0f };
};