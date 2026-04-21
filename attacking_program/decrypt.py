# Clé de chiffrement
CRYPTO_KEY = b"B13nV3nu3ALaM31ll3ur3C0nf3r3nc3D3LAnn33&L3tH4ckCL41r3L3r0ux!2025"
KEY_SIZE = 64

def xor_crypt(data):
    """Chiffre/déchiffre les données avec XOR"""
    if isinstance(data, str):
        data = data.encode()
    
    if len(data) == 0:
        return b""
    
    result = bytearray()
    for i, byte in enumerate(data):
        key_byte = CRYPTO_KEY[i % KEY_SIZE]
        result.append(byte ^ key_byte)
    
    return bytes(result)

def send_encrypted(socket, data):
    """Envoie des données chiffrées"""
    if isinstance(data, str):
        data = data.encode()
    encrypted = xor_crypt(data)
    socket.sendall(encrypted)

def recv_encrypted(socket, size):
    """Reçoit et déchiffre des données"""
    try:
        encrypted_data = socket.recv(size)
        if not encrypted_data:
            print("[DEBUG] No data received")
            return b""
        
        
        decrypted = xor_crypt(encrypted_data)
        
        return decrypted
    except Exception as e:
        return b""