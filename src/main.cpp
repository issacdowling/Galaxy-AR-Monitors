#include <cstdio>
#include <fstream>
#include <stdio.h>
#include <sys/poll.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/signalfd.h>

#define PixelFormat RaylibPixelFormat
#include "raylib.h"
#undef PixelFormat
#include "rlgl.h"
#include "rcamera.h"

#include "include/libscreencapture-wayland/PortalModule/xdg-desktop-portal.hpp"
#include "include/libscreencapture-wayland/common.hpp"
#include "include/libscreencapture-wayland/PipeWireModule/PipeWireStream.hpp"

// Extra thanks to:
// danimartin82 for their test2_camera.cpp example (https://github.com/danimartin82/opencv_raylib/tree/master), teaching me how to turn OpenCV frames into textures, though I have replaced OpenCV now
// Raylib DrawCubeTexture example for helping me make the display (https://www.raylib.com/examples/models/loader.html?name=models_draw_cube_texture)

std::string imufilepath = "/dev/shm/galaxy/glass_imu.csv";
std::string fovfilepath = "/dev/shm/galaxy/vfov";

Texture2D texture;

// From the libscreencapture-wayland example, apparently just necessary
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;
bool thread_running = true;
void* frame;
void* videoCapThread(void *id){
	std::optional<portal::SharedScreen> shareInfo = portal::requestPipeWireShare(CURSOR_MODE_EMBED);

	// TODO: Either work on DMA buf, or figure out that it's not worth it
	auto pwStream = pw::PipeWireStream(shareInfo.value(), false);

	bool shouldStop = false;
	while (!shouldStop) {
		struct pollfd fds[2];
		fds[0] = {pwStream.getEventPollFd(), POLLIN, 0};
		int res = poll(fds, 2, -1);
		if (!(fds[0].revents & POLLIN))
			continue;
		auto ev = pwStream.nextEvent();
		if (ev) {
			// call lambda function appropriate for the type of *ev
			std::visit(overloaded{
					[&] (pw::event::Connected& e) {

					},
					[&] (pw::event::Disconnected&) {
						shouldStop = true;
					},
					[&] (pw::event::MemoryFrameReceived& e) {
						printf("Frame received");
						frame = (void*) (e.frame->memory);
						// shouldStop = true;
					},
					[&] (pw::event::DmaBufFrameReceived& e) {
					}
			}, *ev);
		}
	}

	return NULL;
}

void DrawPlaneTexture(Texture2D texture, Vector3 position, float width, float height, float length); // Draw cube textured

int main(int argc, char** argv)
{
	//// Window setup
	int windowWidth = 1920;
	int windowHeight = 1080;

	InitWindow(windowWidth, windowHeight, "Galaxy");

	// TODO, try using VSYNC instead for supposed performance improvements
	SetTargetFPS(120);

	Camera camera = { 0 };
	camera.position = (Vector3){ 0.0f, 0.0f, 0.0f };
	camera.target = (Vector3){ 0.0f, 0.0f, 4.0f };
	camera.up = (Vector3){ 0.0f, 0.01f, 0.0f };
	camera.fovy = 45.0f;
	camera.projection = CAMERA_PERSPECTIVE;


	//// Screencap setup (MUST!! COME AFTER THE InitWindow, or texure loading will segfault)
	screencapture_wayland_init(&argc, &argv);

	pthread_t ptid;
  pthread_create(&ptid, NULL, videoCapThread, NULL);

	// Just keep temporarily until better solution for waiting for capture happens
	usleep(1500000);

	// I am not sure why it's necessary to load this initially for an image, but setting the properties of a texture
	// directly, then updating as will be done later doesn't work.
	Image initial_display_texture;
	initial_display_texture.width = 1920;
	initial_display_texture.height = 1080;
	initial_display_texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
	initial_display_texture.mipmaps= 1 ;
	initial_display_texture.data = (void*) (frame);

	texture = LoadTextureFromImage(initial_display_texture);

	// Makes screen look MUCH MUCH MUCH better off-axis
	SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);


	//// Init some variables used later

	std::string imustring;
	std::string rollString;
	std::string pitchString;
	std::string yawString;

	float roll;
	float pitch;
	float yaw;

	while (!WindowShouldClose()) 
	{

		UpdateTexture(texture, (void*)(frame));

		// Read and apply IMU file/values at the beginning of each frame
		std::ifstream imufile(imufilepath);
		std::getline(imufile, imustring);

		rollString = imustring.substr(0, imustring.find(","));
		float roll = strtof(rollString.c_str(), NULL);
		imustring.erase(0, imustring.find(",") + 1);
		
		pitchString = imustring.substr(0, imustring.find(","));
		float pitch = strtof(pitchString.c_str(), NULL);
		imustring.erase(0, imustring.find(",") + 1);

		yawString = imustring.substr(0, imustring.find(","));
		float yaw = strtof(yawString.c_str(), NULL);

		// Apply roll pitch and yaw turned into radians. These ADD the rotation to the camera, rather
		// than APPLYING them, meaning we must reverse these rotations at the end of the loop
		CameraRoll(&camera, roll * DEG2RAD);
		CameraPitch(&camera, (pitch * DEG2RAD), false, false, false);
		CameraYaw(&camera, -yaw * DEG2RAD, false);

		BeginDrawing();

			BeginMode3D(camera);

				ClearBackground(BLACK);
				
				DrawPlaneTexture(texture, (Vector3){0.0f, 0.0f, 4.0f}, -5.33333f, -3.0f, 0.0f);

			EndMode3D();
		EndDrawing();

		// Removing previously applied rotation
		CameraYaw(&camera, yaw * DEG2RAD, false);
		CameraPitch(&camera, -pitch * DEG2RAD, false, false, false);
		CameraRoll(&camera, -roll * DEG2RAD);


		// Reload the video capture thread
		if(IsKeyDown(KEY_Q)) {
			printf("Exiting Video Capture Thread...");
			thread_running = false;
			usleep(1000000);
			printf("Starting Video Capture Thread...");
			thread_running = true;
		  pthread_create(&ptid, NULL, videoCapThread, NULL);			
		}

		// Reload FOV value
		if(IsKeyDown(KEY_C)) {
			std::string vfovstring;
			std::ifstream vfovfile(fovfilepath);
			std::getline(vfovfile, vfovstring);
			float vfov = strtof(vfovstring.c_str(), NULL);
			printf("Vertical FOV set to %f", vfov);
			camera.fovy = vfov;
		}

		printf("FPS: %i\n",GetFPS());
	}

    UnloadTexture(texture);
		CloseWindow();

    return 0;
		exit(0);
}

// Originally was DrawCubeTexture from https://www.raylib.com/examples/models/loader.html?name=models_draw_cube_texture but will be DrawPlaneTexture
void DrawPlaneTexture(Texture2D texture, Vector3 position, float width, float height, float length) {
    float x = position.x;
    float y = position.y;
    float z = position.z;

    // Set desired texture to be enabled while drawing following vertex data
    rlSetTexture(texture.id);

			rlBegin(RL_QUADS);

				// White background before texture applied
				rlColor4ub(255,255,255,255);

				// Only face
				rlNormal3f(0.0f, 0.0f, 0.0f);
				rlTexCoord2f(0.0f, 0.0f); rlVertex3f(x - width/2, y - height/2, z - length/2);  // Bottom Right Of The Texture and Quad
				rlTexCoord2f(0.0f, 1.0f); rlVertex3f(x - width/2, y + height/2, z - length/2);  // Top Right Of The Texture and Quad
				rlTexCoord2f(1.0f, 1.0f); rlVertex3f(x + width/2, y + height/2, z - length/2);  // Top Left Of The Texture and Quad
				rlTexCoord2f(1.0f, 0.0f); rlVertex3f(x + width/2, y - height/2, z - length/2);  // Bottom Left Of The Texture and Quad

			rlEnd();

    rlSetTexture(0);
}