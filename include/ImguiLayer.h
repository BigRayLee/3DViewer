#pragma once
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include <string>
#include <fstream>
#include <chrono>
#include <thread>

struct ImguiLayer
{
    ImVec4 color = {1.0f, 1.0f, 1.0f, 1.0f};    /* Clean color*/
    ImGuiIO *io;
    size_t inputVertexCount = 0;                /* Original model info*/ 
    size_t inputTriCount = 0;
    size_t hlodVertexCount = 0;                 /* HLOD info */
    size_t hlodTriCount = 0;
    
    unsigned char* gpuVendorStr;                /* GPU vendor and model */
    unsigned char* gpuModelStr;
    float kappa = 4.0f;                         /* adaptive HLOD parameter value */
    float sigma = 0.1f;
    float gpuUsage;                             /* GPU usage*/
    float overdrawRatio = 0.0f;
    int renderCubeCount = 0;                    /* Number of rendered cells */
    std::ofstream out;                          /*out file stream */
    
    bool isMultiReso = true;        /* Rendering HLOD model*/
    bool isNormalVis = false;       /* Draw normals */
    bool isBBXVis = false;          /* Draw boudingbox */
    bool isWireframe = false;       /* Draw edge */
    bool isLODColor = false;        /* Draw color for different LOD */
    bool isCubeColor = false;       /* Draw color for different cubes */
    bool isAdaptiveLOD = true;      /* Rendering HLOD with vertex interpolation */
    bool isSavePic = false;         /* Save the current rendering result */
    bool VSync = false;             /* Vsync */
    bool isFrustumCulling = true;   /* Frustum Culling */
    bool isSoomthShading = false;   /* Smooth shadering*/
    bool isOverdrawVis = false;     /* Depth buffer visualization */
    bool isRecordData = false;      /* Output data*/
    
    ImguiLayer(/* args */);
    ~ImguiLayer();

    void ImguiInial(GLFWwindow* window);    /* Imgui setup function */
    void ImguiDraw(size_t &tri_num);        /* Imgui Draw */
    void ImguiRender();                     /* Imgui Render */
    void ImguiClean();                      /* Imgui cleanup */
};
