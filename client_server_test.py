#!/usr/bin/env python3
import subprocess
import time
import sys
import os
import platform

SERVER_TIMEOUT = 10  # seconds
CLIENT_TIMEOUT = 10  # seconds

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <build_directory>")
        sys.exit(1)

    build_dir = sys.argv[1]

    # Detect platform and add .exe suffix if on Windows
    exe_suffix = ".exe" if platform.system().lower().startswith("win") else ""

    server_path = os.path.join(build_dir, f"test_server{exe_suffix}")
    client_path = os.path.join(build_dir, f"test_client{exe_suffix}")

    if not os.path.isfile(server_path):
        print(f"❌ Server executable not found: {server_path}")
        sys.exit(1)

    if not os.path.isfile(client_path):
        print(f"❌ Client executable not found: {client_path}")
        sys.exit(1)

    try:
        # Start the server
        server = subprocess.Popen(
            [server_path],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )

        # Give server some time to start
        time.sleep(1)

        # Start the client
        client = subprocess.Popen(
            [client_path],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )

        # Wait for the client with timeout
        try:
            client_stdout, client_stderr = client.communicate(timeout=CLIENT_TIMEOUT)
        except subprocess.TimeoutExpired:
            client.kill()
            client_stdout, client_stderr = client.communicate()
            print("❌ Client timed out.")
            sys.exit(1)

        # Wait for the server with timeout
        try:
            server_stdout, server_stderr = server.communicate(timeout=SERVER_TIMEOUT)
        except subprocess.TimeoutExpired:
            server.kill()
            server_stdout, server_stderr = server.communicate()
            print("❌ Server timed out.")
            sys.exit(1)

        # Check return codes
        client_rc = client.returncode
        server_rc = server.returncode

        print("=== Server Output ===")
        print(server_stdout.decode(errors="ignore"), end="")
        print(server_stderr.decode(errors="ignore"), end="")
        print("=====================\n")

        print("=== Client Output ===")
        print(client_stdout.decode(errors="ignore"), end="")
        print(client_stderr.decode(errors="ignore"), end="")
        print("=====================\n")

        if client_rc != 0:
            print(f"❌ Client failed with return code {client_rc}")
            sys.exit(1)

        if server_rc != 0:
            print(f"❌ Server failed with return code {server_rc}")
            sys.exit(1)

        print("✅ Both client and server exited successfully.")

    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
