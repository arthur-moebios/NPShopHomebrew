<p align="center">
  <img src="https://npshop.org/public/assets/logo.png" alt="NPShop Logo" width="400" height="400"/>
</p>

<h1 align="center">🧩 NPShop</h1>
<p align="center">
  A <b>Homebrew Library Manager</b> for the Nintendo Switch — integrated with <a href="https://npshop.org">NPShop.org</a><br/>
  Cloud-powered library, save management, and encrypted Tinfoil index builder.
</p>

<p align="center">
  <a href="https://github.com/arthur-moebios/NPShopHomebrew/releases">
    <img src="https://img.shields.io/github/v/release/arthur-moebios/NPShopHomebrew?style=flat-square&color=brightgreen" alt="Latest Release">
  </a>
  <a href="https://github.com/arthur-moebios/NPShopHomebrew/actions">
    <img src="https://img.shields.io/github/actions/workflow/status/arthur-moebios/NPShopHomebrew/build.yml?style=flat-square&label=Build" alt="Build Status">
  </a>
  <a href="https://github.com/arthur-moebios/NPShopHomebrew/blob/main/LICENSE">
    <img src="https://img.shields.io/github/license/arthur-moebios/NPShopHomebrew?style=flat-square&color=blue" alt="License">
  </a>
  <img src="https://img.shields.io/badge/platform-Nintendo%20Switch-orange?style=flat-square" alt="Platform">
</p>

---

## 🇧🇷 Português (Brasil)

### Visão Geral
O **NPShop** é um **ecossistema** formado por:

1. **NPShop Homebrew** — aplicativo para o Nintendo Switch que permite navegar, baixar, instalar e gerenciar sua biblioteca diretamente no console;  
2. **NPShop.org** — painel web integrado ao Google Drive para gerenciar jogos, saves, catálogos Tinfoil e autenticação de dispositivos.

Juntos, eles oferecem uma experiência completa para **hospedar, sincronizar e instalar seus próprios backups legais**.

---

### 🔧 Funções (Aplicativo Homebrew)

#### 🎮 Biblioteca de Jogos
- Navegue e instale jogos, updates e DLCs da sua conta **NPShop.org**.  
- Suporte a arquivos **.NSP**, **.XCI**, **.NSZ** e **.XCZ**.  
- Detecta automaticamente a **versão instalada** lendo os metadados *NACP*.  
- Exibe lado a lado as versões “Instalada” e “Disponível”.  
- Gerenciador interno de **atualizações e DLCs** integrado ao Google Drive.  

#### ☁️ Integração com a Nuvem
- Login seguro via **Google OAuth (Device Code / QR)** diretamente no Switch.  
- Downloads diretos do **Google Drive** com tokens reduzidos (downscoped) por arquivo.  
- Geração de **índices Tinfoil encriptados (.tfl)** pelo servidor NPShop.  

#### 💾 Gerenciamento de Saves
- Backup e restauração de **dados salvos** para o Google Drive.  
- Suporte a múltiplos slots e sincronização automática.  
- Compatível com todos os perfis de usuário.  

#### ⚙️ Suporte ao Sistema de Arquivos
- Compatível com **FAT32** e **exFAT**.  
- Baixador inteligente com divisão automática de arquivos grandes (> 4 GB).  
- Retomada automática e verificação de integridade após falhas.  

#### 🧠 Interface e Experiência
- Layout da AppStore totalmente redesenhado (modo grade + detalhes).  
- Ícones e banners dinâmicos com cache local.  
- Arte substituta (placeholder) com gradiente e tipografia aprimorada.  
- Interface multilíngue (Inglês e Português).

## Em breve:
#### 🧩 Integração Sysmodule (NPShop Sysd)
- Serviço opcional de **downloads em segundo plano**, permitindo continuar transferências enquanto o usuário sai do app.  
- Suporte planejado para **Tesla Overlay** exibindo progresso.  

---

### 🌐 Funções (Painel Web — NPShop.org)

- **Painel pessoal** com dispositivos e contas Google vinculadas.  
- Upload e organização de backups diretamente no navegador.  
- **Catálogo de jogos** com leitura automática de metadados e gerenciamento de capas.  
- **Gerenciador de saves** — download e restauração de backups por jogo/usuário.  
- **Gerador de catálogo Tinfoil** encriptado (.tfl) com chaves RSA + AES.  
- **Gerenciamento de dispositivos** com autenticação por QR ou código.  
- Sistema de tarefas em segundo plano para indexação, criptografia e operações no Drive.  
- **Tokens downscoped** para downloads seguros e específicos por arquivo.  

---

### ⚠️ Aviso Legal
> O NPShop **não apoia pirataria**.  
> Este projeto é destinado **somente ao gerenciamento de backups de jogos que você possui legalmente**.  
>  
> Nenhum conteúdo protegido por direitos autorais é hospedado ou distribuído pelo NPShop.org ou pelo aplicativo.  
> O uso indevido é de total responsabilidade do usuário final.  
>  
> Respeite as leis de direitos autorais vigentes no seu país.

---

### Agradecimentos

🙏 Um enorme obrigado ao [@CostelaCNX](https://github.com/CostelaCNX) por incluir o NPShop no CNX Pack 20.5.0-1!
Isso torna ainda mais fácil ter o app atualizado direto no seu Switch.

### 🧑‍💻 Créditos
Autores e contribuidores originais:

- **ITotalJustice** — projeto *Sphaira*, base sobre a qual o NPShop foi desenvolvido.  
- **borealis**, **stb**, **yyjson**, **nx-hbmenu**, **nx-hbloader**, **deko3d-nanovg**, **libpulsar**, **minIni**, **GBATemp**, **hb-appstore**, **haze**, **nxdumptool**, **Liam0**, **libusbhsfs**, **libnxtc**, **oss-nvjpg**, **nsz**, **themezer** — e todos que contribuíram com código, ideias e bibliotecas open-source.

### A NPShop é grátis e sempre será
Se desejar fazer uma doação ao projeto agradecemos desde já.

https://npshop.org/contribute

[<img src="https://npshop.org/qrdoacao.png" alt="NPShop Doação" width="400" height="400"/>](https://npshop.org/contribute)
