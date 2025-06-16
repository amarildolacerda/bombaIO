import socket
import argparse
import threading
import time
from colorama import init, Fore

init(autoreset=True)  # Inicializa o colorama para resetar cores automaticamente

PORT = 12345  # Porta fixa do servidor
SEND_INTERVAL = 5  # Tempo em segundos entre envios de {st|on}
message_id = 0  # Contador de mensagens
state = "on"  # Estado inicial

def format_message(message):
    """ Formata a mensagem enviada de forma mais legível, evitando caracteres estranhos """
    hex_representation = ' '.join(f'{byte:02X}' for byte in message)
    ascii_representation = ''.join(chr(byte) if 32 <= byte <= 126 else '.' for byte in message)
    return f'[{hex_representation}] -> {ascii_representation}'

def send_periodic_status(client_socket, term):
    """ Alterna entre 'on' e 'off' a cada 5 segundos e envia o estado atualizado """
    global message_id, state
    while True:
        try:
            message_id = (message_id + 1) % 256  # Garante que ID fique dentro do intervalo de um byte
            
            # Alterna estado entre "on" e "off"
            state = "off" if state == "on" else "on"

            response = bytes([0x00, term, message_id, 0x05, 0x03]) + f"{{st|{state}}}\n".encode()
            client_socket.sendall(response)
            print(Fore.BLUE + f'Enviado -> {format_message(response)}')
            time.sleep(SEND_INTERVAL)
        except Exception as e:
            print(Fore.RED + f"\nErro ao enviar {st|on}: {e}")
            break

def run_test(ip, term=8):
    global message_id
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as client_socket:
            client_socket.connect((ip, PORT))
            print(Fore.GREEN + f'Conectado ao servidor {ip}:{PORT}')

            # Inicia a thread para envio periódico de {st|on}
            threading.Thread(target=send_periodic_status, args=(client_socket, term), daemon=True).start()

            while True:
                try:
                    data = client_socket.recv(1024)
                    if not data:
                        break

                    to, from_, id_, length, hop = data[:5]
                    payload = data[5:-1].decode()

                    print(Fore.YELLOW + f'Recebido -> To: {to}, From: {from_}, ID: {id_}, Length: {length}, Hop: {hop}, Payload: {payload}')

                    message_id = (message_id + 1) % 256  # Incrementa ID para cada mensagem enviada

                    if "ping" in payload:
                        response = bytes([from_, term, message_id, length, 3]) + b"{pong|ok}\n"
                        client_socket.sendall(response)
                        print(Fore.CYAN + f'Enviado -> {format_message(response)}')
                    if "pub" in payload:
                        response = bytes([from_, term, message_id, length, 3]) + b"{pub|ok}\n"
                        client_socket.sendall(response)
                        print(Fore.CYAN + f'Enviado -> {format_message(response)}')


                    elif "ack" not in payload:
                        response = bytes([from_, term, message_id, length, 3]) + b"{ack|ok}\n"
                        client_socket.sendall(response)
                        print(Fore.MAGENTA + f'Enviado -> {format_message(response)}')

                except KeyboardInterrupt:
                    print(Fore.RED + "\nCliente encerrado pelo usuário.")
                    break

    except KeyboardInterrupt:
        print(Fore.RED + "\nCliente encerrado pelo usuário.")
    except socket.error as e:
        print(Fore.RED + f"\nErro de conexão: {e}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Teste de conexão com gateway IoT")
    parser.add_argument("ip", help="Endereço IP do gateway")
    parser.add_argument("term", type=int, help="Número do terminal")
    args = parser.parse_args()

    run_test(args.ip, args.term)