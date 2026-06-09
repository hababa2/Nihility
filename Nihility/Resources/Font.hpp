#pragma once

#include "Defines.hpp"

#include "Core/Containers.hpp"
#include "Resources/Texture.hpp"

static constexpr U32 FontVersion = MakeVersionNumber(1, 0, 0);
static constexpr U32 FontIdentifier = 'NHF';

struct FontFileHeader
{
	U32 identifier = FontIdentifier;
	U32 version = FontVersion;

	F32 lineHeight;
	F32 scale;
	U32 glyphSize;

	U32 textureWidth;
	U32 textureHeight;
};

struct Glyph
{
	F32 advance = 1.0f;
	F32 leftBearing = 0.0f;
	F32 y = 0.0f;
	F32 x = 0.0f;

	F32 kerning[96]{ 0.0f };
};

namespace msdfgen { class Shape; }
struct stbtt_fontinfo;

struct NH_API Font
{
private:
	struct Indices
	{
		U32 start, end;
	};

public:
	const WString& Name() { return name; }
	const std::shared_ptr<Texture> GetTexture() { return texture; }
	const U32& TextureId() { return texture->Id(); }
	const F32& Scale() { return scale; }
	const U32& GlyphSize() { return glyphSize; }
	const Glyph& GetGlyph(C codePoint) { return glyphs[codePoint]; }

private:
	void LoadData(stbtt_fontinfo* info, U8 glyphSize);
	msdfgen::Shape LoadGlyph(stbtt_fontinfo* info, C codepoint, F32* bitmap, Hashmap<I32, C>& glyphToCodepoint);
	void CreateKerning(stbtt_fontinfo* info, Hashmap<I32, C>& glyphToCodepoint);

	WString name = L"";
	std::shared_ptr<Texture> texture = nullptr;
	F32 scale = 0.0f;
	U32 glyphSize = 0;

	Glyph glyphs[96];

	friend class Resources;
	friend class UI;
};