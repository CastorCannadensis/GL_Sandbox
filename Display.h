#ifndef GLSANDBOX_DISPLAY
#define GLSANDBOX_DISPLAY

#include "wx\wx.h"
#include <wx\glcanvas.h>
#include "Utilstructs.h"
#include "Clock.h"

enum {
	QUAD_BUFFER,
	QUAD_INDEX_BUFFER,
	TILE_PROPS_BUFFER,
	MAP_PROPS_BUFFER,
	THING_PROPS_BUFFER,
	SAMPLER_BUFFER,
	NUM_BUFFERS
};
enum {
	TILESHEET_TEX,
	COLORS_TEX,
	NUM_BAKEDIN_TEXTURES
};
enum {
	FLIP_HOR,
	FLIP_VERT,
	ROT_RIGHT,
	ROT_LEFT
};
enum : uint8_t {
	STATE_NORMAL,
	STATE_FLIP_VERT,
	STATE_FLIP_HOR,
	STATE_ROT_90,
	STATE_ROT_180,
	STATE_ROT_270,
	STATE_FLIP_VERT_ROT_90,
	STATE_FLIP_VERT_ROT_270
};

class Display : public wxGLCanvas {
public:
	Display(wxWindow *p, const int *attribs);
	~Display();

	void _OnRightDown(wxMouseEvent& e);
	void _OnRightUp(wxMouseEvent& e);
	void _OnLeftDown(wxMouseEvent& e);
	void _OnLeftUp(wxMouseEvent& e);
	void _OnLeftDClick(wxMouseEvent& e);
	void _OnMouseMove(wxMouseEvent& e);
	void _OnMousewheel(wxMouseEvent& e);
	void _OnFocus(wxFocusEvent& e);

	void focus();
	void resize();
	void render();

private:
	void _loadPrograms();
	GLuint _loadShader(GLenum type, std::string f);
	void _setProjection();
	void _setView();
	void _setViewProj();
	void _loadTilesheet(std::string f);
	void _fillMap();

	void _setup();
	void _performanceTest();

	void _updateChunks();
	void _setChunkSize(unsigned sz);
	void _orientTiles(uint8_t orient);

		//Thing helpers
	int _registerImg(std::string file);
	int _registerAnim(std::string file, unsigned fw, unsigned fh);
	void _packTextures();

	void _loadImage(SpriteData* data, GLuint** buff);
	void _loadAnim(SpriteData* data, GLuint** buff);

	bool _isRegistered(std::string file);
	bool _isLoaded(std::string file);

	bool panning;
	wxPoint lastPos;

	int disW, disH;
	GLuint programs[2];
	GLuint vaos[2];
	GLuint buffers[NUM_BUFFERS];
	GLuint textures[NUM_BAKEDIN_TEXTURES];
	GLuint queries[32];

	int maxSimTex;		//simultaneous texture bindings
	int maxTexSize;
	int maxTexLayers;	//2D texture array layer limit

	//projection and view transforms
	mat4 view, proj;
	vec2 viewTrans;
	float viewZoom;

	//map data
	MapData md;

	uint16_t* layer1;
	uint16_t* layer2;
	uint16_t* solids;					//0 = no solid, 1 = solid

	//Thing data
	std::vector<SpriteData> imgLoadQueue;
	std::vector<SpriteData> animLoadQueue;
	std::vector<SpriteData> spritesLoaded;
	std::vector<TexData> spriteTextures;

	Clock clock;
	wxGLContext context;
};

#endif
