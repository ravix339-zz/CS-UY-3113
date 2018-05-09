#ifdef _WINDOWS
#include <GL/glew.h>
#endif
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <map>
#include <string>
#include <vector>
#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */
#include "ShaderProgram.h"
#include "Matrix.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifdef _WINDOWS
#define RESOURCE_FOLDER ""
#include <Windows.h>
#else
#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif

#define TIME_STEP_SIZE 0.0016f //Used to ensure that collisions are still detected properly during lag spikes

//State Modes for GameState
#define MENU_MODE 0 
#define GAME_MODE 1
#define GAME_OVER_MODE 2

//Game State Constants
#define PLAYER_Vx 3.5
#define PLAYER_Vy 3.5
#define BOARD_LENGTH 50
#define BOARD_HEIGHT 18

#define PI 3.141592653 //An approximation of Pi.

//Screen Definitions
#define WINDOW_HEIGHT 1920
#define WINDOW_WIDTH 1080
#define FULLSCREEN_MODE  //Comment out this line in not have a fullscreen game.
using namespace std;

class GameState {
public:
	/* GameState()
		\description - Constructor
	*/
	GameState() {
		//Initialize Matrices for ShaderProgram
		modelMatrix.Identity();
		viewMatrix.Identity();
		projectionMatrix.Identity();
		projectionMatrix.SetOrthoProjection(-16.0f, 16.0f, -9.0f, 9.0f, -1.0f, 1.0f);

		//Load .glsl files into program for textured drawings
		program.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");

		glUseProgram(program.programID); //Tell OpenGL to use the program

		//Set program to use the matrices that were initialized
		program.SetModelMatrix(modelMatrix);
		program.SetProjectionMatrix(projectionMatrix);
		program.SetViewMatrix(viewMatrix);

		lastTicks = 0;

		currentState = MENU_MODE; //set initial state of the game to be in the MENU_MODE
		nextState = MENU_MODE; // set the next state (on next Update call) to also be the MENU_MODE

		Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096); //Open Audio channels to allow for music and sounds to be played

		drawTime = -1; //Animation draw time for menu
		clickable = false; //Initially set it so that the user cannot click on the menu during the animation
		frames = 0; //so far 0 frames have been drawn so we will initialize this to 0


		//Insert gun sounds for use when firing a gun
		gunSoundMap.insert(pair<string, Mix_Chunk*>("sniper", Mix_LoadWAV("sniper.wav")));
		gunSoundMap.insert(pair<string, Mix_Chunk*>("shotgun", Mix_LoadWAV("shotgun.wav")));
		gunSoundMap.insert(pair<string, Mix_Chunk*>("shotgun_r", Mix_LoadWAV("shotgun_r.wav"))); //Shotgun reloading sound
		gunSoundMap.insert(pair<string, Mix_Chunk*>("rifle", Mix_LoadWAV("rifle.wav")));
		//guns sounds are from SoundBible.com (http://soundbible.com/tags-gun.html)

		//Load Music from files
		menuMusic = Mix_LoadMUS("menuMusic.mp3"); //Copyright from Nintendo Games: This is the Wii Mii Channel song.
		gameMusic = Mix_LoadMUS("gameMusic.mp3"); //from www.BenSound.com

		//Insert textures for use in drawing
		textureMap.insert(pair<string, GLuint>("font", LoadTexture("font2.png")));
		textureMap.insert(pair<string, GLuint>("terrain", LoadTexture("terrain.png")));
		textureMap.insert(pair<string, GLuint>("guns", LoadTexture("guns.png"))); //gun sheet edited from TdeLeeuw (http://fav.me/d8etym8)
		textureMap.insert(pair<string, GLuint>("character1", LoadTexture("character1.png")));
		textureMap.insert(pair<string, GLuint>("bullet", LoadTexture("bullet.png")));
		textureMap.insert(pair<string, GLuint>("character2", LoadTexture("character2.png")));
	}

	/* ~GameState()
		\description - Destructor
	*/
	~GameState() {
		//Free up all gun sounds and music
		for (map<string, Mix_Chunk*>::iterator i = gunSoundMap.begin(); i != gunSoundMap.end(); ++i) {
			Mix_FreeChunk(i->second);
		}
		Mix_FreeMusic(menuMusic);
		Mix_FreeMusic(gameMusic);

		//If variables were not deleted already (via GAME_OVER_MODE) delete them
		if (playerOne != nullptr) {
			delete playerOne;
			delete playerTwo;
			delete gunOne;
			delete gunTwo;
			delete board;
			for (Bullet* bullet : bullets) {
				delete bullet;
			}
			bullets.clear();
		}
	}

	/* Update()
		\description  - updates the game's state and elements within the game
		\param event  - event that is read to control the flow of the game
		\param done   - tells external loop if game is over
	*/
	void Update(SDL_Event& event, bool&done) {
		float ticks = (float)(SDL_GetTicks()) / 1000.0f; //Get the current number of seconds that SDL has been running
		float elapsed = ticks - lastTicks; //Calculate the time that has elapsed between the last update call and now
		if (drawTime != -1) { //If the animation has started, then increase the time that has elapsed for the animation
			drawTime += elapsed;
		}
		else { //Otherwise start the animation
			drawTime = 0;
		}

		lastTicks = ticks; //Update the lastTicks
		currentState = nextState; //Update the currentState of the game

		float time = elapsed; //Initialize another variable that updates the game in increments of TIME_STEP_SIZE

		float pos[3] = { 0,0,0 }; //Initialize position for various game elements {x,y,z} although z-coordinates are not used in this game

		const Uint8 *keyboard = SDL_GetKeyboardState(NULL); //Get the state of the keyboard
		
		if (keyboard[SDL_SCANCODE_ESCAPE]) { //If the ESCAPE key is pressed, quit the game after this round of updating.
			done = true;
		}
		
		//Based upon the current state of the game update the game.
		switch (currentState) {
			//If we are in the menu
		case MENU_MODE:
			float xCoordinate;
			float yCoordinate;
			if (event.type == SDL_MOUSEMOTION) {

			}
			else if (event.type == SDL_MOUSEBUTTONUP) { //If the mouse clicked
				xCoordinate = ((float)event.button.x / WINDOW_WIDTH) * 32.0f - 16.0f; //Calculate the relative xCoordinate of the mouse
				yCoordinate = (WINDOW_HEIGHT - (float)event.motion.y) / WINDOW_HEIGHT * 18.0f - 9.0f; //Calculate relative yCoordinate of the mouse
				if (fabs(12.3997f - xCoordinate) < 9.6447f && fabs(yCoordinate - 4) < 0.35f && clickable) { //if the mouse is hovering over "START GAME" and the animation is over
					nextState = GAME_MODE; //Start the game
					newGame = true; //Make sure the game knows that it is a new game.
				}
				else if (fabs(xCoordinate - 11.748f) < 3.6 && fabs(yCoordinate - 1.16666f) < 0.333 && clickable) { //If the mouse is hovering over "EXIT" and the animation is over
					done = true; //Quit the game
				}
			}
			break;
			
			//If we are in-game
		case GAME_MODE:
			if (newGame) {
				//For a new game
				newGame = false; //we are no longer in a new game after this
				Mix_HaltMusic(); //Stop the menu music
				Mix_PlayMusic(gameMusic, -1); //Begin the game-music

				board = new Map(BOARD_LENGTH, BOARD_HEIGHT, 0.93f, program, textureMap["terrain"]); //Create a new game board

				//initialize start position of playerOne
				pos[0] = 13;
				pos[1] = -10;
				playerOne = new Character(1, pos, textureMap["character1"], program); //Create playerOne
				
				//Adjust start position of playerTwo
				pos[0] = 20;
				playerTwo = new Character(2, pos, textureMap["character2"], program); //Create playerTwo

				//Create the guns for playerOne and playerTwo (gunOne and gunTwo respectively)
				gunOne = new Gun(*playerOne, textureMap["guns"], program, textureMap["bullet"], gunSoundMap);
				gunTwo = new Gun(*playerTwo, textureMap["guns"], program, textureMap["bullet"], gunSoundMap);
			}
			//If the elapsed time is greater than the TIME_STEP_SIZE, we have to update incrementally
			while (time > TIME_STEP_SIZE) {
				
				//Detect Movement
				if (keyboard[SDL_SCANCODE_D] && !playerOne->collisionFlags[2]) { //If D is pressed and playerOne can move to right
					playerOne->setVelocity(PLAYER_Vx, -100.0f, -100.0f); //allow playerOne to move right
				}
				else if (keyboard[SDL_SCANCODE_A] && !playerOne->collisionFlags[1]) { //If A is pressed and playerOne can move left
					playerOne->setVelocity(-PLAYER_Vx, -100.0f, -100.0f); //allow playerOne to move left
				}
				else {
					playerOne->setVelocity(0, -100.0f, -100.0f); //otherwise set their horizontal velocity to zero.
				}
				if (keyboard[SDL_SCANCODE_W] && playerOne->collisionFlags[0]) { //if W is pressed and playerOne is on the ground
					playerOne->setVelocity(-100.0f, PLAYER_Vy, -100.0f); //make playerOne jump
				}
				
				if (keyboard[SDL_SCANCODE_RIGHT] && !playerTwo->collisionFlags[2]) { //if the right arrow is pressed and playerTwo can move right
					playerTwo->setVelocity(PLAYER_Vx, -100.0f, -100.0f); //allow playerTwo to move right
				}
				else if (keyboard[SDL_SCANCODE_LEFT] && !playerTwo->collisionFlags[1]) { //if the left arrow is pressed and playerTwo can move left
					playerTwo->setVelocity(-PLAYER_Vx, -100.0f, -100.0f); //allow playerTwo to move left
				}
				else {
					playerTwo->setVelocity(0, -100.0f, -100.0f); //otherwise set their horizontal velocity to zero. 
				}
				if (keyboard[SDL_SCANCODE_UP] && playerTwo->collisionFlags[0]) { //if playerTwo is on the ground and the up arrow is pressed
					playerTwo->setVelocity(-100.0f, PLAYER_Vy, -100.0f); //make playerTwo jump
				}

				//Shift Guns
				if (letGo[0]) { //If playerOne has let go of the buttons to shift guns
					if (keyboard[SDL_SCANCODE_Q]) { //if Q is pressed
						gunOne->ShiftGun(-1); //Rotate the gun number back one
						letGo[0] = false; //set flag that a button is pressed
					}
					else if (keyboard[SDL_SCANCODE_E]) { //if E was pressed
						gunOne->ShiftGun(1); //Rotate the gun number forward one
						letGo[0] = false; //set flag that button is pressed
					}
				}
				else if (!keyboard[SDL_SCANCODE_Q] && !keyboard[SDL_SCANCODE_E]) { //if Q & E are not pressed 
					letGo[0] = true;  //set flag that the buttons have been released
				}

				if (letGo[1]) { //If playerTwo has let go of the buttons to shift guns
					if (keyboard[SDL_SCANCODE_PAGEUP] || keyboard[SDL_SCANCODE_SLASH]) { //if PAGE_UP or Forward_Slash (/) is pressed
						gunTwo->ShiftGun(1); //Rotate the gun number forward one
						letGo[1] = false; //set flag that button is pressed
					}
					else if (keyboard[SDL_SCANCODE_PAGEDOWN] || keyboard[SDL_SCANCODE_PERIOD]) { //if PAGE_DOWN or Period (.) is pressed
						gunTwo->ShiftGun(-1); //Rotate the gun number back one
						letGo[1] = false; //set flag that button is pressed
					}
				}
				else if (!keyboard[SDL_SCANCODE_PAGEUP] && !keyboard[SDL_SCANCODE_PAGEDOWN] && !keyboard[SDL_SCANCODE_PERIOD] && !keyboard[SDL_SCANCODE_SLASH]) { 
					//If PAGE_UP, PAGE_DOWN, Forward Slash, and Period are all NOT pressed
					letGo[1] = true; //set flag that buttons have been released
				}

				//Player-Gun Movement
				playerOne->move(TIME_STEP_SIZE);
				playerTwo->move(TIME_STEP_SIZE);
				gunOne->Reposition(); //reposition guns to match player's location
				gunTwo->Reposition();
				
				//Shooting
				if (keyboard[SDL_SCANCODE_S]) { //if S is pressed
					Bullet* bullet = gunOne->Shoot(); //PlayerOne attempts to shoot a bullet
					if (bullet != nullptr) { //If they are successful, add that bullet to our vector of bullets
						bullets.push_back(bullet);
					}
				}
				if (keyboard[SDL_SCANCODE_DOWN] || keyboard[SDL_SCANCODE_RSHIFT]) { //If the down arrow or right shift are pressed
					Bullet* bullet = gunTwo->Shoot(); //PlayerTwo attempts to shoot a bullet
					if (bullet != nullptr) { //If they are successful, add that bullet to our vector of bullets
						bullets.push_back(bullet);
					}
				}
				
				//Bullet Movement
				for (Bullet* bullet : bullets) {
					bullet->move(TIME_STEP_SIZE);
				}

				//Collision detection and cleanup
				Collision(); //Check for any and all collisions
				for (int i = 0; i < bullets.size();) { //go through all of the bullets and remove all dead bullets (those which cannot exist anymore)
					if (bullets[i]->deadBullet()) {
						delete bullets[i];
						bullets.erase(bullets.begin() + i);
					}
					else {
						i++;
					}
				}
				time -= TIME_STEP_SIZE; //subtract timestamp and continue in loop
			}
			if (time > 0) { //If there is still some time remaining repeat what was done in the loop except we move by the remaining time instead of the TIME_STEP

							//Detect Movement
				if (keyboard[SDL_SCANCODE_D] && !playerOne->collisionFlags[2]) { //If D is pressed and playerOne can move to right
					playerOne->setVelocity(PLAYER_Vx, -100.0f, -100.0f); //allow playerOne to move right
				}
				else if (keyboard[SDL_SCANCODE_A] && !playerOne->collisionFlags[1]) { //If A is pressed and playerOne can move left
					playerOne->setVelocity(-PLAYER_Vx, -100.0f, -100.0f); //allow playerOne to move left
				}
				else {
					playerOne->setVelocity(0, -100.0f, -100.0f); //otherwise set their horizontal velocity to zero.
				}
				if (keyboard[SDL_SCANCODE_W] && playerOne->collisionFlags[0]) { //if W is pressed and playerOne is on the ground
					playerOne->setVelocity(-100.0f, PLAYER_Vy, -100.0f); //make playerOne jump
				}

				if (keyboard[SDL_SCANCODE_RIGHT] && !playerTwo->collisionFlags[2]) { //if the right arrow is pressed and playerTwo can move right
					playerTwo->setVelocity(PLAYER_Vx, -100.0f, -100.0f); //allow playerTwo to move right
				}
				else if (keyboard[SDL_SCANCODE_LEFT] && !playerTwo->collisionFlags[1]) { //if the left arrow is pressed and playerTwo can move left
					playerTwo->setVelocity(-PLAYER_Vx, -100.0f, -100.0f); //allow playerTwo to move left
				}
				else {
					playerTwo->setVelocity(0, -100.0f, -100.0f); //otherwise set their horizontal velocity to zero. 
				}
				if (keyboard[SDL_SCANCODE_UP] && playerTwo->collisionFlags[0]) { //if playerTwo is on the ground and the up arrow is pressed
					playerTwo->setVelocity(-100.0f, PLAYER_Vy, -100.0f); //make playerTwo jump
				}

				//Shift Guns
				if (letGo[0]) { //If playerOne has let go of the buttons to shift guns
					if (keyboard[SDL_SCANCODE_Q]) { //if Q is pressed
						gunOne->ShiftGun(-1); //Rotate the gun number back one
						letGo[0] = false; //set flag that a button is pressed
					}
					else if (keyboard[SDL_SCANCODE_E]) { //if E was pressed
						gunOne->ShiftGun(1); //Rotate the gun number forward one
						letGo[0] = false; //set flag that button is pressed
					}
				}
				else if (!keyboard[SDL_SCANCODE_Q] && !keyboard[SDL_SCANCODE_E]) { //if Q & E are not pressed 
					letGo[0] = true;  //set flag that the buttons have been released
				}

				if (letGo[1]) { //If playerTwo has let go of the buttons to shift guns
					if (keyboard[SDL_SCANCODE_PAGEUP] || keyboard[SDL_SCANCODE_SLASH]) { //if PAGE_UP or Forward_Slash (/) is pressed
						gunTwo->ShiftGun(1); //Rotate the gun number forward one
						letGo[1] = false; //set flag that button is pressed
					}
					else if (keyboard[SDL_SCANCODE_PAGEDOWN] || keyboard[SDL_SCANCODE_PERIOD]) { //if PAGE_DOWN or Period (.) is pressed
						gunTwo->ShiftGun(-1); //Rotate the gun number back one
						letGo[1] = false; //set flag that button is pressed
					}
				}
				else if (!keyboard[SDL_SCANCODE_PAGEUP] && !keyboard[SDL_SCANCODE_PAGEDOWN] && !keyboard[SDL_SCANCODE_PERIOD] && !keyboard[SDL_SCANCODE_SLASH]) {
					//If PAGE_UP, PAGE_DOWN, Forward Slash, and Period are all NOT pressed
					letGo[1] = true; //set flag that buttons have been released
				}

				//Actual Movement
				playerOne->move(time);
				playerTwo->move(time);
				gunOne->Reposition(); //reposition guns to match player's location
				gunTwo->Reposition();

				//Shooting
				if (keyboard[SDL_SCANCODE_S]) { //if S is pressed
					Bullet* bullet = gunOne->Shoot(); //PlayerOne attempts to shoot a bullet
					if (bullet != nullptr) { //If they are successful, add that bullet to our vector of bullets
						bullets.push_back(bullet);
					}
				}
				if (keyboard[SDL_SCANCODE_DOWN] || keyboard[SDL_SCANCODE_RSHIFT]) { //If the down arrow or right shift are pressed
					Bullet* bullet = gunTwo->Shoot(); //PlayerTwo attempts to shoot a bullet
					if (bullet != nullptr) { //If they are successful, add that bullet to our vector of bullets
						bullets.push_back(bullet);
					}
				}

				for (Bullet* bullet : bullets) {
					bullet->move(time);
				}

				//Collision detection and cleanup
				Collision(); //Check for any and all collisions
				for (int i = 0; i < bullets.size();) { //go through all of the bullets and remove all dead bullets (those which cannot exist anymore)
					if (bullets[i]->deadBullet()) {
						delete bullets[i];
						bullets.erase(bullets.begin() + i);
					}
					else {
						i++;
					}
				}
			}
			if (playerOne->isDead() || playerTwo->isDead()) { //If either player is dead
				nextState = GAME_OVER_MODE; //set the next game mode
				gameOverTimer = ticks; //set start time for gameOver
				Mix_HaltMusic(); //Stop the in-game music
				Mix_PlayMusic(menuMusic, -1); //start the menu music again
			}
			break;

			//if the game is over
		case GAME_OVER_MODE:
			if (ticks - gameOverTimer >= 4.7f) { //If the time between the current number of ticks and gameOver beginning is >= 4.7seconds (delay)
				if (playerOne != nullptr) { //Delete the player and all  of the other game elements
					delete playerOne;
					delete playerTwo;
					delete gunOne;
					delete gunTwo;
					delete board;
					for (Bullet* bullet : bullets) {
						delete bullet;
					}
					bullets.clear();
					playerOne = nullptr;
					playerTwo = nullptr;
					gunOne = nullptr;
					gunTwo = nullptr;
					board = nullptr;
				}
				newGame = true;
				nextState = MENU_MODE; //set the next mode to MENU_MODE
			}
			break;
		}
	}
	
	/* Draw()
		\description - Draws game elements and Text Entities
	*/
	void Draw() {
		TextEntity TextDrawer(program, textureMap["font"]); //Create an entity meant to draw Text Entities
		float pos[3] = { 0,0,0 }; //Array showing the {x,y,z} positions of a particular entity to be drawn by the TextDrawer
		float avgX = 0; //variable representing the average xCoordinates between playerOne and playerTwo
		switch (currentState) {
			//If we are in the menu
		case MENU_MODE:
			if (frames == 65) { //allows for music to sync with song (there was a part I felt would be nice to sync with animation and wouldn't in fullscreen).
				Mix_PlayMusic(menuMusic, -1);
				frames = -1;
			}
			else if (frames >= 0) { //increment the number of frames
				frames++;
			}
			if (frames != -1) { //Dont draw if the animation is not meant to start yet (removes the grosser parts of the elastic easing)
				return;
			}

			modelMatrix.Identity();
			viewMatrix.Identity();
			program.SetViewMatrix(viewMatrix); //fix view matrix to look at the center of the 2D plane for the drawings
			pos[0] = -13.75f;
			pos[1] = 6.0f;
			TextDrawer.Draw("Friendzone Spheres!:)", pos, 2.5, -1.07f, drawTime, 2, 0.0f, 4.5f, -350.0f, 0, 0.0f, 4.5f, -10); //Draw the title of the game
			pos[0] = -13.0f;
			pos[1] = 3.5f;
			TextDrawer.Draw("By: Ravi Sinha", pos, 1.7, -.75f, drawTime, 0, 5.0f, 8.0f, -16); //Draw the developer's name
			pos[0] = -5;
			pos[1] = 0;
			TextDrawer.Draw("Start Game", pos, 2, -0.9f, drawTime, 0, 6.0f, 8.5f, -22); //Add the "Start Game" button
			pos[0] = -2;
			pos[1] = -5;
			TextDrawer.Draw("Exit", pos, 2, -0.9f, drawTime, 0, 7.0f, 9.0f, -20); //Add the "Exit" button
			if (drawTime > 9.3f) { //If the animation is over (+ 0.3seconds) then make the menu clickable
				clickable = true;
			}
			break;

			//If we are in game
		case GAME_MODE:
			//Draw the game entities
			board->Draw();
			playerOne->draw();
			playerTwo->draw();
			gunOne->draw();
			gunTwo->draw();
			for (Bullet* bullet : bullets) {
				bullet->draw();
			}

			//Setting viewMatrix to follow the two characters and not to overstep the bounds of the map (does not show black portion of screen)
			viewMatrix.Identity();
			viewMatrix.Translate(0, (BOARD_HEIGHT/ 2), 0);
			pos[1] = -1;// -BOARD_HEIGHT / 2 - 2;
			avgX = (playerOne->position[0] + playerTwo->position[0]) / 2; //calculate average xCoordinate
			if (avgX < 16) { //avgX is less than the leftmost that the camera can pan
				viewMatrix.Translate(-16, 0, 0);
				pos[0] = 0.7;
			}
			else if (avgX > BOARD_LENGTH-16) { //avgX is greater than the right most the camera can pan
				viewMatrix.Translate(-(BOARD_LENGTH - 16), 0, 0);
				pos[0] = BOARD_LENGTH - 31.3;
			}
			else { //if the camera is somewhere in between
				viewMatrix.Translate(-avgX, 0, 0);
				pos[0] = avgX - 15.3;
			}
			//Draw the health remaining for each player at the top left and right corners of the screen
			TextDrawer.Draw("PLAYER ONE: " + to_string(int(playerOne->health)), pos, 1, -.4);
			pos[0] += 22;
			TextDrawer.Draw("PLAYER TWO: " + to_string(int(playerTwo->health)), pos, 1, -.4);
			program.SetViewMatrix(viewMatrix);
			glUseProgram(program.programID);
			break;

		//If the game is over
		case GAME_OVER_MODE:
			if (playerOne != nullptr) {
				viewMatrix.Identity();
				program.SetViewMatrix(viewMatrix);
				pos[0] = -13.0f;
				//Draw a text entity that states the winner
				if (playerOne->isDead()) {
					TextDrawer.Draw("Player Two Wins!", pos, 3, -1.28f);
				}
				else {
					TextDrawer.Draw("Player One Wins!", pos, 3, -1.28f);
				}
			}
			break;
		}
	}
private:
	//TextEntity Class - Provides a way to draw text onto the screen
	class TextEntity {
	public:
		/* TextEntity()
			\description         - Constructor
			\param shaderProgram - Game's ShaderProgram
			\param textureID     - ID of Font for use in the drawing
		*/
		TextEntity(ShaderProgram& shaderProgram, GLuint& textureID) {
			program = &shaderProgram;
			texture = textureID;
		}

		/* Draw():
		    \description    - Draws a string of text onto the screen
			\param text     - String to be written on screen
			\param position - Starting x,y,z coordinate from which first letter in text will be printed
			\param size     - Size of the text
			\param spacing  - Spacing between text
			\param elapsed  - Time that has elapsed from start of animation time
			\param typeX    - Type of animation that is wanted for the xDirection (0, 1, or 2)
			\param xStart   - Time at which animation in x direction begins
			\param xFinish  - Time at which animation in x direction finishes
			\param xD       - Max displacement in x direction
			\param typeY    - Type of animation wanted for yDirection (0, 1, or 2)
			\param yStart   - Time at which animation in y direction begins
			\param yFinish  - Time at which animation in y direction finishes
			\param yD       - Max displacement in y direction
		*/
		void Draw(const std::string& text, float* position, float size, float spacing, float elapsed = -1000, int typeX = 0, float xStart = -1,
			float xFinish = -1, float xD = -1, int typeY = 0, float yStart = -1, float yFinish = -1, float yD = -1) {
			
			float texture_size = 1 / 16.0f; //font sprite sheet is a 16x16 grid so textures sizes are 1/16th the size of the image
			vector<float> vertexData; //vector to store vertices to draw on the screen
			vector<float> texCoordData; //Stores texture coordinates

			for (int i = 0; i < text.size(); i++) { //Loop through the entire string
				int spriteIndex = (int)text[i]; //get the ascii character of the current letter

				float texture_x = (float)(spriteIndex % 16) / 16.0f; //get the x & y positions of the letter in the spritesheet
				float texture_y = (float)(spriteIndex / 16) / 16.0f;

				vertexData.insert(vertexData.end(), { //Insert the vertex data
					((size + spacing) * i) + (-0.5f * size), 0.5f * size,
					((size + spacing) * i) + (-0.5f * size), -0.5f * size,
					((size + spacing) * i) + (0.5f * size), 0.5f * size,
					((size + spacing) * i) + (0.5f * size), -0.5f * size,
					((size + spacing) * i) + (0.5f * size), 0.5f * size,
					((size + spacing) * i) + (-0.5f * size), -0.5f * size,
					});
				texCoordData.insert(texCoordData.end(), { //Insert the texture data
					texture_x, texture_y,
					texture_x, texture_y + texture_size,
					texture_x + texture_size, texture_y,
					texture_x + texture_size, texture_y + texture_size,
					texture_x + texture_size, texture_y,
					texture_x, texture_y + texture_size,
					});
			}
			//Set up OpenGL for drawing
			glBindTexture(GL_TEXTURE_2D, texture);
			glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
			glEnableVertexAttribArray(program->positionAttribute);
			glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoordData.data());
			glEnableVertexAttribArray(program->texCoordAttribute);

			modelMatrix.Identity();

			float x = position[0];
			float y = position[1];
			if (elapsed != -1000) { //if there is an animation
				float animationVal;
				if (xStart != -1 && xFinish != -1) { //if there's an x direction animation
					float xDif = (xD == -1.0f ? -10.0f : xD); //determine the max xDisplacement
					animationVal = mapValue(elapsed, xStart, xFinish, 0, 1); //get the animation value from the mapvalue function
					switch (typeX) { //use an animation function depending on the type of animation we want
					case 0:
						x = lerp(position[0] + xDif, position[0], animationVal);
						break;
					case 1:
						x = easeIn(position[0] + xDif, position[0], animationVal);
						break;
					case 2:
						x = easeOutElastic(position[0] + xDif, position[0], animationVal);
						break;
					default:
						x = lerp(position[0] + xDif, position[0], animationVal);
						break;
					}
				}
				if (yStart != -1 && yFinish != -1) { //if there is a y direction animation
					float yDif = (yD == -1.0f ? -10.0f : yD); //determine the max yDisplacement
					animationVal = mapValue(elapsed, yStart, yFinish, 0.0f, 1.0f); //get the animation value from the mapvalue function
					switch (typeY) { //use an animation function depending on the type of animation we want
					case 0:
						y = lerp(position[1] + yDif, position[1], animationVal);
						break;
					case 1:
						y = easeIn(position[1] + yDif, position[1], animationVal);
						break;
					case 2:
						y = easeOutElastic(position[1] + yDif, position[1], animationVal);
						break;
					default:
						y = lerp(position[1] + yDif, position[1], animationVal);
						break;
					}
				}
			}
			modelMatrix.Translate(x, y, position[2]); //form an offset depending on what the animations produced
			program->SetModelMatrix(modelMatrix); 
			glDrawArrays(GL_TRIANGLES, 0, vertexData.size() / 2); //Draw the text
		}
	private:
		/*
		 *
		 * Animation Functions
		 *
		 * \param from - initial position of animation
		 * \param to   - final position of animation
		 * \param time - time (normalized between 0 and 1) that has elapsed in animation
		 *
		 */

		//Linear Interpolation
		float lerp(float from, float to, float time) {
			return (1 - time)*from + time * to;
		}
		
		//Elastic Easing Out
		float easeOutElastic(float from, float to, float time) {
			float p = 0.3f;
			float s = p / 4.0f;
			float diff = (to - from);
			return from + diff + (diff*pow(2.0f, -10.0f*time) * sin((time - s)*(2 * PI) / p));
		}

		//Ease in
		float easeIn(float from, float to, float time) {
			float tVal = time * time*time*time*time;
			return (1.0f - tVal)*from + tVal * to;
		}

		/* mapValue
		   \description  - maps the current elapsed time to a normalized time (0 to 1) based upon the animation time
		   \param value  - elapsed time so far in animation
		   \param srcMin - minimum value for animation
		   \param srcMax - maximum value for animation
		   \param dstMin - minimum value to map the value to
		   \param dstMax - maximum value to map the value to.
		*/
		float mapValue(float value, float srcMin, float srcMax, float dstMin, float dstMax) {
			float retVal = dstMin + ((value - srcMin) / (srcMax - srcMin) * (dstMax - dstMin));
			if (retVal < dstMin) {
				retVal = dstMin;
			}
			if (retVal > dstMax) {
				retVal = dstMax;
			}
			return retVal;
		}
		ShaderProgram * program;
		Matrix modelMatrix;
		GLuint texture;
	};
	
	//Map Class - Procedurely Generates a 2D Map of given dimensions and detects collisions on the map
	class Map {
	public:
		/* Map()
			\description   - Constructor that procedurely generates a 2D map
			\param length  - Length of board to be produced
			\param height  - Height of board to be produced
			\param p       - Probability of a column growing
			\param program - Shader Program to use to draw the map
			\param texture - Texture that is used on the map when drawing
		 */
		Map(int length, int height, float p, ShaderProgram& program, GLuint texture) : height(height), length(length), program(&program), texture(texture) {
			srand(time(NULL)); //Set the seed to the current time

			//Dynamically create a 2D array with [height] rows and [length] columns.
			map = new int*[height];
			for (int i = 0; i < height; i++) {
				map[i] = new int[length];
			}
			//Initialize each element in the array to be 3 (in the texture it is used for the sky)
			for (int i = 0; i < height; i++) {
				for (int j = 0; j < length; j++) {
					map[i][j] = 3;
				}
			}

			//Set the bottom most row to be 8 (top level soil)
			for (int j = 0; j < length; j++) {
				map[height - 1][j] = 8;
			}

			for (int i = height - 2; i > height / 2 - 1; i--) { //Go from bottom to halfway up the map
				for (int j = 0; j < length; j++) { //Go through each column
					if ((j > 0 && map[i + 1][j - 1] == 3) || map[i + 1][j] == 3 || (j < length - 1 && i < height - 2 && map[i + 1][j + 1] == 3)) {
						//If we not at the ends of the map and the piece to our bottom left  or bottom right is sky, then we are also sky
						//OR if the piece below us is sky, then we are also sky.
						//First two conditions described are to ensure that there are no steep hills that cannot be climbed
						//Last condition described is to ensure that we do not have floating platforms
						map[i][j] = 3;
					}
					else if ((rand() % 101) / 100.0f < p) { //If a random number is less than our probability then we build up
						map[i][j] = 8; //set the current level to top level soil
						map[i + 1][j] = 17; //set piece below us as normal soil
					}
					else { //Otherwise make the piece sky
						map[i][j] = 3;
					}
				}
			}

			for (int i = height - 2; i > height / 2 - 1; i--) {
				for (int j = 0; j < length; j++) {
					if (map[i][j] == 8 && map[i][j - 1] == 3 && map[i][j + 1] == 3) {
						//If our current piece is sky but both pieces to the either side of us is top level soil, make our land height one less (allows for platforms)
						map[i][j] = 3;
						map[i + 1][j] = 8;
					}
				}
			}
		}

		/* ~Map()
			\description - Destructor
		*/
		~Map() {

			for (int i = 0; i < height; i++) {
				delete map[i];
			}
			delete map;
		}


		/* Draw()
			\description - Draws the map onto the screen
		*/
		void Draw() {
			vector<float> vertexData; //Holds vertex data
			vector<float> textureCoordinates; //Holds texture coordinate data
			float dim = 350.0f; //Dimensions of the texture
			float tileSize = 70.0f; //Size of each sprite on the texture
			float x, y;
			float w = 1 / dim; //height of each sprite in texture coordinates
			float h = 1 / dim; //width of each sprite in texture coordinates
			for (int yCoordinate = 0; yCoordinate < height; yCoordinate++) {
				for (int xCoordinate = 0; xCoordinate < length; xCoordinate++) { //Go through the entire board
					switch (map[yCoordinate][xCoordinate]) {
					case 0: //if the piece is zero (shouldn't happen)
						x = -1;
						y = -1;
						break;
					case 3: //if the piece is three (sky)
						x = tileSize * 3;
						y = tileSize * 0;
						break;
					case 8: //if the piece is eight (top soil)
						x = tileSize * 2;
						y = tileSize * 1;
						break;
					case 17: //if the piece is 17 (normal soil)
						x = tileSize * 1;
						y = tileSize * 3;
						break;
					}
					if (x == -1 || y == -1) { continue; }
					textureCoordinates.insert(textureCoordinates.end(), { //add texture coordinates to vector
						x / dim, y / dim,
						x / dim, (y + tileSize) / dim,
						(x + tileSize) / dim, y / dim,
						(x + tileSize) / dim, (y + tileSize) / dim,
						x / dim, (y + tileSize) / dim,
						(x + tileSize) / dim, y / dim });
					vertexData.insert(vertexData.end(), { //add vertex coordinates to vector
						(float)xCoordinate, (float)-1 * yCoordinate,
						(float)xCoordinate, (float)-1 * yCoordinate - 1,
						(float)(xCoordinate + 1), (float)-1 * yCoordinate,
						(float)(xCoordinate + 1), (float)-1 * yCoordinate - 1,
						(float)(xCoordinate), (float)-1 * yCoordinate - 1,
						(float)(xCoordinate + 1), (float)-1 * yCoordinate });
				}
			}

			//bind texture to OpenGL and draw the map
			modelMatrix.Identity();
			program->SetModelMatrix(modelMatrix);
			glUseProgram(program->programID);
			glBindTexture(GL_TEXTURE_2D, texture);
			glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
			glEnableVertexAttribArray(program->positionAttribute);
			glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, textureCoordinates.data());
			glEnableVertexAttribArray(program->texCoordAttribute);

			glDrawArrays(GL_TRIANGLES, 0, vertexData.size() / 2);
		}

		/* checkIfCollision()
			\description - Checks if the inputted coordinates collided with dirt
			\param x     - World X coordinate
			\param y     - World Y coordinate
		*/
		bool checkIfCollision(float x, float y) {
			int gridX = (int)(x); //convert x to the grid version of x
			int gridY = ceil(-y - 1); //convert y to grid version of y
			if (gridX < 0 || gridX >= length || gridY < 0 || gridY >= height) { // if the grid values are out of bounds return false
				return false;
			}
			if ((map[gridY][gridX] != 3)) { //if the coordinate on the map is not sky (3) then return  true (collision)
				return true;
			}
			return false; //else false (no collision)
		}

		/* yAdjustment
			\description - returns how much in the y axis the entity should be moved
			\param x     - World X coordinate
			\param y     - World Y coordinate
		*/
		float yAdjustment(float x, float y) {
			if (!checkIfCollision(x, y)) {
				return 0;
			}
			return ceil(y) - y;
		}
	private:
		Matrix viewMatrix;
		Matrix modelMatrix;
		ShaderProgram* program;
		GLuint texture;
		int height;
		int length;
		int** map;
	};
	class Gun;
	class Bullet;
	//Character Class - Contains character attributes and methods
	class Character {
		friend class GameState;
	public:
		/* Character()
			\description     - Constructor
			\param sentiment - Used to determine if character is playerOne or playerTwo (can be used for team numbers if expanded upon)
			\param pos       - initial position of the character
			\param texture   - texture to be used to draw the character
			\param program   - Shader Program used to draw the character
			\param hp        - Total amount of hp that the character has
		*/
		Character(int sentiment, float* pos, GLuint texture, ShaderProgram& program, float hp=500) : sentiment(sentiment), texture(texture), program(&program) {
			//more initialization			
			health = hp;
			gun = nullptr;

			for (int i = 0; i < 3; i++) {
				collisionFlags[i] = false;
			}
			for (int i = 0; i < 3; i++) {
				position[i] = pos[i];
			}
			if (sentiment == 1) { //if it is playerOne, face right
				animation[0] = 3;
				animation[1] = 0;
			}
			else { //otherwise face left
				animation[0] = 1;
				animation[1] = 0;
			}

			vertices[0] = 0;
			vertices[1] = 1;
			vertices[2] = 0;
			vertices[3] = 0;
			vertices[4] = 1;
			vertices[5] = 1;
			vertices[6] = 1;
			vertices[7] = 1;
			vertices[8] = 0;
			vertices[9] = 0;
			vertices[10] = 1;
			vertices[11] = 0;
		}
		
		/* move()
			\description   - moves character based upon velocity and adjusts velocities if colliding or falling
			\param elapsed - seconds that have passed
		*/
		void move(float elapsed) {
			if ((collisionFlags[0] == false) || (collisionFlags[0] && velocity[1] > 0)) { //if we are in the air or are on the ground and jumping
				position[1] += velocity[1] * elapsed; //change our vertical position
			}
			if ((!collisionFlags[1] && velocity[0] < 0) || (!collisionFlags[2] && velocity[0] > 0)) { //if we are not collided (xDirection) in the direction of our movement
				//If we are at risk of going out of bounds fix our position so that we are stuck on the edge
				if (position[0] < 0 && velocity[0] < 0) {
					position[0] = 0;
				}
				else if (position[0] > BOARD_LENGTH - 1 && velocity[0] > 0) {
					position[0] = BOARD_LENGTH - 1;
				}
				//If we are in the clear to move, then we move and adjust our distance traveled
				else {
					position[0] += velocity[0] * elapsed;
					distanceTraveled += velocity[0] * elapsed;
				}
			}
			if (!collisionFlags[0]) { //if we are falling, increase our falling speed (acceleration)
				velocity[1] -= 3.5f * elapsed;
			}
		}
		/* draw()
			\description - Draw character on screen and adjust animation
		*/
		void draw() {
			glBindTexture(GL_TEXTURE_2D, texture); //bind texture to openGL
			glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertices);
			glEnableVertexAttribArray(program->positionAttribute);
			vector<float> textureCoordinates; //textureCoordinates
			float dim = 192.0f; //dimensions of texture
			float tileSize = 48; //size of each texture
			float x = tileSize * animation[0]; //x coordinate of texture (animation[0] is if left facing or right facing)
			float y;
			if (velocity[0] == 0) { //if we are standing still then we go to a specific animation
				y = tileSize * 2;
			}
			else if (fabs(distanceTraveled) >= 0.5f) { //if our distance traveled is >= 0.5f
				animation[1] = (animation[1] == 1 ? 3 : 1); //switch switch running animation we're using to draw
				distanceTraveled = 0.0f; //reset the distance we traveled
				y = animation[1] * tileSize; //set our y value
			}
			else {
				y = animation[1] * tileSize; //otherwise set our y value to the animation
			}
			textureCoordinates.insert(textureCoordinates.end(), { //insert texture coordinates
				x / dim, y / dim,
				x / dim, (y + tileSize) / dim,
				(x + tileSize) / dim, (y) / dim,
				(x + tileSize) / dim, (y) / dim,
				x / dim, (y + tileSize) / dim,
				(x + tileSize) / dim, (y + tileSize) / dim
				});

			//draw triangles
			glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, textureCoordinates.data());
			glEnableVertexAttribArray(program->texCoordAttribute);
			modelMatrix.Identity();
			modelMatrix.Translate(position[0], position[1], position[2]);
			program->SetModelMatrix(modelMatrix);
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}
		/* shiftPosition
			\description      - adjust the position of the character
			\param horizontal - horizontal displacement
			\param vertical   - vertical displacement
		*/
		void shiftPosition(float horizontal, float vertical) {
			position[0] += horizontal;
			position[1] += vertical;
		}
		/* attach()
			\description  - assign gun pointer to the newGun
			\param newGun - gun to be attached to character
		*/
		void attach(Gun& newGun) {
			gun = &newGun;
		}

		/* setVelocity()
			\description - Assigns velocity to specified direction
			\param x     - new x velocity (-100.0f if unchanged)
			\param y     - new y velocity (-100.0f if unchanged)
			\param z     - new z velocity (-100.0f if unchanged)		
		*/
		void setVelocity(float x = -100.0f, float y = -100.0f, float z = -100.0f) { //velocity is never -100 sooo we can use that as a default constructor
			if (x != -100.0f) {
				bool xPositive = (x > 0);
				bool vPositive = (velocity[0] > 0);
				if (xPositive != vPositive) { //reset distance traveled for animation when we swap directions
					distanceTraveled = 0;
				}
				if (x != velocity[0]) {
					animation[1] = 1;
				}
			}
			velocity[0] = (x == -100.0f ? velocity[0] : x);
			velocity[1] = (y == -100.0f ? velocity[1] : y);
			velocity[2] = (z == -100.0f ? velocity[2] : z);
			if (velocity[0] > 0) {
				animation[0] = 3;
			}
			else if (velocity[0] < 0) {
				animation[0] = 1;
			}
		}

		/* gotHit()
			\description  - Takes away hp from character when hit
			\param damage - Amount of hp that should be removed
		*/
		void gotHit(float damage) {
			health -= damage;
			if (health <= 0.0f) {
				health = 0.0f;
			}
		}

		/* isDead()
			\description - Returns if the character has zero hp
		*/
		bool isDead() {
			return (health == 0);
		}

		/* setFlag
			\description - Sets specific collision flags to true or false
			\param down  - 0 if not collided, 1 if collided (-1 if unchanged) with ground
			\param left  - 0 if not collided, 1 if collided (-1 if unchanged) to the left
			\param right - 0 if not collided, 1 if collided (-1 if unchanged) to the right
		*/
		void setFlag(int down = -1, int left = -1, int right = -1) {
			if (down != -1) {
				collisionFlags[0] = (down == 1);
			}
			if (left != -1) {
				collisionFlags[1] = (left == 1);
			}
			if (right != -1) {
				collisionFlags[2] = (right == 1);
			}
		}
	private:
		bool collisionFlags[3]; //down, left, right
		float health;
		GLuint texture;
		Gun* gun;
		ShaderProgram* program;
		int sentiment;
		Matrix modelMatrix;
		float position[3];
		float velocity[3];
		float vertices[12];
		float distanceTraveled;
		float animation[2];
	};

	//Gun Class - Contains gun attributes and methods and shoots bullets
	class Gun {
		friend class Character;
	public:
		/* Gun()
			\description  - Constructor
			\param player - character whose gun this object belongs to
			\param texture - texture for all of the guns
			\param program - Shader Program used to draw the gun
			\param bulletTexture - texture used to draw the bullet
			\param soundMap      - maps string to firing sound
		*/
		Gun(Character& player, GLuint texture, ShaderProgram& program, GLuint bulletTexture, map<string, Mix_Chunk*>& soundMap) : texture(texture), program(&program), bulletTexture(bulletTexture) {
			//More Initializations			
			master = &player;
			sentiment = master->sentiment;

			for (pair<string, Mix_Chunk*> gts : soundMap) {
				GunToSounds.insert(pair<string, Mix_Chunk*>(gts.first, gts.second)); //Copy the sounds into a local copy
			}

			//initialize the vertex data
			vertices[0] = 0;
			vertices[1] = 1;
			vertices[2] = 0;
			vertices[3] = 0;
			vertices[4] = 1;
			vertices[5] = 1;
			vertices[6] = 1;
			vertices[7] = 1;
			vertices[8] = 0;
			vertices[9] = 0;
			vertices[10] = 1;
			vertices[11] = 0;

			//set gun 0 to be the first gun that is brought up in the game
			gunNumber = 0;

			position[0] = master->position[0];
			position[1] = master->position[1];
			position[2] = master->position[2];

			//map each gun number and if its reversed to a texture x,y coordinate 
			GunToTexture.insert(pair<pair<int, bool>, pair<int, int>>(pair<int, bool>(0, false),  pair<int, int>(0, 0)));
			GunToTexture.insert(pair<pair<int, bool>, pair<int, int>>(pair<int, bool>(0, true),   pair<int, int>(7, 0)));
			GunToTexture.insert(pair<pair<int, bool>, pair<int, int>>(pair<int, bool>(1, false),  pair<int, int>(1, 0)));
			GunToTexture.insert(pair<pair<int, bool>, pair<int, int>>(pair<int, bool>(1, true),   pair<int, int>(6, 0)));
			GunToTexture.insert(pair<pair<int, bool>, pair<int, int>>(pair<int, bool>(2, false),  pair<int, int>(2, 0)));
			GunToTexture.insert(pair<pair<int, bool>, pair<int, int>>(pair<int, bool>(2, true),   pair<int, int>(5, 0)));
			GunToTexture.insert(pair<pair<int, bool>, pair<int, int>>(pair<int, bool>(3, false),  pair<int, int>(3, 0)));
			GunToTexture.insert(pair<pair<int, bool>, pair<int, int>>(pair<int, bool>(3, true),   pair<int, int>(4, 0)));
			GunToTexture.insert(pair<pair<int, bool>, pair<int, int>>(pair<int, bool>(4, false),  pair<int, int>(0, 1)));
			GunToTexture.insert(pair<pair<int, bool>, pair<int, int>>(pair<int, bool>(4, true),   pair<int, int>(7, 1)));
			GunToTexture.insert(pair<pair<int, bool>, pair<int, int>>(pair<int, bool>(5, false),  pair<int, int>(1, 1)));
			GunToTexture.insert(pair<pair<int, bool>, pair<int, int>>(pair<int, bool>(5, true),   pair<int, int>(6, 1)));
			GunToTexture.insert(pair<pair<int, bool>, pair<int, int>>(pair<int, bool>(6, false),  pair<int, int>(2, 1)));
			GunToTexture.insert(pair<pair<int, bool>, pair<int, int>>(pair<int, bool>(6, true),   pair<int, int>(5, 1)));
			GunToTexture.insert(pair<pair<int, bool>, pair<int, int>>(pair<int, bool>(7, false),  pair<int, int>(3, 1)));
			GunToTexture.insert(pair<pair<int, bool>, pair<int, int>>(pair<int, bool>(7, true),   pair<int, int>(4, 1)));
			GunToTexture.insert(pair<pair<int, bool>, pair<int, int>>(pair<int, bool>(8, false),  pair<int, int>(0, 2)));
			GunToTexture.insert(pair<pair<int, bool>, pair<int, int>>(pair<int, bool>(8, true),   pair<int, int>(7, 2)));
			GunToTexture.insert(pair<pair<int, bool>, pair<int, int>>(pair<int, bool>(9, false),  pair<int, int>(1, 2)));
			GunToTexture.insert(pair<pair<int, bool>, pair<int, int>>(pair<int, bool>(9, true),   pair<int, int>(6, 2)));
			GunToTexture.insert(pair<pair<int, bool>, pair<int, int>>(pair<int, bool>(10, false), pair<int, int>(2, 2)));
			GunToTexture.insert(pair<pair<int, bool>, pair<int, int>>(pair<int, bool>(10, true),  pair<int, int>(5, 2)));
			GunToTexture.insert(pair<pair<int, bool>, pair<int, int>>(pair<int, bool>(11, false), pair<int, int>(3, 2)));
			GunToTexture.insert(pair<pair<int, bool>, pair<int, int>>(pair<int, bool>(11, true),  pair<int, int>(4, 2)));
			GunToTexture.insert(pair<pair<int, bool>, pair<int, int>>(pair<int, bool>(12, false), pair<int, int>(0, 3)));
			GunToTexture.insert(pair<pair<int, bool>, pair<int, int>>(pair<int, bool>(12, true),  pair<int, int>(7, 3)));
			GunToTexture.insert(pair<pair<int, bool>, pair<int, int>>(pair<int, bool>(13, false), pair<int, int>(1, 3)));
			GunToTexture.insert(pair<pair<int, bool>, pair<int, int>>(pair<int, bool>(13, true),  pair<int, int>(6, 3)));
			GunToTexture.insert(pair<pair<int, bool>, pair<int, int>>(pair<int, bool>(14, false), pair<int, int>(2, 3)));
			GunToTexture.insert(pair<pair<int, bool>, pair<int, int>>(pair<int, bool>(14, true),  pair<int, int>(5, 3)));

			//map each gun number to a rate of fire, damage, magazine size, max range and bullet speed
			GunToStats.insert(pair<int, tuple<float, float, int, float, float>>(0,  tuple<float, float, int, float, float>(1.5f, 10.0f, 6, 5, 3)));
			GunToStats.insert(pair<int, tuple<float, float, int, float, float>>(1,  tuple<float, float, int, float, float>(2.0f, 30.0f, 12, 4, 4)));
			GunToStats.insert(pair<int, tuple<float, float, int, float, float>>(2,  tuple<float, float, int, float, float>(1.0f, 45.0f, 4, 2, 3)));
			GunToStats.insert(pair<int, tuple<float, float, int, float, float>>(3,  tuple<float, float, int, float, float>(7.0f, 30.0f, 14, 4, 5)));
			GunToStats.insert(pair<int, tuple<float, float, int, float, float>>(4,  tuple<float, float, int, float, float>(15.0f, 20.0f, 25, 15, 7)));
			GunToStats.insert(pair<int, tuple<float, float, int, float, float>>(5,  tuple<float, float, int, float, float>(40.0f, 7.0f, 35, 9, 10)));
			GunToStats.insert(pair<int, tuple<float, float, int, float, float>>(6,  tuple<float, float, int, float, float>(90.0f, 18.0f, 45, 20, 8)));
			GunToStats.insert(pair<int, tuple<float, float, int, float, float>>(7,  tuple<float, float, int, float, float>(25.0f, 6.0f, 25, 7, 10)));
			GunToStats.insert(pair<int, tuple<float, float, int, float, float>>(8,  tuple<float, float, int, float, float>(1.0f, 60.0f, 1, 30, 18)));
			GunToStats.insert(pair<int, tuple<float, float, int, float, float>>(9,  tuple<float, float, int, float, float>(1.0f, 35.0f, 2, 5, 18)));
			GunToStats.insert(pair<int, tuple<float, float, int, float, float>>(10, tuple<float, float, int, float, float>(30.0f, 7.0f, 30, 3, 16)));
			GunToStats.insert(pair<int, tuple<float, float, int, float, float>>(11, tuple<float, float, int, float, float>(35.0f, 3.0f, 35, 2, 8)));
			GunToStats.insert(pair<int, tuple<float, float, int, float, float>>(12, tuple<float, float, int, float, float>(50.0f, 1.0f, 50, 3, 9)));
			GunToStats.insert(pair<int, tuple<float, float, int, float, float>>(13, tuple<float, float, int, float, float>(1.0f, 60.0f, 1, 25, 18)));
			GunToStats.insert(pair<int, tuple<float, float, int, float, float>>(14, tuple<float, float, int, float, float>(3.0f, 35.0f, 12, 10, 8)));

		}

		/* draw()
			\description - Draws the gun onto the screen		
		*/
		void draw() {
			glBindTexture(GL_TEXTURE_2D, texture);
			glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertices);
			glEnableVertexAttribArray(program->positionAttribute);
			pair<int, int> textVal = GunToTexture[pair<int, bool>(gunNumber, master->animation[0] != 3)]; //gets texture coordinates
			//size of the file in x & y
			float xDim = 1280.0f; 
			float yDim = 640.0f;
			//size of each sprite
			float tileSize = 160.0f;

			float x = textVal.first * tileSize;
			float y = textVal.second * tileSize;
			vector<float> textureCoordinates; 
			textureCoordinates.insert(textureCoordinates.end(), { //insert texture coordinates
				x / xDim, y / yDim,
				x / xDim, (y + tileSize) / yDim,
				(x + tileSize) / xDim, (y) / yDim,
				(x + tileSize) / xDim, (y) / yDim,
				x / xDim, (y + tileSize) / yDim,
				(x + tileSize) / xDim, (y + tileSize) / yDim
				});
			//draw the object
			glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, textureCoordinates.data());
			glEnableVertexAttribArray(program->texCoordAttribute);
			modelMatrix.Identity();
			modelMatrix.Translate(position[0], position[1], position[2]);
			modelMatrix.Scale(0.5, 0.5, 1); //scale the object so that it doesn't look weird when the person is holding it
			program->SetModelMatrix(modelMatrix);
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		/* Reposition
			\description - Adjusts the gun's position so that it matches the master's (player's)
		*/
		void Reposition() {
			position[0] = master->position[0] + (master->animation[0] == 3 ? 0.45f : 0.0f);
			position[1] = master->position[1];
		}

		/* ShiftGun()
			\description - adjusts the gunNumber so that a new gun is used to shoot
			\param num   - number that the gunNumber should change by
		*/
		void ShiftGun(int num) {
			gunNumber += (num);
			if (gunNumber < 0) {
				gunNumber += GunToStats.size();
			}
			else if (gunNumber >= GunToStats.size()) {
				gunNumber = gunNumber % GunToStats.size();
			}
			magazineLeft = get<2>(GunToStats[gunNumber]); //adjust size of magazine
			lastShotTime = 0.00f; //reset the gun's bullet fire
		}

		/* Shoot()
			\description - attempts to fire a bullet and returns a pointer to a bullet if it was fired or nullpointer if the gun could not fire
		*/
		Bullet* Shoot() {
			//Set the type of gun
			string gunType = "";
			if (gunNumber == 1 || gunNumber == 3 || gunNumber == 9) {
				gunType = "shotgun";
			}
			else if (gunNumber == 8 || gunNumber == 13) {
				gunType = "sniper";
			}
			else {
				gunType = "rifle";
			}
			float time = SDL_GetTicks()/1000.0f; //get the current time
			if (reloading && time - reloadingStartTime < 1.5f) { //If the gun is still reloading
				if (gunType == "shotgun") { //if its a shotgun play the shotgun reloading sound
					Mix_PlayChannel(-1, GunToSounds["shotgun_r"], 0);
				}
				return nullptr; //return nullpointer since we cant fire a bullet
			}
			tuple<float, float, int, float, float> gunStats = GunToStats[gunNumber]; //get the stats for the gun
			if (reloading) { //if we were reloading, we are not anymore so we reset the magazine and set our reloading flag to false
				reloading = false;
				magazineLeft = get<2>(gunStats);
			}
			if (time - lastShotTime > 1.0f / get<0>(gunStats)) { //if the time betwen our last shot and now is more than the inverse of our fire rate (we can fire another bullet)
				lastShotTime = time; //set the current time to when we fired a bullet
				magazineLeft--; //take away a bullet
				if (magazineLeft == 0) { // if the magazine is now empty start to reload
					reloading = true;
					reloadingStartTime = time;
				}
				if (get<0>(gunStats) > 10.0f) { //if our fire rate is large
					Mix_HaltChannel(sentiment); //stop sounds on our designated channel (our sentiment)
				}
				Mix_PlayChannel(sentiment, GunToSounds[gunType], 0); //play the corresponding gun sound
				return new Bullet(sentiment, get<1>(gunStats), position, (master->animation[0] == 3 ? 1.0f : -1.0f)*get<4>(gunStats), bulletTexture, *program, get<3>(gunStats)); //return a bullet
			}
			return nullptr;
		}
	private:
		bool reloading; //flag for if gun is reloading
		float reloadingStartTime; //time that gun started to reload
		float lastShotTime; //time that the last shot was fired
		float magazineLeft; //Amount of bullets left in magazine
		float position[3]; //position of the gun
		float vertices[12]; //vertex coordinates
		GLuint texture; //texture of guns
		GLuint bulletTexture; //bullet textures
		ShaderProgram* program; //shaderProgram
		int sentiment; // sentiment of the gun
		Character* master; //owner of the gun
		Matrix modelMatrix; 
		int gunNumber; //current number of the gun
		bool rightFacing; //if the gun is facing right or left
		map<pair<int, bool>, pair<int, int>> GunToTexture; //gun number and if reversed
		map<int, tuple<float, float, int, float, float>> GunToStats; //gun number to bullets/second & damage & magazineSize & distance & bulletSpeed
		map<string, Mix_Chunk*> GunToSounds;

	};

	//Bullet Class - Contains bullet attributes and methods
	class Bullet {
		friend class GameState;
	public:
		/* Bullet()
			\description       - Constructor
			\param sentiment   - sentiment of the bullet (no friendly fire)
			\param damage      - amount of damage that the bullet does if it hits a target
			\param vel         - horizontal velocity of the bullet
			\param texture     - texture of the bullet
			\param program     - ShaderProgram used to draw the bullet
			\param maxDistance - maximum distance that a bullet can travel before it dies off
		*/
		Bullet(int sentiment, float damage, float* pos, float vel, GLuint texture, ShaderProgram& program, float maxDistance) : texture(texture), program(&program), sentiment(sentiment), damage(damage), maxDistance(maxDistance) {
			for (int i = 0; i < 3; i++) {
				position[i] = pos[i];
			}
			position[0] += 0.1;
			position[1] += 0.2;
			distanceTraveled = 0;

			vertices[0] = 0;
			vertices[1] = 1;
			vertices[2] = 0;
			vertices[3] = 0;
			vertices[4] = 1;
			vertices[5] = 1;
			vertices[6] = 1;
			vertices[7] = 1;
			vertices[8] = 0;
			vertices[9] = 0;
			vertices[10] = 1;
			vertices[11] = 0;

			velocity = vel;
		}

		/* move()
			\description   - Moves the bullet a distance based upon its velocity and time elapsed. Updates the distance traveled
			\param elapsed - Time that has elapsed since the last time the bullet moved
		*/
		void move(float elapsed) {
			position[0] += velocity * elapsed;
			distanceTraveled += velocity * elapsed;
		}
		
		/* deadBullet()
			\description - Returns whether the bullet has traveled the full distance that it was meant to
			\note - the distance traveled can be manually adjusted if there is a collision by the bullet with an object
		*/
		bool deadBullet() {
			return (fabs(distanceTraveled) >= maxDistance);
		}

		/* draw()
			\description draws bullet onto the screen
		*/
		void draw() {
			//bind texture to OpenGL
			glBindTexture(GL_TEXTURE_2D, texture);
			glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertices);
			glEnableVertexAttribArray(program->positionAttribute);
			float textureCoordinates[] = { 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f }; //texture coordinates
			glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, textureCoordinates);
			glEnableVertexAttribArray(program->texCoordAttribute);
			modelMatrix.Identity();
			modelMatrix.Translate(position[0], position[1], position[2]); 
			float ticks = SDL_GetTicks() / 50.0f;
			float scale = .2*fabs(sinf(ticks)) + 0.2; //scale the image based upon the time that has elapsed to form the "pulsing" effect of a heart
			modelMatrix.Scale(scale, scale, 1);
			program->SetModelMatrix(modelMatrix);
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}
	private:
		Matrix modelMatrix;
		GLuint texture;
		ShaderProgram* program;

		int sentiment;
		float position[3];
		float vertices[12];
		float distanceTraveled;
		float maxDistance;
		float velocity;
		float damage;
	};
	
	/*
	 *
	 * Overhead Variables
	 *
	 */

	ShaderProgram program; //ShaderProgram used to draw the Game

	bool letGo[2]; //Checks to see if Player1 or Player2 (index 0 and 1 respectively) have let go of the key to shift guns

	//Matrices used for drawing and displaying the game on the screen
	Matrix projectionMatrix;
	Matrix modelMatrix;
	Matrix viewMatrix;

	//Maps between a descriptor string and a value (described in variable name)
	map<string, GLuint> textureMap;
	map<string, Mix_Chunk*> overallSoundMap;
	map<string, Mix_Chunk*> gunSoundMap;

	//Music for the MENU_MODE and GAME_MODE
	Mix_Music* menuMusic;
	Mix_Music* gameMusic;

	//Animation Variables
	int frames; //Number of frames that has elapsed from the beginning of the game.  Used to sync MENU_MODE animation with menuMusic since there is normally a latency.
	float drawTime; //Time that has elapsed during the current animation
	bool clickable; //Variable to allow user to click on screen (when animation finishes)
	float lastTicks; //Total number of seconds that has elapsed since start of program

	//Variables for State Progression
	float gameOverTimer; //Number of seconds that has elapsed since the game ended
	int currentState; //Keeps track of the current state in the game to update/draw accordingly
	int nextState; //Stores the next state that the game will be in upon the next Update-Draw cycle.
	bool newGame; //Variable to check if game has just started to make sure we initialize variables

    /*
	 *
	 * In-Game Variables
	 *
	 */

	Map* board; //Contains the board that will be used for the game
	Character* playerOne; //Contains Player1's actions and attributes
	Character* playerTwo; //Contains Player2's actions and attributes
	Gun* gunOne; //Gun attached to Player1
	Gun* gunTwo; //Gun attached to Player2
	vector<Bullet*> bullets; //Container for all bullets that are fired (and not destroyed) by either player

	/* Collision()
		\description - Detects collisions between game entities and adjusts their attributes accordingly
	*/
	void Collision() {
		//Note positions for entities are the bottom left corner so we must adjust these values to determine if the entity itself is colliding
		
		//Terrain-playerOne Collision

		//check if playerOne collided with the ground. if so...
		if (board->checkIfCollision(playerOne->position[0] + 0.5, playerOne->position[1])) { // + 0.5 to get center of character
			playerOne->shiftPosition(0, board->yAdjustment(playerOne->position[0], playerOne->position[1])); //adjust
			playerOne->setFlag(1, -1, -1); //set collision flag as true
			playerOne->setVelocity(-100.0f, 0, -100.0f); //set velocity as 0
		}
		else { //otherwise
			playerOne->setFlag(0, -1, -1); //set collision flag as false
		}
		//check if playerOne collided with the right side. if so...
		if (board->checkIfCollision(playerOne->position[0] + .85f, playerOne->position[1] + 0.5f)) {  // +0.85 to get to right side of the character +0.5 to get to center of height
			playerOne->shiftPosition(-0.01f, 0); //adjust position
			playerOne->setFlag(-1, -1, 1); //set flag
			playerOne->setVelocity(0, -100.0f, -100.0f); //set velocity to 0
		}
		else { //otherwise
			playerOne->setFlag(-1, -1, 0); //set collision flag as false
		}
		//check if playerOne collided to the left. if so...
		if (board->checkIfCollision(playerOne->position[0], playerOne->position[1] + 0.5f)) { //+0.5 to get to center of character's height
			playerOne->shiftPosition(0.01f, 0); //adjust position
			playerOne->setFlag(-1, 1, -1); //set flag
			playerOne->setVelocity(0, -100.0f, -100.0f); //set velocity
		}
		else { //otherwise
			playerOne->setFlag(-1, 0, -1); // set flag to 
		}

		//Terrain-playerTwo collision

		//Check if playerTwo collided with the ground. if so...
		if (board->checkIfCollision(playerTwo->position[0] + 0.5, playerTwo->position[1])) { //+0.5 to get to center of character
			playerTwo->shiftPosition(0, board->yAdjustment(playerTwo->position[0], playerTwo->position[1])); //adjust position
			playerTwo->setFlag(1, -1, -1); //set flag
			playerTwo->setVelocity(-100.0f, 0, -100.0f); //set velocity to 0
		}
		else { //otherwise
			playerTwo->setFlag(0, -1, -1); //set flag to false
		}

		//Check if playerTwo collided to the right side. if so...
		if (board->checkIfCollision(playerTwo->position[0] + .85f, playerTwo->position[1] + 0.5f)) { //+0.85 to get to right side of character. +0.5 to get to center of character's height
			playerTwo->shiftPosition(-0.01f, 0); //adjust position
			playerTwo->setFlag(-1, -1, 1); //set flag
			playerTwo->setVelocity(0, -100.0f, -100.0f); //set velocity to 0
		}
		else { //otherwise
			playerTwo->setFlag(-1, -1, 0); //set flag as false
		}
		//Check if playerTwo collided on the left side. if so...
		if (board->checkIfCollision(playerTwo->position[0], playerTwo->position[1] + 0.5f)) { //+0.5 to get to center of height of character.
			playerTwo->shiftPosition(0.01f, 0); //adjust position
			playerTwo->setFlag(-1, 1, -1); //set flag
			playerTwo->setVelocity(0, -100.0f, -100.0f); //set velocity to 0
		}
		else { //otherwise
			playerTwo->setFlag(-1, 0, -1); //set flag as false
		}

		//Bullet Collisions
		for (Bullet* bullet : bullets) {
			if (board->checkIfCollision(bullet->position[0], bullet->position[1])) { //Check if bullet hit a wall
				bullet->distanceTraveled = bullet->maxDistance; //set bullet's distance to max so it dies
			}
			else if (fabs(bullet->position[0] - playerOne->position[0]) < 0.3f && fabs(bullet->position[1] - (playerOne->position[1] +0.5)) < 0.5 && bullet->sentiment == playerTwo->sentiment) {
				//if bullet hits playerOne and its playerTwo's bullet
				bullet->distanceTraveled = bullet->maxDistance; //set the bullet's distance to max so it dies
				playerOne->gotHit(bullet->damage); //decrease playerOne's hp
			}
			else if (fabs(bullet->position[0] - playerTwo->position[0]) < 0.3f && fabs(bullet->position[1] - (playerTwo->position[1] + 0.5)) < 0.5 && bullet->sentiment == playerOne->sentiment) {
				//if bullet hits playerTwo and its playerOne's bullet
				bullet->distanceTraveled = bullet->maxDistance; //set the bullet's distance to max so it dies
				playerTwo->gotHit(bullet->damage); //decrease playerTwo's hp
			}
		}
	}

	/* LoadTexture
		\description    - Takes in a filepath and loads the file into OpenGL for use in drawing
		\param filePath - file path that is used to load image 
	*/
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
};

int main(int argc, char *argv[])
{
	SDL_Window* displayWindow;
	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("Friendship Spheres!", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_HEIGHT, WINDOW_WIDTH, SDL_WINDOW_OPENGL);
#ifdef FULLSCREEN_MODE
	SDL_SetWindowFullscreen(displayWindow, SDL_WINDOW_FULLSCREEN);
#endif
	SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
	SDL_GL_MakeCurrent(displayWindow, context);
#ifdef _WINDOWS
	glewInit();
#endif
	glViewport(0, 0, WINDOW_HEIGHT, WINDOW_WIDTH);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	ShaderProgram program;
	program.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
	GameState game;
	SDL_Event event;
	bool done = false;
	while (!done) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
		}
		glClear(GL_COLOR_BUFFER_BIT);
		game.Update(event, done);
		game.Draw();
		glDisableVertexAttribArray(program.positionAttribute);

		SDL_GL_SwapWindow(displayWindow);
	}

	SDL_Quit();

	return 0;
}
