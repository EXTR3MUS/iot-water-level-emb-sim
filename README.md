# iot-water-level-emb-sim

Simulation of the embedded software using pthreads C lib.

## Running

To build and run the program:
`./run.sh`

In case you get permission errors run:
`chmod +x run.sh`

## Environment
The software is being developed over `WSL Ubuntu 24.04`.

## Threads
Threads simulated.
### Thread 1
- Ler informações do sensor ultrassônico (nível de água no reservatório);
- Executar esta leitura a cada 5 minutos;

### Thread 2
- Gravar os dados lidos em arquivo local (buffer);

### Thread 3
- Enviar dados armazenados para o servidor IoT (central de monitoramento) via rede;
- Realizar o envio a cada 5 minutos;
- Sempre enviar todos os dados do buffer (arquivo) no momento do envio;

### Thread 4
- Esvaziar o buffer (arquivo local) somente em caso de sucesso no envio dos dados para o servidor IoT;
