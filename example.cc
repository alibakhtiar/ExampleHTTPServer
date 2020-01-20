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
 * @modefied : 20 January 2020
 * @created  : 19 January 2020
 * @author   : Ali Bakhtiar
*/

#include "HttpServer.hh"

using namespace SimpleHttpServer;

/**
 * Main
*/
int main(int argc, char* argv[])
{
	Server server;

	server.ip = "0.0.0.0";
	server.port = 5000;

	if (argc >= 2) {
		server.port = std::atoi(argv[1]);
	}

	server.onRequest([](Request *req, Response *res) {
		res->header("Content-Type", "text/plain");
		res->header("Server", "ExampleHTTPServer");
		res->header("Cache-Control", "no-cache, no-store, must-revalidate");

		if (req->url == "/" || req->url == "/index.cpp") {
			res->body = "Your Request:\n";

			res->body += "method: ";
			res->body += req->method;
			res->body += "\n";

			res->body += "http version: ";
			res->body += std::to_string(req->httpMajorVersion);
			res->body += ".";
			res->body += std::to_string(req->httpMinorVersion);
			res->body += "\n";

			res->body += "url: ";
			res->body += req->url;
			res->body += "\n";

			if (req->queryString != "") {
				res->body += "query string: ";
				res->body += req->queryString;
				res->body += "\n";
			}

			res->body += "headers:\n";
			for (auto const &h : req->headers) {
				res->body += h.first;
				res->body += ": ";
				res->body += h.second;
				res->body += "\n";
			}
		}
		else {
			res->statusCode = 404;
			res->errorPage();
		}

		res->send();

		return true;
	});

	bool rc = createServer(&server);
	if (rc == false)
		return 1;

	return 0;
}
