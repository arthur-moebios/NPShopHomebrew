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

## 🇬🇧 English

### Overview
NPShop is an **ecosystem** composed of:

1. **NPShop Homebrew** — a Switch-side client for browsing, downloading, installing, and managing your library directly from your console.  
2. **NPShop.org** — a web backend and dashboard connected to your Google Drive for game management, Tinfoil catalog generation, save backup/restore, and device authorization.

Together, they form a unified experience to **safely host, sync, and install your own legally owned backups**.

---

### 🔧 Features (Homebrew App)

#### 🎮 Game Library
- Browse, search, and install games, updates, and DLCs from your linked **NPShop.org** account.  
- Supports **NSP**, **XCI**, **NSZ**, and **XCZ** file formats.  
- Automatic **version detection** from the console (reads NACP metadata).  
- Displays *Installed* and *Available* versions side-by-side for clarity.  
- Built-in **update and DLC manager** with Google Drive integration.  

#### ☁️ Cloud Integration
- Uses **Google OAuth 2.0 device authorization** — link your Switch by scanning a QR code.  
- Direct, authenticated **Google Drive downloads** using downscoped access tokens (secure per-file access).  
- **Encrypted Tinfoil Index (.tfl)** generation supported by NPShop backend.  

#### 💾 Save Management
- Backup and restore **save data** to and from Google Drive.  
- Multiple slots and automatic cloud sync.  
- Compatible with all user profiles.  

#### ⚙️ File System Support
- Works seamlessly with **FAT32** and **exFAT** SD cards.  
- Smart segmented downloads for large (>4 GB) files with automatic stitching.  
- Auto-resume, error recovery, and integrity verification.  

#### 🧠 UX / UI
- Redesigned AppStore layout (grid + detail view).  
- Dynamic banner and icon loading (cached locally).  
- Smooth navigation, placeholder artwork, and responsive scaling for handheld/docked modes.  
- Multilingual interface (English / Portuguese).  

## Coming soon:
#### 🧩 Sysmodule Integration (NPShop Sysd)
- Optional **background download service**, keeping transfers running while in other apps or home menu.  
- Planned **Tesla overlay** to show download progress notifications.  

---

### 🌐 Features (NPShop.org Web Platform)

- **Personal dashboard** with your linked devices and Google account.  
- Upload and organize your legal backups directly from your browser.  
- **Game catalog** with automatic metadata parsing and cover image management.  
- **Save manager** — download or restore backups per game/user.  
- **Tinfoil index builder** — encrypted `.tfl` catalog generation with RSA-wrapped AES keys.  
- **Device management** — link or unlink Switch consoles via QR or device code.  
- Background processing (queue system) for indexing, encryption, and Drive operations.  
- **Google Drive downscoping** for secure, per-file download links.  

---

### ⚠️ Legal Disclaimer
> NPShop and its developers **do not condone piracy**.  
> This project is **solely intended for managing personal backups of games you legally own**.  
>  
> No copyrighted content, keys, or ROMs are hosted or distributed by NPShop.org or this homebrew.  
> Misuse of the software is the sole responsibility of the end user.  
>  
> Please comply with all copyright laws in your jurisdiction.

---

### 🧑‍💻 Credits
Original authors & major contributors:

- **ITotalJustice** — for *Sphaira*, the foundational project on which NPShop was built.  
- **borealis**, **stb**, **yyjson**, **nx-hbmenu**, **nx-hbloader**, **deko3d-nanovg**, **libpulsar**, **minIni**, **GBATemp**, **hb-appstore**, **haze**, **nxdumptool**, **Liam0**, **libusbhsfs**, **libnxtc**, **oss-nvjpg**, **nsz**, **themezer** — and everyone else whose open-source contributions made NPShop possible.

❤️ *NPShop continues the legacy of Sphaira with modern cloud integration, updated UI, and strong legal boundaries.*

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

### 🧑‍💻 Créditos
Autores e contribuidores originais:

- **ITotalJustice** — projeto *Sphaira*, base sobre a qual o NPShop foi desenvolvido.  
- **borealis**, **stb**, **yyjson**, **nx-hbmenu**, **nx-hbloader**, **deko3d-nanovg**, **libpulsar**, **minIni**, **GBATemp**, **hb-appstore**, **haze**, **nxdumptool**, **Liam0**, **libusbhsfs**, **libnxtc**, **oss-nvjpg**, **nsz**, **themezer** — e todos que contribuíram com código, ideias e bibliotecas open-source.

❤️ *O NPShop continua o legado do Sphaira, com integração à nuvem, interface moderna e foco em usabilidade e legalidade.*
### A NPShop é grátis e sempre será
Se desejar fazer uma doação ao projeto agradecemos desde já.

https://nubank.com.br/cobrar/p7qsa/69025bed-a3ac-44c7-9637-a11a58cbc32b

<img src="https://npshop.org/qrdoacao.png" alt="NPShop Doação" width="400" height="400"/>
