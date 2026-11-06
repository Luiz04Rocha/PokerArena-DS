# Créditos e procedência



Os sprites e retratos da ROM vieram do projeto enviado no ZIP. Foram convertidos para tiles/paletas do Nintendo DS; o conteúdo visual original e os créditos foram preservados. O protótipo usa os quatro iniciais e suas primeiras evoluções.



O Poké Arena 2D é referência de design: https://www.pokearena2d.com/ . Não foram copiados código ou arquivos desse site.



A ROM inclui componentes do SDK BlocksDS, libnds, picolibc e o núcleo ARM7 com Maxmod. Textos de licença fornecidos pelo SDK estão em `licencas-sdk/`. Código-fonte do SDK: https://codeberg.org/blocksds/sdk . Toolchain: https://wonderful.asie.pl/ .



Os créditos abaixo foram recebidos junto com os arquivos; as atribuições/licenças são preservadas conforme constavam no material original. Os cenários 3D e fontes citados pertencem à referência web e não são usados pela ROM atual.



---



Arena Dupla: Guardiões do Vale — créditos dos recursos incluídos



Sprites e retratos (PMDCollab SpriteCollab, licença CC BY-NC 4.0 — uso não comercial). Autores por espécie:



- Bulbasaur (#0001): sprites CHUNSOFT; portrait CHUNSOFT

- Ivysaur (#0002): sprites CHUNSOFT; portrait CHUNSOFT, Emmuffin

- Venusaur (#0003): sprites CHUNSOFT; portrait CHUNSOFT, <@!276175146304405514>, G〜

- Charmander (#0004): sprites CHUNSOFT; portrait CHUNSOFT, Emboarger, baronessfaron

- Charmeleon (#0005): sprites CHUNSOFT, <@!199989579682414592>, bwappi, Grimlin; portrait CHUNSOFT, 0palite

- Charizard (#0006): sprites CHUNSOFT, Emboarger; portrait CHUNSOFT, 0palite

- Squirtle (#0007): sprites CHUNSOFT; portrait CHUNSOFT

- Wartortle (#0008): sprites CHUNSOFT; portrait CHUNSOFT, Emmuffin

- Blastoise (#0009): sprites CHUNSOFT; portrait CHUNSOFT, Emmuffin, Top_Kec

- Pidgey (#0016): sprites CHUNSOFT, JFain; portrait CHUNSOFT, JFain

- Rattata (#0019): sprites CHUNSOFT; portrait CHUNSOFT, Frostdrop1

- Pikachu (#0025): sprites CHUNSOFT; portrait CHUNSOFT

- Raichu (#0026): sprites CHUNSOFT, Grimlin, Top_Kec; portrait CHUNSOFT, C_Pariah, 3Monika4, G〜

- Zubat (#0041): sprites CHUNSOFT; portrait CHUNSOFT, fledermaus

- Machop (#0066): sprites CHUNSOFT, Emboarger; portrait CHUNSOFT, Felis-Licht, Morei

- Geodude (#0074): sprites CHUNSOFT; portrait CHUNSOFT, JustAGunk

- Gastly (#0092): sprites CHUNSOFT; portrait CHUNSOFT, JFain

- Mewtwo (#0150): sprites CHUNSOFT, JFain; portrait CHUNSOFT, baronessfaron



Cenário 3D:

- Quaternius, Ultimate Stylized Nature Pack (CC0)

- Kay Lousberg, KayKit (CC0)

- Texturas: ambientCG (CC0)

- Fonte: ManusCC0 (veja fonts/ManusCC0-LICENSE.txt)



Projeto de fã, sem fins lucrativos, sem afiliação. Pokémon e os nomes dos personagens são marcas da Nintendo, Game Freak e Creatures Inc.

Todos os efeitos sonoros e músicas são sintetizados por código.



Revisão nativa 0.5: os vinte estilos de golpe, impacto e aura foram desenhados por código em `ds/tools/build_visuals.py`. A reprodução das animações e os movimentos adicionais dos Pokémon foram implementados em `visuals.c` e `scene.c`, preservando os desenhos e créditos PMD acima. As malhas Rock_1, Rock_3, NormalTree_5 e PineTree_2 foram reduzidas a partir do pacote de natureza recebido. O HUD foi adaptado dos prints enviados pelo Luiz.



Revisão 0.6: os quadros de ataque/dano foram reconvertidos em 64 × 64 a partir dessas mesmas folhas PMD, preservando seus créditos. Não foram criados desenhos novos dos Pokémon.


Versão 0.8: chão original produzido por build_meadow.py; flores, capim, cerca e troncos feitos com geometria nativa. Árvores e pedras mantêm os créditos dos modelos recebidos.

0.9: árvore em pixel art original, construída por agrupamentos de folhas e paleta fixa. Imagens Black/White enviadas pelo usuário serviram como referência visual, sem extração de seus gráficos.

0.11: cerca, pedras, tronco e lanterna são pixel art original do gerador build_props.py. Substituem as malhas anteriores desses objetos na ROM, com inspiração visual nas referências enviadas pelo usuário. Os créditos dos desenhos PMD dos Pokémon permanecem.

Importação direta do SpriteCollab conferida na [revisão f92cb9f](https://github.com/PMDCollab/SpriteCollab/tree/f92cb9fedb8803714a69b77bb47925c1f7b816b5). As folhas, os tempos e os retratos das onze formas coincidem com os arquivos recebidos. O pacote de importação inclui os créditos por espécie e os arquivos originais de alinhamento. A [política do projeto](https://github.com/PMDCollab/SpriteCollab/blob/master/README.md#submission-and-use-policy) pede atribuição e uso não comercial das contribuições.

Na 0.17, as onze formas são geradas de `reference/spritecollab-f92cb9f`, com os créditos originais em cada pasta. O gerador adapta escala, paleta e formato para o DS. Ele usa o ponto branco de apoio da folha de sombra e o ponto da cabeça da folha de posições, conforme o [leitor do SpriteBot](https://github.com/PMDCollab/SpriteBot/blob/master/SpriteUtils.py).

Na 0.18, as folhas Hop, Faint e Sleep vêm da mesma revisão do SpriteCollab. As lágrimas, estrelas e ondas de impacto são desenhadas pela ROM.
