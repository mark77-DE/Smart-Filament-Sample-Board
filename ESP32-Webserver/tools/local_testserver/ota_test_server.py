from http.server import HTTPServer, SimpleHTTPRequestHandler

class KeepAliveHandler(SimpleHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

if __name__ == "__main__":
    server = HTTPServer(("0.0.0.0", 8000), KeepAliveHandler)
    server.serve_forever()
    