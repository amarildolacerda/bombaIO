import socket
import threading
import time
from colorama import init, Fore

init(autoreset=True)  # Inicializa o colorama para resetar cores automaticamente

HOST = "0.0.0.0"  # Aceita conexões de qualquer IP
PORT = 12345  # Porta TCP para comunicação
BROADCAST_PORT = 54321  # Porta UDP para broadcast
BROADCAST_INTERVAL = 5  # Tempo entre broadcasts
ALIVE_INTERVAL = 30  # Tempo entre mensagens alive
pairing_mode = True  # Indica se está no modo de pareamento
discovered_devices = []  # Lista de dispositivos encontrados
running = True  # Indica se o servidor está rodando

def send_broadcast():
    """ Envia broadcast para descobrir dispositivos na rede """
    global pairing_mode, running
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as broadcast_socket:
        broadcast_socket.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        
        while pairing_mode and running:
            message = f"{socket.gethostbyname(socket.gethostname())}:{PORT}".encode()
            broadcast_socket.sendto(message, ('255.255.255.255', BROADCAST_PORT))
            print(Fore.BLUE + f"🔊 Enviando broadcast: {message.decode()}")
            time.sleep(BROADCAST_INTERVAL)

def listen_for_devices():
    """ Escuta respostas ao broadcast e armazena dispositivos descobertos """
    global pairing_mode, discovered_devices, running
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as listen_socket:
        listen_socket.bind(('', BROADCAST_PORT))
        
        while pairing_mode and running:
            data, addr = listen_socket.recvfrom(1024)
            device_info = addr[0]
            if device_info not in discovered_devices:
                discovered_devices.append(device_info)
                print(Fore.GREEN + f"🆕 Dispositivo descoberto: {device_info}")

def send_alive_messages():
    """ Envia mensagens alive para dispositivos descobertos a cada 30 segundos """
    global discovered_devices, running
    while running:
        for device in discovered_devices:
            try:
                with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as alive_socket:
                    message = b"{alive|server}\n"
                    alive_socket.sendto(message, (device, BROADCAST_PORT))
                    print(Fore.CYAN + f"📡 Enviando alive para {device}")
            except Exception as e:
                print(Fore.RED + f"Erro ao enviar alive: {e}")
        time.sleep(ALIVE_INTERVAL)

def start_server():
    """ Inicia o servidor TCP """
    global running
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server_socket:
        server_socket.bind((HOST, PORT))
        server_socket.listen()
        print(Fore.GREEN + f"🌐 Servidor escutando em {HOST}:{PORT}")

        while running:
            try:
                conn, addr = server_socket.accept()
                with conn:
                    print(Fore.CYAN + f"🤝 Cliente conectado: {addr}")

                    while running:
                        data = conn.recv(1024)
                        if not data:
                            break

                        payload = data.decode(errors="ignore")
                        print(Fore.YELLOW + f"📥 Recebido -> {payload}")

                        if "ping" in payload:
                            response = b"{pong|server}\n"
                        elif "st" in payload:
                            response = b"{ack|status-ok}\n"
                        elif "pub" in payload:
                            response = b"{ack|published}\n"
                        elif "ack" in payload:
                            response = b"{confirm|received}\n"
                        else:
                            response = b"{error|unknown-command}\n"

                        conn.sendall(response)
                        print(Fore.MAGENTA + f"🚀 Enviado -> {response.decode(errors='ignore')}")
            except KeyboardInterrupt:
                running = False
                print(Fore.RED + "\nServidor encerrado pelo usuário.")

if __name__ == "__main__":
    try:
        # Inicia o modo de pareamento
        pairing_mode = True
        threading.Thread(target=send_broadcast, daemon=True).start()
        threading.Thread(target=listen_for_devices, daemon=True).start()

        # Após um tempo, encerra o modo de pareamento e inicia envio de alive
        time.sleep(30)
        pairing_mode = False
        threading.Thread(target=send_alive_messages, daemon=True).start()

        # Inicia o servidor TCP
        start_server()

    except KeyboardInterrupt:
        running = False
        print(Fore.RED + "\nExecução interrompida! Encerrando...")