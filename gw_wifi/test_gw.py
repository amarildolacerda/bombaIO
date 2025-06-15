import socket
import time
import random
import argparse
from struct import pack, unpack
from threading import Thread
from colorama import init, Fore, Back, Style

# Inicializa colorama
init(autoreset=True)

class DeviceSimulator:
    def __init__(self, device_id, gateway_ip, port=12345):
        self.device_id = device_id
        self.gateway_ip = gateway_ip
        self.port = port
        self.socket = None
        self.connected = False
        self.name = f"DispositivoTeste_{device_id}"
        self.max_retries = 5
        self.retry_delay = 3
        self.running = False
        self.msg_counter = 1
        self.status = True
        
    def connect(self):
        retry_count = 0
        while retry_count < self.max_retries and not self.connected:
            try:
                if self.socket:
                    self.socket.close()
                
                self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.socket.settimeout(10)
                self.socket.connect((self.gateway_ip, self.port))
                self.connected = True
                print(f"{Fore.GREEN}[CONEXÃO] Dispositivo {self.device_id} conectado ao gateway")
                return True
                
            except (socket.error, socket.timeout) as e:
                retry_count += 1
                print(f"{Fore.YELLOW}[ERRO] Falha na conexão (tentativa {retry_count}/{self.max_retries}): {str(e)}")
                if retry_count < self.max_retries:
                    time.sleep(self.retry_delay)
        
        return False
    
    def format_message(self, to, from_id, msg_id, hop, event, value):
        """Formata mensagem no padrão %c%c%c%c%c{event|value}"""
        payload = f"{{{event}|{value}}}"
        length = len(payload)
        return pack('BBBBB', to, from_id, msg_id, length, hop) + payload.encode('ascii')+b'\n'
    
    def parse_message(self, data):
        """Analisa mensagens no formato especificado"""
        if len(data) < 5:
            print(f"{Fore.RED}[ERRO] Mensagem muito curta (tamanho: {len(data)})")
            return None
            
        try:
            to, from_id, msg_id, length, hop = unpack('BBBBB', data[:5])
            
            if len(data) != 5 + length:
                print(f"{Fore.RED}[ERRO] Tamanho inconsistente. Header: {length}, Total: {len(data)}")
                return None
                
            payload = data[5:5+length]
            
            try:
                decoded = payload.decode('ascii')
                
                if not decoded.startswith('{') or not decoded.endswith('}'):
                    print(f"{Fore.RED}[ERRO] Formato inválido (sem chaves): {decoded}")
                    return None
                    
                content = decoded[1:-1].split('|', 1)
                if len(content) != 2:
                    print(f"{Fore.RED}[ERRO] Formato inválido (sem |): {decoded}")
                    return None
                    
                return {
                    'to': to,
                    'from': from_id,
                    'id': msg_id,
                    'len': length,
                    'hop': hop,
                    'event': content[0],
                    'value': content[1],
                    'raw': payload
                }
                
            except UnicodeDecodeError:
                print(f"{Fore.RED}[ERRO] Payload não é texto ASCII: {payload.hex()}")
                return None
                
        except Exception as e:
            print(f"{Fore.RED}[ERRO CRÍTICO] Falha ao parsear: {e}")
            return None

    def send_message(self, to, from_id, msg_id, hop, event, value):
        if not self.connected:
            print(f"{Fore.YELLOW}[ERRO] Tentativa de envio sem conexão")
            return False
        
        message = self.format_message(to, from_id, msg_id, hop, event, value)
        
        try:
            self.socket.settimeout(10)
            self.socket.sendall(message)
            
            # Cores por tipo de evento
            if event.startswith("EVT_"):
                color = Fore.CYAN
            elif event.startswith("CMD_"):
                color = Fore.MAGENTA
            elif event in ["ack", "pong"]:
                color = Fore.GREEN
            else:
                color = Fore.WHITE
                
            print(f"{color}[ENVIADO] {Fore.WHITE}Para: {to}, {color}Evento: {event}, Valor: {value}")
            return True
        except (socket.error, socket.timeout) as e:
            print(f"{Fore.RED}[ERRO] Falha no envio: {e}")
            self.connected = False
            return False
    
    def send_ack(self, original_msg):
        """Envia confirmação de recebimento"""
        return self.send_message(
            to=original_msg['from'],
            from_id=self.device_id,
            msg_id=self.msg_counter,
            hop=0,
            event="ack",
            value=str(original_msg['id'])
        )
    
    def send_pong(self, original_msg):
        """Responde a PING com PONG"""
        return self.send_message(
            to=original_msg['from'],
            from_id=self.device_id,
            msg_id=self.msg_counter,
            hop=0,
            event="PONG",
            value=str(original_msg['id'])
        )
    
    def receiver_thread(self):
        """Thread para receber mensagens, tratando \n como delimitador"""
        buffer = b''
        while self.running:
            try:
                if not self.connected:
                    time.sleep(2)
                    continue
                
                data = self.socket.recv(128)  # Recebe dados brutos (pode conter múltiplas mensagens)
                if not data:
                    print(f"{Fore.RED}[ERRO] Conexão encerrada pelo servidor")
                    self.connected = False
                    continue
                    
                buffer += data
                print(f"{Fore.LIGHTBLACK_EX}[DADOS] Recebidos {len(data)} bytes (buffer: {len(buffer)})")
                
                # Separa as mensagens pelo delimitador \n
                messages = buffer.split(b'\n')
                buffer = messages.pop()  # Guarda mensagem incompleta de volta no buffer
                
                for msg in messages:
                    if len(msg) < 5:  # Verifica tamanho mínimo do cabeçalho
                        print(f"{Fore.RED}[ERRO] Mensagem muito curta: {msg}")
                        continue
                    
                    # Processa cada mensagem individualmente
                    parsed = self.parse_message(msg)
                    if parsed:
                        # (Restante do código de processamento...)
                        if parsed['event'] == "PING":
                            color = Fore.YELLOW
                            self.send_pong(parsed)
                        elif parsed['event'].startswith("CMD_"):
                            color = Fore.MAGENTA
                        else:
                            color = Fore.BLUE
                        
                        print(f"{color}[RECEBIDO] {Fore.WHITE}De: {parsed['from']}, {color}Evento: {parsed['event']}, Valor: {parsed['value']}")
                        self.msg_counter += 1
                        self.send_ack(parsed)
            
            except socket.timeout:
                continue
            except ConnectionResetError:
                print(f"{Fore.RED}[ERRO] Conexão resetada pelo servidor")
                self.connected = False
            except Exception as e:
                print(f"{Fore.RED}[ERRO] Exceção inesperada: {e}")
                self.connected = False
    def identify(self):
        """Envia mensagem de identificação"""
        return self.send_message(
            to=0,
            from_id=self.device_id,
            msg_id=self.msg_counter,
            hop=0,
            event="CMD_IDENTIFY",
            value=self.name
        )
    
    def send_sensor_data(self, sensor_type, value):
        """Envia dados de sensores"""
        events = {
            "temperature": "EVT_TEMP",
            "humidity": "EVT_HUMID",
            "pressure": "EVT_PRESS",
            "st": "EVT_STATUS",
            "ping":"EVT_PING"
        }
        if sensor_type in events:
            self.msg_counter += 1
            return self.send_message(
                to=0,
                from_id=self.device_id,
                msg_id=self.msg_counter,
                hop=3,
                event=sensor_type,
                value=str(value)
            )
        return False
    
    def start(self):
        """Inicia o simulador de dispositivo"""
        self.running = True
        Thread(target=self.receiver_thread, daemon=True).start()
        
        if not self.connect():
            print(f"{Fore.RED}[ERRO] Falha na conexão inicial")
            return False
        
        if not self.identify():
            print(f"{Fore.RED}[ERRO] Falha no envio de identificação")
            return False
        
        return True
    
    def stop(self):
        """Para o simulador de dispositivo"""
        self.running = False
        self.disconnect()
    
    def disconnect(self):
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
        self.socket = None
        self.connected = False
        print(f"{Fore.GREEN}[INFO] Dispositivo {self.device_id} desconectado")


def run_test(gateway_ip):
    DEVICE_ID = 42
    device = DeviceSimulator(DEVICE_ID, gateway_ip)
    
    if not device.start():
        return
    
    try:
        while True:
            if not device.connected:
                print(f"{Fore.YELLOW}[RECONEXÃO] Tentando reconectar...")
                if not device.connect():
                    time.sleep(5)
                    continue
                
                device.identify()
            
            # Simula envio de dados de sensores
            temp = round(20 + random.random() * 10, 1)  # 20-30°C
            humidity = round(40 + random.random() * 30, 1)  # 40-70%
            
            device.send_sensor_data("temp", temp)
            time.sleep(1)
            device.send_sensor_data("hum", humidity)
            time.sleep(1)
            device.send_sensor_data("st", "on" if device.status else "off")
            device.status = not device.status
            time.sleep(1)
            device.send_sensor_data("ping", "on" if device.status else "off")
            
            time.sleep(5)
            
    except KeyboardInterrupt:
        print(f"\n{Fore.GREEN}[INFO] Encerrando teste...")
    finally:
        device.stop()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Teste de conexão com gateway IoT')
    parser.add_argument('ip', help='Endereço IP do gateway')
    args = parser.parse_args()
    
    run_test(args.ip)