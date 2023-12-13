#include "MainWin.h"
#include "Display.h"

wxBEGIN_EVENT_TABLE(MainWin, wxMDIParentFrame)
EVT_CLOSE(MainWin::_OnClose)
EVT_SIZE(MainWin::_OnResize)
EVT_ICONIZE(MainWin::_OnRestore)
EVT_SET_FOCUS(MainWin::_OnFocus)
wxEND_EVENT_TABLE();

MainWin::MainWin() : wxMDIParentFrame(nullptr, wxID_ANY, "OpenGL Sandbox", wxDefaultPosition,
									  wxSize(640, 480)),
									  body(nullptr), canvas(nullptr) {


	body = new wxPanel(this, wxID_ANY, wxPoint(0, 0), wxSize(640, 480));

	wxBoxSizer* mainSizer = new wxBoxSizer(wxHORIZONTAL);

	int attr[7] = { WX_GL_RGBA, WX_GL_DOUBLEBUFFER, WX_GL_MAJOR_VERSION, 4, WX_GL_MINOR_VERSION, 3, 0 };
	canvas = new Display(body, attr);

	mainSizer->Add(canvas, 1, wxEXPAND | wxALL);

	body->SetSizerAndFit(mainSizer);
	this->Fit();
	this->SetMinClientSize(wxSize(640, 480));
}

void MainWin::_OnClose(wxCloseEvent& e) {

	e.Skip();
}

void MainWin::_OnResize(wxSizeEvent& e) {
	wxSize nSize = this->GetClientSize();

	if (body) {
		body->SetSize(nSize);
		body->Layout();
	}
	if (canvas) {
		CallAfter(&MainWin::_updateDisplay);
	}
}

void MainWin::_OnRestore(wxIconizeEvent& e) {
	//just a stub so it can propagate
}

void MainWin::_OnFocus(wxFocusEvent& e) {

	canvas->focus();
	e.Skip();
}

void MainWin::_updateDisplay() {
	canvas->focus();
}