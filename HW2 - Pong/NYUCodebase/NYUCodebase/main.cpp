#ifdef _WINDOWS
	#include <GL/glew.h>
#endif
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>
#include "ShaderProgram.h"
#include "Matrix.h"
#ifdef _WINDOWS
	#define RESOURCE_FOLDER ""
#else
	#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif

SDL_Window* displayWindow;

class bar {
public:
	bar(float *pos, ShaderProgram& program) : program(&program) {
		for (size_t i = 0; i < 3; i++) {
			currentPos[i] = pos[i];
		}
		modelMatrix.Identity();
		readjustCorners();
	}
	void changeDirection(int newDirection) {
		direction = newDirection;
	}
	void draw() {
		glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertices);
		glEnableVertexAttribArray(program->positionAttribute);
		modelMatrix.Identity();
		modelMatrix.Translate(currentPos[0], currentPos[1], currentPos[2]);
		program->SetModelMatrix(modelMatrix);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}
	void move(float elapsed) {
		if (fabs(currentPos[1] + direction + elapsed * direction * velocity) > 8.7f && direction != 0) {
			currentPos[1] = direction * 8.7f;
		}
		else {
			currentPos[1] += elapsed * direction * velocity;
		}
		readjustCorners();
	}
	float getCoordinate(int x) const {
		if (x < 8) {
			return corners[x];
		}
	}
	void reset() {
		currentPos[1] = 0;
		direction = 0;
	}
private:
	ShaderProgram * program;
	Matrix modelMatrix;
	float vertices[12] = { -0.5f, -1.0f, 0.5f, -1.0f, -0.5f, 1.0f, 0.5f, -1.0f, 0.5f, 1.0f, -0.5f, 1.0f };
	float corners[8] = { -0.5f, -1.0f, 0.5f, -1.0f, 0.5f, 1.0f, -0.5f, 1.0f };
	float currentPos[3] = { 0,0,0 };
	float velocity = 10.0f;
	int direction = 0;
	void readjustCorners() {
		float originalCorners[8] = { -0.5f, -1.0f, 0.5f, -1.0f, 0.5f, 1.0f, -0.5f, 1.0f };
		for (int i = 0; i < 8; i++) {
			if (i % 2 == 0) {
				corners[i] = currentPos[0] + originalCorners[i];
			}
			else {
				corners[i] = currentPos[1] + originalCorners[i];
			}
		}
	}
};

class ball {
public:
	ball(ShaderProgram& program) : program(&program) {
		xDirection = 1;
		yDirection = -1;
		yVelocity = (float)(rand() % 30);
		modelMatrix.Identity();
	}
	void draw() {
		glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertices);
		glEnableVertexAttribArray(program->positionAttribute);
		modelMatrix.Identity();
		modelMatrix.Translate(currentPos[0], currentPos[1], currentPos[2]);
		program->SetModelMatrix(modelMatrix);

		glDrawArrays(GL_TRIANGLES, 0, 6);
	}
	void move(float elapsed, const bar& left, const bar& right) {
		float ballCorners[8] = { currentPos[0] - 0.3f, currentPos[1] - 0.3f, currentPos[0] + 0.3f, currentPos[1] - 0.3f, currentPos[0] + 0.3f, currentPos[1] + 0.3f, currentPos[0] - 0.3f, currentPos[1] + 0.3f };
		float paddleCorners[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		if (fabs(currentPos[1] + elapsed * yDirection * yVelocity) > 9.0f && yDirection != 0) {
			currentPos[1] = yDirection * 9.0f;
			yDirection *= -1;
		}
		else {
			currentPos[1] += elapsed * yDirection * yVelocity;
		}
		if (fabs(currentPos[0] + elapsed * xDirection * xVelocity) > 16.0f && xDirection != 0) {
			currentPos[0] = xDirection * 16.0f;
			gameOver = -1 * xDirection;
			xDirection = 0;
			yDirection = 0;
			return;
		}
		else {
			currentPos[0] += elapsed * xDirection * xVelocity;
		}
		//left paddle collision
		for (int i = 0; i < 8; i++) {
			paddleCorners[i] = left.getCoordinate(i);
		}
		if (ballCorners[0] <= paddleCorners[2] && ballCorners[0] >= paddleCorners[0]) {
			if ((ballCorners[1] >= paddleCorners[1] && ballCorners[1] <= paddleCorners[7]) || (ballCorners[7] >= paddleCorners[1] && ballCorners[7] <= paddleCorners[7])) {
				xDirection = 1;
			}
		}
		//right paddle collision
		for (int i = 0; i < 8; i++) {
			paddleCorners[i] = right.getCoordinate(i);
		}
		if (ballCorners[2] >= paddleCorners[0] && ballCorners[2] <= paddleCorners[2]) {
			if ((ballCorners[1] >= paddleCorners[1] && ballCorners[1] <= paddleCorners[7]) || (ballCorners[7] >= paddleCorners[1] && ballCorners[7] <= paddleCorners[7])) {
				xDirection = -1;
			}
		}
	}
	void reset() {
		currentPos[0] = 0;
		currentPos[1] = 0;
		xDirection = 1;
		yDirection = -1;
		gameOver = 0;
	}
	int done() {
		return gameOver;
	}
private:
	friend class bar;
	ShaderProgram * program;
	Matrix modelMatrix;
	float vertices[12] = { -0.3f, 0.3f, -0.3f, -0.3f, 0.3f, 0.3f, 0.3f, -0.3f, -0.3f, -0.3f, 0.3f, 0.3f };
	float currentPos[3] = { 0.0f, 0.0f, 0.0f };
	float yVelocity;
	float xVelocity = 6.0f;
	int xDirection = 0;
	int yDirection = 0;
	int gameOver = 0;
};

class background {
public:
	background(ShaderProgram& program) : program(&program) {
		modelMatrix.Identity();
	}
	void draw() {
		glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertices);
		glEnableVertexAttribArray(program->positionAttribute);
		program->SetModelMatrix(modelMatrix);
		glDrawArrays(GL_TRIANGLES, 0, 18);
	}
private:
	ShaderProgram* program;
	float vertices[36] = { -16.0f, 9.0f, -16.0f, 8.7f, 16.0f, 9.0f, 16.0f, 8.7f, 16.0f, 9.0f, -16.0f, 8.7f, //top wall
		-16.0f, -9.0f, -16.0f, -8.7f, 16.0f, -9.0f, 16.0f, -8.7f, 16.0f, -9.0f, -16.0f, -8.7f,			 //bottom wall
		-0.1f, 9.0f, -0.1f, -9.0f, 0.1f, 9.0f, 0.1f, -9.0f, -0.1f, -9.0f, 0.1f, 9.0f };            //side divider
	Matrix modelMatrix;
};

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

	background bg(program);
	float paddleCoordinates[3] = { -14.5f, 0.0f, 0.0f };
	bar leftPaddle(paddleCoordinates, program);
	paddleCoordinates[0] *= -1;
	bar rightPaddle(paddleCoordinates, program);
	ball disk(program);
	const Uint8* keyboard;

	Matrix projectionMatrix;
	Matrix modelMatrix;
	Matrix viewMatrix;

	projectionMatrix.SetOrthoProjection(-16.0f, 16.0f, -9.0f, 9.0f, -1.0f, 1.0f);
	glUseProgram(program.programID);
	SDL_Event event;
	bool done = false;
	float lastTicks = 0;
	program.SetProjectionMatrix(projectionMatrix);
	program.SetViewMatrix(viewMatrix);
	while (!done) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
		}
		glClear(GL_COLOR_BUFFER_BIT);
		keyboard = SDL_GetKeyboardState(NULL);
		if (keyboard[SDL_SCANCODE_S]) {
			leftPaddle.changeDirection(-1);
		}
		else if (keyboard[SDL_SCANCODE_W]) {
			leftPaddle.changeDirection(1);
		}
		else {
			leftPaddle.changeDirection(0);
		}
		if (keyboard[SDL_SCANCODE_UP]) {
			rightPaddle.changeDirection(1);
		}
		else if (keyboard[SDL_SCANCODE_DOWN]) {
			rightPaddle.changeDirection(-1);
		}
		else {
			rightPaddle.changeDirection(0);
		}
		float ticks = (float)SDL_GetTicks() / 1000.0f;
		float elapsed = ticks - lastTicks;
		lastTicks = ticks;
		bg.draw();
		disk.move(elapsed, leftPaddle, rightPaddle);
		leftPaddle.move(elapsed);
		rightPaddle.move(elapsed);
		disk.draw();
		leftPaddle.draw();
		rightPaddle.draw();
		int result = disk.done();
		if (result != 0) {
			while ((float)SDL_GetTicks() / 1000 < elapsed + 2) {}
			disk.reset();
			leftPaddle.reset();
			rightPaddle.reset();
		}
		glDisableVertexAttribArray(program.positionAttribute);

		SDL_GL_SwapWindow(displayWindow);
	}

	SDL_Quit();
	return 0;
}
