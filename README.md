# 🎮 PS4 Controlr Remote

### DualShock 4 → Xbox 360 Virtual Controller (ViGEm + SDL3)

Windows x64 | XInput Emulation | Low-Latency Input Pipeline

---

## 📌 Overview

PS4 Controlr Remote é um aplicativo para Windows que converte um controle **DualShock 4 (PS4)** em um dispositivo virtual **Xbox 360 (XInput)**.

A aplicação utiliza:

- **SDL3** para captura de entrada (Gamepad API)
- **ViGEmBus** para criação do dispositivo virtual XInput
- Processamento interno para filtragem e normalização de eixos

O objetivo é fornecer compatibilidade com jogos que exigem **XInput**, mantendo controle sobre latência, deadzone e estabilidade do sinal.

---

## ⚙️ Requisitos

- Windows 10 ou Windows 11 (64-bit)

### Driver obrigatório

O driver **ViGEmBus** deve estar instalado para que o dispositivo virtual seja criado.

Instalação:

https://docs.nefarius.at/projects/ViGEm/How-to-Install/

Sem o driver, o controle virtual não será inicializado.

---

## 🚀 Instalação

1. Instalar o ViGEmBus
2. Baixar o instalador da versão mais recente
3. Executar o Setup
4. Conectar o DualShock 4
5. Executar o programa

---

## 🔧 Processamento de Entrada

O pipeline interno realiza:

- Deadzone circular em ambos os analógicos
- Filtro anti-jitter
- Conversão de faixa (-32768..32767)
- Inversão configurável do eixo Y
- Envio event-driven com heartbeat fixo

Fluxo de processamento:

DualShock 4 -> SDL3 Input -> Processamento (deadzone, filtro, inversão) -> ViGEm Virtual Xbox 360 -> Jogo (XInput)


---

## ⚡ Latência

### Frequência de Atualização

| Modo        | Frequência | Intervalo |
|------------|------------|-----------|
| Padrão     | 250 Hz    | 4 ms      |
| Alta       | 500 Hz    | 2 ms      |

### Latência Interna

Com:

- `timeBeginPeriod(1)`
- Prioridade elevada de processo/thread
- Sleep ajustado dinamicamente
- Envio imediato em mudança de estado

A latência interna do loop de aplicação situa-se entre:

**1 ms a 3 ms (nível de aplicação)**

Observação: A latência total percebida depende do jogo, taxa de quadros, VSync e agendamento do sistema operacional.

---

## 🧪 Estabilidade

Mecanismos implementados:

- Comparação de estado antes do envio (reduz tráfego redundante)
- Deadzone circular para evitar drift
- Filtro anti-jitter para micro variações
- Heartbeat fixo para evitar timeout de dispositivo
- Ignora dispositivos XInput virtuais para evitar loop de captura

---

## ⚠ Anti-Cheat

Alguns sistemas de anti-cheat podem:

- Detectar dispositivos virtuais
- Restringir ou bloquear entrada

Esse comportamento depende exclusivamente do jogo.

---

## 🧩 Compatibilidade

Compatível com títulos que suportam:

- XInput
- Xbox 360 Controller
