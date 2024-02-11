#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "Camera.h"
#include "ImguiLayer.h"

namespace NavMode {
	enum {
		Orbit,
		Free,
		Walk,
		Fly
	};
};

struct Viewer {
	/* Camera */
	Quat  lastCameraRot;
	Vec3  lastCameraPos;
	Vec3  lastTrackballV;
	Vec3  target = Vec3::Zero;
	static constexpr float SC_SENSITIVITY = 1.0f;
	int   buttons;
	int   mods;
	float lastClickX;
	float lastClickY;

	static constexpr unsigned SC_SCR_WIDTH =  1920; 
    static constexpr unsigned SC_SCR_HEIGHT = 1080; 
    GLFWwindow* window;
	ImguiLayer* imgui;  	               
	Camera* camera;                   
    float deltaTime = 0.0f;              /* timing */
    float lastFrame = 0.0f;
    float kappa = 4.0f;                  /* adaptive LOD parameters */
    float sigma = 0.1f;
    float scale = 1.0f;                  /* model scale */
	int width;                           /* TODO viewport instead */
	int height;
	uint32_t navMode = NavMode::Orbit;  /* Mode */
	bool isFreezeFrame = false;          /* freeze the frame */
	bool isMousePressed = false;

	Viewer();
    bool Init(int w, int h);
	void InitGlfwFunctions();
	void MousePressed(float px, float py, int button, int mod);
	void MouseReleased(int button, int mod);
	void MouseMove(float px, float py);
	void MouseScroll(float xoffset, float yoffset);
	void KeyPressed(int key, int action);
	void ProcessInput(GLFWwindow * window);
};