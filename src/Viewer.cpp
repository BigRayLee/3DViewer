#include <string.h>
#include <stdio.h>
#include <math.h>

#include "Viewer.h"
#include "Camera.h"
#include "Trackball.h"
#include "math/transform.h"

static constexpr float SC_DOUBLE_CLICK_TIME = 0.5f;      
static constexpr float SC_ZOOM_SENSITIVITY = 0.1f;  

Viewer::Viewer(){
	camera = new Camera;
	imgui = new ImguiLayer;
}

static void
resize_window_callback(GLFWwindow* window, int width, int height){
	Viewer* viewer = (Viewer *)glfwGetWindowUserPointer(window);
	
	viewer->width = width;
	viewer->height = height;
	viewer->camera->set_aspect((float)width / height);

	glViewport(0, 0, width, height);
}

static void
mouse_button_callback(GLFWwindow* window, int button, int action, int mods){
	Viewer* viewer = (Viewer *)glfwGetWindowUserPointer(window);
	
    /*add want mouse capture*/
	if (viewer->imgui->io->WantCaptureMouse){
		return;
	}

	if (action == GLFW_PRESS){
		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);
		viewer->MousePressed(xpos, ypos, button, mods);
	} 
	else if (action == GLFW_RELEASE){
		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);
		viewer->MouseReleased(button, mods);
	}

}

static void
cursor_position_callback(GLFWwindow* window, double xpos, double ypos){
	Viewer* viewer = (Viewer *)glfwGetWindowUserPointer(window);
	
	if (viewer->imgui->io->WantCaptureMouse){
		return;
	}

	viewer->MouseMove(xpos, ypos);
}

static void
scroll_callback(GLFWwindow* window, double xoffset, double yoffset){
	Viewer* viewer = (Viewer *)glfwGetWindowUserPointer(window);
	
	if (viewer->imgui->io->WantCaptureMouse){
		return;
	}

	viewer->MouseScroll(xoffset, yoffset);
}

bool Viewer::Init(int w, int h){
	width = w;
	height = h;

	camera->set_aspect((float)width / height);
	camera->set_fov(45.f);
	return (true);
}

void Viewer::MousePressed(float px, float py, int button, int mod){
	static double last_pressed = -1.0;
	double now = glfwGetTime();
	bool double_click = (now - last_pressed) < SC_DOUBLE_CLICK_TIME;
	last_pressed = now;
	
	isMousePressed = true;
	buttons = button;
	mods = mod;
	lastClickX = px;
	lastClickY = py;
	lastCameraPos  = camera->get_position();
	lastCameraRot  = camera->get_rotation();
	lastTrackballV = screen_trackball(px, py, width, height);
		
	if (!double_click) return;

	float pixels[1];
	float tx = px;
	float ty = height - py;
	glReadPixels(tx, ty, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, pixels);
	/* Don't track clicks outisde of model */
	if (approx_equal(pixels[0], 1.f)) {
		return;
	}
	
	target = camera->world_coord_at(px / width, py / height, pixels[0]);
}

void Viewer::MouseReleased(int button, int mods)
{
	(void)button;
	(void) mods;
	isMousePressed = false;
}

void Viewer::MouseMove(float px, float py)
{
	if (!isMousePressed) return;
	
	if (mods & GLFW_MOD_SHIFT){
		/* Translation in x and y */	
		float dist = norm(target - camera->get_position());
		float mult = dist / width;
		Vec3 trans;
		trans.x = (lastClickX - px) * mult;
		trans.y = (py - lastClickY) * mult;
		trans.z = 0.f;
		camera->set_position(lastCameraPos);
		camera->translate(trans, Camera::View);
	}
	else if (mods & GLFW_MOD_CONTROL){
		/* Zoom (translation in target direction) */
		float mu = SC_ZOOM_SENSITIVITY * (px - lastClickX) / 100;
		Vec3 new_pos = target + exp(mu) * (lastCameraPos - target);
		camera->set_position(new_pos);
	}
	else /* no modifier */{
		Vec3 trackball_v = screen_trackball(px, py, width, height);
		Quat rot = great_circle_rotation(lastTrackballV, trackball_v);
		/* rot quat is in view frame, back to world frame */
		rot.xyz = rotate(rot.xyz, lastCameraRot);
		camera->set_position(lastCameraPos);
		camera->set_rotation(lastCameraRot);
		/* TODO adapt SC_SENSITIVITY to camera fov in free mode*/
		rot = pow(rot, SC_SENSITIVITY);
		if (navMode == NavMode::Orbit){
			camera->orbit(-rot, target);
		}
		else if (navMode == NavMode::Free){
			camera->rotate(-rot);
		}
		/**
		 * Great_circle_rotation is singular when from and to are
		 * close to antipodal. To avoid that situation, we checkout
		 * mouse move when the from and to first belong to two opposite
		 * hemispheres.
		 */
		if (dot(trackball_v, lastTrackballV) < 0)
		{
			lastClickX = px;
			lastClickY = py;
			lastCameraPos  = camera->get_position();
			lastCameraRot  = camera->get_rotation();
			lastTrackballV = trackball_v;
		}
	}
}

void Viewer::MouseScroll(float xoffset, float yoffset)
{
	(void)xoffset;
	Vec3 old_pos = camera->get_position();
	Vec3 new_pos = target + exp(-SC_ZOOM_SENSITIVITY * yoffset) * (old_pos - target);
	camera->set_position(new_pos);
}

void Viewer::KeyPressed(int key, int action)
{
	(void)key;
	(void)action;
}

void Viewer::InitGlfwFunctions(){
    /* Set-up GLFW */
	if (!glfwInit()) return;
	
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_DOUBLEBUFFER, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_SAMPLES, 4);
	
	window = glfwCreateWindow(SC_SCR_WIDTH, SC_SCR_HEIGHT, "3DViewer", NULL, NULL);
	
	if (!window) return;
	
	glfwSetWindowUserPointer(window, this);
	glfwMakeContextCurrent(window);
	
	// glfwSetKeyCallback(window, key_callback);	
	glfwSetFramebufferSizeCallback(window, resize_window_callback);
	glfwSetCursorPosCallback(window, cursor_position_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSwapInterval(1);
	
    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){
        cout << "Fialed to initialize GLAD" << endl;
    }
	
	/* Set-up Viewer3D */
	Init(SC_SCR_WIDTH, SC_SCR_HEIGHT);
}

void Viewer::ProcessInput(GLFWwindow * window){
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS){
		glfwSetWindowShouldClose(window, true);
	}
        
    /* Freeze the current frame*/
    if(glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS){
		isFreezeFrame = true;
	}
        
    /* Unfreeze the current frame*/
    if(glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS){
		isFreezeFrame = false;
	}
      
}