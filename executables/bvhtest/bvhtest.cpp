#include <spdlog/spdlog.h>
#include <iostream>
#include <raylib.h>

struct State {
	int vwidth = 1920;
	int vheight = 1080;
	int width = 1920;
	int height = 1080;
	bool mouseLocked = false;
};

int main(int argc, char** argv) {
	spdlog::info("Hello, Raylib!");

	State state;
	InitWindow(state.width, state.height, "Hello Raylib");
	SetWindowState(FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
	SetTargetFPS(60);
	spdlog::info("{}", argv[0]);

	Camera3D camera = {};
	camera.position = { 10.0f, 10.0f, 0.0f }; // Camera position
	camera.target = { 0.0f, 0.0f, 0.0f }; // Camera looking at point
	camera.fovy = 45.0f; // Camera field-of-view Y
	camera.up = { 0.0f, 1.0f, 0.0f }; // Camera up vector (rotation towards target)
	camera.projection = CAMERA_PERSPECTIVE; // Camera mode type

	Model dragon = LoadModel("models/dragon.glb"); // Load a model

	while (!WindowShouldClose()) {
		if (IsWindowResized()) {
			state.width = GetScreenWidth();
			state.height = GetScreenHeight();
			spdlog::debug("Window resized to {}x{}", state.vwidth, state.vheight);
		}
		if (IsKeyPressed(KEY_L)) { // Toggle mouse lock
			state.mouseLocked = !state.mouseLocked;
			if (state.mouseLocked) {
				spdlog::debug("Mouse locked");
				DisableCursor();
			}
			else {
				EnableCursor();
				spdlog::debug("Mouse unlocked");
			}
		}
		if(state.mouseLocked)
		UpdateCamera(&camera, CAMERA_FREE); // Update camera
		if (IsKeyPressed(KEY_ESCAPE)) {
			spdlog::debug("Escape key pressed, exiting...");
			break;
		}

		BeginDrawing();
			ClearBackground(RAYWHITE);
			DrawText("Hello, Raylib!", 190, 200, 20, LIGHTGRAY);
			DrawText("Press ESC to exit", 190, 240, 20, LIGHTGRAY);
			BeginMode3D(camera);
			// Draw 3D objects here
				DrawGrid(10, 1.0f); // Draw a grid with 10 divisions and 20 pixels spacing	
				DrawModel(dragon, { 0.0f, 0.0f, 0.0f }, 1.0f, WHITE); // Draw a model
				DrawModelWires(dragon, { 0.0f, 0.0f, 0.0f }, 1.0f, BLACK); // Draw model wires
			EndMode3D();
		EndDrawing();
	}
	CloseWindow(); // Close window and OpenGL context

    return 0;
}