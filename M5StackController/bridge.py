#!/usr/bin/env python3
import argparse, http.client, urllib.parse
from http.server import BaseHTTPRequestHandler, HTTPServer

class BridgeHandler(BaseHTTPRequestHandler):
    DOLPHIN_HOST = "localhost"
    DOLPHIN_PORT = 8394

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path != "/swap":
            self.send_error(404, "Not found")
            return
        qs = urllib.parse.parse_qs(parsed.query)
        if "path" not in qs:
            self.send_error(400, "Missing 'path'")
            return
        game_path = urllib.parse.unquote(qs["path"][0])
        # forward to Dolphin
        enc = urllib.parse.quote(game_path)
        conn = http.client.HTTPConnection(self.DOLPHIN_HOST, self.DOLPHIN_PORT, timeout=5)
        try:
            conn.request("GET", f"/swap?path={enc}")
            resp = conn.getresponse()
            body = resp.read()
            self.send_response(resp.status)
            self.end_headers()
            self.wfile.write(body)
        except Exception as e:
            self.send_error(500, str(e))
        finally:
            conn.close()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="ESP32→Dolphin HTTP bridge")
    parser.add_argument("--host",  default="0.0.0.0")
    parser.add_argument("--port",  type=int, default=8000)
    parser.add_argument("--dhost", default="localhost")
    parser.add_argument("--dport", type=int, default=8394)
    args = parser.parse_args()

    BridgeHandler.DOLPHIN_HOST = args.dhost
    BridgeHandler.DOLPHIN_PORT = args.dport

    server = HTTPServer((args.host, args.port), BridgeHandler)
    print(f"Listening on {args.host}:{args.port}, forwarding to {args.dhost}:{args.dport}")
    server.serve_forever()
