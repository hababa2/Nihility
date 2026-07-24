#pragma once

#include "Defines.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/compatibility.hpp>

struct NH_API Color
{
public:
	Color() = default;
	Color(F32 red, F32 green, F32 blue, F32 alpha = 1.0f) :
		r(glm::clamp(red, 0.0f, 1.0f)),
		g(glm::clamp(green, 0.0f, 1.0f)),
		b(glm::clamp(blue, 0.0f, 1.0f)),
		a(glm::clamp(alpha, 0.0f, 1.0f))
	{}

	glm::vec4 ToVec4() const { return { r, g, b, a }; }
	operator glm::vec4() const { return { r, g, b, a }; }

	static Color FromHSV(F32 h, F32 s, F32 v, F32 alpha = 1.0f)
	{
		Color out(0, 0, 0, alpha);
		s = glm::clamp(s, 0.0f, 1.0f);
		v = glm::clamp(v, 0.0f, 1.0f);

		if (s <= 0.0f) { return Color(v, v, v, alpha); }

		h = glm::mod(h, 360.0f);
		if (h < 0.0f) { h += 360.0f; }
		h /= 60.0f;

		I32 i = (I32)glm::floor(h);
		F32 f = h - i;
		F32 p = v * (1.0f - s);
		F32 q = v * (1.0f - s * f);
		F32 t = v * (1.0f - s * (1.0f - f));

		switch (i)
		{
		case 0: { out.r = v; out.g = t; out.b = p; } break;
		case 1: { out.r = q; out.g = v; out.b = p; } break;
		case 2: { out.r = p; out.g = v; out.b = t; } break;
		case 3: { out.r = p; out.g = q; out.b = v; } break;
		case 4: { out.r = t; out.g = p; out.b = v; } break;
		default: { out.r = v; out.g = p; out.b = q; } break;
		}

		return out;
	}

	static Color FromHSV(const glm::vec4& hsv)
	{
		return FromHSV(hsv.r, hsv.g, hsv.b, hsv.a);
	}

	glm::vec4 ToHSV() const
	{
		glm::vec4 hsv;
		hsv.a = a;

		F32 minVal = glm::min(r, glm::min(g, b));
		F32 maxVal = glm::max(r, glm::max(g, b));
		F32 delta = maxVal - minVal;

		hsv.b = maxVal;

		if (delta < 0.00001f)
		{
			hsv.r = 0.0f;
			hsv.g = 0.0f;
			return hsv;
		}

		hsv.g = maxVal > 0.0f ? (delta / maxVal) : 0.0f;

		if (r >= maxVal) { hsv.r = (g - b) / delta; }
		else if (g >= maxVal) { hsv.r = 2.0f + (b - r) / delta; }
		else { hsv.r = 4.0f + (r - g) / delta; }

		hsv.r *= 60.0f;
		if (hsv.r < 0.0f) { hsv.r += 360.0f; }

		return hsv;
	}

	Color MultiplySaturation(F32 percentMultiplier) const
	{
		glm::vec4 hsv = ToHSV();
		hsv.g *= percentMultiplier;
		return FromHSV(hsv);
	}

	Color MultiplyValue(F32 percentMultiplier) const
	{
		glm::vec4 hsv = ToHSV();
		hsv.b *= percentMultiplier;
		return FromHSV(hsv);
	}

	F32 PerceivedBrightness() const
	{
		F32 squaredR = r * r;
		F32 squaredG = g * g;
		F32 squaredB = b * b;

		return glm::sqrt(0.299f * squaredR + 0.587f * squaredG + 0.114f * squaredB);
	}

	Color GetContrastTextColor() const
	{
		return PerceivedBrightness() > 0.65f ? Color(0.0f, 0.0f, 0.0f, 1.0f) : Color(1.0f, 1.0f, 1.0f, 1.0f);
	}

	static Color Lerp(const Color& a, const Color& b, F32 t)
	{
		glm::vec4 hsvA = a.ToHSV();
		glm::vec4 hsvB = b.ToHSV();

		t = glm::clamp(t, 0.0f, 1.0f);
		return FromHSV(glm::lerp(hsvA, hsvB, t));
	}

public:
	F32 r = 1.0f;
	F32 g = 1.0f;
	F32 b = 1.0f;
	F32 a = 1.0f;
};