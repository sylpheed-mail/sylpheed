; Dependencies:
; - nsisunz.dll Plug-in
; - InstallOptions.dll Plug-in
;
; usage: plugin-updater.exe /ini 'path\to\some-install-options.ini'
;--------------------------------

!include "sylpheed-defs.nsh"

SetCompressor /SOLID lzma

;--------------------------------

!include "MUI.nsh"
!include "Sections.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"

; location of nsisunz.dll
!addplugindir "."

; MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install-blue.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall-blue.ico"

;--------------------------------

!define INSTALL_OPTIONS_INI "$R2"
!define TEMP1 $R3 ;Temp variable

OutFile "plugin-updater.exe"
Name "${PRODUCT_NAME} Plugin Updater"
Caption "$(^Name)"
ShowInstDetails show
CompletedText "$(plugin_updater_completed_text)"
BrandingText "${INST_NAME} ${INST_VERSION}"

; $INSTDIR
InstallDir "$PROGRAMFILES\${PRODUCT_NAME}"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""

;Things that need to be extracted on startup (keep these lines before any File command!)
;Only useful for BZIP2 compression
;Use ReserveFile for your own InstallOptions INI files too!

ReserveFile "${NSISDIR}\Plugins\InstallOptions.dll"

RequestExecutionLevel admin
XPStyle on

;Order of pages
Page custom SetCustom ValidateCustom ": Select update plugins" ;Custom page. InstallOptions gets called in SetCustom.
!define MUI_PAGE_HEADER_TEXT "$(plugin_updater_extracting)"
!define MUI_PAGE_HEADER_SUBTEXT "$(plugin_updater_extracting_files)"
!define MUI_INSTFILESPAGE_FINISHHEADER_TEXT "$(plugin_updater_extracted)"
!define MUI_INSTFILESPAGE_FINISHHEADER_SUBTEXT "$(plugin_updater_extracted_description)"
!define MUI_INSTFILESPAGE_ABORTHEADER_TEXT "$(plugin_updater_extraction_aborted)"
!define MUI_INSTFILESPAGE_ABORTHEADER_SUBTEXT "$(plugin_updater_extraction_aborted_description)"
; Instfiles page
!insertmacro MUI_PAGE_INSTFILES

;--------------------------------
; Language files
!insertmacro MUI_LANGUAGE "English" # ${LANG_ENGLISH}
!insertmacro MUI_LANGUAGE "Spanish" # ${LANG_SPANISH}
!insertmacro MUI_LANGUAGE "Japanese" # ${LANG_JAPANESE}

!include "English.nsh"
!include "Spanish.nsh"
!include "Japanese.nsh"
;--------------------------------

Var field
Var NumFields
Var URL
Var basename
Var name
Var archive

Section "Components"

  ;Get Install Options dialog user input

  ;DetailPrint "TEMP=$TEMP"

  ReadINIStr $NumFields "${INSTALL_OPTIONS_INI}" "Settings" "NumFields"
  ;DetailPrint "NumFields=$NumFields"

  StrCpy $field 1
  ${While} $field < $NumFields ; >
    IntOp $field $field + 1
    ReadINIStr ${TEMP1} "${INSTALL_OPTIONS_INI}" "Field $field" "State"
    StrCmp ${TEMP1} 0 skip
    ReadINIStr $URL "${INSTALL_OPTIONS_INI}" "Field $field" "URL"
    ReadINIStr $basename "${INSTALL_OPTIONS_INI}" "Field $field" "basename"
    ReadINIStr $name "${INSTALL_OPTIONS_INI}" "Field $field" "name"
    ;DetailPrint "Field $field State=${TEMP1}"
    ;DetailPrint "Field $field URL=$URL"
    ;DetailPrint "Field $field basename=$basename"
    ;DetailPrint "Field $field name=$name"
    StrCpy $archive "$TEMP\sylpheed-plugin-$basename.zip"

    DetailPrint "$name:"
    DetailPrint "Downloading from $URL"
    DetailPrint " to $archive"
    NSISdl::download \
      /TRANSLATE2 "$(downloading)" "$(connecting)" "$(second)" "$(minute)" "$(hour)" "$(seconds)" "$(minutes)" "$(hours)" "$(progress)" \
      /TIMEOUT=30000 "$URL" "$archive"
    Pop $0
    StrCmp "$0" "success" download_ok
    DetailPrint "$(download_failed) $0"
    Abort
    download_ok:

    SetOutPath "$INSTDIR"
    nsisunz::UnzipToLog /text "$(plugin_updater_nsisunz_text)" "$archive" "$INSTDIR"

    ; Always check for errors. Everything else than "success" means an error.
    Pop $0
    StrCmp $0 "success" extract_ok
      Abort "$0"
    extract_ok:

    skip:
  ${EndWhile}

SectionEnd


Function .onInit
  InitPluginsDir

  ${GetParameters} $R0
  ${GetOptions} "$R0" "/ini" "${INSTALL_OPTIONS_INI}"

  StrCmp "${INSTALL_OPTIONS_INI}" "" 0 ini_option_found
    MessageBox MB_ICONEXCLAMATION|MB_OK "usage: plugin-updater.exe /ini 'path\to\some-install-options.ini'"
    Abort
  ini_option_found:
FunctionEnd

Function SetCustom
  ;Display the InstallOptions dialog

  !insertmacro MUI_HEADER_TEXT "$(plugin_updater_header)" "$(plugin_updater_header_description)"

  Push ${TEMP1}

    InstallOptions::dialog "${INSTALL_OPTIONS_INI}"
    Pop ${TEMP1}

  Pop ${TEMP1}

FunctionEnd

Function ValidateCustom

  ReadINIStr $NumFields "${INSTALL_OPTIONS_INI}" "Settings" "NumFields"

  StrCpy $field 1
  ${While} $field < $NumFields ; >
    IntOp $field $field + 1
    ReadINIStr ${TEMP1} "${INSTALL_OPTIONS_INI}" "Field $field" "State"
    StrCmp ${TEMP1} 1 done
  ${EndWhile}

  MessageBox MB_ICONEXCLAMATION|MB_OK "$(plugin_updater_must_select)"
  Abort

  done:
  
FunctionEnd
