@echo off

echo Starting Mosquitto MQTT Broker...
start /min "Mosquitto" "C:\Program Files\Mosquitto\mosquitto.exe" -v -c "C:\Users\28933\Desktop\mosquitto.conf"

echo Starting AI Agent...
start /min "Agent" /D "C:\Users\28933\Desktop\study_linux\work_program\edge-ai-agent-gateway\agent" python main.py

echo All started.
timeout /t 3 /nobreak >nul
