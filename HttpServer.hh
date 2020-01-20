/**
 * Example HTTP Server
 *
 * Copyright (C) 2020 Ali Bakhtiar
 *
 * The MIT License
 *
 * https://twitter.com/ali_bakhtiar
 * https://github.com/alibakhtiar
*/

/**
 * Compile:
 * g++ example.cc HttpServer.hh -std=c++11 -Wall -pthread -O3 -o example.out
 * ./example.out <port>
 *
 * Resources:
 * https://linux.die.net/man/7/socket
 * http://man7.org/linux/man-pages/man7/socket.7.html
 *
 * @modefied : 20 January 2020
 * @created  : 19 January 2020
 * @author   : Ali Bakhtiar
*/

#ifndef EXAMPLEHTTPSERVER_H
#define EXAMPLEHTTPSERVER_H

#include <iostream> // std::string, std::cout
#include <map> // std::map
#include <thread> // std::thread
#include <functional> // std::function
#include <unistd.h> // close
#include <string.h> // memset, strerror
#include <netinet/tcp.h> // TCP_NODELAY
#include <arpa/inet.h> // struct in_addr, htons

#define _FUNC    __PRETTY_FUNCTION__
#define _FILE    __FILE__
#define _LINE    __LINE__
#define _STRERR  strerror(errno)

#define ERROR(message, func, line)\
fprintf(stderr, "%s,  function %s, line %d\n", message, func, line);

#define BACKLOG_SIZE     150
#define RECV_BUFFER_SIZE 2048

#define PARSER_STATE_METHOD    1 // request method (GET, POST...)
#define PARSER_STATE_URL       2 // url path
#define PARSER_STATE_URL_QS    3 // QueryString
#define PARSER_STATE_PROTOCOL  4 // http major version
#define PARSER_STATE_MAJOR     5 // http major version
#define PARSER_STATE_MINOR     6 // http minor version
#define PARSER_STATE_HEADERS   7 // key: value

namespace SimpleHttpServer {

const char *httpStatusMessage(int httpStatusCode);

static ssize_t sendBytes(int sock, const char *buffer, size_t length);

void strtolower(std::string &str);

/********************************************************************************/

/**
 * Request
*/
class Request
{
	public:

	int sock; // Socket
	struct sockaddr_in addr; // Socket Address (IP)

	bool hasError = false;

	// HTTP Params
	std::string method      = "";
	std::string url         = "";
	std::string queryString = "";
	int httpMajorVersion    = 0;
	int httpMinorVersion    = 0;
	std::map<std::string, std::string> headers;

	/**
	 * Destructor
	*/
	virtual ~Request(void)
	{
		this->method.clear();
		this->url.clear();
		this->queryString.clear();
		this->headers.clear();
	}
};

/**
 * Response
*/
class Response
{
	public:

	int sock; // Socket
	Request *req; // for http headers

	int statusCode = 200;
	std::map<std::string, std::string> headers;
	std::string body = "";

	/**
	 * Set http header
	*/
	virtual void header(std::string key, std::string value)
	{
		this->headers[key] = value;
	}

	/**
	 * HTML error page
	*/
	virtual void errorPage(void)
	{
		this->header("Content-Type", "text/html");
		this->header("Cache-Control", "no-cache, no-store, must-revalidate");

		this->body = "<!doctype html><html lang=\"en\">";
		this->body += "<head><title>Error</title></head>";
		this->body += "<body><h1>Error ";
		this->body += std::to_string(this->statusCode);
		this->body += "</h1><hr><p>";
		this->body += httpStatusMessage(this->statusCode);
		this->body += "</p></body></html>";

		return;
	}

	/**
	 * Send all (header and body)
	*/
	virtual bool send(void)
	{
		ssize_t sendLen;
		std::string head;

		this->headers["Content-Length"] = std::to_string(this->body.length());
		this->headers["Conection"] = "close";

		head = "HTTP/1.";
		head += this->req->httpMinorVersion == 0 ? '0' : '1';
		head += " ";
		head += std::to_string(this->statusCode);
		head += " ";
		head += httpStatusMessage(this->statusCode);
		head += "\r\n";

		for (auto const &h : this->headers) {
			head += h.first;
			head += ": ";
			head += h.second;
			head += "\r\n";
		}

		head += "\r\n";

		sendLen = this->write(head.c_str(), head.length());
		if (sendLen < 0) {
			head.clear();
			ERROR(_STRERR, _FUNC, _LINE);
			return false;
		}

		sendLen = this->write(this->body.c_str(), this->body.length());
		if (sendLen < 0) {
			head.clear();
			ERROR(_STRERR, _FUNC, _LINE);
			return false;
		}

		head.clear();
		return true;
	}

	/**
	 * Write to socket
	*/
	virtual ssize_t write(const char *buffer, size_t length)
	{
		return sendBytes(this->sock, buffer, length);
	}

	/**
	 * Destructor
	*/
	virtual ~Response(void)
	{
		this->body.clear();
		this->headers.clear();
	}
};

/**
 * Server
*/
class Server
{
	// Request callback
	typedef std::function<bool(Request *req, Response *res)> callback_t;

	public:

	std::string ip = "0.0.0.0";
	int port       = 5000;

	/**
	 * On request callback
	*/
	virtual void onRequest(callback_t cb)
	{
		this->onRequestCallback = cb;
	}

	// callback function (private)
	callback_t onRequestCallback = NULL;
};

/********************************************************************************/

/**
 * HTTP request parser
*/
bool httpRequestParse(Request *req, const char *buffer, size_t length)
{
	std::string tmp = "";
	int state = PARSER_STATE_METHOD;
	bool isKey = true;

	size_t i;
	for (i=0; i<length; ++i)
	{
		switch (state) {
			case PARSER_STATE_METHOD:
				if (buffer[i] == ' ') {
					req->method = tmp;
					state = PARSER_STATE_URL;
					tmp = "";
					break;
				}
				tmp += buffer[i];
			break;

			case PARSER_STATE_URL:
				if (buffer[i] == ' ' || buffer[i] == '?') {
					req->url = tmp;
					state = buffer[i] == '?' ? PARSER_STATE_URL_QS : PARSER_STATE_PROTOCOL;
					tmp = "";
					continue;
				}

				// Simple XSS filter
				// ascii 34 == "
				// ascii 39 == '
				if (buffer[i] == '<' || buffer[i] == '>' || buffer[i] == 34 || buffer[i] == 39)
					continue;

				tmp += buffer[i];
			break;

			case PARSER_STATE_URL_QS:
				if (buffer[i] == ' ') {
					req->queryString = tmp;
					state = PARSER_STATE_PROTOCOL;
					tmp = "";
					continue;
				}

				// Simple XSS filter
				// ascii 34 == "
				// ascii 39 == '
				if (buffer[i] == '<' || buffer[i] == '>' || buffer[i] == 34 || buffer[i] == 39)
					continue;

				tmp += buffer[i];
			break;

			case PARSER_STATE_PROTOCOL:
				if (buffer[i] == '/') {
					if (tmp != "HTTP")
						req->hasError = true;

					state = PARSER_STATE_MAJOR;
					tmp = "";
					continue;
				}
				else {
					tmp += buffer[i];
				}
			break;

			case PARSER_STATE_MAJOR:
				if (isdigit(buffer[i]) != 0) {
					req->httpMajorVersion = (int)buffer[i] - '0';
				}
				else if (buffer[i] == '.') {
					state = PARSER_STATE_MINOR;
				}
			break;

			case PARSER_STATE_MINOR:
				if (isdigit(buffer[i]) != 0) {
					req->httpMinorVersion = (int)buffer[i] - '0';
				}

				if (buffer[i] == '\n')
					state = PARSER_STATE_HEADERS;
			break;

			case PARSER_STATE_HEADERS:
				if (buffer[i] == '\n') {
					isKey = true;
					tmp = "";
				}
				else if (isKey == true && buffer[i] == ':') {
					strtolower(tmp);
					req->headers[tmp] = "";
					isKey = false;
				}
				else if (isKey == true) {
					tmp += buffer[i];
				}
				else if (isKey == false) {
					if (buffer[i] == '\r' || (req->headers[tmp].size() == 0 && buffer[i] == ' '))
						continue;

					req->headers[tmp] += buffer[i];
				}
			break;
		}
	}

	tmp.clear();

	if (req->httpMajorVersion != 1) {
		req->hasError = true;
	}

	return true;
}

/********************************************************************************/

/**
 * Send bytes to client
*/
static ssize_t sendBytes(int sock, const char *buffer, size_t length)
{
	ssize_t n = 0;
	const char *p = buffer;

	while (length > 0) {
		n = send(sock, p, length, 0);

		if (n < 1)
			break;

		p += n;
		length -= n;
	}

	return n;
}


/**
 * Server request handler
*/
static void serverRequestHandler(Server *server, int sock, struct sockaddr_in clientAddr)
{
	bool rc;
	Request *req;
	Response *res;
	char *buffer;
	ssize_t recvLen;

	// New request
	req = new Request;
	req->sock = sock;
	req->addr = clientAddr;

	// New response
	res = new Response;
	res->sock = sock;
	res->req  = req;

	// Recv buffer
	buffer = new char[1+RECV_BUFFER_SIZE];

	while (1) {
		memset(buffer, '\0', 1+RECV_BUFFER_SIZE);

		recvLen = recv(sock, buffer, RECV_BUFFER_SIZE, 0);
		if (recvLen == 0) {
			break;
		}
		else if (recvLen < 0) {
			ERROR(_STRERR, _FUNC, _LINE);
			break;
		}

		// Request parser
		rc = httpRequestParse(req, buffer, recvLen);
		if (rc == false)
			break;

		if (req->hasError == true) {
			res->statusCode = 400;
			res->errorPage();
			res->send();
			break;
		}

		if (server->onRequestCallback != NULL)
			server->onRequestCallback(req, res);

		break;
	}

	close(sock);

	delete[] buffer;
	buffer = NULL;

	delete req;
	req = NULL;

	delete res;
	res = NULL;

	return;
}


/**
 * Create server
*/
bool createServer(Server *server)
{
	int st;
	int serverFd;
	int optVal = 1;

	// Create new TCP socket(int family, int type, int protocol)
	serverFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (serverFd < 0) {
		ERROR(_STRERR, _FUNC, _LINE);
		return false;
	}

	// Socket options
	// nodelay
	st = setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, (void *)&optVal, sizeof(int));
	if (st == -1) {
		ERROR(_STRERR, _FUNC, _LINE);
		return false;
	}

	// nodelay
	st = setsockopt(serverFd, IPPROTO_TCP, TCP_NODELAY, (void *)&optVal, sizeof(int));
	if (st == -1) {
		ERROR(_STRERR, _FUNC, _LINE);
		return false;
	}

	// Bind
	struct sockaddr_in sockAddr;
	memset(&sockAddr, 0, sizeof(struct sockaddr_in));

	sockAddr.sin_family = AF_INET;
	sockAddr.sin_addr.s_addr = inet_addr((char *)server->ip.c_str());
	sockAddr.sin_port = htons(server->port);

	st = bind(serverFd, (struct sockaddr *)&sockAddr, sizeof(struct sockaddr_in));
	if (st == -1) {
		ERROR(_STRERR, _FUNC, _LINE);
		return false;
	}

	// Listen
	st = listen(serverFd, BACKLOG_SIZE);
	if (st < 0) {
		ERROR(_STRERR, _FUNC, _LINE);
		return false;
	}

	// Run (main loop)
	int clientFd;
	struct sockaddr_in clientAddr;
	socklen_t clientLength;

	clientLength = (socklen_t)sizeof(struct sockaddr_in);
	memset(&clientAddr, 0, sizeof(struct sockaddr_in));

	std::cout << "http://" << 
		server->ip << ":" << server->port << std::endl;

	while (1) {
		// Accept new connection (new socket)
		clientFd = accept(serverFd, (struct sockaddr *)&clientAddr, &clientLength);
		if (clientFd <= 0) {
			continue;
		}

		// New thread per connection
		std::thread(serverRequestHandler, server, std::move(clientFd), std::move(clientAddr)).detach();
	}

	close(serverFd);

	return true;
}

/**
 * Http status message
*/
const char *httpStatusMessage(int httpStatusCode)
{
	int i;
	static const struct {
		int httpStatusCode;
		const char *message;
	} msg[] = {
		{200, "Ok"},
		{301, "Moved Permanently"},
		{302, "Found"},
		{304, "Not Modified"},
		{307, "Temporary Redirect"},
		{400, "Bad Request"},
		{401, "Unauthorized"},
		{403, "Forbidden"},
		{404, "Not Found"},
		{405, "Method Not Allowed"},
		{411, "Length Required"},
		{413, "Request Entity Too Large"},
		{414, "Request-URI Too Long"},
		{429, "Too Many Requests"},
		{500, "Internal Server Error"},
		{502, "Bad Gateway"},
		{503, "Service Unavailable"},
		{504, "Gateway Timeout"},
		{505, "HTTP Version Not Supported"},
		{0, NULL}
	};

	for (i=0; msg[i].httpStatusCode != 0; ++i) {
		if (msg[i].httpStatusCode == httpStatusCode)
			return msg[i].message;
	}

	return NULL;
}


/**
 * Convert a string to lowercase
*/
void strtolower(std::string &str)
{
	for (std::string::size_type i=0; i < str.size(); ++i) {
		if ((str[i] >= 'A') && (str[i] <= 'Z'))
			str[i] += 32;
	}

	return;
}

} // namespace SimpleHttpServer

#endif // EXAMPLEHTTPSERVER_H
