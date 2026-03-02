/* instalador_c.c */
/* Instalador da linguagem JP - Baixa a ultima tag do GitHub e instala no sistema */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Detecta Sistema Operacional */
#ifdef _WIN32
    #include <windows.h>
    #include <shellapi.h>
    #include <direct.h>
    #define IS_WINDOWS 1
    #define PATH_SEP "\\"
#else
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #define IS_WINDOWS 0
    #define PATH_SEP "/"
#endif

/* URL da API do GitHub - Usa endpoint de tags ao inves de releases */
#define GITHUB_TAGS_URL "https://api.github.com/repos/Joaopaulopadilha/JP_BR/tags"
#define GITHUB_ZIP_BASE "https://github.com/Joaopaulopadilha/JP_BR/archive/refs/tags/"

void run(const char *cmd) {
    printf(">> %s\n", cmd);
    int ret = system(cmd);
    if (ret != 0) {
        // Apenas avisa, nao aborta, pois rmdir pode falhar se pasta nao existir
        fprintf(stderr, "(Comando retornou codigo %d - continuando...)\n", ret);
    }
}

/* Verifica se e Admin */
int check_admin() {
    #ifdef _WIN32
        return system("net session >nul 2>&1") == 0;
    #else
        return geteuid() == 0;
    #endif
}

/* Tenta elevar privilegios no Windows */
void elevate_windows() {
    #ifdef _WIN32
    char path[MAX_PATH];
    if (GetModuleFileName(NULL, path, MAX_PATH)) {
        printf("Tentando reiniciar como Administrador...\n");
        // O verbo "runas" solicita o UAC
        HINSTANCE ret = ShellExecute(NULL, "runas", path, NULL, NULL, SW_SHOWNORMAL);
        
        // Se ret > 32, funcionou e a nova janela abriu. Fechamos esta.
        if ((intptr_t)ret > 32) {
            exit(0);
        } else {
            fprintf(stderr, "Falha ao solicitar privilegios de Administrador.\n");
            fprintf(stderr, "Por favor, clique com o botao direito e 'Executar como Administrador'.\n");
            system("pause");
            exit(1);
        }
    }
    #endif
}

void install_linux() {
    printf("--- Buscando ultima tag no GitHub (Linux) ---\n");

    /* Pega o nome da ultima tag via API e baixa o zipball correspondente */
    char download_cmd[1024];
    sprintf(download_cmd,
        "TAG=$(curl -s %s | grep '\"name\"' | head -1 | cut -d '\"' -f 4) && "
        "echo \"Ultima tag: $TAG\" && "
        "curl -L -o /tmp/jp_latest.zip \"%s${TAG}.zip\"",
        GITHUB_TAGS_URL, GITHUB_ZIP_BASE);
    run(download_cmd);

    printf("--- Instalando ---\n");
    run("apt install -y unzip");
    
    run("rm -rf /tmp/jp_install_temp");
    run("mkdir -p /tmp/jp_install_temp");
    run("unzip -o /tmp/jp_latest.zip -d /tmp/jp_install_temp");

    /* Limpeza Versao Anterior */
    run("rm -rf /usr/local/share/jp");

    /* Instalação - entra na subpasta criada pelo GitHub (ex: JP_BR-3.0/) */
    run("mkdir -p /usr/local/share/jp");
    run("SUB=$(ls -d /tmp/jp_install_temp/*/ 2>/dev/null | head -1) && "
        "if [ -n \"$SUB\" ]; then cp -r \"$SUB\"* /usr/local/share/jp/; "
        "else cp -r /tmp/jp_install_temp/* /usr/local/share/jp/; fi");

    /* Permissoes - apenas o interpretador/compilador principal */
    if (access("/usr/local/share/jp/jp.elf", F_OK) == 0) {
        run("chmod +x /usr/local/share/jp/jp.elf");
    } else {
        run("chmod +x /usr/local/share/jp/jp");
    }

    /* Script Desinstalacao */
    FILE *uninstall = fopen("/usr/local/share/jp/desinstalar-jp.sh", "w");
    if (uninstall) {
        fprintf(uninstall,
            "#!/bin/bash\n"
            "rm -f /usr/local/bin/jp\n"
            "rm -rf /usr/local/share/jp\n"
            "echo \"JP desinstalado.\"\n"
        );
        fclose(uninstall);
        run("chmod +x /usr/local/share/jp/desinstalar-jp.sh");
    }

    /* Launcher */
    FILE *launcher = fopen("/usr/local/bin/jp", "w");
    if (launcher) {
        fprintf(launcher,
            "#!/bin/bash\n"
            "JP_ROOT=\"/usr/local/share/jp\"\n"
            "\n"
            "if [ -f \"$JP_ROOT/jp.elf\" ]; then\n"
            "    JP_BIN=\"$JP_ROOT/jp.elf\"\n"
            "else\n"
            "    JP_BIN=\"$JP_ROOT/jp\"\n"
            "fi\n"
            "\n"
            "if [[ \"$1\" == \"desinstalar\" ]]; then\n"
            "    sudo \"$JP_ROOT/desinstalar-jp.sh\"\n"
            "    exit 0\n"
            "fi\n"
            "\n"
            "exec \"$JP_BIN\" \"$@\"\n"
        );
        fclose(launcher);
        run("chmod +x /usr/local/bin/jp");
    }

    run("rm /tmp/jp_latest.zip");
    run("rm -rf /tmp/jp_install_temp");
}

void install_windows() {
    printf("--- Buscando ultima tag no GitHub (Windows) ---\n");

    /* Pega o nome da ultima tag via API e baixa o zip do codigo-fonte dessa tag */
    char ps_cmd[2048];
    sprintf(ps_cmd,
        "powershell -command \""
        "$tag = (Invoke-RestMethod '%s')[0].name; "
        "Write-Host 'Ultima tag:' $tag; "
        "$url = '%s' + $tag + '.zip'; "
        "Write-Host 'Baixando:' $url; "
        "Invoke-WebRequest -Uri $url -OutFile $env:TEMP\\jp_latest.zip"
        "\"",
        GITHUB_TAGS_URL, GITHUB_ZIP_BASE);
    run(ps_cmd);

    printf("--- Instalando ---\n");
    
    // Limpa temp de extracao
    char rm_temp[512];
    sprintf(rm_temp, "rmdir /S /Q \"%%TEMP%%\\jp_temp_install\" 2>nul");
    system(rm_temp);
    
    // Unzip para TEMP
    char unzip_cmd[512];
    sprintf(unzip_cmd, "powershell -command \"Expand-Archive -Force $env:TEMP\\jp_latest.zip $env:TEMP\\jp_temp_install\"");
    run(unzip_cmd);

    /* --- LIMPEZA DA VERSAO ANTERIOR --- */
    printf("Removendo versao anterior...\n");
    system("rmdir /S /Q \"C:\\Program Files\\JP\" 2>nul");
    /* ---------------------------------- */

    system("mkdir \"C:\\Program Files\\JP\" 2>nul");

    // Copia do TEMP para Program Files (entra na subpasta criada pelo GitHub, ex: JP_BR-3.0)
    char copy_cmd[1024];
    sprintf(copy_cmd,
        "powershell -command \""
        "$sub = Get-ChildItem $env:TEMP\\jp_temp_install | Where-Object { $_.PSIsContainer } | Select-Object -First 1; "
        "if ($sub) { xcopy /E /Y ($sub.FullName + '\\*') 'C:\\Program Files\\JP\\' } "
        "else { xcopy /E /Y $env:TEMP\\jp_temp_install\\* 'C:\\Program Files\\JP\\' }"
        "\"");
    run(copy_cmd);

    /* Script Desinstalacao INTELIGENTE (Pede Admin sozinho) */
    FILE *uninstall = fopen("C:\\Program Files\\JP\\desinstalar-jp.cmd", "w");
    if (uninstall) {
        fprintf(uninstall,
            "@echo off\n"
            "REM --- Auto-Elevacao para Admin ---\n"
            "net session >nul 2>&1\n"
            "if %%errorLevel%% neq 0 (\n"
            "    echo Solicitando permissao de Administrador...\n"
            "    powershell -Command \"Start-Process '%%~f0' -Verb RunAs\"\n"
            "    exit /b\n"
            ")\n"
            "REM -------------------------------\n"
            "echo Desinstalando JP...\n"
            "del /F /Q \"C:\\Windows\\jp.cmd\"\n"
            "rmdir /S /Q \"C:\\Program Files\\JP\"\n"
            "echo JP desinstalado com sucesso.\n"
            "pause\n"
        );
        fclose(uninstall);
    }

    /* Launcher */
    FILE *launcher = fopen("C:\\Windows\\jp.cmd", "w");
    if (launcher) {
        fprintf(launcher,
            "@echo off\n"
            "setlocal\n"
            "\n"
            "set \"JP_HOME=C:\\Program Files\\JP\"\n"
            "\n"
            "if \"%%1\"==\"desinstalar\" (\n"
            "    call \"%%JP_HOME%%\\desinstalar-jp.cmd\"\n"
            "    exit /b\n"
            ")\n"
            "\n"
            "set \"EXE=%%JP_HOME%%\\jp.exe\"\n"
            "\n"
            "if not exist \"%%EXE%%\" (\n"
            "    echo Erro: jp.exe nao encontrado em %%JP_HOME%%\n"
            "    exit /b 1\n"
            ")\n"
            "\n"
            "REM Apenas repassa todos os argumentos exatamente como digitados\n"
            "\"%%EXE%%\" %%*\n"
        );
        fclose(launcher);
    }

    // Limpeza final
    system("del \"%TEMP%\\jp_latest.zip\"");
    system("rmdir /S /Q \"%TEMP%\\jp_temp_install\"");
}

int main() {
    if (!check_admin()) {
        #ifdef _WIN32
            // Se for Windows, tenta se elevar automaticamente
            elevate_windows();
            return 0; // Se a elevacao funcionar, esta instancia fecha e a nova abre
        #else
            // No Linux, ainda e melhor pedir sudo explicitamente no terminal
            fprintf(stderr, "Este instalador precisa ser executado como root.\n");
            fprintf(stderr, "Execute: sudo ./instalador_jp_linux\n");
            return 1;
        #endif
    }

    #ifdef _WIN32
        install_windows();
    #else
        install_linux();
    #endif

    printf("\nJP Instalado/Atualizado com sucesso!\n");
    #ifdef _WIN32
        system("pause");
    #endif
    return 0;
}