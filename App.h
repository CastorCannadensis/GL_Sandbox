#ifndef GLSANDBOX_APP
#define GLSANDBOX_APP


#include "wx\wx.h"

class MainWin;

class App : public wxApp {
public:
	App();
	virtual bool OnInit();
	virtual int OnExit();
private:
	MainWin* win;
};


#endif
