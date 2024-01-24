#include <GL\glew.h>
#include "Display.h"
#include "Debug.h"
#include <fstream>
#include "Utilstructs.h"
#include <cstdlib>
#include <map>
#include <CImg.h>
#include <algorithm>

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
	if (GLEW_VERSION_4_4) {
		Debug::log("GL Version 4.4 Core Supported!");
	}
	else {
		Debug::log("SHIT! GL Version 4.4 Core NOT Supported!");
	}
	if (GLEW_VERSION_4_5) {
		Debug::log("GL Version 4.5 Core Supported!");
	}
	else {
		Debug::log("SHIT! GL Version 4.5 Core NOT Supported!");
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
	//_performanceTest();
	_spriteLoadingTest();

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

int Display::packTextures() {
	AtlasData old = atlas;
	int ret = _packTextures();

	//repack with just the previous contents if new pack doesn't fit
	if (ret == 3) {		
		_clearLoadQ();
		for (auto i = spritesLoaded.begin(); i != spritesLoaded.end(); ++i)
			loadQueue.push_back(*i);
		spritesLoaded.clear();
		_allocateAtlas(old.layers, false);
		ret = _packTextures();
	}

	//set metadata for atlas and loaded sprites
	if (ret == 0) {
		unsigned long totImgBytes = 0;
		for (auto i = loadQueue.begin(); i != loadQueue.end(); ++i) {
			totImgBytes += i->imgW * i->imgH * 4;
			delete[] (i->_pixels);
			i->_pixels = nullptr;
			spritesLoaded.push_back(*i);
		}
		_clearLoadQ();

		atlas.imgBytes += totImgBytes;
		atlas.usedBytes = (1024 * 1024 * 4 * atlas.lpos) + (1024 * atlas.ypos * 4);
		atlas.freeBytes = (1024 * 1024 * 4 * atlas.layers) - atlas.usedBytes;

		std::sort(spritesLoaded.begin(), spritesLoaded.end(), spriteDataCompName);
	}

	_clearLoadQ();
	return ret;
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
	glBufferSubData(GL_UNIFORM_BUFFER, 64, sizeof(mat4), &proj);
	glBindBuffer(GL_UNIFORM_BUFFER, NULL);

	_setViewProj();
}

void Display::_setView() {
	view = { {viewZoom, 0.0f, 0.0f, 0.0f},					//x column
		   {0.0f, viewZoom, 0.0f, 0.0f},					//y	column
		   {0.0f, 0.0f, 1.0f, 0.0f},						//z	column
		   {viewTrans.x, viewTrans.y, 0.0f, 1.0f}, };	//w	column

	_setViewProj();
	_updateChunks();
}

void Display::_setViewProj() {
	mat4 viewproj = proj * view;

	glBindBuffer(GL_UNIFORM_BUFFER, buffers[MAP_PROPS_BUFFER]);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(mat4), &viewproj);
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
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, textures[TILESHEET_TEX]);
	
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, md.tileDims, md.tileDims, 257);
	glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, md.tileDims, md.tileDims, nTiles, 
		GL_RGBA, GL_UNSIGNED_BYTE, buff);
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
	glGenBuffers(NUM_BUFFERS, buffers);
	glGenTextures(NUM_TEXTURES, textures);
	glGenQueries(31, queries);
	glBindVertexArray(vaos[0]);

	//texture limit data
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
	glBufferData(GL_UNIFORM_BUFFER, 192, NULL, GL_DYNAMIC_DRAW);
	
	glBufferSubData(GL_UNIFORM_BUFFER, 128, sizeof(unsigned), &nt);
	glBufferSubData(GL_UNIFORM_BUFFER, 132, sizeof(unsigned), &md.tileDims);
	glBufferSubData(GL_UNIFORM_BUFFER, 136, sizeof(unsigned), &half);
	glBufferSubData(GL_UNIFORM_BUFFER, 140, sizeof(unsigned), &mw);
	glBufferSubData(GL_UNIFORM_BUFFER, 144, sizeof(unsigned), &xoff);
	glBufferSubData(GL_UNIFORM_BUFFER, 148, sizeof(unsigned), &yoff);
	glBufferSubData(GL_UNIFORM_BUFFER, 160, sizeof(vec4), &md.vis);
	_setView();
	_setProjection();

	glBindBufferBase(GL_UNIFORM_BUFFER, 0, buffers[MAP_PROPS_BUFFER]);

	//Atlas
	_allocateAtlas(32, false);		//128MB size by default
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

void Display::_spriteLoadingTest() {
	
	_registerSprite("C:/Game Stuff/Assets/guy.png");
	_registerSprite("C:/Game Stuff/Assets/wizard.png");
	_registerSprite("C:/Game Stuff/Assets/bigchonk.png");
	int ret = packTextures();

	if (ret == 0) {
		Debug::log("### Successfully packed textures!");
	}
	else if (ret == 1) {
		Debug::log("### No textures to pack!");
	}
	else if (ret == 2) {
		Debug::log("### Requested asset pack exceeds image data limits!");
	}
	else if (ret == 3) {
		Debug::log("### Couldn't fit texture data");
	}

	_debugTexDump();
}

void Display::_debugTexDump() {
	Debug::log("##########SPRITE TEXTURE DEBUG DUMP###############");

	Debug::log("\n--Current Sprites--");
	for (unsigned i = 0; i < spritesLoaded.size(); ++i) {
		SpriteData sd = spritesLoaded.at(i);
		Debug::log(std::to_string(i) + ": " + sd.path);
	}
	Debug::log("##################################################");

	//print out images of occupied atlas layers
	if (atlas.imgBytes > 0) {
		GLuint* buff = new GLuint[1024 * 1024 * atlas.layers];
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D_ARRAY, textures[ATLAS_TEX]);
		glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, GL_UNSIGNED_BYTE, buff);

		for (unsigned l = 0; l <= atlas.lpos; ++l) {
			unsigned offset = 1024 * 1024 * l;
			_flipImage(buff + offset, 1024, 1024);
			cimg_library::CImg<uint8_t> img(1024u, 1024u, 1u, 4u);
			for (unsigned p = 0; p < 1024 * 1024; ++p) {
				GLuint pix = (buff + offset)[p];
				uint8_t* bpix = (uint8_t*)&pix;
				unsigned x = p % 1024;
				unsigned y = p / 1024;

				img(x, y, 0, 0) = bpix[0];
				img(x, y, 0, 1) = bpix[1];
				img(x, y, 0, 2) = bpix[2];
				img(x, y, 0, 3) = bpix[3];
			}
			std::string f = "atlas_" + std::to_string(l) + ".png";
			img.save(f.c_str());
		}

		delete[] buff;
		glBindTexture(GL_TEXTURE_2D_ARRAY, NULL);
	}
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

	//if the active chunks are the current chunks, no action needed
	if (tlChunk == md.tlc && blChunk == md.blc && trChunk == md.trc) {
		double t = clock.endTimer();
		if (t > 0.003) Debug::log("^^^CHUNKCHECK^^^ Same chunk set determined, time taken: " + std::to_string(t));
		return;
	}
	md.tlc = tlChunk; md.blc = blChunk; md.trc = trChunk;

	//otherwise calculate tile bounds
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

	//load new data to GPU
	glBindBuffer(GL_ARRAY_BUFFER, buffers[TILE_PROPS_BUFFER]);
	void* buff = glMapBufferRange(GL_ARRAY_BUFFER, 0, nt * 3 * sizeof(uint16_t), 
		GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

	uint16_t* ubuff = (uint16_t*)buff;
	unsigned yLim = md.blc.y * md.chunkSize + md.chunkSize;
	unsigned xLim = md.trc.x * md.chunkSize + md.chunkSize;
	unsigned lay1Idx = 0, lay2Idx = nt, lay3Idx = nt * 2;
	unsigned y = md.tlc.y * md.chunkSize, yoff = y * md.tileDims;
	unsigned x = md.tlc.x * md.chunkSize, xoff = x * md.tileDims;
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
	glBufferSubData(GL_UNIFORM_BUFFER, 128, sizeof(unsigned), &nt);		
	glBufferSubData(GL_UNIFORM_BUFFER, 140, sizeof(unsigned), &tw);		
	glBufferSubData(GL_UNIFORM_BUFFER, 144, sizeof(unsigned), &xoff);
	glBufferSubData(GL_UNIFORM_BUFFER, 148, sizeof(unsigned), &yoff);

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

/*Returns:
* 0 = success
* 1 = requested data preservation, but also smaller size
* 2 = out of memory */
int Display::_allocateAtlas(unsigned layers, bool preserve) {
	AtlasData old = atlas;
	AtlasData nw; atlas = nw;
	if (layers > 256) layers = 256;
	if (old.layers == layers && preserve) return 0;
	if (preserve && layers < old.layers) return 1;

	//copy old atlas data before reallocation
	GLuint* buff = nullptr;
	if (old.imgBytes > 0 && preserve) {
		buff = new GLuint[old.layers * 1024 * 1024];
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D_ARRAY, textures[ATLAS_TEX]);
		glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, GL_UNSIGNED_BYTE, buff);
	}

	//allocation
	glDeleteTextures(1, &(textures[ATLAS_TEX]));
	glGenTextures(1, &(textures[ATLAS_TEX]));

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D_ARRAY, textures[ATLAS_TEX]);
	while (glGetError() != GL_NO_ERROR) {}

	glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, 1024, 1024, layers);	

	GLenum err = glGetError();
	if (err == GL_INVALID_OPERATION || err == GL_OUT_OF_MEMORY) {
		Debug::log("ERROR: CANNOT ALLOCATE " + std::to_string(layers * 4) + "MB OF ATLAS MEMORY");
		delete[] buff; buff = nullptr;
		return 2;
	}

	//setup
	atlas.freeBytes = (1024 * 1024 * 4 * layers);
	atlas.layers = layers;

	if (buff) {
		glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 1024, 1024, old.layers, GL_RGBA, GL_UNSIGNED_BYTE, buff);
		glBindTexture(GL_TEXTURE_2D_ARRAY, NULL);
		delete[] buff; buff = nullptr;

		atlas.imgBytes = old.imgBytes;
		atlas.usedBytes = old.usedBytes;
		atlas.freeBytes -= old.usedBytes;
		atlas.ypos = old.ypos;
		atlas.lpos = old.lpos;
	}

	return 0;
}

int Display::_registerSprite(std::string file, uint16_t fw, uint16_t fh) {
	int r = 0;
	if (!_isRegistered(file) && !_isLoaded(file)) {
		SpriteData sd(file, fw, fh);
		loadQueue.push_back(sd);
		r = 1;
	}

	return r;
}

/*Returns:
* 0 = success
* 1 = load queue empty
* 2 = image data limit exceeded
* 3 = max atlas size reached, possible repack needed */
int Display::_packTextures() {
	if (loadQueue.size() < 1) return 1;

	//load images into buffers and prep info
	unsigned sz = loadQueue.size();
	unsigned long totImgBytes = 0;
	for (unsigned i = 0; i < sz; ++i) {
		SpriteData& sd = loadQueue.at(i);
		_loadSprite(&sd);
		totImgBytes += sd.imgW * sd.imgH * 4;
	}

	//if image data size is too much, abort
	if (totImgBytes > atlas.maxBytes || totImgBytes + atlas.imgBytes > atlas.maxBytes) {
		Debug::log("ERROR: Texture data limit exceeded");
		return 2;
	}

	//make sure enough atlas space is allocated
	if (totImgBytes > atlas.freeBytes) {
		long double rem = (long double)totImgBytes - (long double)(atlas.freeBytes);
		double chunks = std::ceil((rem / (1024.0l * 1024.0l * 128.0l)));
		_allocateAtlas(atlas.layers + 32 * chunks, true);
	}

	//sort by descending height, pack
	std::sort(loadQueue.begin(), loadQueue.end(), spriteDataCompH);
	
	unsigned xpos = 0;
	unsigned rowHeight = loadQueue.at(0).imgH;
	unsigned long bytesPacked = 0;
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D_ARRAY, textures[ATLAS_TEX]);
	for (unsigned i = 0; i < loadQueue.size(); ++i) {
		SpriteData& sd = loadQueue.at(i);
		//check contraints
		if (xpos + sd.imgW > 1024) {
			atlas.ypos += rowHeight;
			xpos = 0;
			rowHeight = sd.imgH;
		}
		if (atlas.ypos + rowHeight > 1024) {
			xpos = 0;
			atlas.ypos = 0;
			atlas.lpos += 1;
			if (atlas.lpos == 256)		   return 3;
			if (atlas.lpos > atlas.layers) _allocateAtlas(atlas.layers + 32, true);
		}

		//pack if able
		glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, xpos, atlas.ypos, atlas.lpos, sd.imgW, sd.imgH,
			1, GL_RGBA, GL_UNSIGNED_BYTE, sd._pixels);

		sd.yoff = atlas.ypos;
		sd.xoff = xpos;
		sd.layer = atlas.lpos;

		xpos += sd.imgW;
	}
	atlas.ypos += rowHeight;

	return 0;
}

void Display::_clearLoadQ() {
	unsigned sz = loadQueue.size();
	for (unsigned i = 0; i < sz; ++i) {
		SpriteData& sd = loadQueue.at(i);
		if (sd._pixels) delete[] sd._pixels;
	}
	loadQueue.clear();
}

void Display::_loadSprite(SpriteData* data) {
	if (data->_pixels) return;		//buffer already loaded

	cimg_library::CImg<uint8_t> image(data->path.c_str());

	//fill buffer
	data->imgW = image.width();
	data->imgH = image.height();

	data->_pixels = new GLuint[data->imgW * data->imgH];

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

			data->_pixels[idx] = pix;
		}
	}

}

void Display::_flipImage(GLuint* buff, unsigned w, unsigned h) {
	if (!buff || !w || !h) return;

	for (unsigned y = 0; y < h/2; ++y) {
		for (unsigned x = 0; x < w; ++x) {
			int invY = h - 1 - y;
			int idx = y * w + x;
			int invIdx = invY * w + x;

			unsigned temp = buff[idx];
			buff[idx] = buff[invIdx];
			buff[invIdx] = temp;
		}
	}
}

bool Display::_isRegistered(std::string file) {
	bool r = false;
	for (auto i = loadQueue.begin(); i != loadQueue.end() && !r; ++i) {
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

void Display::_updateThings() {
	//check if we have a new set of things to render
	
	//if so, load new vertex attribute data

}

void Display::_loadThings() {

}