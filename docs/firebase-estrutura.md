# Estrutura de dados lida pela central

A central só **lê** estes caminhos do Realtime Database (quem escreve é o
projeto do sítio):

```
/caixas/caixa1|caixa2|caixa3
    nivel_percentual   (número, %)
    litros             (número)
    timestamp          (segundos desde 1970)
/clima
    temperatura        (°C)
    umidade            (%)
    pressao            (hPa)
    timestamp
/chuva
    status             "seco" | "gotas" | "chuva"
    timestamp
/alertas
    mascara            (número: soma dos alertas pendentes, ver abaixo)
```

Um dado só vale para a central se o `timestamp` tiver até 60 segundos.

## Máscara de alertas

`/alertas/mascara` é uma **soma**:

| Valor | Alerta | Ícone na matriz |
|------:|--------|-----------------|
| 1 | Caixa d'água baixa / queda de nível | caixa d'água |
| 2 | Bateria fraca | pilha |
| 4 | Internet fora | wifi cortado |
| 8 | Alerta geral do sítio | casinha |

Exemplo: `3` = caixa baixa **e** bateria fraca (os dois ícones se revezam).
`0` = nenhum alerta.

O ícone de **internet fora** também acende sozinho na central quando ela
fica 5 minutos sem receber dado novo do sítio.
