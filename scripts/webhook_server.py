from flask import Flask, request
import xml.etree.ElementTree as ET
import requests
import subprocess
import os
import sys

app = Flask(__name__)

# Настройки
API_KEY = "REMOVED_KEY"
OUTPUT_DIR = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.expanduser("~"), "Downloads")
FORMAT = sys.argv[2] if len(sys.argv) > 2 else "MP4 (best)"
MONITOR_STREAMS = sys.argv[3] == "true" if len(sys.argv) > 3 else True
MONITOR_VIDEOS = sys.argv[4] == "true" if len(sys.argv) > 4 else False

# Функция для проверки, является ли видео стримом
def is_stream(video_id):
    url = f"https://www.googleapis.com/youtube/v3/videos?part=snippet,liveStreamingDetails&id={video_id}&key={API_KEY}"
    response = requests.get(url).json()
    if "items" in response and len(response["items"]) > 0:
        video = response["items"][0]
        return "liveStreamingDetails" in video
    return False

# Функция для записи видео или стрима
def record_content(video_id, is_stream_content):
    stream_url = f"https://www.youtube.com/watch?v={video_id}"
    output_file = os.path.join(OUTPUT_DIR, f"{'stream' if is_stream_content else 'video'}_{video_id}_%(timestamp)s.%(ext)s")
    
    arguments = [
        "yt-dlp",
        "--force-overwrites",
        "--ffmpeg-location", "ffmpeg",
        "-o", output_file,
        stream_url
    ]

    # Определяем формат
    if FORMAT == "MP4 (best)":
        arguments.extend(["-f", "bestvideo+bestaudio/best", "--merge-output-format", "mp4", "--recode-video", "mp4"])
    elif FORMAT == "MP4 (1080p)":
        arguments.extend(["-f", "bestvideo[height<=1080]+bestaudio/best", "--merge-output-format", "mp4", "--recode-video", "mp4"])
    elif FORMAT == "MP4 (720p)":
        arguments.extend(["-f", "bestvideo[height<=720]+bestaudio/best", "--merge-output-format", "mp4", "--recode-video", "mp4"])
    elif FORMAT == "WebM (best)":
        arguments.extend(["-f", "bestvideo[ext=webm]+bestaudio[ext=webm]/best", "--merge-output-format", "webm", "--recode-video", "webm"])
    elif FORMAT == "Audio only (MP3)":
        arguments.extend(["-f", "bestaudio", "--extract-audio", "--audio-format", "mp3"])

    if is_stream_content:
        arguments.extend(["--live-from-start", "--no-part", "--buffer-size", "256K"])

    subprocess.run(arguments, check=True)

# Обработчик вебхуков
@app.route("/webhook", methods=["GET", "POST"])
def webhook():
    if request.method == "GET":
        challenge = request.args.get("hub.challenge")
        if challenge:
            return challenge
        return "OK", 200

    xml_data = request.data.decode("utf-8")
    try:
        root = ET.fromstring(xml_data)
        for entry in root.findall("{http://www.w3.org/2005/Atom}entry"):
            video_id = entry.find("{http://www.w3.org/2005/Atom}id").text.split(":")[-1]
            is_stream_content = is_stream(video_id)
            if (is_stream_content and MONITOR_STREAMS) or (not is_stream_content and MONITOR_VIDEOS):
                print(f"Обнаружен {'стрим' if is_stream_content else 'видео'}: {video_id}")
                record_content(video_id, is_stream_content)
    except Exception as e:
        print(f"Ошибка обработки вебхука: {e}")

    return "OK", 200

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8000)
