#include "ImguiLayer.h"
using namespace std;

void ImguiLayer::ImguiInial(GLFWwindow* window){
    /* Imgui context */
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    io = &ImGui::GetIO();
	io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    /* Set color */
    ImGui::StyleColorsDark();

    /* Set platform/Render Buildings */ 
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");
}

void ImguiLayer::ImguiDraw(size_t &tri_num){
    /* Start imgui */
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    /* Draw imgui */ 
    ImGui::Begin("Tools");
    /*Draw button */ 
    ImGui::Indent();

    /* Multi-Reso control module */ 
    ImGui::BulletText("Multi-Reolustion Rendering"); ImGui::SameLine();
    ImGui::Checkbox("", &isMultiReso);
    
    ImGui::Checkbox("LOD Adaptive ", &isAdaptiveLOD);
    ImGui::DragFloat("kappa", &kappa, 0.1, 1, 20, "%.1f");

    ImGui::Text("\n");
    ImGui::Text("Display Option");    
    ImGui::Checkbox("show Bounding Box", &isBBXVis);
    ImGui::Checkbox("show wireframe mode", &isWireframe);
    ImGui::Checkbox("LOD Colorized", &isLODColor);
    ImGui::Checkbox("Cube Colorized", &isCubeColor);

    ImGui::Text("\n");
    ImGui::Text("Color control");
    ImGui::Text("Background color");

    ImGui::ColorEdit3("Color Editor", (float*)&color);
    
    ImGui::Text("\n");
    ImGui::Text("Control");
    ImGui::Checkbox("VSync", &VSync);
    ImGui::Checkbox("Frustum Culling", &isFrustumCulling);
    ImGui::Checkbox("Smooth Shading", &isSoomthShading);
    
    ImGui::Text("\n");
    ImGui::Text("Render Information:");
    ImGui::Text("Average %.3f ms/frame (%.1f FPS)", 
		   1000.0f / io->Framerate, io->Framerate);
    ImGui::Text("Number of Triangles: %ld /frame (%.4f M / frame)", tri_num, float(tri_num)/1000000.0);
    
    ImGui::Text("rendered cells: %d", renderCubeCount);

    ImGui::Text("\n");
    ImGui::Text("Original Model Information:");
    ImGui::Text("vertices: %ld (%.4f M)", inputVertexCount, float(inputVertexCount)/1000000.0f);
    ImGui::Text("faces: %ld (%.4f M)", inputTriCount, float(inputTriCount)/1000000.0f);

    ImGui::Text("\n");
    ImGui::Text("Multi-Resolution Model Information:");
    ImGui::Text("vertices: %ld (%.4f M) ", hlodVertexCount, float(hlodVertexCount)/1000000.0f);
    ImGui::Text("faces: %ld (%.4f M)", hlodTriCount, float(hlodTriCount)/1000000.0f);

    if(ImGui::Button("Screen Shoot")){
        isSavePic = true;
    }

    /* Draw the real-time frametime plot */ 
    static bool animate = true;
    ImGui::Checkbox("Animate", &animate);
    static float values[90] = { 0.0f };
    static int values_offset = 0;
    static float refresh_time = 0.0;
    
    if (!animate || refresh_time == 0.0){
        refresh_time = ImGui::GetTime();
    }
        
    float value_sum = 0.0;
    while (refresh_time < ImGui::GetTime()){
        static float phase = 0.0f;
        values[values_offset] = 1000.0f / io->Framerate;
        values_offset = (values_offset + 1) % IM_ARRAYSIZE(values);
        phase += 0.10f * values_offset;
        refresh_time += 1.0f * 0.0166667f;
        
    }

    for(int i = 0; i < values_offset ; ++ i){
        value_sum += values[values_offset];
    }

    string plot_str = std::to_string( value_sum/values_offset );
    ImGui::PlotLines("Frame Times", values, IM_ARRAYSIZE(values), values_offset, plot_str.c_str(), 0.0f, 36.00f, ImVec2(0,40));

    /* Write the viewer index into data */
    ImGui::Checkbox("Export Data", &isRecordData);
    if(isRecordData){
        out.open("data_val.txt", ios::out|ios::app);
        out << 1000.0f / io->Framerate<<" " << tri_num << endl;
        out.close();
    }
    
    ImGui::Text("GPU VENDOR: %s", gpuVendorStr);
    ImGui::Text("GPU MODEL: %s", gpuModelStr);

    ImGui::End();
}

void ImguiLayer::ImguiRender(){
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImguiLayer::ImguiClean(){
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

ImguiLayer::ImguiLayer(){}

ImguiLayer::~ImguiLayer(){}