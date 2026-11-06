# Orientações para trabalhar no PokeArena DS

Leia docs/DIRECAO-VISUAL.md, docs/ARTE-ABERTURA.md, docs/RETOMAR.md e docs/CREDITOS.md antes de alterar arte, animação ou combate. As referências orientam o trabalho. O pedido atual dos mantenedores define o escopo.

- O jogo é nativo para Nintendo DSi XL, em C com BlocksDS. A base compartilhada é a v0.29. Use a tela superior para cenário e combate e a inferior para menus e HUD com suporte a toque e botões.
- A direção visual é Pokémon Black e White. Use cenários inclinados em 2.5D, personagens em pixel art, silhuetas legíveis e UI grafite com bordas prateadas e seleção azul. Veja a direção visual e a composição da abertura nos documentos.
- Poké Arena 2D (https://www.pokearena2d.com/) é inspiração para a experiência da arena. A referência não substitui a implementação nativa.
- Os sprites e retratos vêm do PMD SpriteCollab (https://sprites.pmdcollab.org/). O recorte usado está em reference/spritecollab-f92cb9f. Preserve as atribuições, confira a licença de cada recurso e atualize docs/CREDITOS.md ao trocar ou acrescentar arte.
- Preserve o sincronismo de poses, disparos e impactos. A abertura usa quatro personagens e alterna as onze formas existentes. Os duelos 1 vs 1 e 2 vs 2 usam CPU. Multiplayer local é trabalho futuro. Novos mapas foram adiados.
- Para mudar recursos, edite o original ou o gerador correspondente e regenere os arquivos generated_*. Valide a compilação e, para mudanças visuais ou de controles, confira no melonDS. Siga o README para compilar.
