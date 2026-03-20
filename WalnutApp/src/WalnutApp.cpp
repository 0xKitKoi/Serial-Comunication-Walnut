#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"
#include <imgui_internal.h>
#include "Walnut/Image.h"

#include <thread>
#include <chrono>
#include "rs232.h" // Cross platform wrapper for USB Serial Com. https://gitlab.com/Teuniz/RS-232.git

#include <stdio.h>
#include <errno.h>
#pragma comment(lib, "Ws2_32.lib")


#include <mutex>
#include <condition_variable>
#include <queue>
#include <fstream>
#include <sstream>





/// Global variables

ImGuiTextBuffer console; // Continuous log buffer, append only.
// console.appendf("[+] {%s} %s\n", timestamp(), text);

/// <summary>
///  Thread buffer management.
/// </summary>
std::queue<std::string> messageQueue; // Shared buffer for messages from recv and send threads
std::mutex queueMutex; // Mutex to protect the shared buffer between threads
std::condition_variable queueCV; // Condition variable for signaling something is in the queue, ready for digestion. 
std::string Out; // dump buffer received from server to user in textbox. This is accessible to the Layer class AND the recv thread.


/// <summary>
/// Networking globals.
/// </summary>
int globalport; // m_SelectedPort isnt accessible when file->close is called. Cherno's gonna hit me over the head with a chair for this
SOCKET ConnectSocket; // Global socket shared amongst send and recv threads. I know, its horrible :(
sockaddr_in clientService;
char ipbuf[20] = { 0 };
char portbuf[6] = { 0 };

char baudbuf[10] = { 0 }; // Baudrate, can be either 9600, 19200, 38400, 57600, and 115200 bits per second (bps). (this should be a dropdown menu)
 

/// <summary>
///  Application state flags
/// </summary>
bool connectionAquired = false;
bool stopThreads = false; // Flag to signal networking threads to stop
bool showPopup = false; // about window.
bool OpenSettings = false;
bool g_IsDropdownOpen = false;
int m_OpenComPort = -1;

// Background thread for polling serial port
std::thread g_SerialPollThread;

bool debugMode = false;

double IN_flashStartTime = 0.0;
double OUT_flashStartTime = 0.0;
const double FLASH_DURATION = 5.0; // seconds
bool inputTriggerLED = false;
bool outputTriggerLED = false;

static bool refocusTextBox = false;
static bool scrollToBottom = false;



struct SaveData { // Default settings. Change to Microcontroller/Server IP:PORT
	std::string ipAddress = "127.0.0.1";
	int port = 5000;
	int baudrate = 115200; // Default baudrate for serial communication
}m_SaveData;

/// <summary>
/// Networking Receive Thread. Just continuously recieves data from the server and pushes it to the message queue.
/// </summary>
/// <param name="lpParam">Socket Object is passed in. IDK Why but Windows casts it to a LPVoid datatype..?</param>
/// <returns>returns 0 on signal to close. (stopThreads bool)</returns>
DWORD WINAPI receiveThread(LPVOID lpParam) {
	SOCKET clientSocket = reinterpret_cast<SOCKET>(lpParam);
	char buffer[1024];
	int bytesReceived;
	while (!stopThreads) {
		//inputTriggerLED = false;
		bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
		if (bytesReceived > 0) {
			IN_flashStartTime = ImGui::GetTime(); // start input LED flash timer
			buffer[bytesReceived] = '\0'; // Null-terminate the received data
			// echo back by signaling the send thread.
			//std::lock_guard<std::mutex> lock(queueMutex);
			//messageQueue.push(std::string(buffer));
			//queueCV.notify_one();
			if (connectionAquired) {
				Out.append("\n [+] " + std::string(buffer));
				console.appendf("\n[+] RECEIVED: %s\n", buffer);
				scrollToBottom = true;
			}


			inputTriggerLED = true; // light up input LED on data recv
			
		}
		else {
			//inputTriggerLED = false;
			break;
		}
	}
	//free(buffer);
	//detete[] buffer;
	return 0;
}



/*

// Continuously print anything the Pico sends
DWORD WINAPI receiver_thread(LPVOID arg) {
	unsigned char buf[1024];
	while (running) {
		int n = RS232_PollComport(cport_nr, buf, sizeof(buf) - 1);
		if (n > 0) {
			buf[n] = '\0';
			printf("<< %s", (char*)buf);
			fflush(stdout);
		}
		Sleep(10);
	}
	return 0;
}
*/


// Background serial polling loop. Calls RS232_PollComport on the opened port and
// forwards incoming bytes to the UI console buffer.
void serialPollLoop()
{
	const int BUF_SIZE = 1024;
	std::vector<unsigned char> buf(BUF_SIZE);
	while (!stopThreads) {
		if (m_OpenComPort != -1) {
			int n = RS232_PollComport(m_OpenComPort, buf.data(), BUF_SIZE-1);
			if (n > 0) {
				buf[n] = '\0';
				std::string s(reinterpret_cast<char*>(buf.data()), n);
				IN_flashStartTime = ImGui::GetTime();
				Out.append("\n [+] " + s);
				console.appendf("[+] RECEIVED: %s\n", s.c_str());
				scrollToBottom = true;
				inputTriggerLED = true;
				fflush(stdout);
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	return;
}






/// <summary>
/// Networking Send Thread. Waits for messages to be available in the message queue, then sends them to the server specified in settings.
/// </summary>
/// <param name="lpParam">Windows needs to convert arguments to LPVoid to pass to threads. This is the Socket object.</param>
/// <returns>returns 0 on signal to close. (stopThreads bool)</returns>
DWORD WINAPI sendThread(LPVOID lpParam) {
	SOCKET clientSocket = reinterpret_cast<SOCKET>(lpParam);
	while (!stopThreads) {
		//outputTriggerLED = false;
		std::unique_lock<std::mutex> lock(queueMutex);
		queueCV.wait(lock, [] { return !messageQueue.empty() || stopThreads; });

		if (stopThreads)
			break; // let 'lock' go out of scope and unlock safely

		std::string message = messageQueue.front();
		messageQueue.pop();
		// No need to manually unlock; optional, but clean:
		lock.unlock();
		OUT_flashStartTime = ImGui::GetTime(); // start output LED flash timer
		int bytes = send(clientSocket, message.c_str(),
			static_cast<int>(message.length()), 0);
		if (bytes == SOCKET_ERROR) {
			printf("Send failed %d\n", WSAGetLastError());
			console.appendf("[-] Send failed %d\n", WSAGetLastError());
			scrollToBottom = true;
			outputTriggerLED = false; // light up output LED on failure
		}
		else {
			printf("Sent %d bytes: %s\n", bytes, message.c_str());
			console.appendf("[+] %s\n", message.c_str());
			outputTriggerLED = true;          // success indicator (if desired)
			scrollToBottom = true;
		}


	}
	return 0;
}

/// <summary>
/// Sets up socket and attempts to connect to server specified in SaveData struct.
/// </summary>
/// <param name="data"> SaveData struct object, from file / settings menu</param>
void AttemptConnect(SaveData data) {
	ConnectSocket = socket(AF_INET, SOCK_STREAM, 0);
	clientService.sin_family = AF_INET;
	clientService.sin_addr.s_addr = inet_addr(data.ipAddress.c_str());
	clientService.sin_port = htons(data.port);
	if (connect(ConnectSocket, (SOCKADDR*)&clientService, sizeof(clientService)) == SOCKET_ERROR) {

		int error_code = WSAGetLastError();
		printf("Unable to connect to server.%d\n", error_code);
		console.appendf("[-] Unable to connect to server. Error Code: %d\n", error_code);
		scrollToBottom = true;
		WSACleanup();
	}
	// send a test byte to see if connection is alive. (This is also used as a VERY simple Client auth with the Proxy Server.)
	send(ConnectSocket, "W", 1, 0); 
}


/// <summary>
/// Reads and Saves IP address, Port, and Baudrate from/to Settings.txt file.
/// </summary>
void OpenSettingsFile()
{
	static std::string filePath = "Settings.txt";   // always know where we save
	ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
	ImGui::OpenPopup("Text Editor");

	if (ImGui::BeginPopupModal("Text Editor", &OpenSettings, ImGuiWindowFlags_NoTitleBar))
	{
		static bool once = [] {
			std::ifstream file(filePath);
			if (file) {
				file >> std::ws; // skip leading nl
				std::getline(file, m_SaveData.ipAddress);
				std::string port, baud;
				std::getline(file, port);
				std::getline(file, baud);
				m_SaveData.port = std::stoi(port);
				m_SaveData.baudrate = std::stoi(baud);
				file.close();
			}
			// pre-fill buffers with current values
			strncpy_s(ipbuf, m_SaveData.ipAddress.c_str(), sizeof(ipbuf));
			strncpy_s(portbuf, std::to_string(m_SaveData.port).c_str(), sizeof(portbuf));
			strncpy_s(baudbuf, std::to_string(m_SaveData.baudrate).c_str(), sizeof(baudbuf));
			return true;
			}();

		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.35f, 0.35f, 0.35f, 1.0f));

		ImGui::InputText("IP address", ipbuf, sizeof(ipbuf));
		ImGui::InputText("Port", portbuf, sizeof(portbuf));
		ImGui::InputText("Baud rate", baudbuf, sizeof(baudbuf));

		ImGui::PopStyleColor(3);

		if (ImGui::Button("Save"))
		{
			SaveData data;
			data.ipAddress = ipbuf;
			data.port = std::stoi(portbuf);
			data.baudrate = std::stoi(baudbuf);

			std::ofstream file(filePath);
			if (file) {
				file << data.ipAddress << '\n'
					<< data.port << '\n'
					<< data.baudrate << '\n';
			}
			m_SaveData = data;
			AttemptConnect(data);
		}
		ImGui::SameLine();
		if (ImGui::Button("Close"))
		{
			ImGui::CloseCurrentPopup();
			OpenSettings = false;
		}

		ImGui::EndPopup();
	}
}


void OpenAbout()
{
	ImGui::SetNextWindowSize(ImVec2(950, 500), ImGuiCond_FirstUseEver);
	ImGui::OpenPopup("Popup Window");

	if (ImGui::BeginPopupModal("Popup Window", &showPopup,
		ImGuiWindowFlags_NoTitleBar))
	{
		ImGui::TextColored(ImVec4(0.90f, 0.29f, 0.29f, 1.00f), "Scuzzy MousePad Client v1.0");
		//ImGui::Text("Scuzzy MousePad Client v1.0");
		ImGui::NewLine();
		ImGui::Text("This Program is a Serial Monitor, which communicates over ");
		ImGui::SameLine(0.0f, 0.0f); // no extra spacing
		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "COM Ports");
		ImGui::SameLine(0.0f, 0.0f);
		ImGui::Text(" with a device using the ");
		ImGui::SameLine(0.0f, 0.0f);
		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "RS-232 Protocol.");
		ImGui::Text("It also has the ability to send data over ");
		ImGui::SameLine(0.0f, 0.0f);
		ImGui::TextColored(ImVec4(1.00f, 0.65f, 0.00f, 1.00f), "raw WiFi Sockets");
		ImGui::SameLine(0.0f, 0.0f);
		ImGui::Text(" to a Server.\n This is cool because some microcontrollers have WiFi chips.");
		ImGui::NewLine();
		ImGui::Text("Toggle Between ");
		ImGui::SameLine(0.0f, 0.0f);
		ImGui::TextColored(ImVec4(1.00f, 0.65f, 0.00f, 1.00f), "Network Mode");
		ImGui::SameLine(0.0f, 0.0f);
		ImGui::Text(" and ");
		ImGui::SameLine(0.0f, 0.0f);
		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Serial Mode");
		//ImGui::SameLine(0.0f, 0.0f);
		ImGui::NewLine();
		ImGui::Text("Network Mode would send and receive data to a server ");
		ImGui::SameLine(0.0f, 0.0f);
		ImGui::TextColored(ImVec4(1.00f, 0.65f, 0.00f, 1.00f), "set in the settings : IP:PORT");
		ImGui::NewLine();
		ImGui::Text("Serial Mode would send data to The serial COM Port ");
		ImGui::SameLine(0.0f, 0.0f);
		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "selected in the dropdown menu. ( Edit->Settings )");
		//ImGui::SameLine(0.0f, 0.0f);
		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "The BaudRate is also set in the Settings.");
		ImGui::NewLine();
		ImGui::TextColored(ImVec4(0.60f, 0.60f, 0.60f, 1.00f), "Mouse Mode ");
		ImGui::SameLine(0.0f, 0.0f);
		ImGui::Text("sends X and Y coordinates over the selected communication mode.\n");
		//x`ImGui::SameLine(0.0f, 0.0f);
		ImGui::TextColored(ImVec4(0.60f, 0.60f, 0.60f, 1.00f), "Buffer looks like: '\\X(int)Y(int)'");
		ImGui::SameLine(0.0f, 0.0f);
		ImGui::Text("for the device (microcontroller) to parse.");
		ImGui::NewLine();
		ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
			"PLEASE SEE MICROCONTROLLER FIRMWARE FOR EXAMPLES.");
		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f),
			"IF YOU ARE TESTING THIS, SEE EXAMPLES FOLDER FOR THE DEMO.");

		ImGui::NewLine();
		if (ImGui::Button("Close"))
		{
			ImGui::CloseCurrentPopup();
			showPopup = false;
		}
		ImGui::EndPopup();
	}
}



class ExampleLayer : public Walnut::Layer
{
public:
	ExampleLayer() {
		buf = new unsigned char[128]; // Buffer populated by Network recv thread 
		m_UserInput = new char[128](); // User input buffer for sending data. Hardcoded to 128 bytes for now.
		console.appendf("[+] Application Started. ");
	}
	~ExampleLayer() {
		delete[] buf;
		delete[] m_UserInput;
		m_UserInput = nullptr;
		stopThreads = true;

      // Notify the sending thread to wake up and check the stopThreads flag
		queueCV.notify_one();

		// Stop and join serial poll thread
		stopThreads = true;
		if (g_SerialPollThread.joinable()) g_SerialPollThread.join();

		// Join the threads // replaced with bool flag for stopping.
		//hReadThread.join();
		//hSendThread.join();

		WSACleanup(); // Windows Sockets cleanup.
		printf("Done.");
		console.appendf("[+] Application Closed. Destructor Finished. ");
		// Write console log to file.
		std::ofstream logFile("ScuzzyClientLog.txt");
		if (logFile.is_open()) {
			logFile << console.c_str();
			logFile.close();
		}
	}


	virtual void OnAttach() override {

		ImGuiStyle& style = ImGui::GetStyle();
		ImVec4* colors = style.Colors;

		// Black window background
		colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);

		// Optional: Black frame backgrounds, buttons, etc.
		colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
		colors[ImGuiCol_FrameBg] = ImVec4(0.05f, 0.05f, 0.05f, 1.0f);
		colors[ImGuiCol_PopupBg] = ImVec4(0.05f, 0.05f, 0.05f, 1.0f);
		colors[ImGuiCol_Header] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
		colors[ImGuiCol_Button] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);



		//colors[ImGuiCol_Border] = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // green
		//colors[ImGuiCol_Border] = ImVec4(0.0f, 0.8f, 0.0f, 1.0f); // slightly dimmer green
		//colors[ImGuiCol_Border] = ImVec4(0.0f, 0.6f, 0.0f, 1.0f); // darker, calmer green
		//colors[ImGuiCol_Border] = ImVec4(0.8f, 0.0f, 0.0f, 1.0f); // dark red
		colors[ImGuiCol_Border] = ImVec4(0.6f, 0.0f, 0.0f, 1.0f); // darker, calmer red

		//colors[ImGuiCol_Border] = ImVec4(0.0f, 1.0f, 1.0f, 1.0f); // bright cyan (full opacity)
		//colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.1f, 0.1f, 1.0f); // very dark teal for contrast
		//colors[ImGuiCol_Border] = ImVec4(1.0f, 0.1f, 0.1f, 1.0f);  // bright red border
		//colors[ImGuiCol_Border] = ImVec4(1.0f, 0.65f, 0.0f, 1.0f); // Orange (RGB: 255, 165, 0)
		//colors[ImGuiCol_ChildBg] = ImVec4(0.1f, 0.0f, 0.0f, 1.0f);  // very dark red background for contrast


		// You can tweak these for glowing/yellow active styles! TODO: make theme selectable in settings.



		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
			printf( "WSAStartup failed\n");
			console.appendf("[-] WSAStartup failed! ");
		}


		// read settings from file.
		std::ifstream m_SettingsFile("Settings.txt");
		if (m_SettingsFile.is_open()) {
			std::string line;
			for (int i = 0; i < 3; i++) {
				std::getline(m_SettingsFile, line);
				if (i == 1) {
					m_SaveData.port = std::stoi(line);
				}
				else if (i == 0) {
					m_SaveData.ipAddress = line;
				}
				else if (i == 2) {
					m_SaveData.baudrate = std::stoi(line);
				}
			}
			m_SettingsFile.close();
		}
		else { 
			// default settings
			std::ofstream file("Settings.txt");
			if (file.is_open()) {
				file << m_SaveData.ipAddress << std::endl;
				file << m_SaveData.port << std::endl;
				file << m_SaveData.baudrate << std::endl;
				file.close();
			}
		}


		// Scan for COM ports on Initialization.
		wchar_t lpTargetPath[5000];
		for (int i = 0; i < 255; i++) {
			std::wstring str = L"COM" + std::to_wstring(i); // converting to COM0, COM1, COM2
			DWORD res = QueryDosDevice(str.c_str(), lpTargetPath, 5000);

			// Test the return value and error if any
			if (res != 0) //QueryDosDevice returns zero if it didn't find an object
			{
				m_ComPorts.push_back(i);
				//std::cout << str << ": " << lpTargetPath << std::endl;
			}
			if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
			}

		}
	}


	void DrawVHSOverlay() // Retro green scanline effect
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (!window) return;

		ImDrawList* draw_list = window->DrawList;

		ImVec2 pos = window->Pos;
		ImVec2 size = window->Size;

		float time = ImGui::GetTime();

		float spacing = 10.0f; // Wider spacing for less busy scanlines

		// Draw scanlines across full window height
		for (float y = pos.y; y < pos.y + size.y; y += spacing)
		{
			// Alpha pulsates between 20 and 70 for subtle flicker
			float alpha = 0.0f + 50.0f * (0.5f + 0.5f * sinf(time * 6.0f + y * 0.1f));
			ImU32 color = IM_COL32(0, 255, 0, (int)alpha);

			draw_list->AddLine(ImVec2(pos.x, y), ImVec2(pos.x + size.x, y), color, 1.0f);
		}

		// Optional: faint glowing green border around whole window
		ImU32 glow = IM_COL32(0, 255, 100, 60 + (int)(30.0f * (0.5f + 0.5f * sinf(time * 3.0f))));
		draw_list->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), glow, 0.0f, 0, 3.0f);
	}



void DrawWormholeGridBackground()
{
	ImDrawList* draw_list = ImGui::GetBackgroundDrawList(); // draw behind everything

	ImVec2 displaySize = ImGui::GetIO().DisplaySize;
	ImVec2 center = ImVec2(displaySize.x * 0.5f, displaySize.y * 0.5f);
	float maxRadius = ImMin(center.x, center.y) * 1.5f;

	int rings = 10;
	int spokes = 24;

	float time = ImGui::GetTime();

	// Draw concentric rings that pulse or swirl
	for (int i = 1; i <= rings; ++i)
	{
		float radius = (maxRadius / rings) * i * (0.8f + 0.2f * sinf(time * 2.0f + i));
		ImU32 ringColor = IM_COL32(100, 100, 255, 40 + (int)(sin(time * 3.0f + i) * 20));
		draw_list->AddCircle(center, radius, ringColor, 64, 1.0f);
	}

	// Draw radial spokes rotating slowly
	for (int i = 0; i < spokes; ++i)
	{
		float angle = (2 * IM_PI * i / spokes) + time * 0.2f;
		ImVec2 p1 = ImVec2(center.x + cosf(angle) * (maxRadius * 0.1f),
			center.y + sinf(angle) * (maxRadius * 0.1f));
		ImVec2 p2 = ImVec2(center.x + cosf(angle) * maxRadius,
			center.y + sinf(angle) * maxRadius);
		ImU32 spokeColor = IM_COL32(150, 150, 255, 30 + (int)(cos(time * 3.5f + i) * 20));
		draw_list->AddLine(p1, p2, spokeColor, 1.0f);
	}
}




void DrawRetroMousePad() // FOR MOUSE MODE!
{
	ImVec2 padSize = ImVec2(300, 300);
	ImVec2 padTopLeft = ImGui::GetCursorScreenPos();
	ImVec2 padBottomRight = ImVec2(padTopLeft.x + padSize.x, padTopLeft.y + padSize.y);
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	draw_list->AddRectFilled(padTopLeft, padBottomRight, IM_COL32(0, 0, 0, 200), 0);



	// Draw gradually decreasing squares background with brighter colors
	int numSquares = 10;
	float maxSize = padSize.x;  // Assuming square pad, width == height

	for (int i = 0; i < numSquares; ++i)
	{
		float t = (float)i / (float)numSquares; // 0 to almost 1
		float size = maxSize * (1.0f - t * 0.8f); // Shrink to ~20% size
		float alpha = 90 * (1.0f - t); // Increased alpha from 40 to 90

		ImVec2 center = ImVec2(padTopLeft.x + padSize.x / 2.0f, padTopLeft.y + padSize.y / 2.0f);
		ImVec2 topLeft = ImVec2(center.x - size / 2.0f, center.y - size / 2.0f);
		ImVec2 bottomRight = ImVec2(center.x + size / 2.0f, center.y + size / 2.0f);

		// More saturated orange for brightness
		ImU32 color = IM_COL32(255, 200, 50, (int)alpha);
		draw_list->AddRect(topLeft, bottomRight, color, 6.0f, 0, 1.5f);
	}



	ImVec2 turretCenter = ImVec2(padTopLeft.x + padSize.x / 2.0f, padBottomRight.y);

	// Draw arc as guide (180 degrees)
	int segments = 64;
	float radius = padSize.y;
	//ImU32 arcColor = IM_COL32(255, 255, 100, 80);
	ImU32 arcColor = IM_COL32(255, 255, 120, 130); // brighter & more visible
	draw_list->PathArcTo(turretCenter, radius, IM_PI, 2 * IM_PI, segments);
	draw_list->PathStroke(arcColor, false, 1.5f);

	float time = ImGui::GetTime();
	float pulse = (sinf(time * 2.0f) * 0.5f + 0.5f); // oscillates 0 to 1
	int alpha = 50 + int(pulse * 80);
	ImU32 gridColor = IM_COL32(255, 165, 0, alpha); // glowing orange


	// Curved guide from top-left to bottom-right
	ImVec2 p0 = padTopLeft;
	ImVec2 p1 = ImVec2(padTopLeft.x + padSize.x * 0.3f, padTopLeft.y + padSize.y * 0.1f);
	ImVec2 p2 = ImVec2(padTopLeft.x + padSize.x * 0.7f, padTopLeft.y + padSize.y * 0.9f);
	ImVec2 p3 = padBottomRight;

	ImU32 curveColor = IM_COL32(255, 100, 100, 80); // subtle red glow

	draw_list->PathClear();
	draw_list->PathLineTo(p0);  // Move to start point by adding a line to the same point
	draw_list->PathBezierCubicCurveTo(p1, p2, p3, 50);
	draw_list->PathStroke(IM_COL32(255, 255, 255, 180), false, 1.5f);



	// --- Background Grid (+ signs) ---
	float spacing = 40.0f;
	float symbolSize = ImGui::CalcTextSize("+").x;
	for (float y = padTopLeft.y + 5; y <= padBottomRight.y; y += spacing)
	{
		for (float x = padTopLeft.x + 10; x <= padBottomRight.x; x += spacing)
		{
			draw_list->AddText(ImVec2(x - symbolSize * 0.5f, y - symbolSize * 0.5f), gridColor, "+");
		}
	}

	// --- Interactive Area ---
	ImGui::InvisibleButton("##RetroPad", padSize);
	bool hovered = ImGui::IsItemHovered();
	bool held = ImGui::IsMouseDown(ImGuiMouseButton_Left);

	// --- Glowing Red Border ---
	for (int i = 0; i < 5; ++i)
	{
		float thickness = 1.0f + i;
		ImU32 color = IM_COL32(255, 50, 50, 50 - i * 10);
		draw_list->AddRect(padTopLeft, padBottomRight, color, 0.0f, 0, thickness);
	}

	ImVec2 padMin = ImGui::GetItemRectMin();
	ImVec2 padMax = ImGui::GetItemRectMax();
	ImVec2 mousePos = ImGui::GetMousePos();
	ImVec2 targetPos = ImVec2(padMin.x + m_MouseX, padMin.y + m_MouseY);

	// Draw laser guide from turret to current mouse coordinates
	draw_list->AddLine(turretCenter, targetPos, IM_COL32(255, 255, 255, 200), 2.0f);


	// Always draw crosshair lines at stored position
	for (int i = 0; i < 4; ++i)
	{
		float thickness = 1.0f + i;
		ImU32 color = IM_COL32(255, 50, 50, 60 - i * 10);
		draw_list->AddLine(ImVec2(padTopLeft.x, targetPos.y), ImVec2(padBottomRight.x, targetPos.y), color, thickness);
		draw_list->AddLine(ImVec2(targetPos.x, padTopLeft.y), ImVec2(targetPos.x, padBottomRight.y), color, thickness);
	}

	// Optional: Always draw pulsing circle at last position
	float pulseRadius = 4.0f + 2.0f * sinf(ImGui::GetTime() * 4.0f);
	ImU32 circleColor = IM_COL32(255, 255, 255, 180);
	draw_list->AddCircle(targetPos, pulseRadius, circleColor, 16, 2.0f);

	if (hovered && held)
	{
		ImVec2 localMouse = ImVec2(mousePos.x - padMin.x, mousePos.y - padMin.y);

		// Only update if inside bounds
		if (localMouse.x >= 0 && localMouse.x <= padSize.x &&
			localMouse.y >= 0 && localMouse.y <= padSize.y)
		{
			m_MouseX = localMouse.x;
			m_MouseY = localMouse.y;
			//printf("Mouse X: %f, Mouse Y: %f\n", m_MouseX, m_MouseY);
			// send m_MouseX and m_MouseY over selected communication mode here.
			if (m_NetworkMode) {
				// send over network
				std::string message = "\\X" + std::to_string((int)m_MouseX) + "Y" + std::to_string((int)m_MouseY) + "\r\n";
				// populate Message Queue and notify sending thread
				{
					//send(ConnectSocket, m_UserInput, i, 0); // Blocking
					std::lock_guard<std::mutex> lock(queueMutex);
					messageQueue.push(message);
					queueCV.notify_one();
				}
			}
			else { // Serial Mode
				std::string msg = "\\X" + std::to_string((int)m_MouseX) +
					"Y" + std::to_string((int)m_MouseY) + "\r\n";

				std::vector<unsigned char> buf(msg.begin(), msg.end()); // mutable

				int result = RS232_SendBuf(m_ComPorts.at(m_SelectedPort) - 1,
					buf.data(),               // unsigned char * (non-const)
					static_cast<int>(buf.size()));
				console.appendf("[+] Sent to COM%d: %s\n", m_ComPorts.at(m_SelectedPort), msg.c_str());
				outputTriggerLED = true; // trigger LED on send
				if (result < 0) {
					console.appendf("[-] Error sending data over serial port.\n");
					scrollToBottom = true;
					outputTriggerLED = false;
					isComPortConnected = false;
				}
			}
			outputTriggerLED = true; // trigger LED on send

			ImVec2 mouseRel = ImVec2(mousePos.x - padTopLeft.x, mousePos.y - padTopLeft.y);

			// Optional: Crosshairs
			for (int i = 0; i < 4; ++i)
			{
				float thickness = 1.0f + i;
				ImU32 color = IM_COL32(255, 50, 50, 60 - i * 10);
				draw_list->AddLine(ImVec2(padTopLeft.x, mousePos.y), ImVec2(padBottomRight.x, mousePos.y), color, thickness);
				draw_list->AddLine(ImVec2(mousePos.x, padTopLeft.y), ImVec2(mousePos.x, padBottomRight.y), color, thickness);
			}

			// Pulsing circle at mouse position
			float pulseRadius = 4.0f + 2.0f * sinf(time * 4.0f);
			ImU32 circleColor = IM_COL32(255, 255, 255, 180);
			draw_list->AddCircle(mousePos, pulseRadius, circleColor, 16, 2.0f);
		}
	}
}




    // Returns true if selection changed.
	/// dropdown menu for com ports
	bool CustomTrapezoidDropdown(const char* label, const std::vector<int>& items, int& selectedIndex)
	{
     ImDrawList* draw_list = ImGui::GetWindowDrawList();
		ImVec2 pos = ImGui::GetCursorScreenPos();

		// Dimensions
		float tabHeight = 30.0f;
		float width = 200.0f;
		float dropdownMaxHeight = 150.0f;

		// Define trapezoid corners (visual only)
		ImVec2 tabTopLeft = pos;
		ImVec2 tabTopRight = ImVec2(pos.x + width - 20, pos.y);
		ImVec2 tabBottomRight = ImVec2(pos.x + width, pos.y + tabHeight);
		ImVec2 tabBottomLeft = ImVec2(pos.x, pos.y + tabHeight);

		// Draw trapezoid tab background
		ImU32 tabFillColor = IM_COL32(0, 0, 0, 255); // solid black fill
		ImVec2 tabPoints[4] = { tabTopLeft, tabTopRight, tabBottomRight, tabBottomLeft };
		draw_list->AddConvexPolyFilled(tabPoints, IM_ARRAYSIZE(tabPoints), tabFillColor);
		for (int g = 2; g >= 0; --g)
		{
			float thickness = 1.5f + g * 1.2f;
			ImU32 glowColor = IM_COL32(255, 255, 100, 60 - g * 15);
			draw_list->AddPolyline(tabPoints, IM_ARRAYSIZE(tabPoints), glowColor, true, thickness);
		}
		ImVec2 textSize = ImGui::CalcTextSize(label);
		ImVec2 tabtextPos = ImVec2(pos.x + (width - textSize.x) * 0.5f, pos.y + (tabHeight - textSize.y) * 0.5f);
		draw_list->AddText(tabtextPos, IM_COL32(255, 255, 255, 255), label);

		bool selectionChanged = false;

		std::string popup_id = std::string("##trapezoid_popup_") + label;

		// Invisible button to receive clicks on trapezoid
		ImGui::SetCursorScreenPos(pos);
		ImGui::InvisibleButton((std::string("##trapezoid_btn_") + label).c_str(), ImVec2(width, tabHeight));
		if (ImGui::IsItemClicked()) {
			ImGui::OpenPopup(popup_id.c_str());
			// debug log
			if (items.empty()) console.appendf("[-] Dropdown opened: no COM ports detected\n");
			else {
				console.appendf("[+] Dropdown opened:\n");
				for (size_t di = 0; di < items.size(); ++di) console.appendf("  idx=%zu -> COM%d\n", di, items[di]);
			}
			scrollToBottom = true;
		}

		// Position popup under trapezoid and make background transparent so trapezoid shows through
		ImVec2 dropdownPos = ImVec2(pos.x, pos.y + tabHeight);
		ImGui::SetNextWindowPos(dropdownPos);
		ImGui::SetNextWindowSize(ImVec2(width, dropdownMaxHeight));
		ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.01f, 0.01f, 0.01f, 0.9f));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.01f, 0.01f, 0.01f, 0.0f));
		if (ImGui::BeginPopup(popup_id.c_str(), ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove)) {
			ImGui::BeginChild((popup_id + std::string("_child")).c_str(), ImVec2(width, dropdownMaxHeight), false, ImGuiWindowFlags_NoScrollWithMouse);
			for (int i = 0; i < (int)items.size(); ++i) {
				bool isSelected = (selectedIndex == i);
				std::string item = "COM" + std::to_string(items[i]);
				ImGui::PushStyleColor(ImGuiCol_Text, isSelected ? ImVec4(1.0f, 0.8f, 0.2f, 1.0f) : ImVec4(1.0f, 1.0f, 0.6f, 1.0f));
				if (ImGui::Selectable(item.c_str(), isSelected)) {
					//if comport is already open, close it before opening new one.
					if (m_OpenComPort != -1) {
						RS232_CloseComport(m_OpenComPort);
						m_OpenComPort = -1;
						isComPortConnected = false;
						console.appendf("[*] Closed previously open COM port.\n");
					}



					int portIndex = items[i] -1;
					if (m_OpenComPort != -1 && m_OpenComPort != portIndex) {
						RS232_CloseComport(m_OpenComPort);
						m_OpenComPort = -1;
					}
					char mode[] = { '8','N','1',0 };
					int openRes = RS232_OpenComport(portIndex, m_SaveData.baudrate, mode, 0);
					if (openRes != 0) {
						console.appendf("[-] Error opening COM%d: ret=%d\n", portIndex + 1, openRes);
						#ifdef _WIN32
						DWORD winErr = GetLastError();
						console.appendf("    OS Error: GetLastError=%lu\n", (unsigned long)winErr);
						#else
						console.appendf("    OS Error: errno=%d (%s)\n", errno, strerror(errno));
						#endif
						isComPortConnected = false;
					} else {
						selectedIndex = i;
						m_OpenComPort = portIndex;
						isComPortConnected = true;
						console.appendf("[+] Connected to com port: COM%d (idx=%d)\n", items[i], i);
						selectionChanged = true;
						// start comport thread 
						//if (!g_SerialPollThread.joinable()) {
						//	g_SerialPollThread = std::thread(&serialPollLoop, this);
						//}
						std::thread ComPortPollThread(serialPollLoop);
						ComPortPollThread.detach();
					}
					ImGui::CloseCurrentPopup();
				}
				ImGui::PopStyleColor();
			}
			ImGui::EndChild();
			ImGui::EndPopup();
		}
		ImGui::PopStyleColor(2);

		// restore cursor
		ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + tabHeight + 4));

		return selectionChanged;
	}


void DrawRetroStatusLED(const char* label, bool isOn, ImVec2 pos)
{
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	float time = ImGui::GetTime();

	bool flashVisible = fmod(time * 10.0f, 1.0f) < 0.5f;

	const float ledWidth = 20.0f;
	const float ledHeight = 12.0f;

	ImVec2 ledPos = pos;
	ImVec2 textPos = ImVec2(pos.x + ledWidth + 8, pos.y + 1);

	ImU32 color = isOn ? IM_COL32(0, 255, 0, 255) : IM_COL32(255, 50, 50, 255);
	ImU32 glowColor = isOn ? IM_COL32(0, 255, 0, 180) : IM_COL32(255, 50, 50, 180);

	if (flashVisible)
	{
		// Glowing outline (square shape)
		for (int i = 0; i < 3; ++i)
		{
			float grow = static_cast<float>(i);
			draw_list->AddRect(
				ImVec2(ledPos.x - grow, ledPos.y - grow),
				ImVec2(ledPos.x + ledWidth + grow, ledPos.y + ledHeight + grow),
				glowColor, 0.0f, 0, 1.5f);
		}

		// Solid fill (no rounding)
		draw_list->AddRectFilled(
			ledPos,
			ImVec2(ledPos.x + ledWidth, ledPos.y + ledHeight),
			color,
			0.0f); // No corner rounding
	}

	// Text
	draw_list->AddText(textPos, color, label);
}




	virtual void OnUIRender() override
	{	

		ImGui::Begin("Laser Turret Control Panel");
			DrawWormholeGridBackground();


		// Settings + About Popups
		if (OpenSettings) OpenSettingsFile();
		if (showPopup) OpenAbout();

		// Status LEDs, intentional delay on input LED to simulate processing time
		
		if (outputTriggerLED) {
			comPortHasOutput = true;
			printf("%f", (ImGui::GetTime() - OUT_flashStartTime));
			if ((ImGui::GetTime() - OUT_flashStartTime) >= FLASH_DURATION) {
				outputTriggerLED = false;
				comPortHasOutput = false;
				OUT_flashStartTime = 0.0f; // reset timer
			}
		}
		if (inputTriggerLED) {
			comPortHasInput = true;
			if ((ImGui::GetTime() - IN_flashStartTime) >= FLASH_DURATION) {
				inputTriggerLED = false;
				comPortHasInput = false;
				IN_flashStartTime = 0.0f; // reset timer
			}
		}

		//if (outputTriggerLED) { comPortHasOutput = true;  }
		//else { comPortHasOutput = false; }
		//if (inputTriggerLED) { comPortHasInput = true; }
		//else { comPortHasInput = false; }

		// Network / Serial Toggle
		ImGui::Text("Sending All Data over Mode: ");
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), m_NetworkMode ? "Network ( Server IP:PORT in Settings )" : "Serial ( COM Port Selected in DropDown Menu )");

		ImGui::SameLine();
		if (ImGui::Button("Toggle Serial/Network")) {
			m_NetworkMode = !m_NetworkMode;
			console.appendf("[+] Application Mode Switched: [%s]\n", m_NetworkMode ? "Network" : "Serial");
			scrollToBottom = true;

			if (m_NetworkMode) {
				if (ConnectSocket == INVALID_SOCKET) {           // close old one first
					closesocket(ConnectSocket);
					connectionAquired = false;                   // reset connection flag
					console.appendf("[-] Invalid Socket Passed into Networking Threads! wow!");
					scrollToBottom = true;
					isNetworkConnected = false;
				}


				if (connectionAquired) {
					// Already connected, dont try to reconnect or make a new socket every frame.
					isNetworkConnected = true;
					return;
				}
				else {

					// Create the socket
					ConnectSocket = socket(AF_INET, SOCK_STREAM, 0);
					if (ConnectSocket == INVALID_SOCKET) {
						printf("Socket creation failed: %d\n", WSAGetLastError());
						console.appendf("[-] Socket Creation Failed! [%d]\n", WSAGetLastError());
						scrollToBottom = true;
						isNetworkConnected = false;
						return;
					}

					// From SaveFile, change in Settings Popup
					clientService.sin_family = AF_INET;
					clientService.sin_addr.s_addr = inet_addr(m_SaveData.ipAddress.c_str());
					clientService.sin_port = htons(m_SaveData.port);

					// Attempt to connect
					if (connect(ConnectSocket, (SOCKADDR*)&clientService, sizeof(clientService)) == SOCKET_ERROR) {
						int error_code = WSAGetLastError();
						printf("Unable to connect to server. %d\n", error_code);
						console.appendf("[-] Unable to connect to server! [%d]\n", error_code);
						scrollToBottom = true;
						closesocket(ConnectSocket);
						ConnectSocket = INVALID_SOCKET;
						connectionAquired = false;
						isNetworkConnected = false;
					}
					else {
						send(ConnectSocket, "W", 1, 0); // initial handshake or ping!

						// Launch threads
						SOCKET sockCopy = ConnectSocket;   // copy while it’s still valid, IDK why this works but static_cast doesn't?
						//std::thread recvThread(&receiveThread, static_cast<void*>(&ConnectSocket));
						//std::thread sendThread(&sendThread, static_cast<void*>(&ConnectSocket));
						std::thread recvThread(&receiveThread, reinterpret_cast<void*>(sockCopy));
						std::thread sendThread(&sendThread, reinterpret_cast<void*>(sockCopy));
						recvThread.detach();
						sendThread.detach();

						connectionAquired = true;
						isNetworkConnected = true;
						console.appendf("[+] Connected to server at %s:%d\n", m_SaveData.ipAddress.c_str(), m_SaveData.port);
						scrollToBottom = true;
					}
				}

			}
			else {
				closesocket(ConnectSocket);
				ConnectSocket = INVALID_SOCKET;
				connectionAquired = false;
				isNetworkConnected = false;
			}

		}


		// Begin Columns with 2 columns, no resizing
		ImGui::Columns(2, nullptr, false);

		// LEFT COLUMN — group mousepad + dropdown + coords inside ONE child box
		ImGui::BeginChild("LeftGroupBox", ImVec2(0, 0), true); // fill column

		// Mouse Pad
		ImGui::BeginGroup();
		ImGui::Text("Mouse Control Pad");
		DrawRetroMousePad();

		ImGui::Spacing();
		ImGui::Dummy(ImVec2(0, 20)); // Push down below coords


		ImVec2 indicatorStart = ImGui::GetCursorScreenPos();
		float time = ImGui::GetTime();

		DrawRetroStatusLED("Networking",isNetworkConnected,  indicatorStart);
		DrawRetroStatusLED("COMs", isComPortConnected, ImVec2(indicatorStart.x, indicatorStart.y + 25));
		/*DrawRetroStatusLED("COM In", comPortHasInput, ImVec2(indicatorStart.x, indicatorStart.y + 50));*/
		DrawRetroStatusLED("COM In", inputTriggerLED, ImVec2(indicatorStart.x, indicatorStart.y + 50));
		/*DrawRetroStatusLED("COM Out", comPortHasOutput, ImVec2(indicatorStart.x, indicatorStart.y + 75));*/
		DrawRetroStatusLED("COM Out", outputTriggerLED, ImVec2(indicatorStart.x, indicatorStart.y + 75));

		// Use Dummy to push the cursor so the next widgets don't overlap
		ImGui::Dummy(ImVec2(150, 80)); // reserve space


		ImGui::EndGroup();

		// Move cursor to right inside same child
		ImGui::SameLine();

		// Dropdown + Mouse coords stacked vertically
		ImGui::BeginGroup();

		float dropdownReservedHeight = 200.0f; // fixed height for dropdown area
		ImGui::BeginChild("DropdownChild", ImVec2(250, dropdownReservedHeight), true, ImGuiWindowFlags_NoScrollbar);
		

            // Refresh COM port list each frame so dropdown stays up-to-date
			m_ComPorts.clear();
			wchar_t lpTargetPath[5000];
			for (int i = 0; i < 255; i++) {
				std::wstring str = L"COM" + std::to_wstring(i);
				DWORD res = QueryDosDevice(str.c_str(), lpTargetPath, 5000);
				if (res != 0) {
					m_ComPorts.push_back(i);
				}
			}

			// Draw dropdown (selection/open handled inside the function)
			CustomTrapezoidDropdown("Select a COM Port", m_ComPorts, m_SelectedPort);
		ImGui::EndChild();

			ImGui::Dummy(ImVec2(0, 10)); // spacing

			// Position & size
			ImVec2 coordBoxSize = ImVec2(200, 50); // adjust as needed
			ImVec2 coordBoxPos = ImGui::GetCursorScreenPos();
			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			ImVec2 coordBoxEnd = ImVec2(coordBoxPos.x + coordBoxSize.x, coordBoxPos.y + coordBoxSize.y);

			for (int i = 0; i < 4; ++i)
			{
				float thickness = 1.0f + i;
				ImU32 glow = IM_COL32(0, 255, 100, 60 - i * 10);
				draw_list->AddRect(coordBoxPos, coordBoxEnd, glow, 6.0f, 0, thickness);
			}

			ImU32 fillColor = IM_COL32(0, 0, 0, 200);
			draw_list->AddRectFilled(coordBoxPos, coordBoxEnd, fillColor, 6.0f);

			ImVec2 innerPos = ImVec2(coordBoxPos.x + 20, coordBoxPos.y /*+ 10*/);
			ImGui::SetCursorScreenPos(innerPos);

			// Glowing green text
			ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 100, 255));
			ImGui::Text("Mouse X: %.1f", m_MouseX);
			ImGui::Text(" Mouse Y: %.1f", m_MouseY);
			ImGui::PopStyleColor();

			// Add spacing afterward if needed
			ImGui::Dummy(ImVec2(coordBoxSize.x, coordBoxSize.y));
			ImGui::Spacing();
			ImGui::EndGroup(); // End dropdown + coords group

			ImGui::EndChild(); // End left column child

			// Move to next column — RIGHT COLUMN
			ImGui::NextColumn();

			ImGui::BeginChild("TextBox", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar);

			ImGui::Text("Serial / Network Terminal");

			if (refocusTextBox) {
				ImGui::SetKeyboardFocusHere();
			}
			//ImGui::SetKeyboardFocusHere(refocusTextBox ? -1 : 0);
			//refocusTextBox = false;               // clear flag immediately
			bool sendByEnter = ImGui::InputText("##Text", m_UserInput, 128, ImGuiInputTextFlags_EnterReturnsTrue);
			ImGui::SameLine();

			bool sendByClick = ImGui::Button("Send");
			if (sendByEnter || sendByClick) {
				outputTriggerLED = true; // trigger output LED on send
				int len = strlen(m_UserInput); // safer than looping
				char sendBuffer[130]; // 128 + \n + \0
				memcpy(sendBuffer, m_UserInput, len);
				//sendBuffer[len] = '\n';  // newline to trigger Arduino parsing? LOL
				//sendBuffer[len + 1] = '\0';
				sendBuffer[len] = '\r';  // newline to trigger Arduino parsing? LOL
				sendBuffer[len + 1] = '\n';


				if (debugMode) {
					// hexdump time. 
					console.appendf("\n");
					console.appendf("  **  HEX DUMP  **\n");
					const int COLS = 8;
					console.appendf("___________________________________\n");
					console.appendf("00 01 02 03 04 05 06 07  | ascii  |\n");
					console.appendf("___________________________________\n");
					for (int i = 0; i < len; i += COLS) {
						for (int j = 0; j < COLS; j++) {
							unsigned char b = (i + j < len) ? (unsigned char)sendBuffer[i + j] : 0x00;
							printf("%s%02X", j ? " " : "", b);
							console.appendf("%s%02X", j ? " " : "", b);
						}
						printf("  |");
						console.appendf("  |");
						for (int j = 0; j < COLS; j++) {
							unsigned char b = (i + j < len) ? (unsigned char)sendBuffer[i + j] : 0x00;
							char ch = (i + j < len) ? ((b >= 32 && b < 127) ? b : '.') : '0';
							printf("%c", ch);
							console.appendf("%c", ch);
						}
						printf("|\n");
						console.appendf("|\n");
					}
					console.appendf("___________________________________\n");
				}

				



				//printf("\n%d byte DUMP:", len + 1);
				//console.appendf("\n%d byte DUMP:", len + 1);
				//printf("\n%.*s", len + 1, sendBuffer);
				//console.appendf("\n%.*s", len + 1, sendBuffer);
				//printf("\n%d byte DUMP (HEX):", len + 1);
				//console.appendf("\n%d byte DUMP (HEX):", len + 1);
				//for (int j = 0; j < len + 1; j++) {
				//	printf(" %02X", (unsigned char)sendBuffer[j]);
				//	console.appendf(" %02X", (unsigned char)sendBuffer[j]);
				//}
				//printf("\n");
				//console.appendf("\n");
				scrollToBottom = true;

				// We have user input, send over user selected protocol
				if (!m_NetworkMode) {
					// Write to COM port.
					//int e = RS232_SendBuf(m_ComPorts.at(m_SelectedPort) - 1, reinterpret_cast<unsigned char*>(m_UserInput), i+1);
					int result = RS232_SendBuf(m_ComPorts.at(m_SelectedPort) - 1, reinterpret_cast<unsigned char*>(sendBuffer), len + 1);
					//RS232_cputs((m_ComPorts.at(m_SelectedPort) - 1), m_UserInput);

					if (result < 0) {
						printf("\nWriting failed! Port: %d", m_ComPorts.at(m_SelectedPort));
						console.appendf("\n[-] Writing failed! Port: COM%d", m_ComPorts.at(m_SelectedPort));
						printf("\n%.*s", len, sendBuffer);
						console.appendf("\n%.*s", len, sendBuffer);
						scrollToBottom = true;
						comPortHasOutput = false; 
					}
					printf("\nWrote %d bytes on Comport %d\n", result, m_ComPorts.at(m_SelectedPort));
					console.appendf("\n[+] Wrote %d bytes on COM%d\n", result, m_ComPorts.at(m_SelectedPort));
					comPortHasOutput = true;

				}
				else {
					// TODO: Send Text to Server.
					//send(ConnectSocket, m_UserInput, i, 0); // int result = RS232_SendBuf(m_ComPorts.at(m_SelectedPort) - 1, reinterpret_cast<unsigned char*>(sendBuffer), len + 1);
					std::lock_guard<std::mutex> lock(queueMutex);
					messageQueue.push(m_UserInput);
					queueCV.notify_one();
					isNetworkConnected = true;

				}
				memset(m_UserInput, 0, sizeof(m_UserInput)); // clear the user input buffer after sending.

				Out += "\n[+] Sent: ";
				Out += sendBuffer; // This is the "console window" in Text Mode.
				Out += "\n";
				//ImGui::SetKeyboardFocusHere(-1);
				refocusTextBox = true;
			}
			else {
				refocusTextBox = false;
			}





			ImGui::SameLine();
			if (ImGui::Button("Clear Buffers")) {
				Out.clear();
				console.clear();
				memset(m_UserInput, 0, 128);
			}


			// Console Output Area
			ImGui::BeginChild("Console", ImVec2(0, 300), true, ImGuiWindowFlags_HorizontalScrollbar);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.65f, 0.0f, 1.0f));
			console.append("\n");
			ImGui::InputTextMultiline(
				"##console",
				const_cast<char*>(console.c_str()),
				console.size() + 1,
				ImVec2(-FLT_MIN, -FLT_MIN),
				ImGuiInputTextFlags_ReadOnly
			);
			console.Buf.pop_back();  // remove the \0
			console.Buf.pop_back();  // remove the \n
			console.Buf.push_back('\0');  // restore terminator

			if (scrollToBottom) {
				for (ImGuiWindow* w : ImGui::GetCurrentContext()->Windows) {
					if (w && strstr(w->Name, "##console")) {
						ImGui::SetScrollY(w, w->ScrollMax.y + 9999.0f);
						break;
					}
				}
				scrollToBottom = false;
			}

			ImGui::PopStyleColor();
			//if (scrollToBottom)
			//	ImGui::SetScrollHereY(1.0f);
			scrollToBottom = false;
			ImGui::EndChild();




			// BUTTONS FOR SENDING SPECIAL COMMANDS LIKE RESETS

			if (ImGui::Button("Serial Reset DTR [HARD RESET]")) {
				std::thread([this]() {
					RS232_enableDTR(m_OpenComPort);
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
					RS232_disableDTR(m_OpenComPort);
					std::this_thread::sleep_for(std::chrono::milliseconds(1500));
					RS232_flushRXTX(m_OpenComPort);
					}).detach();
				console.appendf("[*] DTR reset sent.\n");
				scrollToBottom = true;
			}

			if (ImGui::Button("Reset MicroPython Device [CTRL+D]")) {
				std::thread([this]() {
					RS232_SendByte(m_OpenComPort, 0x03);  // Ctrl+C - interrupt
					std::this_thread::sleep_for(std::chrono::milliseconds(200));
					RS232_SendByte(m_OpenComPort, 0x02);  // Ctrl+B - exit raw REPL
					std::this_thread::sleep_for(std::chrono::milliseconds(200));
					RS232_SendByte(m_OpenComPort, 0x04);  // Ctrl+D - soft reset
					std::this_thread::sleep_for(std::chrono::milliseconds(1500));
					RS232_flushRXTX(m_OpenComPort);
					}).detach();
				console.appendf("[*] Soft reset sent, waiting for device...\n");
				scrollToBottom = true;
			}
			//if (ImGui::Button("Serial Reset DTR [HARD RESET]")) {
			//	RS232_enableDTR(m_OpenComPort);
			//	std::this_thread::sleep_for(std::chrono::milliseconds(100));
			//	RS232_disableDTR(m_OpenComPort);
			//	std::this_thread::sleep_for(std::chrono::milliseconds(1500));
			//	RS232_flushRXTX(m_OpenComPort);
			//	console.appendf("[*] DTR reset sent.\n");
			//	scrollToBottom = true;
			//}
			//if (ImGui::Button("Reset MicroPython Device [sends CTRL+D]")) {
			//	//RS232_SendByte(m_OpenComPort, 0x04);  // Ctrl+D soft reset (MicroPython)
			//	//std::this_thread::sleep_for(std::chrono::milliseconds(1500));
			//	//RS232_flushRXTX(m_OpenComPort);
			//	//console.appendf("[*] Device reset sent.\n");
			//	//scrollToBottom = true;
			//	// Run this right after successfully opening the port
			//	RS232_SendByte(m_OpenComPort, 0x03);  // Ctrl+C - stop REPL
			//	//Sleep(200);
			//	std::this_thread::sleep_for(std::chrono::milliseconds(200));
			//	RS232_SendByte(m_OpenComPort, 0x04);  // Ctrl+D - soft reset, runs main.py
			//	std::this_thread::sleep_for(std::chrono::milliseconds(1500));
			//	RS232_flushRXTX(m_OpenComPort);
			//	console.appendf("[*] Port opened, waiting for device...\n");
			//}


			ImGui::EndChild(); // End right column child

			// End Columns, back to single column
			ImGui::Columns(1);


		DrawVHSOverlay();
		ImGui::End(); // End main window
		//DrawVHSOverlay();

		
	}


public:
	static char* m_UserInput; // Obligatory buffer for user input.
	float m_MouseX = 0;
	float m_MouseY = 0;

	bool isNetworkConnected = false;
	bool isComPortConnected = false;
	bool comPortHasInput = false;
	bool comPortHasOutput = false;

private:

	std::vector<int> m_ComPorts; // Dynamic array of available COM ports.
	bool m_AboutModalOpen = false;
	bool m_MouseMode = false; // Toggles Mouse mode.
	bool m_NetworkMode = false; // Switches between serial and network mode.



	std::shared_ptr<Walnut::Image> m_Image; 
	static std::string m_Out; // This is the backgroud buffer that will eventually get written to the forward buffer on screen.
	std::string ErrorMsg; // For telling the user they probably selected the wrong com port. could be a popup? annoying
	int m_SelectedPort = 0;
	unsigned char* buf; // Buffer for incoming data from serial port.
	
	std::ifstream m_SettingsFile;

};

// Shameful Global variables. Unresolved external symbol error happens if I don't have this here. There's probably a better way.
char* ExampleLayer::m_UserInput = new char[128]();
std::string ExampleLayer::m_Out; // Cherno would smite me for this.
//int globalport; // m_SelectedPort isnt accessible when file->close is called. Cherno's gonna hit me over the head with a chair for this



Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{
	Walnut::ApplicationSpecification spec;
	spec.Name = "Serial Comunication Walnut App";
	spec.Height = 600;
	spec.Width = 1200;

	Walnut::Application* app = new Walnut::Application(spec);
	app->PushLayer<ExampleLayer>();
	app->SetMenubarCallback([app]()
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Exit"))
			{
			 app->Close();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit")) {
			

			if (ImGui::MenuItem("Settings")) {
				OpenSettings = true;
			}
			if (ImGui::MenuItem("About")) {
				showPopup = true;
			}

			ImGui::EndMenu();

		}
	});
	return app;
}