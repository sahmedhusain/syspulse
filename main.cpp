#include "header.h"
#include <SDL.h>

/*
NOTE : You are free to change the code as you wish, the main objective is to make the
       application work and pass the audit.

       It will be provided the main function with the following functions :

       - `void systemWindow(const char *id, ImVec2 size, ImVec2 position)`
            This function will draw the system window on your screen
       - `void memoryProcessesWindow(const char *id, ImVec2 size, ImVec2 position)`
            This function will draw the memory and processes window on your screen
       - `void networkWindow(const char *id, ImVec2 size, ImVec2 position)`
            This function will draw the network window on your screen
*/

// About Desktop OpenGL function loaders:
//  Modern desktop OpenGL doesn't have a standard portable header file to load OpenGL function pointers.
//  Helper libraries are often used for this purpose! Here we are supporting a few common ones (gl3w, glew, glad).
//  You may use another loader/header of your choice (glext, glLoadGen, etc.), or chose to manually implement your own.
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
#include <GL/gl3w.h> // Initialize with gl3wInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
#include <GL/glew.h> // Initialize with glewInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
#include <glad/glad.h> // Initialize with gladLoadGL()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD2)
#include <glad/gl.h> // Initialize with gladLoadGL(...) or gladLoaderLoadGL()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING2)
#define GLFW_INCLUDE_NONE      // GLFW including OpenGL headers causes ambiguity or multiple definition errors.
#include <glbinding/Binding.h> // Initialize with glbinding::Binding::initialize()
#include <glbinding/gl/gl.h>
using namespace gl;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING3)
#define GLFW_INCLUDE_NONE        // GLFW including OpenGL headers causes ambiguity or multiple definition errors.
#include <glbinding/glbinding.h> // Initialize with glbinding::initialize()
#include <glbinding/gl/gl.h>
using namespace gl;
#else
#include IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#endif

// systemWindow, display information for the system monitorization
void systemWindow(const char *id, ImVec2 size, ImVec2 position)
{
    ImGui::Begin(id);
    ImGui::SetWindowSize(id, size);
    ImGui::SetWindowPos(id, position);

    ImGui::Text("Operating System used : %s", getOsName());
    ImGui::Text("Computer name : %s", getHostName().c_str());
    ImGui::Text("User logged in : %s", getLoggedInUser().c_str());
    TaskCounts tasks = getTaskCounts();
    ImGui::Text("Number of working processes : %d", tasks.total);
    ImGui::Text("CPU : %s", getCPUModel().c_str());

    ImGui::Separator();

    // Shared graph controls and state variables
    static bool animate = true;
    static float graphFPS = 5.0f; // Default 5 updates per second
    static float scaleMax = 100.0f; // Default scale max is 100

    // History buffers for CPU, Thermal, and Fan
    static std::vector<float> cpuHistory(100, 0.0f);
    static std::vector<float> thermalHistory(100, 0.0f);
    static std::vector<float> fanHistory(100, 0.0f);
    static float timeAccumulator = 0.0f;
    static float currentCPUVal = 0.0f;
    static float currentThermalVal = 0.0f;
    static FanStats currentFanStats;

    // Timer logic to sample values based on graphFPS using SDL_GetTicks()
    static Uint32 lastTime = SDL_GetTicks();
    Uint32 currentTime = SDL_GetTicks();
    float deltaTime = (float)(currentTime - lastTime) / 1000.0f;
    lastTime = currentTime;

    if (deltaTime > 1.0f) deltaTime = 1.0f; // cap to avoid spikes
    timeAccumulator += deltaTime;
    float samplePeriod = 1.0f / graphFPS;

    if (timeAccumulator >= samplePeriod)
    {
        currentCPUVal = getCPUUsage();
        currentThermalVal = getTemperature();
        currentFanStats = getFanStats();
        if (animate)
        {
            // Shift history
            for (size_t i = 0; i < (int)cpuHistory.size() - 1; ++i)
            {
                cpuHistory[i] = cpuHistory[i + 1];
                thermalHistory[i] = thermalHistory[i + 1];
                fanHistory[i] = fanHistory[i + 1];
            }
            cpuHistory.back() = currentCPUVal;
            thermalHistory.back() = currentThermalVal;
            fanHistory.back() = (float)currentFanStats.speed;
        }
        timeAccumulator = 0.0f;
    }

    if (ImGui::BeginTabBar("SystemTabs"))
    {
        if (ImGui::BeginTabItem("CPU"))
        {
            ImGui::Checkbox("Animate", &animate);
            ImGui::SliderFloat("FPS", &graphFPS, 1.0f, 60.0f, "%.0f");
            ImGui::SliderFloat("scale max", &scaleMax, 10.0f, 100.0f, "%.0f");
            ImGui::Text("percentage : %.2f %%", currentCPUVal);
            
            char overlayText[32];
            snprintf(overlayText, sizeof(overlayText), "Usage: %.1f%%", currentCPUVal);
            
            // Plot CPU lines
            ImGui::PlotLines("CPU", cpuHistory.data(), (int)cpuHistory.size(), 0, overlayText, 0.0f, scaleMax, ImVec2(ImGui::GetContentRegionAvail().x, 150));
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Fan"))
        {
            ImGui::Checkbox("Animate", &animate);
            ImGui::SliderFloat("FPS", &graphFPS, 1.0f, 60.0f, "%.0f");
            ImGui::SliderFloat("scale max", &scaleMax, 10.0f, 100.0f, "%.0f");
            
            ImGui::Text("status : %s", currentFanStats.status.c_str());
            ImGui::Text("level : %s", currentFanStats.level.c_str());
            ImGui::Text("speed : %d RPM", currentFanStats.speed);
            
            char overlayText[32];
            snprintf(overlayText, sizeof(overlayText), "Speed: %d RPM", currentFanStats.speed);
            
            // Plot Fan lines
            ImGui::PlotLines("speed", fanHistory.data(), (int)fanHistory.size(), 0, overlayText, 0.0f, scaleMax * 50.0f, ImVec2(ImGui::GetContentRegionAvail().x, 150));
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Thermal"))
        {
            ImGui::Checkbox("Animate", &animate);
            ImGui::SliderFloat("FPS", &graphFPS, 1.0f, 60.0f, "%.0f");
            ImGui::SliderFloat("scale max", &scaleMax, 10.0f, 100.0f, "%.0f");
            
            ImGui::Text("temperature : %.2f C", currentThermalVal);
            
            char overlayText[32];
            snprintf(overlayText, sizeof(overlayText), "Temp: %.1f C", currentThermalVal);
            
            // Plot Thermal lines
            ImGui::PlotLines("temperature", thermalHistory.data(), (int)thermalHistory.size(), 0, overlayText, 0.0f, scaleMax, ImVec2(ImGui::GetContentRegionAvail().x, 150));
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

// memoryProcessesWindow, display information for the memory and processes information
void memoryProcessesWindow(const char *id, ImVec2 size, ImVec2 position)
{
    ImGui::Begin(id);
    ImGui::SetWindowSize(id, size);
    ImGui::SetWindowPos(id, position);

    MemoryStats mem = getMemoryStats();
    
    // RAM Progress Bar
    float ramPercent = 0.0f;
    if (mem.ramTotal > 0)
    {
        ramPercent = (float)mem.ramUsed / mem.ramTotal;
    }
    float ramUsedGB = (float)mem.ramUsed / (1024.0f * 1024.0f * 1024.0f);
    float ramTotalGB = (float)mem.ramTotal / (1024.0f * 1024.0f * 1024.0f);
    
    ImGui::Text("Physic Memory (RAM):");
    char ramOverlay[64];
    snprintf(ramOverlay, sizeof(ramOverlay), "%.2f / %.2f GB (%.1f%%)", ramUsedGB, ramTotalGB, ramPercent * 100.0f);
    ImGui::ProgressBar(ramPercent, ImVec2(-1, 0), ramOverlay);

    ImGui::Spacing();

    // SWAP Progress Bar
    float swapPercent = 0.0f;
    if (mem.swapTotal > 0)
    {
        swapPercent = (float)mem.swapUsed / mem.swapTotal;
    }
    float swapUsedGB = (float)mem.swapUsed / (1024.0f * 1024.0f * 1024.0f);
    float swapTotalGB = (float)mem.swapTotal / (1024.0f * 1024.0f * 1024.0f);

    ImGui::Text("Virtual Memory (SWAP):");
    char swapOverlay[64];
    if (mem.swapTotal > 0)
    {
        snprintf(swapOverlay, sizeof(swapOverlay), "%.2f / %.2f GB (%.1f%%)", swapUsedGB, swapTotalGB, swapPercent * 100.0f);
    }
    else
    {
        snprintf(swapOverlay, sizeof(swapOverlay), "No SWAP configured");
    }
    ImGui::ProgressBar(swapPercent, ImVec2(-1, 0), swapOverlay);

    ImGui::Spacing();

    // Disk Progress Bar
    DiskStats disk = getDiskStats();
    float diskPercent = 0.0f;
    if (disk.totalBytes > 0)
    {
        diskPercent = (float)disk.usedBytes / disk.totalBytes;
    }
    float diskUsedGB = (float)disk.usedBytes / (1024.0f * 1024.0f * 1024.0f);
    float diskTotalGB = (float)disk.totalBytes / (1024.0f * 1024.0f * 1024.0f);

    ImGui::Text("Disk Usage (Root /):");
    char diskOverlay[64];
    snprintf(diskOverlay, sizeof(diskOverlay), "%.2f / %.2f GB (%.1f%%)", diskUsedGB, diskTotalGB, diskPercent * 100.0f);
    ImGui::ProgressBar(diskPercent, ImVec2(-1, 0), diskOverlay);

    ImGui::Spacing();

    // Tab Bar for Processes
    if (ImGui::BeginTabBar("MemProcTabs"))
    {
        if (ImGui::BeginTabItem("Processes"))
        {
            ImGui::Text("* Process Table");
            ImGui::Text("filter the process by name:");

            // Text input filter
            static char filterText[128] = "";
            ImGui::InputText("Filter (inc,-dec)", filterText, sizeof(filterText));
            ImGui::SameLine();
            if (ImGui::Button("Clear"))
            {
                filterText[0] = '\0';
            }

            // Name sorting controls
            static int sortOrder = 0; // 0 = None, 1 = Name (Asc), 2 = Name (Desc)
            const char* sortItems[] = { "None", "Name (Asc)", "Name (Desc)" };
            ImGui::PushItemWidth(120.0f);
            ImGui::Combo("Sort Order", &sortOrder, sortItems, IM_ARRAYSIZE(sortItems));
            ImGui::PopItemWidth();

            ImGui::BeginChild("ProcTableChild", ImVec2(0, 0), true);
            if (ImGui::BeginTable("ProcessTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
            {
                ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("NAME", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("STATE", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("CPU", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("MEM", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableHeadersRow();

                vector<Proc> processes = getProcesses(mem.ramTotal);
                static std::set<int> selectedPids;

                // Sort the processes if a sort order is selected
                if (sortOrder == 1) // Ascending by Name
                {
                    std::sort(processes.begin(), processes.end(), [](const Proc &a, const Proc &b) {
                        return a.name < b.name;
                    });
                }
                else if (sortOrder == 2) // Descending by Name
                {
                    std::sort(processes.begin(), processes.end(), [](const Proc &a, const Proc &b) {
                        return a.name > b.name;
                    });
                }

                std::string filterStr = filterText;
                std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(),
                               [](unsigned char c) { return std::tolower(c); });

                for (const auto &p : processes)
                {
                    // Filter match
                    if (!filterStr.empty())
                    {
                        std::string procNameLower = p.name;
                        std::transform(procNameLower.begin(), procNameLower.end(), procNameLower.begin(),
                                       [](unsigned char c) { return std::tolower(c); });
                        if (procNameLower.find(filterStr) == std::string::npos)
                        {
                            continue;
                        }
                    }

                    ImGui::TableNextRow();
                    
                    // PID (Selectable row)
                    ImGui::TableSetColumnIndex(0);
                    char pidStr[32];
                    snprintf(pidStr, sizeof(pidStr), "%d", p.pid);
                    bool isSelected = (selectedPids.count(p.pid) > 0);
                    if (ImGui::Selectable(pidStr, isSelected, ImGuiSelectableFlags_SpanAllColumns))
                    {
                        if (isSelected)
                        {
                            selectedPids.erase(p.pid);
                        }
                        else
                        {
                            selectedPids.insert(p.pid);
                        }
                    }
                    
                    // Name
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%s", p.name.c_str());
                    
                    // State
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%s", p.state.c_str());
                    
                    // CPU
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%.2f", p.cpuUsage);
                    
                    // Memory
                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("%.2f", p.memUsage);
                }
                ImGui::EndTable();
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

// network, display information network information
void networkWindow(const char *id, ImVec2 size, ImVec2 position)
{
    ImGui::Begin(id);
    ImGui::SetWindowSize(id, size);
    ImGui::SetWindowPos(id, position);

    ImGui::Text("ip4 network:");
    Networks nets = getNetworkInterfaces();
    for (const auto &ip : nets.ip4s)
    {
        ImGui::BulletText("%s : %s", ip.name.c_str(), ip.addressBuffer);
    }
    ImGui::Spacing();

    ImGui::Text("* network table");

    map<string, pair<TX, RX>> netStats = getNetworkStats();

    // Tab Bar for RX / TX Tables
    if (ImGui::BeginTabBar("NetProcTabs"))
    {
        if (ImGui::BeginTabItem("Receive(RX)"))
        {
            if (ImGui::BeginTable("RXTable", 9, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("Interface");
                ImGui::TableSetupColumn("Bytes");
                ImGui::TableSetupColumn("Packets");
                ImGui::TableSetupColumn("Errs");
                ImGui::TableSetupColumn("Drop");
                ImGui::TableSetupColumn("Fifo");
                ImGui::TableSetupColumn("Frame");
                ImGui::TableSetupColumn("Compressed");
                ImGui::TableSetupColumn("Multicast");
                ImGui::TableHeadersRow();

                for (const auto &pair : netStats)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%s", pair.first.c_str());

                    const TX &rx = pair.second.first; // Mapped to TX struct
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%lld", rx.bytes);
                    ImGui::TableSetColumnIndex(2); ImGui::Text("%lld", rx.packets);
                    ImGui::TableSetColumnIndex(3); ImGui::Text("%lld", rx.errs);
                    ImGui::TableSetColumnIndex(4); ImGui::Text("%lld", rx.drop);
                    ImGui::TableSetColumnIndex(5); ImGui::Text("%lld", rx.fifo);
                    ImGui::TableSetColumnIndex(6); ImGui::Text("%lld", rx.frame);
                    ImGui::TableSetColumnIndex(7); ImGui::Text("%lld", rx.compressed);
                    ImGui::TableSetColumnIndex(8); ImGui::Text("%lld", rx.multicast);
                }
                ImGui::EndTable();
            }

            ImGui::Spacing();
            ImGui::Text("* Network Visual Usage (Receive/RX)");
            double maxBytes = 2.0 * 1024.0 * 1024.0 * 1024.0; // 2 GB
            for (const auto &pair : netStats)
            {
                const TX &rx = pair.second.first;
                float fraction = (float)((double)rx.bytes / maxBytes);
                if (fraction > 1.0f) fraction = 1.0f;
                if (fraction < 0.0f) fraction = 0.0f;

                ImGui::Text("%s", pair.first.c_str());
                std::string formatted = formatBytes(rx.bytes);
                ImGui::ProgressBar(fraction, ImVec2(ImGui::GetContentRegionAvail().x - 65.0f, 0), formatted.c_str());
                ImGui::SameLine();
                ImGui::Text("2.00 GB");
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Transmit(TX)")) // Let's fix the typo too, it should be "Transmit(TX)"
        {
            if (ImGui::BeginTable("TXTable", 9, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("Interface");
                ImGui::TableSetupColumn("Bytes");
                ImGui::TableSetupColumn("Packets");
                ImGui::TableSetupColumn("Errs");
                ImGui::TableSetupColumn("Drop");
                ImGui::TableSetupColumn("Fifo");
                ImGui::TableSetupColumn("Colls");
                ImGui::TableSetupColumn("Carrier");
                ImGui::TableSetupColumn("Compressed");
                ImGui::TableHeadersRow();

                for (const auto &pair : netStats)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%s", pair.first.c_str());

                    const RX &tx = pair.second.second; // Mapped to RX struct
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%lld", tx.bytes);
                    ImGui::TableSetColumnIndex(2); ImGui::Text("%lld", tx.packets);
                    ImGui::TableSetColumnIndex(3); ImGui::Text("%lld", tx.errs);
                    ImGui::TableSetColumnIndex(4); ImGui::Text("%lld", tx.drop);
                    ImGui::TableSetColumnIndex(5); ImGui::Text("%lld", tx.fifo);
                    ImGui::TableSetColumnIndex(6); ImGui::Text("%lld", tx.colls);
                    ImGui::TableSetColumnIndex(7); ImGui::Text("%lld", tx.carrier);
                    ImGui::TableSetColumnIndex(8); ImGui::Text("%lld", tx.compressed);
                }
                ImGui::EndTable();
            }

            ImGui::Spacing();
            ImGui::Text("* Network Visual Usage (Transmit/TX)");
            double maxBytes = 2.0 * 1024.0 * 1024.0 * 1024.0; // 2 GB
            for (const auto &pair : netStats)
            {
                const RX &tx = pair.second.second;
                float fraction = (float)((double)tx.bytes / maxBytes);
                if (fraction > 1.0f) fraction = 1.0f;
                if (fraction < 0.0f) fraction = 0.0f;

                ImGui::Text("%s", pair.first.c_str());
                std::string formatted = formatBytes(tx.bytes);
                ImGui::ProgressBar(fraction, ImVec2(ImGui::GetContentRegionAvail().x - 65.0f, 0), formatted.c_str());
                ImGui::SameLine();
                ImGui::Text("2.00 GB");
            }

            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

// Main code
int main(int, char **)
{
    // Setup SDL
    // (Some versions of SDL before <2.0.10 appears to have performance/stalling issues on a minority of Windows systems,
    // depending on whether SDL_INIT_GAMECONTROLLER is enabled or disabled.. updating to latest version of SDL is recommended!)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // Decide GL+GLSL versions
#if defined(__APPLE__)
    // GL 3.2 Core + GLSL 150
    const char *glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    const char *glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window *window = SDL_CreateWindow("SysPulse - Desktop System Monitor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Initialize OpenGL loader
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
    bool err = gl3wInit() != 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
    bool err = glewInit() != GLEW_OK;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
    bool err = gladLoadGL() == 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD2)
    bool err = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress) == 0; // glad2 recommend using the windowing library loader instead of the (optionally) bundled one.
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING2)
    bool err = false;
    glbinding::Binding::initialize();
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING3)
    bool err = false;
    glbinding::initialize([](const char *name) { return (glbinding::ProcAddress)SDL_GL_GetProcAddress(name); });
#else
    bool err = false; // If you use IMGUI_IMPL_OPENGL_LOADER_CUSTOM, your loader is likely to requires some form of initialization.
#endif
    if (err)
    {
        fprintf(stderr, "Failed to initialize OpenGL loader!\n");
        return 1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    // render bindings
    ImGuiIO &io = ImGui::GetIO();

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.TabRounding = 6.0f;
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.09f, 0.11f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.28f, 0.30f, 0.36f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.35f, 0.38f, 0.45f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.24f, 0.26f, 0.33f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.32f, 0.35f, 0.44f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.40f, 0.44f, 0.55f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.20f, 0.25f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.28f, 0.35f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.30f, 0.33f, 0.41f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.25f, 0.27f, 0.33f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.32f, 0.35f, 0.42f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.38f, 0.42f, 0.50f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.39f, 0.69f, 1.00f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.39f, 0.69f, 1.00f, 1.00f);

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // background color
    // note : you are free to change the style of the application
    ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();

        {
            ImVec2 mainDisplay = io.DisplaySize;
            memoryProcessesWindow("== Memory and Processes ==",
                                  ImVec2((mainDisplay.x / 2) - 20, (mainDisplay.y / 2) + 30),
                                  ImVec2((mainDisplay.x / 2) + 10, 10));
            // --------------------------------------
            systemWindow("== System ==",
                         ImVec2((mainDisplay.x / 2) - 10, (mainDisplay.y / 2) + 30),
                         ImVec2(10, 10));
            // --------------------------------------
            networkWindow("== Network ==",
                          ImVec2(mainDisplay.x - 20, (mainDisplay.y / 2) - 60),
                          ImVec2(10, (mainDisplay.y / 2) + 50));
        }

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
