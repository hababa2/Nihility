#pragma once

#include "Defines.hpp"

#include "Platform/Audio.hpp"
#include "Core/DataWriter.hpp"
#include "Core/DataReader.hpp"

#include <glm/glm.hpp>

struct NH_API Transform2D
{
	glm::vec2 position{ 0.0f, 0.0f };
	glm::vec2 scale{ 1.0f, 1.0f };
	F32 rotation{ 0.0f };

	void Serialize(DataWriter& writer) const
	{
		writer.Write(position);
		writer.Write(scale);
		writer.Write(rotation);
	}

	void Deserialize(DataReader& reader)
	{
		reader.Read(position);
		reader.Read(scale);
		reader.Read(rotation);
	}
};

struct NH_API SpriteComponent
{
	U32 textureId{ 0 };
	glm::vec4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
	U32 zIndex{ 0 };

	void Serialize(DataWriter& writer) const
	{
		writer.Write(textureId);
		writer.Write(color);
		writer.Write(zIndex);
	}

	void Deserialize(DataReader& reader)
	{
		reader.Read(textureId);
		reader.Read(color);
		reader.Read(zIndex);
	}
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

	void Serialize(DataWriter& writer) const
	{
		writer.Write(moveSpeed);
		writer.Write(jumpForce);
		writer.Write(coyoteTime);
		writer.Write(coyoteTimer);
		writer.Write(jumpBufferTime);
		writer.Write(jumpBufferTimer);
		writer.Write(spawnPosition);
	}

	void Deserialize(DataReader& reader)
	{
		reader.Read(moveSpeed);
		reader.Read(jumpForce);
		reader.Read(coyoteTime);
		reader.Read(coyoteTimer);
		reader.Read(jumpBufferTime);
		reader.Read(jumpBufferTimer);
		reader.Read(spawnPosition);
	}
};

struct NH_API Camera
{
	glm::mat4 projection;
	glm::mat4 view;

	void Serialize(DataWriter& writer) const
	{
		writer.Write(projection);
		writer.Write(view);
	}

	void Deserialize(DataReader& reader)
	{
		reader.Read(projection);
		reader.Read(view);
	}
};

struct NH_API CameraTarget
{
	U32 targetEntity = U32_MAX;
	glm::vec2 offset{ 0.0f, 0.0f };
	F32 smoothSpeed = 15.0f;

	void Serialize(DataWriter& writer) const
	{
		writer.Write(targetEntity);
		writer.Write(offset);
		writer.Write(smoothSpeed);
	}

	void Deserialize(DataReader& reader)
	{
		reader.Read(targetEntity);
		reader.Read(offset);
		reader.Read(smoothSpeed);
	}
};

struct NH_API AudioEmitter
{
	PlaybackHandle handle;
	glm::vec2 prevPosition{ 0.0f, 0.0f };

	void Serialize(DataWriter& writer) const
	{
		writer.Write(handle);
		writer.Write(prevPosition);
	}

	void Deserialize(DataReader& reader)
	{
		reader.Read(handle);
		reader.Read(prevPosition);
	}
};

struct NH_API NoSerialization
{
	void Serialize(DataWriter& writer) const {}
	void Deserialize(DataReader& reader) {}
};