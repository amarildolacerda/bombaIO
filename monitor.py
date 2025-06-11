#!/usr/bin/env python3
import asyncio
import websockets
from datetime import datetime, timedelta
import argparse
from colorama import init, Fore, Style
from collections import defaultdict

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
        self.message_stats = defaultdict(int)
        self.total_messages = 0
        self.last_stats_time = datetime.now()
        self.stats_interval = timedelta(seconds=60)

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

        # Atualiza estatísticas
        self._update_stats(message)

    def _update_stats(self, message):
        """Atualiza as estatísticas de mensagens"""
        now = datetime.now()
        prefix = message[:6]
        
        # Contagem por tipo de mensagem
        for key in self.COLOR_MAP:
            if key in prefix:
                self.message_stats[key] += 1
                break
        else:
            self.message_stats['[OTHER]'] += 1
        
        self.total_messages += 1
        
        # Verifica se é hora de mostrar estatísticas
        if now - self.last_stats_time >= self.stats_interval:
            self._show_stats()
            self.last_stats_time = now

    def _show_stats(self):
        """Exibe as estatísticas coletadas"""
        stats_time = datetime.now().strftime('%H:%M:%S')
        print(f"\n{Fore.YELLOW}=== Estatísticas ({stats_time}) ===")
        print(f"{Fore.CYAN}Total de mensagens: {self.total_messages}")
        
        for msg_type, count in sorted(self.message_stats.items()):
            color = self.COLOR_MAP.get(msg_type, Fore.WHITE)
            print(f"{color}{msg_type}: {count} mensagens")
        
        print(f"{Fore.YELLOW}=============================\n{Style.RESET_ALL}")
        
        # Reseta contadores para o próximo intervalo
        self.message_stats.clear()
        self.total_messages = 0

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