#ifdef _WINDOWS
#include <GL/glew.h>
#endif
#include <math.h>
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>
#include "Matrix.h"
#include "ShaderProgram.h"
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifdef _WINDOWS
#define RESOURCE_FOLDER ""
#else
#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif

SDL_Window* displayWindow;

GLuint LoadTexture(const char *filePath) {
	int w, h, comp;
	unsigned char* image = stbi_load(filePath, &w, &h, &comp, STBI_rgb_alpha);
	if (image == NULL) {
		std::cout << "Unable to load image. Make sure the path is correct\n";
		assert(false);
	}
	GLuint retTexture;
	glGenTextures(1, &retTexture);
	glBindTexture(GL_TEXTURE_2D, retTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	stbi_image_free(image);
	return retTexture;
}


int main(int argc, char *argv[])
{
	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("My Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 360, SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
	SDL_GL_MakeCurrent(displayWindow, context);
	#ifdef _WINDOWS
		glewInit();
	#endif
	glViewport(0, 0, 640, 360);
	ShaderProgram program;
	program.Load(RESOURCE_FOLDER"vertex.glsl", RESOURCE_FOLDER"fragment.glsl");
	ShaderProgram textProgram;
	textProgram.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");

	GLuint bg = LoadTexture(RESOURCE_FOLDER"background.png");
	GLuint friends = LoadTexture(RESOURCE_FOLDER"friends.png");
	GLuint bird = LoadTexture(RESOURCE_FOLDER"bird.png");

	Matrix projectionMatrix;
	Matrix modelMatrix;
	Matrix viewMatrix;

	projectionMatrix.SetOrthoProjection(-3.55, 3.55, -2.0f, 2.0f, -1.0f, 1.0f);
	SDL_Event event;
	bool done = false;
	float lastTicks = 0;
	while (!done) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
		}
		glClear(GL_COLOR_BUFFER_BIT);
		glUseProgram(textProgram.programID);
		float vertices[] = { -3.55f, 2.0, -3.55f, -2.0, 3.55f, 2.0, 3.55f, -2.0, 3.55f, 2.0, -3.55f, -2.0f};
		float texCoord[] = { 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f };
		modelMatrix.Identity();
		textProgram.SetModelMatrix(modelMatrix);
		textProgram.SetProjectionMatrix(projectionMatrix);
		textProgram.SetViewMatrix(viewMatrix);
		glBindTexture(GL_TEXTURE_2D, bg);
		glVertexAttribPointer(textProgram.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
		glEnableVertexAttribArray(textProgram.positionAttribute);
		glVertexAttribPointer(textProgram.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoord);
		glEnableVertexAttribArray(textProgram.texCoordAttribute);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glBindTexture(GL_TEXTURE_2D, friends);
		float vertices_friends[] = { 0, .7, 0, 0, .7, .7, .7, 0, .7, .7, 0, 0 };
		modelMatrix.Identity();
		float ticks = (float)SDL_GetTicks() / 1000.0f;
		float elapsed = ticks - lastTicks;
		lastTicks = ticks;
		modelMatrix.Scale(abs(3 * sin(2 * 3.1415926*ticks/10)), abs(3 * sin(2 * 3.1415926*ticks / 10)), 1);
		textProgram.SetModelMatrix(modelMatrix);
		glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices_friends);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glBindTexture(GL_TEXTURE_2D, bird);
		float vertices_bird[] = { 0, -.3, 0, 0, -.3, -.3, -.3, 0, -.3, -.3, 0, 0 };
		modelMatrix.Identity();
		modelMatrix.Translate(3.0f * sin(2*3.1415926 * ticks/10), 0, 0);
		textProgram.SetModelMatrix(modelMatrix);
		glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices_friends);
		glDrawArrays(GL_TRIANGLES, 0, 6);


		glDisableVertexAttribArray(textProgram.positionAttribute);
		glDisableVertexAttribArray(textProgram.texCoordAttribute);

		SDL_GL_SwapWindow(displayWindow);
	}

	SDL_Quit();
	return 0;
}
