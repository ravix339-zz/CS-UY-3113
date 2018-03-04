#ifdef _WINDOWS
	#include <GL/glew.h>
#endif
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>
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

#define TIME_STEP_SIZE 0.00016f
#define MENU_MODE 0
#define GAME_MODE 1
#define GAME_OVER_MODE 2
#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 360

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

class TextEntity {
public:
	TextEntity(ShaderProgram& shaderProgram, GLuint& textureID) {
		program = &shaderProgram;
		texture = textureID;
	}
	void Draw(const std::string& text, float* position, float size) {
		float spacing = 0;
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
private:
	ShaderProgram * program;
	Matrix modelMatrix;
	GLuint texture;
};

class GameState {
public:
	GameState() {
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
	void Update(SDL_Event& event, bool& done) {
		const Uint8 *keys = SDL_GetKeyboardState(NULL);
		switch (currentMode) {
		case MENU_MODE:
			float xCoordinate;
			float yCoordinate;
			if (event.type == SDL_MOUSEMOTION) {
				xCoordinate = ((float)event.motion.x / WINDOW_WIDTH) * 32.0f - 16.0f;
				yCoordinate = (WINDOW_HEIGHT - (float)event.motion.y) / WINDOW_HEIGHT * 18.0f - 9.0f;

			}
			if (event.type == SDL_MOUSEBUTTONUP) {
				xCoordinate = ((float)event.button.x / WINDOW_WIDTH) * 32.0f - 16.0f;
				yCoordinate = (WINDOW_HEIGHT - (float)event.button.y) / WINDOW_HEIGHT * 18.0f - 9.0f;
			}
			break;
		case GAME_MODE:
			break;
		case GAME_OVER_MODE:
			break;
		}
	}
	void Draw() {
		TextEntity TextDrawer(*currentProgram, fonts);
		float pos[3] = { -15,0,0 };
		TextDrawer.Draw("Hello Darkness My Old Friend", pos, 1);
	}
private:
	//overhead variables
	int currentMode = MENU_MODE;

	ShaderProgram gameProgram;
	ShaderProgram menuProgram;
	ShaderProgram* currentProgram;

	Matrix projectionMatrix;
	Matrix modelMatrix;
	Matrix viewMatrix;
	
	//menu items

	//in-game elements
	std::vector<Ship*> enemies;
	Ship* player;
};

class Entity {
public:
	Entity(ShaderProgram& shaderProgram, float* pos, float rotate) {
		program = &shaderProgram;
		for (size_t positionIndex = 0; positionIndex < 3; positionIndex++) {
			position[positionIndex] = pos[positionIndex];
		}
		rotationValue = rotate;
		modelMatrix.Identity();
	}
	void draw() {
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
protected:
	ShaderProgram * program;
	GLuint texture;
	Matrix modelMatrix;
	float position[3];
	float vertices[12];
	float rotationValue = 0;
	virtual void getTexture(std::vector<float>& textureCoordinates) = 0;
};

class KillableObject : public Entity {
public:
	KillableObject(ShaderProgram& shaderProgram, float* pos, float rotate, int hpValue) : Entity(shaderProgram, pos, rotate) {
		hp = hpValue;
		texture = gameShapes;
	}
	virtual void move(float elapsed) {
		float time = elapsed;
		for (size_t positionIndex = 0; positionIndex < 3; positionIndex++) {
			position[positionIndex] += velocity[positionIndex] * elapsed;
		}
	}
protected:
	int hp;
	bool alive = true;
	float velocity[3] = { 0,0,0 };
	void readjustLivingStatus() {
		alive = (hp != 0);
	}
};

class Bullet : public KillableObject {
public:
	Bullet(ShaderProgram& shaderProgram, float* pos, int damage, float yVelocity, bool player) : KillableObject(shaderProgram, pos, 0, 1) {
		isPlayers = player;
		velocity[1] = yVelocity;
	}
protected:
	virtual void getTexture(std::vector<float>& textureCoordinates) {
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
		textureCoordinates.push_back(x);
		textureCoordinates.push_back(y);
		textureCoordinates.push_back(x);
		textureCoordinates.push_back(y + h);
		textureCoordinates.push_back(x + w);
		textureCoordinates.push_back(y);
		textureCoordinates.push_back(x + w);
		textureCoordinates.push_back(y + h);
		textureCoordinates.push_back(x + w);
		textureCoordinates.push_back(y);
		textureCoordinates.push_back(x);
		textureCoordinates.push_back(y + h);
	}
private:
	bool isPlayers;
	int damage;
};

class Ship : public KillableObject {
public:
	Ship(ShaderProgram& shaderProgram, float* pos, float xVelocity, bool player) : KillableObject(shaderProgram, pos, 0, 10) {
		velocity[0] = xVelocity;
		isPlayer = player;
		if (isPlayer) {
			rotationValue = 3.14159265f; //face upwards
		}
	}
	virtual void move(float elapsed) {
		KillableObject::move(elapsed);
	}
	void shoot() {
		if (isPlayer) {
			float newPosition[3] = { position[0], position[1] + 0.5f, position[3] };
			bullets.push_back(new Bullet(*program, newPosition, 10, 6.0f, true));
		}
		else {
			float newPosition[3] = { position[0], position[1] - 0.5f, position[3] };
			bullets.push_back(new Bullet(*program, newPosition, 4, -3.0f, false));
		}
	}
protected:
	virtual void getTexture(std::vector<float>& textureCoordinates) {
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
	}
private:
	bool isPlayer;
	std::vector<Bullet*> bullets;
	float velocity[3] = { 0,0,0 };
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
	glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
	GameState game;
	SDL_Event event;
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	fonts = LoadTexture(RESOURCE_FOLDER"font.png");
	bool done = false;
	while (!done) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
			else {
				//game.Update(event, done);
			}
		}
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
