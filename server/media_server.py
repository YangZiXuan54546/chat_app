#!/usr/bin/env python3
"""
HTTP File Server for serving media files (images, videos, etc.)
Runs on port 8889 by default.
"""

import os
import sys
from http.server import HTTPServer, SimpleHTTPRequestHandler
from urllib.parse import unquote
import mimetypes

# 媒体目录 - 按优先级检测
def find_media_dir():
    """查找媒体目录"""
    # 首先检查环境变量
    env_dir = os.environ.get('MEDIA_DIR')
    if env_dir and os.path.exists(env_dir):
        return env_dir
    
    # 获取脚本所在目录
    script_dir = os.path.dirname(os.path.abspath(__file__))
    
    # 可能的媒体目录位置
    possible_dirs = [
        os.path.join(script_dir, 'build', 'media'),  # server/build/media
        os.path.join(script_dir, 'media'),            # server/media
        os.path.join(os.getcwd(), 'media'),           # 当前目录/media
        os.path.join(os.getcwd(), 'build', 'media'),  # 当前目录/build/media
    ]
    
    for d in possible_dirs:
        if os.path.exists(d):
            return d
    
    # 默认创建
    default_dir = os.path.join(script_dir, 'media')
    os.makedirs(default_dir, exist_ok=True)
    return default_dir

MEDIA_DIR = find_media_dir()
PORT = 8889

print(f"[MediaServer] Using media directory: {MEDIA_DIR}")

class MediaHandler(SimpleHTTPRequestHandler):
    """Custom handler for media files with CORS support."""
    
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=MEDIA_DIR, **kwargs)
    
    def end_headers(self):
        # Add CORS headers for cross-origin access
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', '*')
        self.send_header('Cache-Control', 'public, max-age=86400')  # Cache for 1 day
        super().end_headers()
    
    def do_OPTIONS(self):
        """Handle OPTIONS request for CORS preflight."""
        self.send_response(200)
        self.end_headers()
    
    def do_HEAD(self):
        """Handle HEAD request."""
        # Parse path: /media/<file_id>/<filename>
        path = unquote(self.path)
        
        # Remove /media prefix if present
        if path.startswith('/media/'):
            path = path[7:]  # Remove '/media/'
        
        # Split into file_id and filename
        parts = path.split('/', 1)
        if len(parts) == 2:
            file_id, filename = parts
            file_path = self._find_file(filename)
            if file_path:
                self._send_file_headers(file_path)
                return
        
        self.send_error(404, "File not found")
    
    def _send_file_headers(self, file_path):
        """Send headers for a file."""
        mime_type, _ = mimetypes.guess_type(file_path)
        if mime_type is None:
            mime_type = 'application/octet-stream'
        
        file_size = os.path.getsize(file_path)
        
        self.send_response(200)
        self.send_header('Content-Type', mime_type)
        self.send_header('Content-Length', file_size)
        self.end_headers()
    
    def do_GET(self):
        """Handle GET request."""
        # Parse path: /media/<file_id>/<filename>
        path = unquote(self.path)
        
        # Remove /media prefix if present
        if path.startswith('/media/'):
            path = path[7:]  # Remove '/media/'
        
        # Split into file_id and filename
        parts = path.split('/', 1)
        if len(parts) == 2:
            file_id, filename = parts
            # Construct file path
            # The actual files are stored in media/YYYY/MM/DD/user_timestamp.ext
            # For simplicity, we search for the file by filename
            file_path = self._find_file(filename)
            if file_path:
                self._serve_file(file_path)
                return
        
        # If file_id is provided without filename or file not found
        self.send_error(404, "File not found")
    
    def _find_file(self, filename):
        """Search for a file by filename in the media directory."""
        # 首先尝试直接路径
        direct_path = os.path.join(MEDIA_DIR, filename)
        if os.path.exists(direct_path):
            return direct_path
        
        # 然后尝试带日期子目录的路径
        for root, dirs, files in os.walk(MEDIA_DIR):
            if filename in files:
                return os.path.join(root, filename)
        return None
    
    def _serve_file(self, file_path):
        """Serve a file with proper content type."""
        try:
            # Guess content type
            mime_type, _ = mimetypes.guess_type(file_path)
            if mime_type is None:
                mime_type = 'application/octet-stream'
            
            file_size = os.path.getsize(file_path)
            
            self.send_response(200)
            self.send_header('Content-Type', mime_type)
            self.send_header('Content-Length', file_size)
            self.end_headers()
            
            with open(file_path, 'rb') as f:
                while True:
                    data = f.read(8192)
                    if not data:
                        break
                    self.wfile.write(data)
        except Exception as e:
            self.send_error(500, f"Internal server error: {e}")
    
    def log_message(self, format, *args):
        """Custom log format."""
        print(f"[MediaServer] {self.address_string()} - {format % args}")


def main():
    # Create media directory if not exists
    os.makedirs(MEDIA_DIR, exist_ok=True)
    
    server_address = ('', PORT)
    httpd = HTTPServer(server_address, MediaHandler)
    
    print(f"Media file server started on port {PORT}")
    print(f"Serving files from: {os.path.abspath(MEDIA_DIR)}")
    print("Press Ctrl+C to stop")
    
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down media server...")
        httpd.shutdown()


if __name__ == '__main__':
    main()
