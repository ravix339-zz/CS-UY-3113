#ifdef _WINDOWS
#include <GL/glew.h>
#endif
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>
#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include "ShaderProgram.h"
#include "Matrix.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifdef _WINDOWS
	#define RESOURCE_FOLDER ""
#else
	#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif

#define TIME_STEP_SIZE 0.016f
#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 360

using namespace std;

SDL_Window* displayWindow;
enum EntityType {PLAYER, ENEMY};
class Entity {
public:
	Entity(ShaderProgram& shaderProgram, float* pos, GLuint texture, EntityType entity) : texture(texture), type(entity) {
		program = &shaderProgram;
		for (int i = 0; i < 3; i++) {
			position[i] = pos[i];
		}
		dead = false;
		vertices[0] = 0;
		vertices[1] = 1;
		vertices[2] = 0;
		vertices[3] = 0;
		vertices[4] = 1;
		vertices[5] = 1;
		vertices[6] = 1;
		vertices[7] = 0;
		vertices[8] = 1;
		vertices[9] = 1;
		vertices[10] = 0;
		vertices[11] = 0;
		for (int i = 0; i < 12; i++) {
			vertices[i] *= 1.5;
		}
		velocity[0] = 0;
		velocity[1] = 0;
		velocity[2] = 0;
	}
	void draw() {
		modelMatrix.Identity();
		modelMatrix.Translate(position[0], position[1], position[2]);
		program->SetModelMatrix(modelMatrix);
		float texCoord[] = { 0.0f, 0.0f, 0.0f, 0.25f, 0.25f, 0.0f, 0.25f, 0.25f, 0.25f, 0.0f, 0.0f, 0.25f };
		glBindTexture(GL_TEXTURE_2D, texture);
		glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertices);
		glEnableVertexAttribArray(program->positionAttribute);
		glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoord);
		glEnableVertexAttribArray(program->texCoordAttribute);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}
	void setVelocity(float x, float y, float z= 0) {
		velocity[1] = (y != -1 ? y : velocity[1]);
		if (type == PLAYER) {
			velocity[0] = (x != -1 ? x : velocity[0]);
			velocity[2] = (z != -1 ? x : velocity[2]);
		}
	}
	void move(float elapsed) {
		if (velocity[0] > 0 && collisionFlags[3] != true && position[0] < 18.75) {
			position[0] += velocity[0] * elapsed;
		}
		if (velocity[0] < 0 && collisionFlags[2] != true && position[0] > 0) {
			position[0] += velocity[0] * elapsed;
		}
		if (collisionFlags[2] != true && position[2] > -10) {
			position[1] += velocity[1] * elapsed;
			velocity[1] -= elapsed*0.000001f;
		}
	}
	float* getPoints() {
		points[0] = vertices[1] + position[1];
		points[1] = position[1];
		points[2] = position[0];
		points[3] = position[0] + vertices[3];
		return points;
	}
	void translate(float x, float y) {
		position[0] += x;
		position[1] += y;
	}
	void setFlags(bool top, bool bottom, bool right, bool left) {
		collisionFlags[0] = top;
		collisionFlags[1] = bottom;
		collisionFlags[2] = left;
		collisionFlags[3] = right;
	}
	float* getPosition() {
		return position;
	}
protected:
	ShaderProgram * program;
	GLuint texture;
	Matrix modelMatrix;
	Matrix viewMatrix;
	Matrix projectionMatrix;
	float position[3];
	float vertices[12];
	float points[4];
	float rotationValue;
	float velocity[3]; //x,y,z
	bool collisionFlags[4]; //top, bottom, left, right
	bool dead;
	EntityType type;
};

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

class Game {
public:
	Game() {}
	Game(ifstream* map, GLuint* textures);
	void Update(SDL_Event& event, bool& done);
	void Draw() {
		drawBackground();
		for (Entity* entity : entities) {
			entity->draw();
		}
	}
private:
	int width;
	int height;
	int** tilemap;
	vector<vector<int>> map; //some reason other one stopped working for collisions
	float tileSize;
	GLuint textures[3];
	float lastTicks;
	Matrix projectionMatrix;
	Matrix modelMatrix;
	Matrix viewMatrix;
	ShaderProgram program;
	vector<Entity*> entities;
	void Collision() {}
	bool readHeader(ifstream* map);
	void readLayer(ifstream* map);
	void drawBackground();
	void worldToTileMap(float worldX, float worldY, int* gridX, int* gridY);
};

Game::Game(ifstream* map, GLuint* texture) {
	program.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
	//program.Load(RESOURCE_FOLDER"vertex.glsl", RESOURCE_FOLDER"fragment.glsl");
	projectionMatrix.SetOrthoProjection(-16.0f, 16.0f, -9.0f, 9.0f, -1.0f, 1.0f);
	program.SetModelMatrix(modelMatrix);
	program.SetProjectionMatrix(projectionMatrix);
	program.SetViewMatrix(viewMatrix);
	glUseProgram(program.programID);
	tileSize = 70.0f;
	for (int i = 0; i < 3; i++) {
		textures[i] = texture[i];
	}
	width = -1;
	height = 1;
	string line;
	while (getline(*map, line)) {
		if (line == "[header]" && !readHeader(map)) {
			assert(false);
		}
		else if (line == "[layer]") {
			readLayer(map);
		}
	}
	lastTicks = 0;
	float pos[3] = { 0,-8.2,0 };
	entities.push_back(new Entity(program, pos, textures[0], PLAYER));
	pos[1] += 3;
	pos[0] += 8;
	entities.push_back(new Entity(program, pos, textures[2], ENEMY));
}
void Game::Update(SDL_Event& event, bool& done) {
	float ticks = SDL_GetTicks();
	float elapsed = ticks - lastTicks;
	bool t, b, r, l;
	float buffer = 0.01f;
	lastTicks = ticks;
	const Uint8 *keyboard = SDL_GetKeyboardState(NULL);
	float xV = 0;
	float yV = -1;
	if (keyboard[SDL_SCANCODE_RIGHT]) {
		xV = 0.001f;
	}
	else if (keyboard[SDL_SCANCODE_LEFT]) {
		xV = -0.001f;
	}
	else if (keyboard[SDL_SCANCODE_UP]) {
		yV = 0.001f;
	}
	entities[0]->setVelocity(xV, yV, 0);
	for (Entity* entity : entities) {
		int x, y;
		t = false;
		b = false;
		r = false;
		l = false;
		entity->move(elapsed);
		float* places = entity->getPoints(); //top bottom left right
		float* positions = entity->getPosition();
		worldToTileMap(positions[0]+0.75, positions[1]+0.2, &x, &y);
		if (tilemap[y][x] != 0) {
			b = true;
			entity->setVelocity(-1, 0, 0);
		}
		worldToTileMap(positions[0] + 0.75, positions[1] + 1.5, &x, &y);
		if (tilemap[y][x] != 0) {
			t = true;
			entity->setVelocity(-1, -0.0001f, 0);
		}
		worldToTileMap(positions[0] + 1.4, positions[1] + 0.75, &x, &y);
		if (tilemap[y][x] != 0) {
			r = true;
			entity->setVelocity(0, -1, 0);
		}
		worldToTileMap(positions[0] - 0.1, positions[1] + 0.75, &x, &y);
		if (tilemap[y][x] != 0) {
			l = true;
			entity->setVelocity(0, -1, 0);
		}
		entity->setFlags(t, b, r, l);
	}
	if (entities.size() > 1) {
		int x1, y1;
		int x2, y2;
		worldToTileMap(entities[0]->getPosition()[0] + 0.75, entities[0]->getPosition()[1] + 0.5, &x1, &y1);
		worldToTileMap(entities[1]->getPosition()[0] + 0.75, entities[1]->getPosition()[1] + 0.5, &x2, &y2);
		if (x2 == x1 && y1 == y2) {
			delete entities[1];
			entities.pop_back();
		}
	}
}
bool Game::readHeader(ifstream* map) {
	string line;
	while (getline(*map, line)) {
		if (line == "") { break; }
		istringstream stream(line);
		string key, value;
		getline(stream, key, '=');
		getline(stream, value);
		if (key == "width") { width = atoi(value.c_str()); }
		else if (key == "height") { height = atoi(value.c_str()); }
	}
	if (height == -1 || width == -1) { return false; }
	int** tiles = new int*[height];
	for (int i = 0; i < height; i++) {
		tiles[i] = new int[width];
	}
	tilemap = tiles;
	return true;
}
void Game::readLayer(ifstream* map) {
	string line;
	while (getline(*map, line)) {
		if (line == "") { break; }
		istringstream stream(line);
		string key, value;
		getline(stream, key, '=');
		getline(stream, value);
		if (value == "Terrain") {
			getline(*map, line);
			for (int y = 0; y < height; y++) {
				getline(*map, line);
				istringstream lineStream(line);
				string tile;
				for (int x = 0; x < width; x++) {
					getline(lineStream, tile, ',');
					int val = atoi(tile.c_str());
					tilemap[y][x] = ((val > 0) ? val : 0);
				}
			}
		}
	}
}
void Game::drawBackground() {
	vector<float> vertexData;
	vector<float> textureCoordinates;
	int lol[5] = { 0,0,0,0,0 };
	string var;
	float dim = 350.0f;
	float x, y;
	float w = 1 / dim;
	float h = 1 / dim;
	for (int yCoordinate = 0; yCoordinate < height; yCoordinate++) {
		for (int xCoordinate = 0; xCoordinate < width; xCoordinate++) {
			switch (tilemap[yCoordinate][xCoordinate]) {
			case 0:
				x = -1;
				y = -1;
				break;
			case 1:
				x = tileSize * 0;
				y = tileSize * 0;
				break;
			case 4:
				x = tileSize * 3;
				y = tileSize * 0;
				break;
			case 5:
				x = tileSize * 4;
				y = tileSize * 0;
				break;
			case 8:
				x = tileSize * 2;
				y = tileSize * 1;
				break;
			case 11:
				x = tileSize * 0;
				y = tileSize * 2;
				break;
			}
			if (x == -1 || y == -1) { continue; }
			textureCoordinates.insert(textureCoordinates.end(), { 
				x / dim, y / dim,
				x / dim, (y + tileSize) / dim,
				(x + tileSize) / dim, y / dim,
				(x + tileSize) / dim, (y + tileSize) / dim,
				x / dim, (y + tileSize) / dim,
				(x + tileSize) / dim, y / dim });
			vertexData.insert(vertexData.end(), {
				(float)xCoordinate, (float)-1* yCoordinate,
				(float)xCoordinate, (float)-1* yCoordinate - 1,
				(float)(xCoordinate + 1), (float)-1* yCoordinate,
				(float)(xCoordinate + 1), (float)-1* yCoordinate - 1,
				(float)(xCoordinate), (float)-1* yCoordinate - 1,
				(float)(xCoordinate + 1), (float)-1 * yCoordinate });
		}
	}
	float xPos = entities[0]->getPosition()[0];
	float yPos = entities[0]->getPosition()[1];
	modelMatrix.Identity();
	projectionMatrix.Identity();
	projectionMatrix.SetOrthoProjection(-16.0f, 16.0f, -9.0f, 9.0f, -1.0f, 1.0f);
	projectionMatrix.Scale(3, 3, 1);
	viewMatrix.Identity();
	viewMatrix.Translate((xPos < 9.3 ? -5.4 - xPos : -14.7), -2.2 - yPos, 0);
	program.SetModelMatrix(modelMatrix);
	program.SetProjectionMatrix(projectionMatrix);
	program.SetViewMatrix(viewMatrix);
	glUseProgram(program.programID);
	glBindTexture(GL_TEXTURE_2D, textures[1]);
	glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
	glEnableVertexAttribArray(program.positionAttribute);
	glVertexAttribPointer(program.texCoordAttribute, 2, GL_FLOAT, false, 0, textureCoordinates.data());
	glEnableVertexAttribArray(program.texCoordAttribute);

	glDrawArrays(GL_TRIANGLES, 0, vertexData.size() / 2);
}
void Game::worldToTileMap(float worldX, float worldY, int* gridX, int* gridY) {
	*gridX = (int)(worldX);
	*gridY = ceil(-worldY - 1);
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
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GLuint player = LoadTexture("player.png");
	GLuint terrain = LoadTexture("terrain.png");
	GLuint enemy = LoadTexture("enemy.png");
	GLuint textures[] = { player,terrain, enemy};
	ifstream map("map.txt");
	Game game(&map, textures);

	SDL_Event event;
	bool done = false;
	while (!done) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
		}
		game.Update(event, done);
		glClear(GL_COLOR_BUFFER_BIT);
		game.Draw();
		SDL_GL_SwapWindow(displayWindow);
	}

	SDL_Quit();
	return 0;
}
