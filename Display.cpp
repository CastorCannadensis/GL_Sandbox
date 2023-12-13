#include <GL\glew.h>
#include "Display.h"
#include "Debug.h"
#include <fstream>
#include "Utilstructs.h"
#include <cstdlib>
#include <CImg.h>

Display::Display(wxWindow* p, const int* attribs) : wxGLCanvas(p, wxID_ANY, attribs, wxPoint(0, 0)),
													panning(false), lastPos({ 0, 0 }), 
													viewTrans({ 0, 0 }), viewZoom(1.0), 
													tileDims(32), chunkSize(16), ntChunk(chunkSize*chunkSize), 
													ppChunk(chunkSize*tileDims), 
													mapW(512), mapH(512), mapWPix(mapW * tileDims), 
													mapHPix(mapH * tileDims), nTiles(mapW * mapH), 
													nTilesRendered(1),
													layer1(nullptr), layer2(nullptr), 
											        solids(nullptr), context(this) {
	cxSlack = (mapW % chunkSize > 0) ? chunkSize - (mapW % chunkSize) : 0;
	cySlack = (mapH % chunkSize > 0) ? chunkSize - (mapH % chunkSize) : 0;
	maxChunkX = (mapW - 1) / chunkSize;
	maxChunkY = (mapH - 1) / chunkSize;
	tlc.x = 2; trc.x = 1;

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

	_loadProgram();
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
	glDeleteTextures(NUM_TEXTURES, textures);
	glDeleteQueries(31, queries);
	glDeleteVertexArrays(1, &vao);
	glDeleteProgram(program);
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

	if (wPos.x > 0 && wPos.x < mapWPix && wPos.y > 0 && wPos.y < mapHPix) {
		Debug::log("**Clicked Tile: " + std::to_string(wPos.x / tileDims) + ", " + std::to_string(wPos.y / tileDims));
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

	if (viewZoom >= 5.0 && chunkSize != 8) _setChunkSize(8);
	else if (viewZoom < 5.0 && viewZoom >= 0.25 && chunkSize != 16) _setChunkSize(16);

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

	if (vis.z != 0.0)
		glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0, nTilesRendered * 2);
	else
		glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0, nTilesRendered * 3);

	SwapBuffers();
}

void Display::_loadProgram() {
	GLuint vs = _loadShader(GL_VERTEX_SHADER, "vertex.vs");
	GLuint fs = _loadShader(GL_FRAGMENT_SHADER, "fragment.fs");

	program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);
	glUseProgram(program);

	GLint err, logl;
	glGetProgramiv(program, GL_LINK_STATUS, &err);
	if (err != GL_TRUE) {
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logl);
		char* buff = (logl > 0) ? new char[logl] : nullptr;
		glGetProgramInfoLog(program, logl, NULL, buff);

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
	unsigned wInTiles = imW / tileDims;
	unsigned hInTiles = imH / tileDims;
	unsigned nTiles = wInTiles * hInTiles;
	GLuint* buff = new GLuint[imW * imH];

	//for each tile
	for (int n = 0; n < nTiles; ++n) {

		//get index
		unsigned tileX = (n % wInTiles);
		unsigned tileY = (n / wInTiles);
		unsigned xoff = tileX * tileDims;
		unsigned yoff = tileY * tileDims;
	
		//load tile into buffer
		for (int y = yoff; y < yoff + tileDims; ++y) {
			for (int x = xoff; x < xoff + tileDims; ++x) {
				int idx = (n * tileDims * tileDims) + ((y-yoff) * tileDims + (x-xoff));

				GLuint pix;
				uint8_t* p = (uint8_t*)&pix;
				int invY = yoff + tileDims - 1 - (y - yoff);
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

	glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, tileDims, tileDims, 257);
	//user-loaded tiles
	for (unsigned nt = 0; nt < nTiles; ++nt) {
		unsigned offset = (nt * tileDims * tileDims);
		glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, nt, tileDims, tileDims, 1,
			GL_RGBA, GL_UNSIGNED_BYTE, buff + offset);
	}
	delete[] buff;

	//transparent tile and solid tile
	GLuint* tbuff = new GLuint[tileDims * tileDims];
	GLuint* sbuff = new GLuint[tileDims * tileDims];
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
	for (unsigned i = 0; i < tileDims * tileDims; ++i) {
		tbuff[i] = t;
		sbuff[i] = s;
	}
	glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 255, tileDims, tileDims, 1, 
			GL_RGBA, GL_UNSIGNED_BYTE, tbuff);
	glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 256, tileDims, tileDims, 1, 
			GL_RGBA, GL_UNSIGNED_BYTE, sbuff);

	delete[] tbuff;
	delete[] sbuff;
}

void Display::_fillMap() {
	layer1 = new uint16_t[mapW * mapH];
	layer2 = new uint16_t[mapW * mapH];
	solids = new uint16_t[mapW * mapH];

	for (unsigned y = 0; y < mapH; ++y) {
		for (unsigned x = 0; x < mapW; ++x) {
			unsigned idx = y * mapW + x;
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
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glGenBuffers(NUM_BUFFERS, buffers);
	glGenQueries(31, queries);

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
	glBufferData(GL_ARRAY_BUFFER, sizeof(uint16_t) * nTiles * 3, NULL, GL_DYNAMIC_DRAW);

	glVertexAttribIPointer(1, 1, GL_UNSIGNED_BYTE, sizeof(uint16_t), 0);
	glVertexAttribIPointer(2, 1, GL_UNSIGNED_BYTE, sizeof(uint16_t), (GLvoid*)sizeof(uint8_t));
	glVertexAttribDivisor(1, 1);
	glVertexAttribDivisor(2, 1);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	glBindBuffer(GL_ARRAY_BUFFER, NULL);

	_loadTilesheet("C:/Game Stuff/Assets/testsheet.png");

	//map data uniform buffer
	unsigned half = tileDims / 2;
	vis = { 0.0, 0.0, -2.0, 0.0 };
	unsigned xoff = 0; unsigned yoff = 0;
	unsigned nt = 1;   unsigned mw = 1;
	glBindBuffer(GL_UNIFORM_BUFFER, buffers[MAP_PROPS_BUFFER]);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(mat4) * 3 + sizeof(unsigned) * 6 + sizeof(vec4) * 2, 
		NULL, GL_DYNAMIC_DRAW);
	glBufferSubData(GL_UNIFORM_BUFFER, 192, sizeof(unsigned), &nt);
	glBufferSubData(GL_UNIFORM_BUFFER, 196, sizeof(unsigned), &tileDims);
	glBufferSubData(GL_UNIFORM_BUFFER, 200, sizeof(unsigned), &half);
	glBufferSubData(GL_UNIFORM_BUFFER, 204, sizeof(unsigned), &mw);
	glBufferSubData(GL_UNIFORM_BUFFER, 208, sizeof(unsigned), &xoff);
	glBufferSubData(GL_UNIFORM_BUFFER, 212, sizeof(unsigned), &yoff);
	glBufferSubData(GL_UNIFORM_BUFFER, 224, sizeof(vec4), &vis);
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
	Debug::log("Chunk Size: " + std::to_string(chunkSize));
	Debug::log("Tiles Rendered per Layer: " + std::to_string(nTilesRendered));
	Debug::log("Chunks Rendered: " + std::to_string(nTilesRendered / ntChunk));
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
	bl.x = viewTrans.x;							bl.y = mapHPix * viewZoom + viewTrans.y;
	tr.x = mapWPix * viewZoom + viewTrans.x;	tr.y = viewTrans.y;
	 
	//no viewport intersection with map, set rendered tiles to 0
	if (tl.y > disH || bl.y < 0 || tl.x > disW || tr.x < 0) {
		tlc.x = 2; trc.x = 1;		//set nonsense chunk data to function as null
		nTilesRendered = 0;
		clock.endTimer();
		return;
	}
	//otherwise find active chunks
	uvec2 tlChunk, blChunk, trChunk;
	tlChunk.x = (tl.x >= 0) ? 0 : (-tl.x / viewZoom) / ppChunk;
	tlChunk.y = (tl.y >= 0) ? 0 : (-tl.y / viewZoom) / ppChunk;
	trChunk.x = (tr.x <= disW) ? maxChunkX : ((disW - viewTrans.x) / viewZoom) / ppChunk;
	trChunk.y = tlChunk.y;
	blChunk.x = tlChunk.x;
	blChunk.y = (bl.y <= disH) ? maxChunkY : ((disH - viewTrans.y) / viewZoom) / ppChunk;

	//if this is a new set of chunks, load new data to GPU
	if (tlChunk == tlc && blChunk == blc && trChunk == trc) {
		double t = clock.endTimer();
		if (t > 0.003) Debug::log("^^^CHUNKCHECK^^^ Same chunk set determined, time taken: " + std::to_string(t));
		return;
	}
	tlc = tlChunk; blc = blChunk; trc = trChunk;

	//Debug::log("********************");
	//Debug::log("Top Left Chunk: " + std::to_string(tlChunk.x) + ", " + std::to_string(tlChunk.y));
	//Debug::log("Bottom Left Chunk: " + std::to_string(blChunk.x) + ", " + std::to_string(blChunk.y));
	//Debug::log("Top Right Chunk: " + std::to_string(trChunk.x) + ", " + std::to_string(trChunk.y));
	//Debug::log("********************");

	unsigned nch = (blc.y - tlc.y + 1);
	unsigned ncw = (trc.x - tlc.x + 1);
	unsigned tw = ncw * chunkSize;
	unsigned nt = ncw * nch * ntChunk;
	if (trc.x == maxChunkX) {
		nt -= cxSlack * nch;
		tw -= cxSlack;
	}
	if (blc.y == maxChunkY) {
		nt -= cySlack * ncw;
	}
	glBindBuffer(GL_ARRAY_BUFFER, buffers[TILE_PROPS_BUFFER]);
	void* buff = glMapBufferRange(GL_ARRAY_BUFFER, 0, nt * 3 * sizeof(uint16_t), 
		GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

	uint16_t* ubuff = (uint16_t*)buff;
	unsigned yLim = blc.y * chunkSize + chunkSize;
	unsigned xLim = trc.x * chunkSize + chunkSize;
	unsigned lay1Idx = 0; unsigned lay2Idx = nt; unsigned lay3Idx = nt * 2;
	unsigned y = tlc.y * chunkSize;
	unsigned x = tlc.x * chunkSize;
	unsigned yoff = y * tileDims;
	unsigned xoff = x * tileDims;
	for (; y < mapH && y < yLim; ++y) {
		for (x = tlc.x * chunkSize; x < mapW && x < xLim; ++x) {
			unsigned mapIdx = y * mapW + x;
			ubuff[lay1Idx] = layer1[mapIdx];
			ubuff[lay2Idx] = layer2[mapIdx];
			ubuff[lay3Idx] = solids[mapIdx];
			++lay1Idx; ++lay2Idx; ++lay3Idx;
		}
	}

	glUnmapBuffer(GL_ARRAY_BUFFER);
	glBindBuffer(GL_ARRAY_BUFFER, NULL);
	nTilesRendered = nt;
	
	glBindBuffer(GL_UNIFORM_BUFFER, buffers[MAP_PROPS_BUFFER]);
	glBufferSubData(GL_UNIFORM_BUFFER, 192, sizeof(unsigned), &nt);		
	glBufferSubData(GL_UNIFORM_BUFFER, 204, sizeof(unsigned), &tw);		
	glBufferSubData(GL_UNIFORM_BUFFER, 208, sizeof(unsigned), &xoff);
	glBufferSubData(GL_UNIFORM_BUFFER, 212, sizeof(unsigned), &yoff);

	double t = clock.endTimer();
	if (t > 0.8) Debug::log("^^^CHUNKCHECK^^^ New chunks loaded, time taken: " + std::to_string(t));
}

void Display::_setChunkSize(unsigned sz) {
	chunkSize = sz;
	ntChunk = chunkSize * chunkSize;
	ppChunk = chunkSize * tileDims;
	cxSlack = (mapW % chunkSize > 0) ? chunkSize - (mapW % chunkSize) : 0;
	cySlack = (mapH % chunkSize > 0) ? chunkSize - (mapH % chunkSize) : 0;
	maxChunkX = (mapW - 1) / chunkSize;
	maxChunkY = (mapH - 1) / chunkSize;
	tlc.x = 2; trc.x = 1;
}

void Display::_orientTiles(uint8_t orient) {
	for (int x = 0; x < mapW; ++x) {
		for (int y = 0; y < mapH; ++y) {
			int idx = y * mapW + x;
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

	int nTiles = mapW * mapH;
	glBindBuffer(GL_ARRAY_BUFFER, buffers[TILE_PROPS_BUFFER]);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(uint16_t) * nTiles, layer1);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(uint16_t) * nTiles, sizeof(uint16_t) * nTiles, layer2);
	glBindBuffer(GL_ARRAY_BUFFER, NULL);
}