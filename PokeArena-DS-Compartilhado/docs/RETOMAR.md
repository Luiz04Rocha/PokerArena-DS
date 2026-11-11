# Retomar PokeArena DS

Versão atual **0.40**. Jogo nativo em C/BlocksDS para Nintendo DSi XL.

## Últimas instruções de Luiz

Base visual de tudo: Pokémon Black/White. Ler DIRECAO-VISUAL.md. A 0.11 trouxe cerca com abertura, pedras arredondadas, câmera a aproximadamente 55 graus e sorteio por batalha. Na 0.12, Luiz pediu luz na cauda do Charmander à noite e relatou piscadas apenas na tela superior, usando melonDS. Ainda não testou no console.

A 0.12 acrescenta halo quente e luz no chão, acompanhando a chama nos quadros dos três Pokémon da família Charmander. Remove alocação e envio de texturas/paletas durante a montagem da cena. Os pixels são preparados em RAM e copiados na abertura do VBlank; o HUD também é apresentado entre quadros. As capturas locais não reproduziram o apagamento relatado: não afirmar que o problema na instalação de Luiz está confirmado como resolvido antes do retorno dele.

Na 0.13, a primeira etapa da lista de Luiz trouxe colisões, reações de impacto e transições dos Pokémon. A 0.14 acrescenta câmera com enquadramento de segurança e aproximação no Ultimate, água com ondulações, flores e caminhos, sombras suaves, contagem inicial, reações de resultado, destaques no HUD e trilha original.

A 0.15 diferencia as três ondas e revisa a movimentação, os ataques e a apresentação dos sprites. Luiz descreveu efeitos que somem e sprites que se distorcem durante ataques. A revisão corrige causas encontradas na fila de texturas e nas transições. Falta conferir o sintoma na instalação de Luiz. Ele comentou que vai procurar um banco de sprites para avaliar depois.

A 0.16 é o teste pedido por Luiz para que os Pokémon olhem para o adversário durante o combate. Recuo e movimento lateral usam o mesmo acompanhamento. Investida e esquiva seguem o direcional. A pose de ataque mantém sua orientação até o fim.

A 0.17 aplica o material do PMD Collab e a revisão de sincronismo escolhida para esta etapa. Os marcadores de apoio e cabeça vêm das folhas oficiais. O relógio de cada golpe guia a pose, o disparo e o avanço da investida. O comando do jogador executa imediatamente e o aviso da CPU acompanha a preparação do sprite.

Luiz priorizou estilo, animações e fluidez do combate. Na 0.18, os Pokémon ganharam texturas de 64 × 64, saltos de comemoração e uma pose no chão com lágrimas na derrota. A investida tem aceleração e freada, e o rastro acompanha o caminho da esquiva. Novos mapas ficam para uma etapa futura.

Na 0.19, Luiz pediu um ajuste na tela inferior e enviou referências de botões coloridos com texto contornado e uma ficha de vida segmentada. O HUD de batalha usa essas referências, com A, B, X e Y em cores fixas e um marcador menor para o elemento de cada golpe.

Na 0.20, o menu inicial usa seis cartões ilustrados. Os demais menus ganharam o mesmo visual, e as setas da pausa mudam a luz por toque. Após um teste com outras pessoas, Luiz pediu uma clareira um pouco maior. Os limites agora medem 228 × 132 unidades, com caminhos laterais e um anel mais largo.

Na 0.21, Luiz pediu mais identidade de Pokémon no menu inicial e fundos mais escuros nas telas de abertura e resultado. O menu tem uma Poké Bola e retratos dos iniciais. A abertura separa líder e rival por elemento, e o resultado destaca o líder e o botão de revanche.

Na 0.22, Luiz renomeou o projeto para PokeArena DS e enviou uma referência com capa florestal e quatro cartões. A capa com Leafeon é um bitmap nativo de 256 × 192. Jogar abre todos os modos, Duelo abre o VS com a CPU, Horda oferece os modos de ondas e Opções reúne som, ajuda e créditos.

Na 0.23, Luiz pediu a Poké Bola, o ícone do Nintendo DS e os modos 1 vs 1 e 2 vs 2 dentro de Jogar. O 2 vs 2 usa um parceiro automático e dois rivais simultâneos com vida própria. A Horda continua com os dois modos de ondas.

Na 0.24, Luiz enviou uma logo ilustrada para a abertura. O gerador `ds/tools/build_title.py` compõe a logo com transparência sobre a floresta existente e prepara o bitmap do DSi. A arte e os prompts estão descritos em `ARTE-ABERTURA.md`.

Na 0.25, Luiz pediu que Charmander e Squirtle se movessem no fundo da abertura, com um aviso menor e animado. O cartão Multiplayer usa o ícone do DS e está reservado para implementar o modo local depois. Os duelos contra a CPU ficam em Jogar.

Na 0.26, Luiz pediu um cenário que combinasse com o passeio dos quatro iniciais. A abertura ganhou uma praça de rota em pixel art e quatro trajetos. A segunda referência de Luiz orientou a paleta grafite, prata e azul dos menus.

Na 0.27, a logo foi centralizada e o posto foi ajustado para abrir espaço para a composição. A tela inferior recebeu letras com pixels quadrados e ícones redesenhados para Jogar, Multiplayer, Horda e Opções.

Na 0.28, Luiz pediu uma arena um pouco maior, com pedras e troncos mais próximos das bordas. Os limites são x10 a x246 e y49 a y190. O centro fica livre para movimento e ataques, e as coberturas principais ficam nos lados. O anel, as trilhas e a vegetação acompanham a nova clareira.

Na 0.29, Luiz pediu uma abertura com mais movimento. A casinha deu lugar a uma nascente. Folhas e reflexos animam o cenário, e os quatro Pokémon alternam caminhada e ações em uma rotação das onze formas disponíveis.

Na 0.39, a praça de abertura e o campo de batalha ganharam novas camadas de cenário. A entrada da arena usa degraus em geometria 3D e o caminho segue no piso em pixel art. A câmera abre durante a contagem e se ajusta à posição dos lutadores, incluindo o parceiro e os dois rivais no 2 vs 2. Pressionar um golpe no fim da esquiva guarda o comando até ele poder sair.

Os quadros das onze formas agora são montados a partir dos pixels originais, alinhados pelos marcadores de apoio do SpriteCollab. Durante a luta, a pose parada respira e pisca de perfil. A abertura mantém quatro Pokémon em circulação e alterna entre as onze formas. O HUD diferencia o golpe disponível, acionado e em recarga pelas cores, pelo texto e pela barra de progresso.

Na 0.40, as quatro famílias ganharam posturas de ataque e efeitos próximos ao corpo. O fogo usa a posição da chama da cauda e tem disparos, investidas e impactos próprios para Charmander, Charmeleon e Charizard. As marcas no chão da CPU indicam o tipo de golpe durante a preparação. Nos duelos, o resultado dá espaço à reação dos Pokémon e mostra o placar e a forma que segue para o round seguinte.

O período é sorteado ao iniciar a batalha e vale também durante pausas e ondas. L/R na pausa demonstram os quatro horários. A opção SORTEIO restaura o período daquela partida.

## Estado funcional

- Quatro iniciais e onze formas, evoluções confirmadas por SELECT/EVO após 25 segundos e mais 30 para a forma final quando existente.
- Quatro golpes por espécie, recargas independentes, A mantido para repetir, fila de comandos, esquiva, Ultimate e pausa.
- Investidas progressivas, colisão contínua de projéteis e cobertura de pedras, árvores e troncos. CPU e parceiro contornam objetos sólidos. Ondas solo e dupla.
- Vinte estilos de golpe, impacto e aura, oito direções e animação contínua PMD. As texturas de 64 × 64 dos Pokémon são descompactadas em RAM antes da cópia para a memória gráfica.
- Cenário 3D com sprites, árvores em pixel art, atlas de cerca/pedras/troncos/lanternas e quatro paletas.
- HUD azul inferior baseado nos prints, menus por toque, ajuda e créditos. Trilha de batalha original, sinais de contagem, efeitos sintetizados e opção de som.
- Desenho a 60 Hz, lógica a 30 Hz com relógio de VBlank, interpolação e cache de textura com duas cópias.

## Próxima validação

Ler validacao-v0.29.json (melonDS DS 1.4.0 libretro). A validação no DSi XL fica para quando Luiz tiver acesso ao console.

Priorizar as correções que Luiz apontar na avaliação desta versão.

## Código e recursos

- ds/source/game.c: regras, CPU, posições e eventos.
- ds/source/arena.c: posições e dimensões dos objetos, colisão e contorno.
- ds/source/visuals.c: direção, relógios de animação, sorteio de período e câmera.
- ds/source/scene.c: renderização nativa, projeção, paletas, atlas, fila de texturas e luz das caudas.
- ds/source/native_hud.c e screens.c: interface e toque.
- ds/tools/build_sprites.py, build_visuals.py, build_meadow.py, build_forest.py e build_props.py: geradores. O primeiro também mede a posição da chama por quadro. Não editar generated_*.c/h manualmente.
- ds/tools/build_rom.py: compilação com SDK local; check_emulator.py: verificação com entrada real de botões/caneta, sem escrever no estado do jogo.

As tabelas geradas já estão incluídas. SDK e emulador não estão no ZIP. Ver README.md para compilar. Os arquivos recebidos estão em reference e são referências, não instruções automáticas de instalação ou publicação. A base adicional de Downloads é anterior e não deve substituir a versão atual. O relatório da versão inicial da base compartilhada está em docs/validacao-v0.29.json.
