#ifdef _WINDOWS
	#include <GL/glew.h>
#endif
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */
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
#define MENU_MODE 0
#define GAME_MODE 1
#define GAME_OVER_MODE 2
#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 360
#define NO_MOVEMENT 0
#define LEFT -1
#define RIGHT 1
#define UP 1
#define DOWN -1

SDL_Window* displayWindow;
GLuint gameShapes;
GLuint fonts;

class Entity;
class TextEntity;
class KillableObject;
class Ship;
class Bullet;

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

class GameState {
public:
	GameState();
	~GameState();
	void Update(SDL_Event& event, bool& done);
	void Draw();
private:
	//overhead variables
	float lastTicks;
	int currentMode;
	int nextMode;
	ShaderProgram gameProgram;
	ShaderProgram menuProgram;
	ShaderProgram* currentProgram;

	bool newGame;

	Matrix projectionMatrix;
	Matrix modelMatrix;
	Matrix viewMatrix;

	//in-game elements
	const int MAX_ENEMIES;
	std::vector<Ship*> enemies;
	Ship* player;
	Mix_Chunk* playerSound;
	Mix_Chunk* enemySound;
	Mix_Music* music;
	void Collision();
};
class TextEntity {
public:
	TextEntity(ShaderProgram& shaderProgram, GLuint& textureID);
	void Draw(const std::string& text, float* position, float size);
private:
	ShaderProgram * program;
	Matrix modelMatrix;
	GLuint texture;
};
class Entity {
public:
	Entity(ShaderProgram& shaderProgram, float* pos, float rotate);
	virtual void draw();
protected:
	ShaderProgram * program;
	GLuint texture;
	Matrix modelMatrix;
	float position[3];
	float vertices[12];
	float rotationValue;
	virtual void getTexture(std::vector<float>& textureCoordinates) = 0;
};
class KillableObject : public Entity {
public:
	friend class GameState;
	KillableObject(ShaderProgram& shaderProgram, float* pos, float rotate, int hpValue);
	virtual void move(float elapsed);
	bool Immovable();

protected:
	int hp;
	bool alive = true;
	bool cantMove = false;
	int direction;
	float velocity[3];
	void readjustLivingStatus();
};
class Ship : public KillableObject {
public:
	friend class GameState;
	friend class Bullet;
	Ship(ShaderProgram& shaderProgram, float* pos, float xVelocity, bool player);
	virtual void move(float elapsed, int movementDirection);
	virtual void draw();
	void shoot();
protected:
	virtual void getTexture(std::vector<float>& textureCoordinates);
private:
	float lastFire;
	float fireRate;
	bool isPlayer;
	std::vector<Bullet*> bullets;
};
class Bullet : public KillableObject {
public:
	friend class GameState;
	friend class Ship;
	Bullet(ShaderProgram& shaderProgram, float* pos, int damage, float yVelocity, bool player, int movementDirection);
	bool collision(const Bullet& other);
	bool collision(const Ship& ship);
protected:
	virtual void getTexture(std::vector<float>& textureCoordinates);
private:
	bool isPlayers;
	int damage;
};

//Game State
GameState::GameState() : MAX_ENEMIES(11), currentMode(MENU_MODE), nextMode(MENU_MODE) {
	Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);
	enemySound = Mix_LoadWAV("enemyGun.wav");
	playerSound = Mix_LoadWAV("playerGun.wav");
	music = Mix_LoadMUS(RESOURCE_FOLDER"nier.mp3"); //Credits to  Square Enix for making this in their OST for NieR: Automata
	Mix_PlayMusic(music, -1);
	lastTicks = 0;

	gameProgram.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
	menuProgram.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");

	projectionMatrix.SetOrthoProjection(-16.0f, 16.0f, -9.0f, 9.0f, -1.0f, 1.0f);

	gameProgram.SetModelMatrix(modelMatrix);
	gameProgram.SetProjectionMatrix(projectionMatrix);
	gameProgram.SetViewMatrix(viewMatrix);

	menuProgram.SetModelMatrix(modelMatrix);
	menuProgram.SetProjectionMatrix(projectionMatrix);
	menuProgram.SetViewMatrix(viewMatrix);

	currentProgram = &menuProgram;

	glUseProgram(currentProgram->programID);
}
GameState::~GameState() {
	if (player) {
		delete player;
	}
	for (Ship* enemy : enemies) {
		if (enemy) {
			delete enemy;
		}
	}
	Mix_FreeChunk(playerSound);
	Mix_FreeChunk(enemySound);
	Mix_FreeMusic(music);
}
void GameState::Update(SDL_Event& event, bool& done) {
	srand(time(NULL));
	float ticks = (float)SDL_GetTicks() / 1000.0f;
	float elapsed = ticks - lastTicks;
	lastTicks = elapsed;
	currentMode = nextMode;
	const Uint8 *keyboard = SDL_GetKeyboardState(NULL);
	switch (currentMode) {
	case MENU_MODE:
		float xCoordinate;
		float yCoordinate;
		if (event.type == SDL_MOUSEMOTION) {
			xCoordinate = ((float)event.motion.x / WINDOW_WIDTH) * 32.0f - 16.0f;
			yCoordinate = (WINDOW_HEIGHT - (float)event.motion.y) / WINDOW_HEIGHT * 18.0f - 9.0f;
		}
		else if (event.type == SDL_MOUSEBUTTONUP) {
			xCoordinate = ((float)event.button.x / WINDOW_WIDTH) * 32.0f - 16.0f;
			yCoordinate = (WINDOW_HEIGHT - (float)event.button.y) / WINDOW_HEIGHT * 18.0f - 9.0f;
			if (fabs(xCoordinate) < 6.5f && (yCoordinate > 1.5f && yCoordinate < 3.5)) {
				nextMode = GAME_MODE;
				newGame = true;
			}
			else if (fabs(xCoordinate) < 2.3f && (yCoordinate > -3.5 && yCoordinate < -1.5)) {
				done = true;
			}
		}
		break;
	case GAME_MODE:
		if (newGame) {
			float shiftPos = -11.3f;
			float playerPos[3] = { 0, -6, 0 };
			player = new Ship(*currentProgram, playerPos, .03f, true);
			float enemyPos[3] = { -14, 6, 0 };
			for (int enemyNumber = 0; enemyNumber < MAX_ENEMIES; enemyNumber++) {
				enemies.push_back(new Ship(*currentProgram, enemyPos, 0, false));
				if (enemyPos[0] >= 8) {
					enemyPos[0] = shiftPos;
					enemyPos[1] -= 4;
					shiftPos = (shiftPos == -11.3f) ? -14 : -11.3f;
				}
				else {
					enemyPos[0] += 5;
				}
			}
			nextMode = GAME_MODE;
			newGame = false;
		}
		else {
			if (keyboard[SDL_SCANCODE_SPACE]) {
				player->shoot();
				Mix_PlayChannel(-1, playerSound, 0);
			}
			for (Ship* enemy : enemies) {
				if (rand() % 100 <= 10) {
					enemy->shoot();
					Mix_PlayChannel(-1, enemySound, 0);
				}
			}
			while (elapsed > TIME_STEP_SIZE) {
				if (keyboard[SDL_SCANCODE_RIGHT]) {
					player->move(TIME_STEP_SIZE, RIGHT);
				}
				else if (keyboard[SDL_SCANCODE_LEFT]) {
					player->move(TIME_STEP_SIZE, LEFT);
				}
				else {
					player->move(TIME_STEP_SIZE, NO_MOVEMENT);
				}
				for (Ship* enemy : enemies) {
					enemy->move(TIME_STEP_SIZE, NO_MOVEMENT);
				}
				Collision();
				elapsed -= TIME_STEP_SIZE;
			}
			if (keyboard[SDL_SCANCODE_RIGHT]) {
				player->move(elapsed, RIGHT);
			}
			else if (keyboard[SDL_SCANCODE_LEFT]) {
				player->move(elapsed, LEFT);
			}
			else {
				player->move(elapsed, NO_MOVEMENT);
			}
			for (Ship* enemy : enemies) {
				enemy->move(elapsed, NO_MOVEMENT);
			}
			Collision();
		}
		break;
	case GAME_OVER_MODE:
		delete player;
		for (Ship* enemy : enemies) {
			delete enemy;
		}
		enemies.clear();
		newGame = true;
		nextMode = MENU_MODE;
		break;
	}
}
void GameState::Draw() {
	TextEntity TextDrawer(*currentProgram, fonts);
	float pos[3] = { 0, 0, 0 };
	switch (currentMode) {
	case MENU_MODE:
		currentProgram = &menuProgram;
		pos[0] = -14.0f;
		pos[1] = 6.0f;
		TextDrawer.Draw("SPACE INVADERS", pos, 3);
		pos[0] = -6.0f;
		pos[1] = 2.5;
		TextDrawer.Draw("Start Game", pos, 2);
		pos[0] = -2;
		pos[1] = -2.5;
		TextDrawer.Draw("Exit", pos, 2);
		break;
	case GAME_MODE:
		currentProgram = &gameProgram;
		player->draw();
		for (Ship* enemy : enemies) {
			enemy->draw();
		}
		break;
	case GAME_OVER_MODE:
		break;
	}
}
void GameState::Collision() {
	for (int enemyNumber = 0; enemyNumber < enemies.size(); enemyNumber++) { //bullet-bullet collision
		for (int bulletNumber = 0; bulletNumber < enemies[enemyNumber]->bullets.size(); bulletNumber++) {
			for (int laserNumber = 0; laserNumber < player->bullets.size(); laserNumber++) {
				if (enemies[enemyNumber]->bullets[bulletNumber]->collision(*player->bullets[laserNumber])) {
					delete enemies[enemyNumber]->bullets[bulletNumber];
					delete player->bullets[laserNumber];
					enemies[enemyNumber]->bullets.erase(enemies[enemyNumber]->bullets.begin() + bulletNumber);
					player->bullets.erase(player->bullets.begin() + laserNumber);
					break;
				}
			}
		}
	}

	for (int bulletNumber = 0; bulletNumber < player->bullets.size(); bulletNumber++) { //bullet-enemy collision
		for (int enemyNumber = 0; enemyNumber < enemies.size(); enemyNumber++) {
			if (player->bullets[bulletNumber]->collision(*enemies[enemyNumber])) {
				delete player->bullets[bulletNumber];
				delete enemies[enemyNumber];
				player->bullets.erase(player->bullets.begin() + bulletNumber);
				enemies.erase(enemies.begin() + enemyNumber);
				break;
			}
		}
	}

	for (int enemyNumber = 0; enemyNumber < enemies.size(); enemyNumber++) { //bullet - player collision
		for (int bulletNumber = 0; bulletNumber < enemies[enemyNumber]->bullets.size(); bulletNumber++) {
			if (enemies[enemyNumber]->bullets[bulletNumber]->collision(*player)) {
				player->hp -= enemies[enemyNumber]->bullets[bulletNumber]->damage;
				player->readjustLivingStatus();
				delete enemies[enemyNumber]->bullets[bulletNumber];
				enemies[enemyNumber]->bullets.erase(enemies[enemyNumber]->bullets.begin() + bulletNumber);
				break;
			}
		}
	}

 	if (player->alive == false || enemies.size() == 0) {
		nextMode = GAME_OVER_MODE;
	}
}

//Text Entity
TextEntity::TextEntity(ShaderProgram& shaderProgram, GLuint& textureID) {
	program = &shaderProgram;
	texture = textureID;
}
void TextEntity::Draw(const std::string& text, float* position, float size) {
	float spacing = -0.8f;
	float texture_size = 1.0 / 16.0f;
	std::vector<float> vertexData;
	std::vector<float> texCoordData;
	for (int i = 0; i < text.size(); i++) {
		int spriteIndex = (int)text[i];
		float texture_x = (float)(spriteIndex % 16) / 16.0f;
		float texture_y = (float)(spriteIndex / 16) / 16.0f;
		vertexData.insert(vertexData.end(), {
			((size + spacing) * i) + (-0.5f * size), 0.5f * size,
			((size + spacing) * i) + (-0.5f * size), -0.5f * size,
			((size + spacing) * i) + (0.5f * size), 0.5f * size,
			((size + spacing) * i) + (0.5f * size), -0.5f * size,
			((size + spacing) * i) + (0.5f * size), 0.5f * size,
			((size + spacing) * i) + (-0.5f * size), -0.5f * size,
			});
		texCoordData.insert(texCoordData.end(), {
			texture_x, texture_y,
			texture_x, texture_y + texture_size,
			texture_x + texture_size, texture_y,
			texture_x + texture_size, texture_y + texture_size,
			texture_x + texture_size, texture_y,
			texture_x, texture_y + texture_size,
			});
	}
	glBindTexture(GL_TEXTURE_2D, texture);
	glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
	glEnableVertexAttribArray(program->positionAttribute);
	glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoordData.data());
	glEnableVertexAttribArray(program->texCoordAttribute);

	modelMatrix.Identity();
	modelMatrix.Translate(position[0], position[1], position[2]);
	program->SetModelMatrix(modelMatrix);
	glDrawArrays(GL_TRIANGLES, 0, vertexData.size() / 2);
}

//Entity
Entity::Entity(ShaderProgram& shaderProgram, float* pos, float rotate) {
	program = &shaderProgram;
	for (int positionIndex = 0; positionIndex < 3; positionIndex++) {
		position[positionIndex] = pos[positionIndex];
	}
	rotationValue = rotate;
	modelMatrix.Identity();
}
void Entity::draw() {
	glBindTexture(GL_TEXTURE_2D, texture);
	glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertices);
	glEnableVertexAttribArray(program->positionAttribute);
	std::vector<float> textureCoordinates;
	getTexture(textureCoordinates);
	glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, textureCoordinates.data());
	glEnableVertexAttribArray(program->texCoordAttribute);
	modelMatrix.Identity();
	modelMatrix.Translate(position[0], position[1], position[2]);
	modelMatrix.Rotate(rotationValue);
	program->SetModelMatrix(modelMatrix);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

//Killable Object
KillableObject::KillableObject(ShaderProgram& shaderProgram, float* pos, float rotate, int hpValue) : Entity(shaderProgram, pos, rotate) {
	direction = NO_MOVEMENT;
	hp = hpValue;
	texture = gameShapes;
}
void KillableObject::move(float elapsed) {
	float time = elapsed;
	for (int positionIndex = 0; positionIndex < 3; positionIndex++) {
		position[positionIndex] += direction * velocity[positionIndex] * elapsed;
	}
	if (position[0] > 15.0f) {
		position[0] = 15.0f;
	}
	else if (position[0] < -13.5f && velocity[0] != 0) {
		position[0] = -13.5f;
	}
	if (fabs(position[1]) > 8.5f) {
		//position[1] = direction * 8.5f;
		cantMove = true;
	}
}
bool KillableObject::Immovable() {
	return cantMove;
}
void KillableObject::readjustLivingStatus() {
	alive = (hp > 0);
}

//Bullet
Bullet::Bullet(ShaderProgram& shaderProgram, float* pos, int bulletDamage, float yVelocity, bool player, int movementDirection) : KillableObject(shaderProgram, pos, 0, 1) {
	damage = bulletDamage;
	direction = movementDirection;
	isPlayers = player;
	velocity[0] = 0;
	velocity[1] = yVelocity;
	velocity[2] = 0;
}
bool Bullet::collision(const Bullet& other) {
	float xMin = position[0];
	float xMax = 0.8f + position[0];
	float yMax = vertices[1] + position[1];
	float yMin = position[1];

	float xMin_o = other.position[0];
	float xMax_o = 0.8f + other.position[0];
	float yMax_o = other.vertices[1] + other.position[1];
	float yMin_o = other.position[1];

	if (yMin > yMax_o || yMax < yMin_o || xMin > xMax_o || xMax < xMin_o) {
		return false;
	}
	return true;
}
bool Bullet::collision(const Ship& ship) {
	if (ship.isPlayer == isPlayers) {
		return false;
	}
	float xMin = position[0];
	float xMax = 0.8f + position[0];
	float yMax = vertices[1] + position[1];
	float yMin = position[1];
	
	float xMin_o = ship.position[0];
	float xMax_o = 3.0f + ship.position[0];
	float yMax_o = ship.vertices[1] + ship.position[1];
	float yMin_o = ship.position[1];
	
	if (ship.isPlayer) { //different because of the rotation by 180 degrees.
		xMin_o = ship.position[0] - 3.0f;
		xMax_o = ship.position[0];
		yMax_o = ship.position[1];
		yMin_o = ship.position[1] - ship.vertices[1];
	}

	if (yMin > yMax_o || yMax < yMin_o || xMin > xMax_o || xMax < xMin_o) {
		return false;
	}
	return true;
}
void Bullet::getTexture(std::vector<float>& textureCoordinates) {
	float x, y, w, h;
	float imageSize = 1024.0f;
	if (isPlayers) { //laserBlue07
		x = 856.0f / imageSize;
		y = 775.0f / imageSize;
		w = 9.0f / imageSize;
		h = 37.0f / imageSize;
	}
	else { //laserRed07
		x = 856.0f / imageSize;
		y = 131.0f / imageSize;
		w = 9.0f / imageSize;
		h = 37.0f / imageSize;
	}
	float ratio = h / w;

	textureCoordinates.push_back(x);
	textureCoordinates.push_back(y);
	vertices[0] = 0;
	vertices[1] = ratio * 0.8f;

	textureCoordinates.push_back(x);
	textureCoordinates.push_back(y + h);
	vertices[2] = 0;
	vertices[3] = 0;

	textureCoordinates.push_back(x + w);
	textureCoordinates.push_back(y);
	vertices[4] = 0.8f;
	vertices[5] = ratio * 0.8f;

	textureCoordinates.push_back(x + w);
	textureCoordinates.push_back(y + h);
	vertices[6] = 0.8f;
	vertices[7] = 0;

	textureCoordinates.push_back(x + w);
	textureCoordinates.push_back(y);
	vertices[8] = 0.8f;
	vertices[9] = ratio * 0.8f;

	textureCoordinates.push_back(x);
	textureCoordinates.push_back(y + h);
	vertices[10] = 0;
	vertices[11] = 0;
}

//Ship
Ship::Ship(ShaderProgram& shaderProgram, float* pos, float xVelocity, bool player) : KillableObject(shaderProgram, pos, 0, 10) {
	velocity[0] = xVelocity;
	velocity[1] = 0;
	velocity[2] = 0;
	lastFire = 0;
	isPlayer = player;
	if (isPlayer) {
		fireRate = 3;
		rotationValue = 3.14159265f; //face upwards
	}
	else {
		fireRate = 1;
	}
}

void Ship::move(float elapsed, int movementDirection) {
	direction = movementDirection;
	KillableObject::move(elapsed);
	for (int bulletNumber = 0; bulletNumber < bullets.size(); bulletNumber++) {
		bullets[bulletNumber]->move(elapsed);
		if (bullets[bulletNumber]->Immovable()) {
			delete bullets[bulletNumber];
			bullets.erase(bullets.begin() + bulletNumber);
		}

	}
}
void Ship::draw() {
	KillableObject::draw();
	for (Bullet* bullet : bullets) {
		bullet->draw();
	}
}
void Ship::shoot() {
	float ticks = SDL_GetTicks() / 1000.0f;
	float elapsed = ticks - lastFire;
	if (elapsed >= 1.0f / fireRate) {
		if (isPlayer) {
			float newPosition[3] = { position[0] - 1.85f, -5.5f, position[3] };
			bullets.push_back(new Bullet(*program, newPosition, 10, 0.05f, true, UP));
		}
		else {
			float newPosition[3] = { position[0] + 1.2f, position[1] - 3.0f, position[3] };
			bullets.push_back(new Bullet(*program, newPosition, 4, 0.01f, false, DOWN));
		}
		lastFire = ticks;
	}
}
void Ship::getTexture(std::vector<float>& textureCoordinates) {
	float x, y, w, h;
	float imageSize = 1024.0f;
	if (isPlayer) { //enemyBlue2
		x = 143.0f / imageSize;
		y = 293.0f / imageSize;
		w = 104.0f / imageSize;
		h = 84.0f / imageSize;
	}
	else { //enemyRed3
		x = 224.0f / imageSize;
		y = 580.0f / imageSize;
		w = 103.0f / imageSize;
		h = 84.0f / imageSize;
	}
	float ratio = h / w;

	textureCoordinates.push_back(x);
	textureCoordinates.push_back(y);
	vertices[0] = 0;
	vertices[1] = ratio * 3.0f;

	textureCoordinates.push_back(x);
	textureCoordinates.push_back(y + h);
	vertices[2] = 0;
	vertices[3] = 0;

	textureCoordinates.push_back(x + w);
	textureCoordinates.push_back(y);
	vertices[4] = 3;
	vertices[5] = ratio * 3.0f;

	textureCoordinates.push_back(x + w);
	textureCoordinates.push_back(y + h);
	vertices[6] = 3;
	vertices[7] = 0;

	textureCoordinates.push_back(x + w);
	textureCoordinates.push_back(y);
	vertices[8] = 3;
	vertices[9] = ratio * 3;

	textureCoordinates.push_back(x);
	textureCoordinates.push_back(y + h);
	vertices[10] = 0;
	vertices[11] = 0;
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
	glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
	GameState game;
	SDL_Event event;
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	fonts = LoadTexture(RESOURCE_FOLDER"font.png");
	gameShapes = LoadTexture(RESOURCE_FOLDER"sheet.png");
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
		//float vertices[] = { 0.5f, -0.5f, 0.0f, 0.5f, -0.5f, -0.5f };
		//glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
		//glEnableVertexAttribArray(program.positionAttribute);

		//glDrawArrays(GL_TRIANGLES, 0, 3);

		//glDisableVertexAttribArray(program.positionAttribute);

		SDL_GL_SwapWindow(displayWindow);
	}

	SDL_Quit();
	return 0;
}
