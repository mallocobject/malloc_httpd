#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>
#include <string>
#include <iostream>
#include <sstream>
#include <thread>
#include <ctime>

#include <WinSock2.h>
#pragma comment(lib, "WS2_32.lib")

SOCKET startup(unsigned short* port)
{
	WSADATA data;
	if (WSAStartup(MAKEWORD(1, 1), &data))
	{
		exit(1);
		printf("%d", __LINE__);
	}
	SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server_socket == -1)
	{
		exit(1);
		printf("%d", __LINE__);
	}

	// 设置端口可复用
	int opt = 1;
	if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) == -1)
	{
		exit(1);
		printf("%d", __LINE__);
	}

	SOCKADDR_IN server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(*port);

	if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) < 0)
	{
		exit(1);
		printf("%d", __LINE__);
	}

	int name_len = sizeof(server_addr);
	if (*port == 0)
	{
		if (getsockname(server_socket, (sockaddr*)&server_addr, &name_len) < 0)
		{
			exit(1);
			printf("%d", __LINE__);
		}
		*port = ntohs(server_addr.sin_port);
	}

	if (listen(server_socket, 5) < 0)
	{
		exit(1);
		printf("%d", __LINE__);
	}

	return server_socket;
}

int get_line(SOCKET socket, char* buff, size_t size)
{
	char c = 0;
	int i = 0;
	while (i < size - 1 && c != '\n')
	{
		int n = recv(socket, &c, 1, 0);
		if (n > 0)
		{
			if (c == '\r')
			{
				n = recv(socket, &c, 1, MSG_PEEK); // 瞄一眼
				if (n > 0 && c == '\n')
				{
					recv(socket, &c, 1, 0);
				}
				else
				{
					c = '\n';
				}
			}
			buff[i++] = c;
		}
		else
		{
			c = '\n';
		}
	}
	buff[i] = '\0';
	return i;
}

void respond(SOCKET client, FILE* file);
void not_found(SOCKET client)
{
	char buff[1024];

	/* 404 页面 */
	sprintf_s(buff, "HTTP/1.0 404 NOT FOUND\r\n");
	send(client, buff, (int)strlen(buff), 0);
	/*服务器信息*/
	sprintf_s(buff, "Server: mallochttpd/0.1\r\n");
	send(client, buff, (int)strlen(buff), 0);
	sprintf_s(buff, "Content-Type: text/html\r\n");
	send(client, buff, (int)strlen(buff), 0);
	sprintf_s(buff, "\r\n");
	send(client, buff, (int)strlen(buff), 0);

	FILE* file = NULL;
	fopen_s(&file, "htdocs/not_found.html", "r");
	if (file == NULL)
	{
		sprintf_s(buff, "<HTML><TITLE>Not Found</TITLE>\r\n");
		send(client, buff, (int)strlen(buff), 0);
		sprintf_s(buff, "<BODY><P>The server could not fulfill\r\n");
		send(client, buff, (int)strlen(buff), 0);
		sprintf_s(buff, "your request because the resource specified\r\n");
		send(client, buff, (int)strlen(buff), 0);
		sprintf_s(buff, "is unavailable or nonexistent.\r\n");
		send(client, buff, (int)strlen(buff), 0);
		sprintf_s(buff, "</BODY></HTML>\r\n");
		send(client, buff, (int)strlen(buff), 0);
	}
	else
	{
		respond(client, file);
		fclose(file);
	}
}

void unimplement(SOCKET socket)
{

}

void headers(SOCKET client, const char* type)
{
	char buff[1024];

	sprintf_s(buff, "HTTP/1.0 200 OK\r\n");
	send(client, buff, (int)strlen(buff), 0);

	sprintf_s(buff, "Server: MallocHttpd/0.1\r\n");
	send(client, buff, (int)strlen(buff), 0);

	sprintf_s(buff, "Content-Type: %s\r\n", type);
	send(client, buff, (int)strlen(buff), 0);

	sprintf_s(buff, "\r\n");
	send(client, buff, (int)strlen(buff), 0);
}

void respond(SOCKET client, FILE* file)
{
	char buff[1024];
	int cnt = 0;

	while (true)
	{
		int ret = (int)fread_s(buff, sizeof(buff), sizeof(char), sizeof(buff) / sizeof(char), file);
		if (ret <= 0)
		{
			break;
		}
		send(client, buff, ret, 0);
		cnt += ret;
	}
	printf("[%d]已发送 %d 字节\n", __LINE__, cnt);
}

const char* get_header_type(const std::string& path)
{
	const char* ret = "text/html; charset=utf-8";
	const char* p = strrchr(path.c_str(), '.');
	if (!p) return ret;
	p++;
	if (!strcmp(p, "css")) ret = "text/css";
	else if (!strcmp(p, "jpg")) ret = "image/jpeg";
	else if (!strcmp(p, "png")) ret = "image/png";
	else if (!strcmp(p, "js")) ret = "application/x-javascript";

	return ret;
}

char* get_header_value(char* header, const char* key) {
	// 查找key
	char* key_pos = strstr(header, key);
	if (key_pos == NULL) { return NULL; }
	// 查找value
	char* value_pos = strchr(key_pos, ':');
	if (value_pos == NULL) {
		return NULL;
	}
	value_pos++;

	// 跳过前面的空格
	while (*value_pos == ' ') {
		value_pos++;
	}

	// 查找结尾的\r\n
	char* end_pos = strstr(value_pos, "\r\n");
	if (end_pos == NULL) {
		return NULL;
	}

	// 截取value
	*end_pos = '\0';

	return value_pos;
}

void server_file(SOCKET client, const std::string& url, const std::string& method, const std::string& body)
{
	std::string path = "htdocs" + url;
	if (method != "GET" && method != "POST")
	{
		unimplement(client);
	}
	else if (method == "GET")
	{
		// 返回请求路径
		if (url.back() == '/')
		{
			path += "index.html";
		}

		printf("[%d]path: %s\n", __LINE__, path.c_str());

		struct stat status;
		if (stat(path.c_str(), &status) == -1)
		{
			not_found(client);
		}
		else
		{
			if ((status.st_mode & S_IFMT) == S_IFDIR)
			{
				path += "/index.html";
			}
		}
	}

	// 使用管道与CGI通信，未完待续......
	else if (method == "POST")
	{
		std::string buff = "HTTP/1.1 200 OK\r\n";
		buff.append("Connection:keep-alive\r\n");
		buff.append("Content-Type:application/x-www-form-urlencoded\r\n");
		buff.append("\r\n");
		/*buff.append("<html><body><strong>Hello World</strong><body><html>");*/
		buff.append(body);
		printf("[%d]%s<-------------------------------->\n", __LINE__, buff.c_str());
		send(client, buff.c_str(), strlen(buff.c_str()), 0);
		return;
	}

	/*打开 sever 的文件*/
	FILE* file = NULL;
	if (strstr(path.c_str(), ".html") != NULL)
	{
		fopen_s(&file, path.c_str(), "r");
	}
	else
	{
		fopen_s(&file, path.c_str(), "rb");
	}
	if (file == NULL)
		not_found(client);
	else
	{
		printf("[%d]正在发送 %s\n", __LINE__, path.c_str());
		/*写 HTTP header */
		headers(client, get_header_type(path));
		/*复制文件*/
		respond(client, file);
		fclose(file);
	}
}

DWORD WINAPI accept_request(LPVOID arg)
{
	SOCKET client = (SOCKET)arg;
	//clock_t start, end;
	//start = clock();

	while (true)
	{
		std::string request;
		char buff[1024];
		int cnt = recv(client, buff, sizeof(buff), 0);
		if (cnt <= 0) break;
		/*if (cnt < 0) break;
		else if (cnt == 0)
		{
			end = clock();
			if (end - start >= 10) break;
			else continue;
		}*/
		/*start = clock();*/
		request.append(buff, cnt);

		// 解析请求方式
		std::string method = "";
		size_t pos = request.find(' ');
		if (pos == std::string::npos) continue;
		method = request.substr(0, pos);
		printf("[%d]method: %s\n", __LINE__, method.c_str());

		// 解析URL
		size_t next_pos = request.find(' ', pos + 1);
		if (next_pos == std::string::npos) continue;
		std::string url = request.substr(pos + 1, next_pos - pos - 1);
		printf("[%d]URL: %s\n", __LINE__, url.c_str());

		// 查找HTTP请求头尾
		pos = request.find("\r\n\r\n");
		if (pos == std::string::npos) continue;

		// 解析HTTP请求体
		std::string body = request.substr(pos + 4);
		printf("[%d]body: %s\n", __LINE__, body.c_str());

		server_file(client, url, method, body);
		break;
	}

	shutdown(client, SD_SEND);
	//Sleep(1e6);

	closesocket(client);
	printf("tcp连接已关闭...\n");
	printf("----------------------\n");
	return 0;
}

// 测试管道通信
// 
//int main(void)
//{
//	char buff[1024];
//	const wchar_t* name = L"\\\\.\\pipe\\test";
//	DWORD num;
//	if (!WaitNamedPipe(name, NMPWAIT_WAIT_FOREVER))
//	{
//		printf("wait name pipe error\n");
//		return 1;
//	}
//	printf("wait name pipe successfully\n");
//	HANDLE hpipe = CreateFile(name, GENERIC_WRITE | GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
//	if (hpipe == INVALID_HANDLE_VALUE)
//	{
//		printf("open name pipe error\n");
//		return 1;
//	}
//	printf("open name pipe and connect successfully\n");
//	int send_num = 0;
//	while (true)
//	{
//		sprintf_s(buff, "sendxxxxxxxx %d", send_num);
//		Sleep(2000);
//		if (!WriteFile(hpipe, buff, sizeof(buff), &num, NULL))
//		{
//			printf("write file error\n");
//			break;
//		}
//		printf("send ok... [%d]\n", send_num++);
//	}
//
//	CloseHandle(hpipe);
//	return 0;
//}

int main(void)
{
	unsigned short port = 80;
	SOCKET server_sock = startup(&port);
	printf("http服务器已经启动， 正在监听 %d 端口...\n", port);

	SOCKADDR_IN client_addr;
	int client_addr_len = sizeof(client_addr);
	while (1)
	{
		SOCKET client_sock = accept(server_sock, (sockaddr*)&client_addr, &client_addr_len);
		if (client_sock == -1)
		{
			exit(1);
			printf("%d", __LINE__);
		}
		DWORD thread_id = 0;
		HANDLE hthread = CreateThread(0, 0, accept_request, (void*)client_sock, 0, &thread_id);
		if (!hthread)
		{
			exit(1);
			printf("%d", __LINE__);
		}
	}
	closesocket(server_sock);

	return 0;
}
