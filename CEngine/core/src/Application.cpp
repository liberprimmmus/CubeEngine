﻿#include "../includes/Application.h"
#include "../render/includes/CVertex.h"
#include "../includes/Window.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "imgui_internal.h"
#include "../includes/CMath.h"
#include "../includes/ImguiThemes.h"
#include "../includes/SceneSerializer.h"
#include "../includes/WindowsUtils.h"
#include "../scripting/includes/ScriptEngine.h"
#include <Commdlg.h>
#include <memory>
#include <stdio.h>
#include <string.h>
#include <chrono>
#include <thread>
#include <math.h>
#include <iostream>


extern bool ImGuiSaveStyle(const char* filename, const ImGuiStyle& style);
extern bool ImGuiLoadStyle(const char* filename, ImGuiStyle& style);


namespace Cube
{
	Application::Application(int width, int height, WindowType type) 
		: 
		m_Window(width, height, type),
		light(m_Window.Gfx()), 
		skybox(std::make_unique<SkyBox>(m_Window.Gfx(), "textures\\skyboxmain.dds"))
	{
		models.push_back(std::make_unique<Model>(m_Window.Gfx(), "models\\cube.obj", id, "Cube"));
		++id;
		m_Window.Gfx().SetTexture(&pCubeIco, L"icons\\cubeico2.png");

		ScriptEngine::Init();

		CUBE_INFO("Application has been set up");
	}

	Application::~Application()
	{
		if (scenePath != "Unnamed Scene")
		{
			SceneSerializer serializer(*this);
			serializer.Serialize(scenePath);
		}
		ScriptEngine::Shutdown();
		pCubeIco->Release();
		models.clear();
		ImGuiSaveStyle("style.style", ImGui::GetStyle());
	}

	void Application::HadleInput(float dt)
	{
		const auto io = ImGui::GetIO();
		if (!io.WantCaptureMouse && !io.WantCaptureKeyboard)
		{
			if (m_Window.kbd.KeyIsPressed('W'))
			{
				cam.Translate({ 0.0f, 0.0f, dt});
			}
			if (m_Window.kbd.KeyIsPressed('S'))
			{
				cam.Translate({ 0.0f, 0.0f, -dt});
			}
			if (m_Window.kbd.KeyIsPressed('A'))
			{
				cam.Translate({ -dt, 0.0f, 0.0f });
			}
			if (m_Window.kbd.KeyIsPressed('D'))
			{
				cam.Translate({ dt, 0.0f, 0.0f });
			}
			if (m_Window.kbd.KeyIsPressed(VK_SPACE))
			{
				cam.Translate({ 0.0f, dt, 0.0f });
			}
			if (m_Window.kbd.KeyIsPressed(VK_SHIFT))
			{
				cam.Translate({ 0.0f, -dt, 0.0f });
			}
			while (const auto d = m_Window.mouse.ReadRawDelta())
			{
				if (m_Window.mouse.RightIsPressed())
				{
					cam.Rotate((float)d->x, (float)d->y);
				}
			}
			while (const auto b = m_Window.mouse.Read())
			{
				if (b->GetType() == Mouse::Event::Type::WheelUp)
				{
					cam.Translate({ 0.0f, 0.0f, dt * 10});
				}
				if (b->GetType() == Mouse::Event::Type::WheelDown)
				{
					cam.Translate({ 0.0f, 0.0f, -dt * 10});
				}
			}
		}
		switch (m_Window.kbd.ReadKey().GetCode())
		{
		case 'S':
		{
			if (m_Window.kbd.KeyIsPressed(VK_CONTROL) && m_Window.kbd.KeyIsPressed(VK_SHIFT))
			{
				saveSceneAs();
			}
			else if (m_Window.kbd.KeyIsPressed(VK_CONTROL))
			{
				if (scenePath != "Unnamed Scene")
				{
					saveScene();
				}
				else
				{
					saveSceneAs();
				}
			}
			break;
		}
		case 'O':
		{
			if (m_Window.kbd.KeyIsPressed(VK_CONTROL))
			{
				openScene();
			}
			break;
		}
		case 'N':
		{
			if (m_Window.kbd.KeyIsPressed(VK_CONTROL))
			{
				newScene();
			}
			break;
		}
		}
	}

	int Application::run()
	{
		while ( true )
		{
			if (const auto ecode = Window::ProcessMessages())
			{
				return ecode->wParam;
			}
			doFrame();
			if (!nofpslimit && !m_Window.Gfx().isVSYCNenabled())
			{
				std::chrono::milliseconds frameTimeMin(maxfps);
				std::this_thread::sleep_for(frameTimeMin - timer.MarkDuration());
			}
		}
	}

	void Application::doFrame()
	{

		auto dt = timer.Mark();
		HadleInput(dt);
		m_Window.Gfx().ClearBuffer(0.07f, 0.07f, 0.07f);
		m_Window.Gfx().SetCamera(cam.GetMatrix());

		ImGuiID did = m_Window.Gfx().ShowDocksape();
		ImGuiDockNode* node = ImGui::DockBuilderGetCentralNode(did);
		m_Window.Gfx().CreateViewport(node->Size.x, node->Size.y, node->Pos.x, node->Pos.y);
		m_Window.Gfx().SetProjection(DirectX::XMMatrixPerspectiveFovLH(to_rad(75.0f), node->Size.x / node->Size.y, 0.01f, 300.0f));

	

		if (skybox)
		{
			skybox->Draw(m_Window.Gfx());
		}

		light.Bind(m_Window.Gfx(), cam.GetMatrix());
	

		for (auto& m : models) 
		{
			m->Draw(m_Window.Gfx());
		}


		light.drawSpheres(m_Window.Gfx());
		


		if (drawGrid)
		{
			m_Window.Gfx().DrawGrid(cam.pos);
		}

		ShowMenuBar(); 
		ShowSceneWindow(); 
		ShowToolBar(); 
		showLightHelp();
		showSettingsWindow();

		m_Window.Gfx().EndFrame();						
	}


	void Application::ShowSceneWindow()
	{
		const auto io = ImGui::GetIO();
		ImGui::SetNextWindowSize(ImGui::GetContentRegionAvail(), ImGuiCond_FirstUseEver);

		ImGui::Begin("Scene");
		if (ImGui::BeginPopupContextWindow())
		{
			if (ImGui::MenuItem("Add object from file..."))
			{
				AddObj();
			}
			if (ImGui::MenuItem("Add Light"))
			{
				light.AddLight(m_Window.Gfx());
			}
			if (models.size() != 0)
			{
				for (int i = 0; i < models.size(); ++i)
				{
					std::string name = "Delete " + models[i]->modelName;
					if (ImGui::MenuItem(name.c_str()))
					{
						models.erase(models.begin() + i);
					}
				}
			}
			for (int i = 0; i < light.getLighstCount(); ++i)
			{
				std::string name = "Delete " + light.getName(i);
				if (ImGui::MenuItem(name.c_str()))
				{
					light.DeleteLight(i);
				}
			}
			if (ImGui::MenuItem("Clear Scene"))
			{
				models.clear();
				light.clearLights();
			}
			ImGui::EndPopup();
		}
		for (int i = 0; i < models.size(); ++i)
		{
			const int selectedId = (pSelectedModel == nullptr) ? -1 : pSelectedModel->GetId();
			bool expanded = ImGui::TreeNodeEx((void*)(intptr_t)models[i]->GetId(), 0, models[i]->modelName.c_str());
			if (ImGui::IsItemClicked() || ImGui::IsItemActivated())
			{
				pSelectedModel = models[i].get();
			}
			if (expanded)
			{
				models[i]->ShowWindow(m_Window.Gfx(), pSelectedModel);
				ImGui::TreePop();
				ImGui::Spacing();
			}
			if (ImGui::IsMouseDoubleClicked(0) && ImGui::IsItemClicked())
			{
				pSelectedModel = nullptr;
			}
		}
		
		light.spawnWnds();

		cam.SpawnControlWindow();

		ImGui::Begin("Proprieties");
		ImGui::End();

		ImGui::End();
	}



	void Application::ShowMenuBar()
	{ 
		if (ImGui::BeginMainMenuBar())
		{
			if (m_Window.type == CUSTOM)
			{
				if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered())
				{
					m_Window.m_titleBarHovered = true;
				}
				else
				{
					m_Window.m_titleBarHovered = false;
				}
				auto tw = ImGui::GetWindowWidth();
				std::string text = "Cube Engine";
				ImGui::SetCursorPosX((tw - ImGui::CalcTextSize(text.c_str()).x) / 2);
				ImGui::Text(text.c_str());

				ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
				ImGui::Text(scenePath.filename().string().c_str());
				ImGui::PopStyleColor();

				std::string close_text = "X";
				ImGui::SetCursorPosX((tw - ImGui::CalcTextSize(close_text.c_str()).x) - 10.0f);
				ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg]);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.7f, 0.1f, 0.25f, 1.0f });
				ImGui::Button(close_text.c_str(), ImVec2{ 20.0f, 20.0f });
				if (ImGui::IsItemHovered())
				{
					m_Window.m_CloseButton = true;
				}
				else
				{
					m_Window.m_CloseButton = false;
				}
				ImGui::PopStyleVar(2);
				ImGui::PopStyleColor(2);

				char max_sym = '=';
				std::string max_text{max_sym};
				ImGui::SetCursorPosX(tw - ImGui::CalcTextSize(max_text.c_str()).x - 32.5f);
				ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg]);
				ImGui::Button(max_text.c_str(), ImVec2{ 20.0f, 20.0f });
				if (ImGui::IsItemHovered())
				{
					m_Window.m_MaxButton = true;
				}
				else
				{
					m_Window.m_MaxButton = false;
				}
				ImGui::PopStyleVar(2);
				ImGui::PopStyleColor();

				std::string min_text = "-";
				ImGui::SetCursorPosX(tw - ImGui::CalcTextSize(min_text.c_str()).x - 57.5f);
				ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg]);
				ImGui::Button(min_text.c_str(), ImVec2{ 20.0f, 20.0f });
				if (ImGui::IsItemHovered())
				{
					m_Window.m_MinButton = true;
				}
				else
				{
					m_Window.m_MinButton = false;
				}
				ImGui::PopStyleVar(2);
				ImGui::PopStyleColor();
			}
			ImGui::SetCursorPosX(10.0f);
			if (ImGui::BeginMenu("Project")) {
				if (ImGui::MenuItem("Create new project", "Ctrl+N"))
				{
					newScene();
				}
				if (ImGui::MenuItem("Open project...", "Ctrl+O"))
				{
					openScene();

				}
				if (ImGui::MenuItem("Save project", "Ctrl+S"))
				{
					if (scenePath != "Unnamed Scene")
					{
						saveScene();
					}
					else
					{
						saveSceneAs();
					}
				}
				if (ImGui::MenuItem("Save project as..", "Ctrl+Shift+S"))
				{
					saveSceneAs();
				}
				if (ImGui::MenuItem("Project Settings"))
				{
					projSettWindowOpen = true;
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Scene")) {
				if (ImGui::MenuItem("Add object from file..."))
				{
					AddObj();
				}
				if (ImGui::MenuItem("Add Light"))
				{
					light.AddLight(m_Window.Gfx());
				}
				if (models.size() != 0)
				{
					for (int i = 0; i < models.size(); ++i)
					{
						std::string name = "Delete " + models[i]->modelName;
						if (ImGui::MenuItem(name.c_str()))
						{
							models.erase(models.begin() + i);
						}
					}
				}
				for (int i = 0; i < light.getLighstCount(); ++i)
				{
					std::string name = "Delete " + light.getName(i);
					if (ImGui::MenuItem(name.c_str()))
					{
						light.DeleteLight(i);
					}
				}
				if (ImGui::MenuItem("Clear Scene"))
				{
					models.clear();
					light.clearLights();
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Themes")) {
				if (ImGui::MenuItem("Default Dark"))
				{
					SetDDTheme();
				}
				if (ImGui::MenuItem("Classic Light"))
				{
					SetLightTheme();
				}
				if (ImGui::MenuItem("Deep Purple"))
				{
					SetDPTheme();
				}
				if (ImGui::MenuItem("Sailor moon"))
				{
					SetSMTheme();
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Help"))
			{
				lHelpWindowOpen = true;
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}
	}

	void Application::showLightHelp()
	{
		if (lHelpWindowOpen)
		{
			ImGuiIO& io = ImGui::GetIO();
			ImGui::SetNextWindowSize(ImVec2{ 400, 700 }, ImGuiCond_Appearing);
			ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
			ImGui::Begin("Help", &lHelpWindowOpen, ImGuiWindowFlags_NoCollapse);

			ImGui::SeparatorText("MOVE/CAMERA HELP ");
			ImGui::TextWrapped("Camera movement:\nW - Forward\nS - Back\nA - Left\nD - Right\nShift - Descend\nSpace - Rise\n\nTo rotate the camera, right - click and move the mouse.\n\nAdding an object from a file :\n1. Right - click on an empty space in the 'Scene' window or left - click on the 'Scene' item in the top menu \n2. Select the item 'Add object from file...'\n3. Select a 3D model file.You can add four types of files : .obj, .gltf, .fbx and .md5mesh.\n4. Click");

			ImGui::SeparatorText("LIGHT HELP: ");
			ImGui::Columns(1);
			ImGui::TextWrapped("Cube Engine uses one type of shading - Phong shading. So here are presented the recommended values for Point light sources, depending on them ranges from objects:");

			ImGui::Columns(4);
			ImGui::BulletText("Range ->");
			ImGui::BulletText("0.7 ->");
			ImGui::BulletText("2 ->");
			ImGui::BulletText("5 ->");
			ImGui::BulletText("10 ->");
			ImGui::BulletText("16 ->");
			ImGui::BulletText("20 ->");
			ImGui::BulletText("32.5 ->");
			ImGui::BulletText("60 ->");
			ImGui::BulletText("325 ->");

			ImGui::NextColumn();
			ImGui::Text("Constant");
			ImGui::Text("1.0");
			ImGui::Text("1.0");
			ImGui::Text("1.0");
			ImGui::Text("1.0");
			ImGui::Text("1.0");
			ImGui::Text("1.0");
			ImGui::Text("1.0");
			ImGui::Text("1.0");
			ImGui::Text("1.0");

			ImGui::NextColumn();
			ImGui::Text("Linear");
			ImGui::Text("0.7");
			ImGui::Text("0.22");
			ImGui::Text("0.09");
			ImGui::Text("0.045");
			ImGui::Text("0.027");
			ImGui::Text("0.022");
			ImGui::Text("0.014");
			ImGui::Text("0.007");
			ImGui::Text("0.0014");

			ImGui::NextColumn();
			ImGui::Text("Quadratic");
			ImGui::Text("1.8");
			ImGui::Text("0.20");
			ImGui::Text("0.032");
			ImGui::Text("0.0075");
			ImGui::Text("0.0028");
			ImGui::Text("0.0019");
			ImGui::Text("0.0007");
			ImGui::Text("0.0002");
			ImGui::Text("0.000007");
			ImGui::Columns(1);
			ImGui::End();
		}
	}

	void Application::showSettingsWindow()
	{
		if(projSettWindowOpen)
		{
			ImGuiIO& io = ImGui::GetIO();
			ImGui::SetNextWindowSize(ImVec2{ 700, 550 }, ImGuiCond_Appearing);
			ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
			ImGui::Begin("Project Settings", &projSettWindowOpen, ImGuiWindowFlags_NoCollapse);


			ImGui::BeginChild("left pane", ImVec2(150, 0), true);

			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);


			static int selected = 0;

			ImGui::Selectable("General Settings", selected == 1, ImGuiSelectableFlags_SelectOnNav);
			if (ImGui::IsItemFocused())
			{
				selected = 1;
			}

			ImGui::Selectable("Graphics Settings", selected == 2, ImGuiSelectableFlags_SelectOnNav);
			if (ImGui::IsItemFocused())
			{
				selected = 2;
			}

			ImGui::PopStyleVar(2);
			ImGui::EndChild();

			ImGui::SameLine(); 
			ImGui::BeginChild("item view", ImVec2(0, -ImGui::GetFrameHeightWithSpacing())); 
			if (selected == 1)
			{
				ImGui::SeparatorText("General");
				ImGui::Text("Skybox: ");
				ImGui::SameLine();
				ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
				if (ImGui::Button("1"))
				{
					skybox.release();
					skybox = std::make_unique<SkyBox>(m_Window.Gfx(), "textures\\skyboxmain.dds");
				}
				ImGui::SameLine();
				if (ImGui::Button("2"))
				{
					skybox.release();
					skybox = std::make_unique<SkyBox>(m_Window.Gfx(), "textures\\skybox2.dds");
				}
				ImGui::SameLine();
				if (ImGui::Button("3"))
				{
					skybox.release();
					skybox = std::make_unique<SkyBox>(m_Window.Gfx(), "textures\\earth.dds");
				}
				if (ImGui::Button("Delete Skybox"))
				{
					skybox.release();
				}
				if (ImGui::Button("Load Skybox from file..."))
				{
					std::filesystem::path filepath = FileDialogs::OpenfileA("DDS files(*.dds)\0*.dds\0\0");
					if (!filepath.empty())
					{
						skybox.release();
						skybox = std::make_unique<SkyBox>(m_Window.Gfx(), filepath.string());
					}
				}
				ImGui::PopStyleVar(2);
			}
			if (selected == 2)
			{
				ImGui::SeparatorText("Graphics");
				ImGui::Checkbox("VSync", &m_Window.Gfx().VSYNCenabled);

				ImGui::BeginDisabled(m_Window.Gfx().isVSYCNenabled());
				ImGui::BulletText("Set Max Framerate");

				ImGui::BeginDisabled(nofpslimit);
				if (ImGui::Button("30"))
					maxfps = std::chrono::milliseconds(27);
				ImGui::SameLine();
				if (ImGui::Button("60"))
					maxfps = std::chrono::milliseconds(14);
				ImGui::SameLine();
				ImGui::EndDisabled();
				ImGui::Checkbox("No limit", &nofpslimit);

				ImGui::EndDisabled();
			}
			ImGui::EndChild(); 

			ImGui::End();
		}
	}

	void Application::ShowToolBar()
	{
		const auto io = ImGui::GetIO();
		ImGui::SetNextWindowSize({ 150, ImGui::GetContentRegionAvail().y }, ImGuiCond_FirstUseEver);
		ImGui::Begin("ToolBar");

		AddCube();
		ImGui::Checkbox("Draw Grid", &drawGrid);

		auto th = ImGui::GetWindowHeight();
		std::string text = "%.1f FPS";
		ImGui::SetCursorPosY(th - ImGui::CalcTextSize(text.c_str()).y - 10.0f); 
		ImGui::Text(text.c_str(), ImGui::GetIO().Framerate); 

		ImGui::End();
	}

	void Application::AddObj()
	{

		std::filesystem::path filepath = FileDialogs::OpenfileA("OBJ files(*.obj)\0*.obj\0GLTF files(*.gltf)\0*.gltf\0FBX files(*.fbx)\0*.fbx\0MD5MESH files(*.md5mesh)\0*.md5mesh\0\0");
		if (!filepath.empty())
		{
			models.push_back(std::make_unique<Model>(m_Window.Gfx(), filepath.string(), id));
			++id;
		}
	}

	void Application::AddCube()
	{

		ImGui::Image((void*)pCubeIco, ImVec2{ 24.0f, 24.0f });
		ImGui::SameLine();
		if (ImGui::Button("Add Cube"))
		{
			models.push_back(std::make_unique<Model>(m_Window.Gfx(), "models\\cube.obj", id, "Cube"));
			++id;
		}
	}


	void Application::newScene()
	{
		skybox.release();
		skybox = std::make_unique<SkyBox>(m_Window.Gfx(), "textures\\skyboxmain.dds");
		models.clear();
		light.clearLights();
		scenePath = "Unnamed Scene";
		cam.Reset();
		drawGrid = true;
	}
	void Application::openScene()
	{
		std::filesystem::path filepath = FileDialogs::OpenfileA("Cube Scene (*.cubeproj)\0*.cubeproj\0\0");
		if (!filepath.empty())
		{
			SceneSerializer serializer(*this);
			serializer.Deserialize(filepath);
			scenePath = filepath.string();
		}
	}
	void Application::saveScene()
	{
		SceneSerializer serializer(*this);
		serializer.Serialize(scenePath);
	}
	void Application::saveSceneAs()
	{
		std::filesystem::path filepath = FileDialogs::Savefile("Cube Scene (*.cubeproj)\0*.cubeproj\0\0");
		if (!filepath.empty())
		{
			SceneSerializer serializer(*this);
			serializer.Serialize(filepath);
			scenePath = filepath.string();
		}
	}

}
