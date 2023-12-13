#ifndef GLSANDBOX_CLOCK_H
#define GLSANDBOX_CLOCK_H

#include <Windows.h>


class Clock {
public:
	Clock(unsigned fpers = 60);

	void beginFrame() { QueryPerformanceCounter(&frameStart); }
	void endFrame();

	unsigned getFPS() { return fps; }
	void setFPS(unsigned framesps) { fps = framesps; idealFrameLength = 1.0 / fps; }

	double getIdealFrameLength() { return idealFrameLength; }
	double getLengthOfLastFrame() { return lengthOfLastFrame; }

	void beginTimer() { QueryPerformanceCounter(&frameStart); }
	double endTimer();

	static void getTicks(LARGE_INTEGER* ticks) { QueryPerformanceCounter(ticks); }
	static void getFreq(LARGE_INTEGER* fr) { QueryPerformanceFrequency(fr); }

private:
	unsigned fps;					//in fps
	double idealFrameLength;		//in seconds
	LARGE_INTEGER tickFrequency;	

	LARGE_INTEGER frameStart;		//in system ticks
	LARGE_INTEGER frameEnd;			//in system ticks
	double lengthOfLastFrame;		//in seconds

};


#endif