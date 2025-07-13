# py\_async\_lib

Minha própria implementação personalizada da biblioteca asyncio a partir do zero, usando Wrappers em C e Python.

## Benchmark

Você pode comparar a taxa de transferência do loop de eventos do projeto contra o loop `asyncio` embutido do Python com o script de benchmark:

```bash
PYTHONPATH=. python -m benchmarks.throughput  # exibe tempos de execução em segundos
```

Para usar o loop em C de alto desempenho com asyncio:

```python
import py_async_lib

py_async_lib.install()
```

### Executando os testes

Instale o pacote no modo editável e execute a suíte de testes com `pytest`:

```bash
pip install -e .
pytest
```

Os testes cobrem cada marco das issues **#1** a **#9**, incluindo o loop Python, o esqueleto em C, I/O watchers, helpers de subprocess e tratamento de sinais.

---

## 📚 Visão Geral da Arquitetura

A seguir, diagramas Mermaid ilustram como as peças se encaixam.

### ER Diagram (Entidades e Relacionamentos)

```mermaid
erDiagram
    PYTHON_USER_CODE ||--o{ PY_ASYNC_LIB : calls
    PY_ASYNC_LIB ||--|| CASYNCIO : binds
    CASYNCIO ||--o{ ASYNCIO_TASKS : schedules
```

### Sequence Diagram (Fluxo de Chamadas)

```mermaid
sequenceDiagram
    participant User as Python user code 🐍
    participant Lib as py_async_lib package 📦
    participant CLoop as casyncio (C) ⚙️
    participant Tasks as asyncio tasks/futures 🔑

    User->>Lib: import & chamadas de API
    Lib->>CLoop: install(), create_event_loop()
    Lib->>CLoop: StreamWriter.write(data)
    CLoop->>CLoop: epoll_wait() (monitoramento de I/O)
    CLoop-->>Tasks: expedites callbacks
    Tasks->>Lib: executa callback Python
```

### State Diagram (Estados do Loop de Eventos)

```mermaid
stateDiagram
    [*] --> INIT : novo loop
    INIT --> RUNNING : run_forever()
    RUNNING --> STOPPED : stop()
    STOPPED --> [*]
```

---

## Como Funciona

1. **Python user code** chama funções no pacote `py_async_lib`.
2. **py\_async\_lib** faz binding para o loop em C (`casyncio`).
3. **casyncio** gerencia um loop baseado em `epoll`, integrando timers, I/O e sinais.
4. Callbacks são enfileirados em **asyncio tasks/futures** para execução no contexto Python.

---

### Sequence Diagram: Fluxo de `loop._c_write` e Escrita em C

````mermaid
sequenceDiagram
    participant PyUser as Código Python
    participant Loop as EventLoop
    participant Ensure as ensure_fdslot
    participant OutBuf as OutBuf
    participant Socket as socket_write_now
    participant EpollCtl as epoll_ctl
    participant EpollWait as epoll_wait

    PyUser->>Loop: loop._c_write(fd, buffer)
    Loop->>Ensure: ensure_fdslot(fd)
    Ensure-->>Loop: slot preparado
    alt OutBuf não existente
        Loop->>OutBuf: outbuf_new()
        OutBuf-->>Loop: OutBuf retornado
    end
    Loop->>Socket: socket_write_now(fd, OutBuf)
    alt dados pendentes
        Socket-->>Loop: retorna 1 (pending)
        Loop->>EpollCtl: epoll_ctl(ADD/MOD, fd, EPOLLOUT)
        EpollCtl-->>Loop: OK
        Loop->>EpollWait: epoll_wait bloqueante
        EpollWait-->>Loop: EPOLLOUT evento
        Loop->>Socket: socket_write_now(fd, OutBuf)
    else completo
        Socket-->>Loop: retorna 0 (complete)
    end
```mermaid
sequenceDiagram
    participant PyUser as Código Python
    participant Loop as PyEventLoopObject
    participant Ensure as ensure_fdslot()
    participant OutBuf as outbuf_new()/OutBuf
    participant Socket as socket_write_now()
    participant EpollCtl as epoll_ctl
    participant EpollWait as epoll_wait

    PyUser->>Loop: loop._c_write(fd, buffer)
    Loop->>Ensure: ensure_fdslot(fd)
    Ensure-->>Loop: slot preparado
    alt ob->obuf não existente
        Loop->>OutBuf: outbuf_new()
        OutBuf-->>Loop: OutBuf retornado
    end
    Loop->>Socket: socket_write_now(fd, OutBuf)
    alt dados pendentes
        Socket-->>Loop: retorna 1 (pending)
        Loop->>EpollCtl: epoll_ctl(ADD/MOD, fd, EPOLLOUT)
        EpollCtl-->>Loop: OK
        Loop->>EpollWait: epoll_wait bloqueante
        EpollWait-->>Loop: EPOLLOUT evento
        Loop->>Socket: socket_write_now(fd, OutBuf)
    else completo
        Socket-->>Loop: retorna 0 (complete)
    end
````

1. **Python user code** chama funções no pacote `py_async_lib`.
2. **py\_async\_lib** faz binding para o loop em C (`casyncio`).
3. **casyncio** gerencia um loop baseado em `epoll`, integrando timers, I/O e sinais.
4. Callbacks são enfileirados em **asyncio tasks/futures** para execução no contexto Python.

---

## Progresso de Desenvolvimento

* Estrutura inicial em Python e testes básicos (#1).
* Esqueleto em C, configuração de build (#2).
* `call_soon`, `run_forever` (#3).
* Temporizadores (`call_later`) (#4).
* Integração de I/O com `epoll` (#5).
* Escrita não bloqueante e back-pressure (#6).
* Cancelamento de tarefas e timeouts (#7).
* Tratamento de sinais e subprocessos (#8).
* Compatibilidade total com `asyncio` (#9).
* Otimizações finais e perfilamento (#10).
