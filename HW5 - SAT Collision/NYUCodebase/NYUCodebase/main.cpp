#ifdef _WINDOWS
	#include <GL/glew.h>
#endif
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>
#include <vector>
#include <math.h>
#include "ShaderProgram.h"
#include "Matrix.h"
#include "SatCollision.h"
#ifdef _WINDOWS
	#define RESOURCE_FOLDER ""
#else
	#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif
using namespace std;
SDL_Window* displayWindow;

class Triangle {
public:
	Triangle(float* pos, float rotate, float size, float xV, float yV, ShaderProgram& shader) : scale(size), rotation(rotate), program(&shader) {
		for (int i = 0; i < 3; i++) {
			position[i] = pos[i];
		}
		velocity[0] = xV;
		velocity[1] = yV;
		velocity[2] = 0;
		modelMatrix.Identity();
		modelMatrix.Scale(scale, scale, 1);
		modelMatrix.Rotate(rotation);
		modelMatrix.Translate(position[0], position[1], position[2]);
		TriVertices[0] = 0.5f;
		TriVertices[1] = -0.5f;
		TriVertices[2] = 0.0f;
		TriVertices[3] = 0.5f;
		TriVertices[4] = -0.5f;
		TriVertices[5] = -0.5f;
	}
	void Update(float elapsed) {
		for (int i = 0; i < 3; i++) {
			position[i] += velocity[i] * elapsed;
		}
		modelMatrix.Identity();
		modelMatrix.Scale(scale, scale, 1);
		modelMatrix.Rotate(rotation);
		modelMatrix.Translate(position[0], position[1], position[2]);
	}
	void Draw() {
		program->SetModelMatrix(modelMatrix);
		glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, TriVertices);
		glEnableVertexAttribArray(program->positionAttribute);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		glDisableVertexAttribArray(program->positionAttribute);
	}
	bool Collision(Triangle& other) {
		computeWorldCoordinate();
		other.computeWorldCoordinate();
		pair<float, float> penetration;
		vector<pair<float, float>> othersPoints = other.world;
		bool collided = CheckSATCollision(world, othersPoints, penetration);
		if (collided) {
			for (int i = 0; i < 2; i++) {
				if (velocity[i] == 0 || other.velocity[i] == 0) {}
				else if (velocity[i] / fabs(velocity[i]) == other.velocity[i] / fabs(other.velocity[i])) {
					velocity[i] *= -1;
					other.velocity[i] *= -1;
				}
				else {
					velocity[i] -= other.velocity[i];
					other.velocity[i] += velocity[i];
				}
			}
		}
		return collided;
	}
private:
	float position[3];
	float rotation;
	float scale;
	Matrix modelMatrix;
	float velocity[3];
	float TriVertices[6];
	ShaderProgram* program;
	vector<pair<float, float>> world;
	void computeWorldCoordinate() {
		float Sx = scale;
		float Sy = scale;
		float c = cos(rotation); //cos
		float s = sin(rotation); //sin
		float Tx = position[0];
		float Ty = position[1];
		world.clear();
		for (int i = 0; i < 6; i+=2) {
			float X = TriVertices[i+0];
			float Y = TriVertices[i+1];
			float worldX = Sx * X * c - Sx * Y * s + Sx * Tx*c - Sx * Ty*s;
			float worldY = Sy * X * s + Sy * Y * c + Sy * Tx*s + Sy * Ty*c;
			world.push_back(make_pair(worldX, worldY));
		}
	}
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

	Matrix projectionMatrix;
	Matrix viewMatrix;
	float pos[3] = { 0,0,0 };
	Triangle e1(pos, 3.14159295f, 1.2, 0.00001f, 0.00005f, program);
	pos[1] = 1.0f;
	pos[0] = 1.6f;
	Triangle e2(pos, 3.14159295f/3.0f, 1.2, 0.0001f, 0.0001f, program);
	pos[0] = 2;
	pos[1] = -1;
	Triangle e3(pos, 0, 0.5, 0.001f, 0, program);
	projectionMatrix.SetOrthoProjection(-3.55, 3.55, -2.0f, 2.0f, -1.0f, 1.0f);
	glUseProgram(program.programID);
	SDL_Event event;
	float lastTicks = 0;
	bool done = false;
	while (!done) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
		}
		float ticks = SDL_GetTicks();
		float elapsed = lastTicks - ticks;
		lastTicks = ticks;
		glClear(GL_COLOR_BUFFER_BIT);
		program.SetProjectionMatrix(projectionMatrix);
		program.SetViewMatrix(viewMatrix);
		e1.Update(elapsed);
		e2.Update(elapsed);
		e3.Update(elapsed);
		e1.Collision(e2);
		e2.Collision(e3);
		e3.Collision(e1);
		e1.Draw();
		e2.Draw();
		e3.Draw();
		SDL_GL_SwapWindow(displayWindow);
	}

	SDL_Quit();
	return 0;
}
