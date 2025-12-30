# TCP Chat Application

This project implements a TCP Chat Client using Python (Tkinter) and C++ (Winsock DLL).

## Prerequisites Check

### Windows (PowerShell)
Run the following commands to verify you have the necessary tools:

```powershell
# 1. Check Python Version (Should be 3.x)
python --version

# 2. Check Tkinter (GUI Library)
python -c "import tkinter; print('Tkinter is installed and ready.')"

# 3. Check G++ Compiler (Required for building the C++ DLL)
g++ --version
```

### Ubuntu/Linux (Bash)
Run the following commands to install and verify dependencies:

```bash
# Install Python3, Tkinter, and G++
sudo apt update
sudo apt install python3 python3-tk g++ -y

# Verify installations
python3 --version
python3 -c "import tkinter; print('Tkinter is installed and ready.')"
g++ --version
```

## To Use

To start the application, navigate to the python directory and run the client script:

```bash
cd python
python chat_client.py
```
