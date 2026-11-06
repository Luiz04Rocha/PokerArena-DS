# PokeArena DS — base v0.29

Extraia o ZIP e use esta pasta como raiz do repositório privado. Cada pessoa deve clonar o mesmo repositório e trabalhar em uma branch por tarefa.

## Compilar a ROM

Instale o Python e o ambiente Wonderful com BlocksDS em cada máquina. Na raiz do projeto, rode:

    python ds/tools/build_rom.py --wonderful "C:/caminho/para/wonderful"

A ROM sai em ds/arena_dupla_dsi.nds. Os arquivos gerados em ds/source e ds/include fazem parte do projeto e já estão prontos para compilar. Para editar recursos, instale as dependências de ds/tools/requirements.txt. A pasta reference contém os recortes de arte usados pelos geradores. Os créditos e licenças estão em docs.

## Trabalhar em duas máquinas

Antes de começar, atualize sua cópia. Crie uma branch, teste sua ROM no melonDS e envie suas mudanças para revisão. Inclua no mesmo commit a arte editada e o recurso nativo regenerado.

## Trabalhar com IA

Peça à IA para ler AGENTS.md na raiz antes de editar. O arquivo aponta para as referências, créditos e decisões de design.

## Revisão em equipe

Usem a branch main como base estável. Atualizem main antes de abrir uma branch nova. Antes de trabalhar no mesmo menu, cenário ou gerador, combinem quem vai editar cada arquivo. Abram um pull request por tarefa e indiquem o que mudou, como testar e se algum recurso foi regenerado. Para mudanças visuais, anexem a captura ao pull request.

Quando duas branches alterarem o mesmo recurso gerado, resolvam a edição no arquivo-fonte ou no gerador e gerem o recurso novamente. Incluam os créditos da arte junto da mudança. Cada máquina instala seu próprio SDK e emulador.

O relatório docs/validacao-v0.29.json identifica a ROM de referência. Para rodar os testes de lógica, use python ds/tools/run_tests.py --cc gcc com um compilador C disponível. O script ds/tools/check_emulator.py usa um núcleo libretro local, a ROM recém-compilada e o arm-none-eabi-nm. Execute com --help para ver os caminhos aceitos.
