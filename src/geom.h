#ifndef GEOM_H
#define GEOM_H

#include "core.h"

typedef struct
{
	f32 x, y;
} v2;

typedef union
{
	struct { f32 x, y, z; };
	struct { f32 r, g, b; };
} v3;

typedef union
{
	struct { f32 x, y, z, w; };
	struct { f32 r, g, b, a; };
} v4;

typedef struct
{
	struct
	{
		f32 x0, y0;
		f32 x1, y1;
	};
	f32 m[2][2];
} m22;

typedef struct
{
	v2  pos;
	m22 rot;
} xform2d_t;

typedef union
{
	struct
	{
		f32 x0, y0, z0, w0;
		f32 x1, y1, z1, w1;
		f32 x2, y2, z2, w2;
		f32 x3, y3, z3, w3;
	};
	f32 m[4][4];
} m44;

/* V2 */
inline v2 V2(f32 x, f32 y)
{
	v2 v;
	v.x = x;
	v.y = y;
	return v;
}
inline v2  v2_add(v2 a, v2 b)		{ return V2(a.x+b.x, a.y+b.y); };
inline v2  v2_sub(v2 a, v2 b)  		{ return V2(a.x-b.x, a.y-b.y); };
inline v2  v2_scale(v2 v, f32 s)	{ return V2(v.x*s, v.y*s); };
inline v2  v2_neg(v2 v)  			{ return V2(-v.x, -v.y); };
inline v2  v2_perp(v2 v)			{ return V2(v.y, -v.x); }
inline f32 v2_cross(v2 a, v2 b)		{ return a.x*b.y - a.y*b.x; };
inline f32 v2_dot(v2 a, v2 b)		{ return a.x*b.x + a.y*b.y; };
inline f32 v2_len2(v2 v)			{ return v2_dot(v, v); };
inline v2  v2_norm(v2 v)	
{
	const f32 l2 = v2_len2(v);
	if(l2 > 1e-8f)
		return v2_scale(v, f32_isqrt(l2));
	return v;
}

/* V3 */
inline v3 V3(f32 x, f32 y, f32 z)
{
	v3 v;
	v.x = x;
	v.y = y;
	v.z = z;
	return v;
}

inline v3 v3_cross(v3 a, v3 b)
{
	v3 r;
	r.x = a.y*b.z - a.z*b.y;
	r.y = a.z*b.x - a.x*b.z;
	r.z = a.x*b.y - a.y*b.x;
	return r;
};

/* V4 */
inline v4 V4(f32 x, f32 y, f32 z, f32 w)
{
	v4 v;
	v.x = x;
	v.y = y;
	v.z = z;
	v.w = w;
	return v;
}

/* M22 */
inline m22 m22_identity()
{
	return (m22)
	{{
		1.f, 0.f,
		0.f, 1.f,
	}};
};
inline m22 m22_rotation(f32 theta)
{
	const f32 c = f32_cos(theta);
	const f32 s = f32_sin(theta);
	return (m22) 
	{{
		c,  -s,
		s,   c,
	}};
};
inline v2 m22_transform(m22 m, v2 v)
{
	v2 r;
	r.x = v.x*m.x0 + v.y*m.y0;
	r.y = v.x*m.x1 + v.y*m.y1;
	return r;
};

/* XFORM */
inline xform2d_t xform2d(v2 pos, f32 angle)
{
	xform2d_t xform;
	xform.pos = pos;
	xform.rot = m22_rotation(angle);
	return xform;
};
inline xform2d_t xform2d_id()
{
	xform2d_t xform;
	xform.pos = V2(0.f,0.f);
	xform.rot = m22_identity();
	return xform;
};
inline v2 xform2d_apply(xform2d_t xform, v2 v)
{
	return v2_add(xform.pos, m22_transform(xform.rot, v));
};


/* M44 */
inline m44 m44_identity()
{
	return (m44)
	{{
		1.f, 0.f, 0.f, 0.f,
		0.f, 1.f, 0.f, 0.f,
		0.f, 0.f, 1.f, 0.f,
		0.f, 0.f, 0.f, 1.f,
	}};
};
inline m44 m44_scale(f32 x, f32 y, f32 z)
{
	return (m44)
	{{
		x,   0.f, 0.f, 0.f,
		0.f, y,   0.f, 0.f,
		0.f, 0.f, z,   0.f,
		0.f, 0.f, 0.f, 1.f,
	}};
};
inline m44 m44_rotationZ(f32 theta)
{
	const f32 c = f32_cos(theta);
	const f32 s = f32_sin(theta);
	return (m44)
	{{
		c,  -s,   0.f, 0.f,
		s,   c,   0.f, 0.f,
		0.f, 0.f, 1.f, 0.f,
		0.f, 0.f, 0.f, 1.f,
	}};
};
inline m44 m44_translation(f32 x, f32 y, f32 z)
{
	return (m44)
	{{
		1.f, 0.f, 0.f, 0.f,
		0.f, 1.f, 0.f, 0.f,
		0.f, 0.f, 1.f, 0.f,
		x,   y,   z,   1.f,
	}};
};
inline m44 m44_orthoOffCenter(f32 l, f32 r, f32 b, f32 t, f32 zn, f32 zf)
{
	const f32 sx = (2.f / (r-l));
	const f32 sy = (2.f / (t-b));
	const f32 sz = (1.f / (zf-zn));

	const f32 tx = (l+r)/(l-r);
	const f32 ty = (t+b)/(b-t);
	const f32 tz = zn / (zn - zf);

	return (m44)
	{{
		sx, 0.f, 0.f, 0.f,
		0.f, sy, 0.f, 0.f,
		0.f, 0.f, sz, 0.f,
		tx,  ty,  tz, 1.f,
	}};
};
inline m44 m44_mul(m44 a, m44 b)
{
	m44 out;
	for(u32 i = 0; i < 4; i++)
	{
		for(u32 j = 0; j < 4; j++)
		{
			out.m[i][j] = 0.f;
			for(u32 k = 0; k < 4; k++)
			{
				out.m[i][j] += a.m[k][j] * b.m[i][k];
			};
		};
	};
	return out;
};

typedef struct
{
	v2 min, max;
} aabb_t;

inline f32 aabb_perimeter(aabb_t aabb)
{
	const f32 d_x = aabb.max.x - aabb.min.x;
	const f32 d_y = aabb.max.y - aabb.min.y;
	return 2.f*(d_x+d_y);
};
inline bool aabbs_overlap(aabb_t aabb_a, aabb_t aabb_b)
{
	const bool x_min = (aabb_a.min.x <= aabb_b.max.x); 
	const bool x_max = (aabb_a.max.x >= aabb_b.min.x);
	const bool y_min = (aabb_a.min.y <= aabb_b.max.y); 
	const bool y_max = (aabb_a.max.y >= aabb_b.min.y); 
	return x_min && x_max && y_min && y_max;
};
inline aabb_t aabbs_merge(aabb_t aabb_a, aabb_t aabb_b)
{
	aabb_t aabb;
	aabb.min.x = min(aabb_a.min.x, aabb_b.min.x); 
	aabb.min.y = min(aabb_a.min.y, aabb_b.min.y); 
	aabb.max.x = max(aabb_a.max.x, aabb_b.max.x); 
	aabb.max.y = max(aabb_a.max.y, aabb_b.max.y); 
	return aabb;
};

#endif