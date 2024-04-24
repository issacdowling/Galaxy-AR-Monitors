#include <fstream>
#include <opencv4/opencv2/videoio.hpp>
#include <stdio.h>
#include "raylib.h"
#include "rlgl.h"
#include "rcamera.h"
#include <pthread.h>
#include <unistd.h>

// Extra thanks to:
// danimartin82 for their test2_camera.cpp example (https://github.com/danimartin82/opencv_raylib/tree/master), teaching me how to turn OpenCV frames into textures
// Raylib DrawCubeTexture example for helping me make the display (https://www.raylib.com/examples/models/loader.html?name=models_draw_cube_texture)

std::string imufilepath = "/dev/shm/galaxy/glass_imu.csv";
std::string fovfilepath = "/dev/shm/galaxy/vfov";

cv::Mat frame;
cv::VideoCapture cap;

bool thread_running = true;

pthread_mutex_t videomutex;

void* videoCapThread(void *id){
		cap.open("shmsrc socket-path=/dev/shm/galaxy/centremonitor ! video/x-raw, format=RGBA, width=1920, height=1080, framerate=120/1 ! appsink");

    while(thread_running) {
				if (cap.grab()) {
					pthread_mutex_lock(&videomutex);
					cap.retrieve(frame);
					pthread_mutex_unlock(&videomutex);			
					
				}
    }
		cap.release();
		return NULL;

}

void DrawPlaneTexture(Texture2D texture, Vector3 position, float width, float height, float length, Color color); // Draw cube textured

int main(int argc, char** argv)
{
	pthread_t ptid;
	pthread_mutex_init(&videomutex,NULL);  
  pthread_create(&ptid, NULL, videoCapThread, NULL);

	// Just keep temporarily until better solution for waiting for capture happens
	usleep(100000);

	int windowWidth = 1920;
	int windowHeight = 1080;

	InitWindow(windowWidth, windowHeight, "Galaxy");

	SetTargetFPS(120);

	Camera camera = { 0 };
	camera.position = (Vector3){ 0.0f, 0.0f, 0.0f };
	camera.target = (Vector3){ 0.0f, 0.0f, 4.0f };
	camera.up = (Vector3){ 0.0f, 0.01f, 0.0f };
	camera.fovy = 45.0f;
	camera.projection = CAMERA_PERSPECTIVE;

	// I am not sure why it's necessary to load this initially for an image, but setting the properties of a texture
	// directly, then updating as will be done later doesn't work.
	Image initial_display_texture;
	pthread_mutex_lock(&videomutex);
	initial_display_texture.width = frame.cols;
	initial_display_texture.height = frame.rows;
	initial_display_texture.format = 7;
	initial_display_texture.mipmaps= 1 ;
	initial_display_texture.data= (void*)(frame.data);
	pthread_mutex_unlock(&videomutex);

	Texture2D texture = LoadTextureFromImage(initial_display_texture);

	float roll;
	float pitch;
	float yaw;
	
	// Makes screen look MUCH MUCH MUCH better off-axis
	SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);

	std::string imustring;

	while (!WindowShouldClose()) 
	{

		UpdateTexture(texture, (void*)(frame.data));

		// Read and apply IMU file/values at the beginning of each frame
		std::ifstream imufile(imufilepath);
		std::getline(imufile, imustring);

		std::string rollString = imustring.substr(0, imustring.find(","));
		float roll = strtof(rollString.c_str(), NULL);
		imustring.erase(0, imustring.find(",") + 1);
		
		std::string pitchString = imustring.substr(0, imustring.find(","));
		float pitch = strtof(pitchString.c_str(), NULL);
		imustring.erase(0, imustring.find(",") + 1);

		std::string yawString = imustring.substr(0, imustring.find(","));
		float yaw = strtof(yawString.c_str(), NULL);

		// Apply roll pitch and yaw turned into radians. These ADD the rotation to the camera, rather
		// than APPLYING them, meaning we must reverse these rotations at the end of the loop
		CameraRoll(&camera, roll * DEG2RAD);
		CameraPitch(&camera, (pitch * DEG2RAD), false, false, false);
		CameraYaw(&camera, -yaw * DEG2RAD, false);

		BeginDrawing();

			BeginMode3D(camera);

				ClearBackground(BLACK);
				
				DrawPlaneTexture(texture, (Vector3){0.0f, 0.0f, 4.0f}, -5.33333f, -3.0f, 0.0f, WHITE);

			EndMode3D();
		EndDrawing();

		// Removing previously applied rotation
		CameraYaw(&camera, yaw * DEG2RAD, false);
		CameraPitch(&camera, -pitch * DEG2RAD, false, false, false);
		CameraRoll(&camera, -roll * DEG2RAD);


		// Reload the video capture thread
		if(IsKeyDown(KEY_Q)) {
			printf("Exiitng Video Capture Thread...");
			thread_running = false;
		}

		// Reload the video capture thread
		if(IsKeyPressed(KEY_E)) {
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
void DrawPlaneTexture(Texture2D texture, Vector3 position, float width, float height, float length, Color color)
{
    float x = position.x;
    float y = position.y;
    float z = position.z;

    // Set desired texture to be enabled while drawing following vertex data
    rlSetTexture(texture.id);

        rlBegin(RL_QUADS);
            rlColor4ub(color.r, color.g, color.b, color.a);

            // Only face
            rlNormal3f(0.0f, 0.0f, 0.0f);
            rlTexCoord2f(0.0f, 0.0f); rlVertex3f(x - width/2, y - height/2, z - length/2);  // Bottom Right Of The Texture and Quad
            rlTexCoord2f(0.0f, 1.0f); rlVertex3f(x - width/2, y + height/2, z - length/2);  // Top Right Of The Texture and Quad
            rlTexCoord2f(1.0f, 1.0f); rlVertex3f(x + width/2, y + height/2, z - length/2);  // Top Left Of The Texture and Quad
            rlTexCoord2f(1.0f, 0.0f); rlVertex3f(x + width/2, y - height/2, z - length/2);  // Bottom Left Of The Texture and Quad

        rlEnd();

    rlSetTexture(0);
}