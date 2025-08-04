#!/usr/bin/env python3
"""
Flask-based HTTP Audio Streaming Server for MP3 Rewind Project

This Flask server provides the same API as the original BaseHTTPServer but with:
1. Proper HTTP connection handling for embedded clients
2. Reliable response delivery 
3. Chunked HTTP audio streaming for WAV files
4. RESTful API for playback control
5. Better timing characteristics for Zephyr HTTP client

Usage:
    python3 flask_server.py [--host HOST] [--port PORT] [--audio-dir AUDIO_DIR]

Example:
    python3 flask_server.py --host 0.0.0.0 --port 8000 --audio-dir ../test_data


The server immplementation was created using a combination of online resources 
and Claude Sonnet 4. Frankly, this is my first time working with both Flask and
HTTP servers in Python. This version is v1.0.
"""

import os
import sys
import json
import wave
import time
import struct
import argparse
import threading
from pathlib import Path
from flask import Flask, jsonify, request, Response, render_template_string
from urllib.parse import parse_qs
from typing import Optional, Dict, Any

class AudioStreamServer:
    """Manages audio streaming state and control"""
    
    def __init__(self, audio_dir: str):
        self.audio_dir = Path(audio_dir)
        self.current_track: Optional[str] = None
        self.is_playing = False
        self.is_paused = False
        self.volume = 100  # 0-100
        self.position = 0  # Current position in bytes
        self.chunk_size = 1024  # Bytes per chunk for streaming
        self.lock = threading.Lock()
        
        # Discover available audio files
        self.available_tracks = self._discover_audio_files()
        print(f"Found {len(self.available_tracks)} audio files:")
        for track in self.available_tracks:
            print(f"  - {track}")
    
    def _discover_audio_files(self):
        """Find all supported audio files in the audio directory"""
        supported_extensions = {'.wav', '.mp3', '.flac'}
        audio_files = []
        
        if not self.audio_dir.exists():
            print(f"Warning: Audio directory '{self.audio_dir}' does not exist")
            return audio_files
        
        for file_path in self.audio_dir.rglob('*'):
            if file_path.is_file() and file_path.suffix.lower() in supported_extensions:
                audio_files.append(file_path.name)
        
        return sorted(audio_files)
    
    def get_status(self) -> Dict[str, Any]:
        """Get current server status"""
        with self.lock:
            return {
                "status": "playing" if self.is_playing else "paused" if self.is_paused else "stopped",
                "track": self.current_track,
                "volume": self.volume,
                "position": self.position,
                "available_tracks": self.available_tracks
            }
    
    def play(self, track: Optional[str] = None) -> Dict[str, Any]:
        """Start playing audio"""
        with self.lock:
            if track and track in self.available_tracks:
                self.current_track = track
                self.position = 0
            
            if not self.current_track:
                if self.available_tracks:
                    self.current_track = self.available_tracks[0]
                else:
                    return {"status": "error", "message": "No tracks available"}
            
            self.is_playing = True
            self.is_paused = False
            
            return {
                "status": "playing", 
                "track": self.current_track,
                "message": f"Started playing {self.current_track}"
            }
    
    def pause(self) -> Dict[str, Any]:
        """Pause audio playback"""
        with self.lock:
            if self.is_playing:
                self.is_playing = False
                self.is_paused = True
                return {"status": "paused", "message": "Playback paused"}
            else:
                return {"status": "error", "message": "Not currently playing"}
    
    def stop(self) -> Dict[str, Any]:
        """Stop audio playback"""
        with self.lock:
            self.is_playing = False
            self.is_paused = False
            self.position = 0
            return {"status": "stopped", "message": "Playback stopped"}
    
    def set_volume(self, volume: int) -> Dict[str, Any]:
        """Set playback volume"""
        with self.lock:
            if 0 <= volume <= 100:
                self.volume = volume
                return {"status": "ok", "volume": self.volume, "message": f"Volume set to {volume}%"}
            else:
                return {"status": "error", "message": "Volume must be between 0 and 100"}
    
    def get_track_path(self, track_name: str) -> Optional[Path]:
        """Get full path for a track"""
        if track_name in self.available_tracks:
            return self.audio_dir / track_name
        return None

# Global audio server instance
audio_server = None

# Create Flask app
app = Flask(__name__)

# Configure Flask for embedded clients
app.config['JSON_SORT_KEYS'] = False

@app.route('/')
def index():
    """Serve basic web interface"""
    html_template = '''
    <!DOCTYPE html>
    <html>
    <head>
        <title>MP3 Rewind Audio Server</title>
        <style>
            body { font-family: Arial, sans-serif; margin: 40px; }
            .status { background: #f0f8ff; padding: 20px; border-radius: 5px; margin: 20px 0; }
            .endpoint { background: #f5f5f5; padding: 10px; margin: 10px 0; border-radius: 3px; }
            .method { font-weight: bold; color: #0066cc; }
        </style>
    </head>
    <body>
        <h1>MP3 Rewind Audio Server</h1>
        <div class="status">
            <h2>Server Status: Running</h2>
            <p>Flask-based HTTP server optimized for embedded clients</p>
            <p>Available tracks: {{ tracks|length }}</p>
            {% for track in tracks %}
            <li>{{ track }}</li>
            {% endfor %}
        </div>
        
        <h2>API Endpoints</h2>
        <div class="endpoint">
            <div class="method">GET /api/status</div>
            <p>Get current playback status and track information</p>
        </div>
        <div class="endpoint">
            <div class="method">POST /api/play</div>
            <p>Start playback. Optional JSON body: {"track": "filename.wav"}</p>
        </div>
        <div class="endpoint">
            <div class="method">POST /api/stop</div>
            <p>Stop playback</p>
        </div>
        <div class="endpoint">
            <div class="method">POST /api/volume</div>
            <p>Set volume. JSON body: {"volume": 75}</p>
        </div>
        <div class="endpoint">
            <div class="method">GET /audio/stream</div>
            <p>Stream audio data with chunked encoding</p>
        </div>
    </body>
    </html>
    '''
    status = audio_server.get_status()
    return render_template_string(html_template, tracks=status['available_tracks'])

@app.route('/api/status', methods=['GET'])
def api_status():
    """Get server status"""
    try:
        status = audio_server.get_status()
        return jsonify(status), 200
    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 500

@app.route('/api/play', methods=['POST'])
def api_play():
    """Start playing audio"""
    try:
        track = None
        if request.is_json and request.json:
            track = request.json.get('track')
        
        result = audio_server.play(track)
        
        # Longer delay to ensure response is fully sent before connection closes
        time.sleep(0.1)
        
        if result.get('status') == 'error':
            return jsonify(result), 400
        else:
            return jsonify(result), 200
            
    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 500

@app.route('/api/pause', methods=['POST'])
def api_pause():
    """Pause audio playback"""
    try:
        result = audio_server.pause()
        time.sleep(0.1)  # Longer delay for embedded clients
        
        if result.get('status') == 'error':
            return jsonify(result), 400
        else:
            return jsonify(result), 200
            
    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 500

@app.route('/api/stop', methods=['POST'])
def api_stop():
    """Stop audio playback"""
    try:
        result = audio_server.stop()
        time.sleep(0.1)  # Longer delay for embedded clients
        return jsonify(result), 200
        
    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 500

@app.route('/api/volume', methods=['POST'])
def api_volume():
    """Set playback volume"""
    try:
        if not request.is_json or not request.json:
            return jsonify({"status": "error", "message": "JSON body required"}), 400
        
        volume = request.json.get('volume')
        if volume is None:
            return jsonify({"status": "error", "message": "Volume parameter required"}), 400
        
        try:
            volume = int(volume)
        except (ValueError, TypeError):
            return jsonify({"status": "error", "message": "Volume must be a number"}), 400
        
        result = audio_server.set_volume(volume)
        time.sleep(0.1)  # Longer delay for embedded clients
        
        if result.get('status') == 'error':
            return jsonify(result), 400
        else:
            return jsonify(result), 200
            
    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 500

@app.route('/api/tracks', methods=['GET'])
def api_tracks():
    """Get list of available tracks"""
    try:
        status = audio_server.get_status()
        return jsonify({"tracks": status['available_tracks']}), 200
    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 500

@app.route('/audio/stream', methods=['GET'])
def audio_stream():
    """Stream audio data with embedded client compatibility"""
    try:
        # Get track parameter from query string
        track_name = request.args.get('track')
        
        if not track_name:
            # Use current playing track or first available
            status = audio_server.get_status()
            track_name = status.get('track') or (status['available_tracks'][0] if status['available_tracks'] else None)
        
        if not track_name:
            return jsonify({"status": "error", "message": "No track specified and none available"}), 400
        
        track_path = audio_server.get_track_path(track_name)
        if not track_path or not track_path.exists():
            return jsonify({"status": "error", "message": f"Track '{track_name}' not found"}), 404
        
        print(f"Streaming audio file: {track_path}")
        
        # Read entire file to avoid chunked encoding issues with embedded clients
        try:
            with open(track_path, 'rb') as audio_file:
                audio_data = audio_file.read()
                print(f"Read {len(audio_data)} bytes from {track_name}")
        except Exception as e:
            print(f"Error reading audio file: {e}")
            return jsonify({"status": "error", "message": f"Could not read audio file: {str(e)}"}), 500
        
        # Return complete file with embedded-friendly headers
        response = Response(
            audio_data,
            mimetype='audio/wav',
            headers={
                'Cache-Control': 'no-cache',
                'Connection': 'close',
                'Content-Length': str(len(audio_data)),
                'Accept-Ranges': 'bytes'
            }
        )
        
        print(f"Sending {len(audio_data)} bytes of audio data for {track_name}")
        return response
        
    except Exception as e:
        print(f"Error in audio_stream: {e}")
        return jsonify({"status": "error", "message": str(e)}), 500

def main():
    global audio_server
    
    parser = argparse.ArgumentParser(description='Flask HTTP Audio Streaming Server')
    parser.add_argument('--host', default='127.0.0.1', help='Host to bind to (default: 127.0.0.1)')
    parser.add_argument('--port', type=int, default=8000, help='Port to bind to (default: 8000)')
    parser.add_argument('--audio-dir', default='../test_data', help='Directory containing audio files')
    parser.add_argument('--debug', action='store_true', help='Enable Flask debug mode')
    
    args = parser.parse_args()
    
    # Initialize audio server
    audio_server = AudioStreamServer(args.audio_dir)
    
    print("\n" + "="*60)
    print("MP3 Rewind Flask Audio Server Starting")
    print("="*60)
    print(f"Server URL: http://{args.host}:{args.port}")
    print(f"Audio Directory: {Path(args.audio_dir).resolve()}")
    print("Available at:")
    print(f"  - Local: http://127.0.0.1:{args.port}")
    if args.host != '127.0.0.1':
        print(f"  - Network: http://{args.host}:{args.port}")
    print("="*60)
    print("Optimized for embedded HTTP clients (Zephyr)")
    print("Press Ctrl+C to stop the server")
    print()
    
    try:
        # Run Flask server with embedded-friendly configuration
        app.run(
            host=args.host,
            port=args.port,
            debug=args.debug,
            threaded=True,  # Handle multiple requests concurrently
            use_reloader=False  # Disable reloader to avoid issues
        )
    except KeyboardInterrupt:
        print("\nServer stopped by user")
    except Exception as e:
        print(f"Server error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
