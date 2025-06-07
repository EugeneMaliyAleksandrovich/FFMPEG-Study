#include "Application.h"

Application::Application() {

}

Application::~Application() {

}

void Application::init(const char* title, int xpos, int ypos, int width, int height, bool fullscreen) {

	int flags = 0;
	if (fullscreen) {
		flags = SDL_WINDOW_FULLSCREEN;
	}

	if (SDL_Init(SDL_INIT_EVERYTHING) == 0) {
		std::cout << "Subsystem initialised..." << std::endl;

		window = SDL_CreateWindow(title, xpos, ypos, width, height, flags);
		if (window) {
			std::cout << "Window is created..." << std::endl;
		}

		renderer = SDL_CreateRenderer(window, -1, 0);
		if (renderer) {
			SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
			std::cout << "Renderer is created..." << std::endl;
		}

		rectangle.x = 0;
		rectangle.y = 0;
		rectangle.w = width;
		rectangle.h = height;

		texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, width, height);
		if (!texture)
		{
			std::cout << "Texture is created..." << std::endl;
		}

		isRunning = true;
	}
	else {
		isRunning = false;
	}

}

void Application::handleEvent() {
	SDL_Event event;
	SDL_PollEvent(&event);
	switch (event.type) {
	case SDL_QUIT:
		isRunning = false;
		break;
	default:
		break;
	}
}

void Application::update() {
	std::cout << ++cnt << std::endl;
}

void Application::render() {
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

void Application::clean() {
	SDL_RenderClear(renderer);
	SDL_DestroyRenderer(renderer);
	SDL_Quit();

	std::cout << "Game cleaned..." << std::endl;
}

void Application::updateTexture(const Uint8* Yplane, int Ypitch, const Uint8* Uplane, int Upitch, const Uint8* Vplane, int Vpitch) {
	SDL_UpdateYUVTexture(texture, &rectangle, Yplane, Ypitch, Uplane, Upitch, Vplane, Vpitch);
}