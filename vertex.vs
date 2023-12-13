#version 430 core

layout(std140, binding = 0) uniform MapProperties
{								//byte offset
	mat4 projection;			//0
	mat4 view;					//64
	mat4 projview;				//128
	uint numTiles;				//192
	uint tileDims;				//196	
	uint halfTile;				//200
	uint mapW;					//204
	uint xoff;					//208
	uint yoff;					//212
	vec3 layerVisible;			//224
	vec3 pad;					//240
} md;

struct TileCoords 
{
	vec2 coords[4];
};
	
layout(location = 0) in vec4 pos;
layout(location = 1) in uint tileID;	//instanced
layout(location = 2) in uint tileProps;	//instanced

out vec2 tCoord;
flat out uint id;

const TileCoords c[8] = {  {{vec2(0.0, 0.0), vec2(0.0, 1.0), vec2(1.0, 1.0), vec2(1.0, 0.0)}}, 	//NORMAL
						   {{vec2(0.0, 1.0), vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0)}}, 	//FLIP_VERT
						   {{vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0), vec2(0.0, 0.0)}}, 	//FLIP_HOR
						   {{vec2(1.0, 0.0), vec2(0.0, 0.0), vec2(0.0, 1.0), vec2(1.0, 1.0)}}, 	//ROT_90
						   {{vec2(1.0, 1.0), vec2(1.0, 0.0), vec2(0.0, 0.0), vec2(0.0, 1.0)}}, 	//ROT_180
						   {{vec2(0.0, 1.0), vec2(1.0, 1.0), vec2(1.0, 0.0), vec2(0.0, 0.0)}}, 	//ROT_270
						   {{vec2(1.0, 1.0), vec2(0.0, 1.0), vec2(0.0, 0.0), vec2(1.0, 0.0)}}, 	//FLIP_VERT_ROT_90
						   {{vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0)}}};	//FLIP_VERT_ROT_270

void main(void)
{
	//depth adjust
	vec4 p = pos;
	uint layer = gl_InstanceID / md.numTiles;
	uint layerInstance = gl_InstanceID % md.numTiles;
	p.z -= md.layerVisible[layer];

	//construct model matrix from instance id and map data, set position
	mat4 modelmat;
	uint xidx = layerInstance % md.mapW;
	uint yidx = layerInstance / md.mapW;
	float xtrans = float(md.xoff) + float(md.tileDims * xidx) + float(md.halfTile);
	float ytrans = float(md.yoff) + float(md.tileDims * yidx) + float(md.halfTile);
	modelmat[0] = vec4(md.tileDims, 0.0, 0.0, 0.0);
	modelmat[1] = vec4(0.0, md.tileDims, 0.0, 0.0);
	modelmat[2] = vec4(0.0, 0.0, 1.0, 0.0);
	modelmat[3] = vec4(xtrans, ytrans, 0.0, 1.0);

	gl_Position = md.projview * modelmat * p;

	//uint orient = bitfieldExtract(tileProps, 0, 3);
	tCoord = c[tileProps].coords[gl_VertexID];
	id = tileID + ((layer/2) * 255);
}

