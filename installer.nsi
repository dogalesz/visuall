; ═══════════════════════════════════════════════════════════════════════════════
; Visuall — NSIS Windows Installer
;
; Build:  makensis installer.nsi
; Output: visuallc-v<version>-windows-x86_64-installer.exe
;
; Silent install:  installer.exe /S /D=C:\custom\path
; ═══════════════════════════════════════════════════════════════════════════════

!define PRODUCT_NAME    "Visuall"
!define PRODUCT_VERSION "1.3.3"
!define PRODUCT_PUB     "Visuall"
!define OUTFILE         "visuallc-v${PRODUCT_VERSION}-windows-x86_64-installer.exe"

Name    "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "${OUTFILE}"
InstallDir      "$PROGRAMFILES64\visuall"
InstallDirRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\visuall" "InstallLocation"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
ShowInstDetails   show
ShowUninstDetails show

; ── Modern UI (bundled with NSIS) ────────────────────────────────────────────
!include "MUI2.nsh"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; ── Install section ──────────────────────────────────────────────────────────

Section "Install"
  SetOutPath "$INSTDIR"

  ; Executables
  File "build\visuallc.exe"
  File /nonfatal "build\tools\visuall-lsp\visuall-lsp.exe"
  File "build\vslpkg.exe"

  ; DLLs — grab everything CMake copied (names vary across MSYS2 versions)
  File "build\*.dll"

  ; Bundled linker — ld.lld (LLVM) so users don't need MinGW installed
  File /nonfatal "build\ld.lld.exe"
  File /nonfatal "build\lld.exe"

  ; MinGW CRT startup objects + import libraries for the bundled linker
  SetOutPath "$INSTDIR\mingw_libs"
  File /nonfatal "build\mingw_libs\crt2.o"
  File /nonfatal "build\mingw_libs\crtbegin.o"
  File /nonfatal "build\mingw_libs\crtend.o"
  File /nonfatal "build\mingw_libs\*.a"

  SetOutPath "$INSTDIR"

  ; Documentation & examples
  File "README.md"
  File "LICENSE"
  File "CHANGELOG.md"
  File "examples\hello.vsl"

  ; Standard library
  SetOutPath "$INSTDIR\stdlib"
  File "stdlib\collections.vsl"
  File "stdlib\datetime.vsl"
  File "stdlib\io.vsl"
  File "stdlib\json.vsl"
  File "stdlib\math.vsl"
  File "stdlib\network.vsl"
  File "stdlib\random.vsl"
  File "stdlib\string.vsl"
  File "stdlib\sys.vsl"

  SetOutPath "$INSTDIR"

  ; ── Add to system PATH ──────────────────────────────────────────────────
  ; Read current PATH, append $INSTDIR
  ReadRegStr $0 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "Path"
  ${If} $0 != ""
    StrCpy $1 "$0;$INSTDIR"
    WriteRegExpandStr HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "Path" "$1"
  ${Else}
    WriteRegExpandStr HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "Path" "$INSTDIR"
  ${EndIf}

  ; Notify all windows that the environment changed
  SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=1000

  ; ── Write uninstaller ────────────────────────────────────────────────────
  WriteUninstaller "$INSTDIR\uninstall.exe"

  ; ── Add/Remove Programs registry entries ─────────────────────────────────
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\visuall" \
    "DisplayName"     "${PRODUCT_NAME} ${PRODUCT_VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\visuall" \
    "DisplayVersion"  "${PRODUCT_VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\visuall" \
    "Publisher"       "${PRODUCT_PUB}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\visuall" \
    "UninstallString" "$INSTDIR\uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\visuall" \
    "InstallLocation" "$INSTDIR"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\visuall" \
    "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\visuall" \
    "NoRepair" 1
SectionEnd

; ── Uninstall section ────────────────────────────────────────────────────────

Section "Uninstall"
  ; Remove executables
  Delete "$INSTDIR\visuallc.exe"
  Delete "$INSTDIR\visuall-lsp.exe"
  Delete "$INSTDIR\vslpkg.exe"

  ; Remove DLLs
  Delete "$INSTDIR\*.dll"

  ; Remove bundled linker
  Delete "$INSTDIR\ld.lld.exe"
  Delete "$INSTDIR\lld.exe"

  ; Remove bundled MinGW libraries
  Delete "$INSTDIR\mingw_libs\crt2.o"
  Delete "$INSTDIR\mingw_libs\crtbegin.o"
  Delete "$INSTDIR\mingw_libs\crtend.o"
  Delete "$INSTDIR\mingw_libs\*.a"
  RMDir  "$INSTDIR\mingw_libs"

  ; Remove docs
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\LICENSE"
  Delete "$INSTDIR\CHANGELOG.md"
  Delete "$INSTDIR\hello.vsl"

  ; Remove stdlib
  Delete "$INSTDIR\stdlib\collections.vsl"
  Delete "$INSTDIR\stdlib\datetime.vsl"
  Delete "$INSTDIR\stdlib\io.vsl"
  Delete "$INSTDIR\stdlib\json.vsl"
  Delete "$INSTDIR\stdlib\math.vsl"
  Delete "$INSTDIR\stdlib\network.vsl"
  Delete "$INSTDIR\stdlib\random.vsl"
  Delete "$INSTDIR\stdlib\string.vsl"
  Delete "$INSTDIR\stdlib\sys.vsl"
  RMDir  "$INSTDIR\stdlib"

  Delete "$INSTDIR\uninstall.exe"
  RMDir  "$INSTDIR"

  ; ── Remove from PATH ────────────────────────────────────────────────────
  ReadRegStr $0 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "Path"
  ${If} $0 != ""
    ; Build new PATH without our directory
    StrCpy $1 ""
    ${Do}
      ; Extract next semicolon-delimited entry
      StrCpy $2 $0 1
      ${If} $2 == ";"
        StrCpy $0 $0 "" 1               ; skip leading semicolon
        ${Continue}
      ${EndIf}
      StrCpy $2 $0 1
      ${If} $2 == ""
        ${ExitDo}
      ${EndIf}
      ; Find the next semicolon or end
      StrLen $3 $0
      StrCpy $4 0
      ${Do}
        StrCpy $2 $0 1 $4
        ${If} $2 == ";"
        ${OrIf} $4 >= $3
          ${ExitDo}
        ${EndIf}
        IntOp $4 $4 + 1
      ${Loop}
      ; $4 is the length of this entry (or end of string)
      StrCpy $2 $0 $4                   ; this entry
      ${If} $2 != "$INSTDIR"
        ${If} $1 != ""
          StrCpy $1 "$1;$2"
        ${Else}
          StrCpy $1 "$2"
        ${EndIf}
      ${EndIf}
      IntOp $4 $4 + 1                   ; skip past entry + separator
      StrCpy $0 $0 "" $4
      StrLen $3 $0
      ${If} $3 == 0
        ${ExitDo}
      ${EndIf}
    ${Loop}
    WriteRegExpandStr HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "Path" "$1"
  ${EndIf}

  ; Notify system
  SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=1000

  ; Remove registry key
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\visuall"
SectionEnd
