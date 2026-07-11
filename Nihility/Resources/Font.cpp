#include "Font.hpp"

#include "stb/stb_truetype.h"

#include "msdfgen/msdfgen.h"

void Font::LoadData(stbtt_fontinfo* info, U8 size)
{
	glyphSize = size;

	scale = stbtt_ScaleForMappingEmToPixels(info, (F32)glyphSize);
}

msdfgen::Shape Font::LoadGlyph(stbtt_fontinfo* info, C codepoint, F32* bitmap, Hashmap<I32, C>& glyphToCodepoint)
{
	msdfgen::Shape shape;

	Glyph& glyph = glyphs[codepoint - 32];

	I32 index = stbtt_FindGlyphIndex(info, codepoint);
	glyphToCodepoint.insert({ index, codepoint - 32 });

	I32 advance, leftBearing;
	stbtt_GetGlyphHMetrics(info, index, &advance, &leftBearing);
	glyph.advance = advance * scale;

	I32 x0, y0, x1, y1;
	stbtt_GetGlyphBox(info, index, &x0, &y0, &x1, &y1);

	F32 translateX = glyphSize * 0.5f - ((x1 - x0) * scale) * 0.5f - x0 * scale;
	F32 translateY = glyphSize * 0.5f - ((y1 - y0) * scale) * 0.5f - y0 * scale;

	glyph.x = translateX;
	glyph.y = translateY;

	stbtt_vertex* verts;
	I32 vertexCount = stbtt_GetGlyphShape(info, index, &verts);

	I32 contourCount = 0;
	for (I32 i = 0; i < vertexCount; ++i)
	{
		if (verts[i].type == STBTT_vmove) { ++contourCount; }
	}

	if (contourCount == 0) { return shape; }

	Indices* contours;
	Memory::Allocate(&contours, contourCount);

	I32 j = 0;
	for (I32 i = 0; i <= vertexCount; ++i)
	{
		if (verts[i].type == STBTT_vmove)
		{
			if (i > 0)
			{
				contours[j].end = i;
				++j;
			}

			contours[j].start = i;
		}
		else if (i >= vertexCount) { contours[j].end = i; }
	}

	msdfgen::Vector2 initial = { 0, 0 };
	F32 cscale = 64.0f;

	for (I32 i = 0; i < contourCount; ++i)
	{
		msdfgen::Contour& contour = shape.addContour();

		for (U32 j = contours[i].start; j < contours[i].end; ++j)
		{
			stbtt_vertex* v = &verts[j];

			switch (v->type)
			{
			case STBTT_vmove: { initial = { v->x / cscale, v->y / cscale }; } break;
			case STBTT_vline: {
				msdfgen::Vector2 p = { v->x / cscale, v->y / cscale };

				contour.addEdge(msdfgen::EdgeHolder(initial, p));

				initial = p;
			} break;
			case STBTT_vcurve: {
				msdfgen::Vector2 p = { v->x / cscale, v->y / cscale };
				msdfgen::Vector2 c = { v->cx / cscale, v->cy / cscale };

				if ((initial.x == c.x && initial.y == c.y) || (c.x == p.x && c.y == p.y)) { c = (initial + p) * 0.5f; }

				contour.addEdge(msdfgen::EdgeHolder(initial, c, p));

				initial = p;
			} break;
			case STBTT_vcubic: {
				msdfgen::Vector2 p = { v->x / cscale, v->y / cscale };
				msdfgen::Vector2 c = { v->cx / cscale, v->cy / cscale };
				msdfgen::Vector2 c1 = { v->cx1 / cscale, v->cy1 / cscale };

				contour.addEdge(msdfgen::EdgeHolder(initial, c, c1, p));

				initial = p;
			} break;
			}
		}
	}

	Memory::Free(&contours);

	return shape;
}

void Font::CreateKerning(stbtt_fontinfo* info, Hashmap<I32, C>& glyphToCodepoint)
{
	I32 length = stbtt_GetKerningTableLength(info);

	stbtt_kerningentry* kerningTable;
	Memory::Allocate(&kerningTable, length);

	stbtt_GetKerningTable(info, kerningTable, length);

	I32 lastGlyph = 0;

	U8 codepoint = 255;

	for (I32 i = 0; i < length; ++i)
	{
		stbtt_kerningentry& entry = kerningTable[i];

		auto it = glyphToCodepoint.find(entry.glyph1);
		if (it == glyphToCodepoint.end() || it->second < 32 || it->second > 127) { continue; }
		codepoint = it->second - 32;

		it = glyphToCodepoint.find(entry.glyph1);
		if (it == glyphToCodepoint.end() || it->second < 32 || it->second > 127 || (it->second >= '0' && it->second <= '9')) { continue; }

		glyphs[codepoint].kerning[it->second - 32] = (F32)entry.advance;
	}
}