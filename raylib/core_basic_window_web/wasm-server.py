# wasm-server.py
import sys

if sys.version_info.major == 2:
    import SimpleHTTPServer
    import SocketServer

    class WasmHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
        def end_headers(self):        
            # Include additional response headers here. CORS for example:
            #self.send_header('Access-Control-Allow-Origin', '*')
            SimpleHTTPServer.SimpleHTTPRequestHandler.end_headers(self)
elif sys.version_info.major >= 3:
    import sys
    import socketserver
    from http.server import SimpleHTTPRequestHandler

    class WasmHandler(SimpleHTTPRequestHandler):
        def end_headers(self):        
            # Include additional response headers here. CORS for example:
            #self.send_header('Access-Control-Allow-Origin', '*')
            SimpleHTTPRequestHandler.end_headers(self)

# Python 3.7.5 adds in the WebAssembly Media Type. Version 2.x doesn't
# have this so add it in.
if sys.version_info < (3, 7, 5):
    WasmHandler.extensions_map['.wasm'] = 'application/wasm'

if __name__ == '__main__':
    PORT = 8080
    if sys.version_info.major == 2:
        httpd = SocketServer.TCPServer(("", PORT), WasmHandler)
        print("Listening on port {}. Press Ctrl+C to stop.".format(PORT))
        httpd.serve_forever()
    elif sys.version_info.major >= 3:
        with socketserver.TCPServer(("", PORT), WasmHandler) as httpd:
            print("Listening on port {}. Press Ctrl+C to stop.".format(PORT))
            httpd.serve_forever()
