#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"

#include "Walnut/Image.h"
#include <thread>
#include "rs232.h" // Cross platform wrapper for USB Serial Com. https://github.com/Marzac/rs232

#include <stdio.h>
#pragma comment(lib, "Ws2_32.lib")

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

// Global variables, I know, yucky.
std::queue<std::string> messageQueue; // Shared buffer for messages
std::mutex queueMutex; // Mutex to protect the shared buffer
std::condition_variable queueCV; // Condition variable for signaling
int globalport; // m_SelectedPort isnt accessible when file->close is called. Cherno's gonna hit me over the head with a chair for this
bool stopThreads = false; // Flag to signal threads to stop
std::string Out; // dump buffer received from server to user in textbox.

DWORD WINAPI receiveThread(LPVOID lpParam) {
	SOCKET clientSocket = reinterpret_cast<SOCKET>(lpParam); // Cast the LPVOID back to SOCKET
	char buffer[1024];
	int bytesReceived;
	while (true) {
		// Receive data from the server
		bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
		if (bytesReceived > 0) {
			buffer[bytesReceived] = '\0';
			Out.append("\n [+] " +  std::string(buffer));
			// Lock the mutex before accessing the shared buffer
			std::lock_guard<std::mutex> lock(queueMutex);
			// Place the received message into the buffer
			messageQueue.push(std::string(buffer));
			// Notify the main thread that new data is available
			queueCV.notify_one();
		}
		else {
			// Handle error or connection closed
			break;
		}
	}
}

// Function to handle sending data to the server
//void sendThread(SOCKET clientSocket) {
DWORD WINAPI sendThread(LPVOID lpParam) {
	SOCKET clientSocket = (SOCKET)lpParam;
	while (!stopThreads) {
		// Wait for data to be available in the buffer
		std::unique_lock<std::mutex> lock(queueMutex);
		queueCV.wait(lock, [] { return !messageQueue.empty(); });
		// Get the message from the buffer
		std::string message = messageQueue.front();
		messageQueue.pop();
		lock.unlock(); // Unlock the mutex before sending data
		// Send the message to the server
		send(clientSocket, message.c_str(), message.size(), 0);
		printf("Sent: %s\n", message.c_str());
	}
}


class ExampleLayer : public Walnut::Layer
{
public:
	// Needed to handle yucky necessary char buffers.
	ExampleLayer() {
		buf = new unsigned char[128];
		m_UserInput = new char[128];
	}
	~ExampleLayer() {
		delete[] buf;
		delete[] m_UserInput;
		stopThreads = true;

		// Notify the sending thread to wake up and check the stopThreads flag
		queueCV.notify_one();

		// Join the threads
		//hReadThread.join();
		//hSendThread.join();

		WSACleanup();
	}

	virtual void OnAttach() override {
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
			printf( "WSAStartup failed\n");
			//return 1;
		}
		
		std::string ipAddress = "192.168.0.222";
		int port = 5000;
		ConnectSocket = socket(AF_INET, SOCK_STREAM, 0);
		clientService.sin_family = AF_INET;
		clientService.sin_addr.s_addr = inet_addr(ipAddress.c_str());
		clientService.sin_port = htons(port);
		if (connect(ConnectSocket, (SOCKADDR*)&clientService, sizeof(clientService)) == SOCKET_ERROR) {
			
			int error_code = WSAGetLastError();
			printf("Unable to connect to server.%d\n", error_code);
			WSACleanup();
		}
		send (ConnectSocket, "W", 1, 0);
		/*
		std::thread recvThread(receiveThread, clientSocket);
		std::thread sendThread(sendThread, clientSocket);
		*/
		HANDLE hReadThread = CreateThread(NULL, 0, receiveThread, (LPVOID)ConnectSocket, 0, NULL);
		HANDLE hSendThread = CreateThread(NULL, 0, sendThread, (LPVOID)ConnectSocket, 0, NULL);

		m_Image = std::make_shared<Walnut::Image>("Scuzzy.png");

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

	virtual void OnUIRender() override
	{

		ImGui::Begin("Serial Comunication");
		/*
		if (ImGui::Button("BALLS")) {
			m_ShowImage = !m_ShowImage;
		}*/
		ImGui::SameLine();
		if (ImGui::Button("Toggle Mouse Mode")) {
			m_Mousemode = !m_Mousemode;
		}
		ImGui::SameLine();
		ImGui::PushItemWidth(200);

		if (ImGui::Button("Toggle Serial/Network")) {
			m_NetworkMode = !m_NetworkMode;
			if (m_NetworkMode) {
				int error_code;
				int error_code_size = sizeof(error_code);
				getsockopt(ConnectSocket, SOL_SOCKET, SO_ERROR, (char*)&error_code, &error_code_size);
				if (error_code == SOCKET_ERROR) {
					printf("fuckywucky %d\n", error_code);
				}
			}
		}

		ImGui::SameLine();
		ImGui::PushItemWidth(200);

		
		bool isDropdownOpen = false;
		if (!m_NetworkMode) {
			if (ImGui::BeginCombo("##combo", "Select a Com Port")) {
				int size = m_ComPorts.size();
				for (int i = 0; i < size; i++) {
					bool isSelected = (m_SelectedPort == i);

					//m_ComPorts.at(i).c_str();
					//  "expression must have class type but it has type "int""
					// is the stupidest error ive ever seen. Forced to convert it three times.
					int tmp = m_ComPorts.at(i); // stupid
					//tmp.c_str(); // r u buttering my pancakes rn
					std::string item = std::to_string(tmp); // just convert a damn int from a vector to a char AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAH

					if (ImGui::Selectable(item.c_str() /*im going to commit crimes*/, isSelected)) {
						m_SelectedPort = i;
						globalport = i;
						// Attempt to connect to Selected Serial Port.
						char mode[] = { '8','N','1',0 }; // Serial port magic dont touch it 

						if (RS232_OpenComport(m_ComPorts.at(m_SelectedPort) - 1, 9600, mode, 0))
						{
							//printf("Can not open comport COM%i\n", m_ComPorts.at(m_SelectedPort));
							ErrorMsg = "Can not open comport COM" + std::to_string(m_ComPorts.at(m_SelectedPort));
						}
					}
					if (isSelected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
		}
		//else {
		//	ImGui::NewLine();
		//	// TODO: Two text boxes for IP and Port.
		//	static char buf1[128];
		//	ImGui::InputText("ip", buf1, 128);
		//	// Add a new line to move to the next line
		//	ImGui::SameLine();

		//	// Set the cursor position on the X-axis
		//	//ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 300);
		//	// Add second text box
		//	static char buf2[128];
		//	ImGui::InputText("port", buf2, 128);
		//}


		// Check if the dropdown menu is clicked to open
		if (ImGui::IsItemClicked()) {
			isDropdownOpen = true;

			// Clear the vector of detected ports and scan for more.
			m_ComPorts.clear();
			// Getting list of COM ports super easy
			// https://stackoverflow.com/a/60950058/9274593
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
					printf("ERROR: %d\n", ::GetLastError());
					ErrorMsg = "ERROR: " + std::to_string(::GetLastError());
				}

			}
		}
		else {
			isDropdownOpen = false;
		}
		// Stop messing with size of ImGUI objects.
		ImGui::PopItemWidth();
		//ImGui::SameLine();
		//ImGui::NewLine();
		ImGui::Text(ErrorMsg.c_str());


		// Toggles between Sending Mouse Coordinates or Text.
		if (m_Mousemode) {

			ImGui::Text("Hold Left Click over the image to Send coords!");
			ImVec2 imagePos = ImGui::GetCursorScreenPos();
			ImGui::Image(m_Image->GetDescriptorSet(), { 400, 400 });


			ImVec2 pos = ImGui::GetMousePos();
			// Position of mouse cursor relative to image, then scaled by 180 degrees of rotation for the servo motor.
			ImVec2 relativePos = ImVec2((pos.x - imagePos.x) / (m_Image->GetHeight() / 180), (pos.y - imagePos.y) / (m_Image->GetWidth() / 180));
			ImGui::Text("Coords are sent over serial as: (X%uY%u)", (int)relativePos.x, (int)relativePos.y);


			// Basically, you hold left click while cursor is over the image.
			if (ImGui::IsMouseDragging(0)) {
				//printf("Holding mouse button down.\n");
				// Michael's arduino parses this to control two servo motors for Pan And Yaw. Yee Haw.
				char coords[10];
				std::string temp;
				temp += "X";
				temp += std::to_string((int)relativePos.x);
				temp += "Y";
				temp += std::to_string((int)relativePos.y);
				temp += "\n";

				//RS232_SendBuf(2, reinterpret_cast<unsigned char*>(temp.c_str()), temp.size());
				//unsigned char* charArray = reinterpret_cast<const unsigned char*>(temp.c_str());
				// Dont take strings for granted.
				unsigned char* charArray = new unsigned char[temp.size()];
				std::memcpy(charArray, temp.c_str(), temp.size());
				// Send Mouse Position over Serial.
				if (!m_NetworkMode) {
					RS232_SendBuf(m_ComPorts.at(m_SelectedPort) - 1, charArray, temp.size());
				}
				else {
					// TODO: Send Mouse Position over Network.
					//send(ConnectSocket, temp.c_str(), temp.size(), 0);
					std::lock_guard<std::mutex> lock(queueMutex);
					messageQueue.push(temp.c_str());
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(15));
			}

		}
		else {// Send Text to Serial.

			// Read from COM port.
			if (!m_NetworkMode) {
				int n = RS232_PollComport(m_ComPorts.at(m_SelectedPort), buf, 128);

				if (n > 0)
				{
					buf[n] = 0; // Null terminate.
					// convert the buffer to a string. char buffers make me sad theres gotta be a better way to send text.
					for (int i = 0; i < n; i++) {
						if (buf[i] == ' ') {
							m_Out += buf[i];
						}
						// spaces are weird.
						if (buf[i] >= 32)
							m_Out += buf[i];
					}
					m_Out += "\n";

					printf("received %i bytes: %s\n", n, (char*)buf);
				}
			}
			else {

				// Maybe the recv thread should copy the buffer it recieves into the m_Out string.

				// TODO: Read a buffer from the server.
				//char* readbuf = new char[128];
				//recv(ConnectSocket, readbuf, 128, 0);
				// 

				//std::unique_lock<std::mutex> lock(queueMutex); // Make sure no thread is overwriting what i want to read
				////queueCV.wait(lock, [] { return !messageQueue.empty() || stopThreads; }); // Wait for data to be available in the buffer
				//std::string message;
				//	// Check if new data is available
				//if (!messageQueue.empty()) {
				//	// Get the message from the buffer
				//	message = messageQueue.front();
				//	messageQueue.pop();
				//	queueMutex.unlock(); // Unlock the mutex after retrieving data
				//}
				m_Out = Out; // set the buffer to a global var :(

			}
			
			ImGui::Text("Serial Comunication.\nInput:");
			ImGui::InputText("##Text", m_UserInput, IM_ARRAYSIZE(m_UserInput));
			ImGui::SameLine;
			if (ImGui::Button("Send")) {
				int i;
				for (i = 0; i < 128; i++) {
					if (m_UserInput[i] == '\n') {
						break;
					}
				}
				if (!m_NetworkMode) {
					// Write to COM port.
					RS232_SendBuf(m_ComPorts.at(m_SelectedPort), reinterpret_cast<unsigned char*>(m_UserInput), i);
				}
				else {
					// TODO: Send Text to Server.
					//send(ConnectSocket, m_UserInput, i, 0);
					std::lock_guard<std::mutex> lock(queueMutex);
					messageQueue.push(m_UserInput);

				}
				memset(m_UserInput, 0, sizeof(m_UserInput)); // clear the user input buffer after sending.
			}
			ImGui::BeginChild("##Text Box", ImVec2(400, 300), true, ImGuiWindowFlags_NoScrollbar);
			ImGui::Text(m_Out.c_str());
			ImGui::EndChild();
			std::memset(buf, 0, sizeof(buf)); // clear the buffer.
			/*
			for (int i = 0; i < 128; i++) {
				if ([i] == 0) {
					break;
				}
				ImGui::Text("%c", str0[i]);	
				ImGui::SameLine;
			}
			*/


		}

		ImGui::End();

		//UI_DrawAboutModal();
	}

	/*
	SOCKET ConnectToServer(std::string ipAddress, int port) {
		SOCKET ConnectSocket = socket(AF_INET, SOCK_STREAM, 0);
		sockaddr_in clientService;
		clientService.sin_family = AF_INET;
		clientService.sin_addr.s_addr = inet_addr(ipAddress.c_str());
		clientService.sin_port = htons(port);
		connect(ConnectSocket, (SOCKADDR*)&clientService, sizeof(clientService));
		return ConnectSocket;
	}
	*/

private:
	SOCKET ConnectSocket;
	sockaddr_in clientService;

	std::vector<int> m_ComPorts; // Dynamic array of available COM ports.
	bool m_AboutModalOpen = false;
	bool m_Mousemode = false; // Toggles Mouse mode.
	bool m_NetworkMode = false; // Switches between serial and network mode.
	std::shared_ptr<Walnut::Image> m_Image; // When playing with Walnut, i had an AI gen'd image of Giygas from Earthbound. This is it.
	static std::string m_Out; // Command to send to Arduino / Pico W (for the laser turret by Michael Reeves)
	std::string ErrorMsg; // For telling the user they probably selected the wrong com port.
	int m_SelectedPort = 0;
	unsigned char* buf; // Buffer for incoming data from serial port.
	static char* m_UserInput; // Obligatory buffer for user input. Yucky.
};

// Shameful Global variables. Unresolved external symbol error happens if I don't have this here. There's probably a better way.
char* ExampleLayer::m_UserInput = nullptr;
std::string ExampleLayer::m_Out; // Cherno would smite me for this.
//int globalport; // m_SelectedPort isnt accessible when file->close is called. Cherno's gonna hit me over the head with a chair for this


Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{
	Walnut::ApplicationSpecification spec;
	spec.Name = "Walnut Example";

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
			if (ImGui::MenuItem("test")) {

			}
			ImGui::EndMenu();
		}
	});
	return app;
}