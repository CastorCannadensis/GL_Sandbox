#include <GL\glew.h>
#include "Display.h"
#include "Debug.h"
#include <fstream>
#include "Utilstructs.h"
#include <cstdlib>
#include <map>
#include <CImg.h>

Display::Display(wxWindow* p, const int* attribs) : wxGLCanvas(p, wxID_ANY, attribs, wxPoint(0, 0)),
													panning(false), lastPos({ 0, 0 }), 
													viewTrans({ 0, 0 }), viewZoom(1.0),
													layer1(nullptr), layer2(nullptr), 
											        solids(nullptr), context(this) {
	//map data for rendering
	md.tileDims = 32;
	md.chunkSize = 16;
	md.ntChunk = md.chunkSize * md.chunkSize;
	md.ppChunk = md.chunkSize * md.tileDims;
	md.mapW = 512;	md.mapH = 512;
	md.mapWPix = md.mapW * md.tileDims;
	md.mapHPix = md.mapH * md.tileDims;
	md.nTiles = md.mapW * md.mapH;
	md.nTilesRendered = 1;
	md.cxSlack = (md.mapW % md.chunkSize > 0) ? md.chunkSize - (md.mapW % md.chunkSize) : 0;
	md.cySlack = (md.mapH % md.chunkSize > 0) ? md.chunkSize - (md.mapH % md.chunkSize) : 0;
	md.maxChunkX = (md.mapW - 1) / md.chunkSize;
	md.maxChunkY = (md.mapH - 1) / md.chunkSize;
	md.tlc.x = 2; md.trc.x = 1;
	md.vis = vec4(0.0, 0.0, 0.0, 0.0);

	//GL Setup
	SetCurrent(context);
	glewExperimental = GL_TRUE;
	GLenum err = glewInit();
	
	if (err != GLEW_OK) {
		Debug::log("ERROR: GLEW is NOT okay");
	}
	if (GLEW_VERSION_4_3) {
		Debug::log("GL Version 4.3 Core Supported!");
	}
	else {
		Debug::log("ERROR: GL Version 4.3 Core NOT Supported");
	}

	disW = this->GetSize().GetWidth();
	disH = this->GetSize().GetHeight();
	glViewport(0, 0, disW, disH);
	glClearColor(0.75, 0.15, 0.75, 1.0);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquation(GL_FUNC_ADD);

	int v;
	glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &v);
	Debug::log("MAX ARRAY TEXTURE LAYERS: " + std::to_string(v));
	_loadPrograms();
	_setup();
	_performanceTest();

	//Event Handling
	this->Bind(wxEVT_RIGHT_DOWN, &Display::_OnRightDown, this);
	this->Bind(wxEVT_RIGHT_UP, &Display::_OnRightUp, this);
	this->Bind(wxEVT_LEFT_DOWN, &Display::_OnLeftDown, this);
	this->Bind(wxEVT_LEFT_UP, &Display::_OnLeftUp, this);
	this->Bind(wxEVT_LEFT_DCLICK, &Display::_OnLeftDClick, this);
	this->Bind(wxEVT_MOTION, &Display::_OnMouseMove, this);
	this->Bind(wxEVT_MOUSEWHEEL, &Display::_OnMousewheel, this);
	this->Bind(wxEVT_SET_FOCUS, &Display::_OnFocus, this);

	render();
}

Display::~Display() {
	delete[] layer1;
	delete[] layer2;
	delete[] solids;
	glDeleteBuffers(NUM_BUFFERS, buffers);
	glDeleteTextures(NUM_BAKEDIN_TEXTURES, textures);
	glDeleteQueries(31, queries);
	glDeleteVertexArrays(2, vaos);
	glDeleteProgram(programs[0]);
	glDeleteProgram(programs[1]);
}

void Display::_OnRightDown(wxMouseEvent& e) {
	panning = true;
	lastPos = e.GetPosition();
	e.Skip();
}

void Display::_OnRightUp(wxMouseEvent& e) {
	panning = false;
	lastPos = e.GetPosition();
	e.Skip();
}

void Display::_OnLeftDown(wxMouseEvent& e) {

	wxPoint sPos = e.GetPosition();
	wxPoint wPos;
	wPos.x = (sPos.x - viewTrans.x) / viewZoom;
	wPos.y = (sPos.y - viewTrans.y) / viewZoom;

	if (wPos.x > 0 && wPos.x < md.mapWPix && wPos.y > 0 && wPos.y < md.mapHPix) {
		Debug::log("**Clicked Tile: " + std::to_string(wPos.x / md.tileDims) + ", " + std::to_string(wPos.y / md.tileDims));
	}

	_performanceTest();
	e.Skip();
}

void Display::_OnLeftUp(wxMouseEvent& e) {

	e.Skip();
}

void Display::_OnLeftDClick(wxMouseEvent& e) {

	e.Skip();
}

void Display::_OnMouseMove(wxMouseEvent& e) {
	wxPoint p = e.GetPosition();
	if (panning && !e.RightIsDown()) {
		panning = false;
		lastPos = p;
	}
	if (panning) {
		int dX = p.x - lastPos.x;
		int dY = p.y - lastPos.y;
		lastPos = p;

		viewTrans.x += dX;
		viewTrans.y += dY;
		_setView();
		render();
	}
	e.Skip();
}

void Display::_OnMousewheel(wxMouseEvent& e) {

	int delta = e.GetWheelDelta();
	int rotation = e.GetWheelRotation();
	wxPoint mPos = e.GetPosition();
	wxPoint wPos;
	wPos.x = (mPos.x - viewTrans.x) / viewZoom;
	wPos.y = (mPos.y - viewTrans.y) / viewZoom;

	bool z = false;
	if (rotation > 0.0 && viewZoom <= 9.75) {
		z = true;
		viewZoom += 0.25;
	}
	else if (rotation < 0.0 && viewZoom >= 0.5) {
		z = true;
		viewZoom -= 0.25;
	}

	if (viewZoom >= 5.0 && md.chunkSize != 8) _setChunkSize(8);
	else if (viewZoom < 5.0 && viewZoom >= 0.25 && md.chunkSize != 16) _setChunkSize(16);

	if (z) {
		viewTrans.x = -wPos.x * viewZoom + mPos.x;
		viewTrans.y = -wPos.y * viewZoom + mPos.y;
		_setView();
		render();
	}

	e.Skip();
}

void Display::_OnFocus(wxFocusEvent& e) {
	focus();
	e.Skip();
}

void Display::focus() {
	resize();
	render();
}

void Display::resize() {
	disW = this->GetSize().GetWidth();
	disH = this->GetSize().GetHeight();
	glViewport(0, 0, disW, disH);
	Debug::log("WINDOW RESIZE: " + std::to_string(disW) + ", " + std::to_string(disH));
	_setProjection();
	_updateChunks();
}

void Display::render() {
	static GLuint fidx = 0;
	glClear(GL_COLOR_BUFFER_BIT);

	if (md.vis.z != 0.0)
		glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0, md.nTilesRendered * 2);
	else
		glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0, md.nTilesRendered * 3);

	SwapBuffers();
}

void Display::_loadPrograms() {
	GLuint vs = _loadShader(GL_VERTEX_SHADER, "vertex.vs");
	GLuint fs = _loadShader(GL_FRAGMENT_SHADER, "fragment.fs");

	programs[0] = glCreateProgram();
	glAttachShader(programs[0], vs);
	glAttachShader(programs[0], fs);
	glLinkProgram(programs[0]);
	glUseProgram(programs[0]);

	GLint err, logl;
	glGetProgramiv(programs[0], GL_LINK_STATUS, &err);
	if (err != GL_TRUE) {
		glGetProgramiv(programs[0], GL_INFO_LOG_LENGTH, &logl);
		char* buff = (logl > 0) ? new char[logl] : nullptr;
		glGetProgramInfoLog(programs[0], logl, NULL, buff);

		Debug::log("ERROR: Rendering program failed to link!");
		Debug::log("-------------------LINKER INFO LOG:-------------------");
		Debug::log(buff ? buff : "");
		Debug::log("------------------------------------------------------");

		if (buff) delete[] buff;
	}

	glDeleteShader(vs);
	glDeleteShader(fs);
}

GLuint Display::_loadShader(GLenum type, std::string f) {
	GLuint shader = 0;
	std::ifstream file(f);

	file.seekg(0, file.end);
	file.clear();
	int sz = file.tellg();
	file.seekg(0, file.beg);

	char * buff = (sz > 0) ? new char[sz + 1] : nullptr;
	int readSz = 0;
	if (buff) {
		file.read(buff, sz);
		readSz = file.gcount();
		buff[readSz] = '\0';
	}

	file.close();

	shader = glCreateShader(type);
	glShaderSource(shader, 1, &buff, &readSz);
	glCompileShader(shader);

	GLint err;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &err);
	if (err != GL_TRUE) {
		Debug::log("ERROR: Failed to compile shader " + f);
		Debug::log("-------------------SHADER BUFFER DUMP:---------------------");
		Debug::log(buff ? buff : "");
		Debug::log("-----------------------------------------------------------");

		GLint logl;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logl);
		char* logbuff = (logl > 0) ? new char[logl] : nullptr;
		glGetShaderInfoLog(shader, logl, NULL, logbuff);

		Debug::log("-------------------SHADER INFO LOG:------------------------");
		Debug::log(logbuff ? logbuff : "");
		Debug::log("-----------------------------------------------------------");

		if (logbuff) delete[] logbuff;
	}
	else {
		Debug::log("Shader " + f + " compiled successfully");
	}

	if (buff) delete[] buff;
	return shader;
}

void Display::_setProjection() {
	//orthographic projection matrix
	proj = { {2.0f / disW, 0.0f, 0.0f, 0.0f},	//x column
		   {0.0f, 2.0f / -disH, 0.0f, 0.0f},	//y	column
		   {0.0f, 0.0f, 1.0f, 0.0f},			//z	column
		   {-1.0f, 1.0f, 0.0f, 1.0f}, };		//w	column

	glBindBuffer(GL_UNIFORM_BUFFER, buffers[MAP_PROPS_BUFFER]);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(mat4), &proj);
	glBindBuffer(GL_UNIFORM_BUFFER, NULL);

	_setViewProj();
}

void Display::_setView() {
	view = { {viewZoom, 0.0f, 0.0f, 0.0f},					//x column
		   {0.0f, viewZoom, 0.0f, 0.0f},					//y	column
		   {0.0f, 0.0f, 1.0f, 0.0f},						//z	column
		   {viewTrans.x, viewTrans.y, 0.0f, 1.0f}, };	//w	column

	glBindBuffer(GL_UNIFORM_BUFFER, buffers[MAP_PROPS_BUFFER]);
	glBufferSubData(GL_UNIFORM_BUFFER, 64, sizeof(mat4), &view);
	glBindBuffer(GL_UNIFORM_BUFFER, NULL);

	_setViewProj();
	_updateChunks();
}

void Display::_setViewProj() {
	mat4 viewproj = proj * view;

	glBindBuffer(GL_UNIFORM_BUFFER, buffers[MAP_PROPS_BUFFER]);
	glBufferSubData(GL_UNIFORM_BUFFER, 128, sizeof(mat4), &viewproj);
	glBindBuffer(GL_UNIFORM_BUFFER, NULL);
}

void Display::_loadTilesheet(std::string f) {
	cimg_library::CImg<uint8_t> image(f.c_str());

	//fill big buffer
	int imW = image.width();
	int imH = image.height();
	unsigned wInTiles = imW / md.tileDims;
	unsigned hInTiles = imH / md.tileDims;
	unsigned nTiles = wInTiles * hInTiles;
	GLuint* buff = new GLuint[imW * imH];

	//for each tile
	for (int n = 0; n < nTiles; ++n) {

		//get index
		unsigned tileX = (n % wInTiles);
		unsigned tileY = (n / wInTiles);
		unsigned xoff = tileX * md.tileDims;
		unsigned yoff = tileY * md.tileDims;
	
		//load tile into buffer
		for (int y = yoff; y < yoff + md.tileDims; ++y) {
			for (int x = xoff; x < xoff + md.tileDims; ++x) {
				int idx = (n * md.tileDims * md.tileDims) + ((y-yoff) * md.tileDims + (x-xoff));

				GLuint pix;
				uint8_t* p = (uint8_t*)&pix;
				int invY = yoff + md.tileDims - 1 - (y - yoff);
				p[0] = image(x, invY, 0, 0);
				p[1] = image(x, invY, 0, 1);
				p[2] = image(x, invY, 0, 2);
				p[3] = (p[0] == 255 && p[1] == 0 && p[2] == 255) ? 0 : 255;

				buff[idx] = pix;
			}
		}
	}
	
	glGenTextures(1, &textures[TILESHEET_TEX]);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, textures[TILESHEET_TEX]);
	
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, md.tileDims, md.tileDims, 257);
	//user-loaded tiles
	for (unsigned nt = 0; nt < nTiles; ++nt) {
		unsigned offset = (nt * md.tileDims * md.tileDims);
		glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, nt, md.tileDims, md.tileDims, 1,
			GL_RGBA, GL_UNSIGNED_BYTE, buff + offset);
	}
	delete[] buff;

	//transparent tile and solid tile
	GLuint* tbuff = new GLuint[md.tileDims * md.tileDims];
	GLuint* sbuff = new GLuint[md.tileDims * md.tileDims];
	GLuint s; uint8_t* sb = (uint8_t*)&s;
	GLuint t; uint8_t* tb = (uint8_t*)&t;
	sb[0] = 255;
	sb[1] = 0;
	sb[2] = 255;
	sb[3] = 255;

	tb[0] = 0;
	tb[1] = 0;
	tb[2] = 0;
	tb[3] = 0;
	for (unsigned i = 0; i < md.tileDims * md.tileDims; ++i) {
		tbuff[i] = t;
		sbuff[i] = s;
	}
	glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 255, md.tileDims, md.tileDims, 1, 
			GL_RGBA, GL_UNSIGNED_BYTE, tbuff);
	glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 256, md.tileDims, md.tileDims, 1, 
			GL_RGBA, GL_UNSIGNED_BYTE, sbuff);

	delete[] tbuff;
	delete[] sbuff;
}

void Display::_fillMap() {
	layer1 = new uint16_t[md.mapW * md.mapH];
	layer2 = new uint16_t[md.mapW * md.mapH];
	solids = new uint16_t[md.mapW * md.mapH];

	for (unsigned y = 0; y < md.mapH; ++y) {
		for (unsigned x = 0; x < md.mapW; ++x) {
			unsigned idx = y * md.mapW + x;
			uint16_t val1, val2, val3;
			uint8_t* val1s = (uint8_t*)&val1;
			uint8_t* val2s = (uint8_t*)&val2;
			uint8_t* val3s = (uint8_t*)&val3;
			uint8_t v = rand() % 7;
			val1s[0] = (v == 3) ? 4 : v;
			val1s[1] = rand() % 8;
			val2s[0] = (rand() % 2) ? 3 : 7;
			val2s[1] = 0;
			val3s[0] = rand() % 2;
			val3s[1] = 0;

			layer1[idx] = val1;
			layer2[idx] = val2;
			solids[idx] = val3;
		}
	}

}

void Display::_setup() {
	glGenVertexArrays(2, vaos);
	glBindVertexArray(vaos[0]);
	glGenBuffers(NUM_BUFFERS, buffers);
	glGenQueries(31, queries);

	//texture limit data
	glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxSimTex);
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
	glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxTexLayers);

	//vertex data for quad					  Renders as:
	vec4 q[4] = { {-0.5, 0.5, 0.0, 1.0},	//0: bottom left
				  {-0.5, -0.5, 0.0, 1.0},	//1: top left
				  {0.5, -0.5, 0.0, 1.0},	//2: top right
				  {0.5, 0.5, 0.0, 1.0} };	//3: bottom right
	GLubyte qi[6] = { 0, 1, 2, 2, 3, 0 };
	glBindBuffer(GL_ARRAY_BUFFER, buffers[QUAD_BUFFER]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vec4) * 4, q, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, NULL);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[QUAD_INDEX_BUFFER]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLubyte) * 6, qi, GL_STATIC_DRAW);

	//map and instanced tile properties buffer
	_fillMap();
	glBindBuffer(GL_ARRAY_BUFFER, buffers[TILE_PROPS_BUFFER]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(uint16_t) * md.nTiles * 3, NULL, GL_DYNAMIC_DRAW);

	glVertexAttribIPointer(1, 1, GL_UNSIGNED_BYTE, sizeof(uint16_t), 0);
	glVertexAttribIPointer(2, 1, GL_UNSIGNED_BYTE, sizeof(uint16_t), (GLvoid*)sizeof(uint8_t));
	glVertexAttribDivisor(1, 1);
	glVertexAttribDivisor(2, 1);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	glBindBuffer(GL_ARRAY_BUFFER, NULL);

	_loadTilesheet("C:/Game Stuff/Assets/testsheet.png");

	//map data uniform buffer
	unsigned half = md.tileDims / 2;
	md.vis = { 0.0, 0.0, -2.0, 0.0 };
	unsigned xoff = 0; unsigned yoff = 0;
	unsigned nt = 1;   unsigned mw = 1;
	glBindBuffer(GL_UNIFORM_BUFFER, buffers[MAP_PROPS_BUFFER]);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(mat4) * 3 + sizeof(unsigned) * 6 + sizeof(vec4) * 2, 
		NULL, GL_DYNAMIC_DRAW);
	glBufferSubData(GL_UNIFORM_BUFFER, 192, sizeof(unsigned), &nt);
	glBufferSubData(GL_UNIFORM_BUFFER, 196, sizeof(unsigned), &md.tileDims);
	glBufferSubData(GL_UNIFORM_BUFFER, 200, sizeof(unsigned), &half);
	glBufferSubData(GL_UNIFORM_BUFFER, 204, sizeof(unsigned), &mw);
	glBufferSubData(GL_UNIFORM_BUFFER, 208, sizeof(unsigned), &xoff);
	glBufferSubData(GL_UNIFORM_BUFFER, 212, sizeof(unsigned), &yoff);
	glBufferSubData(GL_UNIFORM_BUFFER, 224, sizeof(vec4), &md.vis);
	_setProjection();
	_setView();

	glBindBufferBase(GL_UNIFORM_BUFFER, 0, buffers[MAP_PROPS_BUFFER]);
}

void Display::_performanceTest() {
	static unsigned it = 0;
	GLint st, lat;
	glQueryCounter(GL_TIMESTAMP, queries[0]);
	glGetQueryObjectiv(queries[0], GL_QUERY_RESULT, &st);
	glQueryCounter(GL_TIMESTAMP, queries[1]);
	glGetQueryObjectiv(queries[1], GL_QUERY_RESULT, &lat);
	lat -= st;

	Debug::log("--------------------PERFORMANCE TEST " + std::to_string(it) + "---------------------------");
	Debug::log("Translation: " + std::to_string(viewTrans.x) + ", " + std::to_string(viewTrans.y));
	Debug::log("Scale factor: " + std::to_string(viewZoom));
	Debug::log("Chunk Size: " + std::to_string(md.chunkSize));
	Debug::log("Tiles Rendered per Layer: " + std::to_string(md.nTilesRendered));
	Debug::log("Chunks Rendered: " + std::to_string(md.nTilesRendered / md.ntChunk));
	Debug::log("Pipeline Latency: " + std::to_string(lat) + "  (" + std::to_string(lat / 1000000) + "ms)");

	for (int i = 2; i < 32; ++i) {
		glBeginQuery(GL_TIME_ELAPSED, queries[i]);
		render();
		glEndQuery(GL_TIME_ELAPSED);
	}
	GLuint64 sum = 0;
	for (int j = 2; j < 32; ++j) {
		GLuint nano;
		glGetQueryObjectuiv(queries[j], GL_QUERY_RESULT, &nano);
		sum += nano;
		Debug::log("Render Time " + std::to_string(j - 1) + ": " + std::to_string(nano) + "  (" + std::to_string(nano / 1000000) + "ms)");
	}
	Debug::log("Average Render Time: " + std::to_string(sum / 30) + "  (" + std::to_string((sum/30)/1000000) + "ms)");
	Debug::log("----------------------------------------------------------");
	it++;
}

void Display::_updateChunks() {
	clock.beginTimer();
	//get map bounds in view space
	vec2 tl, bl, tr;
	tl = viewTrans;
	bl.x = viewTrans.x;							bl.y = md.mapHPix * viewZoom + viewTrans.y;
	tr.x = md.mapWPix * viewZoom + viewTrans.x;	tr.y = viewTrans.y;
	 
	//no viewport intersection with map, set rendered tiles to 0
	if (tl.y > disH || bl.y < 0 || tl.x > disW || tr.x < 0) {
		md.tlc.x = 2; md.trc.x = 1;		//set nonsense chunk data to function as null
		md.nTilesRendered = 0;
		clock.endTimer();
		return;
	}
	//otherwise find active chunks
	uvec2 tlChunk, blChunk, trChunk;
	tlChunk.x = (tl.x >= 0) ? 0 : (-tl.x / viewZoom) / md.ppChunk;
	tlChunk.y = (tl.y >= 0) ? 0 : (-tl.y / viewZoom) / md.ppChunk;
	trChunk.x = (tr.x <= disW) ? md.maxChunkX : ((disW - viewTrans.x) / viewZoom) / md.ppChunk;
	trChunk.y = tlChunk.y;
	blChunk.x = tlChunk.x;
	blChunk.y = (bl.y <= disH) ? md.maxChunkY : ((disH - viewTrans.y) / viewZoom) / md.ppChunk;

	//if this is a new set of chunks, load new data to GPU
	if (tlChunk == md.tlc && blChunk == md.blc && trChunk == md.trc) {
		double t = clock.endTimer();
		if (t > 0.003) Debug::log("^^^CHUNKCHECK^^^ Same chunk set determined, time taken: " + std::to_string(t));
		return;
	}
	md.tlc = tlChunk; md.blc = blChunk; md.trc = trChunk;

	unsigned nch = (md.blc.y - md.tlc.y + 1);
	unsigned ncw = (md.trc.x - md.tlc.x + 1);
	unsigned tw = ncw * md.chunkSize;
	unsigned nt = ncw * nch * md.ntChunk;
	if (md.trc.x == md.maxChunkX) {
		nt -= md.cxSlack * nch;
		tw -= md.cxSlack;
	}
	if (md.blc.y == md.maxChunkY) {
		nt -= md.cySlack * ncw;
	}
	glBindBuffer(GL_ARRAY_BUFFER, buffers[TILE_PROPS_BUFFER]);
	void* buff = glMapBufferRange(GL_ARRAY_BUFFER, 0, nt * 3 * sizeof(uint16_t), 
		GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

	uint16_t* ubuff = (uint16_t*)buff;
	unsigned yLim = md.blc.y * md.chunkSize + md.chunkSize;
	unsigned xLim = md.trc.x * md.chunkSize + md.chunkSize;
	unsigned lay1Idx = 0; unsigned lay2Idx = nt; unsigned lay3Idx = nt * 2;
	unsigned y = md.tlc.y * md.chunkSize;
	unsigned x = md.tlc.x * md.chunkSize;
	unsigned yoff = y * md.tileDims;
	unsigned xoff = x * md.tileDims;
	for (; y < md.mapH && y < yLim; ++y) {
		for (x = md.tlc.x * md.chunkSize; x < md.mapW && x < xLim; ++x) {
			unsigned mapIdx = y * md.mapW + x;
			ubuff[lay1Idx] = layer1[mapIdx];
			ubuff[lay2Idx] = layer2[mapIdx];
			ubuff[lay3Idx] = solids[mapIdx];
			++lay1Idx; ++lay2Idx; ++lay3Idx;
		}
	}

	glUnmapBuffer(GL_ARRAY_BUFFER);
	glBindBuffer(GL_ARRAY_BUFFER, NULL);
	md.nTilesRendered = nt;
	
	glBindBuffer(GL_UNIFORM_BUFFER, buffers[MAP_PROPS_BUFFER]);
	glBufferSubData(GL_UNIFORM_BUFFER, 192, sizeof(unsigned), &nt);		
	glBufferSubData(GL_UNIFORM_BUFFER, 204, sizeof(unsigned), &tw);		
	glBufferSubData(GL_UNIFORM_BUFFER, 208, sizeof(unsigned), &xoff);
	glBufferSubData(GL_UNIFORM_BUFFER, 212, sizeof(unsigned), &yoff);

	double t = clock.endTimer();
	if (t > 0.8) Debug::log("^^^CHUNKCHECK^^^ New chunks loaded, time taken: " + std::to_string(t));
}

void Display::_setChunkSize(unsigned sz) {
	md.chunkSize = sz;
	md.ntChunk = md.chunkSize * md.chunkSize;
	md.ppChunk = md.chunkSize * md.tileDims;
	md.cxSlack = (md.mapW % md.chunkSize > 0) ? md.chunkSize - (md.mapW % md.chunkSize) : 0;
	md.cySlack = (md.mapH % md.chunkSize > 0) ? md.chunkSize - (md.mapH % md.chunkSize) : 0;
	md.maxChunkX = (md.mapW - 1) / md.chunkSize;
	md.maxChunkY = (md.mapH - 1) / md.chunkSize;
	md.tlc.x = 2; md.trc.x = 1;
}

void Display::_orientTiles(uint8_t orient) {
	for (int x = 0; x < md.mapW; ++x) {
		for (int y = 0; y < md.mapH; ++y) {
			int idx = y * md.mapW + x;
			uint16_t val = layer1[idx];
			uint16_t val2 = layer2[idx];
			uint8_t* vals = (uint8_t*)&val;
			uint8_t* val2s = (uint8_t*)&val2;
			vals[1] = orient;
			val2s[1] = orient;

			layer1[idx] = val;
			layer2[idx] = val2;
		}
	}

	int nTiles = md.mapW * md.mapH;
	glBindBuffer(GL_ARRAY_BUFFER, buffers[TILE_PROPS_BUFFER]);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(uint16_t) * nTiles, layer1);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(uint16_t) * nTiles, sizeof(uint16_t) * nTiles, layer2);
	glBindBuffer(GL_ARRAY_BUFFER, NULL);
}

int Display::_registerImg(std::string file) {
	int r = 0;
	if (!_isRegistered(file)) {
		SpriteData sd(file);
		imgLoadQueue.push_back(sd);
		r = 1;
	}

	return r;
}

int Display::_registerAnim(std::string file, unsigned fw, unsigned fh) {
	int r = 0;
	if (!_isRegistered(file)) {
		SpriteData sd(file, fw, fh);
		animLoadQueue.push_back(sd);
		r = 1;
	}

	return r;
}

void Display::_packTextures() {
	if (imgLoadQueue.size() + animLoadQueue.size() < 1) return;

	//load images into buffers and prep info
	GLuint** buffers = new GLuint * [imgLoadQueue.size() + animLoadQueue.size()];
	for (unsigned i = 0; i < animLoadQueue.size(); ++i) {
		_loadAnim(&animLoadQueue.at(i), &buffers[i]);
	}

	for (unsigned i = 0; i < imgLoadQueue.size(); ++i) {
		_loadImage(&imgLoadQueue.at(i), &buffers[i + animLoadQueue.size()]);
	}

	//determine how much new texture space MUST be allocated, allocate it
	std::map<uvec2, int> slotsNeeded;
	for (auto i = animLoadQueue.begin(); i != animLoadQueue.end(); ++i) {
		uvec2 s(i->frW, i->frH);
		slotsNeeded[s] += (i->imgW / i->frW) * (i->imgH / i->frH);
	}
	for (auto i = imgLoadQueue.begin(); i != imgLoadQueue.end(); ++i) {
		uvec2 s(i->imgW, i->imgH);
		slotsNeeded[s] += 1;
	}
	for (auto i = spriteTextures.begin(); i != spriteTextures.end(); ++i) {
		uvec2 s(i->w, i->h);
		slotsNeeded[s] -= i->openSlots;
	}

	for (auto i = slotsNeeded.begin(); i != slotsNeeded.end(); ++i) {
		//allocate more space for this frame size if needed
		if (i->second <= 0) continue;

		int nFull = i->second / maxTexLayers;
		int nRem = i->second % maxTexLayers;

		for (int k = 0; k < nFull; ++k) {
			GLuint t;
			glGenTextures(1, &t);
			glBindTexture(GL_TEXTURE_2D_ARRAY, t);

			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, i->first.x, i->first.y, maxTexLayers);
			spriteTextures.push_back(TexData(t, i->first.x, i->first.y, 0, maxTexLayers));
		}
		if (nRem) {
			GLuint t;
			glGenTextures(1, &t);
			glBindTexture(GL_TEXTURE_2D_ARRAY, t);

			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, i->first.x, i->first.y, nRem);
			spriteTextures.push_back(TexData(t, i->first.x, i->first.y, 0, nRem));
		}
	}
	
	//pack animations
	for (unsigned anim = 0; anim < animLoadQueue.size(); ++anim) {

		SpriteData sd = animLoadQueue.at(anim);
		TexData *bestFit = nullptr;
		unsigned slots = (sd.imgW / sd.frW) * (sd.imgH / sd.frH);
		for (auto tex = spriteTextures.begin(); tex != spriteTextures.end(); ++tex) {
			if (tex->w == sd.frW && tex->h == sd.frH && tex->openSlots >= slots) {
				bestFit = (bestFit) ?
						  ((tex->openSlots < bestFit->openSlots) ? &(*tex) : bestFit) :
						  &(*tex);
			}
		}

		//allocate new texture if this anim doesn't fit anywhere
		if (!bestFit) {
			GLuint tid;
			glGenTextures(1, &tid);
			glBindTexture(GL_TEXTURE_2D_ARRAY, tid);

			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, sd.frW, sd.frH, slots);

			spriteTextures.push_back(TexData(tid, sd.frW, sd.frH, 0, slots));
			bestFit = &(spriteTextures.at(spriteTextures.size() - 1));
		}

		//pack animation
		glBindTexture(GL_TEXTURE_2D_ARRAY, bestFit->texID);
		for (unsigned i = 0; i < slots; ++i) {
			unsigned offset = i * sd.frW * sd.frH;
			glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, bestFit->filled + i, sd.frW, sd.frH, 1,
				GL_RGBA, GL_UNSIGNED_BYTE, buffers[anim] + offset);
		}
		sd.texID = bestFit->texID;
		sd.texOff = bestFit->filled;
		bestFit->filled += slots;
		bestFit->openSlots -= slots;
		spritesLoaded.push_back(sd);
	}

	//pack images
	for (unsigned img = 0; img < imgLoadQueue.size(); ++img) {

		SpriteData sd = imgLoadQueue.at(img);
		unsigned b = animLoadQueue.size() + img;
		bool fitFound = false;
		for (auto tex = spriteTextures.begin(); tex != spriteTextures.end() && !fitFound; ++tex) {
			//pack in first open slot
			if (tex->w == sd.imgW && tex->h == sd.imgH && tex->openSlots > 0) {
				fitFound = true;
				glBindTexture(GL_TEXTURE_2D_ARRAY, tex->texID);
				glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, tex->filled, sd.imgW, sd.imgH, 1,
					GL_RGBA, GL_UNSIGNED_BYTE, buffers[b]);
				sd.texID = tex->texID;
				sd.texOff = tex->filled;
				tex->filled += 1;
				tex->openSlots -= 1;
				spritesLoaded.push_back(sd);
			}
		}
	}

	//clear
	for (unsigned i = 0; i < animLoadQueue.size() + imgLoadQueue.size(); ++i) {
		if (buffers[i]) delete[] buffers[i];
	}
	delete[] buffers;
	imgLoadQueue.clear();
	animLoadQueue.clear();
}

void Display::_loadImage(SpriteData* data, GLuint** buff) {
	bool loaded = true;
	cimg_library::CImg<uint8_t> image(data->path.c_str());

	//fill big buffer
	data->imgW = image.width();
	data->imgH = image.height();
	data->frW = data->imgW;
	data->frH = data->imgH;
	*buff = new GLuint[data->imgW * data->imgH];

	for (int y = 0; y < data->imgH; ++y) {
		for (int x = 0; x < data->imgW; ++x) {
			int idx = y * data->imgW + x;

			GLuint pix;
			uint8_t* p = (uint8_t*)&pix;
			int invY = data->imgH - 1 - y;
			p[0] = image(x, invY, 0, 0);
			p[1] = image(x, invY, 0, 1);
			p[2] = image(x, invY, 0, 2);
			p[3] = (p[0] == 255 && p[1] == 0 && p[2] == 255) ? 0 : 255;

			*buff[idx] = pix;
		}
	}

}

void Display::_loadAnim(SpriteData *data, GLuint** buff) {
	cimg_library::CImg<uint8_t> image(data->path.c_str());

	//fill big buffer
	data->imgW = image.width();
	data->imgH = image.height();
	if (data->imgW % data->frW != 0 || data->imgH % data->frH != 0) {
		*buff = nullptr;
		data->imgW = data->imgH = data->frW = data->frH = 0;
		return;
	}
	unsigned wInFrames = (data->imgW / data->frW);
	unsigned hInFrames = (data->imgH / data->frH);
	unsigned nFrames = wInFrames * hInFrames;
	*buff = new GLuint[data->imgW * data->imgH];
	
	//for each frame
	for (int n = 0; n < nFrames; ++n) {

		//get index
		unsigned frameX = (n % wInFrames);
		unsigned frameY = (n / wInFrames);
		unsigned xoff = frameX * data->frW;
		unsigned yoff = frameY * data->frH;

		//load tile into buffer
		for (int y = yoff; y < yoff + data->frH; ++y) {
			for (int x = xoff; x < xoff + data->frW; ++x) {
				int idx = (n * data->frW * data->frH) + ((y - yoff) * data->frW + (x - xoff));

				GLuint pix;
				uint8_t* p = (uint8_t*)&pix;
				int invY = yoff + data->frH - 1 - (y - yoff);
				p[0] = image(x, invY, 0, 0);
				p[1] = image(x, invY, 0, 1);
				p[2] = image(x, invY, 0, 2);
				p[3] = (p[0] == 255 && p[1] == 0 && p[2] == 255) ? 0 : 255;

				*buff[idx] = pix;
			}
		}
	}

}

bool Display::_isRegistered(std::string file) {
	bool r = false;
	for (auto i = imgLoadQueue.begin(); i != imgLoadQueue.end() && !r; ++i) {
		if (i->path.compare(file) == 0) r = true;
	}
	for (auto i = animLoadQueue.begin(); i != animLoadQueue.end() && !r; ++i) {
		if (i->path.compare(file) == 0) r = true;
	}

	return r;
}

bool Display::_isLoaded(std::string file) {
	bool r = false;
	for (auto i = spritesLoaded.begin(); i != spritesLoaded.end() && !r; ++i) {
		if (i->path.compare(file) == 0) r = true;
	}

	return r;
}