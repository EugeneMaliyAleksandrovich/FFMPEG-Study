#ifndef Application_h
#define Application_h

extern "C" {
#include <SDL2/SDL.h>
}
#include <iostream>

class Application {

public:
	Application();
	~Application();

	void init(const char* title, int xpos, int ypos, int width, int height, bool fullscreen);
	void handleEvent();
	void update();
	void render();
	void clean();
	void updateTexture(const Uint8* Yplane, int Ypitch, const Uint8* Uplane, int Upitch, const Uint8* Vplane, int Vpitch);

	bool running() { return isRunning; }

private:
	int cnt = 0;
	bool isRunning;
	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Texture* texture;
	SDL_Rect rectangle;
};

#endif