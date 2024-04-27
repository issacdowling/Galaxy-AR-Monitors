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

#include "opencv2/imgproc.hpp"

#include "include/libscreencapture-wayland/PortalModule/xdg-desktop-portal.hpp"
#include "include/libscreencapture-wayland/common.hpp"
#include "include/libscreencapture-wayland/PipeWireModule/PipeWireStream.hpp"

// Extra thanks to:
// danimartin82 for their test2_camera.cpp example (https://github.com/danimartin82/opencv_raylib/tree/master), teaching me how to turn OpenCV frames into textures, though I have replaced OpenCV now
// Raylib DrawCubeTexture example for helping me make the display (https://www.raylib.com/examples/models/loader.html?name=models_draw_cube_texture)

std::string imufilepath = "/dev/shm/galaxy/glass_imu.csv";
std::string fovfilepath = "/dev/shm/galaxy/vfov";

Texture2D texture;

cv::Mat screencapMat;
int screencapHeight;
int screencapWidth;

bool cap_thread_ready = false;

// From the libscreencapture-wayland example, apparently just necessary
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

bool thread_running = true;
void* videoCapThread(void *id){
	thread_running = true;
	std::optional<portal::SharedScreen> shareInfo = portal::requestPipeWireShare(CURSOR_MODE_EMBED);

	// TODO: Either work on DMA buf, or figure out that it's not worth it
	auto pwStream = pw::PipeWireStream(shareInfo.value(), false);

	while (thread_running) {
		auto ev = pwStream.nextEvent();
		if (ev) {
			// call lambda function appropriate for the type of *ev
			std::visit(overloaded{
					[&] (pw::event::Connected& e) {
						printf("Connected to Pipewire capture\n");
					},
					[&] (pw::event::Disconnected&) {
						printf("Disconnected from Pipewire capture\n");
						cap_thread_ready = false;
						thread_running = false;
					},
					[&] (pw::event::MemoryFrameReceived& e) {
						// The Pipewire input is BGRX (which we will treat as BGRA), so we convert it to RGB (as we do not need this useless Alpha channel)
						// This is done by passing a Mat which specifies the height / width (using that of the current frame), type, and pixel data of the frame, and
						// setting the output to a previously created empty Mat.
						// The awkward part of this solution for me was the type. I was getting corrupted outputs / segfaults,
						// and it seems to have been a combination of not remembering to add an Alpha channel at the start of the conversion type,
						// and - the main reason - not understanding the Mat types. CV_8U is not enough, as it specifies 8 bit per channel, but
						// assumes 3 channels, and I need 4 to take BGRA in. CV_8UC4 therefore works.
						screencapHeight = e.frame->height;
						screencapWidth = e.frame->width;
						cv::cvtColor(cv::Mat(screencapHeight, screencapWidth, CV_8UC4, (void*) (e.frame->memory)), screencapMat, cv::COLOR_RGBA2BGR);

						cap_thread_ready = true;

					},
					[&] (pw::event::DmaBufFrameReceived& e) {
					}
			}, *ev);
		}
	}
	cap_thread_ready = false;
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


	//// Screencap setup (!!MUST!! COME AFTER THE InitWindow, or texure loading will segfault)
	screencapture_wayland_init(&argc, &argv);

	pthread_t ptid;
  pthread_create(&ptid, NULL, videoCapThread, NULL);

	// Wait until the capture thread has at least received one frame before continuing
	while (!cap_thread_ready) usleep(200000);

	// I am not sure why it's necessary to load this initially for an image, but setting the properties of a texture
	// directly, then updating as will be done later doesn't work.
	Image initial_display_texture;
	initial_display_texture.width = screencapWidth;
	initial_display_texture.height = screencapHeight;
	initial_display_texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8;
	initial_display_texture.mipmaps= 1 ;
	initial_display_texture.data = (void*) (screencapMat.data);

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

		UpdateTexture(texture, (void*)(screencapMat.data));

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
			printf("Starting Video Capture Thread...");
		  pthread_create(&ptid, NULL, videoCapThread, NULL);
			while (!cap_thread_ready) usleep(200000);

			// Re-init the texture with the potentially new resolution
			Image initial_display_texture;
			initial_display_texture.width = screencapWidth;
			initial_display_texture.height = screencapHeight;
			initial_display_texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8;
			initial_display_texture.mipmaps= 1 ;
			initial_display_texture.data = (void*) (screencapMat.data);

			texture = LoadTextureFromImage(initial_display_texture);			
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