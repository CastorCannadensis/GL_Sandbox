#include "Clock.h"
#include "Debug.h"

Clock::Clock(unsigned fpers) : fps(fpers) {
	//set ideal frame length and tick frequency
	idealFrameLength = 1.0 / fps;
	QueryPerformanceFrequency(&tickFrequency);

	frameStart.QuadPart = 0;
	frameEnd.QuadPart = 0;

	//set length of last frame to the ideal frame length by default
	lengthOfLastFrame = idealFrameLength;

	Debug::log("clock data");
	Debug::log(std::to_string(fps));
	Debug::log(std::to_string(idealFrameLength));
	Debug::log(std::to_string(tickFrequency.QuadPart));
}

void Clock::endFrame() {
	QueryPerformanceCounter(&frameEnd);
	double time = (frameEnd.QuadPart - frameStart.QuadPart) / (double)(tickFrequency.QuadPart);
	if (time < idealFrameLength)
		Sleep((idealFrameLength - time) * 1000.0);

	lengthOfLastFrame = time;

}

double Clock::endTimer() {
	QueryPerformanceCounter(&frameEnd);
	double time = (frameEnd.QuadPart - frameStart.QuadPart) / (double)(tickFrequency.QuadPart);
	return time * 1000;  //ms
}