#!/usr/bin/env python3
import asyncio
import websockets
from datetime import datetime
import argparse
from colorama import init, Fore, Style

init(autoreset=True)  # Configura colorama para auto-reset

class DynamicColorMonitor:
    COLOR_MAP = {
        '[INFO]': Fore.GREEN,
        '[SEND]': Fore.BLUE,
        '[DBUG]': Fore.YELLOW,
        '[RECV]': Fore.MAGENTA,
        '[ERRO]': Fore.RED,
        '[WARN]': Fore.CYAN,
        '✔': Fore.GREEN,  # Conexão estabelecida
        '⚠': Fore.RED,    # Avisos/erros
        '↻': Fore.YELLOW  # Reconexão
    }

    def __init__(self, ip, port=80):
        self.uri = f"ws://{ip}:{port}/ws"
        self.running = True

    def _get_color(self, message):
        """Determina a cor baseada nos primeiros caracteres"""
        prefix = message[:6]
        for key, color in self.COLOR_MAP.items():
            if key in prefix:
                return color
        return Fore.WHITE  # Padrão

    async def log(self, message):
        """Log com cor dinâmica"""
        time = datetime.now().strftime('%H:%M:%S')
        color = self._get_color(message)
        print(f"{Fore.LIGHTBLACK_EX}{time}{Style.RESET_ALL} {color}{message}")

    async def run(self):
        """Conexão WebSocket principal"""
        while self.running:
            try:
                async with websockets.connect(self.uri) as ws:
                    await self.log("✔ Conectado ao WebSocket")
                    
                    async for message in ws:
                        await self.log(message)

            except Exception as e:
                await self.log(f"⚠ Erro: {str(e)}")
                await self.log("↻ Tentando reconectar em 3 segundos...")
                await asyncio.sleep(3)

async def main():
    parser = argparse.ArgumentParser(description='Monitor WebSocket com Cores Dinâmicas')
    parser.add_argument('ip', help='IP do dispositivo (ex: 192.168.1.100)')
    parser.add_argument('-p', '--port', type=int, default=80, help='Porta WebSocket')
    
    args = parser.parse_args()
    
    monitor = DynamicColorMonitor(args.ip, args.port)
    print(f"\n{Fore.YELLOW}▶ Iniciando monitor {monitor.uri} (Ctrl+C para sair){Style.RESET_ALL}\n")
    
    try:
        await monitor.run()
    except KeyboardInterrupt:
        print(f"\n{Fore.RED}✖ Monitor encerrado{Style.RESET_ALL}")

if __name__ == "__main__":
    asyncio.run(main())