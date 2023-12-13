#ifndef GLSANDBOX_MAINWIN
#define GLSANDBOX_MAINWIN

#include "wx\wx.h"
#include <wx\glcanvas.h>

class Display;

class MainWin : public wxMDIParentFrame {
public:
	MainWin();
	
	//Event handling
	void _OnClose(wxCloseEvent& e);
	void _OnResize(wxSizeEvent& e);
	void _OnRestore(wxIconizeEvent& e);
	void _OnFocus(wxFocusEvent& e);
private:
	void _updateDisplay();

	wxPanel* body;
	Display* canvas;
	
	wxDECLARE_EVENT_TABLE();
};


#endif

