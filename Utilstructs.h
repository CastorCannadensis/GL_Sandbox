#ifndef BEAVER_BUILDER_UTILSTRUCTS
#define BEAVER_BUILDER_UTILSTRUCTS

#include <cstdlib>

struct vec2 {
	vec2() : x(0), y(0) { }
	vec2(unsigned xx, unsigned yy) : x(xx), y(yy) { }
	vec2(const vec2& c) : x(c.x), y(c.y) { }
	vec2& operator=(const vec2& c) { x = c.x; y = c.y; return *this; }
	float x;
	float y;
};

struct uvec2 {
	uvec2() : x(0), y(0) { }
	uvec2(unsigned xx, unsigned yy) : x(xx), y(yy) { }
	uvec2(const uvec2& c) : x(c.x), y(c.y) { }
	uvec2& operator=(const uvec2& c) { x = c.x; y = c.y; return *this; }
	unsigned x;
	unsigned y;
};

struct vec3 {
	float x;
	float y;
	float z;
};

struct vec4 {
	vec4() : x(0.0), y(0.0), z(0.0), w(0.0) { }
	vec4(float r, float g, float b, float a) : x(r), y(g), z(b), w(a) { }
	vec4(const vec4& v) : x(v.x), y(v.y), z(v.z), w(v.w) { }
	vec4& operator=(const vec4& v) { x = v.x; y = v.y; z = v.z; w = v.w; return *this; }
	float& operator[](std::size_t n);
	const float& operator[](std::size_t n) const;

	float x;
	float y;
	float z;
	float w;
};

struct mat4 {
	mat4() { }
	mat4(vec4 r, vec4 g, vec4 b, vec4 a) : x(r), y(g), z(b), w(a) { }
	mat4(const mat4& m) : x(m.x), y(m.y), z(m.z), w(m.w) { }
	mat4& operator=(const mat4& m) { x = m.x; y = m.y; z = m.z; w = m.w; return *this; }
	vec4& operator[](std::size_t n); 
	const vec4& operator[](std::size_t n) const;

	vec4 x;
	vec4 y;
	vec4 z;
	vec4 w;
};

vec2 operator+(const vec2& l, const vec2& r);
vec2 operator-(const vec2& l, const vec2& r);
bool operator==(const vec2& l, const vec2& r);
bool operator!=(const vec2& l, const vec2& r);
uvec2 operator+(const uvec2& l, const uvec2& r);
uvec2 operator-(const uvec2& l, const uvec2& r);
bool operator==(const uvec2& l, const uvec2& r);
bool operator!=(const uvec2& l, const uvec2& r);
mat4 operator*(const mat4& l, const mat4& r);

#endif
