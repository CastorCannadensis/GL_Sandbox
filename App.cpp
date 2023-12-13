#include "App.h"
#include "MainWin.h"
#include "Debug.h"

wxIMPLEMENT_APP(App);

App::App() : win(nullptr) {

}

bool App::OnInit() {
	win = new MainWin();
	win->Show();

	return true;
}

int App::OnExit() {
	Debug::terminate();

	return 1;
}