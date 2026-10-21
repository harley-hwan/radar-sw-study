@echo off
start "TargetSim capture agent" /min powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0capture_agent.ps1"
