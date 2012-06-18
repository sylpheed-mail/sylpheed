;--------------------------------

!include "sylpheed-defs.nsh"

SetCompressor /SOLID lzma

;--------------------------------

!include "nsProcess.nsh"

!include "MUI.nsh"
!include "Sections.nsh"
!include "FileFunc.nsh"

; MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install-blue.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall-blue.ico"

; Welcome page
;!insertmacro MUI_PAGE_WELCOME
; License page
;!define MUI_LICENSEPAGE_RADIOBUTTONS
;!insertmacro MUI_PAGE_LICENSE $(license)
; Components page
;!insertmacro MUI_PAGE_COMPONENTS
; Directory page
;!insertmacro MUI_PAGE_DIRECTORY
; Instfiles page
!insertmacro MUI_PAGE_INSTFILES
; Finish page
;!define MUI_FINISHPAGE_RUN "$INSTDIR\sylpheed.exe"
;!define MUI_FINISHPAGE_RUN_NOTCHECKED
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\README.txt"
;!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\README-win32.txt"
;!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\README-win32-ja.txt"
!define MUI_FINISHPAGE_SHOWREADME_FUNCTION "ShowReadme"
!define MUI_FINISHPAGE_SHOWREADME_TEXT "$(readme)"
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED
!define MUI_FINISHPAGE_LINK "$(jump)"
!define MUI_FINISHPAGE_LINK_LOCATION ${PRODUCT_WEB_SITE}
!insertmacro MUI_PAGE_FINISH

; Language files
!insertmacro MUI_LANGUAGE "English" # ${LANG_ENGLISH}
!insertmacro MUI_LANGUAGE "Spanish" # ${LANG_SPANISH}
!insertmacro MUI_LANGUAGE "Japanese" # ${LANG_JAPANESE}

!include "English.nsh"
!include "Spanish.nsh"
!include "Japanese.nsh"
;--------------------------------

Caption "$(^Name) Update Manager"
Name "${PRODUCT_NAME}"
OutFile "update-manager.exe"
InstallDir "$PROGRAMFILES\${PRODUCT_NAME}"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""

RequestExecutionLevel admin

ShowInstDetails hide
XPStyle on
BrandingText "${INST_NAME} ${INST_VERSION}"

!define TEMP_INSTALLER_PATH  "$TEMP\${PRODUCT_NAME}_setup.exe"

Section "Download"
;  SetOutPath "$TEMP"

  ${GetParameters} $R0
  ${GetOptions} "$R0" "/uri" $1

  StrCmp "$1" "" 0 uri_option_found
    DetailPrint "usage: update-manager.exe /uri 'http://example.com/Sylpheed-VER_setup.exe'"
    DetailPrint "or update-manager.exe /uri path\to\Sylpheed-VER_setup.exe"
    Abort
  uri_option_found:

  IfFileExists "$1" copy_file

  DetailPrint "Downloading $1"
  NSISdl::download \
    /TRANSLATE2 "$(downloading)" "$(connecting)" "$(second)" "$(minute)" "$(hour)" "$(seconds)" "$(minutes)" "$(hours)" "$(progress)" \
    /TIMEOUT=30000 "$1" "${TEMP_INSTALLER_PATH}"
  Pop $0
  StrCmp "$0" "success" download_ok
  DetailPrint "$(download_failed) $0"
  Abort


  copy_file:
  CopyFiles "$1" "${TEMP_INSTALLER_PATH}"
  Goto download_ok

  download_ok:
SectionEnd

Section "Exec Installer"
  ${nsProcess::FindProcess} "sylpheed.exe" $R0
  StrCmp $R0 "0" 0 +3
  MessageBox MB_ICONQUESTION|MB_YESNO "$(kill_and_update_confirm)" /SD IDYES IDYES +2
  Abort

  ExecWait "$INSTDIR\sylpheed.exe --exit" $R0

  ; Wait for sylpheed.exe quit completely
  ${nsProcess::FindProcess} "sylpheed.exe" $R0
  StrCmp $R0 "0" 0 wait_done
  Sleep 1000
  ${nsProcess::FindProcess} "sylpheed.exe" $R0
  StrCmp $R0 "0" 0 wait_done
  Sleep 1000
  ${nsProcess::FindProcess} "sylpheed.exe" $R0
  StrCmp $R0 "0" 0 wait_done
  Sleep 1000

  wait_done:
  ExecWait '"${TEMP_INSTALLER_PATH}" /S' $0
SectionEnd

Function ShowReadme
  ExecShell open "$INSTDIR\README.txt"
FunctionEnd
