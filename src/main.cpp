#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <stdio.h>
#include <sys/poll.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/signalfd.h>
#include <sys/stat.h>

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

std::string galaxyshmpath = "/dev/shm/galaxy/";
std::string imufilepath = "/dev/shm/galaxy/glass_imu";
std::string driverexitpath = "/dev/shm/galaxy/exitGlassesDriver";
std::string fovfilepath;

Texture2D texture;

cv::Mat screencapMat;
int screencapHeight = 0;
int screencapWidth = 0;
bool swapRB = false;

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
		// This pollfd stuff must be kept, as it seems to limit things from using 100% of the CPU
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
						printf("Connected to Pipewire capture, waiting for frame...\n");
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
						if (screencapHeight != e.frame->height || screencapWidth != e.frame->width) {
							screencapHeight = e.frame->height;
							screencapWidth = e.frame->width;
						}

						if (swapRB) cv::cvtColor(cv::Mat(screencapHeight, screencapWidth, CV_8UC4, (void*) (e.frame->memory)), screencapMat, cv::COLOR_RGBA2BGRA);
						else screencapMat = cv::Mat(screencapHeight, screencapWidth, CV_8UC4, (void*) (e.frame->memory));
						
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
	// Create /dev/shm/galaxy/
	mkdir(galaxyshmpath.c_str(), 0755);

	remove(driverexitpath.c_str());

	// Create ~/.config/galaxy/
	std::string homedir(secure_getenv("HOME"));
	std::string configdir(homedir + "/.config/galaxy");
	mkdir(configdir.c_str(), 0755);
	fovfilepath = configdir + "/vfov";

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
	camera.fovy = 22.5f;
	camera.projection = CAMERA_PERSPECTIVE;

	// Get whether the R and B channnels need to be swapped
	std::string rbswapconfigstring = configdir + "/rbswap";
	if (access(rbswapconfigstring.c_str(), F_OK) == 0) {
		swapRB = true;
		printf("SwapRB file found (%s), swapping R and B channels of screencap\n", rbswapconfigstring.c_str());
	} else printf("SwapRGB file (%s) not found, so not swapping the R and B channels.\nIf your image has the R and B channels swapped, create this file so Galaxy can correct it.\n", rbswapconfigstring.c_str());

	//// Screencap setup (!!MUST!! COME AFTER THE InitWindow, or texure loading will segfault)
	screencapture_wayland_init(&argc, &argv);

	pthread_t ptid;
  pthread_create(&ptid, NULL, videoCapThread, NULL);

	// Wait until the capture thread has at least received one frame before continuing
	printf("Started screencap thread, waiting for confirmation...\n");
	while (!cap_thread_ready) usleep(200000);

	// I am not sure why it's necessary to load this initially for an image, but setting the properties of a texture
	// directly, then updating as will be done later doesn't work.
	Image initial_display_texture;
	initial_display_texture.width = screencapWidth;
	initial_display_texture.height = screencapHeight;
	initial_display_texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
	initial_display_texture.mipmaps= 1 ;
	initial_display_texture.data = (void*) (screencapMat.data);
	texture = LoadTextureFromImage(initial_display_texture);

	// Makes screen look MUCH MUCH MUCH better off-axis
	SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);

	// Init the FOV
	if (access(fovfilepath.c_str(), F_OK) == 0) {
		std::string vfovstring;
		std::ifstream vfovfile(fovfilepath);
		std::getline(vfovfile, vfovstring);
		float vfov = strtof(vfovstring.c_str(), NULL);
		printf("FOV File found, vertical FOV set to %f\n", vfov);
		camera.fovy = vfov;
	} else printf("No FOV file found, using default vertical FOV of %f\n", camera.fovy);

	//// Init some variables used later

	std::string imustring;
	std::string rollString;
	std::string pitchString;
	std::string yawString;

	float roll = 0.0;
	float pitch = 0.0;
	float yaw = 0.0;

	float calRoll = 0.0;
	float calPitch = 0.0;
	float calYaw = 0.0;

	printf("==============\n");
	printf("Galaxy AR Monitors has started.\nPress R to reload the video capture setup, useful if you want to switch the captured display / window\nPress F to reload the FOV value at %s, which is useful for testing your adjustments live\nPress C to calibrate where the centre of your vision should be.\nIf your screen just appears to be black after selecting a source, try pressing C to see if you're just looking away from the display.", fovfilepath.c_str());
	printf("==============\n");

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

		// Apply roll pitch and yaw turned into radians, minus the calibration values also as radians. These ADD the rotation to the camera, rather
		// than APPLYING them, meaning we must reverse these rotations at the end of the loop.
		// Yaw (and therefore calYaw) must be made negative
		CameraRoll(&camera, ((roll * DEG2RAD) - (calRoll * DEG2RAD)));
		CameraPitch(&camera, ((pitch * DEG2RAD) - (calPitch * DEG2RAD)), false, false, false);
		CameraYaw(&camera, ((-yaw * DEG2RAD) - (-calYaw * DEG2RAD)), false);

		BeginDrawing();

			BeginMode3D(camera);

				ClearBackground(BLACK);
				
				DrawPlaneTexture(texture, (Vector3){0.0f, 0.0f, 8.0f}, -5.33333f, -3.0f, 0.0f);

			EndMode3D();
		EndDrawing();

		// Removing previously applied rotation
		CameraYaw(&camera, -((-yaw * DEG2RAD) - (-calYaw * DEG2RAD)), false);
		CameraPitch(&camera, -((pitch * DEG2RAD) - (calPitch * DEG2RAD)), false, false, false);
		CameraRoll(&camera, -((roll * DEG2RAD) - (calRoll * DEG2RAD)));


		// Reload the video capture thread
		if(IsKeyPressed(KEY_R)) {
			printf("Exiting Video Capture Thread...\n");
			thread_running = false;
			printf("Started screencap thread, waiting for confirmation...\n");
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
		if(IsKeyPressed(KEY_F)) {
			if (access(fovfilepath.c_str(), F_OK) == 0) {
				std::string vfovstring;
				std::ifstream vfovfile(fovfilepath);
				std::getline(vfovfile, vfovstring);
				float vfov = strtof(vfovstring.c_str(), NULL);
				printf("Vertical FOV reloaded, set to %f\n", vfov);
				camera.fovy = vfov;
			} else printf("Couldn't reload the vertical FOV file, as it doesn't exist\n");
		}

		// Set calibration values
		if(IsKeyPressed(KEY_C)) {
			calRoll = roll;
			calPitch = pitch;
			calYaw = yaw;
			printf("Set calibration values to %f, %f, %f\n", calRoll, calPitch, calYaw);
		}

		// Toggle Fullscreen
		if(IsKeyPressed(KEY_T)) {
			ToggleFullscreen();
			printf("Toggled fullscreen\n");
		}

		// Toggle Borderless
		if(IsKeyPressed(KEY_B)) {
			ToggleBorderlessWindowed();
			printf("Toggled borderless mode\n");
		}

	}

    UnloadTexture(texture);
		CloseWindow();

		// Create the file that tells the glasses driver to close, as otherwise it would keep running
 		FILE *driverCloseFile;
		driverCloseFile = fopen(driverexitpath.c_str(), "w");
		fclose(driverCloseFile); 

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