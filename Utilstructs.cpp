#include "Utilstructs.h"

float& vec4::operator[](std::size_t n) {
	float* ret = &x;
	switch (n) {
	case 1:
		ret = &y;
		break;
	case 2:
		ret = &z;
		break;
	case 3:
		ret = &w;
		break;
	}
	return *ret;
}

const float& vec4::operator[](std::size_t n) const {
	const float* ret = &x;
	switch (n) {
	case 1:
		ret = &y;
		break;
	case 2:
		ret = &z;
		break;
	case 3:
		ret = &w;
		break;
	}
	return *ret;
}

vec4& mat4::operator[](std::size_t n) {
	vec4* ret = &x;
	switch (n) {
	case 1:
		ret = &y;
		break;
	case 2:
		ret = &z;
		break;
	case 3:
		ret = &w;
		break;
	}
	return *ret;
}

const vec4& mat4::operator[](std::size_t n) const {
	const vec4* ret = &x;
	switch (n) {
	case 1:
		ret = &y;
		break;
	case 2:
		ret = &z;
		break;
	case 3:
		ret = &w;
		break;
	}
	return *ret;
}

vec2 operator+(const vec2& l, const vec2& r) {
	vec2 ret = l;
	ret.x += r.x;	ret.y += r.y;
	return ret;
}

vec2 operator-(const vec2& l, const vec2& r) {
	vec2 ret = l;
	ret.x -= r.x;	ret.y -= r.y;
	return ret;
}

bool operator==(const vec2& l, const vec2& r) {
	return l.x == r.x && l.y == r.y;
}

bool operator!=(const vec2& l, const vec2& r) {
	return l.x != r.x || l.y != r.y;
}

uvec2 operator+(const uvec2& l, const uvec2& r) {
	uvec2 ret = l;
	ret.x += r.x;	ret.y += r.y;
	return ret;
}

uvec2 operator-(const uvec2& l, const uvec2& r) {
	uvec2 ret = l;
	ret.x -= r.x;	ret.y -= r.y;
	return ret;
}

bool operator==(const uvec2& l, const uvec2& r) {
	return l.x == r.x && l.y == r.y;
}

bool operator!=(const uvec2& l, const uvec2& r) {
	return l.x != r.x || l.y != r.y;
}

mat4 operator*(const mat4& l, const mat4& r) {
	mat4 ret;
	vec4 lrow1 = { l.x.x, l.y.x, l.z.x, l.w.x };
	vec4 lrow2 = { l.x.y, l.y.y, l.z.y, l.w.y };
	vec4 lrow3 = { l.x.z, l.y.z, l.z.z, l.w.z };
	vec4 lrow4 = { l.x.w, l.y.w, l.z.w, l.w.w };
	vec4 lparts[4] = { lrow1, lrow2, lrow3, lrow4 };
	vec4 rparts[4] = { r.x, r.y, r.z, r.w };

	for (unsigned row = 0; row < 4; ++row) {
		for (unsigned col = 0; col < 4; ++col) {
			vec4 lrow, rcol;
			rcol = rparts[col];
			lrow = lparts[row];

			ret[col][row] = lrow.x * rcol.x + lrow.y * rcol.y + lrow.z * rcol.z + lrow.w * rcol.w;
		}
	}

	return ret;
}